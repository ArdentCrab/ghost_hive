#include "access.h"

HiveAccess::HiveAccess() {}

bool HiveAccess::same(const char* a, const char* b) const {
    if (a == nullptr || b == nullptr) return false;
    uint8_t i = 0;
    while (a[i] != '\0' && b[i] != '\0') {
        if (a[i] != b[i]) return false;
        ++i;
    }
    return a[i] == b[i];
}

bool HiveAccess::isLockvogel(const char* share) const {
    return same(share, "lockvogel") || same(share, "honeypot");
}

bool HiveAccess::onShareAccess(const char* share) const {
    return isLockvogel(share);
}

bool HiveAccess::canConfigure() const {
    return false;
}
