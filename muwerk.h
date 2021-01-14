// mutils.h - muwerk utility functions

#pragma once

#include "platform.h"
#include "array.h"

namespace ustd {

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