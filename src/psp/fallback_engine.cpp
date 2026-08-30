#include "fallback_engine.h"

// =====================================================
// Ghost Hive v1.7.1
// Fallback Engine
// =====================================================

TaskTarget FallbackEngine::resolve(
    bool workerOnline,
    bool phoneOnline,
    bool nasOnline
) const {
    // §20: Worker degradiert → PSP analysiert
    if (!workerOnline) {
        return TaskTarget::Psp;
    }

    // Worker online → Worker
    return TaskTarget::Worker;
}
