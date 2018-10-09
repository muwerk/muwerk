# muwerk

Muwerk cooperative scheduler with mqtt-like queues.


## Projekt overview

Muwerk provides a cooperative scheduler that allows fixed-intervall task creation on platforms from attiny up to ESP32 and Linux. (Linux support for testing with debuggers and valgrind.)

Tasks can simply be created by:
```c++
int tID=sched.add(taskSerial, "serial", 50000L);
```
This example creates a task identified by function `void taskSerial()` with task-name `serial` that is executed every 50ms (50000us). On ESP8266 minimum schedule time is around 50us.

The arduino main look simply needs to contain the line:
```c++
void loop() {
    sched.loop();
}
```
This calls the muwerk scheduler who dispatches the registered tasks.

if `USE_SERIAL_DEBUG` is defined, detailed timing information and statistics for each tasks are provided. (Not available on ATTINY platforms). 

A more complete example is available at [muwerk-blink](muwerk/Examples/blob/master/blink/README.md).

Tasks can communicate with each other using an MQTT-like pub/sub mechanism. On ESP8266 or ESP32 platforms, the internal communication can be exported transparently using [munet](muwerk/munet/blob/master/README.md)'s interface to the [PubSubClient](https://github.com/knolleary/pubsubclient) Arduino MQTT library.

Muwerk relies only on [ustd](muwerk/ustd/blob/master/README.md). 

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
|                   muwerk                     |  Cooperative scheduler and  
+----------------------------------------------+    MQTT-like communication (pub/sub)
       |             |                |
+----------------------------------------------+    MQTT-like communication (pub/sub)
|                    ustd                      |  Minimal implementations of Queue, Map (Dicts),
+----------------------------------------------+    Arrays
       |             |                |
+------------+ +------------+ +----------------+  
| Mac, Linux | |Arduino SDK | | ESP8266/32 SDK |  OS and Arduino-Frameworks
+------------+ +------------+ +----------------+
```

## Related projects:

* [ustd](muwerk/ustd/blob/master/README.md) (micro-stdlib), a minimal implementation of array, vector and map c++ classes that work on all arduino platforms, from 8kb attiny up to ESP32 and Unixoids Mac or Linux (for testing).
* [muwerk](muwerk/muwerk/blob/master/README.md) (microWerk), a cooperative scheduler and an MQTT-like communication-queue for all arduino devices (attiny up to ESP32 [and Unixoids Mac or Linux for testing])
* [munet](muwerk/munet/blob/master/README.md), modules for network connectivity for ESP8266 and ESP32 devices, implements Wireless connection to access point, NTP time protocol, OTA over-the-air udpate, MQTT-stack (using [PubSubClient]).
* [mupplets](muwerk/mupplets/blob/master/README.md), a number of implementations for sensors and io-devices. Mupplets implement processes for muwerk and expose muwerk's pub/sub interface to allow other tinkers or Apps to access the mupplet's functionality. Mupplets can be sensor-drivers or or more specialized modules, e.g. clock functionality for a led display.
