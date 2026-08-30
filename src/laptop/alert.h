#ifndef HIVE_ALERT_H
#define HIVE_ALERT_H

// =====================================================
// hive-alert — Laptop / Phone Noah
// Spec-Basis: §5, §7, §11, §29
// Zeigt Danger / Down / Kill. Entscheidet nie.
// =====================================================

#include "ghost_core.h"

class HiveAlert {
public:
    HiveAlert();

    void init();
    bool ingest(const Event& event);
    bool danger() const;
    bool down() const;
    bool kill() const;
    bool showing() const;

private:
    bool danger_;
    bool down_;
    bool kill_;
};

#endif
