#include "ghost_peek.h"
#include "transport/ghost_transport.h"

GhostPeek::GhostPeek() : transport_(nullptr) {
    init();
}

void GhostPeek::init() {
    mineCount_ = 0;
    for (uint8_t i = 0; i < MAX_MINES; ++i) {
        uint8_t* p = reinterpret_cast<uint8_t*>(&mines_[i]);
        for (uint8_t j = 0; j < sizeof(MineInfo); ++j) p[j] = 0;
    }
}

void GhostPeek::attachTransport(GhostTransport* transport) {
    transport_ = transport;
}

void GhostPeek::perform() {
    init();
    if (transport_ != nullptr) {
        transport_->openPeekWindow();
    }
}

void GhostPeek::perform(ReplayGuard& guard) {
    ingestGuard(guard);
}

void GhostPeek::ingestGuard(const ReplayGuard& guard) {
    perform();
    uint8_t n = guard.trackedCount();
    if (n > MAX_MINES) n = MAX_MINES;
    for (uint8_t i = 0; i < n; ++i) {
        const char* id = guard.mineIdAt(i);
        if (id == nullptr) continue;
        uint8_t k = 0;
        while (id[k] != '\0' && k < 31) {
            mines_[mineCount_].mine_id[k] = id[k];
            ++k;
        }
        mines_[mineCount_].mine_id[k] = '\0';
        mines_[mineCount_].status = guard.blockedAt(i) ? 1 : 0;
        mines_[mineCount_].lastEvent = guard.lastCounterAt(i);
        ++mineCount_;
    }
}

void GhostPeek::ingestMine(const MinePayload& payload) {
    if (mineCount_ >= MAX_MINES) return;
    uint8_t k = 0;
    while (payload.mine_id[k] != '\0' && k < 31) {
        mines_[mineCount_].mine_id[k] = payload.mine_id[k];
        ++k;
    }
    mines_[mineCount_].mine_id[k] = '\0';
    mines_[mineCount_].status = 0;
    mines_[mineCount_].lastEvent = payload.counter;
    ++mineCount_;
}

uint8_t GhostPeek::getMineCount() const {
    return mineCount_;
}

const MineInfo* GhostPeek::getMine(uint8_t index) const {
    if (index >= mineCount_) return nullptr;
    return &mines_[index];
}

bool GhostPeek::coldStart() const {
    return false;
}
