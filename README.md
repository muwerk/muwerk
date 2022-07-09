muwerk
======

[![ESP12e build](https://travis-ci.org/muwerk/muwerk.svg?branch=master)](https://travis-ci.org/muwerk/muwerk)
[![Dev Docs](https://img.shields.io/badge/docs-dev-blue.svg)](https://muwerk.github.io/muwerk/docs/index.html)
[![CMake](https://github.com/muwerk/muwerk/workflows/CMake/badge.svg)](https://github.com/muwerk/muwerk/actions)
[![PlatformIO CI](https://github.com/muwerk/muwerk/workflows/PlatformIO%20CI/badge.svg)](https://github.com/muwerk/muwerk/actions)
[![Console](https://github.com/muwerk/muwerk/workflows/Console/badge.svg)](https://github.com/muwerk/muwerk/actions)
[![Raspberry_Pico](https://github.com/muwerk/muwerk/workflows/Raspberry_Pico/badge.svg)](https://github.com/muwerk/muwerk/actions)

muwerk is a cooperative scheduler with mqtt-like queues.

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
    "dt" : 500001, "syt" : 57340, "apt" : 347452, "mat" : 10, "upt":2, "mem":2147483647, "tsks" : 2,
        "tdt" : [["task1", 1, 50000, 10, 99240, 7], ["task2", 2, 75000, 7, 34937, 0]]}
```

| Field | Explanation                                                                                                                                                                                                                                                                                              |
| ----- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| dt    | µsec since last stat sample                                                                                                                                                                                                                                                                              |
| syt   | time in usec used by OS                                                                                                                                                                                                                                                                                  |
| apt   | time in usec used by mwerk tasks                                                                                                                                                                                                                                                                         |
| mat   | time in usec for housekeeping                                                                                                                                                                                                                                                                            |
| upt   | uptime of system in seconds                                                                                                                                                                                                                                                                              |
| mem   | free memory, max. INT_MAX for unixoids                                                                                                                                                                                                                                                                   |
| tsks  | number of muwerk tasks `tn`                                                                                                                                                                                                                                                                              |
| tdt   | array of `tn` entries for each task, containing: task-name `tname` , `tid` taskID of process, `sched_time` scheduling time, number of times task was executed during sample time `cn`, usecs used by this task during this sample `sct`, accumulated usecs task execution was later than scheduled `slt` |

This example shows a `dt=500ms` sample, it has two tasks, `task1`, `id=1` was called
10 times, every 50ms, and used average 9.9240ms per call (task-code has approx. 10ms sleep),
and `task2`, `id=2` was called 7 times, every 75ms, and used average 4.991ms (5ms sleep in code).
Both tasks were always executed as schedules (negligable late-times `cn`).

See `Examples\mac-linux`. (Not available on ATTINY platforms, only ATMEGA
and better).

For systems that are connected to an MQTT-server (via [`munet`](https://github.com/muwerk/munet/blob/master/README.md)),
a python example script [mutop](https://github.com/muwerk/muwerk/tree/master/Examples/mutop) shows
how to parse the statistical information.

MQTT-like Communications and Architecture Overview
--------------------------------------------------

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

Debugging and Troubleshooting
-----------------------------

<img align="right" width="480" src="https://github.com/muwerk/muwerk/blob/master/Resources/console.jpg?raw=true">

The [`Console`](https://muwerk.github.io/muwerk/docs/classustd_1_1Console.html) class allows
to bind a serial console to muwerk that allows to inspect message passing, file system (if
available) and statistical information. See the
[`mu_console`](https://github.com/muwerk/muwerk/blob/master/Examples/console/mu_console.cpp)
example for more information.

Documentation
-------------

* [ustd::muwerk documentation.](https://muwerk.github.io/muwerk/docs/index.html)
* `ustd` required [platform defines.](https://github.com/muwerk/ustd/blob/master/README.md)
* [ustd:: documentation.](https://muwerk.github.io/ustd/docs/index.html)

Related projects:
-----------------

* [ustd](https://github.com/muwerk/ustd/blob/master/README.md) (micro-stdlib), a minimal
  implementation of array, vector and map c++ classes that work on all arduino platforms,
  from 8kb attiny up to ESP32 and Unixoids Mac or Linux (for testing).
* [muwerk](https://github.com/muwerk/muwerk/blob/master/README.md) (microWerk), a cooperative
  scheduler and an MQTT-like communication-queue for all arduino devices (attiny up to ESP32
  [and Unixoids Mac or Linux for testing])
* [munet](https://github.com/muwerk/munet/blob/master/README.md), modules for network
  connectivity for ESP8266 and ESP32 devices, implements Wireless connection to access point,
  NTP time protocol, OTA over-the-air udpate, MQTT-stack (using [PubSubClient]).
* [mupplets](https://github.com/muwerk/mupplets/blob/master/README.md), a number of
  implementations for sensors and io-devices. Mupplets implement processes for muwerk and expose
  muwerk's pub/sub interface to allow other mupplets or apps to access the mupplet's functionality.
  Mupplets can be sensor-drivers or or more specialized modules, e.g. clock functionality for a
  led display.

History
-------
* 0.6.2 (2021-02-xx) (not yet released) Scheduler support for rp2040 Raspberry Pico, fix console.h data type.
* 0.6.1 (2021-02-12)
  * New: numericFunction approximator class: piece-wise linear approximation
    of a function defined by a set of points (x1,y1), (x2, y2)...(xn,yn) for
    calibration etc. 
  * fix: `doctor.h` and `i2cdoctor.h` hat wrong casing for `Arduino_JSON.h` include.
* 0.6.0 (2021-01-30) **Breaking change** for ustd library include: ustd include-files have now `ustd_` prefix to prevent name-clashes with various platform-sdks. [queue.h clashed with ESP8266-Wifi, platform.h clashed with
RISC-V SDK, hence new names `ustd_queue.h` and `ustd_platform.h` etc.]
* 0.5.5 (2021-01-29) Support for all platforms with `Doctor` and `I2CDoctor`. 
* CI (2021-01-28 All supported platforms are build-checked automatically with Github actions.
* 0.5.4 (2021-01-28) Minor fixes:
  * `ustd::Console` and `ustd::SerialConsole` do now honour the USTD_FEATURE_xxx defines.
  * Safe struct inits (no memset())
  * Support for new platforms defined in [ustd](https://github.com/muwerk/ustd#platform-defines) (new platforms for `__ARM__`, `__RISC_V__`)
* 0.5.3 (2021-01-22) `UST_FEATURE_MEM` defines used (requires `ustd` version 0.4.1 or higher), ATtiny T_TASK limited to static functions.
* 0.5.2 (2021-01-20) `library.properties` project name repaired, to allow Arduino automatic update.
* 0.5.1 (2021-01-19)
  * Added `readString` with length validation to `ustd::jsonfile`
  * New task `ustd::doctor` implements a remote diagnostics interface via pub/sub messages.
  * `ustd::Console` has now a better interface for using other Printer-derived stream objects, `ustd::SerialConsole` can now be used with alternative serial interfaces
  * Improvements to stability and memory handling on `ustd::Scheduler`
  * Additional commandline options for `mutop`
* 0.5.0 (2021-01-15) Major update including:
  * New Serial Console class that provides a minimal interactive shell that can be extended with custom commands.
  * New portable filesystem functions that provide abstraction between LittleFS and SPIFFS (and in future others).
  * New class for managing JSON files with possibility to access members with an MQTT-topic-like path syntax.
  * New utility classes `heartbeat` and `timeout`.
  * New utility functions `shift` and `split` for string argument handling.
  * Analysis tool `mutop` that allows to monitor tasks and resource consumption for mqtt connected systems.
  * `$SYS/STAT` format expanded to contain uptime, free memory, and per-task infos task-id and schedule-time.
* 0.4.0 (2021-01-11) Optional serial console for muwerk, file system support for ESP8266 and ESP32.
* 0.3.2 (2020-12-25) Small platform updates, no functional change.
* 0.3.1 (2019-11-29) Compile problem with attiny: resetStats() referenced for attiny.
* 0.3.0 (2019-11-28) Statistical information is no longer flooding serial port, but is published (on demand) to topic `$SYS/stat`. Use publish to `$SYS/stat/get`, message body `number` (as string encoded) to receive stat information every `number` milliseconds.
* 0.2.1 (2019-09-19) Functional support for AVRs via [`ustd::function<>`](https://muwerk.github.io/ustd/docs/index.html).

References
----------

`muwerk` is a derivative and lightweight version of [Meisterwerk](https://github.com/yeasoft/Meisterwerk).
