#include "invariants.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char** argv) {
    char before[LAB_SNAP_LINE];
    char after[LAB_SNAP_LINE];
    before[0] = '\0';
    after[0] = '\0';
    if (argc >= 3) {
        snprintf(before, LAB_SNAP_LINE, "%s", argv[1]);
        snprintf(after, LAB_SNAP_LINE, "%s", argv[2]);
    } else {
        if (fgets(before, LAB_SNAP_LINE, stdin) == nullptr) return 2;
        if (fgets(after, LAB_SNAP_LINE, stdin) == nullptr) return 2;
    }
    LabSnap a;
    LabSnap b;
    if (!lab_snap_parse(before, &a) || !lab_snap_parse(after, &b)) {
        printf("FAIL parse\n");
        return 2;
    }
    LabInvResult r = lab_check_pair(a, b);
    if (r.ok) {
        printf("OK\n");
        return 0;
    }
    printf("FAIL %s\n", r.id);
    return 1;
}
