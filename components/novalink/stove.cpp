#include "stove.h"

#include <cstring>

namespace esphome::novalink {

void DisplayReader::start(uint32_t now) {
  this->active_ = true;
  this->start_ = now;
  this->received_ = 0;
  this->failures_ = 0;
  this->has_previous_ = false;
  this->count_ = 0;
}

DisplayReader::Result DisplayReader::on_char(size_t index, uint8_t value, uint32_t now) {
  if (!this->active_)
    return Result::IDLE;
  if (index >= SIZE)
    return Result::WAIT;
  this->pass_[index] = value;
  this->received_ |= 1U << index;
  if (this->received_ != 0xFFFF)
    return Result::WAIT;
  this->received_ = 0;
  if (this->has_previous_ && std::memcmp(this->pass_, this->previous_, SIZE) == 0)
    this->add_text_();
  std::memcpy(this->previous_, this->pass_, SIZE);
  this->has_previous_ = true;
  if (now - this->start_ < this->window)
    return Result::NEXT_PASS;
  this->active_ = false;
  return Result::DONE;
}

DisplayReader::Result DisplayReader::on_failure(uint32_t now) {
  if (!this->active_)
    return Result::IDLE;
  this->received_ = 0;
  this->has_previous_ = false;
  if (++this->failures_ < MAX_FAILURES && now - this->start_ < this->window)
    return Result::NEXT_PASS;
  this->active_ = false;
  return Result::DONE;
}

void DisplayReader::add_text_() {
  char text[TEXT_SIZE];
  size_t len = 0;
  for (size_t line = 0; line < SIZE; line += LINE) {
    size_t first = line, last = line + LINE;
    auto blank = [this](size_t i) { return this->pass_[i] <= ' ' || this->pass_[i] >= 0x7F; };
    while (first < last && blank(first))
      first++;
    while (last > first && blank(last - 1))
      last--;
    if (first == last)
      continue;
    if (len > 0)
      text[len++] = ' ';
    for (size_t i = first; i < last; i++)
      text[len++] = blank(i) ? ' ' : static_cast<char>(this->pass_[i]);
  }
  text[len] = '\0';
  if (len == 0)
    return;
  size_t place = 0;
  for (; place < this->count_; place++) {
    const int order = std::strcmp(text, this->texts_[place]);
    if (order == 0)
      return;
    if (order < 0)
      break;
  }
  if (this->count_ == MAX_TEXTS)
    return;
  for (size_t i = this->count_; i > place; i--)
    std::memcpy(this->texts_[i], this->texts_[i - 1], TEXT_SIZE);
  std::memcpy(this->texts_[place], text, TEXT_SIZE);
  this->count_++;
}

size_t DisplayReader::join(char *out, size_t size) const {
  size_t len = 0;
  for (size_t i = 0; i < this->count_; i++) {
    const size_t separator = i > 0 ? 3 : 0;
    const size_t n = std::strlen(this->texts_[i]);
    if (len + separator + n + 1 > size)
      break;
    std::memcpy(out + len, " / ", separator);
    std::memcpy(out + len + separator, this->texts_[i], n);
    len += separator + n;
  }
  if (size > 0)
    out[len] = '\0';
  return len;
}

}  // namespace esphome::novalink
