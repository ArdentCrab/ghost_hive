#include "event_queue.h"

// =====================================================
// Ghost Hive v1.7.1
// Event Queue
// Spec-Basis: §14.2, §18, §24
// Ringpuffer, kein Heap, kein Blockieren
// =====================================================

EventQueue::EventQueue() {
    init();
}

void EventQueue::init() {
    head_ = 0;
    tail_ = 0;
    count_ = 0;
}

bool EventQueue::push(const Event& event) {
    if (isFull()) return false;
    if (event.source_device_id[0] == '\0') return false;

    buffer_[tail_] = event;
    tail_ = (tail_ + 1) % EVENT_QUEUE_SIZE;
    ++count_;
    return true;
}

bool EventQueue::pop(Event& event) {
    if (isEmpty()) return false;

    event = buffer_[head_];
    head_ = (head_ + 1) % EVENT_QUEUE_SIZE;
    --count_;
    return true;
}

bool EventQueue::isEmpty() const {
    return count_ == 0;
}

bool EventQueue::isFull() const {
    return count_ >= EVENT_QUEUE_SIZE;
}

uint8_t EventQueue::getSize() const {
    return count_;
}

const Event* EventQueue::peek(uint8_t index) const {
    if (index >= count_) return nullptr;
    return &buffer_[(head_ + index) % EVENT_QUEUE_SIZE];
}

void EventQueue::clear() {
    init();
}
