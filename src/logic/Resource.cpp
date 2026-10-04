#include "logic/Resource.h"
#include <algorithm>

Resource::Resource(std::string name, float value, float maxValue)
    : name_(std::move(name)), value_(value), max_(maxValue) {
    set(value);
}

void Resource::set(float v) { value_ = std::clamp(v, 0.0f, max_); }

Resource& Resource::operator+=(float amount) {
    set(value_ + amount);
    return *this;
}

Resource& Resource::operator-=(float amount) {
    set(value_ - amount);
    return *this;
}
