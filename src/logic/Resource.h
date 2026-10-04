#pragma once
#include <string>

// One colony bar (oxygen, warmth, water, food). Shows operator overloading:
// += refills and -= drains, always clamped to 0..100 (SRS NFR-6).
class Resource {
public:
    Resource(std::string name = "", float value = 0.0f, float maxValue = 100.0f);

    Resource& operator+=(float amount);
    Resource& operator-=(float amount);
    bool operator<(float threshold) const { return value_ < threshold; }

    float value() const { return value_; }
    float max() const { return max_; }
    const std::string& name() const { return name_; }
    void set(float v);

private:
    std::string name_;
    float value_;
    float max_;
};
