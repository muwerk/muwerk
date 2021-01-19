// scheduler.h - the muwerk simple scheduler

#pragma once

/*
#include "../ustd/platform.h"
#include "../ustd/array.h"
#include "../ustd/queue.h"
#include "../ustd/functional.h"
*/
#include "platform.h"
#include "array.h"
#include "queue.h"
#include "functional.h"
#include "muwerk.h"

#include <stdio.h>

#if defined(__ESP__) || defined(__UNIXOID__)
#include <functional>
#endif

namespace ustd {

#define SCHEDULER_MAIN 0

/*! \brief Scheduler Task Priority

__WARNING__: Task Priorities are currently not supported by the scheduler.
*/
enum T_PRIO {
    PRIO_SYSTEMCRITICAL = 0,  /// System critical priority
    PRIO_TIMECRITICAL = 1,    /// Time critical priority
    PRIO_HIGH = 2,            /// High Priority
    PRIO_NORMAL = 3,          /// Standard Priority (default for all tasks)
    PRIO_LOW = 4,             /// Low priority
    PRIO_LOWEST = 5           /// Lowest Priority
};

//! \brief Scheduler Message Type
enum T_MSGTYPE {
    MSG_NONE = 0,
    MSG_DIRECT = 1,
    MSG_SUBSCRIBE = 2,
    MSG_UNSUBSCRIBE = 3,
    MSG_PUBLISH = 4,
    MSG_PUBLISHRAW = 5
};

//! \brief Scheduler Task Function
#if defined(__ESP__) || defined(__UNIXOID__)
typedef std::function<void()> T_TASK;
#else
// typedef void (*T_TASK)();
typedef ustd::function<void()> T_TASK;
#endif

typedef struct {
    char *originator;
    char *topic;
    char *msg;
} T_MSG;

//! \brief Scheduler Subscription Function
#if defined(__ESP__) || defined(__UNIXOID__)
typedef std::function<void(String topic, String msg, String originator)> T_SUBS;
#else
// typedef void (*T_SUBS)(String topic, String msg, String originator);
typedef ustd::function<void(String topic, String msg, String originator)> T_SUBS;
#endif

typedef struct {
    int subscriptionHandle;
    int taskID;
    char *originator;
    char *topic;
    T_SUBS subs;
} T_SUBSCRIPTION;

typedef struct {
    int taskID;
    char *szName;
    T_TASK task;
    T_PRIO prio;
    unsigned long minMicros;
    unsigned long lastCall;
    unsigned long lateTime;
    unsigned long cpuTime;
    unsigned long callCount;
    /*
    unsigned long cpuPerCall;
    unsigned long latePerCall;
    */
} T_TASKENTRY;

// forward declaration
class Console;

/*! \brief muwerk Scheduler Class

Implements a cooperative task scheduler. Tasks are defined as `void
myTask()` type functions and can be added to the scheduler for execution
at fixed intervals. Tasks can communicate with each other via pub/sub
messages, using MQTT-style topics for subscription and publishing of
messages.

The library header-only.

Make sure to provide the <a
href="https://github.com/muwerk/ustd/blob/master/README.md">required platform
define</a> before including ustd headers.

## Minimal scheduler:

~~~{.cpp}
#define __ATTINY__ 1   // Platform defines required, see doc, mainpage.
#include <scheduler.h>

ustd::Scheduler sched;
void appLoop();

void someTask() {
    // This is called every 50ms (50000us)
    // Do things here
}

void setup() {
    // Create a task for the main application loop code:
    int tID = sched.add(appLoop, "main");

    // Create a second task that is called every 50ms
    sched.add(someTask, "someTask", 50000L);
}

void appLoop() {
    // your code you would normally put into Arduino's loop goes here.
    // use async programming to prevent locking the cooperative scheduler.
}

// Never add code to this loop, use appLoop() instead.
void loop() {
    sched.loop();
}
~~~

## Pub/Sub communication between tasks

~~~{.cpp}
#define __ESP__ 1   // Platform defines required, see doc, mainpage.
#include "scheduler.h"

ustd::Scheduler sched;
void appLoop();

void subMsg(String topic, String msg, String originator) {
    if (msg == "on")
        digitalWrite(LED_BUILTIN, LOW);  // Turn the LED on
    if (msg == "off")
        digitalWrite(LED_BUILTIN, HIGH);  // Turn the LED off
}

void pubTask() {
    // Publish a "led" "on/off" message every 500ms
    static int s1 = 0;
    static unsigned long t1;
    if (ustd::timeDiff(t1, millis()) > 500L) {
        if (s1 == 0) {
            s1 = 1;
            sched.publish("led", "on");
        } else {
            s1 = 0;
            sched.publish("led", "off");
        }
        t1 = millis();
    }
    if (t1 == 0)
        t1 = millis();
}

void setup() {
    pinMode(LED_BUILTIN, OUTPUT);

    // Create a task for general use:
    int tID = sched.add(appLoop, "main");

    // Subscribe to "led" messages. subMsg will be called on receive of "led"
    // dmessages.
    sched.subscribe(tID, "led", subMsg);

    // Create a task that will publish "led" messages
    sched.add(pubTask, "pubTask", 50000L);
}

void appLoop() {
    // your code goes here.
}

// Never add code to this loop, use appLoop() instead.
void loop() {
    sched.loop();
}
~~~
 */

class Scheduler {
  private:
    friend class Console;
    ustd::array<T_TASKENTRY> taskList;
    ustd::queue<T_MSG *> msgqueue;
    ustd::array<T_SUBSCRIPTION> subscriptionList;
    int subscriptionHandle;
    int taskID;
    bool bSingleTaskMode = false;
    int singleTaskID = -1;
    bool bGenStats = false;
    unsigned long statIntervallMs = 0;
    unsigned long statTimer;
    // unsigned long idleTime = 0;
    unsigned long systemTimer;
    unsigned long systemTime = 0;
    unsigned long appTimer;
    unsigned long appTime = 0;
    unsigned long mainTime = 0;  // Time spent with SCHEDULER_MAIN id.
    unsigned long upTime = 0;    // Seconds system is running
    unsigned long upTimeTicker = 0;
    int currentTaskID = -2;  // TaskID that is currently been executed

