#ifndef PRIORITY_ENGINE_H
#define PRIORITY_ENGINE_H

// =====================================================
// Ghost Hive v1.7.1
// Priority Engine
// Spec-Basis: §19
// =====================================================

#include "ghost_core.h"

enum class Priority : uint8_t {
    Worker = 1,
    Phone = 2,
    Psp = 3,
    Nas = 4,
    Mine = 5
};

class PriorityEngine {
public:
    Priority compute(
        uint8_t role,
        uint8_t trustLevel,
        bool online,
        bool heartbeatOk
    ) const;
};

#endif // PRIORITY_ENGINE_H
