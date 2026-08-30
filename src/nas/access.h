#ifndef HIVE_ACCESS_H
#define HIVE_ACCESS_H

// =====================================================
// hive-access — NAS Lockvogel
// Spec-Basis: §11, §38, §39
// Erkennt Zugriff auf Lockvogel-Shares. Alarmiert nie.
// =====================================================

#include "ghost_core.h"

class HiveAccess {
public:
    HiveAccess();

    bool isLockvogel(const char* share) const;
    bool onShareAccess(const char* share) const;
    bool canConfigure() const;

private:
    bool same(const char* a, const char* b) const;
};

#endif
