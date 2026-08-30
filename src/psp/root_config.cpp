#include "root_config.h"

#include <stdio.h>

static bool json_key_is(const char* json, const char* key) {
    if (json == nullptr || key == nullptr) return false;
    uint16_t i = 0;
    while (json[i] != '\0' && i < ROOT_CONFIG_MAX_BYTES) {
        if (json[i] == '"') {
            uint16_t k = 0;
            uint16_t j = static_cast<uint16_t>(i + 1);
            bool match = true;
            while (key[k] != '\0') {
                if (json[j] != key[k]) {
                    match = false;
                    break;
                }
                ++k;
                ++j;
                if (j >= ROOT_CONFIG_MAX_BYTES) return false;
            }
            if (match && json[j] == '"') return true;
        }
        ++i;
    }
    return false;
}

static bool looks_like_secret(const char* json) {
    if (json_key_is(json, "root_key")) return true;
    if (json_key_is(json, "secret")) return true;
    if (json_key_is(json, "private_key")) return true;
    if (json_key_is(json, "hmac_key")) return true;
    if (json_key_is(json, "totp_seed")) return true;
    if (json_key_is(json, "device_key")) return true;
    if (json_key_is(json, "session_key")) return true;
    if (json_key_is(json, "mine_key")) return true;
    if (json_key_is(json, "root")) return true;
    return false;
}

static bool signatures_required(const char* json) {
    const char* k = "require_signatures";
    uint16_t i = 0;
    while (json[i] != '\0' && i < ROOT_CONFIG_MAX_BYTES) {
        uint16_t n = 0;
        while (k[n] != '\0' && json[i + n] == k[n]) ++n;
        if (k[n] == '\0') {
            uint16_t j = static_cast<uint16_t>(i + n);
            while (j < ROOT_CONFIG_MAX_BYTES && json[j] != '\0' &&
                   json[j] != 't' && json[j] != 'f') {
                ++j;
            }
            if (json[j] == 'f') return false;
            return true;
        }
        ++i;
    }
    return true;
}

static bool parse_string_value(const char* json, uint16_t start,
                               char* dst, uint8_t max) {
    uint16_t i = start;
    while (i < ROOT_CONFIG_MAX_BYTES && json[i] != '\0' && json[i] != '"') ++i;
    if (json[i] != '"') return false;
    ++i;
    uint8_t n = 0;
    while (i < ROOT_CONFIG_MAX_BYTES && json[i] != '\0' && json[i] != '"' &&
           n < max) {
        dst[n++] = json[i++];
    }
    if (json[i] != '"') return false;
    dst[n] = '\0';
    return n > 0;
}

static bool map_type(const char* type, uint8_t& role, uint8_t& trust,
                     bool& mine) {
    mine = false;
    if (type[0] == 'w' && type[1] == 'o') {
        role = ROLE_WORKER;
        trust = 2;
        return true;
    }
    if (type[0] == 'p' && type[1] == 'h') {
        role = ROLE_PHONE;
        trust = 1;
        return true;
    }
    if (type[0] == 's' && type[1] == 'e') {
        role = ROLE_SENSOR;
        trust = 1;
        return true;
    }
    if (type[0] == 's' && type[1] == 'a') {
        role = ROLE_SAFE;
        trust = 1;
        return true;
    }
    if (type[0] == 'n' && type[1] == 'a') {
        role = ROLE_SAFE;
        trust = 1;
        return true;
    }
    if (type[0] == 'r' && type[1] == 'o') {
        role = ROLE_ROUTER;
        trust = 1;
        return true;
    }
    if (type[0] == 'm' && type[1] == 'i') {
        role = ROLE_MINE;
        trust = 0;
        mine = true;
        return true;
    }
    return false;
}

static bool enroll_one(Registry& registry, const char* id, uint8_t role,
                       uint8_t trust, bool mine) {
    if (id == nullptr || id[0] == '\0') return false;
    if (registry.getDevice(id) != nullptr) return true;
    Device d{};
    uint8_t i = 0;
    while (id[i] != '\0' && i < 31) {
        d.id[i] = id[i];
        ++i;
    }
    d.id[i] = '\0';
    d.role = role;
    d.trust_level = trust;
    d.last_seen = 0;
    d.capability_mask = 0;
    d.tag_mask = 0;
    d.status = mine ? DeviceState::Silent : DeviceState::Pending;
    if (!registry.addDevice(d)) return false;
    if (!mine) return registry.pairDevice(d.id);
    return true;
}

static bool apply_members(const char* json, Registry& registry) {
    const char* marker = "hive_members";
    uint16_t i = 0;
    bool found = false;
    while (json[i] != '\0' && i < ROOT_CONFIG_MAX_BYTES) {
        uint16_t n = 0;
        while (marker[n] != '\0' && json[i + n] == marker[n]) ++n;
        if (marker[n] == '\0') {
            found = true;
            i = static_cast<uint16_t>(i + n);
            break;
        }
        ++i;
    }
    if (!found) return false;

    uint8_t added = 0;
    while (json[i] != '\0' && i < ROOT_CONFIG_MAX_BYTES && json[i] != ']') {
        if (json[i] != '{') {
            ++i;
            continue;
        }
        uint16_t obj = i;
        uint16_t end = i;
        uint8_t depth = 1;
        ++end;
        while (end < ROOT_CONFIG_MAX_BYTES && json[end] != '\0' && depth > 0) {
            if (json[end] == '{') ++depth;
            else if (json[end] == '}') --depth;
            ++end;
        }
        char id[32];
        char type[32];
        id[0] = '\0';
        type[0] = '\0';
        uint16_t p = obj;
        while (p < end) {
            if (json[p] == '"' && json[p + 1] == 'i' && json[p + 2] == 'd' &&
                json[p + 3] == '"') {
                (void)parse_string_value(json, static_cast<uint16_t>(p + 4),
                                         id, 31);
            }
            if (json[p] == '"' && json[p + 1] == 't' && json[p + 2] == 'y' &&
                json[p + 3] == 'p' && json[p + 4] == 'e' && json[p + 5] == '"') {
                (void)parse_string_value(json, static_cast<uint16_t>(p + 6),
                                         type, 31);
            }
            ++p;
        }
        if (id[0] != '\0' && type[0] != '\0') {
            uint8_t role = 0;
            uint8_t trust = 0;
            bool mine = false;
            if (!map_type(type, role, trust, mine)) return false;
            if (!enroll_one(registry, id, role, trust, mine)) return false;
            ++added;
        }
        i = end;
    }
    return added > 0;
}

bool root_config_ingest(const char* path, Registry& registry) {
    if (path == nullptr || path[0] == '\0') return false;
    FILE* f = fopen(path, "rb");
    if (f == nullptr) return false;
    char buf[ROOT_CONFIG_MAX_BYTES + 1];
    size_t n = fread(buf, 1, ROOT_CONFIG_MAX_BYTES + 1, f);
    fclose(f);
    if (n == 0 || n > ROOT_CONFIG_MAX_BYTES) return false;
    buf[n] = '\0';

    if (looks_like_secret(buf)) return false;
    if (!signatures_required(buf)) return false;
    if (!apply_members(buf, registry)) return false;

    (void)remove(path);
    return true;
}
