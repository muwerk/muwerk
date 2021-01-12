// console.h - the muwerk simple console

#pragma once

#include "platform.h"
#include "array.h"
#include "scheduler.h"

#ifdef __ATMEGA__
#include <time.h>
#define __SUPPORT_EXTEND__ 1
#endif

#ifdef __ESP__
#define __SUPPORT_FS__ 1
#define __SUPPORT_EXTEND__ 1
// #define __SUPPORT_SETDATE__ 1
#endif

#ifdef __UNO__
#include <time.h>
#endif

#ifdef __SUPPORT_FS__
#include "filesystem.h"
#include "jsonfile.h"
#endif

namespace ustd {

/*! \brief Console Extension Function

Extension function that implements a new command for a Console.
 */

#if defined(__ESP__) || defined(__UNIXOID__)
typedef std::function<void(String command, String args)> T_COMMANDFN;
#else
typedef ustd::function<void(String command, String args)> T_COMMANDFN;
#endif

/*! \brief muwerk Console Class

The console class implements a simple but effective console shell that
is the base for \ref SerialConsole and can also be used for implementing
shells over other transport mechanisms.
The simple interpreter has the follwing builtin commands:

* help: displays a list of supported commands
* reboot: restarts the device
* sub: add or remove message subscriptions shown on console
* pub: allows to publish a message
* debug: enable or disable wifi diagnostics (if available)
* wifi: show wifi status (if available)
* info: show system status

If a file system is available (usually when compiling for ESP8266 or ESP32)
there are additional builtin commands available:

* ls: display directory contents
* cat: outputs the specified files
* rm: removes the specified files
* jf: read, write or delete values in json files

The commandline parser can be extended (see example below)

## Sample of a serial console with extension

~~~{.cpp}

#include "scheduler.h"
#include "console.h"

ustd::Scheduler sched( 10, 16, 32 );
ustd::SerialConsole con;

void apploop() {}

void setup() {
    // extend console
    con.extend( "hurz", []( String cmd, String args ) {
        Output.println( "Der Wolf... Das Lamm.... Auf der grünen Wiese....  HURZ!" );
        while ( args.length() ) {
            String arg = Console::shift( args );
            Output.println( arg + "   HURZ!" );
        }
    } );

    con.begin( &sched );
    int tID = sched.add( apploop, "main", 50000 );
}

void loop() {
    sched.loop();
}
~~~

*/
class Console {
  protected:
#if (__SUPPORT_EXTEND__)
    typedef struct {
        int id;
        char *command;
        T_COMMANDFN fn;
    } T_COMMAND;
    ustd::array<T_COMMAND> commands;
    int commandHandle = 0;
#endif
    Print &Output;
    Scheduler *pSched;
    int tID;
    String name;
    String args = "";
    ustd::array<int> subsub;
    bool suball = false;
    bool debug = false;

  public:
    Console(String name) : Output(Serial), name(name) {
    }
    ~Console() {
#if (__SUPPORT_EXTEND__)
        for (unsigned int i = 0; i < commands.length(); i++) {
            free(commands[i].command);
        }
        commands.erase();
#endif
    }

    void execute(String command) {
        args = command;
        execute();
    }

#if (__SUPPORT_EXTEND__)
    int extend(String command, T_COMMANDFN handler) {
        /*! Extend the console with a custom command
         *
         * @param command The name of the command
         * @param handler Callback of type void myCallback(String command, String
         * args) that is called, if a matching command is entered.
         * @return commandHandle on success (needed for unextend), or -1
         * on error.
         */
        T_COMMAND cmd;
        memset(&cmd, 0, sizeof(cmd));
        cmd.id = ++commandHandle;
        cmd.fn = handler;
        cmd.command = (char *)malloc(command.length() + 1);
        strcpy(cmd.command, command.c_str());
        return commands.add(cmd) != -1 ? commandHandle : -1;
    }

