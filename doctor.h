// doctor.h -- system diagnostics via messages / mqtt
#pragma once

#include <Wire.h>
#include <Arduino_JSON.H>

#include "scheduler.h"
#include "heartbeat.h"

namespace ustd {

/*! \brief muwerk Doctor class

The doctor class implements a remote diagnistics interface via pub/sub messages.
If the system is connected to MQTT, any MQTT client can be used to access diagnotics.

* publish: hostname/doctor/memory/get  -> hostname/doctor/memory, msgs=free memory
* publish: hostname/doctor/i2cinfo/get -> hostname/doctor/i2cinfo, json list of used i2c-ports in
the system
* publish: hostname/doctor/

## Sample of adding the doctor:

~~~{.cpp}

#include "scheduler.h"
#include "console.h"

ustd::Scheduler sched( 10, 16, 32 );
ustd::Doctor doc("doctor");
ustd::Net net(LED_BUILTIN);
ustd::Mqtt mqtt;

void apploop() {}

void setup() {
    net.begin(&sched);
    mqtt.begin(&sched);

    doc.begin(&sched);
    int tID = sched.add( apploop, "main", 50000 );
}

void loop() {
    sched.loop();
}
~~~

*/
class Doctor {
  public:
    String DOCTOR_VERSION = "0.1";
    Scheduler *pSched;
    int tID;
    String name;
    bool bActive = false;
    heartbeat memoryInterval;

#ifdef __ESP__
    HomeAssistant *pHA;
#endif

    Doctor(String name) : name(name) {
    }

    ~Doctor() {
    }

    void begin(Scheduler *_pSched) {
        pSched = _pSched;
        auto ft = [=]() { this->loop(); };
        tID = pSched->add(ft, name, 100000);  // every 100 ms

        auto fnall = [=](String topic, String msg, String originator) {
            this->subsMsg(topic, msg, originator);
        };
        pSched->subscribe(tID, name + "/#", fnall);
        bActive = true;
    }

#ifdef __ESP__
/*
        void registerHomeAssistant(String homeAssistantFriendlyName, String projectName = "",
                                   String homeAssistantDiscoveryPrefix = "homeassistant") {
            pHA = new HomeAssistant(name, tID, homeAssistantFriendlyName, projectName,
                                    AIRQUALITY_VERSION, homeAssistantDiscoveryPrefix);
            pHA->addSensor("temperature", "Temperature", "\\u00B0C", "temperature",
                           "mdi:thermometer");
            pHA->addSensor("humidity", "Humidity", "%", "humidity", "mdi:water-percent");
            pHA->addSensor("pressure", "Pressure", "hPa", "pressure", "mdi:altimeter");
            pHA->begin(pSched);
        }
        */
#endif

    int hwErrs = 0;
    int i2cDevs = 0;

    bool i2c_checkAddress(uint8_t address) {
        Wire.beginTransmission(address);
        byte error = Wire.endTransmission();
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

    void publishDiagnostics() {
        JSONVar i2cinfo;
        i2cinfo["free_memory"] = freeMemory();
#ifdef __ESP__
        i2cinfo["sdk_version"] = (const char *)ESP.getSdkVersion();
        i2cinfo["cpu_frequency"] = (int)ESP.getCpuFreqMHz();
        i2cinfo["free_scetch_space"] = (int)ESP.getFreeSketchSpace();
        i2cinfo["flash_size"] = (int)ESP.getFlashChipSize();
        i2cinfo["flash_speed_mhz"] = (float)((float)ESP.getFlashChipSpeed() / 1000000.0f);
        i2cinfo["program_size"] = (int)ESP.getSketchSize();
#ifdef __ESP32__
        ic2info["hardware"] = (const char *)"ESP32";
        i2cinfo["chip_revision"] = (int)ESP.getChipRevision();
#else
        i2cinfo["hardware"] = (const char *)"ESP8266";
        i2cinfo["chip_id"] = (int)ESP.getChipId();
        i2cinfo["core_version"] = (const char *)(ESP.getCoreVersion().c_str());
        i2cinfo["flash_chip_id"] = (int)ESP.getFlashChipId();
        i2cinfo["real_flash_size"] = (int)ESP.getFlashChipRealSize();
        i2cinfo["last_reset_reason"] = (const char *)(ESP.getResetReason().c_str());
#endif  // __ESP32__
#elif defined(__ARDUINO__)
#ifdef __ATMEGA__
        ic2info["hardware"] = (const char *)"Arduino MEGA";
#elif defined(__UNO__)
        ic2info["hardware"] = (const char *)"Arduino UNO";
#else
        ic2info["hardware"] = (const char *)"Arduino unknown";
#endif  // __ARDUINO__
#endif  // __ESP__
        pSched->publish(name + "/diagnostics", JSON.stringify(i2cinfo));
    }

    void publishMemory() {
        if (bActive) {
            int mem = freeMemory();
            pSched->publish(name + "/memory", String(mem));
        }
    }

    void loop() {
        if (bActive) {
            if (memoryInterval.beat()) {
                publishMemory();
            }
        }
    }

    void subsMsg(String topic, String msg, String originator) {
        if (topic == name + "/memory/get") {
            if (msg != "") {
                int period = msg.toInt();
                memoryInterval = period * 1000;
            } else {
                memoryInterval = 0;
            }
            publishMemory();
        }
        if (topic == name + "/i2cinfo/get") {
            publishI2C();
        }
        if (topic == name + "/diagnostics/get") {
            publishDiagnostics();
        }
    };
};  // Doctor

}  // namespace ustd
