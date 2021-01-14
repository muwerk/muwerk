// jsonfile.h - the muwerk json file class

#pragma once

#include "platform.h"
#include "array.h"
#include "map.h"
#include "filesystem.h"

#include <Arduino_JSON.h>  // Platformio lib no. 6249

#ifndef MAX_FRICKEL_DEPTH
#define MAX_FRICKEL_DEPTH 9
#endif

namespace ustd {

/*! \brief muwerk JSON File Class

Implements a class that allows to easyly manage files that contain information
stored in a JSON object. The class implements an internal caching mechanism:
when a file is accessed for the first time, the whole file content is read
and parsed into the internal memory. All subsequent reads are served from
the internally cached data. When writing values, the changes are commited
to the file system based on the initial `autocommit` setting specified
when creating the instance (default: `true`).

This class implements also a set of static functions that allow to perform
atomic operations on JSON files.

Values are referenced by a `key`. This `key` is an MQTT-topic-like path,
structured like this: `filename/a/b/c/d`. e.G.: when reading a value by
specifying this key, the corresponding function will read the jsonfile
`/filename.json` containg the  example content `{"a": {"b": {"c": {"d": false}}}}`
and wil return the value `false`.

### Example of reading multiple values

~~~{.cpp}
    JsonFile jf;
    dhcp = jf.readBool( "net/station/dhcp", false );
    reconnectMaxRetries = jf.readLong( "net/station/maxRetries", 1, 1000000000, 40 );
    connectTimeout = jf.readLong( "net/station/connectTimeout", 3, 3600, 15 );
    bRebootOnContinuedWifiFailure = jf.readBool( "net/station/rebootOnFailure", true );
~~~

### Example of writing multiple values

In this example the values are first written in the cache and then commited
in order to speed up the operation.

~~~{.cpp}
    // full migration
    JsonFile sf;    // object for original file
    JsonFile nf( false );  // no autocommit
    nf.writeLong( "net/version", NET_CONFIG_VERSION );
    nf.writeString( "net/mode", "station" );
    nf.writeString( "net/station/SSID", sf.readString( "net/SSID" ) );
    nf.writeString( "net/station/password", sf.readString( "net/password" ) );
    nf.writeString( "net/station/hostname", sf.readString( "net/hostname" ) );
    ...
    // do not forget to commit!
    nf.commit();
~~~
*/
class JsonFile {
  private:
    bool loaded = false;
    bool forcenew = false;
    bool autocommit = true;
    String path = "/";
    String filename = "";
    JSONVar obj;

  public:
    JsonFile(bool auto_commit = true, bool force_new = false, String path = "/")
        : forcenew(force_new), autocommit(auto_commit), path(path) {
        /*! Creates a new JSON file manager
        @param auto_commit (optional, default is `true`) If set to `false`, all write operations
                           will be postponed until calling the method \ref commit
        @param force_new (optional, default is `false`) If set to `true`, a new file will be
                         created instead of changing the existing one.
        @param path (optional, default is `/`) The path under which the json file will be read
                    or written.
        */
        obj = JSON.parse("{}");
    }

    void clear(bool auto_commit = true, bool force_new = false) {
        /*! Clear the JSON file manager
        @param auto_commit (optional, default is `true`) If set to `false`, all write operations
                           will be postponed until calling the method \ref commit
        @param force_new (optional, default is `false`) If set to `true`, a new file will be
                         created instead of changing the existing one.
        */
        filename = "";
        obj = JSON.parse("{}");
        autocommit = auto_commit;
        forcenew = force_new;
        loaded = false;
    }

    bool init(String basename, JSONVar &value, bool auto_commit = true) {
        /*! Initialize The JSON file manager to a given JSON variable.
        @param basename The basename of the configuration file in which to keep the values.
        @param value JSONVar variable that contains the initial content.
        @param auto_commit (optional, default is `true`) If set to `false`, all
                           write operations will be postponed until calling the
                           method \ref commit
        @return `true` on success.
        */
        filename = basename;
        obj = value;
        autocommit = auto_commit;
        forcenew = true;
        loaded = false;
        return autocommit ? commit() : true;
    }

    bool init(String basename, String value, bool auto_commit = true) {
        /*! Initialize The JSON file manager to a given JSON variable.
        @param basename The basename of the configuration file in which to keep the values.
        @param value String containing a value in JSON format representing the initial content.
        @param auto_commit (optional, default is `true`) If set to `false`, all write operations
                           will be postponed until calling the method \ref commit
        @return `true` on success.
        */
        JSONVar jv = JSON.parse(value);
        return init(basename, jv, auto_commit);
    }

