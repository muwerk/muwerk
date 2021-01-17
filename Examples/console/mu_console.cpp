#define USE_SERIAL_DBG 1

// Make sure to use a platform define before following includes
#include "platform.h"
#include "scheduler.h"
#include "heartbeat.h"
#include "console.h"

ustd::Scheduler sched;
ustd::Console console;

int blinkerID;
void appLoop();

void task0(String topic, String msg, String originator) {
    static bool led = false;

    // when receiving a message published to subscribed topic "led", do:
    if (msg == "on") {
        digitalWrite(LED_BUILTIN, LOW);  // Turn the LED on
        led = true;
    }
    if (msg == "off") {
        digitalWrite(LED_BUILTIN, HIGH);  // Turn the LED off
        led = false;
    }
}

void task1() {                                // scheduled every 50ms
    static ustd::heartbeat intervall = 500L;  // 500 msec

    if (intervall.beat()) {
        if (ison == false) {
            ison = true;
            sched.publish("led", "on");  // MQTT-style publish to topic "led"
            // If munet with mqtt is used, those messages are transparently send
            // to an external MQTT-server, by default, muwerk only dispatches
            // them between local tasks.
        } else {
            ison = false;
            sched.publish("led", "off");
        }
    }
}

void command0(String cmd, String args) {
    // extract first argument
    String arg1 = ustd::shift(args);
    arg1.toLowerCase();

    if (arg1 == "on" && blinkerID == -1) {
        // start blinker task
        blinkerID = sched.add(task1, "task1", 50000L);
        Serial.println("\nLED blinker is switched on");
    } else if (arg1 == "off" && blinkerID != -1) {
        // stop blinker task
        if (sched.remove(blinkerID)) {
            blinkerID = -1;
        }
        Serial.println("\nLED blinker is switched off");
    } else if (arg1 == "toggle") {
        // toggle led
        command0(cmd, blinkerID == -1 ? "on" : "off");
    } else if (arg1 == "") {
        // show led status
        if (blinkerID == -1)
            Serial.println("\nLED blinker is off");
        else
            Serial.println("\nLED blinker is on");

    } else if (arg1 == "-h") {
        // show help
        Serial.println("\nusage: led [on | off | toggle]");
    } else {
        Serial.println("\nInvalid option " + arg1 + " supplied");
    }
}

void setup() {
    Serial.begin(115200);

    pinMode(LED_BUILTIN, OUTPUT);

    console.extend("led", command0);
    console.begin(&sched);

    int tID = sched.add(appLoop, "main");

    sched.subscribe(tID, "led", task0);             // subscribe to MQTT-like top "led"
    blinkerID = sched.add(task1, "task1", 50000L);  // execute task1 every 50ms (50000us)
}

void appLoop() {
    // your code goes here.
}

// Never add code to this loop, use appLoop() instead.
void loop() {
    sched.loop();
}