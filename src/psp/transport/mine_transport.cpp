#include "mine_transport.h"

MineTransport::MineTransport() : wlan_(nullptr), ir_(nullptr) {
    id_[0] = '\0';
}

void MineTransport::init(const char* mineId) {
    transport_copy_id(id_, mineId);
}

void MineTransport::attach(MediumWlan* wlan, MediumIr* ir) {
    wlan_ = wlan;
    ir_ = ir;
}

bool MineTransport::send(const MinePayload& payload, uint32_t now) {
    if (id_[0] == '\0') return false;
    TransportFrame frame;
    transport_clear_frame(frame);
    frame.kind = TransportKind::MineFrame;
    frame.src_role = ROLE_MINE;
    frame.dst_role = ROLE_KERNEL;
    transport_copy_id(frame.src_id, id_);
    transport_copy_id(frame.dst_id, KERNEL_SOURCE_ID);
    frame.mine = payload;
    transport_copy_id(frame.mine.mine_id, id_);
    frame.mine.timestamp = now;
    frame.stamp = now;
    if (ir_ != nullptr && ir_->toKernel(frame)) return true;
    if (wlan_ != nullptr && wlan_->toKernel(frame)) return true;
    return false;
}

bool MineTransport::recv(MinePayload& payload) {
    (void)payload;
    return false;
}
