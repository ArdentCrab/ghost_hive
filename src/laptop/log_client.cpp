#include "log_client.h"

#include <stdio.h>
#include <sys/stat.h>

HiveLogClient::HiveLogClient() {
    init();
}

void HiveLogClient::init() {
    count_ = 0;
}

static void forensic_append(const Event& event) {
#if !defined(__PSP__)
    (void)mkdir("/tmp/ghost_hive", 0700);
    FILE* f = fopen("/tmp/ghost_hive/forensic.log", "ab");
    if (f == nullptr) return;
    fprintf(f, "log ev=%u src=%.32s ts=%u\n",
            static_cast<unsigned>(event.type), event.source_device_id,
            event.timestamp);
    fclose(f);
    (void)chmod("/tmp/ghost_hive/forensic.log", 0600);
#else
    (void)event;
#endif
}

bool HiveLogClient::ingest(const Event& event) {
    forensic_append(event);
    if (count_ >= LOG_CLIENT_SLOTS) return false;
    ram_[count_] = event;
    ++count_;
    return true;
}

bool HiveLogClient::pullBackup(const Event& event) {
    if (event.type != EventType::BackupWritten) return false;
    return ingest(event);
}

uint8_t HiveLogClient::storedCount() const {
    return count_;
}

const Event* HiveLogClient::peek(uint8_t index) const {
    if (index >= count_) return nullptr;
    return &ram_[index];
}

bool HiveLogClient::canFlush() const {
    return false;
}
