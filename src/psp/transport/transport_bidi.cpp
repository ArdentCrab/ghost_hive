#include "transport_bidi.h"

TransportBidi::TransportBidi()
    : role_(0),
      wlan_(nullptr),
      ir_(nullptr),
      pending_at_(0),
      retries_(0),
      pending_used_(false),
      last_acked_(false) {
    id_[0] = '\0';
    pending_.source_device_id[0] = '\0';
}

void TransportBidi::init(const char* deviceId, uint8_t role) {
    transport_copy_id(id_, deviceId);
    role_ = role;
    pending_used_ = false;
    last_acked_ = false;
    retries_ = 0;
    if (wlan_ != nullptr && transport_is_bidi(role_)) {
        wlan_->registerPeer(id_, role_);
    }
}

void TransportBidi::attach(MediumWlan* wlan, MediumIr* ir) {
    wlan_ = wlan;
    ir_ = ir;
    if (wlan_ != nullptr && id_[0] != '\0' && transport_is_bidi(role_)) {
        wlan_->registerPeer(id_, role_);
    }
}

bool TransportBidi::recvAllowed() const {
    return true;
}

bool TransportBidi::pushKernel(const TransportFrame& frame) {
    if (wlan_ != nullptr && wlan_->toKernel(frame)) return true;
    if (ir_ != nullptr && ir_->toKernel(frame)) return true;
    return false;
}

bool TransportBidi::send(const Event& event, uint32_t now) {
    if (id_[0] == '\0') return false;
    TransportFrame frame;
    transport_clear_frame(frame);
    frame.kind = TransportKind::EventFrame;
    frame.src_role = role_;
    frame.dst_role = ROLE_KERNEL;
    transport_copy_id(frame.src_id, id_);
    transport_copy_id(frame.dst_id, KERNEL_SOURCE_ID);
    frame.event = event;
    transport_copy_id(frame.event.source_device_id, id_);
    frame.stamp = now;
    if (!pushKernel(frame)) return false;
    pending_ = frame.event;
    pending_at_ = now;
    retries_ = 0;
    pending_used_ = true;
    last_acked_ = false;
    return true;
}

bool TransportBidi::poll(Event& out) {
    if (wlan_ == nullptr) return false;
    TransportFrame in;
    if (!wlan_->fromPeer(id_, in)) return false;
    if (in.kind == TransportKind::AckFrame) {
        if (pending_used_ && in.stamp == pending_.timestamp) {
            pending_used_ = false;
            last_acked_ = true;
        }
        return false;
    }
    if (in.kind == TransportKind::MineFrame) return false;
    out = in.event;
    TransportFrame ack;
    transport_clear_frame(ack);
    ack.kind = TransportKind::AckFrame;
    ack.src_role = role_;
    ack.dst_role = ROLE_KERNEL;
    transport_copy_id(ack.src_id, id_);
    transport_copy_id(ack.dst_id, KERNEL_SOURCE_ID);
    ack.stamp = in.event.timestamp;
    ack.event = in.event;
    (void)pushKernel(ack);
    return true;
}

void TransportBidi::tick(uint32_t now) {
    if (!pending_used_) return;
    uint32_t gap = (now > pending_at_) ? (now - pending_at_) : 0;
    if (gap < ACK_RETRY_SEC) return;
    if (retries_ < ACK_RETRY_LIMIT) {
        TransportFrame frame;
        transport_clear_frame(frame);
        frame.kind = TransportKind::EventFrame;
        frame.src_role = role_;
        frame.dst_role = ROLE_KERNEL;
        transport_copy_id(frame.src_id, id_);
        transport_copy_id(frame.dst_id, KERNEL_SOURCE_ID);
        frame.event = pending_;
        frame.stamp = now;
        if (pushKernel(frame)) {
            ++retries_;
            pending_at_ = now;
        }
        return;
    }
    if (gap >= ACK_TIMEOUT_SEC) {
        pending_used_ = false;
        last_acked_ = false;
    }
}

bool TransportBidi::lastAcked() const {
    return last_acked_;
}
