#include "alert.h"

HiveAlert::HiveAlert() {
    init();
}

void HiveAlert::init() {
    danger_ = false;
    down_ = false;
    kill_ = false;
}

bool HiveAlert::ingest(const Event& event) {
    if (event.type == EventType::DangerModeEnter) {
        danger_ = true;
        return true;
    }
    if (event.type == EventType::DangerModeExit) {
        danger_ = false;
        return true;
    }
    if (event.type == EventType::GhostDownStart) {
        down_ = true;
        kill_ = true;
        return true;
    }
    if (event.type == EventType::GhostDownEnd) {
        down_ = false;
        return true;
    }
    if (event.type == EventType::AlertSent) {
        return true;
    }
    return false;
}

bool HiveAlert::danger() const { return danger_; }
bool HiveAlert::down() const { return down_; }
bool HiveAlert::kill() const { return kill_; }
bool HiveAlert::showing() const { return danger_ || down_ || kill_; }
