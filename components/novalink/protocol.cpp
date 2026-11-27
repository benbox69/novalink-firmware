#include "protocol.h"

namespace esphome::novalink {

static constexpr uint32_t DAY_MS = 24UL * 3600 * 1000;

uint8_t frame_checksum(const uint8_t *data, size_t len) {
  uint8_t sum = 0;
  for (size_t i = 0; i < len; i++)
    sum += data[i];
  return sum;
}

bool is_readable(uint8_t location, uint8_t address) {
  return (location & WRITE_BIT) == 0 && address != (LOCATION_RAM | WRITE_BIT) &&
         address != (LOCATION_EEPROM | WRITE_BIT);
}

const char *write_result_to_string(WriteResult result) {
  switch (result) {
    case WriteResult::ACCEPTED:
      return "accepted";
    case WriteResult::DONE:
      return "done";
    case WriteResult::UNCHANGED:
      return "unchanged, nothing written";
    case WriteResult::SUPERSEDED:
      return "replaced by a newer request";
    case WriteResult::REFUSED_READ_ONLY:
      return "refused: read only";
    case WriteResult::REFUSED_BLOCKED:
      return "refused: blocked address";
    case WriteResult::REFUSED_NOT_ALLOWED:
      return "refused: address not allowed";
    case WriteResult::REFUSED_ADDRESS:
      return "refused: invalid address";
    case WriteResult::REFUSED_GUARD:
      return "refused: current value or stove state not accepted";
    case WriteResult::REFUSED_LIMIT:
      return "refused: too many EEPROM writes in 24 h";
    case WriteResult::QUEUE_FULL:
      return "refused: queue full";
    case WriteResult::FAILED_READ:
      return "failed: no reading before the write, nothing written";
    case WriteResult::FAILED:
      return "failed: sent but not confirmed";
  }
  return "unknown";
}

bool Guard::accepts(uint8_t value) const {
  switch (this->type) {
    case RANGE:
      return value >= this->min && value <= this->max;
    case STATES:
      return value < 32 && (this->states & (1UL << value)) != 0;
    default:
      return true;
  }
}

bool Bus::contains_(const Key *list, size_t count, uint8_t location, uint8_t address) {
  for (size_t i = 0; i < count; i++) {
    if (list[i].location == location && list[i].address == address)
      return true;
  }
  return false;
}

bool Bus::allow_write(uint8_t location, uint8_t address) {
  if (contains_(this->allowed_, this->allowed_count_, location, address))
    return true;
  if (this->allowed_count_ == MAX_RULES)
    return false;
  this->allowed_[this->allowed_count_++] = {location, address};
  return true;
}

bool Bus::block_write(uint8_t location, uint8_t address) {
  if (contains_(this->blocked_, this->blocked_count_, location, address))
    return true;
  if (this->blocked_count_ == MAX_RULES)
    return false;
  this->blocked_[this->blocked_count_++] = {location, address};
  return true;
}

bool Bus::queue_read(uint8_t location, uint8_t address) {
  if (!is_readable(location, address))
    return false;
  for (size_t i = 0; i < this->read_count_; i++) {
    const Key &key = this->reads_[(this->read_head_ + i) % MAX_READS];
    if (key.location == location && key.address == address)
      return true;
  }
  if (this->read_count_ == MAX_READS)
    return false;
  this->reads_[(this->read_head_ + this->read_count_) % MAX_READS] = {location, address};
  this->read_count_++;
  return true;
}

WriteResult Bus::queue_write(const WriteRequest &request) {
  if (this->config.read_only)
    return WriteResult::REFUSED_READ_ONLY;
  // The address is read before and after the write, so it must be readable
  if (!is_readable(request.location, request.address))
    return WriteResult::REFUSED_ADDRESS;
  if (contains_(this->blocked_, this->blocked_count_, request.location, request.address))
    return WriteResult::REFUSED_BLOCKED;
  const bool allowed = request.custom
                           ? this->config.allow_custom_writes
                           : contains_(this->allowed_, this->allowed_count_, request.location, request.address);
  if (!allowed)
    return WriteResult::REFUSED_NOT_ALLOWED;

  for (size_t i = 0; i < this->write_count_; i++) {
    WriteRequest &queued = this->writes_[i];
    if (queued.location == request.location && queued.address == request.address) {
      const WriteRequest old = queued;
      queued = request;
      queued.attempts = 0;
      this->listener_->on_write_result(old, WriteResult::SUPERSEDED);
      return WriteResult::ACCEPTED;
    }
  }
  if (this->write_count_ == MAX_WRITES)
    return WriteResult::QUEUE_FULL;
  this->writes_[this->write_count_] = request;
  this->writes_[this->write_count_].attempts = 0;
  this->write_count_++;
  return WriteResult::ACCEPTED;
}

void Bus::loop(uint32_t now) {
  this->now_ = now;
  if (this->eeprom_window_active_ && now - this->eeprom_window_start_ >= DAY_MS) {
    this->eeprom_window_count_ = 0;
    this->eeprom_window_active_ = false;
  }
  switch (this->phase_) {
    case Phase::IDLE:
      this->start_next_();
      break;
    case Phase::ECHO:
    case Phase::REPLY:
      this->receive_();
      break;
    case Phase::RECOVER:
      if (now - this->phase_start_ >= this->config.recovery_delay) {
        this->phase_ = Phase::IDLE;
        this->recover_();
      }
      break;
  }
}

void Bus::start_next_() {
  if (this->had_exchange_ && this->now_ - this->last_exchange_ < this->config.exchange_gap)
    return;
  if (this->take_write_()) {
    this->start_read_(Task::BEFORE_WRITE, this->write_.location, this->write_.address);
    return;
  }
  if (this->read_count_ == 0)
    return;
  const Key key = this->reads_[this->read_head_];
  this->read_head_ = (this->read_head_ + 1) % MAX_READS;
  this->read_count_--;
  this->start_read_(Task::READ, key.location, key.address);
}

bool Bus::take_write_() {
  for (size_t i = 0; i < this->write_count_; i++) {
    if (is_eeprom_(this->writes_[i].location) && this->eeprom_written_ &&
        this->now_ - this->last_eeprom_write_ < this->config.eeprom_write_interval)
      continue;  // too early, the other requests go first
    this->write_ = this->writes_[i];
    this->write_count_--;
    for (size_t j = i; j < this->write_count_; j++)
      this->writes_[j] = this->writes_[j + 1];
    return true;
  }
  return false;
}

void Bus::start_read_(Task task, uint8_t location, uint8_t address) {
  this->task_ = task;
  this->current_ = {location, address};
  this->retries_left_ = this->config.read_retries;
  const uint8_t frame[] = {location, address};
  this->send_frame_(frame, sizeof(frame));
}

bool Bus::eeprom_limit_reached_() const {
  return is_eeprom_(this->write_.location) && this->config.eeprom_daily_limit > 0 &&
         this->eeprom_window_count_ >= this->config.eeprom_daily_limit;
}

void Bus::send_write_() {
  this->task_ = Task::WRITE;
  uint8_t frame[MAX_FRAME] = {static_cast<uint8_t>(this->write_.location | WRITE_BIT), this->write_.address,
                              this->write_.value, 0};
  frame[3] = frame_checksum(frame, 3);
  if (is_eeprom_(this->write_.location)) {
    this->last_eeprom_write_ = this->now_;
    this->eeprom_written_ = true;
    if (!this->eeprom_window_active_) {
      this->eeprom_window_active_ = true;
      this->eeprom_window_start_ = this->now_;
    }
    if (this->eeprom_window_count_ < 0xFFFF)
      this->eeprom_window_count_++;
    this->eeprom_writes_++;
  }
  this->write_.attempts++;
  this->send_frame_(frame, sizeof(frame));
}

void Bus::send_frame_(const uint8_t *data, size_t len) {
  // Late bytes of an earlier exchange must not be taken for the echo or the reply
  while (this->transport_->available() > 0)
    this->transport_->read_byte();
  for (size_t i = 0; i < len; i++)
    this->frame_[i] = data[i];
  this->frame_len_ = len;
  this->received_ = 0;
  this->transport_->send(this->frame_, len);
  this->phase_ = this->config.echo ? Phase::ECHO : Phase::REPLY;
  this->phase_start_ = this->now_;
}

void Bus::receive_() {
  while (this->transport_->available() > 0) {
    const uint8_t byte = this->transport_->read_byte();
    if (this->phase_ == Phase::ECHO) {
      if (byte != this->frame_[this->received_]) {
        this->exchange_failed_();
        return;
      }
      if (++this->received_ == this->frame_len_) {
        this->received_ = 0;
        this->phase_ = Phase::REPLY;
        this->phase_start_ = this->now_;
      }
      continue;
    }
    this->reply_[this->received_++] = byte;
    if (this->received_ < REPLY_SIZE)
      continue;
    // Reply: checksum of location, address and value, then the value
    const uint8_t covered[] = {this->frame_[0], this->frame_[1], this->reply_[1]};
    const bool valid = this->reply_[0] == frame_checksum(covered, sizeof(covered)) &&
                       (this->task_ != Task::WRITE || this->reply_[1] == this->frame_[2]);
    if (valid) {
      this->exchange_done_(this->reply_[1]);
    } else {
      this->exchange_failed_();
    }
    return;
  }
  if (this->now_ - this->phase_start_ >= this->config.reply_timeout)
    this->exchange_failed_();
}

void Bus::exchange_failed_() {
  this->transport_->release();
  this->phase_ = Phase::RECOVER;
  this->phase_start_ = this->now_;
  if (this->failures_ < 0xFF)
    this->failures_++;
  if (this->failures_ >= this->config.link_down_after)
    this->set_link_(false);
}

void Bus::exchange_done_(uint8_t value) {
  this->transport_->release();
  this->phase_ = Phase::IDLE;
  this->last_exchange_ = this->now_;
  this->had_exchange_ = true;
  this->failures_ = 0;
  this->set_link_(true);

  const Key key = this->current_;
  switch (this->task_) {
    case Task::READ:
      this->task_ = Task::NONE;
      this->listener_->on_value(key.location, key.address, value);
      break;
    case Task::BEFORE_WRITE:
      this->listener_->on_value(key.location, key.address, value);
      if (this->write_.verify && value == this->write_.value) {
        this->finish_write_(this->write_.attempts > 0 ? WriteResult::DONE : WriteResult::UNCHANGED);
      } else if (!this->write_.guard.accepts(value)) {
        this->finish_write_(WriteResult::REFUSED_GUARD);
      } else if (this->eeprom_limit_reached_()) {
        this->finish_write_(WriteResult::REFUSED_LIMIT);
      } else {
        this->send_write_();
      }
      break;
    case Task::WRITE:
      if (this->write_.verify) {
        this->start_read_(Task::AFTER_WRITE, key.location, key.address);
      } else {
        this->finish_write_(WriteResult::DONE);
      }
      break;
    case Task::AFTER_WRITE:
      this->listener_->on_value(key.location, key.address, value);
      if (value == this->write_.value) {
        this->finish_write_(WriteResult::DONE);
      } else {
        this->retry_or_fail_write_();
      }
      break;
    case Task::NONE:
      break;
  }
}

void Bus::recover_() {
  switch (this->task_) {
    case Task::READ:
    case Task::BEFORE_WRITE:
    case Task::AFTER_WRITE: {
      if (this->retries_left_ > 0) {
        this->retries_left_--;
        const uint8_t frame[] = {this->current_.location, this->current_.address};
        this->send_frame_(frame, sizeof(frame));
        return;
      }
      const Key key = this->current_;
      const Task task = this->task_;
      if (task == Task::READ)
        this->task_ = Task::NONE;
      this->listener_->on_read_failed(key.location, key.address);
      if (task == Task::BEFORE_WRITE) {
        this->finish_write_(this->write_.attempts > 0 ? WriteResult::FAILED : WriteResult::FAILED_READ);
      } else if (task == Task::AFTER_WRITE) {
        this->finish_write_(WriteResult::FAILED);
      }
      break;
    }
    case Task::WRITE:
      // The stove may have taken the frame or not: only a reading can tell, never send it again blindly
      if (this->write_.verify) {
        this->start_read_(Task::AFTER_WRITE, this->current_.location, this->current_.address);
      } else {
        this->finish_write_(WriteResult::FAILED);
      }
      break;
    case Task::NONE:
      break;
  }
}

void Bus::retry_or_fail_write_() {
  if (!this->write_.retry || this->write_.attempts > 1) {
    this->finish_write_(WriteResult::FAILED);
    return;
  }
  for (size_t i = 0; i < this->write_count_; i++) {
    if (this->writes_[i].location == this->write_.location && this->writes_[i].address == this->write_.address) {
      this->finish_write_(WriteResult::SUPERSEDED);
      return;
    }
  }
  if (this->write_count_ == MAX_WRITES) {
    this->finish_write_(WriteResult::FAILED);
    return;
  }
  this->writes_[this->write_count_++] = this->write_;
  this->task_ = Task::NONE;
}

void Bus::finish_write_(WriteResult result) {
  this->task_ = Task::NONE;
  const WriteRequest request = this->write_;
  this->listener_->on_write_result(request, result);
}

void Bus::set_link_(bool up) {
  const Link link = up ? Link::UP : Link::DOWN;
  if (this->link_ == link)
    return;
  this->link_ = link;
  this->listener_->on_link(up);
}

}  // namespace esphome::novalink