    bool unextend(String command) {
        /*! Removes a custom command from the console
         * @param command The name of the command to remove.
         * @return true on success.
         */
        for (unsigned int i = 0; i < commands.length(); i++) {
            if (command == (const char *)commands[i].command) {
                return commands.erase(i);
            }
        }
        return false;
    }

    bool unextend(int commandHandle) {
        /*! Removes a custom command from the console
         * @param commandHandle The commandHandle of the command to remove.
         * @return true on success.
         */
        for (unsigned int i = 0; i < commands.length(); i++) {
            if (commands[i].id == commandHandle) {
                return commands.erase(i);
            }
        }
        return false;
    }
#endif
    static String shift(String &args, String defValue = "") {
        /*! Extract the first arg from the supplied args
         * @param args The string object from which to shift out an argument
         * @param defValue (optional, default empty string) Default value to return if no more args
         * @return The extracted arg
         */
        if (args == "") {
            return defValue;
        }
        int ind = args.indexOf(' ');
        String ret = defValue;
        if (ind == -1) {
            ret = args;
            args = "";
        } else {
            ret = args.substring(0, ind);
            args = args.substring(ind + 1);
            args.trim();
        }
        return ret;
    }

  protected:
    virtual void prompt() {
        Output.print("\rmuwerk> ");
        Output.print(args);
    }

    size_t outputf(const char *format, ...) {
        va_list arg;
        va_start(arg, format);
        char temp[64];
        char *buffer = temp;
        size_t len = vsnprintf(temp, sizeof(temp), format, arg);
        va_end(arg);
        if (len > sizeof(temp) - 1) {
            buffer = new char[len + 1];
            if (!buffer) {
                return 0;
            }
            va_start(arg, format);
            vsnprintf(buffer, len + 1, format, arg);
            va_end(arg);
        }
        len = Output.write((const uint8_t *)buffer, len);
        if (buffer != temp) {
            delete[] buffer;
        }
        return len;
    }

    void execute() {
        args.trim();
        Output.println();
        if (args.length()) {
            commandparser();
            args = "";
        }
    }

    void commandparser() {
        String cmd = pullArg();
        if (cmd == "help") {
            cmd_help();
#ifdef __ESP__
        } else if (cmd == "reboot") {
            cmd_reboot();
        } else if (cmd == "debug") {
            cmd_debug();
        } else if (cmd == "wifi") {
            cmd_wifi();
#endif
        } else if (cmd == "uname") {
            cmd_uname();
        } else if (cmd == "info") {
            cmd_info();
#ifndef __UNO__
        } else if (cmd == "mem") {
            cmd_mem();
#endif
        } else if (cmd == "ps") {
            cmd_ps();
#ifndef __UNO__
        } else if (cmd == "date") {
            cmd_date();
#endif
        } else if (cmd == "sub") {
            cmd_sub();
        } else if (cmd == "pub") {
            cmd_pub();
#ifdef __SUPPORT_FS__
        } else if (cmd == "ls") {
            cmd_ls();
        } else if (cmd == "rm") {
            cmd_rm();
        } else if (cmd == "cat") {
            cmd_cat();
        } else if (cmd == "jf") {
            cmd_jf();
#endif
        } else if (!cmd_custom(cmd)) {
            Serial.println("Unknown command " + cmd);
        }
    }

    void cmd_help() {
        String help = "commands: help, sub, pub, info, uname, ps";
#ifndef __UNO__
        help += ", date, mem";
#endif
#ifdef __SUPPORT_FS__
        help += ", ls, rm, cat, jf";
#endif
#ifdef __ESP__
        help += ", debug, wifi, reboot";
#endif
#ifdef __SUPPORT_EXTEND__
        for (unsigned int i = 0; i < commands.length(); i++) {
            help += ", ";
            help += commands[i].command;
        }
#endif
        Output.println(help);
    }