    bool initFromFile(String basename, String fn, bool auto_commit = true) {
        /*! Initialize The JSON file manager to the content on existing JSON file.
        @param basename The basename of the configuration file in which to keep the values.
        @param fn String containing a filename from which to take the initial content.
        @param auto_commit (optional, default is `true`) If set to `false`, all write operations
                           will be postponed until calling the method \ref commit
        @return `true` on success.
        */
        bool result = loadFile(basename, fn);
        if (result) {
            autocommit = auto_commit;
            forcenew = true;
            return autocommit ? commit() : true;
        }
        return false;
    }

    String toString() const {
        /*! Returns the content of the JSON file manager as String in JSON format.
        @return The content of the JSON file manager as String in JSON format.
        */
        return JSON.stringify(obj);
    }

    bool commit() {
        /*! Commit changes to file. If the instance was created with
        `autocommit` set to `true` (default), it is not needed to
        invoke this function explicitely.
        @return `true` on success.
        */
        if (filename == "") {
            DBG("Cannot commit uninitialized object");
            return false;
        }
        String jsonString = JSON.stringify(obj);

        DBG2("Writing file: " + path + filename + ".json, content: " + jsonString);

        fs::File f = fsOpen(path + filename + ".json", "w");
        if (!f) {
            DBG("File " + path + filename + ".json can't be opened for write, failure.");
            return false;
        } else {
            f.print(jsonString.c_str());
            f.close();
            forcenew = false;
            return true;
        }
    }

    bool exists(String key) {
        /*! Test if a value exists in a JSON-file.
        @param key Combined filename and json-object-path.
        @return `true` on success.
        */
        ustd::array<String> keyparts;
        JSONVar subobj;
        if (prepareRead(key, keyparts, subobj)) {
            DBG2("From " + key + ", element found.");
            return true;
        };
        return false;
    }

    static bool atomicExists(String key) {
        /*! Test if a value exists in a JSON-file.
        @param key Combined filename and json-object-path.
        @return `true` on success.
        */
        JsonFile jf;
        return jf.exists(key);
    }

    bool remove(String key) {
        /*! Remove a value from a JSON-file.
        @param key Combined filename and json-object-path.
        @return `true` on success.
        */
        JSONVar target;
        if (!prepareWrite(key, target)) {
            return false;
        }
        target = undefined;
        return autocommit ? commit() : true;
    }

    static bool atomicRemove(String key) {
        /*! Remove a value from a JSON-file.
        @param key Combined filename and json-object-path.
        @return `true` on success.
        */
        JsonFile jf;
        return jf.remove(key);
    }

    bool readJsonVar(String key, JSONVar &value) {
        /*! Read a JSON value from a JSON-file.
        @param key Combined filename and json-object-path.
        @param value JSONVar variable that will receive the read value if found.
                     Since the viariable will stay untouched if the value is not
                     found, it may be initialized with the default value.
        @return `true` on success.
        */
        ustd::array<String> keyparts;
        if (!prepareRead(key, keyparts, value)) {
            return false;
        }
        return true;
    }

    static bool atomicReadJsonVar(String key, JSONVar &value) {
        /*! Read a JSON value from a JSON-file.
        @param key Combined filename and json-object-path.
        @param value JSONVar variable that will receive the read value if found.
                     Since the viariable will stay untouched if the value is not
                     found, it may be initialized with the default value.
        @return `true` on success.
        */
        JsonFile jf;
        return jf.readJsonVar(key, value);
    }

    bool readJsonVarArray(String key, ustd::array<JSONVar> &values) {
        /*! Read an array of JSON values from a JSON-File.
        @param key Combined filename and json-object-path.
        @param value Array of JSONVar that will receive the read value if found.
                     Since the array will stay untouched if the value is not
                     found, it may be initialized with the default value.
        @return `true` on success.
        */
        ustd::array<String> keyparts;
        JSONVar subobj;
        if (!prepareRead(key, keyparts, subobj)) {
            return false;
        }
        if (JSON.typeof(subobj) != "array") {
            DBG("From " + key + ", element has wrong type '" + JSON.typeof(subobj) +
                "' - expected 'array'");
            return false;
        }
        values.resize(subobj.length());
        for (int i = 0; i < subobj.length(); i++) {
            JSONVar element(subobj[i]);
            values[i] = element;
        }
        return true;
    }

