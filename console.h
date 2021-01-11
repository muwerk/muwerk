// console.h - the muwerk simple console

#pragma once

#include "platform.h"
#include "array.h"
#include "scheduler.h"

#ifdef __ATMEGA__
#include <time.h>
#endif

#ifdef __ESP__
#include "filesystem.h"
#include "jsonfile.h"
#endif

namespace ustd {

/*! \brief Serial Console Extension Function

Extension function that implements a new command for a Serial Console.
 */

#if defined(__ESP__) || defined(__UNIXOID__)
typedef std::function<void(String command, String args)> T_COMMANDFN;
#else
typedef ustd::function<void(String command, String args)> T_COMMANDFN;
#endif

#ifdef USE_SERIAL_DBG
#define NDBG_ONLY(f)
#define NDBG(f)
#define NDBGP(f)
#define NDBGF(...)
#else
#define NDBG_ONLY(f) f
#define NDBG(f) Serial.println(f)
#define NDBGP(f) Serial.print(f)
#define NDBGF(...) Serial.printf(__VA_ARGS__)
#endif

/*! \brief muwerk Serial Console Class

The console class implements a simple but effective serial console shell that
allows to communicate to the device via the serial interface. The simple
interpreter has the follwing builtin commands:

* help: displays a list of supported commands
* reboot: restarts the device
* spy: add or remove message spies
* pub: allows to publish a message
* debug: enable or disable wifi diagnostics
* wifi: show wifi status
* info: show system status

If a file system is available (usually when compiling for ESP8266 or ESP32)
there are additional builtin commands available:

* ls: display directory contents
* cat: outputs the specified files
* rm: removes the specified files
* jf: read, write or delete values in json files

The commandline parser can be extended (see example below)

## Sample of console with extension

~~~{.cpp}

#include "scheduler.h"
#include "console.h"

ustd::Scheduler sched( 10, 16, 32 );
ustd::Console con;

void apploop() {}

void setup() {
    // extend console
    con.extend( "hurz", []( String cmd, String args ) {
        Serial.println( "Der Wolf... Das Lamm.... Auf der grünen Wiese....  HURZ!" );
        while ( args.length() ) {
            String arg = Console::shift( args );
            Serial.println( arg + "   HURZ!" );
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
    typedef struct {
        int id;
        char *command;
        T_COMMANDFN fn;
    } T_COMMAND;

    Scheduler *pSched;
    int tID;
    String args = "";
    char buffer[16];
    char *pcur;
    int commandHandle = 0;
    ustd::array<T_COMMAND> commands;
    ustd::array<int> spysub;
    bool spyall = false;
    bool debug = false;

  public:
    Console() {
        pcur = buffer;
        memset(pcur, 0, sizeof(buffer) / sizeof(char));
        commandHandle = 0;
    }

    ~Console() {
        for (unsigned int i = 0; i < commands.length(); i++) {
            free(commands[i].command);
        }
        commands.erase();
    }

    void begin(Scheduler *_pSched, String initialArgs = "", unsigned long baudrate = 115200) {
        /*! Starts the console
         *
         * @param _pSched Pointer to the muwerk scheduler.
         * @param initialArgs (optional, default none) Initial command to execute.
         * @param baudrate (optional, default 115200) The baud rate of the serial interface
         */
        pSched = _pSched;
#ifndef USE_SERIAL_DBG
        Serial.begin(baudrate);
#endif
        tID = pSched->add([this]() { this->loop(); }, "console", 250000);
        args = initialArgs;
        Serial.println("");
        prompt();
        if (args.length()) {
            commandparser();
        }
    }

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
    void loop() {
        int incomingByte;
        bool changed = false;
        int count = 0;

        while ((incomingByte = Serial.read()) != -1 && count < 16) {
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
        memset(pcur, 0, sizeof(buffer) / sizeof(char));
    }

    void execute() {
        args.trim();
        if (args.length()) {
            Serial.println();
            commandparser();
            args = "";
        }
    }

    void prompt() {
        Serial.print("\rmuwerk> ");
        Serial.print(args);
        Serial.print(buffer);
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
        } else if (cmd == "info") {
            cmd_info();
        } else if (cmd == "uname") {
            cmd_uname();
        } else if (cmd == "ps") {
            cmd_ps();
        } else if (cmd == "date") {
            cmd_date();
        } else if (cmd == "spy") {
            cmd_spy();
        } else if (cmd == "pub") {
            cmd_pub();
#ifdef __ESP__
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
#ifdef __ESP__
        String help =
            "\rcommands: help, reboot, spy, pub, debug, wifi, info, uname, ps, date, ls, cat, jf";
#else
        String help = "\rcommands: help, reboot, spy, pub, info, uname, ps, date";
#endif
        for (unsigned int i = 0; i < commands.length(); i++) {
            help += ", ";
            help += commands[i].command;
        }
        Serial.println(help);
    }

    void cmd_spy() {
        String arg = pullArg();
        if (arg == "-h" || arg == "") {
            Serial.println("usage: spy on | all | off | topic [topic [..]]");
            return;
        } else if (arg == "off") {
            clearSpy();
        } else if (arg == "on" || arg == "all") {
            addSpy("#");
        } else {
            do {
                if (addSpy(arg)) {
                    arg = pullArg();
                } else {
                    arg = "";
                }
            } while (arg.length());
        }

        Serial.print("Message spy is ");
        if (spysub.length()) {
            Serial.print("on.");
            if (spyall) {
                Serial.println();
            } else {
#ifdef __ESP__
                Serial.printf(" (%i subs)\n", spysub.length());
#else
                Serial.println(" (" + String(spysub.length()) + " subs)");
#endif
            }
        } else {
            Serial.println("off.");
        }
    }

    void cmd_pub() {
        String topic = pullArg();
        if (!topic.length() || topic == "-h") {
            Serial.println("usage: pub <topic> <message>");
            return;
        }
        pSched->publish(topic, args, "console");
    }

#ifdef __ESP__
    void cmd_reboot() {
        Serial.println("Restarting...");
        ESP.restart();
    }

    void cmd_debug() {
        Serial.print("\rWiFi debug output ");
        if (debug) {
            Serial.setDebugOutput(false);
            debug = false;
            Serial.println("disabled.");
        } else {
            Serial.setDebugOutput(true);
            debug = true;
            Serial.println("enabled.");
        }
    }

    void cmd_wifi() {
        Serial.println("\nWiFi Information:\n-----------------");
        WiFi.printDiag(Serial);
        Serial.println("");
    }
#endif
    void cmd_ps() {
        Serial.println("\nScheduler Information:\n----------------------");
        Serial.println("System Time: " + String(pSched->systemTime));
        Serial.println("App Time: " + String(pSched->appTime));
        Serial.println("Running Tasks: " + String(pSched->taskList.length()));
        if (pSched->taskList.length()) {
            Serial.println(
                "\n  TID    Interval       Count    CPU Time   Late Time  "
                "Name\n----------------------------------------------------------------");
        }
        for (unsigned int i = 0; i < pSched->taskList.length(); i++) {
#ifdef __ESP__
            Serial.printf("%5i  %10lu  %10lu  %10lu  %10lu  %s\n", pSched->taskList[i].taskID,
                          pSched->taskList[i].minMicros, pSched->taskList[i].callCount,
                          pSched->taskList[i].cpuTime, pSched->taskList[i].lateTime,
                          pSched->taskList[i].szName);
#else
            char buffer[64];
            snprintf(buffer, sizeof(buffer) / sizeof(char), "%5i  %10lu  %10lu  %10lu  %10lu",
                     pSched->taskList[i].taskID, pSched->taskList[i].minMicros,
                     pSched->taskList[i].callCount, pSched->taskList[i].cpuTime,
                     pSched->taskList[i].lateTime);
            buffer[(sizeof(buffer) / sizeof(char)) - 1] = 0;
            Serial.print(buffer);
            Serial.println(pSched->taskList[i].szName);
#endif
        }
        Serial.println("");
    }

    void cmd_info() {
#ifdef __ESP__
#ifdef __ESP32__
        Serial.println("\nESP32 Information:\n-------------");
        Serial.printf("Chip Verion: %u\n", (unsigned int)ESP.getChipRevision());
        Serial.printf("CPU Frequency: %u MHz\n", (unsigned int)ESP.getCpuFreqMHz());
        Serial.printf("SDK Version: %s\n", ESP.getSdkVersion());
        Serial.printf("Program Size: %u\n", (unsigned int)ESP.getSketchSize());
        Serial.printf("Program Free: %u\n", (unsigned int)ESP.getFreeSketchSpace());
        Serial.printf("Flash Chip Size: %u\n", (unsigned int)ESP.getFlashChipSize());
        Serial.printf("Flash Chip Speed: %u hz\n", (unsigned int)ESP.getFlashChipSpeed());

        cmd_wifi();

        Serial.println("Internal Ram:\n-------------");
        Serial.printf("Size: %u\n", (unsigned int)ESP.getHeapSize());
        Serial.printf("Free: %u\n", (unsigned int)ESP.getFreeHeap());
        Serial.printf("Used: %u\n", (unsigned int)ESP.getHeapSize() - ESP.getFreeHeap());
        Serial.printf("Peak: %u\n", (unsigned int)ESP.getHeapSize() - ESP.getMinFreeHeap());
        Serial.printf("MaxB: %u\n", (unsigned int)ESP.getMaxAllocHeap());

        Serial.println("\nSPI Ram:\n--------");
        Serial.printf("Size: %u\n", (unsigned int)ESP.getPsramSize());
        Serial.printf("Free: %u\n", (unsigned int)ESP.getFreePsram());
        Serial.printf("Used: %u\n", (unsigned int)ESP.getPsramSize() - ESP.getFreePsram());
        Serial.printf("Peak: %u\n", (unsigned int)ESP.getPsramSize() - ESP.getMinFreePsram());
        Serial.printf("MaxB: %u\n\n", (unsigned int)ESP.getMaxAllocPsram());
#else
        Serial.println("\nESP Information:\n-------------");
        Serial.printf("Chip ID: %u\n", (unsigned int)ESP.getChipId());
        Serial.printf("Chip Version: %s\n", ESP.getCoreVersion().c_str());
        Serial.printf("SDK Version: %s\n", ESP.getSdkVersion());
        Serial.printf("CPU Frequency: %u MHz\n", (unsigned int)ESP.getCpuFreqMHz());
        Serial.printf("Program Size: %u\n", (unsigned int)ESP.getSketchSize());
        Serial.printf("Program Free: %u\n", (unsigned int)ESP.getFreeSketchSpace());
        Serial.printf("Flash Chip ID: %u\n", (unsigned int)ESP.getFlashChipId());
        Serial.printf("Flash Chip Size: %u\n", (unsigned int)ESP.getFlashChipSize());
        Serial.printf("Flash Chip Real Size: %u\n", (unsigned int)ESP.getFlashChipRealSize());
        Serial.printf("Flash Chip Speed: %u hz\n", (unsigned int)ESP.getFlashChipSpeed());
        Serial.printf("Last Reset Reason: %s\n", ESP.getResetReason().c_str());

        cmd_wifi();
#endif
#else
        Serial.println("\nNo information available");
#endif
    }

    void cmd_uname() {
        String arg = pullArg();
        arg.toLowerCase();
        if (arg == "-h") {
            Serial.println("usage: uname [-a]");
            return;
        }
        Serial.print("muwerk");
        if (arg == "-a") {
#ifdef __ESP__
#ifdef __ESP32__
            Serial.print("  " + String(WiFi.getHostname()) + " Arduino ESP32 Version " +
                         ESP.getSdkVersion());
#else
            Serial.print("  " + WiFi.hostname() + " Arduino ESP Version " + ESP.getSdkVersion());
#endif
#else
            Serial.print("  localhost Arduino ");
#endif
            Serial.print(": " __DATE__ " " __TIME__);
        }
        Serial.println("");
    }

    void cmd_date() {
        String arg = pullArg();
        arg.toLowerCase();
        if (arg == "-h") {
            Serial.println("usage: date [[YYYY-MM-DD] hh:mm:ss]");
            return;
        }
        time_t t = time(nullptr);
        struct tm *lt = localtime(&t);
        if (!lt) {
            Serial.println("error: current time cannot be determined");
            return;
        }
        if (arg == "") {
#ifdef __ESP__
            Serial.printf("%4.4i-%2.2i-%2.2i %2.2i:%2.2i:%2.2i - epoch %lu\n", lt->tm_year + 1900,
                          lt->tm_mon + 1, lt->tm_mday, lt->tm_hour, lt->tm_min, lt->tm_sec, t);
#else
            char buffer[48];
            sprintf(buffer, "%4.4i-%2.2i-%2.2i %2.2i:%2.2i:%2.2i - epoch %lu\n", lt->tm_year + 1900,
                    lt->tm_mon + 1, lt->tm_mday, lt->tm_hour, lt->tm_min, lt->tm_sec, t);
            Serial.print(buffer);
#endif
        } else {
            if (args.length()) {
                // we have 2 arguments - the first is a date
                int i = sscanf(arg.c_str(), "%4i-%2i-%2i", &lt->tm_year, &lt->tm_mon, &lt->tm_mday);
                if (i != 3 || lt->tm_year < 1970 || lt->tm_year > 2038 || lt->tm_mon < 1 ||
                    lt->tm_mon > 11 || lt->tm_mday < 1 || lt->tm_mday > 31) {
                    Serial.println("error: " + arg + " is not a date");
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
                Serial.println("error: " + arg + " is not a time");
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
            Serial.print("Date set to: ");
            cmd_date();
        }
    }

#ifdef __ESP__
    void cmd_ls() {
        ustd::array<String> paths;
        bool extended = false;

        for (String arg = pullArg(); arg.length(); arg = pullArg()) {
            if (arg == "-h" || arg == "-H") {
                Serial.println("\rusage: ls [-l] <path> [<path> [...]]");
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

#ifdef __USE_SPIFFS_FS__
        fs::File root = SPIFFS.open("/");
        fs::File file = root.openNextFile();
        while (file) {
            if (extended) {
                Serial.printf("%crw-rw-rw-  %10u  ", (file.isDirectory() ? 'd' : '-'), file.size());
                time_t tt = file.getLastWrite();
                struct tm *lt = localtime(&tt);
                if (lt) {
                    Serial.printf("%4.4i-%2.2i-%2.2i %2.2i:%2.2i:%2.2i  ", lt->tm_year + 1900,
                                  lt->tm_mon + 1, lt->tm_mday, lt->tm_hour, lt->tm_min, lt->tm_sec);
                }
            }
            Serial.println(file.name() + 1);
            file = root.openNextFile();
        }
#else

        for (unsigned int i = 0; i < paths.length(); i++) {
            fs::Dir dir = fsOpenDir(paths[i]);
            while (dir.next()) {
                if (extended) {
                    Serial.printf("%crw-rw-rw-  %10u  ", (dir.isDirectory() ? 'd' : '-'),
                                  dir.fileSize());
                    time_t tt = dir.fileTime();
                    struct tm *lt = localtime(&tt);
                    if (lt) {
                        Serial.printf("%4.4i-%2.2i-%2.2i %2.2i:%2.2i:%2.2i  ", lt->tm_year + 1900,
                                      lt->tm_mon + 1, lt->tm_mday, lt->tm_hour, lt->tm_min,
                                      lt->tm_sec);
                    }
                }
                Serial.println(dir.fileName());
            }
        }
#endif
    }

    void cmd_rm() {
        String arg = pullArg();
        if (arg == "-h" || arg == "-H") {
            Serial.println("usage: rm <filename>");
            return;
        }
        if (!fsDelete(arg)) {
            NDBG("error: File " + arg + " can't be deleted.");
        }
    }

    void cmd_cat() {
        String arg = pullArg();
        if (arg == "-h" || arg == "-H") {
            Serial.println("usage: cat <filename>");
            return;
        }

        fs::File f = fsOpen(arg, "r");
        if (!f) {
            NDBG("error: File " + arg + " can't be opened.");
            return;
        }
        if (!f.available()) {
            f.close();
            return;
        }
        while (f.available()) {
            // Lets read line by line from the file
            Serial.println(f.readStringUntil('\n'));
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
            Serial.println("error: bad command " + arg + "specified.");
            cmd_jf_help();
        }
    }

    void cmd_jf_help() {
        Serial.println("usage: jf get <jsonpath>");
        Serial.println("usage: jf set <jsonpath> <jsonvalue>");
        Serial.println("usage: jf del <jsonpath>");
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
            NDBG("Cannot read value " + arg);
            return;
        }
        Serial.print(arg);

        String type = JSON.typeof(value);
        Serial.print(": " + type);
        if (type == "unknown") {
            Serial.println("");
            return;
        } else {
            Serial.println(", " + JSON.stringify(value));
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
            Serial.println("error: Cannot parse value " + args);
            return;
        }
        if (!jf.writeJsonVar(arg, value)) {
            NDBG("error: Failed to write value " + arg);
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
            NDBG("error: Failed to delete value " + arg);
        }
    }
#endif
    bool cmd_custom(String &cmd) {
        for (unsigned int i = 0; i < commands.length(); i++) {
            if (cmd == commands[i].command) {
                commands[i].fn(cmd, args);
                return true;
            }
        }
        return false;
    }

    bool addSpy(String topic) {
        if (spyall) {
            return false;
        } else if (topic == "#" && spysub.length()) {
            clearSpy();
        }
        int iSubId =
            pSched->subscribe(tID, topic, [this](String topic, String msg, String originator) {
                if (originator.length() == 0) {
                    originator = "unknown";
                }
#ifdef __ESP__
                Serial.printf("\rfrom %s: %s %s\n", originator.c_str(), topic.c_str(), msg.c_str());
#else
                Serial.println( "\rfrom " + originator + ": " + topic + " " + msg );
#endif
                prompt();
            });
        spysub.add(iSubId);
        if (topic == "#") {
            spyall = true;
        }
        return true;
    }

    void clearSpy() {
        if (spysub.length()) {
            for (unsigned int i = 0; i < spysub.length(); i++) {
                pSched->unsubscribe(spysub[i]);
            }
        }
        spysub.erase();
        spyall = false;
    }

    String pullArg(String defValue = "") {
        return shift(args, defValue);
    }
};  // class Console

}  // namespace ustd