    void cmd_sub() {
        String arg = pullArg();
        if (arg == "-h" || arg == "-H") {
            Output.println("usage: sub [all | none]");
            Output.println("usage: sub topic [topic [..]]");
            return;
        } else if (arg == "none") {
            clearsub();
        } else if (arg == "all") {
            addsub("#");
        } else if (arg.length()) {
            do {
                if (addsub(arg)) {
                    arg = pullArg();
                } else {
                    arg = "";
                }
            } while (arg.length());
        }
        if (subsub.length()) {
            if (suball) {
                Output.println("All topics subscribed");
            } else {
                outputf("%i subscriptions\r\n", subsub.length());
            }
        } else {
            Output.println("No subscriptions");
        }
    }

    void cmd_pub() {
        String arg = pullArg();
        if (arg == "-h" || arg == "-H" || arg == "") {
            Output.println("usage: pub <topic> <message>");
            return;
        }
        pSched->publish(arg, args, name);
    }

#ifdef __ESP__
    void cmd_reboot() {
        Output.println("Restarting...");
        ESP.restart();
    }

    void cmd_debug() {
        Output.print("WiFi debug output ");
        if (debug) {
            Serial.setDebugOutput(false);
            debug = false;
            Output.println("disabled.");
        } else {
            Serial.setDebugOutput(true);
            debug = true;
            Output.println("enabled.");
        }
    }

    void cmd_wifi() {
        Output.println("WiFi Information:");
        Output.println("-----------------");
        WiFi.printDiag(Serial);
        Output.println();
    }
#endif
    void cmd_ps() {
        Output.println();
        Output.println("Scheduler Information:");
        Output.println("----------------------");
        Output.println("System Time: " + String(pSched->systemTime));
        Output.println("App Time: " + String(pSched->appTime));
        Output.println("Running Tasks: " + String(pSched->taskList.length()));
        if (pSched->taskList.length()) {
            Output.println();
            Output.println("  TID    Interval       Count    CPU Time   Late Time  Name");
            Output.println("----------------------------------------------------------------");
        }
        for (unsigned int i = 0; i < pSched->taskList.length(); i++) {
            outputf("%5i  %10lu  %10lu  %10lu  %10lu  ", pSched->taskList[i].taskID,
                    pSched->taskList[i].minMicros, pSched->taskList[i].callCount,
                    pSched->taskList[i].cpuTime, pSched->taskList[i].lateTime);
            Output.println(pSched->taskList[i].szName);
        }
        Serial.println();
    }

#ifndef __UNO__
    void cmd_mem() {
        Output.println();
#ifdef __ESP__
#ifdef __ESP32__
        Output.println("Internal Ram:");
        Output.println("-------------");
        outputf("Size: %u\r\n", (unsigned int)ESP.getHeapSize());
        outputf("Free: %u\r\n", (unsigned int)ESP.getFreeHeap());
        outputf("Used: %u\r\n", (unsigned int)ESP.getHeapSize() - ESP.getFreeHeap());
        outputf("Peak: %u\r\n", (unsigned int)ESP.getHeapSize() - ESP.getMinFreeHeap());
        outputf("MaxB: %u\r\n", (unsigned int)ESP.getMaxAllocHeap());
        Output.println();

        Output.println("SPI Ram:");
        Output.println("--------");
        outputf("Size: %u\r\n", (unsigned int)ESP.getPsramSize());
        outputf("Free: %u\r\n", (unsigned int)ESP.getFreePsram());
        outputf("Used: %u\r\n", (unsigned int)ESP.getPsramSize() - ESP.getFreePsram());
        outputf("Peak: %u\r\n", (unsigned int)ESP.getPsramSize() - ESP.getMinFreePsram());
        outputf("MaxB: %u\r\n", (unsigned int)ESP.getMaxAllocPsram());
        Output.println();
#else
        Output.println("Internal Ram:");
        Output.println("-------------");
        outputf("Free: %u\r\n", (unsigned int)ESP.getFreeHeap());
        outputf("Fragmentation: %u%%\r\n", (unsigned int)ESP.getHeapFragmentation());
        outputf("Largest Free Block: %u\r\n", (unsigned int)ESP.getMaxFreeBlockSize());
        Output.println();
#endif
#else
#ifdef __ARDUINO__
        Output.print("memfree: ");
        Output.print(freeMemory());
        Output.println(" bytes.");
#else
        Output.println("No information available");
        Output.println();
#endif
#endif
    }
#endif  // __UNO__

