// timeout.h - muwerk timeout class

#pragma once

#include "platform.h"
#include "scheduler.h"  // timeDiff()

namespace ustd {

/*! \brief muwerk Timeout Class

Implements a helper class for handling timeouts in milliseconds.
See \ref ustd::utimeout for a high precision implmentation with
microsecond resolution.

The following example based on a reconnection timeout illustrates
the usage of this class:

~~~{.cpp}
#include <scheduler.h>
#include <timeout>

ustd::Scheduler sched;
ustd::timeout   connectTimeout = 15000; // 15 seconds in millliseconds

void appLoop();

void someTask() {
    // This is called every 50ms (50000us)
    // Do things here..

    state == getCurrentState();

    if ( state == RECONNECTING ) {
        if ( connectTimeout.test() ) {
            // this happens when the timeout occur
            DBG("Failed to connect");
            // do things here....
        }
    } else if ( state == CONNECTED ) {
        connectTimeout.reset();

    } else if ( state == DISCONNECTED ) {
        state == RECONNECTING
        connectTimeout.reset()
        // start reconnection attempt here
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
class timeout {
  private:
    unsigned long timerStart;
    unsigned long timeoutVal;

  public:
    timeout(unsigned long value = 0) : timeoutVal{value} {
        /*! Creates a timeout
        @param value (optional, default 0) Timeout value in milliseconds.
        */
        timerStart = millis();
    }

    timeout &operator=(const unsigned long value) {
        /*! Assigns a new timeout value
        @param value Timeout value in milliseconds.
        */
        timeoutVal = value;
        return *this;
    }

    operator unsigned long() const {
        /*! Returns the current timeout value
        @return Timeout value in milliseconds.
        */
        return timeoutVal;
    }

    bool test() const {
        /*! Returns if a timeout has occurred
        @return `true` if a timeout has occurred since last \ref reset.
        */
        return ustd::timeDiff(timerStart, millis()) > timeoutVal;
    }

    void reset() {
        //! Resets the timeout
        timerStart = millis();
    }
};

/*! \brief muwerk High Precision Timeout Class

Implements a helper class for handling timeouts in microseconds.
See \ref ustd::timeout for a lower precision implmentation with
millisecond resolution and with a usage example.
*/
class utimeout {
  private:
    unsigned long timerStart;
    unsigned long timeoutVal;

  public:
    utimeout(unsigned long value = 0) : timeoutVal{value} {
        /*! Creates a timout
        @param value (optional, default 0) Timeout value in microseconds.
        */
        timerStart = micros();
    }

    utimeout &operator=(const unsigned long value) {
        /*! Assigns a new timeout value
        @param value Timeout value in microseconds.
        */
        timeoutVal = value;
        return *this;
    }

    operator unsigned long() const {
        /*! Returns the current timeout value
        @return Timeout value in microseconds.
        */
        return timeoutVal;
    }

    bool test() const {
        /*! Returns if a timeout has occurred
        @return `true` if a timeout has occurred since last \ref reset
        */
        return ustd::timeDiff(timerStart, micros()) > timeoutVal;
    }

    void reset() {
        //! Resets the timeout
        timerStart = micros();
    }
};

}  // namespace ustd