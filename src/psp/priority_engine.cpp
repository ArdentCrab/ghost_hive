#include "priority_engine.h"

// =====================================================
// Ghost Hive v1.7.1
// Priority Engine
// =====================================================

Priority PriorityEngine::compute(
    uint8_t role,
    uint8_t trustLevel,
    bool online,
    bool heartbeatOk
) const {
    // §19: Priorität folgt Rolle + Trust + Online + Heartbeat
    switch (role) {
        case 1: // Worker
            if (online && heartbeatOk) return Priority::Worker;
            return Priority::Psp;

        case 2: // Phone
            if (online && heartbeatOk) return Priority::Phone;
            return Priority::Psp;

        case 3: // PSP
            return Priority::Psp;

        case 4: // NAS
            if (online && heartbeatOk) return Priority::Nas;
            return Priority::Psp;

        default: // Mine
            return Priority::Mine;
    }
}