    void cmd_info() {
        Output.println();
#ifdef __ESP__
#ifdef __ESP32__
        Output.println("ESP32 Information:");
        Output.println("------------------");
        outputf("Chip Verion: %u\r\n", (unsigned int)ESP.getChipRevision());
        outputf("CPU Frequency: %u MHz\r\n", (unsigned int)ESP.getCpuFreqMHz());
        outputf("SDK Version: %s\r\n", ESP.getSdkVersion());
        outputf("Program Size: %u\r\n", (unsigned int)ESP.getSketchSize());
        outputf("Program Free: %u\r\n", (unsigned int)ESP.getFreeSketchSpace());
        outputf("Flash Chip Size: %u\r\n", (unsigned int)ESP.getFlashChipSize());
        outputf("Flash Chip Speed: %u hz\r\n", (unsigned int)ESP.getFlashChipSpeed());
        Output.println();
#else
        Serial.println("ESP Information:");
        Serial.println("----------------");
        outputf("Chip ID: %u\r\n", (unsigned int)ESP.getChipId());
        outputf("Chip Version: %s\r\n", ESP.getCoreVersion().c_str());
        outputf("SDK Version: %s\r\n", ESP.getSdkVersion());
        outputf("CPU Frequency: %u MHz\r\n", (unsigned int)ESP.getCpuFreqMHz());
        outputf("Program Size: %u\r\n", (unsigned int)ESP.getSketchSize());
        outputf("Program Free: %u\r\n", (unsigned int)ESP.getFreeSketchSpace());
        outputf("Flash Chip ID: %u\r\n", (unsigned int)ESP.getFlashChipId());
        outputf("Flash Chip Size: %u\r\n", (unsigned int)ESP.getFlashChipSize());
        outputf("Flash Chip Real Size: %u\r\n", (unsigned int)ESP.getFlashChipRealSize());
        outputf("Flash Chip Speed: %u hz\r\n", (unsigned int)ESP.getFlashChipSpeed());
        outputf("Last Reset Reason: %s\r\n", ESP.getResetReason().c_str());
        Output.println();
#endif  // __ESP32__
#else
#ifdef __ARDUINO__
        Output.print("memfree: ");
        Output.print(freeMemory());
        Output.print(" bytes, ");
        Output.print(F_CPU);
        Output.println("Hz.");
#else
        Output.println("No information available");
        Output.println();
#endif  // __ARDUINO__
#endif  // __ESP__
    }

