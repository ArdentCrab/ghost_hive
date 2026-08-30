#ifndef HIVE_INDEX_H
#define HIVE_INDEX_H

// =====================================================
// hive-index — NAS
// Spec-Basis: §11, §28
// =====================================================

#include "safe.h"

class HiveIndex {
public:
    HiveIndex();

    void attach(Safe* safe);
    bool write(const char* index, char* output, uint8_t maxLen) const;

private:
    Safe* safe_;
};

#endif
