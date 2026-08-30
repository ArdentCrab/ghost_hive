#include "analyzer.h"

HiveAnalyzer::HiveAnalyzer() : worker_(nullptr) {}

void HiveAnalyzer::attach(Worker* worker) {
    worker_ = worker;
}

bool HiveAnalyzer::analyze(const Event& event, Event& result) const {
    if (worker_ == nullptr) return false;
    return worker_->analyze(event, result);
}

bool HiveAnalyzer::fillAnomaly(Event* out, uint32_t now, const char* note) const {
    if (worker_ == nullptr) return false;
    return worker_->fillAnalysis(out, now, note);
}

bool HiveAnalyzer::canDecide() const {
    return false;
}