    void cmd_uname() {
        String arg = pullArg();
        arg.toLowerCase();
        if (arg == "-h") {
            Output.println("usage: uname [-a]");
            return;
        }
        Output.print("munix");
        if (arg == "-a") {
#ifdef __ESP__
#ifdef __ESP32__
            Output.print(" " + String(WiFi.getHostname()) + " Arduino ESP32 Version " +
                         ESP.getSdkVersion());
#else
            Output.print(" " + WiFi.hostname() + " Arduino ESP Version " + ESP.getSdkVersion());
#endif
#else
            Output.print(" localhost Arduino");
#endif
            Output.print(": " __DATE__ " " __TIME__);
#ifdef PLATFORMIO
            Output.print("; PlatformIO " + String(PLATFORMIO));
#ifdef ARDUINO_VARIANT
            Output.print(", " ARDUINO_VARIANT);
#endif
#endif
        }
        Output.println();
    }

#ifndef __UNO__
    void cmd_date() {
        String arg = pullArg();
        arg.toLowerCase();
        if (arg == "-h") {
#ifdef __SUPPORT_SETDATE__
            Output.println("usage: date [[YYYY-MM-DD] hh:mm:ss]");
#else
            Output.println("usage: date");
#endif
            return;
        }
        time_t t = time(nullptr);
        struct tm *lt = localtime(&t);
        if (!lt) {
            Output.println("error: current time cannot be determined");
            return;
        }
        if (arg == "") {
            outputf("%4.4i-%2.2i-%2.2i %2.2i:%2.2i:%2.2i - epoch %lu\r\n", (int)lt->tm_year + 1900,
                    (int)lt->tm_mon + 1, (int)lt->tm_mday, (int)lt->tm_hour, (int)lt->tm_min,
                    (int)lt->tm_sec, t);
        } else {
#ifdef __SUPPORT_SETDATE__
            if (args.length()) {
                // we have 2 arguments - the first is a date
                int i = sscanf(arg.c_str(), "%4i-%2i-%2i", &lt->tm_year, &lt->tm_mon, &lt->tm_mday);
                if (i != 3 || lt->tm_year < 1970 || lt->tm_year > 2038 || lt->tm_mon < 1 ||
                    lt->tm_mon > 11 || lt->tm_mday < 1 || lt->tm_mday > 31) {
                    Output.println("error: " + arg + " is not a date");
                    return;
                }
                lt->tm_year -= 1900;
                lt->tm_mon -= 1;
                // fetch next
                arg = pullArg();
            }
            int i = sscanf(arg.c_str(), "%2i:%2i:%2i", &lt->tm_hour, &lt->tm_min, &lt->tm_sec);
            if (i != 3 || lt->tm_hour < 0 || lt->tm_hour > 23 || lt->tm_min < 0 ||
                lt->tm_min > 59 || lt->tm_sec < 0 || lt->tm_sec > 59) {
                Output.println("error: " + arg + " is not a time");
                return;
            }
            time_t newt = mktime(lt);
#ifdef __ESP__
            DBGF("Setting date to: %4.4i-%2.2i-%2.2i %2.2i:%2.2i:%2.2i - epoch %lu\n",
                 lt->tm_year + 1900, lt->tm_mon + 1, lt->tm_mday, lt->tm_hour, lt->tm_min,
                 lt->tm_sec, newt);
#endif
            time(&newt);
            // invoke myself to display resulting date
            args = "";
            Output.print("Date set to: ");
            cmd_date();
#endif
        }
    }
#endif  // __UNO__

#ifdef __SUPPORT_FS__
    void cmd_ls() {
        ustd::array<String> paths;
        bool extended = false;

        for (String arg = pullArg(); arg.length(); arg = pullArg()) {
            if (arg == "-h" || arg == "-H") {
                Output.println("\rusage: ls [-l] <path> [<path> [...]]");
                return;
            } else if (arg == "-l" || arg == "-L" || arg == "-la") {
                extended = true;
            } else {
                paths.add(arg);
            }
        }

        if (paths.length() == 0) {
            String root = "/";
            paths.add(root);
        }

        for (unsigned int i = 0; i < paths.length(); i++) {
            fs::Dir dir = fsOpenDir(paths[i]);
            while (dir.next()) {
                if (extended) {
                    outputf("%crw-rw-rw-  root  root  %10u  ", (dir.isDirectory() ? 'd' : '-'),
                            dir.fileSize());
                    time_t tt = dir.fileTime();
                    struct tm *lt = localtime(&tt);
                    if (lt) {
                        outputf("%4.4i-%2.2i-%2.2i %2.2i:%2.2i:%2.2i  ", lt->tm_year + 1900,
                                lt->tm_mon + 1, lt->tm_mday, lt->tm_hour, lt->tm_min, lt->tm_sec);
                    }
                }
                Output.println(dir.fileName());
            }
        }
    }

