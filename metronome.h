// metronome.h - muwerk metronome class

#pragma once

#include "platform.h"
#include "scheduler.h"  // timeDiff()

namespace ustd {

/*! \brief muwerk Metronome Class

Implements a helper class for handling periodical operations at fixed intervals in
scheduled tasks. The typical use case is in conjunction with a \ref ustd::Scheduler
task: let's assume you have an operation that has to be performed every 15 seconds.

~~~{.cpp}
#include <scheduler.h>
#include <metronome.h>

ustd::Scheduler sched;
ustd::metronome periodical = 15000; // 15 seconds in millliseconds

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
class metronome {
  private:
    unsigned long timerStart;
    unsigned long beatLength;

  public:
    metronome(unsigned long length = 0) : beatLength{length} {
        /*! Creates a metronome
        @param length (optional, default 0) Cycle length in milliseconds
        */
        timerStart = millis();
    }

    metronome &operator=(const unsigned long length) {
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
        /*! Tests if a cycle has completed trying to be synchronous with the
        beat resulting from the configured cycle length.
        @return `0` if the current cycle is not completed or the number of
                cycles passed since the last execution.
        */
        unsigned long now = millis();
        unsigned long diff = ustd::timeDiff(timerStart, now);
        if (beatLength && diff >= beatLength) {
            timerStart = now - (diff % beatLength);
            return diff / beatLength;
        }
        return 0;
    }

    // watchdog style: the specified interval has passed
    unsigned long woof() {
        /*! Tests if a cycle has completed without any compensation. In
        comparison to the function of \ref beat this method behaves more
        like a classical watchdog.
        @return `0` if the current cycle is not completed or the number of
                cycles passed since the last execution.
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
