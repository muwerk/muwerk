// console.h - the muwerk simple console

#pragma once

#include "platform.h"
#include "array.h"
#include "muwerk.h"
#include "scheduler.h"

// project configration defines:
//
// __SUPPORT_FS__       - include file system functions
// __SUPPORT_EXTEND__   - allow to extend consoles with custom commands
// __SUPPORT_DATE__     - include `date` command
// __SUPPORT_SETDATE__  - allow to set date and time with `date`
// __SUPPORT_LOWMEM__   - reduce memory imprint where possible

#ifdef __ATMEGA__
#define __SUPPORT_EXTEND__
#endif

#ifdef __ESP__
#define __SUPPORT_FS__
#define __SUPPORT_EXTEND__
#define __SUPPORT_DATE__
#endif

#ifdef __UNO__
#define __SUPPORT_LOWMEM__
#endif

#ifdef __SUPPORT_FS__
#include "filesystem.h"
#include "jsonfile.h"
#endif

#ifdef __SUPPORT_DATE__
#include <time.h>
#endif

#ifdef __SUPPORT_LOWMEM__
#undef __SUPPORT_DATE__
#undef __SUPPORT_EXTEND__
#undef __SUPPORT_FS__
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
is the base for \ref ustd::SerialConsole and can also be used for implementing
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
            String arg = ustd::shift( args );
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
#ifdef __SUPPORT_EXTEND__
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
    Console(String name, Print &Output) : Output(Output), name(name) {
        /*! Creates a console
        @param name Name used for task registration and message origin
        @param Output Reference to a printer that receives the output
        */
    }

    virtual ~Console() {
#ifdef __SUPPORT_EXTEND__
        for (unsigned int i = 0; i < commands.length(); i++) {
            free(commands[i].command);
        }
        commands.erase();
#endif
    }

    bool execute(String command) {
        /*! Executes the given command
        @param command The command to execute
        @return `true` if something happened (also if the commad is invalid)
        */
        args = command;
        return execute();
    }

#ifdef __SUPPORT_EXTEND__
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

    bool execute() {
        args.trim();
        if (args.length()) {
            commandparser();
            args = "";
            return true;
        }
        return false;
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
        } else if (cmd == "uptime") {
            cmd_uptime();
        } else if (cmd == "info") {
            cmd_info();
#ifndef __SUPPORT_LOWMEM__
        } else if (cmd == "mem") {
            cmd_mem();
#endif
        } else if (cmd == "ps") {
            cmd_ps();
#ifdef __SUPPORT_DATE__
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
            Output.println("Unknown command " + cmd);
        }
    }

    void cmd_help() {
        String help = "commands: help, pub, sub, uname, uptime, ps, info";
#ifndef __SUPPORT_LOWMEM__
        help += ", mem";
#endif
#ifdef __SUPPORT_DATE__
        help += ", date";
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
        Output.print("WiFi serial debug output ");
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
        WiFi.printDiag(Output);
        Output.println();
    }
#endif
    void cmd_uptime() {
        unsigned long uptime = pSched->getUptime();
        unsigned long days = uptime / 86400;
        unsigned long hours = (uptime % 86400) / 3600;
        unsigned long minutes = (uptime % 3600) / 60;
        unsigned long seconds = uptime % 60;

        Output.print("up ");
        if (days) {
            Output.print(days);
            if (days > 1) {
                Output.print(" days, ");
            } else {
                Output.print(" day, ");
            }
        }
        outputf("%2.2lu:%2.2lu:%2.2lu\r\n", hours, minutes, seconds);
    }

    void cmd_ps() {
        Output.println();
#ifndef __SUPPORT_LOWMEM__
        Output.println("Scheduler Information:");
        Output.println("----------------------");
#endif
        Output.println("System Time: " + String(pSched->systemTime));
        Output.println("App Time: " + String(pSched->appTime));
        Output.println("Running Tasks: " + String(pSched->taskList.length()));
        if (pSched->taskList.length()) {
            Output.println();
            Output.println("  TID    Interval       Count    CPU Time   Late Time  Name");
#ifndef __SUPPORT_LOWMEM__
            Output.println("----------------------------------------------------------------");
#endif
        }
        for (unsigned int i = 0; i < pSched->taskList.length(); i++) {
            outputf("%5i  %10lu  %10lu  %10lu  %10lu  ", pSched->taskList[i].taskID,
                    pSched->taskList[i].minMicros, pSched->taskList[i].callCount,
                    pSched->taskList[i].cpuTime, pSched->taskList[i].lateTime);
            Output.println(pSched->taskList[i].szName);
        }
        Output.println();
    }

