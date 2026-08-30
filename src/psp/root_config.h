#ifndef ROOT_CONFIG_H
#define ROOT_CONFIG_H

// =====================================================
// Root-JSON Ingest — §32 Registry, §33 Root bleibt Kernel-intern
// Kein Secret in der Datei. Kein neues Policy-Modul.
// Extra-Felder (scanner_policy, danger_matrix, vault_policy) werden ignoriert.
// =====================================================

#include "registry.h"

#if defined(__PSP__)
static const char ROOT_CONFIG_PATH[] = "ms0:/ghost_hive/root_config.json";
#else
static const char ROOT_CONFIG_PATH[] = "/tmp/ghost_hive/root_config.json";
#endif

const uint16_t ROOT_CONFIG_MAX_BYTES = 4096;

// true = validiert, Members in Registry, Datei konsumiert.
bool root_config_ingest(const char* path, Registry& registry);

#endif
