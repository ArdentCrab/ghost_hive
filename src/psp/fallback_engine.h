#ifndef FALLBACK_ENGINE_H
#define FALLBACK_ENGINE_H

// =====================================================
// Ghost Hive v1.7.1
// Fallback Engine
// Spec-Basis: §20
// =====================================================

#include "ghost_core.h"

enum class TaskTarget : uint8_t {
    Worker,
    Psp,
    Phone,
    Nas
};

class FallbackEngine {
public:
    TaskTarget resolve(
        bool workerOnline,
        bool phoneOnline,
        bool nasOnline
    ) const;
};

#endif // FALLBACK_ENGINE_H