  public:
    Scheduler(int nTaskListSize = 2, int queueSize = 2, int nSubscriptionListSize = 2)
        : taskList(nTaskListSize), msgqueue(queueSize), subscriptionList(nSubscriptionListSize) {
        /*! Instantiate a cooperative scheduler
         *
         * All list sizes are optional, they will be dynamically incremented, if
         * necessary.
         */
        subscriptionHandle = 0;
        taskID = 0;  // 0 is SCHEDULER_MAIN
        upTime = 0;
        upTimeTicker = micros();
#ifndef __ATTINY__
        resetStats(true);
#endif

#if defined(__ESP__) && !defined(__ESP32__)
        ESP.wdtDisable();
        ESP.wdtEnable(WDTO_8S);
#endif
    }

#ifndef __ATTINY__
    virtual ~Scheduler() {
        for (unsigned int i = 0; i < taskList.length(); i++) {
            if (taskList[i].szName != nullptr)
                free(taskList[i].szName);
        }
        int l = msgqueue.length();
        for (int i = 0; i < l; i++) {
            msgqueue.pop();
        }
    }
#endif

    static bool mqttmatch(const String pubstr, const String substr) {
        /*! compare publish and subscribe topics.
         *
         * subscriptions can contain the MQTT wildcards '#' and '+'.
         * @param pubstr Topic that is being published. No wildcards allowed.
         * @param substr Topics that are subscribed, allows MQTT wildcards '#'
         * and '*'.
         * @return true, if pubstr matches substr, false otherwise.
         */
        if (pubstr == substr)
            return true;

        // Attiny compile: core needs to be extended, add c_str():
        // In WString.h add:  const char *c_str() const { return buffer; }
        const char *pub = (const char *)pubstr.c_str();
        const char *sub = (const char *)substr.c_str();

        int lp = strlen(pub);
        int ls = strlen(sub);

        bool wPos = true;  // sub wildcard is legal now
        int ps = 0;
        for (int pp = 0; pp < lp; pp++) {
            if (pub[pp] == '+' || pub[pp] == '#') {
                return false;  // Illegal wildcards in pub
            }
            if (wPos) {
                wPos = false;
                if (sub[ps] == '#') {
                    if (ps == ls - 1) {
                        return true;
                    } else {
                        return false;  // In sub, # must not be followed by
                                       // anything else
                    }
                }
                if (sub[ps] == '+') {
                    while (pp < lp && pub[pp] != '/')
                        ++pp;
                    ++ps;
                    if (pp == lp) {
                        if (ps == ls) {
                            return true;
                        } else if (!strcmp(&sub[ps], "/#")) {
                            return true;
                        }
                    }
                }
            } else {
                if (sub[ps] == '+' || sub[ps] == '#') {
                    return false;  // Illegal wildcard-position
                }
            }
            if (pub[pp] != sub[ps] && strcmp(&sub[ps], "/#")) {
                return false;
            }
            if (pub[pp] == '/')
                wPos = true;
            if (pp == lp - 1) {
                if (ps == ls - 1) {
                    return true;
                }
                if (!strcmp(&sub[ps + 1], "/#") || !strcmp(&sub[ps + 1], "#") ||
                    !strcmp(&sub[ps + 1], "+")) {
                    return true;
                }
                return false;
            }
            ++ps;
        }
        if (ps == ls) {
            return true;
        } else {
            return false;
        }
    }

