// metronome.h - ustd metronome class

#pragma once

#include "platform.h"

namespace ustd {

class metronome {
  private:
    unsigned long timerStart;
    unsigned long beatLength;

  public:
    metronome(unsigned long beatLength = 0) : beatLength{beatLength} {
        timerStart = millis();
    }

    metronome &operator=(const unsigned long beatLen) {
        beatLength = beatLen;
        return *this;
    }

    operator unsigned long() const {
        return beatLength;
    }

    unsigned long getlength() const {
        return beatLength;
    }

    void setlength(unsigned long length) {
        beatLength = length;
        timerStart = millis();
    }

    // real metronome: tries to be synchrtonous with the real beat
    unsigned long beat() {
        unsigned long now = millis();
        unsigned long diff = delta(timerStart, now);
        if (beatLength && diff >= beatLength) {
            timerStart = now - (diff % beatLength);
            return diff / beatLength;
        }
        return 0;
    }

    // watchdog style: the specified interval has passed
    unsigned long woof() {
        unsigned long now = millis();
        unsigned long diff = delta(timerStart, now);
        if (beatLength && diff >= beatLength) {
            timerStart = now;
            return diff / beatLength;
        }
        return 0;
    }

    static unsigned long delta(const unsigned long then, const unsigned long now) {
        return now >= then ? now - then : ((unsigned long)-1) - then + now + 1;
    }
};

}  // namespace ustd
