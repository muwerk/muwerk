// heartbeat.h - muwerk heartbeat class

#pragma once

#include "ustd_platform.h"
#include "muwerk.h"

namespace ustd {

/*! \brief muwerk HeartBeat Class

Implements a helper class for handling periodical operations at fixed intervals in
scheduled tasks. The typical use case is in conjunction with a \ref ustd::Scheduler
task: let's assume you have an operation that has to be performed every 15 seconds.

~~~{.cpp}
#include <scheduler.h>
#include <heartbeat.h>

ustd::Scheduler sched;
ustd::heartbeat periodical = 15000; // 15 seconds in millliseconds

void appLoop();

void someTask() {
    // This is called every 50ms (50000us)
    // Do things here..


    if (periodical.beat()) {
        // this happens each 15 seconds
        // do special things here
    }
}

void setup() {
    // Create a task for the main application loop code:
    int tID = sched.add(appLoop, "main");

    // Create a second task that is called every 50ms
    sched.add(someTask, "someTask", 50000L);
}

~~~

*/
class heartbeat {
  private:
    unsigned long timerStart;
    unsigned long beatLength;

  public:
    heartbeat(unsigned long length = 0) : beatLength{length} {
        /*! Creates a heartbeat
        @param length (optional, default 0) Cycle length in milliseconds
        */
        timerStart = millis();
    }

    heartbeat &operator=(const unsigned long length) {
        /*! Assigns a new cycle length
        @param length Cycle length in milliseconds
        */
        beatLength = length;
        return *this;
    }

    operator unsigned long() const {
        /*! Returns the current cycle length
        @return Cycle length in milliseconds
        */
        return beatLength;
    }

    unsigned long beat() {
        /*! Tests if a cycle has completed trying to be synchronous with the beat resulting from the
        configured cycle length.
        @return `0` if the current cycle is not completed or the number of full cycles passed since
        the last execution.
        */
        unsigned long now = millis();
        unsigned long diff = ustd::timeDiff(timerStart, now);
        if (beatLength && diff >= beatLength) {
            timerStart = now - (diff % beatLength);
            return diff / beatLength;
        }
        return 0;
    }

    unsigned long elapsed() {
        /*! Tests if a cycle has completed without any compensation. In comparison to the function
        of \ref beat this method behaves more like a classical watchdog. It is absolutely guaranteed
        that the previous `elapsed()` has occurred at least one cycle length before.
        @return `0` if the current cycle is not completed or the number of full cycles passed since
        the last execution.
        */
        unsigned long now = millis();
        unsigned long diff = ustd::timeDiff(timerStart, now);
        if (beatLength && diff >= beatLength) {
            timerStart = now;
            return diff / beatLength;
        }
        return 0;
    }
};

}  // namespace ustd