    static bool atomicReadJsonVarArray(String key, ustd::array<JSONVar> &values) {
        /*! Read an array of JSON values from a JSON-File.
        @param key Combined filename and json-object-path.
        @param value Array of JSONVar that will receive the read value if found.
                     Since the array will stay untouched if the value is not
                     found, it may be initialized with the default value.
        @return `true` on success.
        */
        JsonFile jf;
        return jf.readJsonVarArray(key, values);
    }

    bool readStringArray(String key, ustd::array<String> &values, bool strict = false) {
        /*! Read an array of strings from a JSON-File.
        @param key Combined filename and json-object-path.
        @param values Array of strings that will receive the read value if found.
                      Since the array will stay untouched if the value is not
                      found, it may be initialized with the default value.
        @param strict (optional, default `false`) If `true` the method will fail also
                      if the array contains any values that are not of type `string`
        @return `true` on success.
        */
        ustd::array<String> keyparts;
        JSONVar subobj;
        if (!prepareRead(key, keyparts, subobj)) {
            return false;
        }
        if (JSON.typeof(subobj) != "array") {
            DBG("From " + key + ", element has wrong type '" + JSON.typeof(subobj) +
                "' - expected 'array'");
            return false;
        }
        if (strict) {
            // check that all elements are strings
            for (int i = 0; i < subobj.length(); i++) {
                if (JSON.typeof(subobj[i]) != "string") {
                    DBG("From " + key + ", array element " + String(i) + " has wrong type '" +
                        JSON.typeof(subobj[i]) + "' - expected 'string'");
                    return false;
                }
            }
        }
        values.resize(subobj.length());
        for (int i = 0; i < subobj.length(); i++) {
            values[i] = String((const char *)subobj[i]);
        }
        return true;
    }

    static bool atomicReadStringArray(String key, ustd::array<String> &values,
                                      bool strict = false) {
        /*! Read an array of strings from a JSON-File.
        @param key Combined filename and json-object-path.
        @param values Array of strings that will receive the read value if found.
                      Since the array will stay untouched if the value is not
                      found, it may be initialized with the default value.
        @param strict (optional, default `false`) If `true` the method will fail also
                      if the array contains any values that are not of type `string`
        @return `true` on success.
        */
        JsonFile jf;
        return jf.readStringArray(key, values, strict);
    }

    bool readBoolArray(String key, ustd::array<bool> &values, bool strict = false) {
        /*! Read an array of boolean values from a JSON-File.
        @param key Combined filename and json-object-path.
        @param values Array of bool that will receive the read value if found.
                      Since the array will stay untouched if the value is not
                      found, it may be initialized with the default value.
        @param strict (optional, default `false`) If `true` the method will fail also
                      if the array contains any values that are not of type `boolean`
        @return `true` on success.
        */
        ustd::array<String> keyparts;
        JSONVar subobj;
        if (!prepareRead(key, keyparts, subobj)) {
            return false;
        }
        if (JSON.typeof(subobj) != "array") {
            DBG("From " + key + ", element has wrong type '" + JSON.typeof(subobj) +
                "' - expected 'array'");
            return false;
        }
        if (strict) {
            // check that all elements are bools
            for (int i = 0; i < subobj.length(); i++) {
                if (JSON.typeof(subobj[i]) != "boolean") {
                    DBG("From " + key + ", array element " + String(i) + " has wrong type '" +
                        JSON.typeof(subobj[i]) + "' - expected 'boolean'");
                    return false;
                }
            }
        }
        values.resize(subobj.length());
        for (int i = 0; i < subobj.length(); i++) {
            values[i] = (bool)subobj[i];
        }
        return true;
    }

    static bool atomicReadBoolArray(String key, ustd::array<bool> &values, bool strict = false) {
        /*! Read an array of boolean values from a JSON-File.
        @param key Combined filename and json-object-path.
        @param values Array of bool that will receive the read value if found.
                      Since the array will stay untouched if the value is not
                      found, it may be initialized with the default value.
        @param strict (optional, default `false`) If `true` the method will fail also
                      if the array contains any values that are not of type `boolean`
        @return `true` on success.
        */
        JsonFile jf;
        return jf.readBoolArray(key, values, strict);
    }

