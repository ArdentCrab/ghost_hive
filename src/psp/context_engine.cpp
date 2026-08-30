#include "context_engine.h"

// =====================================================
// Ghost Hive v1.7.1
// Context Engine
// =====================================================

ContextEngine::ContextEngine() {
}

ContextMode ContextEngine::compute(
    const char* ssid,
    const char* ipRange,
    bool nasOnline,
    bool phoneOnline
) const {
    if (ssid == nullptr || ssid[0] == '\0') {
        return ContextMode::Offline;
    }

    if (nasOnline) {
        return ContextMode::Home;
    }

    if (phoneOnline) {
        return ContextMode::Mobile;
    }

    return ContextMode::Public;
}
