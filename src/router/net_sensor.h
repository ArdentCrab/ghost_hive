#ifndef NET_SENSOR_H
#define NET_SENSOR_H

// =====================================================
// Router — net sensor
// Spec-Basis: §5, §7, §11, §25
// Nur Net-Events. Keine Config. Kein Admin-Login.
// =====================================================

#include "sensor.h"

class NetSensor {
public:
    NetSensor();

    void attach(Sensor* sensor);
    bool fillPortScan(Event* out, uint32_t now) const;
    bool fillNewDevice(Event* out, uint32_t now) const;
    bool fillSuspiciousFlow(Event* out, uint32_t now) const;
    bool canConfigure() const;
    bool canWrite() const;

private:
    Sensor* sensor_;

    bool putTag(Event* out, const char* tag) const;
};

#endif
