#ifndef CONTEXT_ENGINE_H
#define CONTEXT_ENGINE_H

// =====================================================
// Ghost Hive v1.7.1
// Context Engine
// Spec-Basis: §20
// =====================================================

#include "ghost_core.h"

enum class ContextMode : uint8_t {
    Home,
    Mobile,
    Public,
    Offline
};

class ContextEngine {
public:
    ContextEngine();

    ContextMode compute(
        const char* ssid,
        const char* ipRange,
        bool nasOnline,
        bool phoneOnline
    ) const;
};

#endif // CONTEXT_ENGINE_H