    void cmd_rm() {
        String arg = pullArg();
        if (arg == "-h" || arg == "-H") {
            Output.println("usage: rm <filename>");
            return;
        }
        if (!fsDelete(arg)) {
#ifndef USE_SERIAL_DBG
            Output.println("error: File " + arg + " can't be deleted.");
#endif
        }
    }

    void cmd_cat() {
        String arg = pullArg();
        if (arg == "-h" || arg == "-H") {
            Output.println("usage: cat <filename>");
            return;
        }

        fs::File f = fsOpen(arg, "r");
        if (!f) {
#ifndef USE_SERIAL_DBG
            Output.println("error: File " + arg + " can't be opened.");
#endif
            return;
        }
        if (!f.available()) {
            f.close();
            return;
        }
        while (f.available()) {
            // Lets read line by line from the file
            Output.println(f.readStringUntil('\n'));
        }
        f.close();
    }

    void cmd_jf() {
        String arg = pullArg();
        arg.toLowerCase();

        if (arg == "-h" || arg == "") {
            cmd_jf_help();
        } else if (arg == "get") {
            cmd_jf_get();
        } else if (arg == "set") {
            cmd_jf_set();
        } else if (arg == "del") {
            cmd_jf_del();
        } else {
            Output.println("error: bad command " + arg + "specified.");
            cmd_jf_help();
        }
    }

    void cmd_jf_help() {
        Output.println("usage: jf get <jsonpath>");
        Output.println("usage: jf set <jsonpath> <jsonvalue>");
        Output.println("usage: jf del <jsonpath>");
    }

    void cmd_jf_get() {
        String arg = pullArg();

        if (arg == "-h" || arg == "-H" || arg == "") {
            cmd_jf_help();
            return;
        }

        JsonFile jf;
        JSONVar value;

        if (!jf.readJsonVar(arg, value)) {
#ifndef USE_SERIAL_DBG
            Output.println("error: Cannot read value " + arg);
#endif
            return;
        }
        Output.print(arg);

        String type = JSON.typeof(value);
        Output.print(": " + type);
        if (type == "unknown") {
            Output.println("");
        } else {
            Output.println(", " + JSON.stringify(value));
        }
    }

    void cmd_jf_set() {
        String arg = pullArg();

        if (arg == "-h" || arg == "-H" || arg == "" || args == "") {
            cmd_jf_help();
            return;
        }

        JsonFile jf;
        JSONVar value = JSON.parse(args);
        if (JSON.typeof(value) == "undefined") {
            Output.println("error: Cannot parse value " + args);
            return;
        }
        if (!jf.writeJsonVar(arg, value)) {
#ifndef USE_SERIAL_DBG
            Output.println("error: Failed to write value " + arg);
#endif
        }
    }

    void cmd_jf_del() {
        String arg = pullArg();

        if (arg == "-h" || arg == "-H" || arg == "") {
            cmd_jf_help();
            return;
        }

        JsonFile jf;
        if (!jf.remove(arg)) {
#ifndef USE_SERIAL_DBG
            Output.println("error: Failed to delete value " + arg);
#endif
        }
    }
#endif
    bool cmd_custom(String &cmd) {
#ifdef __SUPPORT_EXTEND__
        for (unsigned int i = 0; i < commands.length(); i++) {
            if (cmd == commands[i].command) {
                commands[i].fn(cmd, args);
                return true;
            }
        }
#endif
        return false;
    }

    bool addsub(String topic) {
        if (suball) {
            return false;
        } else if (topic == "#" && subsub.length()) {
            clearsub();
        }
        int iSubId =
            pSched->subscribe(tID, topic, [this](String topic, String msg, String originator) {
#ifndef __UNO__
                if (originator.length() == 0) {
                    originator = "unknown";
                }
                Output.print("\rfrom: ");
                Output.print(originator);
                Output.print(": ");
#endif
                Output.print(topic);
                Output.print(" ");
                Output.println(msg);
                prompt();
            });
        subsub.add(iSubId);
        if (topic == "#") {
            suball = true;
        }
        return true;
    }