#ifndef __SUPPORT_LOWMEM__
    void cmd_mem() {
        Output.println();
#ifdef __ESP__
#ifdef __ESP32__
        Output.println("Internal Ram:");
        Output.println("-------------");
        outputf("Size: %u B\r\n", (unsigned int)ESP.getHeapSize());
        outputf("Free: %u B\r\n", (unsigned int)ESP.getFreeHeap());
        outputf("Used: %u B\r\n", (unsigned int)ESP.getHeapSize() - ESP.getFreeHeap());
        outputf("Peak: %u B\r\n", (unsigned int)ESP.getHeapSize() - ESP.getMinFreeHeap());
        outputf("MaxB: %u B\r\n", (unsigned int)ESP.getMaxAllocHeap());
        Output.println();

        Output.println("SPI Ram:");
        Output.println("--------");
        outputf("Size: %u B\r\n", (unsigned int)ESP.getPsramSize());
        outputf("Free: %u B\r\n", (unsigned int)ESP.getFreePsram());
        outputf("Used: %u B\r\n", (unsigned int)ESP.getPsramSize() - ESP.getFreePsram());
        outputf("Peak: %u B\r\n", (unsigned int)ESP.getPsramSize() - ESP.getMinFreePsram());
        outputf("MaxB: %u B\r\n", (unsigned int)ESP.getMaxAllocPsram());
        Output.println();
#else
        Output.println("Internal Ram:");
        Output.println("-------------");
        outputf("Free: %u B\r\n", (unsigned int)ESP.getFreeHeap());
        outputf("Fragmentation: %u%%\r\n", (unsigned int)ESP.getHeapFragmentation());
        outputf("Largest Free Block: %u B\r\n", (unsigned int)ESP.getMaxFreeBlockSize());
        Output.println();
#endif  // __ESP32__
#else
#ifdef __ARDUINO__
        Output.println("Memory:");
        Output.println("-------");
        Output.print("Free: ");
        Output.print(freeMemory());
        Output.println(" B");
        Output.println();
#else
        Output.println("No information available");
        Output.println();
#endif  // __ARDUINO__
#endif  // __ESP__
    }
#endif  // __SUPPORT_LOWMEM__

    void cmd_info() {
        Output.println();
#ifdef __ESP__
#ifdef __ESP32__
        Output.println("ESP32 Information:");
        Output.println("------------------");
        outputf("Chip Verion: %u\r\n", (unsigned int)ESP.getChipRevision());
        outputf("CPU Frequency: %u MHz\r\n", (unsigned int)ESP.getCpuFreqMHz());
        outputf("SDK Version: %s\r\n", ESP.getSdkVersion());
        outputf("Program Size: %u B\r\n", (unsigned int)ESP.getSketchSize());
        outputf("Program Free: %u B\r\n", (unsigned int)ESP.getFreeSketchSpace());
        outputf("Flash Chip Size: %u B\r\n", (unsigned int)ESP.getFlashChipSize());
        outputf("Flash Chip Speed: %.2f MHz\r\n", (float)ESP.getFlashChipSpeed() / 1000000.0);
        Output.println();
#else
        Output.println("ESP Information:");
        Output.println("----------------");
        outputf("Chip ID: %u\r\n", (unsigned int)ESP.getChipId());
        outputf("Chip Version: %s\r\n", ESP.getCoreVersion().c_str());
        outputf("SDK Version: %s\r\n", ESP.getSdkVersion());
        outputf("CPU Frequency: %u MHz\r\n", (unsigned int)ESP.getCpuFreqMHz());
        outputf("Program Size: %u B\r\n", (unsigned int)ESP.getSketchSize());
        outputf("Program Free: %u B\r\n", (unsigned int)ESP.getFreeSketchSpace());
        outputf("Flash Chip ID: %u\r\n", (unsigned int)ESP.getFlashChipId());
        outputf("Flash Chip Size: %u B\r\n", (unsigned int)ESP.getFlashChipSize());
        outputf("Flash Chip Real Size: %u B\r\n", (unsigned int)ESP.getFlashChipRealSize());
        outputf("Flash Chip Speed: %.2f MHz\r\n", (float)ESP.getFlashChipSpeed() / 1000000.0);
        outputf("Last Reset Reason: %s\r\n", ESP.getResetReason().c_str());
        Output.println();
#endif  // __ESP32__
#else
#ifdef __ARDUINO__
#ifdef __SUPPORT_LOWMEM__
        Output.print("mem ");
        Output.print(freeMemory());
        Output.print(", ");
        Output.print((float)(F_CPU) / 1000000.0, 2);
        Output.println(" MHz");
        Output.println();
#else
        Output.print("CPU Frequency: ");
        Output.print((float)(F_CPU) / 1000000.0, 2);
        Output.println(" MHz");
        outputf("Free Memory: %u B\r\n", (unsigned int)freeMemory());
        Output.println();
#endif  // __LOWMEM
#else
#ifdef __SUPPORT_LOWMEM__
        Output.println("No info");
#else
        Output.println("No information available");
#endif  //__SUPPORT_LOWMEM__
        Output.println();
#endif  // __ARDUINO__
#endif  // __ESP__
    }

    void cmd_uname(char opt = '\0', bool crlf = true) {
        String arg;
        switch (opt) {
        case '\0':
            arg = pullArg();
            switch (arg.length()) {
            case 0:
                break;
            case 2:
                if (arg[0] == '-') {
                    cmd_uname(arg[1]);
                    return;
                }
                // intended fallthrough
            default:
                cmd_uname('h');
                return;
            }
            // intended fallthrough
        case 's':
            Output.print("munix");
            break;
        case 'a':
            // all
            cmd_uname('s', false);
            Output.print(" ");
            cmd_uname('n', false);
            Output.print(" ");
            cmd_uname('v', false);
            break;
        case 'n':
#ifdef __ESP__
#ifdef __ESP32__
            Output.print(WiFi.getHostname());
#else
            Output.print(WiFi.hostname());
#endif
#else
            Output.print("localhost");
#endif
            break;
        case 'r':
#ifdef __ESP__
            Output.print(ESP.getSdkVersion());
#else
            Output.print("unknown");
#endif
            break;
        case 'p':
#ifdef __ESP__
#ifdef __ESP32__
            Output.print("ESP32");
#else
            Output.print("ESP");
#endif
#else
#ifdef __ARDUINO__
            Output.print("Arduino");
#else
            Output.print("Unknown");
#endif
#endif
            break;
        case 'v':
            cmd_uname('p', false);
            Output.print(" Version ");
            cmd_uname('r', false);
            Output.print(": " __DATE__ " " __TIME__);
#ifdef PLATFORMIO
            Output.print("; PlatformIO " + String(PLATFORMIO));
#ifdef ARDUINO_VARIANT
            Output.print(", " ARDUINO_VARIANT);
#endif
#endif
            break;
        case 'h':
        default:
            Output.println("usage: uname [-amnprsv]");
            return;
        }
        if (crlf) {
            Output.println();
        }
    }

