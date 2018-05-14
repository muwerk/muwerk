// loctime.h - the muwerk local time handler

#pragma once

#if defined(__ESP__)

#include "platform.h"
#include "array.h"
#include "map.h"

#include "scheduler.h"

#include <Time.h>
#include <TimeLib.h>
#include <Timezone.h>

#include <ArduinoJson.h>

#include <FS.h>

namespace ustd {
class LocTime {
  public:
    Scheduler *pSched;
    TimeChangeRule CESTtz;
    TimeChangeRule CETtz;
    Timezone *CEtz;

    LocTime() {
    }

    void begin(Scheduler *_pSched) {
        pSched = _pSched;
        // give a c++11 lambda as callback scheduler task registration of
        // this.loop():
        std::function<void()> ft = [=]() { this->loop(); };
        pSched->add(ft);

        // Central European Summer Time
        TimeChangeRule tc = {"CEST", Last, Sun, Mar, 2, 120};
        CESTtz = tc;
        // Central European Standard Time
        TimeChangeRule tcw = {"CET ", Last, Sun, Oct, 3, 60};
        CETtz = tcw;
        CEtz = new Timezone(CESTtz, CETtz);

        // give a c++11 lambda as callback for incoming mqttmessages:
        std::function<void(String, String, String)> fng =
            [=](String topic, String msg, String originator) {
                this->subsGet(topic, msg, originator);
            };
        pSched->subscribe("net/network/get", fng);
    }

    void subsGet(String topic, String msg, String originator) {
    }

    void loop() {
    }
};
}  // namespace ustd

#endif  // defined(__ESP__)
