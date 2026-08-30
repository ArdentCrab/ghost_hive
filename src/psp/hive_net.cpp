#include "hive_net.h"

#if defined(__PSP__) || defined(PSP_BUILD)
#include <pspkernel.h>
#include <pspnet.h>
#include <pspnet_inet.h>
#include <pspnet_adhoc.h>
#include <pspnet_adhocctl.h>
#include <pspwlan.h>
#include <psputility.h>
#include <psputility_netmodules.h>
#define HIVE_NET_PSP 1
#endif

#ifdef HIVE_NET_PSP
static bool s_up_ = false;

static void hive_net_teardown_partial(bool inet, bool net, bool wlan) {
    if (inet) (void)sceNetInetTerm();
    if (net) (void)sceNetTerm();
    if (wlan) (void)sceWlanDevDetach();
}

static void hive_copy9(char* dst, const char* src) {
    uint8_t i = 0;
    while (i < 8 && src[i] != '\0') {
        dst[i] = src[i];
        ++i;
    }
    while (i < 9) {
        dst[i] = '\0';
        ++i;
    }
}
#endif

bool hive_net_up() {
#ifdef HIVE_NET_PSP
    if (s_up_) return true;
    if (sceWlanDevAttach() < 0) return false;
    (void)sceUtilityLoadNetModule(PSP_NET_MODULE_COMMON);
    (void)sceUtilityLoadNetModule(PSP_NET_MODULE_INET);
    (void)sceUtilityLoadNetModule(PSP_NET_MODULE_ADHOC);
    if (sceNetInit(0x20000, 0x20, 0x1000, 0x20, 0x1000) < 0) {
        hive_net_teardown_partial(false, false, true);
        return false;
    }
    if (sceNetInetInit() < 0) {
        hive_net_teardown_partial(false, true, true);
        return false;
    }
    if (sceNetAdhocInit() < 0) {
        hive_net_teardown_partial(true, true, true);
        return false;
    }
    struct productStruct prod{};
    prod.unknown = 0;
    hive_copy9(prod.product, "GHIVE000");
    if (sceNetAdhocctlInit(0x2000, 0x30, &prod) < 0) {
        (void)sceNetAdhocTerm();
        hive_net_teardown_partial(true, true, true);
        return false;
    }
    /* IBSS create. SPEC-v1 §22a: no Associate, no Apctl STA, no DHCP. */
    if (sceNetAdhocctlCreate(HIVE_IBSS_SSID) < 0) {
        (void)sceNetAdhocctlTerm();
        (void)sceNetAdhocTerm();
        hive_net_teardown_partial(true, true, true);
        return false;
    }
    /* GetState==1 means connected (SDK). Bound wait — no infinite OFW hang. */
    bool ready = false;
    for (uint8_t n = 0; n < 40; ++n) {
        int st = 0;
        if (sceNetAdhocctlGetState(&st) == 0 && st == 1) {
            ready = true;
            break;
        }
        sceKernelDelayThread(50 * 1000);
    }
    if (!ready) {
        (void)sceNetAdhocctlDisconnect();
        (void)sceNetAdhocctlTerm();
        (void)sceNetAdhocTerm();
        hive_net_teardown_partial(true, true, true);
        return false;
    }
    s_up_ = true;
    return true;
#else
    return true;
#endif
}

void hive_net_down() {
#ifdef HIVE_NET_PSP
    if (!s_up_) {
        (void)sceWlanDevDetach();
        return;
    }
    (void)sceNetAdhocctlDisconnect();
    (void)sceNetAdhocctlTerm();
    (void)sceNetAdhocTerm();
    (void)sceNetInetTerm();
    (void)sceNetTerm();
    (void)sceWlanDevDetach();
    s_up_ = false;
#else
#endif
}

bool hive_net_ready() {
#ifdef HIVE_NET_PSP
    return s_up_;
#else
    return false;
#endif
}
