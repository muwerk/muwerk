#include <iostream>
#include <list>
#include <string>

#include <stdio.h>
#include <time.h>

#define USE_SERIAL_DBG 1

#include "platform.h"

#include "array.h"
#include "map.h"
#include "queue.h"

#include "scheduler.h"

using std::cout;
using std::endl;

using ustd::array;
using ustd::map;
using ustd::queue;

ustd::Scheduler sched;  // = ustd::Scheduler();

typedef struct t_testcase {
    String pub;
    String sub;
    bool groundTruth;
} T_TESTCASE;

std::list<T_TESTCASE> tcs = {
    {"t1", "t2", false},
    {"t1", "t1", true},
    {"t12", "t1", false},
    {"t1", "t13", false},
    {"t1", "t12", false},
    {"t1", "t1/#", true},
    {"t1", "t1/+", false},
    {"t1/", "t1/#", true},
    {"t1/", "t1/+", true},
    {"t1", "t1/#", true},

    {"t1/t3", "t2/t#", false},
    {"t1/t3", "t2/t+", false},

    {"123/345/567", "#", true},
    {"123/345/567", "+/#", true},
    {"123/345/567", "+/+/+", true},
    {"123/345/567", "+/+/#", true},
    {"123/345/567", "+/+/+/#", true},
    {"123/345/567", "+/+/+/a", false},
    {"123/345/567", "+/345/567", true},
    {"123/45/567", "+/34/567", false},

    {"a", "+", true},
    {"a", "#", true},
    {"", "", true},
    {"a", "", false},
    {"", "a", false},
    {"", "#", false},

    {"abc/def/ghi", "abc/def/ghi", true},
    {"abc/def/ghi", "abc/def/ghi/", false},
    {"abc/def/ghi", "abc/def/gh", false},
    {"abc/def/ghi", "abc/df/ghi", false},
    {"abc/def/ghi", "ab/def/ghi", false},
    {"abc/def/ghi", "abc/def/ghj", false},
    {"abc/def/ghi", "abc/def/ghia", false},
};

unsigned int testcase(T_TESTCASE tc) {
    int errs = 0;
    String gt = "false";
    if (tc.groundTruth)
        gt = "true";
    if (sched.mqttmatch(tc.pub, tc.sub) != tc.groundTruth) {
        Serial.println(tc.pub + "<->" + tc.sub + ", groundTruth=" + gt +
                       ": ERROR.");
        ++errs;
    } else {
        Serial.println(tc.pub + "<->" + tc.sub + ", groundTruth=" + gt +
                       ": OK.");
    }
    return errs;
}

unsigned int testcases() {
    int errs = 0;
    for (auto tc : tcs) {
        errs += testcase(tc);
    }
    return errs;
}

void subs1(String topic, String message, String originator) {
    printf("Subs: %s: %s\n", topic.c_str(), message.c_str());
}

void t1() {
    printf("t1\n");
    String s1 = "t1";
    String s2 = "is working";
    sched.publish(s1, s2);
    unsigned long m1 = millis();
    while (ustd::timeDiff(m1, millis()) < 10)
        ;
}

void t2() {
    printf("t2\n");
    String s1 = "t2";
    String s2 = "is working";
    sched.publish(s1, s2);
    unsigned long m1 = millis();
    while (ustd::timeDiff(m1, millis()) < 5)
        ;
}

int main() {
    cout << "Testing mustd..." << endl;
    array<int> ar = array<int>(1, 100, 1);
    queue<int> qu = queue<int>(128);
    map<String, int> mp = map<String, int>(7, 100, 1);

    for (int i = 0; i < 100; i++) {
        printf("%d ", i);
        ar[i] = i;
        printf(" - ");
        qu.push(int(i));
        printf(" - ");
        mp[std::to_string(i)] = i;
        printf("\n");
    }
    printf("ar len: %d, alloc=%d\n", ar.length(), ar.alloclen());
    printf("qu len: %d, alloc=%d\n", qu.length(), ar.alloclen());
    printf("mp len: %d, alloc=%d\n", mp.length(), ar.alloclen());

    for (int i = 0; i < 100; i++)
        qu.pop();

    bool merr = false;
    for (int i = 0; i < mp.length(); i++) {
        if (mp.keys[i] != std::to_string(i) || i != mp.values[i]) {
            printf("Maps err at %d: %s<->%d\n", i, mp.keys[i].c_str(),
                   mp.values[i]);
            merr = true;
        }
    }
    if (merr)
        printf("Map selftest failed!\n");
    else
        printf("Map selftest ok over %d!\n", mp.length());
    bool aerr = false;
    for (int i = 0; i < ar.length(); i++) {
        if (ar[i] != i) {
            aerr = true;
            printf("Array: err at: %d\n", i);
        }
    }
    if (aerr)
        printf("Array selftest failed!\n");
    else
        printf("Array selftest ok over %d!\n", ar.length());
    cout << "Done ustd." << endl;

    sched.add(t1, "task1", 50000);
    sched.add(t2, "task2", 75000);
    time_t t1 = time(nullptr);
    int sH = sched.subscribe(SCHEDULER_MAIN, "#", subs1);
    unsigned long oldt = -1;
    while (time(nullptr) - t1 < 5) {
        if (time(nullptr) - t1 != oldt) {
            oldt = time(nullptr) - t1;
            Serial.print("========Timestamp: ");
            Serial.println(oldt);
        }
        sched.loop();
    }
    sched.unsubscribe(sH);
    cout << "Done sched test" << endl;

    testcases();

    return 0;
}