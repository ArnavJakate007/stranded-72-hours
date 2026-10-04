#pragma once
#include <string>

class Colony;
enum class Bar;

// A colony machine that uses one power unit while running.
// Abstract base: every child refills or acts in its own way (runtime polymorphism).
class System {
public:
    System(std::string id, std::string name, std::string effect, bool built);
    virtual ~System() = default;

    // Called once per game hour, only while the system is on.
    virtual void applyHour(Colony& colony) = 0;
    // How much this system adds to a bar per hour while on (for the HUD rate display).
    virtual float barEffect(Bar bar) const;

    const std::string& id() const { return id_; }
    const std::string& name() const { return name_; }
    const std::string& effect() const { return effect_; }
    bool built() const { return built_; }
    bool on() const { return on_; }
    int powerCost() const { return 1; }

protected:
    friend class Colony;   // only the colony may switch systems, so power can never exceed the units
    void setOn(bool v) { on_ = v; }
    void setBuilt(bool v) { built_ = v; }

private:
    std::string id_, name_, effect_;
    bool built_;
    bool on_ = false;
};

class OxygenSystem : public System {
public:
    explicit OxygenSystem(float refill);
    void applyHour(Colony& colony) override;
    float barEffect(Bar bar) const override;
private:
    float refill_;
};

class MedicalSystem : public System {
public:
    explicit MedicalSystem(float heal);
    void applyHour(Colony& colony) override;
private:
    float heal_;
};

class HeaterSystem : public System {
public:
    explicit HeaterSystem(float refill);
    void applyHour(Colony& colony) override;
    float barEffect(Bar bar) const override;
private:
    float refill_;
};

class CommsSystem : public System {
public:
    explicit CommsSystem(float chargePerHour);
    void applyHour(Colony& colony) override;
private:
    float charge_;
};

class GreenhouseSystem : public System {
public:
    explicit GreenhouseSystem(float refill);
    void applyHour(Colony& colony) override;
    float barEffect(Bar bar) const override;
private:
    float refill_;
};

class PurifierSystem : public System {
public:
    explicit PurifierSystem(float refill);
    void applyHour(Colony& colony) override;
    float barEffect(Bar bar) const override;
private:
    float refill_;
};

class BeaconSystem : public System {
public:
    BeaconSystem();
    void applyHour(Colony& colony) override;
};
