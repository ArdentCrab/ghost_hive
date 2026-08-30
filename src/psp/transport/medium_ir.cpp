#include "medium_ir.h"
#include "ghost_ir.h"

MediumIr::MediumIr() {
    init();
}

void MediumIr::init() {
    ir_ = nullptr;
    head_ = 0;
    tail_ = 0;
    count_ = 0;
    for (uint8_t i = 0; i < TRANSPORT_IR_SLOTS; ++i) {
        transport_clear_frame(rx_[i]);
    }
}

void MediumIr::attach(GhostIR* ir) {
    ir_ = ir;
}

bool MediumIr::toKernel(const TransportFrame& frame) {
    if (count_ >= TRANSPORT_IR_SLOTS) return false;
    rx_[tail_] = frame;
    tail_ = static_cast<uint8_t>((tail_ + 1) % TRANSPORT_IR_SLOTS);
    ++count_;
    return true;
}

bool MediumIr::fromKernel(TransportFrame& frame) {
    if (count_ == 0) return false;
    frame = rx_[head_];
    head_ = static_cast<uint8_t>((head_ + 1) % TRANSPORT_IR_SLOTS);
    --count_;
    return true;
}
