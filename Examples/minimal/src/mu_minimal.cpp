#define USE_SERIAL_DBG 1

// Make sure to use a platform define before following includes
#include "ustd_platform.h"
#include "scheduler.h"
#include "heartbeat.h"

ustd::Scheduler sched;

void appLoop();

void task0(String topic, String msg, String originator) {
    // when receiving a message published to subscribed topic "led", do:
    if (msg == "on")
        digitalWrite(LED_BUILTIN, LOW);  // Turn the LED on
    if (msg == "off")
        digitalWrite(LED_BUILTIN, HIGH);  // Turn the LED off
}

void task1() {  // scheduled every 50ms
    static int s1 = 0;
    static ustd::heartbeat intervall = 500L;  // 500 msec

    if (intervall.beat()) {
        if (s1 == 0) {
            s1 = 1;
            sched.publish("led", "on");  // MQTT-style publish to topic "led"
            // If munet with mqtt is used, those messages are transparently send
            // to an external MQTT-server, by default, muwerk only dispatches
            // them between local tasks.
        } else {
            s1 = 0;
            sched.publish("led", "off");
        }
    }
}

void setup() {
    pinMode(LED_BUILTIN, OUTPUT);

    int tID = sched.add(appLoop, "main");
    sched.subscribe(tID, "led", task0);  // subscribe to MQTT-like top "led"

    sched.add(task1, "task1", 50000L);  // execute task1 every 50ms (50000us)
}

void appLoop() {
    // your code goes here.
}

// Never add code to this loop, use appLoop() instead.
void loop() {
    sched.loop();
}
