#include "psp_time.h"

#ifdef __PSP__
#include <psprtc.h>
#include <psputils.h>
#include <pspthreadman.h>
#include <time.h>
#else
#include <time.h>
#include <unistd.h>
#endif

// §2.3: kein NTP. Lokale Uhr / PSP-RTC.

uint32_t psp_now_ms() {
#ifdef __PSP__
    u64 tick = 0;
    u32 tickres = 0;
    sceRtcGetCurrentTick(&tick);
    tickres = sceRtcGetTickResolution();
    if (tickres == 0) tickres = 1000000;
    return static_cast<uint32_t>((tick / (tickres / 1000u)));
#else
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0;
    }
    return static_cast<uint32_t>(
        static_cast<uint64_t>(ts.tv_sec) * 1000u +
        static_cast<uint64_t>(ts.tv_nsec) / 1000000u);
#endif
}

uint32_t psp_now_sec() {
#ifdef __PSP__
    time_t t = 0;
    sceKernelLibcTime(&t);
    return static_cast<uint32_t>(t);
#else
    time_t t = time(0);
    return static_cast<uint32_t>(t);
#endif
}

void psp_sleep_ms(uint32_t ms) {
#ifdef __PSP__
    sceKernelDelayThread(ms * 1000u);
#else
    if (ms == 0) return;
    usleep(ms * 1000u);
#endif
}
