#define USE_SERIAL_DBG 1

#include "platform.h"
#include "scheduler.h"

ustd::Scheduler sched;

int led;
void appLoop();

void task0(String topic, String msg, String originator) {

    if (msg == "on")
        digitalWrite(LED_BUILTIN, LOW);  // Turn the LED on
    if (msg == "off")
        digitalWrite(LED_BUILTIN, HIGH);  // Turn the LED off
}

void task1() {
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

    int tID = sched.add(appLoop, "main");
    sched.subscribe(tID, "led", task0);

    sched.add(task1, "task1", 50000L);
}

void appLoop() {
    // your code goes here.
}

// Never add code to this loop, use appLoop() instead.
void loop() {
    sched.loop();
}
