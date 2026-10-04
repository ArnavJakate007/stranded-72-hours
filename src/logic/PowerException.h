#pragma once
#include <stdexcept>
#include <string>

// Thrown by Colony::switchOn when a system cannot run. The power panel catches it
// and shows the red message (GDD "add later": exceptions).
class PowerException : public std::runtime_error {
public:
    enum class Reason { NoFreeUnit, NotBuilt, NoPower, Damaged };
    PowerException(Reason r, const std::string& what) : std::runtime_error(what), reason_(r) {}
    Reason reason() const { return reason_; }

private:
    Reason reason_;
};

// Thrown when a data file is missing or invalid (SRS section 6).
class DataError : public std::runtime_error {
public:
    explicit DataError(const std::string& what) : std::runtime_error(what) {}
};
