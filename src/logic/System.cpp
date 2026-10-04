#include "logic/System.h"
#include "logic/Colony.h"

System::System(std::string id, std::string name, std::string effect, bool built)
    : id_(std::move(id)), name_(std::move(name)), effect_(std::move(effect)), built_(built) {}

float System::barEffect(Bar) const { return 0.0f; }

OxygenSystem::OxygenSystem(float refill)
    : System("oxygen", "OXYGEN", "Colony oxygen +" + std::to_string(static_cast<int>(refill)) + "/h", true), refill_(refill) {}
void OxygenSystem::applyHour(Colony& c) { c.bar(Bar::Oxygen) += refill_; }
float OxygenSystem::barEffect(Bar b) const { return b == Bar::Oxygen ? refill_ : 0.0f; }

MedicalSystem::MedicalSystem(float heal) : System("medical", "MEDICAL", "Most injured crew +3 hp/h", true), heal_(heal) {}
void MedicalSystem::applyHour(Colony& c) { c.healWeakest(heal_); }

HeaterSystem::HeaterSystem(float refill)
    : System("heater", "HEATER", "Warmth +" + std::to_string(static_cast<int>(refill)) + "/h", true), refill_(refill) {}
void HeaterSystem::applyHour(Colony& c) { c.bar(Bar::Warmth) += refill_; }
float HeaterSystem::barEffect(Bar b) const { return b == Bar::Warmth ? refill_ : 0.0f; }

CommsSystem::CommsSystem(float chargePerHour)
    : System("comms", "COMMS", "Transmitter +25%/h", false), charge_(chargePerHour) {}
void CommsSystem::applyHour(Colony& c) { c.chargeTransmitter(charge_); }

GreenhouseSystem::GreenhouseSystem(float refill) : System("greenhouse", "GREENHOUSE", "Food +3/h after 8 h of growth", false), refill_(refill) {}
void GreenhouseSystem::applyHour(Colony& c) {
    if (++c.greenhouseGrow > 8) c.bar(Bar::Food) += refill_;   // seedlings need 8 hours of light first
}
float GreenhouseSystem::barEffect(Bar b) const { return b == Bar::Food ? refill_ : 0.0f; }

PurifierSystem::PurifierSystem(float refill) : System("purifier", "PURIFIER", "Water +4/h", false), refill_(refill) {}
void PurifierSystem::applyHour(Colony& c) { c.bar(Bar::Water) += refill_; }
float PurifierSystem::barEffect(Bar b) const { return b == Bar::Water ? refill_ : 0.0f; }

BeaconSystem::BeaconSystem() : System("beacon", "BEACON", "Guides the shuttle down", false) {}
void BeaconSystem::applyHour(Colony&) {}