    bool readDoubleArray(String key, ustd::array<double> &values, bool strict = false) {
        /*! Read an array of double precision values from a JSON-File.
        @param key Combined filename and json-object-path.
        @param values Array of double that will receive the read value if found.
                      Since the array will stay untouched if the value is not
                      found, it may be initialized with the default value.
        @param strict (optional, default `false`) If `true` the method will fail also
                      if the array contains any values that are not of type `number`
        @return `true` on success.
        */
        ustd::array<String> keyparts;
        JSONVar subobj;
        if (!prepareRead(key, keyparts, subobj)) {
            return false;
        }
        if (JSON.typeof(subobj) != "array") {
            DBG("From " + key + ", element has wrong type '" + JSON.typeof(subobj) +
                "' - expected 'array'");
            return false;
        }
        if (strict) {
            // check that all elements are bools
            for (int i = 0; i < subobj.length(); i++) {
                if (JSON.typeof(subobj[i]) != "number") {
                    DBG("From " + key + ", array element " + String(i) + " has wrong type '" +
                        JSON.typeof(subobj[i]) + "' - expected 'number'");
                    return false;
                }
            }
        }
        values.resize(subobj.length());
        for (int i = 0; i < subobj.length(); i++) {
            values[i] = (double)subobj[i];
        }
        return true;
    }

    static bool atomicReadDoubleArray(String key, ustd::array<double> &values,
                                      bool strict = false) {
        /*! Read an array of double precision values from a JSON-File.
        @param key Combined filename and json-object-path.
        @param values Array of double that will receive the read value if found.
                      Since the array will stay untouched if the value is not
                      found, it may be initialized with the default value.
        @param strict (optional, default `false`) If `true` the method will fail also
                      if the array contains any values that are not of type `number`
        @return `true` on success.
        */
        JsonFile jf;
        return jf.readDoubleArray(key, values, strict);
    }

    bool readLongArray(String key, ustd::array<long> &values, bool strict = false) {
        /*! Read an array of long integer values from a JSON-File.
        @param key Combined filename and json-object-path.
        @param values Array of long that will receive the read value if found.
                      Since the array will stay untouched if the value is not
                      found, it may be initialized with the default value.
        @param strict (optional, default `false`) If `true` the method will fail also
                      if the array contains any values that are not of type `number`
        @return `true` on success.
        */
        ustd::array<String> keyparts;
        JSONVar subobj;
        if (!prepareRead(key, keyparts, subobj)) {
            return false;
        }
        if (JSON.typeof(subobj) != "array") {
            DBG("From " + key + ", element has wrong type '" + JSON.typeof(subobj) +
                "' - expected 'array'");
            return false;
        }
        if (strict) {
            // check that all elements are bools
            for (int i = 0; i < subobj.length(); i++) {
                if (JSON.typeof(subobj[i]) != "number") {
                    DBG("From " + key + ", array element " + String(i) + " has wrong type '" +
                        JSON.typeof(subobj[i]) + "' - expected 'number'");
                    return false;
                }
            }
        }
        values.resize(subobj.length());
        for (int i = 0; i < subobj.length(); i++) {
            values[i] = (long)subobj[i];
        }
        return true;
    }

    static bool atomicReadLongArray(String key, ustd::array<long> &values, bool strict = false) {
        /*! Read an array of long integer values from a JSON-File.
        @param key Combined filename and json-object-path.
        @param values Array of long that will receive the read value if found.
                      Since the array will stay untouched if the value is not
                      found, it may be initialized with the default value.
        @param strict (optional, default `false`) If `true` the method will fail also
                      if the array contains any values that are not of type `number`
        @return `true` on success.
        */
        JsonFile jf;
        return jf.readLongArray(key, values, strict);
    }

    bool readBool(String key, bool defaultVal) {
        /*! Read a boolean value from a JSON-file.
        @param key Combined filename and json-object-path.
        @param defaultValue value returned, if key is not found.
        @return The requested value or `defaultVal` if value not found.
        */
        ustd::array<String> keyparts;
        JSONVar subobj;
        if (!prepareRead(key, keyparts, subobj)) {
            return defaultVal;
        }
        if (JSON.typeof(subobj) != "boolean") {
            DBG("From " + key + ", element has wrong type '" + JSON.typeof(subobj) +
                "' - expected 'boolean'");
            return defaultVal;
        }
        bool result = (bool)subobj;
        DBG2("From " + key + ", value: " + (result ? "true" : "false"));
        return result;
    }

