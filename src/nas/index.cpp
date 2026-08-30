#include "index.h"

HiveIndex::HiveIndex() : safe_(nullptr) {}

void HiveIndex::attach(Safe* safe) {
    safe_ = safe;
}

bool HiveIndex::write(const char* index, char* output, uint8_t maxLen) const {
    if (safe_ == nullptr) return false;
    return safe_->writeIndex(index, output, maxLen);
}
