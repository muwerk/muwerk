muwerk
======


[![ESP12e build](https://travis-ci.org/muwerk/muwerk.svg?branch=master)](https://travis-ci.org/muwerk/muwerk)
[![Dev Docs](https://img.shields.io/badge/docs-dev-blue.svg)](https://muwerk.github.io/muwerk/docs/index.html)

muwerk cooperative scheduler with mqtt-like queues.

Dependencies
------------

muwerk relies only on [ustd](https://github.com/muwerk/ustd). Check documentation for
required [platform defines](https://github.com/muwerk/ustd/blob/master/README.md).

Project overview
----------------

muwerk provides a cooperative scheduler that allows fixed-intervall task creation on
platforms from attiny up to ESP32 and Linux. (Linux support for testing with debuggers
and valgrind.)

Tasks can simply be created by:
```c++
#define __ESP__ 1  // replace with appropriate platform define
#include "scheduler.h"

ustd::Scheduler sched;

void myTask() {
    // add code for this task here
}

void setup() {
    int tID = sched.add(myTask, "taskname", 50000L);
}
```

This example creates a task identified by function `void myTask()` with task-name
`taskname` that is executed every 50ms (50000us). On ESP8266 minimum schedule
time is around 50us.

The arduino main look simply needs to contain the line:
```c++
void loop() {
    sched.loop();
}
```
This calls the muwerk scheduler who dispatches the registered tasks.

Statistics
----------

If a message with topic `$SYS/stat/get` with string-encoded integer `N`
as message is received, the scheduler sends a statistics json object
to topic `$SYS/stat` every `N` milliseconds. If `N` is zero, no more
stat information is published.

Sample stat json (single output-line from `Examples/mac-linux`):

```json
{
    "dt" : 500001, "syt" : 57340, "apt" : 347452, "mat" : 10, "tsks" : 2,
        "tdt" : [["task1", 10, 99240, 7], ["task2", 7, 34937, 0]]}
```

| Field | Explanation                         |
| ----- | ----------------------------------- |
| dt    | µsec since last stat sample         |
| syt   | time in usec used by OS             |
| apt   | time in usec used by mwerk tasks    |
| mat   | time in usec for housekeeping       |
| tsks  | number of muwerk tasks `tn`         |
| tdt   | array of `tn` entries for each task, containing: task-name `tname` , number of times task was executed during sample time `cn`, usecs used by this task during this sample `sct`, accumulated usecs task execution was later than scheduled `slt` |

This example shows a `dt=500ms` sample, it has two tasks, `task1` was called
10 times and used average 9.9240ms per call (task-code has approx. 10ms sleep),
and `task2` was called 7 times and used average 4.991ms (5ms sleep in code).
Both tasks were always executed as schedules (negligable late-times `cn`).

See `Examples\mac-linux`. (Not available on ATTINY platforms, only ATMEGA
and better).

MQTT-like communcations and architecture overview
-------------------------------------------------

A more complete example is available at [blink](https://github.com/muwerk/muwerk/blob/master/Examples/minimal/mu_minimal.cpp)
that shows how tasks can communicate MQTT-style with each other and -- blink a led.

Tasks can communicate with each other using an MQTT-like pub/sub mechanism.
On ESP8266 or ESP32 platforms, the internal communication can be exported
transparently using [munet](https://github.com/muwerk/munet)'s interface to
the [PubSubClient](https://github.com/knolleary/pubsubclient) Arduino MQTT
library. The scheduler's task information is also exported via MQTT.

See the [documentation](https://muwerk.github.io/muwerk/docs/classustd_1_1Scheduler.html)
for more samples on muwerk's task scheduler and pub/sub intertask communication.

```
               +-------------------------------+
               |            Apps               |  Samples
               +-------------------------------+
                              |
               +-------------------------------+
               |          Mupplets             |  Sensors, IO-libs, reusable functional units
               +-------------------------------+
                     |                |
+------------+       |        +----------------+  Munet: ESP8266 and ESP32 only:
|  Testcode  |       |        |  munet (ESPx)  |  Access point client connection, NTP,
+------------+       |        +----------------+    OTA-update, MQTT (via PubSubClient)
       |             |                |
+----------------------------------------------+
|                   muwerk                     |  Cooperative scheduler and task-to-task
+----------------------------------------------+    MQTT-like communication (pub/sub)
       |             |                |
+----------------------------------------------+
|                    ustd                      |  Minimal implementations of Queue, Map (Dicts),
+----------------------------------------------+    Arrays
       |             |                |
+------------+ +------------+ +----------------+
| Mac, Linux | |Arduino SDK | | ESP8266/32 SDK |  OS and Arduino-Frameworks
+------------+ +------------+ +----------------+
```

Documentation
-------------

* [ustd::muwerk documentation.](https://muwerk.github.io/muwerk/docs/index.html)
* `ustd` required [platform defines.](https://github.com/muwerk/ustd/blob/master/README.md)
* [ustd:: documentation.](https://muwerk.github.io/ustd/docs/index.html)

Related projects:
-----------------

* [ustd](https://github.com/muwerk/ustd/blob/master/README.md) (micro-stdlib), a minimal implementation of array, vector and map c++ classes that work on all arduino platforms, from 8kb attiny up to ESP32 and Unixoids Mac or Linux (for testing).
* [muwerk](https://github.com/muwerk/muwerk/blob/master/README.md) (microWerk), a cooperative scheduler and an MQTT-like communication-queue for all arduino devices (attiny up to ESP32 [and Unixoids Mac or Linux for testing])
* [munet](https://github.com/muwerk/munet/blob/master/README.md), modules for network connectivity for ESP8266 and ESP32 devices, implements Wireless connection to access point, NTP time protocol, OTA over-the-air udpate, MQTT-stack (using [PubSubClient]).
* [mupplets](https://github.com/muwerk/mupplets/blob/master/README.md), a number of implementations for sensors and io-devices. Mupplets implement processes for muwerk and expose muwerk's pub/sub interface to allow other mupplets or apps to access the mupplet's functionality. Mupplets can be sensor-drivers or or more specialized modules, e.g. clock functionality for a led display.

History
-------

* 0.3.2 (2020-12-25) Small platform updates, no functional change.
* 0.3.1 (2019-11-29) Compile problem with attiny: resetStats() referenced for attiny.
* 0.3.0 (2019-11-28) Statistical information is no longer flooding serial port, but is published (on demand) to topic `$SYS/stat`. Use publish to `$SYS/stat/get`, message body `number` (as string encoded) to receive stat information every `number` milliseconds.
* 0.2.1 (2019-09-19) Functional support for AVRs via [`ustd::function<>`](https://muwerk.github.io/ustd/docs/index.html).

References
----------

`muwerk` is a derivative and lightweight version of [Meisterwerk](https://github.com/yeasoft/Meisterwerk).