    static bool atomicReadBool(String key, bool defaultVal) {
        /*! Read a boolean value from a JSON-file.
        @param key Combined filename and json-object-path.
        @param defaultValue value returned, if key is not found.
        @return The requested value or `defaultVal` if value not found.
        */
        JsonFile jf;
        return jf.readBool(key, defaultVal);
    }

    String readString(String key, String defaultVal = "") {
        /*! Read a string value from a JSON-file.
        @param key Combined filename and json-object-path.
        @param defaultValue value returned, if key is not found.
        @return The requested value or `defaultVal` if value not found.
        */
        ustd::array<String> keyparts;
        JSONVar subobj;
        if (!prepareRead(key, keyparts, subobj)) {
            return defaultVal;
        }
        if (JSON.typeof(subobj) != "string") {
            DBG("From " + key + ", element has wrong type '" + JSON.typeof(subobj) +
                "' - expected 'string'");
            return defaultVal;
        }
        String result = (const char *)subobj;
        DBG2("From " + key + ", value: " + result);
        return result;
    }

    static String atomicReadString(String key, String defaultVal = "") {
        /*! Read a string value from a JSON-file.
        @param key Combined filename and json-object-path.
        @param defaultValue value returned, if key is not found.
        @return The requested value or `defaultVal` if value not found.
        */
        JsonFile jf;
        return jf.readString(key, defaultVal);
    }

    double readDouble(String key, double defaultVal) {
        /*! Read a number value from a JSON-file.
        @param key Combined filename and json-object-path.
        @param defaultValue value returned, if key is not found.
        @return The requested value or `defaultVal` if value not found.
        */
        ustd::array<String> keyparts;
        JSONVar subobj;
        if (!prepareRead(key, keyparts, subobj)) {
            return defaultVal;
        }
        if (JSON.typeof(subobj) != "number") {
            DBG("From " + key + ", element has wrong type '" + JSON.typeof(subobj) +
                "' - expected 'number'");
            return defaultVal;
        }
        double result = (double)subobj;
        DBG2("From " + key + ", value: " + String(result));
        return result;
    }

    static double atomicReadDouble(String key, double defaultVal) {
        /*! Read a number value from a JSON-file.
        @param key Combined filename and json-object-path.
        @param defaultValue value returned, if key is not found.
        @return The requested value or `defaultVal` if value not found.
        */
        JsonFile jf;
        return jf.readDouble(key, defaultVal);
    }

    double readDouble(String key, double minVal, double maxVal, double defaultVal) {
        /*! Read a number value from a JSON-file.
        @param key Combined filename and json-object-path.
        @param minVal minimum accepatable value.
        @param maxVal maximum accepatable value.
        @param defaultValue value returned, if key is not found or value outside specified
        bondaries.
        @return The requested value or `defaultVal` if value not found or invalid.
        */
        long val = readDouble(key, defaultVal);
        return (val < minVal || val > maxVal) ? defaultVal : val;
    }

    static double atomicReadDouble(String key, double minVal, double maxVal, double defaultVal) {
        /*! Read a number value from a JSON-file.
        @param key Combined filename and json-object-path.
        @param minVal minimum accepatable value.
        @param maxVal maximum accepatable value.
        @param defaultValue value returned, if key is not found or value outside specified
        bondaries.
        @return The requested value or `defaultVal` if value not found or invalid.
        */
        JsonFile jf;
        return jf.readDouble(key, minVal, maxVal, defaultVal);
    }

    long readLong(String key, long defaultVal) {
        /*! Read a long integer value from a JSON-file.
        @param key Combined filename and json-object-path.
        @param defaultValue value returned, if key is not found.
        @return The requested value or `defaultVal` if value not found.
        */
        return (long)readDouble(key, (double)defaultVal);
    }

    static long atomicReadLong(String key, long defaultVal) {
        /*! Read a long integer value from a JSON-file.
        @param key Combined filename and json-object-path.
        @param defaultValue value returned, if key is not found.
        @return The requested value or `defaultVal` if value not found.
        */
        JsonFile jf;
        return jf.readLong(key, defaultVal);
    }

