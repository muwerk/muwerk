//#define USE_SERIAL_DBG 1

// Make sure to use a platform define before following includes
#include "platform.h"
#include "scheduler.h"
#include "heartbeat.h"
#include "doctor.h"

#if USTD_FEATURE_MEMORY > USTD_FEATURE_MEM_2K
// Protrinket has not enough mem, UNO would work.
#include "i2cdoctor.h"
#endif
#include "console.h"

#if defined(QUIRK_RENAME_SERIAL)
// Feather M0 fix for non-standard port-name
#define Serial Serial5
#endif

ustd::Scheduler sched;

ustd::Doctor doc;
#if USTD_FEATURE_MEMORY > USTD_FEATURE_MEM_2K
ustd::I2CDoctor i2cdoc;
#endif
// Console will use default Serial:
ustd::SerialConsole console;

int blinkerID;
void appLoop();

void task0(String topic, String msg, String originator) {
    // when receiving a message published to subscribed topic "led", do:
    if (msg == "on") {
        digitalWrite(LED_BUILTIN, LOW);  // Turn the LED on
    }
    if (msg == "off") {
        digitalWrite(LED_BUILTIN, HIGH);  // Turn the LED off
    }
}

void task1() {                                // scheduled every 50ms
    static ustd::heartbeat intervall = 500L;  // 500 msec
    static bool ison;

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

#if USTD_FEATURE_MEMORY > USTD_FEATURE_MEM_2K
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
#endif

void setup() {
    Serial.begin(115200);
    doc.begin(&sched);
#if USTD_FEATURE_MEMORY > USTD_FEATURE_MEM_2K
    i2cdoc.begin(&sched);
#endif
    pinMode(LED_BUILTIN, OUTPUT);

#if USTD_FEATURE_MEMORY > USTD_FEATURE_MEM_2K
    console.extend("led", command0);
#endif
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