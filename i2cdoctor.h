// i2cdoctor.h -- i2c system diagnostics via messages / mqtt
#pragma once

#include <Arduino_JSON.h>
#ifndef tiny_twi_h
#include <Wire.h>
#else
typedef TinyWire TwoWire;
#endif

#include "scheduler.h"
#include "heartbeat.h"

namespace ustd {

/*! \brief muwerk I2CDoctor Class

The I2CDoctor class implements a remote diagnostics for i2c-devices interface via pub/sub messages.
If the system is connected to MQTT, any MQTT client can be used to access diagnostics.

* publish: `hostname/doctor/i2cinfo/get` -> `hostname/doctor/i2cinfo`, json list of used
i2c-ports in the system.

## Sample of adding the i2c-doctor:

~~~{.cpp}

#include <scheduler.h>

#include <doctor.h>
#include <console.h>

ustd::Scheduler sched( 10, 16, 32 );
ustd::I2CDoctor i2cdoc;
ustd::Net net(LED_BUILTIN);
ustd::Mqtt mqtt;

void apploop() {}

void setup() {
    net.begin(&sched);
    mqtt.begin(&sched);

    i2cdoc.begin(&sched);
    int tID = sched.add( apploop, "main", 50000 );
}

void loop() {
    sched.loop();
}
~~~

*/
class I2CDoctor {
  private:
    // muwerk task management
    Scheduler *pSched;
    int tID;
    TwoWire *pWire;

    // active configuration
    String name;

    // runtime control - state management
    bool bActive = false;
    // runtime control - i2c scanner
    int hwErrs = 0;
    int i2cDevs = 0;

  public:
    I2CDoctor(String name = "doctor") : name(name) {
        //! Instantiates an I2CDoctor Task
    }

    ~I2CDoctor() {
    }

    void begin(Scheduler *_pSched, TwoWire *_pWire) {
        /*! Starts the Doctor Task
         *
         * Wire.begin() must have been called before calling this. ESP32
         * will crash otherwise.
         *
         * @param _pSched Pointer to the muwerk scheduler.
         * @param _pWire Pointer to Wire-instance, use &Wire for default
         */
        pSched = _pSched;
        pWire = _pWire;

        tID = pSched->add([this]() { this->loop(); }, name, 100000);  // every 100 ms

        pSched->subscribe(tID, name + "/#", [this](String topic, String msg, String originator) {
            this->subsMsg(topic, msg, originator);
        });

        bActive = true;
    }

  protected:
    bool i2c_checkAddress(uint8_t address) {
        pWire->beginTransmission(address);
        byte error = pWire->endTransmission();
        if (error == 0) {
            return true;
        } else if (error == 4) {
            ++hwErrs;
        }
        return false;
    }

    void publishI2C() {
        i2cDevs = 0;
        hwErrs = 0;
        JSONVar i2cinfo;
        for (uint8_t address = 1; address < 127; address++) {
            if (i2c_checkAddress(address)) {
                char msg[32];
                sprintf(msg, "0x%02x", address);
                i2cinfo["addresses"][i2cDevs] = (const char *)msg;
                ++i2cDevs;
            }
        }
        i2cinfo["device_count"] = i2cDevs;
        i2cinfo["hardware_errors"] = hwErrs;
        pSched->publish(name + "/i2cinfo", JSON.stringify(i2cinfo));
    }

    void loop() {
        if (bActive) {
        }
    }

    void subsMsg(String topic, String msg, String originator) {
        if (topic == name + "/i2cinfo/get") {
            publishI2C();
        }
    };
};  // I2CDoctor

}  // namespace ustd