#ifdef __SUPPORT_DATE__
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
#endif  // __ESP__
        }
    }
#endif  // __SUPPORT_DATE__

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

        ustd::jsonfile jf;
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

        ustd::jsonfile jf;
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

        ustd::jsonfile jf;
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
#ifndef __SUPPORT_LOWMEM__
                if (originator.length() == 0) {
                    originator = "unknown";
                }
                Output.print("\rfrom: ");
                Output.print(originator);
                Output.print(": ");
#else
                Output.print("\r");
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
        return ustd::shift(args, ' ', defValue);
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
            String arg = ustd::shift( args );
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

#ifdef __SUPPORT_LOWMEM__
#define MU_SERIAL_BUF_SIZE 0
#else
#ifdef __ARDUINO__
#define MU_SERIAL_BUF_SIZE 2
#endif
#endif

#ifndef MU_SERIAL_BUF_SIZE
#define MU_SERIAL_BUF_SIZE 16
#endif
#ifndef MU_SERIAL_CHUNK_SIZE
#define MU_SERIAL_CHUNK_SIZE 32
#endif

//! \brief muwerk Serial Console Class
class SerialConsole : public Console {
  protected:
#if MU_SERIAL_BUF_SIZE > 0
    char buffer[MU_SERIAL_BUF_SIZE];
    char *pcur;
#endif

  public:
    SerialConsole() : Console("serial", Serial) {
#if MU_SERIAL_BUF_SIZE > 0
        pcur = buffer;
        memset(buffer, 0, sizeof(buffer) / sizeof(char));
#endif
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
        pSched = _pSched;
        tID = pSched->add([this]() { this->loop(); }, name, 60000);  // 60ms
        Serial.println();
        execute(initialCommand);
        prompt();
    }

  protected:
#if MU_SERIAL_BUF_SIZE > 0
    virtual void prompt() {
        Console::prompt();
        Serial.print(buffer);
    }
#endif

    void loop() {
        int incomingByte;
        bool changed = false;
        int count = 0;

        while ((incomingByte = Serial.read()) != -1 && count < MU_SERIAL_CHUNK_SIZE) {
            ++count;  // limit reads per cycle
            switch (incomingByte) {
            case 0:   // ignore
            case 10:  // ignore
                break;
            case 8:  // backspace
#if MU_SERIAL_BUF_SIZE > 0
                if (pcur > buffer) {
                    --pcur;
                    *pcur = 0;
                } else if (args.length()) {
                    args.remove(args.length() - 1, 1);
                }
#else
                if (args.length()) {
                    args.remove(args.length() - 1, 1);
                }
#endif
                changed = true;
                break;
            case 13:  // enter
                Output.println();
#if MU_SERIAL_BUF_SIZE > 0
                flush();
#endif
                execute();
                changed = true;
                break;
            case 9:  // tab
                incomingByte = 32;
                // treat like space
            default:
#if MU_SERIAL_BUF_SIZE > 0
                *pcur = (char)incomingByte;
                ++pcur;
                if (pcur == buffer + (sizeof(buffer) / sizeof(char)) - 1) {
                    // buffer full - flush it to args
                    flush();
                }
#else
                args += (char)incomingByte;
#endif
                changed = true;
                break;
            }
        }

        if (changed) {
            prompt();
        }
    }

#if MU_SERIAL_BUF_SIZE > 0
    void flush() {
        *pcur = 0;
        args += buffer;
        // rewind
        pcur = buffer;
        memset(buffer, 0, sizeof(buffer) / sizeof(char));
    }
#endif
};  // class Console

}  // namespace ustd