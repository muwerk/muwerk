// doctor.h -- system diagnostics via messages / mqtt
#pragma once

#include <Arduino_JSON.H>

#include "scheduler.h"
#include "heartbeat.h"

namespace ustd {

/*! \brief muwerk Doctor Class

The doctor class implements a remote diagnostics interface via pub/sub messages.
If the system is connected to MQTT, any MQTT client can be used to access diagnostics.

* publish: `hostname/doctor/memory/get`  -> `hostname/doctor/memory`, msgs=free memory.
* publish: `hostname/doctor/timeinfo/get` -> `hostname/doctor/timeinfo`, json time related
information.
* publish: `hostname/doctor/diagnostics/get` -> `hostname/doctor/diagnostics`, json system related
information.
* publish: `hostname/doctor/restart`  -> restarts system.

## Sample of adding the doctor:

~~~{.cpp}

#include <scheduler.h>

#include <doctor.h>
#include <console.h>

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
  private:
    // muwerk task management
    Scheduler *pSched;
    int tID;

    // active configuration
    String name;

    // runtime control - state management
    bool bActive = false;
    heartbeat memoryInterval;

  public:
    Doctor(String name = "doctor") : name(name) {
        //! Instantiates a Doctor Task
    }

    ~Doctor() {
    }

    void begin(Scheduler *_pSched) {
        /*! Starts the Doctor Task
         *
         * @param _pSched Pointer to the muwerk scheduler.
         */
        pSched = _pSched;

        tID = pSched->add([this]() { this->loop(); }, name, 100000);  // every 100 ms

        pSched->subscribe(tID, name + "/#", [this](String topic, String msg, String originator) {
            this->subsMsg(topic, msg, originator);
        });

        bActive = true;
    }

  protected:
    void publishDiagnostics() {
        JSONVar diaginfo;
#if defined(USTD_FEATURE_FREE_MEMORY)
        diaginfo["free_memory"] = freeMemory();
#endif
#ifdef __ESP__
        diaginfo["sdk_version"] = (const char *)ESP.getSdkVersion();
        diaginfo["cpu_frequency"] = (int)ESP.getCpuFreqMHz();
        diaginfo["free_sketch_space"] = (int)ESP.getFreeSketchSpace();
        diaginfo["flash_size"] = (int)ESP.getFlashChipSize();
        diaginfo["flash_speed_mhz"] = (float)((float)ESP.getFlashChipSpeed() / 1000000.0f);
        diaginfo["program_size"] = (int)ESP.getSketchSize();
#ifdef __ESP32__
        diaginfo["hardware"] = (const char *)"ESP32";
        diaginfo["chip_revision"] = (int)ESP.getChipRevision();
#else
        diaginfo["hardware"] = (const char *)"ESP8266";
        diaginfo["chip_id"] = (int)ESP.getChipId();
        diaginfo["core_version"] = (const char *)(ESP.getCoreVersion().c_str());
        diaginfo["flash_chip_id"] = (int)ESP.getFlashChipId();
        diaginfo["real_flash_size"] = (int)ESP.getFlashChipRealSize();
        diaginfo["last_reset_reason"] = (const char *)(ESP.getResetReason().c_str());
#endif  // __ESP32__
#endif  // __ESP__

#if defined(__ARDUINO__)
#ifdef __ATMEGA__
        diaginfo["hardware"] = (const char *)"Arduino MEGA";
#elif defined(__UNO__)
        diaginfo["hardware"] = (const char *)"Arduino UNO";
#else
        diaginfo["hardware"] = (const char *)"Arduino unknown";
#endif
#endif  // __ARDUINO__

#if defined(__RISC_V__)
        diaginfo["hardware"] = (const char *)"RISC-V";
#endif  // __RISC_V__

#if defined(__ARM__)
        diaginfo["hardware"] = (const char *)"ARM";
#endif  // __ARM__

        pSched->publish(name + "/diagnostics", JSON.stringify(diaginfo));
    }

#if defined(USTD_FEATURE_FREE_MEMORY)
    void publishMemory() {
        int mem = freeMemory();
        pSched->publish(name + "/memory", String(mem));
    }
#endif

    void publishTimeinfo() {
        JSONVar timeinfo;
#ifdef __USTD_FEATURE_CLK_READ__
        time_t now = time(nullptr);
        char szTime[24];
        struct tm *plt = localtime(&now);
        strftime(szTime, 9, "%T", plt);
        timeinfo["time"] = (const char *)szTime;
        strftime(szTime, 20, "%Y.%m.%d %H:%M:%S", plt);
        timeinfo["date"] = (const char *)szTime;
        timeinfo["time_t"] = (long)time(nullptr);
#endif
        timeinfo["uptime"] = (long)pSched->getUptime();
        timeinfo["millis"] = millis();
        pSched->publish(name + "/timeinfo", JSON.stringify(timeinfo));
    }

    void loop() {
        if (bActive) {
#if defined(USTD_FEATURE_FREE_MEMORY)
            if (memoryInterval.beat()) {
                publishMemory();
            }
#endif
        }
    }

    void subsMsg(String topic, String msg, String originator) {
#if defined(USTD_FEATURE_FREE_MEMORY)
        if (topic == name + "/memory/get") {
            if (msg != "") {
                int period = msg.toInt();
                memoryInterval = period * 1000;
            } else {
                memoryInterval = 0;
            }
            publishMemory();
        }
#endif
        if (topic == name + "/diagnostics/get") {
            publishDiagnostics();
        }
        if (topic == name + "/timeinfo/get") {
            publishTimeinfo();
        }
#ifdef __ESP__
        if (topic == name + "/restart") {
            ESP.restart();
        }
#endif
    };
};  // Doctor

}  // namespace ustd
