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

namespace ustd {
class LocTime {
  public:
    Scheduler *pSched;
    Timezone *pTz;

    LocTime() {
    }

    bool parseDstRules(String dstrules, TimeChangeRule *stdRule,
                       TimeChangeRule *dstRule) {
        return false;
    }
    void begin(Scheduler *_pSched, String dstrules) {
        TimeChangeRule tcDst;  //  = {"CEST", Last, Sun, Mar, 2, 120};
        TimeChangeRule tcStd;  // = {"CET ", Last, Sun, Oct, 3, 60};
        pSched = _pSched;
        // give a c++11 lambda as callback scheduler task registration of
        // this.loop():
        std::function<void()> ft = [=]() { this->loop(); };
        pSched->add(ft);

        if (parseDstRules(dstrules, &tcStd, &tcDst)) {
            pTz = new Timezone(tcDst, tcStd);
        }

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
