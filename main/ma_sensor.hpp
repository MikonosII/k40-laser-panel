// Laser tube current (mA) from an INA226 or INA219 on the panel's I2C-OUT header,
// placed (through an I2C isolator) in the tube's cathode return wire. See docs/tube-current-sensor.md.
#pragma once

namespace ma_sensor {

struct Reading {
    bool present = false;  // sensor answered at boot
    bool ina226 = false;   // false = INA219
    float ma = 0;          // latest (averaged) tube current
    float peak_ma = 0;     // highest reading in the last ~2 s
};

void init();  // probe 0x40 on the panel's I2C bus; starts a 10 Hz reader if found
Reading get();

}  // namespace ma_sensor
