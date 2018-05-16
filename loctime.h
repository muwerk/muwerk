// loctime.h - the muwerk local time and dst handler
// at some point this will hopefully be replaced by something within the ESP
// SDK.

// That point is now. loctime.h is completely replace by SDK functionality!

#pragma once

#if defined(__ESP__)

#include "platform.h"
#include "array.h"
#include "map.h"

#include "scheduler.h"

//#include <Time.h>
//#include <TimeLib.h>
//#include <Timezone.h>

// to platform.h:
//#include <time.h>       // time() ctime()
//#include <sys/time.h>   // struct timeval
//#include <coredecls.h>  // settimeofday_cb()

#include <ArduinoJson.h>

namespace ustd {

/*
bool cbtime_set = false;
void time_is_set(void) {
    timeval cbtime;
    gettimeofday(&cbtime, NULL);
    cbtime_set = true;
#ifdef USE_SERIAL
    Serial.println(
        "------------------ settimeofday() was called ------------------");
#endif
}
extern "C" int clock_gettime(clockid_t unused, struct timespec *tp);
*/

class LocTime {
  public:
    Scheduler *pSched;

    LocTime() {
    }
    /* to port and replace Timezone.h:
    WDORDER = {'last': -1, 'first': 1, 'second': 2, 'third': 3, 'fourth': 4}
    WDNAME = {'mon': 0, 'tue': 1, 'wed': 2, 'thu': 3, 'fri': 4, 'sat': 5, 'sun':
    6} MONNAME = {'jan': 1, 'feb': 2, 'mar': 3, 'apr': 4, 'may': 5, 'jun': 6,
               'jul': 7, 'aug': 8, 'sep': 9, 'oct': 10, 'nov': 11, 'dec': 12}


    def time_border(yr, mons, wds, nums, hr):
        if nums not in WDORDER:
            return None
        num = WDORDER[nums]
        if wds not in WDNAME:
            return None
        wd = WDNAME[wds]
        if mons not in MONNAME:
            return None
        mon = MONNAME[mons]
        t = calendar.timegm((yr, mon, 1, 0, 0, 0, -1, -1, -1))
        tm = time.gmtime(t)
        dt = wd - tm.tm_wday + 1
        if dt < 1:
            dt = dt + 7
        if num > 1:
            dt += (num - 1) * 7
        if num == -1:
            dt = dt + 4 * 7
            if dt > 31:
                dt = dt - 7
        tb = calendar.timegm((yr, mon, dt, hr, 0, 0, -1, -1, -1))
        return tb


    def isdst(t):
        tm = time.gmtime(t)
        if tm.tm_mon < 3 or tm.tm_mon > 10:
            return 0
        if tm.tm_mon > 3 and tm.tm_mon < 10:
            return 1
        if tm.tm_mon == 3:
            sd = tm.tm_mday - tm.tm_wday + 6 - 7
            while sd <= 31:
                sd = sd + 7
            sd = sd - 7
            # print(sd)
            if tm.tm_mday < sd:
                return 0
            if tm.tm_mday > sd:
                return 1
            if tm.tm_hour < 1:
                return 0
            if tm.tm_hour >= 1:
                return 1
        if tm.tm_mon == 10:
            sd = tm.tm_mday - tm.tm_wday + 6 - 7
            while sd <= 31:
                sd = sd + 7
            sd = sd - 7
            # print(sd)
            if tm.tm_mday < sd:
                return 1
            if tm.tm_mday > sd:
                return 0
            if tm.tm_hour < 1:
                return 1
            if tm.tm_hour >= 1:
                return 0
        return -1
    */
    // from Timezone.h
    // convenient constants for TimeChangeRules
    enum week_t { Last, First, Second, Third, Fourth };
    enum dow_t { Sun = 1, Mon, Tue, Wed, Thu, Fri, Sat };
    enum month_t {
        Jan = 1,
        Feb,
        Mar,
        Apr,
        May,
        Jun,
        Jul,
        Aug,
        Sep,
        Oct,
        Nov,
        Dec
    };

    // structure to describe rules for when daylight/summer time begins,
    // or when standard time begins.
    struct TimeChangeRule {
        char abbrev[6];  // five chars max
        uint8_t
            week;     // First, Second, Third, Fourth, or Last week of the month
        uint8_t dow;  // day of week, 1=Sun, 2=Mon, ... 7=Sat
        uint8_t month;  // 1=Jan, 2=Feb, ... 12=Dec
        uint8_t hour;   // 0-23
        int offset;     // offset from UTC in minutes
    };

    TimeChangeRule tcDst;  //  = {"CEST", Last, Sun, Mar, 2, 120};
    TimeChangeRule tcStd;  // = {"CET ", Last, Sun, Oct, 3, 60};
    // XXX: Timezone *pTz;
    int tz_sec = 0;
    int dst_sec = 0;
    bool isDst = false;
    String timeserver;
    bool bActive = false;

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

    void checkSetDst(bool bCache = true) {
        timespec tp;
        bool newIsDst = true;  // XXX: = pTz->utcIsDST(time(NULL));
        if (isDst != newIsDst || !bCache) {
#ifdef USE_SERIAL
            char msg[256];
            sprintf(msg, "(1) %s tz_sec: %d, %s dst_sec: %d", tcStd.abbrev,
                    tz_sec, tcDst.abbrev, dst_sec);
            Serial.println(msg);
#endif
            if (newIsDst) {
                pSched->publish("timezone", tcDst.abbrev);
                configTime(tz_sec, dst_sec, timeserver.c_str());
            } else {
                pSched->publish("timezone", tcStd.abbrev);
                configTime(tz_sec, 0, timeserver.c_str());
            }
#ifdef USE_SERIAL
            sprintf(msg, "(2) %s tz_sec: %d, %s dst_sec: %d", tcStd.abbrev,
                    tz_sec, tcDst.abbrev, dst_sec);
            Serial.println(msg);
#endif
        }
        isDst = newIsDst;
    }

    void begin(Scheduler *_pSched, String _timeserver, String dstrules) {
        pSched = _pSched;
        timeserver = _timeserver;

#ifdef USE_SERIAL
        Serial.println("Loctime.begin() start");
#endif
        // settimeofday_cb(time_is_set);

        // give a c++11 lambda as callback scheduler task registration of
        // this.loop():
        std::function<void()> ft = [=]() { this->loop(); };
        pSched->add(ft, 100000);  // loop every 100ms.

        if (parseDstRules(dstrules, &tcStd, &tcDst)) {
            // XXX: pTz = new Timezone(tcDst, tcStd);
            tz_sec = 3600;   // XXX: tcStd.offset * 60;
            dst_sec = 3600;  // XXX: tcDst.offset * 60 - tz_sec;
#ifdef USE_SERIAL
            char msg[256];
            sprintf(msg, "%s tz_sec: %d, %s dst_sec: %d", tcStd.abbrev, tz_sec,
                    tcDst.abbrev, dst_sec);
            Serial.println(msg);
#endif
            checkSetDst(false);
            bActive = true;
        } else {
#ifdef USE_SERIAL
            Serial.println("No valid DST rules received!");
#endif
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
        if (bActive) {
            checkSetDst(true);
        }
    }
};  // namespace ustd
}  // namespace ustd

#endif  // defined(__ESP__)