  private:
    int getIndexFromTaskID(int taskID) {
        for (unsigned int i = 0; i < taskList.length(); i++) {
            if (taskList[i].taskID == taskID) {
                return i;
            }
        }
        return -1;
    }

#ifndef __ATTINY__
    bool schedReceive(const char *topic, const char *msg) {
        const char *p0, *p1;
        p0 = topic ? topic : "";
        p1 = strchr(p0, '/');
        if (p1) {
            ++p1;
            if (!strcmp(p1, "stat/get")) {
                statIntervallMs = msg ? atoi(msg) : 0;
                if (statIntervallMs) {
                    bGenStats = true;
                    resetStats(true);
                } else
                    bGenStats = false;
                return true;
            }
        }
        return false;
    }
#endif

  public:
    bool publish(String topic, String msg = "", String originator = "") {
        /*! publish a message to a given topic
         *
         * @param topic MQTT-style topic of the message (no wildcards allowed)
         * @param msg Message content
         * @param originator Optional name of originator-task
         * @return true on successful publish.
         */
#ifndef __ATTINY__
        if (!strncmp(topic.c_str(), "$SYS", 4))
            if (schedReceive(topic.c_str(), msg.c_str()))
                return true;
#endif
        T_MSG *pMsg = (T_MSG *)malloc(sizeof(T_MSG) +
                                      (3 + originator.length() + topic.length() + msg.length()) *
                                          sizeof(char));
        if (pMsg) {
            pMsg->originator = (char *)(&pMsg[1]);
            pMsg->topic = pMsg->originator + ((originator.length() + 1) * sizeof(char));
            pMsg->msg = pMsg->topic + ((topic.length() + 1) * sizeof(char));
            strcpy(pMsg->originator, originator.c_str());
            strcpy(pMsg->topic, topic.c_str());
            strcpy(pMsg->msg, msg.c_str());
            return msgqueue.push(pMsg);
        }
        return false;
    }

