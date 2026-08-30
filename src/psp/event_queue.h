#ifndef EVENT_QUEUE_H
#define EVENT_QUEUE_H

// =====================================================
// Ghost Hive v1.7.1
// Event Queue
// Spec-Basis: §14.2, §18, §24
// Ringpuffer, kein Heap
// =====================================================

#include "ghost_core.h"

const uint8_t EVENT_QUEUE_SIZE = 64;

class EventQueue {
public:
    EventQueue();

    void init();
    bool push(const Event& event);
    bool pop(Event& event);
    bool isEmpty() const;
    bool isFull() const;
    uint8_t getSize() const;
    const Event* peek(uint8_t index) const;
    void clear();

private:
    Event buffer_[EVENT_QUEUE_SIZE];
    uint8_t head_;
    uint8_t tail_;
    uint8_t count_;
};

#endif // EVENT_QUEUE_H