    long readLong(String key, long minVal, long maxVal, long defaultVal) {
        /*! Read a long integer value from a JSON-file.
        @param key Combined filename and json-object-path.
        @param minVal minimum accepatable value.
        @param maxVal maximum accepatable value.
        @param defaultValue value returned, if key is not found or value outside specified
        bondaries.
        @return The requested value or `defaultVal` if value not found or invalid.
        */
        long val = readLong(key, defaultVal);
        return (val < minVal || val > maxVal) ? defaultVal : val;
    }

    static long atomicReadLong(String key, long minVal, long maxVal, long defaultVal) {
        /*! Read a long integer value from a JSON-file.
        @param key Combined filename and json-object-path.
        @param minVal minimum accepatable value.
        @param maxVal maximum accepatable value.
        @param defaultValue value returned, if key is not found or value outside specified
        bondaries.
        @return The requested value or `defaultVal` if value not found or invalid.
        */
        JsonFile jf;
        return jf.readLong(key, minVal, maxVal, defaultVal);
    }

    bool writeJsonVar(String key, JSONVar &value) {
        /*! Write a JSON value to a JSON-file.
        @param key Combined filename and json-object-path.
        @param value JSONVar variable that contains the value to be saved.
        @return `true` on success.
        */
        JSONVar target;
        if (!prepareWrite(key, target)) {
            return false;
        }
        target = value;
        return autocommit ? commit() : true;
    }

    static bool atomicWriteJsonVar(String key, JSONVar &value) {
        /*! Write a JSON value to a JSON-file.
        @param key Combined filename and json-object-path.
        @param value JSONVar variable that contains the value to be saved.
        @return `true` on success.
        */
        JsonFile jf;
        return jf.writeJsonVar(key, value);
    }

    bool writeJsonVar(String key, String value) {
        /*! Write a JSON value to a JSON-file.
        @param key Combined filename and json-object-path.
        @param value String containing a value in JSON format to be saved.
        @return `true` on success.
        */
        JSONVar jv = JSON.parse(value);
        if (JSON.typeof(jv) == "undefined") {
            DBG("Invalid JSON-value " + value);
            return false;
        }

        JSONVar target;
        if (!prepareWrite(key, target)) {
            return false;
        }
        target = jv;
        return autocommit ? commit() : true;
    }

    static bool atomicWriteJsonVar(String key, String value) {
        /*! Write a JSON value to a JSON-file.
        @param key Combined filename and json-object-path.
        @param value String containing a value in JSON format to be saved.
        @return `true` on success.
        */
        JsonFile jf;
        return jf.writeJsonVar(key, value);
    }

    bool writeString(String key, String value) {
        /*! Write a string value to a JSON-file.
        @param key Combined filename and json-object-path. (maxdepth is limited to 9)
        @param value Value to be written.
        @return `true` on success.
        */
        JSONVar target;
        if (!prepareWrite(key, target)) {
            return false;
        }
        target = (const char *)value.c_str();
        return autocommit ? commit() : true;
    }

    static bool atomicWriteString(String key, String value) {
        /*! Write a string value to a JSON-file.
        @param key Combined filename and json-object-path. (maxdepth is limited to 9)
        @param value Value to be written.
        @return `true` on success.
        */
        JsonFile jf;
        return jf.writeString(key, value);
    }

    bool writeStringArray(String key, ustd::array<String> &values) {
        /*! Write an array of Strings to a JSON-File.
        @param key Combined filename and json-object-path.
        @param value Array of values to be written.
        @return `true` on success.
        */
        JSONVar target;
        if (!prepareWrite(key, target)) {
            return false;
        }
        target = JSON.parse("[]");
        for (unsigned int i; i < values.length(); i++) {
            target[i] = (const char *)values[i].c_str();
        }
        return autocommit ? commit() : true;
    }

    static bool atomicWriteStringArray(String key, ustd::array<String> &values) {
        /*! Write an array of Strings to a JSON-File.
        @param key Combined filename and json-object-path.
        @param value Array of values to be written.
        @return `true` on success.
        */
        JsonFile jf;
        return jf.writeStringArray(key, values);
    }

    bool writeBool(String key, bool value) {
        /*! Write a boolean value to a JSON-file.
        @param key Combined filename and json-object-path. (maxdepth is limited to 9)
        @param value Value to be written.
        @return `true` on success.
        */
        JSONVar target;
        if (!prepareWrite(key, target)) {
            return false;
        }
        target = value;
        return autocommit ? commit() : true;
    }