    int subscribe(int taskID, String topic, T_SUBS subs, String originator = "") {
        /*! Subscribe to a topic to receive messages published to this topic
         *
         * @param taskID taskID of the task that is associated with this
         * subscriptions (only used for statistics)
         * @param topic MQTT-style topic to be subscribed, can contain MQTT
         * wildcards '#' and '*'. (A subscription to '#' receives all pubs)
         * @param subs Callback of type void myCallback(String topic, String
         * msg, String originator) that is called, if a matching message is
         * received. On ESP or Unixoid platforms, this can be a member function.
         * @param originator Optional name of associated task.
         * @return subscriptionHandle on success (needed for unsubscribe), or -1
         * on error.
         */
        T_SUBSCRIPTION sub;
        memset(&sub, 0, sizeof(sub));
        sub.taskID = taskID;
        sub.subs = subs;
        sub.subscriptionHandle = subscriptionHandle + 1;
        sub.topic = (char *)malloc((topic.length() + originator.length() + 2) * sizeof(char));
        if (sub.topic) {
            sub.originator = sub.topic + ((topic.length() + 1) * sizeof(char));
            strcpy(sub.topic, topic.c_str());
            strcpy(sub.originator, originator.c_str());
            if (subscriptionList.add(sub) != -1) {
                ++subscriptionHandle;
                return subscriptionHandle;
            }
            // free up memory
            free(sub.topic);
        }
        return -1;
    }

    bool unsubscribe(int subscriptionHandle) {
        /*! Unsubscribe a subscription
         *
         * @param subscriptionHandle Handle to subscription as returned by
         * Subscribe
         * @return true on successful unsubscription, false if no corresponding
         * subscription is found.
         */
        for (unsigned int i = 0; i < subscriptionList.length(); i++) {
            if (subscriptionList[i].subscriptionHandle == subscriptionHandle) {
                free(subscriptionList[i].topic);
                subscriptionList.erase(i);
                return true;
            }
        }
        return false;
    }

  private:
    void checkMsgQueue() {
        T_MSG *pMsg;
        while ((pMsg = msgqueue.pop()) != nullptr) {
            for (unsigned int i = 0; i < subscriptionList.length(); i++) {
                if (mqttmatch(pMsg->topic, subscriptionList[i].topic)) {
                    if (*(pMsg->originator) != 0)
                        if (strcmp(subscriptionList[i].originator, pMsg->originator) == 0) {
                            continue;
                        }
                    unsigned long startTime = micros();
                    subscriptionList[i].subs(pMsg->topic, pMsg->msg, pMsg->originator);

                    if (subscriptionList[i].taskID != SCHEDULER_MAIN) {
                        int ind = getIndexFromTaskID(subscriptionList[i].taskID);
                        if (ind != -1)
                            taskList[ind].cpuTime += timeDiff(startTime, micros());
                    } else {
                        mainTime += timeDiff(startTime, micros());
                    }
                }
            }
            free(pMsg);
        }
    }

  public:
    int add(T_TASK task, String name, unsigned long minMicroSecs = 100000L,
            T_PRIO prio = PRIO_NORMAL) {
        /*! Add a task to the schedule
         *
         * @param task Task function of type void myTask() that will be called
         * by the scheduler. This can be a member function for ESP and Unixoid
         * platforms (Functional support).
         * @param name Task name (for statistics)
         * @param minMicroSecs Task function is called every minMicroSecs. Note
         * this is not guaranteed, because it's a cooperative scheduler.
         * @param prio Not yet supported.
         * @return taskID is successful, -1 on error.
         */
        T_TASKENTRY taskEnt;
        memset(&taskEnt, 0, sizeof(taskEnt));
        taskEnt.taskID = taskID + 1;
        taskEnt.task = task;
        taskEnt.minMicros = minMicroSecs;
        taskEnt.prio = prio;
        if (name.length()) {
            taskEnt.szName = (char *)malloc(name.length() + 1);
            if (!taskEnt.szName) {
                return -1;
            }
            strcpy(taskEnt.szName, name.c_str());
        } else {
            taskEnt.szName = nullptr;
        }
        if (taskList.add(taskEnt) >= 0) {
            ++taskID;
            return taskID;
        }
        if (taskEnt.szName) {
            free(taskEnt.szName);
        }
        return -1;
    }

