#define USE_SERIAL_DBG 1

// Make sure to use a platform define before following includes
#include "platform.h"
#include "scheduler.h"
#include "console.h"

ustd::Scheduler sched;
ustd::Console console;

bool led;
void appLoop();

void task0(String topic, String msg, String originator) {
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

void task1() {  // scheduled every 50ms
    static int s1 = 0;
    static unsigned long t1;
    if (ustd::timeDiff(t1, millis()) > 500L) {
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
        t1 = millis();
    }
    if (t1 == 0)
        t1 = millis();
}

void command0(String cmd, String args) {
    // extract first argument
    String arg1 = Console::shift(args);
    arg1.toLowerCase();

    if (arg1 == "on" || arg1 == "off") {
        // turn led on or off
        sched.publish("led", arg1);
        Serial.println("\nLED is switched " + arg1);
    } else if (arg1 == "toggle") {
        // toggle led
        sched.publish("led", led ? "off" : "on");
        Serial.println("\nLED is toggled");
    } else if (arg1 == "") {
        // show led status
        if (led)
            Serial.println("\nLED is on");
        else
            Serial.println("\nLED is off");

    } else if (arg1 == "-h") {
        // show help
        Serial.println("\nusage: led [on | off | toggle]");
    } else {
        Serial.println("\nInvalid option " + arg1 + " supplied");
    }
}

void setup() {
    pinMode(LED_BUILTIN, OUTPUT);

    console.extend("led", command0);
    console.begin(&sched);

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