    static bool atomicWriteBool(String key, bool value) {
        /*! Write a boolean value to a JSON-file.
        @param key Combined filename and json-object-path. (maxdepth is limited to 9)
        @param value Value to be written.
        @return `true` on success.
        */
        JsonFile jf;
        return jf.writeBool(key, value);
    }

    bool writeBoolArray(String key, ustd::array<bool> &values) {
        /*! Write an array of boolean values to a JSON-File.
        @param key Combined filename and json-object-path.
        @param value Array of values to be written.
        @return `true` on success.
        */
        JSONVar target;
        if (!prepareWrite(key, target)) {
            return false;
        }
        target = JSON.parse("[]");
        for (unsigned int i; i < values.length(); i++) {
            target[i] = values[i];
        }
        return autocommit ? commit() : true;
    }

    static bool atomicWriteBoolArray(String key, ustd::array<bool> &values) {
        /*! Write an array of boolean values to a JSON-File.
        @param key Combined filename and json-object-path.
        @param value Array of values to be written.
        @return `true` on success.
        */
        JsonFile jf;
        return jf.writeBoolArray(key, values);
    }

    bool writeDouble(String key, double value) {
        /*! Write a numerical value to a JSON-file.
        @param key Combined filename and json-object-path. (maxdepth is limited to 9)
        @param value Value to be written.
        @return `true` on success.
        */
        JSONVar target;
        if (!prepareWrite(key, target)) {
            return false;
        }
        target = value;
        return autocommit ? commit() : true;
    }

    static bool atomicWriteDouble(String key, double value) {
        /*! Write a numerical value to a JSON-file.
        @param key Combined filename and json-object-path. (maxdepth is limited to 9)
        @param value Value to be written.
        @return `true` on success.
        */
        JsonFile jf;
        return jf.writeDouble(key, value);
    }

    bool writeDoubleArray(String key, ustd::array<double> &values) {
        /*! Write an array of numerical values to a JSON-File.
        @param key Combined filename and json-object-path.
        @param value Array of values to be written.
        @return `true` on success.
        */
        JSONVar target;
        if (!prepareWrite(key, target)) {
            return false;
        }
        target = JSON.parse("[]");
        for (unsigned int i; i < values.length(); i++) {
            target[i] = values[i];
        }
        return autocommit ? commit() : true;
    }

    static bool atomicWriteDoubleArray(String key, ustd::array<double> &values) {
        /*! Write an array of numerical values to a JSON-File.
        @param key Combined filename and json-object-path.
        @param value Array of values to be written.
        @return `true` on success.
        */
        JsonFile jf;
        return jf.writeDoubleArray(key, values);
    }

    bool writeLong(String key, long value) {
        /*! Write a long integer value to a JSON-file.
        @param key Combined filename and json-object-path. (maxdepth is limited to 9)
        @param value Value to be written.
        @return `true` on success.
        */
        JSONVar target;
        if (!prepareWrite(key, target)) {
            return false;
        }
        target = value;
        return autocommit ? commit() : true;
    }

    static bool atomicWriteLong(String key, long value) {
        /*! Write a long integer value to a JSON-file.
        @param key Combined filename and json-object-path. (maxdepth is limited to 9)
        @param value Value to be written.
        @return `true` on success.
        */
        JsonFile jf;
        return jf.writeLong(key, value);
    }

    bool writeLongArray(String key, ustd::array<long> &values) {
        /*! Write an array of long integer values to a JSON-File.
        @param key Combined filename and json-object-path.
        @param value Array of values to be written.
        @return `true` on success.
        */
        JSONVar target;
        if (!prepareWrite(key, target)) {
            return false;
        }
        target = JSON.parse("[]");
        for (unsigned int i; i < values.length(); i++) {
            target[i] = values[i];
        }
        return autocommit ? commit() : true;
    }

    static bool atomicWriteLongArray(String key, ustd::array<long> &values) {
        /*! Write an array of long integer values to a JSON-File.
        @param key Combined filename and json-object-path.
        @param value Array of values to be written.
        @return `true` on success.
        */
        JsonFile jf;
        return jf.writeLongArray(key, values);
    }