    bool remove(int taskID) {
        /*! Remove an existing task
         *
         * @param taskID Remove the corresponding task from scheduler
         *
         * Note: a task can't delete itself while being executed.
         *
         * @return true, if task was found and removed, false on error
         */
        if (currentTaskID == taskID) {
            return false;  // A task can't delete itself.
        }
        for (unsigned int i = 0; i < taskList.length(); i++) {
            if (taskList[i].taskID == taskID) {
                if (taskList[i].szName != nullptr)
                    free(taskList[i].szName);
                taskList.erase(i);
                return true;
            }
        }
        return false;
    }

    bool reschedule(int taskID, unsigned long minMicroSecs = 100000L, T_PRIO prio = PRIO_NORMAL) {
        /*! Reschedule an existing task
         *
         * @param taskID Task ID to be rescheduled
         * @param minMicroSecs New schedule: task function is called every minMicroSecs.
         * If minMicrosSecs is set to zero, the task's process-function is not called any more.
         * Note: this is not guaranteed, because it's a cooperative scheduler.
         * @param prio Not yet supported.
         * @return true, if task was found and rescheduled, false on error
         */
        for (unsigned int i = 0; i < taskList.length(); i++) {
            if (taskList[i].taskID == taskID) {
                taskList[i].minMicros = minMicroSecs;
                return true;
            }
        }
        return false;
    }

    unsigned long getUptime() {
        /*! Get uptime in seconds
        *
        $ @return Returns the number of seconds passed since system start.
        */
        return upTime;
    }

    void singleTaskMode(int _singleTaskID) {
        /*! Instruct scheduler to go into single-task mode
         *
         * @param _singleTaskID taskID of the task that should be executed
         * exclusively. This is useful for time-critical procedures like OTA
         * firmware updates. If _singleTaskID is -1, normal schedule operation
         * of all existing tasks resumed.
         */
        singleTaskID = _singleTaskID;
        if (_singleTaskID == -1) {
            bSingleTaskMode = false;
        } else {
            bSingleTaskMode = true;
        }
    }

  private:
    void runTask(T_TASKENTRY *pTaskEnt) {
        unsigned long startTime = micros();
        unsigned long tDelta = timeDiff(pTaskEnt->lastCall, startTime);
        if (tDelta >= pTaskEnt->minMicros && pTaskEnt->minMicros) {
            currentTaskID = pTaskEnt->taskID;  // prevent task() to delete itself.
            pTaskEnt->task();
            currentTaskID = -2;
            pTaskEnt->lastCall = startTime;
            pTaskEnt->lateTime += tDelta - pTaskEnt->minMicros;
            pTaskEnt->cpuTime += timeDiff(startTime, micros());
            ++pTaskEnt->callCount;
        }
    }

#ifndef __ATTINY__
    void resetStats(bool bHard = false) {
        for (unsigned int i = 0; i < taskList.length(); i++) {
            taskList[i].cpuTime = 0;
            taskList[i].lateTime = 0;
            taskList[i].callCount = 0;
        }
        statTimer = micros();
        if (bHard)
            systemTimer = micros();
        systemTime = 0;
        appTime = 0;
        mainTime = 0;
    }

