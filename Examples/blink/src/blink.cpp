#define USE_SERIAL_DBG 1

#include "platform.h"
#include "scheduler.h"
#include "net.h"
#include "mqtt.h"

void appLoop();

ustd::Scheduler sched;  // = ustd::scheduler();
int s1 = 0;
unsigned long t1 = 0;

#if defined(__ATTINY__)
#define LED_BUILTIN (0)  // Pin 2 (neben Vcc)
#endif

#if defined(__ESP32__)
#define LED_BUILTIN (5)
#endif

int led;

#if defined(__ESP__)
int connectLed;

ustd::Net net;

void netconnect(String topic, String msg, String originator) {
    digitalWrite(connectLed, HIGH);  // Turn the LED on
}
#endif

void task0(String topic, String msg, String originator) {
    if (msg == "on")
        digitalWrite(led, LOW);  // Turn the LED on
    if (msg == "off")
        digitalWrite(led, HIGH);  // Turn the LED off
}

#if defined(__ATTINY__)
#define rx (1)
#define tx (2)
SoftwareSerial bl4(rx, tx);

void taskSerial() {
    int b;
    digitalWrite(led, LOW);
    if (bl4.available()) {
        b = bl4.read();
        if (b != -1) {
            ++b;
            bl4.write(b);
            digitalWrite(led, HIGH);
            delay(50);
            digitalWrite(led, LOW);
        } else {
            bl4.write(0xff);
            digitalWrite(led, HIGH);
            delay(500);
            digitalWrite(led, LOW);
        }
    }
}
#endif

void task1() {
    if (ustd::timeDiff(t1, millis()) > 500L) {
        if (s1 == 0) {
            s1 = 1;
            sched.publish("led", "on");
        } else {
            s1 = 0;
            sched.publish("led", "off");
        }
        t1 = millis();
        // delay(100);
    }
    if (t1 == 0)
        t1 = millis();
}

#ifndef __ATTINY__
typedef struct t_testcase {
    String pub;
    String sub;
    bool groundTruth;
} T_TESTCASE;

T_TESTCASE tcs[] = {
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
    if (sched.mqttmatch(tc.pub, tc.sub) != tc.groundTruth) {
        if (tc.groundTruth)
            gt = "true";
        String msg =
            tc.pub + "<->" + tc.sub + ", groundTruth=" + gt + ": ERROR.";
#ifdef USE_SERIAL_DBG
        Serial.println(msg.c_str());
#endif
        ++errs;
    } else {
        String msg = tc.pub + "<->" + tc.sub + ", groundTruth=" + gt + ": OK.";
#ifdef USE_SERIAL_DBG
        Serial.println(msg.c_str());
#endif
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
#endif  // ATTINY

int tID;

void setup() {
#ifdef __ATTINY__
    bl4.begin(9600);
#else
#ifdef USE_SERIAL_DBG
    Serial.begin(115200);
    Serial.println();
    Serial.println("Startup");
#endif  // USE_SERIAL_DBG
#endif
    led = LED_BUILTIN;
    pinMode(led, OUTPUT);
#if defined(__ESP__)
    connectLed = 14;
    pinMode(connectLed, OUTPUT);
    digitalWrite(connectLed, LOW);  // Turn the LED off
#endif
    for (int i = 0; i < 10; i++) {
        digitalWrite(led, LOW);  // Turn the LED on
        delay(50);
        digitalWrite(led, HIGH);  // Turn the LED off
        delay(50);
    }

#ifdef __ESP__
    testcases();
#endif

    tID = sched.add(appLoop, "main");

    sched.subscribe(tID, "led", task0);
#if defined(__ESP__)
    sched.subscribe(tID, "net/rssi", netconnect);
#endif
#ifndef __ATTINY__
    sched.add(task1, "task1", 50000L);
#endif
#if defined(__ESP__)
    Serial.println("Starting net.begin");
    net.begin(&sched);
    Serial.println("Done net.begin");
#endif
#ifdef __ATTINY__
    delay(500);
    sched.add(taskSerial, "serial", 50000L);
#endif
#ifndef __ATTINY__
    Serial.println("Setup complete");
#endif
}

void appLoop() {
    // your code goes here.
}

// Never add code to this loop, use appLoop() instead.
void loop() {
    sched.loop();
}