  private:
    bool loadFile(String basename, String fn) {
        filename = basename;
        fs::File f = fsOpen(fn, "r");
        if (!f) {
            return false;
        }
        String jsonstr = "";
        if (!f.available()) {
            DBG2("Opened " + fn + ", but no data in file!");
            return false;
        }
        while (f.available()) {
            // Lets read line by line from the file
            String line = f.readStringUntil('\n');
            jsonstr += line;
        }
        f.close();
        JSONVar content = JSON.parse(jsonstr);
        if (JSON.typeof(content) == "undefined") {
            DBG("Parsing input file " + fn + "failed, invalid JSON!");
            DBG2("Content: " + jsonstr);
            return false;
        }
        DBG2("Input file " + fn + " successfully parsed");
        DBG3("Content: " + jsonstr);
        obj = content;
        loaded = true;
        return true;
    }

    bool checkLoad(String basename) {
        if (basename != filename) {
            filename = basename;
            loaded = false;
        }
        if (loaded || forcenew) {
            return true;
        }
        return loadFile(basename, path + basename + ".json");
    }

    bool prepareRead(String key, ustd::array<String> &keyparts, JSONVar &subobj,
                     bool objmode = false) {
        normalize(key);
        split(key, '/', keyparts);
        if (keyparts.length() < (objmode ? 1 : 2)) {
            DBG("Key-path too short, minimum needed is filename/topic, got: " + key);
            return false;
        }
        if (!checkLoad(keyparts[0])) {
            return false;
        }
        JSONVar iterator(obj);
        for (unsigned int i = 1; i < keyparts.length() - 1; i++) {
            JSONVar tmpCopy(iterator[keyparts[i]]);
            iterator = tmpCopy;
            if (JSON.typeof(iterator) == "undefined") {
                DBG2("From " + key + ", element " + keyparts[i] + " not found.");
                return false;
            }
        }
        String lastKey = keyparts[keyparts.length() - 1];
        if (!iterator.hasOwnProperty(lastKey)) {
            DBG2("From " + key + ", element " + lastKey + " not found.");
            return false;
        } else {
            JSONVar tmpCopy(iterator[lastKey]);
            subobj = tmpCopy;
        }
        return true;
    }

    bool prepareWrite(String key, JSONVar &target, bool objmode = false) {
        ustd::array<String> keyparts;

        normalize(key);
        split(key, '/', keyparts);
        if (keyparts.length() < (objmode ? 1 : 2)) {
            DBG("Key-path too short, minimum needed is filename/topic, got: " + key);
            return false;
        }
        if (keyparts.length() > MAX_FRICKEL_DEPTH) {
            DBG("Key-path too long, maxdepth is " + String(MAX_FRICKEL_DEPTH) + ", got: " + key);
            return false;
        }
        if (!checkLoad(keyparts[0]) && forcenew) {
            DBG("Creating new file /" + keyparts[0] + ".json");
        }

        // frickel
        switch (keyparts.length()) {
        case 1:
            // possible only in object mode
            target = obj;
            return true;
        case 2:
            target = obj[keyparts[1]];
            return true;
        case 3:
            target = obj[keyparts[1]][keyparts[2]];
            return true;
        case 4:
            target = obj[keyparts[1]][keyparts[2]][keyparts[3]];
            return true;
        case 5:
            target = obj[keyparts[1]][keyparts[2]][keyparts[3]][keyparts[4]];
            return true;
        case 6:
            target = obj[keyparts[1]][keyparts[2]][keyparts[3]][keyparts[4]][keyparts[5]];
            return true;
        case 7:
            target =
                obj[keyparts[1]][keyparts[2]][keyparts[3]][keyparts[4]][keyparts[5]][keyparts[6]];
            return true;
        case 8:
            target = obj[keyparts[1]][keyparts[2]][keyparts[3]][keyparts[4]][keyparts[5]]
                        [keyparts[6]][keyparts[7]];
            return true;
        case 9:
            target = obj[keyparts[1]][keyparts[2]][keyparts[3]][keyparts[4]][keyparts[5]]
                        [keyparts[6]][keyparts[7]][keyparts[8]];
            return true;
        default:
            DBG("SERIOUS PROGRAMMING ERROR - MAX_FRICKEL_DEV higher than implemented support "
                "in " __FILE__ " line number " +
                String(__LINE__));
            return false;
        }
    }

    static void normalize(String &src) {
        if (src.c_str()[0] == '/') {
            src = src.substring(1);
        }
    }

  public:
    static void split(String &src, char delimiter, array<String> &result) {
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
};  // namespace ustd

}  // namespace ustd