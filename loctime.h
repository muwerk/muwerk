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
    /*
    // convenient constants for TimeChangeRules
    enum week_t {Last, First, Second, Third, Fourth};
    enum dow_t {Sun=1, Mon, Tue, Wed, Thu, Fri, Sat};
    enum month_t {Jan=1, Feb, Mar, Apr, May, Jun, Jul, Aug, Sep, Oct, Nov, Dec};

    // structure to describe rules for when daylight/summer time begins,
    // or when standard time begins.
    struct TimeChangeRule
    {
    char abbrev[6];    // five chars max
    uint8_t week;      // First, Second, Third, Fourth, or Last week of the
    month uint8_t dow;       // day of week, 1=Sun, 2=Mon, ... 7=Sat uint8_t
    month;     // 1=Jan, 2=Feb, ... 12=Dec
    uint8_t hour;      // 0-23
    int offset;        // offset from UTC in minutes
    };
     */
    ustd::map<String, week_t> week_toks;
    ustd::map<String, dow_t> dow_toks;
    ustd::map<String, month_t> month_toks;

    void initToks() {
        week_toks["Last"] = Last;
        week_toks["First"] = First;
        week_toks["Second"] = Second;
        week_toks["Third"] = Third;
        week_toks["Fourth"] = Fourth;
        dow_toks["Sun"] = Sun;
        dow_toks["Mon"] = Mon;
        dow_toks["Tue"] = Tue;
        dow_toks["Wed"] = Wed;
        dow_toks["Thu"] = Thu;
        dow_toks["Fri"] = Fri;
        dow_toks["Sat"] = Sat;
        month_toks["Jan"] = Jan;
        month_toks["Feb"] = Feb;
        month_toks["Mar"] = Mar;
        month_toks["Apr"] = Apr;
        month_toks["May"] = May;
        month_toks["Jun"] = Jun;
        month_toks["Jul"] = Jul;
        month_toks["Aug"] = Aug;
        month_toks["Sep"] = Sep;
        month_toks["Oct"] = Oct;
        month_toks["Nov"] = Nov;
        month_toks["Dec"] = Dec;
    }

    char *parseRule(char *p, TimeChangeRule *pRule) {
        memset(pRule, 0, sizeof(TimeChangeRule));
        strncpy(pRule->abbrev, p, 5);
        p = strtok(NULL, ",");
        if (!p)
            return NULL;
        if (week_toks.find(p) == -1)
            return NULL;
        pRule->week = week_toks[p];
        p = strtok(NULL, ",");
        if (!p)
            return NULL;
        if (dow_toks.find(p) == -1)
            return NULL;
        pRule->dow = dow_toks[p];
        p = strtok(NULL, ",");
        if (!p)
            return NULL;
        if (month_toks.find(p) == -1)
            return NULL;
        pRule->month = month_toks[p];
        p = strtok(NULL, ",");
        if (!p)
            return NULL;
        int h = atoi(p);
        if (h < 0 || h > 23)
            return NULL;
        pRule->hour = h;
        p = strtok(NULL, ",");
        if (!p)
            return NULL;
        int o = atoi(p);
        if (o < 0 || o > 24 * 60)
            return NULL;
        pRule->offset = o;
        return p;
    }

    bool parseDstRules(String dstrules, TimeChangeRule *pStdRule,
                       TimeChangeRule *pDstRule) {
        char szDst[256];
        szDst[255] = 0;
        strncpy(szDst, dstrules.c_str(), 255);
        initToks();
        char *p = strtok(szDst, ",");
        p = parseRule(p, pStdRule);
        if (!p)
            return false;
        p = strtok(NULL, ",");
        if (!p)
            return false;
        if (!parseRule(p, pDstRule))
            return false;
        return true;
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
