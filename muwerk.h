// mutils.h - muwerk utility functions

#pragma once

/*! \mainpage muwerk is a cooperative scheduler with MQTT-like communication queues
\section Introduction

muwerk implements the following classes:

* * \ref ustd::jsonfile A utility class for easily managing data stored in JSON files
* * \ref ustd::heartbeat A utility class for handling periodical operations at fixed intervals
* * \ref ustd::Scheduler A cooperative scheduler and MQTT-like queues
* * \ref ustd::sensorprocessor An exponential sensor value filter
* * \ref ustd::SerialConsole A serial debug console for the scheduler
* * \ref ustd::timeout and \ref ustd::utimeout Utility classes for handling timeouts

Some additional utility functions are avaiable in the \ref ustd namespace by including
muwerk.h

Libraries are header-only and should work with any c++11 compiler
and support platforms starting with 8k attiny, avr, arduinos, up to esp8266
amd esp32.

This library requires the ustd library (for `array`, `map` and `queue`) and requires a
<a href="https://github.com/muwerk/ustd/blob/master/README.md">platform
define</a>.

\section Reference
<a href="https://github.com/muwerk/muwerk">muwerk github repository</a>

depends on:
* * <a href="https://github.com/muwerk/ustd">ustd github repository</a>

used by:
* * <a href="https://github.com/muwerk/munet">munet github repository</a>
*/

#include "platform.h"
#include "array.h"

namespace ustd {

unsigned long timeDiff(unsigned long first, unsigned long second) {
    /*! Calculate time difference between two time values
     *
     * This function calculates the difference between two time values honouring
     * overflow conditions. This always works under the assumption that:
     * 1. the first value represents an earlier time as the second value
     * 2. the difference between the first and second value is lesser that the
     *    maximum value that `unsigned long` can hold.
     *
     * @param first first time value
     * @param second second time value
     * @return the time difference between the two values
     */
    if (second >= first)
        return second - first;
    return (unsigned long)-1 - first + second + 1;
}

void split(String &src, char delimiter, array<String> &result) {
    /*! Split a String into an array of segamnts using a specified delimiter.
    @param src Source String.
    @param delimiter Delimiter for splitting the string.
    @param result Array if strings holding the result of the operation.
    */
    int ind;
    String source = src;
    String sb;
    while (true) {
        ind = source.indexOf(delimiter);
        if (ind == -1) {
            result.add(source);
            return;
        } else {
            sb = source.substring(0, ind);
            result.add(sb);
            source = source.substring(ind + 1);
        }
    }
}

String shift(String &src, char delimiter = ' ', String defValue = "") {
    /*! Extract the first argument from the supplied string using a given delimiter
     * @param src The string object from which to shift out an argument
     * @param delimiter (optional, default whitespace) The delimiter character used for separating
     * arguments
     * @param defValue (optional, default empty string) Default value to return if no more arguments
     * available in the source string
     * @return The extracted arg
     */
    if (src == "") {
        return defValue;
    }
    int ind = src.indexOf(delimiter);
    String ret = defValue;
    if (ind == -1) {
        ret = src;
        src = "";
    } else {
        ret = src.substring(0, ind);
        src = src.substring(ind + 1);
        src.trim();
    }
    return ret;
}

}  // namespace ustd