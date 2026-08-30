#ifndef HIVE_ANALYZER_H
#define HIVE_ANALYZER_H

// =====================================================
// hive-analyzer — Laptop Noah
// Spec-Basis: §5, §7, §11, §46
// Analysiert. Entscheidet nie. Blockiert nie.
// TETACT-Events kommen vom Worker-Prozess, nicht von der PSP.
// =====================================================

#include "worker.h"

class HiveAnalyzer {
public:
    HiveAnalyzer();

    void attach(Worker* worker);
    bool analyze(const Event& event, Event& result) const;
    bool fillAnomaly(Event* out, uint32_t now, const char* note) const;
    bool canDecide() const;

private:
    Worker* worker_;
};

#endif