    void checkStats() {
        if (!bGenStats || !statIntervallMs)
            return;
        unsigned long now = micros();
        unsigned long tDelta = timeDiff(statTimer, now);
        unsigned long mem = (unsigned long)freeMemory();
        if (tDelta > statIntervallMs * 1000) {
            // local stats
            for (unsigned int i = 0; i < taskList.length(); i++) {
            }
            // mqtt stats
            const char *null_name = "<null>";
            const char *skeleton_head =
                "{\"dt\":%ld,\"syt\":%ld,\"apt\":%ld,"
                "\"mat\":%ld,\"upt\":%ld,\"mem\":%ld,\"tsks\":%ld,\"tdt\":[";
            const char *skeleton_tail = "]}";
            const char *bone = "[\"%s\",%ld,%ld,%ld,%ld,%ld],";
            unsigned long memreq =
                strlen(skeleton_head) + 7 * 7 + (strlen(bone) + 7 * 5) * taskList.length();
            for (unsigned int i = 0; i < taskList.length(); i++) {
                if (taskList[i].szName == nullptr)
                    memreq += strlen(null_name);
                else
                    memreq += strlen(taskList[i].szName);
            }
            memreq += 1;
            char *jsonstr = (char *)malloc(memreq);
            if (jsonstr != nullptr) {
                memset(jsonstr, 0, memreq);
                sprintf(jsonstr, skeleton_head, tDelta, systemTime, appTime, mainTime, upTime, mem,
                        taskList.length());
                for (unsigned int i = 0; i < taskList.length(); i++) {
                    char *p = &jsonstr[strlen(jsonstr)];
                    if (taskList[i].szName == nullptr) {
                        sprintf(p, bone, null_name, (long)taskList[i].taskID, taskList[i].minMicros,
                                taskList[i].callCount, taskList[i].cpuTime, taskList[i].lateTime);
                    } else {
                        sprintf(p, bone, taskList[i].szName, (long)taskList[i].taskID,
                                taskList[i].minMicros, taskList[i].callCount, taskList[i].cpuTime,
                                taskList[i].lateTime);
                    }
                }
                char *p = &jsonstr[strlen(jsonstr)];
                if (taskList.length() > 0)
                    --p;  // no final ','
                strcpy(p, skeleton_tail);

                //#ifdef USE_SERIAL_DBG
                // Serial.print(memreq);
                // Serial.print(" ");
                // Serial.println(strlen(jsonstr));
                // Serial.println(jsonstr);
                // char brz[50];
                // sprintf(brz, "%ld,%ld", memreq, strlen(jsonstr));
                // String msg = "JSONMEM: " + String(brz);
                // publish("$SYS/stat", msg, "scheduler-dbg");
                //#endif
                publish("$SYS/stat", jsonstr, "scheduler");
                free(jsonstr);
            }
            resetStats(false);
        }
    }
#endif

  public:
    void loop() {
        /*! Main scheduler loop
         * This loop() function should be called in Arduino's loop() function.
         * Preferably no other code should be in Arduino's loop().
         */
        unsigned long current = micros();
        if (timeDiff(upTimeTicker, current) > 1000000L) {
            upTime += 1;
            upTimeTicker += 1000000;
        }
        systemTime += timeDiff(systemTimer, current);
        appTimer = current;
        if (!bSingleTaskMode) {
#ifndef __ATTINY__
            checkStats();
#endif
            checkMsgQueue();
        }
        for (unsigned int i = 0; i < taskList.length(); i++) {
            if (!bSingleTaskMode) {
                checkMsgQueue();
                runTask(&taskList[i]);
            } else {
                if (taskList[i].taskID == singleTaskID) {
                    runTask(&taskList[i]);
                }
            }
#if defined(__ESP__) && !defined(__ESP32__)
            appTime += timeDiff(appTimer, micros());
            systemTimer = micros();
            yield();
            systemTime += timeDiff(systemTimer, micros());
            appTimer = micros();
#endif
        }
        appTime += timeDiff(appTimer, micros());
        systemTimer = micros();
#if defined(__ESP__) && !defined(__ESP32__)
        ESP.wdtFeed();
#endif
    }
};  // namespace ustd
}  // namespace ustd