    void clearsub() {
        if (subsub.length()) {
            for (unsigned int i = 0; i < subsub.length(); i++) {
                pSched->unsubscribe(subsub[i]);
            }
        }
        subsub.erase();
        suball = false;
    }

    String pullArg(String defValue = "") {
        return shift(args, defValue);
    }
};

/*! \brief muwerk Serial Console Class

The serial console class implements a simple but effective serial console shell that
allows to communicate to the device via the serial interface. See \ref Console for a
list of supported commands.

## Sample of serial console with extension

~~~{.cpp}

#include "scheduler.h"
#include "console.h"

ustd::Scheduler sched( 10, 16, 32 );
ustd::SerialConsole con;

void apploop() {}

void setup() {
    // extend console
    con.extend( "hurz", []( String cmd, String args ) {
        Output.println( "Der Wolf... Das Lamm.... Auf der grünen Wiese....  HURZ!" );
        while ( args.length() ) {
            String arg = Console::shift( args );
            Output.println( arg + "   HURZ!" );
        }
    } );

    con.begin( &sched );
    int tID = sched.add( apploop, "main", 50000 );
}

void loop() {
    sched.loop();
}
~~~

*/

#ifdef __ARDUINO__
#define MU_SERIAL_BUF_SIZE 2
#else
#define MU_SERIAL_BUF_SIZE 16
#endif

class SerialConsole : public Console {
  protected:
    char buffer[MU_SERIAL_BUF_SIZE];
    char *pcur;

  public:
    SerialConsole() : Console("serial") {
        pcur = buffer;
        memset(buffer, 0, sizeof(buffer) / sizeof(char));
    }

    void begin(Scheduler *_pSched, String initialCommand = "", unsigned long baudrate = 115200) {
        /*! Starts the console
         *
         * @param _pSched Pointer to the muwerk scheduler.
         * @param initialCommand (optional, default none) Initial command to execute.
         * @param baudrate (optional, default 115200) The baud rate of the serial interface
         */
#ifndef USE_SERIAL_DBG
        Serial.begin(baudrate);
#endif
        Output = Serial;
        pSched = _pSched;
        tID = pSched->add([this]() { this->loop(); }, name, 60000);  // 60ms
        Serial.println();
        prompt();
        execute(initialCommand);
    }

  protected:
    virtual void prompt() {
        Serial.print("\rmuwerk> ");
        Serial.print(args);
        Serial.print(buffer);
    }

    void loop() {
        int incomingByte;
        bool changed = false;
        int count = 0;

        while ((incomingByte = Serial.read()) != -1 && count < MU_SERIAL_BUF_SIZE) {
            ++count;  // limit reads per cycle
            switch (incomingByte) {
            case 0:   // ignore
            case 10:  // ignore
                break;
            case 8:  // backspace
                if (pcur > buffer) {
                    --pcur;
                    *pcur = 0;
                } else if (args.length()) {
                    args.remove(args.length() - 1, 1);
                }
                changed = true;
                break;
            case 13:  // enter
                flush();
                execute();
                changed = true;
                break;
            case 9:  // tab
                incomingByte = 32;
                // treat like space
            default:
                *pcur = (char)incomingByte;
                ++pcur;
                if (pcur == buffer + (sizeof(buffer) / sizeof(char)) - 1) {
                    // buffer full - flush it to args
                    flush();
                }
                changed = true;
                break;
            }
        }

        if (changed) {
            prompt();
        }
    }

    void flush() {
        *pcur = 0;
        args += buffer;
        // rewind
        pcur = buffer;
        memset(buffer, 0, sizeof(buffer) / sizeof(char));
    }
};  // class Console

}  // namespace ustd