// console.h - the muwerk simple console

#pragma once

#include "ustd_platform.h"
#include "ustd_array.h"
#include "muwerk.h"
#include "scheduler.h"

#ifdef USTD_FEATURE_FILESYSTEM
#include "filesystem.h"
#include "jsonfile.h"
#endif

namespace ustd {

/*! \brief Console Extension Function

Extension function that implements a new command for a Console.
 */

#if defined(__ESP__) || defined(__UNIXOID__)
typedef std::function<void(String command, String args, Print *printer, Console *pCon)> T_COMMANDFN;
#else
typedef ustd::function<void(String command, String args, Print *printer, Console *pCon)> T_COMMANDFN;
#endif

/*! \brief muwerk Command extension helper for Console Classes */
class ExtendableConsole {
  protected:
#if USTD_FEATURE_MEMORY >= USTD_FEATURE_MEM_8K
    typedef struct {
        int id;
        char *command;
        T_COMMANDFN fn;
    } T_COMMAND;
    ustd::array<T_COMMAND> commands;
    int commandHandle = 0;
#endif

    virtual ~ExtendableConsole() {
#if USTD_FEATURE_MEMORY >= USTD_FEATURE_MEM_8K
        for (unsigned int i = 0; i < commands.length(); i++) {
            free(commands[i].command);
        }
        commands.erase();
#endif
    }

  public:
#if USTD_FEATURE_MEMORY >= USTD_FEATURE_MEM_8K
    int extend(String command, T_COMMANDFN handler) {
        /*! Extend the console with a custom command
         *
         * @param command The name of the command
         * @param handler Callback of type void myCallback(String command, String args, Print *printer Console *pCon)
         * that is called, if a matching command is entered.
         * @return commandHandle on success (needed for unextend), or -1
         * on error.
         */
        T_COMMAND cmd = {};
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
};

/*! \brief muwerk Console Class

The console class implements a simple but effective console shell that
is the base for \ref ustd::SerialConsole and can also be used for implementing
shells over other transport mechanisms.
The simple interpreter has the follwing builtin commands:

* help: displays a list of supported commands
* reboot: restarts the device (only on esp)
* uname: prints system information
* uptime: tells how long the system has been running.
* mem:
* sub: add or remove message subscriptions shown on console
* pub: allows to publish a message
* wifi: show wifi status (if available)
* info: show system status

If a file system is available (usually when compiling for ESP8266 or ESP32)
there are additional builtin commands available:

* ls: display directory contents
* cat: outputs the specified files
* rm: removes the specified files
* jf: read, write or delete values in json files
* man: displays man pages for the supported commands

If the filesystem contains a valid `auth.json` file, the console supports
user authorization and the following additional buildin commands:

* passwd: update user's authentication tokens
* useradd: create a new user or update default new user information
* userdel: delete a user account and related files
* id: print real and effective user and group IDs
* who: show who is logged on
* logout: log out the currently logged in user

The commandline parser can be extended (see example below)

## Sample of a serial console with extension

~~~{.cpp}

#include "scheduler.h"
#include "console.h"

ustd::Scheduler sched( 10, 16, 32 );
ustd::SerialConsole con;

void apploop() {}

void setup() {
    // initialize the serial interface
    Serial.begin( 115200 );

    // extend console
    con.extend( "hurz", []( String cmd, String args, Print *printer, Console *pCon ) {
        printer->println( "Der Wolf... Das Lamm.... Auf der grünen Wiese....  HURZ!" );
        while ( args.length() ) {
            String arg = ustd::shift( args );
            printer->println( arg + "   HURZ!" );
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
class Console : public ExtendableConsole {

  protected:
    enum AuthState {
        NAUTH,
        PASS,
        AUTH,
        PASS1,
        PASS2
    };

    Print *printer;
    Scheduler *pSched;
    int tID;
    String name;
    String args = "";
    ustd::array<int> subsub;
    bool suball = false;
    bool debug = false;
#ifdef USTD_FEATURE_FILESYSTEM
    static bool auth;
    static int iManagerTask;
    static array<Console *> consoles;
    static ustd::jsonfile config;
    bool finished = false;
    AuthState authState = NAUTH;
    uint16_t uid = 65535;
    uint16_t gid = 65535;
    String userid;
    String param1;
    String param2;
    String home = "/";
    String cwd = "/";
#endif

  public:
    Console(String name, Print *printer)
        : printer(printer), name(name) {
        /*! Creates a console
        @param name Name used for task registration and message origin
        @param printer Pointer to a printer that receives the output
        */
    }

    virtual ~Console() {
    }

    virtual void init() {
#ifdef USTD_FEATURE_FILESYSTEM
        // check if there are declared users
        auth = config.exists("passwd/users/root");
        if (!auth) {
            uid = 0;
            gid = 0;
            userid = "root";
            authState = AUTH;
        }
        // setup console management task if not already setup (static scope)
        if (iManagerTask == -1) {
            iManagerTask = pSched->add([]() { Console::manager(); }, "conman", 1000000);
        }
        // add console to list
        Console *pCon = this;
        Console::consoles.add(pCon);
#endif
    }

    virtual void end() {
    }

    bool execute(String command) {
        /*! Executes the given command
        @param command The command to execute
        @return `true` if something happened (also if the commad is invalid)
        */
        args = command;
        return execute();
    }

  protected:
    virtual void prompt() {
#ifdef USTD_FEATURE_FILESYSTEM
        switch (authState) {
        case NAUTH:
            printer->print("\rUsername: ");
            printer->print(args);
            break;
        case PASS:
            printer->print("\rPassword: ");
            stars(args.length());
            break;
        case PASS1:
            printer->print("\rNew password: ");
            stars(args.length());
            break;
        case PASS2:
            printer->print("\rRetype new password: ");
            stars(args.length());
            break;
        case AUTH:
            outputf("\r%s@%s:%s%c %s", userid.c_str(), hostname().c_str(), getPromptPath().c_str(), (uid == 0 ? '#' : '$'), args.c_str());
            break;
        }
#else
        printer->print("\rroot@");
        printer->print(hostname());
        printer->print("~# ");
        printer->print(args);
#endif
    }

#ifdef USTD_FEATURE_FILESYSTEM
    String getPromptPath() {
        if (cwd == home) {
            return "~";
        } else if (cwd == "/") {
            return "/";
        } else {
#if defined(__UNIXOID__) || defined(__RP_PICO__)
            return cwd.substr(0, cwd.length() - 1);
#else
            return cwd.substring(0, cwd.length() - 1);
#endif
        }
    }

    uint16_t getUid() {
        return uid;
    }
    uint16_t getGid() {
        return gid;
    }
    virtual String getUser() {
        return (authState == AUTH) ? userid : "-";
    }
    virtual String getFrom() = 0;
    virtual String getRemoteAddress() = 0;
    virtual String getRemotePort() = 0;
#endif

    bool processInput() {
#ifdef USTD_FEATURE_FILESYSTEM
        switch (authState) {
        default:
        case NAUTH:
            param1 = args;
            if (!param1.isEmpty()) {
                authState = PASS;
            }
            args = "";
            return true;
        case PASS:
            if ((config.exists("passwd/users/" + param1)) && checkLogin(param1, args)) {
                login(param1.c_str());
            } else {
                // report failure data
                config.writeLong("passwd/failedCount", config.readLong("passwd/failedCount", 0) + 1);
                config.writeString("passwd/lastFailedTime", getDate());
                config.writeString("passwd/lastFailedFrom", getFrom());
                printer->println();
                printer->print("Login incorrect\r\n");
                logout();
            }
            param1 = "";
            args = "";
            return true;
        case AUTH:
            return execute();
        case PASS1:
        case PASS2:
            return cmd_passwd();
        }
#else
        return execute();
#endif
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
        } else if (cmd == "wifi") {
            cmd_wifi();
#endif
        } else if (cmd == "uname") {
            cmd_uname();
        } else if (cmd == "uptime") {
            cmd_uptime();
        } else if (cmd == "info") {
            cmd_info();
#if USTD_FEATURE_MEMORY >= USTD_FEATURE_MEM_8K
        } else if (cmd == "mem") {
            cmd_mem();
#endif
#ifdef USTD_FEATURE_CLK_READ
        } else if (cmd == "date") {
            cmd_date();
#endif
        } else if (cmd == "sub") {
            cmd_sub();
        } else if (cmd == "pub") {
            cmd_pub();
#ifdef USTD_FEATURE_FILESYSTEM
        } else if (cmd == "man") {
            cmd_man();
        } else if (cmd == "ls") {
            cmd_ls();
        } else if (cmd == "cd") {
            cmd_cd();
        } else if (cmd == "rm") {
            cmd_rm();
        } else if (cmd == "df") {
            cmd_df();
        } else if (cmd == "cat") {
            cmd_cat();
        } else if (cmd == "mkdir") {
            cmd_mkdir();
        } else if (cmd == "rmdir") {
            cmd_rmdir();
        } else if (cmd == "jf") {
            cmd_jf();
        } else if (auth && cmd == "passwd") {
            cmd_passwd();
        } else if (auth && cmd == "useradd") {
            cmd_useradd();
        } else if (auth && cmd == "userdel") {
            cmd_userdel();
        } else if (auth && cmd == "id") {
            cmd_id();
        } else if (auth && cmd == "who") {
            cmd_who();
        } else if (auth && cmd == "logout") {
            cmd_logout();
#endif
        } else if (!cmd_custom(cmd)) {
            printer->println("-mush: " + cmd + ": command not found");
        }
    }

    void cmd_help() {
        String help = "commands: help, pub, sub, uname, uptime, info";
#if USTD_FEATURE_MEMORY >= USTD_FEATURE_MEM_8K
        help += ", mem";
#endif
#ifdef USTD_FEATURE_CLK_READ
        help += ", date";
#endif
#ifdef USTD_FEATURE_FILESYSTEM
        help += ", man, ls, rm, cat, jf";
        if (auth) { help += ", passwd, useradd, userdel, id, who, logout"; }
#endif
#ifdef __ESP__
        help += ", wifi, reboot";
#endif
#if USTD_FEATURE_MEMORY >= USTD_FEATURE_MEM_8K
        for (unsigned int i = 0; i < commands.length(); i++) {
            help += ", ";
            help += commands[i].command;
        }
#endif
        printer->println(help);
    }

    void cmd_sub() {
        String arg = pullArg();
        if (help(arg, "usage: sub [all | none]\r\nusage: sub topic [topic [..]]")) {
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
                printer->println("All topics subscribed");
            } else {
                outputf("%i subscriptions\r\n", subsub.length());
            }
        } else {
            printer->println("No subscriptions");
        }
    }

    void cmd_pub() {
        String arg = pullArg();
        if (help(arg, "usage: pub <topic> <message>")) {
            return;
        }
        pSched->publish(arg, args, name);
    }

#ifdef __ESP__
    void cmd_reboot() {
        printer->println("Restarting...");
        ESP.restart();
    }

    void cmd_wifi() {
        printer->println("WiFi Information:");
        printer->println("-----------------");
        WiFi.printDiag(*printer);
        printer->println();
    }
#endif

    void cmd_uptime() {
        unsigned long uptime = pSched->getUptime();
        unsigned long days = uptime / 86400;
        unsigned long hours = (uptime % 86400) / 3600;
        unsigned long minutes = (uptime % 3600) / 60;
        unsigned long seconds = uptime % 60;

#ifdef USTD_FEATURE_CLK_READ
        printer->print(getDate(3));
        printer->print(" up ");
#else
        printer->print("up ");
#endif
        if (days) {
            printer->print(days);
            if (days > 1) {
                printer->print(" days, ");
            } else {
                printer->print(" day, ");
            }
        }
        outputf("%2.2lu:%2.2lu:%2.2lu\r\n", hours, minutes, seconds);
    }

#if USTD_FEATURE_MEMORY >= USTD_FEATURE_MEM_8K
    void cmd_mem() {
        printer->println();
#ifdef __ESP__
#if defined(__ESP32__) || defined(__ESP32_RISC__)
        printer->println("Internal Ram:");
        printer->println("-------------");
        outputf("Size: %u B\r\n", (unsigned int)ESP.getHeapSize());
        outputf("Free: %u B\r\n", (unsigned int)ESP.getFreeHeap());
        outputf("Used: %u B\r\n", (unsigned int)ESP.getHeapSize() - ESP.getFreeHeap());
        outputf("Peak: %u B\r\n", (unsigned int)ESP.getHeapSize() - ESP.getMinFreeHeap());
        outputf("MaxB: %u B\r\n", (unsigned int)ESP.getMaxAllocHeap());
        printer->println();

        printer->println("SPI Ram:");
        printer->println("--------");
        outputf("Size: %u B\r\n", (unsigned int)ESP.getPsramSize());
        outputf("Free: %u B\r\n", (unsigned int)ESP.getFreePsram());
        outputf("Used: %u B\r\n", (unsigned int)ESP.getPsramSize() - ESP.getFreePsram());
        outputf("Peak: %u B\r\n", (unsigned int)ESP.getPsramSize() - ESP.getMinFreePsram());
        outputf("MaxB: %u B\r\n", (unsigned int)ESP.getMaxAllocPsram());
        printer->println();
#else
        printer->println("Internal Ram:");
        printer->println("-------------");
        outputf("Free: %u B\r\n", (unsigned int)ESP.getFreeHeap());
        outputf("Fragmentation: %u%%\r\n", (unsigned int)ESP.getHeapFragmentation());
        outputf("Largest Free Block: %u B\r\n", (unsigned int)ESP.getMaxFreeBlockSize());
        printer->println();
#endif  // __ESP32__ || __ESP32_RISC__
#else
#ifdef __ARDUINO__
        printer->println("Memory:");
        printer->println("-------");
        printer->print("Free: ");
        printer->print(freeMemory());
        printer->println(" B");
        printer->println();
#else
        printer->println("No information available");
        printer->println();
#endif  // __ARDUINO__
#endif  // __ESP__
    }
#endif

    void cmd_info() {
        printer->println();
#ifdef __ESP__
#if defined(__ESP32__) || defined(__ESP32_RISC__)
        printer->println("ESP32 Information:");
        printer->println("------------------");
#ifdef __ESP32_RISC__
        outputf("Silicon type: RISC-V");
#else
        outputf("Silicon type: Tensilica");
#endif
        outputf("Chip Verion: %u\r\n", (unsigned int)ESP.getChipRevision());
        outputf("CPU Frequency: %u MHz\r\n", (unsigned int)ESP.getCpuFreqMHz());
        outputf("SDK Version: %s\r\n", ESP.getSdkVersion());
        outputf("Program Size: %u B\r\n", (unsigned int)ESP.getSketchSize());
        outputf("Program Free: %u B\r\n", (unsigned int)ESP.getFreeSketchSpace());
        outputf("Flash Chip Size: %u B\r\n", (unsigned int)ESP.getFlashChipSize());
        outputf("Flash Chip Speed: %.2f MHz\r\n", (float)ESP.getFlashChipSpeed() / 1000000.0);
        printer->println();
#else
        printer->println("ESP Information:");
        printer->println("----------------");
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
        printer->println();
#endif  // __ESP32__ || __ESP32_RISC__
#else
#ifdef __ARDUINO__
#if USTD_FEATURE_MEMORY < USTD_FEATURE_MEM_8K
        printer->print("mem ");
        printer->print(freeMemory());
        printer->print(", ");
        printer->print((float)(F_CPU) / 1000000.0, 2);
        printer->println(" MHz");
        printer->println();
#else
        printer->print("CPU Frequency: ");
        printer->print((float)(F_CPU) / 1000000.0, 2);
        printer->println(" MHz");
        outputf("Free Memory: %u B\r\n", (unsigned int)freeMemory());
        printer->println();
#endif  // __LOWMEM
#else
#if USTD_FEATURE_MEMORY < USTD_FEATURE_MEM_8K
        printer->println("No info");
#else
        printer->println("No information available");
#endif
        printer->println();
#endif  // __ARDUINO__
#endif  // __ESP__
    }

#ifdef USTD_FEATURE_FILESYSTEM
    virtual void cmd_logout() {
        if (auth) {
            printer->println();
            printer->println("Bye!");
            authState = NAUTH;
        }
    }

    bool cmd_passwd() {
        String arg;

        switch (authState) {
        case AUTH:
            arg = pullArg();
            if (help(arg, "usage: passwd <accountName>")) {
                return false;
            } else if (arg.isEmpty()) {
                arg = userid;
            } else if ((arg != userid) && (gid != 0)) {
                printer->println("passwd: Only root can specify a user name.");
                return false;
            }

            if (!config.exists("passwd/users/" + arg)) {
                outputf("passwd: Unknown user name '%s'.\r\n", arg.c_str());
                return false;
            }
            outputf("Changing password for user %s.\r\n", arg.c_str());
            param1 = arg;
            args = "";
            authState = PASS1;
            return true;
        case PASS1:
            if (args.isEmpty()) {
                printer->println("No password has been supplied.");
                authState = AUTH;
                resetparams();
                return false;
            }
            param2 = args;
            args = "";
            authState = PASS2;
            return true;
        case PASS2:
            if (args != param2) {
                printer->println("Sorry, passwords do not match.");
                resetparams();
                args = "";
                authState = PASS1;
                return false;
            }
            savePassword(param1, args);
            printer->println("passwd: all authentication tokens updated successfully.");
            resetparams();
            args = "";
            authState = AUTH;
            return true;
        default:
            return false;
        }
        return true;
    }

    void cmd_useradd() {
        String nuserid;
        uint16_t nuid = 1000;
        uint16_t ngid = 100;
        bool autoid = true;

        for (String arg = pullArg(); arg.length(); arg = pullArg()) {
            if (help(arg, "usage: useradd [-g GID] [-u UID] LOGIN")) {
                return;
            } else if (arg == "-g") {
                arg = pullArg();
                gid = parseId(arg, 65535);
                if (gid == 65535) {
                    printer->println("\rInvalid gid specified.");
                    return;
                }
            } else if (arg == "-u") {
                arg = pullArg();
                uid = parseId(arg, 65535);
                if (uid == 65535) {
                    printer->println("\rInvalid uid specified.");
                    return;
                }
                autoid = false;
            } else {
                if (!validLogin(arg)) {
                    outputf("useradd: invalid user name '%s'.\r\n", arg.c_str());
                    return;
                }
                nuserid = arg;
            }
        }
        if (nuserid.isEmpty()) {
            printer->println("usage: useradd [-g GID] [-u UID] LOGIN");
            return;
        } else if (gid != 0) {
            printer->println("useradd: Permission denied.");
            return;
        }
        if (autoid) {
            JSONVar users;
            config.readJsonVar("passwd/users", users);
            nuid = 0;
            for (int i = 0; i < users.keys().length(); i++) {
                long luid = config.readLong("passwd/users/" + String((const char *)users.keys()[i]) + "/uid", 0);
                if (luid > nuid) {
                    nuid = luid;
                }
            }
            if (++nuid < 1000) {
                nuid = 1000;
            }
        }
        config.writeLong("passwd/users/" + nuserid + "/uid", nuid);
        config.writeLong("passwd/users/" + nuserid + "/gid", ngid);
        config.writeString("passwd/users/" + nuserid + "/password", "");
    }

    void cmd_userdel() {
        String arg = pullArg();
        if (help(arg, "usage: userdel LOGIN", true)) {
            return;
        }
        if (gid != 0) {
            printer->println("userdel: Permission denied.");
            return;
        }
        if (arg == "root") {
            printer->println("userdel: user 'root' cannot be deleted\r\n");
        } else if (arg == userid) {
            outputf("userdel: user '%s' is currently used\r\n", arg.c_str());
        } else if (!config.exists("passwd/users/" + arg)) {
            outputf("userdel: user '%s' does not exist\r\n", arg.c_str());
        } else {
            config.remove("passwd/users/" + arg);
        }
    }

    void cmd_id() {
        if (args.isEmpty()) {
            outputf("uid=%d(%s) gid=%d(%s)\r\n", (int)uid, userid.c_str(), (int)gid, groupname(gid));
            return;
        }
        for (String arg = pullArg(); arg.length(); arg = pullArg()) {
            if (config.exists("passwd/users/" + arg)) {
                uint16_t iduid = config.readLong("passwd/users/" + arg + "/uid", 65535);
                uint16_t idgid = config.readLong("passwd/users/" + arg + "/gid", 65535);
                outputf("uid=%d(%s) gid=%d(%s)\r\n", (int)iduid, arg.c_str(), (int)idgid, groupname(idgid));
            } else {
                outputf("id: %s: no such user\r\n", arg.c_str());
            }
        }
    }

    void cmd_who() {
        outputf("ID   User      Address               Port\n");
        outputf("-------------------------------------------\n");
        for (unsigned int i = 0; i < consoles.length(); i++) {
            Console *pCon = Console::consoles[i];
            if (pCon->finished) {
                outputf("%-3d  Connection is terminated\n", i);
            } else {
                outputf("%-3d  %-8.8s  %-20.20s  %s\n", i, pCon->getUser().c_str(), pCon->getRemoteAddress().c_str(), pCon->getRemotePort());
            }
        }
    }
#endif

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
            printer->print("munix");
            break;
        case 'a':
            // all
            cmd_uname('s', false);
            printer->print(" ");
            cmd_uname('n', false);
            printer->print(" ");
            cmd_uname('v', false);
            break;
        case 'n':
            printer->print(hostname());
            break;
        case 'r':
#ifdef __ESP__
            printer->print(ESP.getSdkVersion());
#else
            printer->print("unknown");
#endif
            break;
        case 'p':
            printer->print(architecture());
            break;
        case 'v':
            cmd_uname('p', false);
            printer->print(" Version ");
            cmd_uname('r', false);
            printer->print(": " __DATE__ " " __TIME__);
#ifdef PLATFORMIO
            printer->print("; PlatformIO " + String(PLATFORMIO));
#ifdef ARDUINO_VARIANT
            printer->print(", " ARDUINO_VARIANT);
#endif
#endif
            break;
        case 'o':
            printer->print("muwerk");
            break;
        case 'h':
        default:
            printer->println("usage: uname [-amnoprsv]");
            return;
        }
        if (crlf) {
            printer->println();
        }
    }

#ifdef USTD_FEATURE_CLK_READ
    void cmd_date() {
        String arg = pullArg();

#ifdef USTD_FEATURE_CLK_SET
        if (help(arg, "usage: date [-f FORMAT]\r\nusage: date [[YYYY-MM-DD] hh:mm:ss]")) {
#else
        if (help(arg, "usage: date [-f FORMAT]")) {
#endif
            return;
        }

        uint16_t mode = 0;
        time_t t = time(nullptr);
        struct tm *lt = localtime(&t);
        if (!lt) {
            printer->println("error: current time cannot be determined");
            return;
        }

        if (arg == "-f") {
            arg = pullArg();
            mode = parseId(arg, 0);
            arg = pullArg();
        }

        if (arg == "") {
            printer->println(getDate(lt, t, mode));
        } else {
#ifdef USTD_FEATURE_CLK_SET
            if (args.length()) {
                // we have 2 arguments - the first is a date
                int i = sscanf(arg.c_str(), "%4i-%2i-%2i", &lt->tm_year, &lt->tm_mon, &lt->tm_mday);
                if (i != 3 || lt->tm_year < 1970 || lt->tm_year > 2038 || lt->tm_mon < 1 ||
                    lt->tm_mon > 11 || lt->tm_mday < 1 || lt->tm_mday > 31) {
                    printer->println("error: " + arg + " is not a date");
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
                printer->println("error: " + arg + " is not a time");
                return;
            }
            time_t newt = mktime(lt);
#ifdef __ESP__
            DBGF("Setting date to: %4.4i-%2.2i-%2.2i %2.2i:%2.2i:%2.2i - epoch %lu\n",
                 lt->tm_year + 1900, lt->tm_mon + 1, lt->tm_mday, lt->tm_hour, lt->tm_min,
                 lt->tm_sec, (unsigned long)newt);
#endif
            time(&newt);
            // invoke myself to display resulting date
            args = "";
            printer->print("Date set to: ");
            printer->print(getDate(0));
#endif  // __ESP__
        }
    }
#endif  // USTD_FEATURE_CLK_READ

#ifdef USTD_FEATURE_FILESYSTEM
    void cmd_man() {
        if (help(args, "usage: man COMMAND\r\n       Displays help for COMMAND")) {
            return;
        }
        args.replace(' ', '_');
        String file = "/man/" + args + ".man";
        if (!cat(file.c_str(), false)) {
            cat("/man/man.man", false);
            outputf("man: No manual entry for %s\r\n", args.c_str());
        }
    }

    void cmd_cd() {
        String arg = pullArg();
        if (help(arg, "usage: cd [dir]")) {
            return;
        }
        cd(arg.c_str());
    }

    void cmd_ls() {
        ustd::array<String> paths;
        bool extended = false;
        bool all = false;

        for (String arg = pullArg(); arg.length(); arg = pullArg()) {
            if (help(arg, "usage: ls [-l] <path> [<path> [...]]")) {
                return;
            } else if (arg == "-l" || arg == "-L") {
                extended = true;
            } else if (arg == "-a") {
                all = true;
            } else if (arg == "-la" || arg == "-al") {
                all = extended = true;
            } else {
                paths.add(arg);
            }
        }

        if (paths.length() == 0) {
            paths.add(cwd);
        }

        ls(paths, extended, all);
    }

    void cmd_rm() {
        ustd::array<String> paths;

        for (String arg = pullArg(); arg.length(); arg = pullArg()) {
            if (help(arg, "usage: rm [FILE]...", true)) {
                return;
            } else {
                paths.add(arg);
            }
        }

        rm(paths);
    }

    void cmd_cat() {
        ustd::array<String> paths;
        bool lineNumbers = false;
        unsigned int startLine = 1;
        unsigned int endLine = (unsigned int)-1;

        for (String arg = pullArg(); arg.length(); arg = pullArg()) {
            if (help(arg, "usage: cat [-l] [-s <start>] [-e <end>] <filename>", true)) {
                return;
            } else if (arg == "-l") {
                lineNumbers = true;
            } else if (arg == "-s") {
                arg = pullArg();
                sscanf(arg.c_str(), "%ud", &startLine);
                if (startLine < 1) {
                    startLine = 1;
                }
            } else if (arg == "-e") {
                arg = pullArg();
                sscanf(arg.c_str(), "%ud", &endLine);
            } else if (!arg.isEmpty()) {
                paths.add(arg);
            }
        }

        cat(paths, true, lineNumbers, startLine, endLine);
    }

    void cmd_mkdir() {
        ustd::array<String> paths;
        bool parents = false;
        bool verbose = false;

        for (String arg = pullArg(); arg.length(); arg = pullArg()) {
            if (help(arg, "usage: mkdir [-p] DIRECTORY...", true)) {
                return;
            } else if (arg == "-p" || arg == "--parents") {
                parents = true;
            } else if (arg == "-v" || arg == "--verbose") {
                verbose = true;
            } else if (arg == "-vp" || arg == "-pv") {
                parents = verbose = true;
            } else if (!arg.isEmpty()) {
                paths.add(arg);
            }
        }

        mkdir(paths, parents, verbose);
    }

    void cmd_rmdir() {
        ustd::array<String> paths;
        bool verbose = false;

        for (String arg = pullArg(); arg.length(); arg = pullArg()) {
            if (help(arg, "usage: rmdir DIRECTORY...", true)) {
                return;
            } else if (arg == "-v" || arg == "--verbose") {
                verbose = true;
            } else if (!arg.isEmpty()) {
                paths.add(arg);
            }
        }

        rmdir(paths, verbose);
    }

    void cmd_df() {
        bool showFsType = false;
        bool showHuman = false;
        ustd::array<String> filenames;

        for (String arg = pullArg(); arg.length(); arg = pullArg()) {
            if (help(arg, "usage: df [-T] [-h] [FILE]...", false, true)) {
                return;
            } else if (arg == "-T" || arg == "--print-type") {
                showFsType = true;
            } else if (arg == "-h" || arg == "--human-readable") {
                showHuman = true;
            } else if (!arg.isEmpty()) {
                filenames.add(arg);
            }
        }

        // print header
        printer->print("Filesystem      ");
        if (showFsType) {
            printer->print("Type       ");
        }
        if (showHuman) {
            printer->print("    Size     Used    Avail ");
        } else {
            printer->print(" 1K-blocks       Used  Available ");
        }
        printer->println("Use% Mounted on");

        // internal filesystem
        String internalType = FSNAME;
        internalType.toLowerCase();
        dfLine("internal", internalType.c_str(), fsTotalBytes(), fsUsedBytes(), "/", showFsType, showHuman);
        // future todo: add sdcard(s) if available
    }

    void cmd_jf() {
        String arg = pullArg();
        arg.toLowerCase();

        if (arg == "-h" || arg == "--help" || arg == "") {
            cmd_jf_help();
        } else if (arg == "get") {
            cmd_jf_get();
        } else if (arg == "set") {
            cmd_jf_set();
        } else if (arg == "del") {
            cmd_jf_del();
        } else {
            outputf("jf: '%s' is not a jf command. See 'jf --help'.\r\n", arg.c_str());
            cmd_jf_help();
        }
    }

    void cmd_jf_help() {
        printer->println("usage: jf get <jsonpath>");
        printer->println("usage: jf set <jsonpath> <jsonvalue>");
        printer->println("usage: jf del <jsonpath>");
    }

    void cmd_jf_get() {
        String arg = pullArg();

        if (arg == "-h" || arg == "-H" || arg == "") {
            cmd_jf_help();
            return;
        }

        ustd::jsonfile jf(true, false, cwd);
        JSONVar value;

        if (!jf.readJsonVar(arg, value)) {
#ifndef USE_SERIAL_DBG
            printer->println("error: Cannot read value " + arg);
#endif
            return;
        }
        printer->print(arg);

        String type = JSON.typeof(value);
        printer->print(": " + type);
        if (type == "unknown") {
            printer->println("");
        } else {
            printer->println(", " + JSON.stringify(value));
        }
    }

    void cmd_jf_set() {
        String arg = pullArg();

        if (arg == "-h" || arg == "-H" || arg == "" || args == "") {
            cmd_jf_help();
            return;
        }

        ustd::jsonfile jf(true, false, cwd);
        JSONVar value = JSON.parse(args);
        if (JSON.typeof(value) == "undefined") {
            printer->println("error: Cannot parse value " + args);
            return;
        }
        if (!jf.writeJsonVar(arg, value)) {
#ifndef USE_SERIAL_DBG
            printer->println("error: Failed to write value " + arg);
#endif
        }
    }

    void cmd_jf_del() {
        String arg = pullArg();

        if (arg == "-h" || arg == "-H" || arg == "") {
            cmd_jf_help();
            return;
        }

        ustd::jsonfile jf(true, false, cwd);
        if (!jf.remove(arg)) {
#ifndef USE_SERIAL_DBG
            printer->println("error: Failed to delete value " + arg);
#endif
        }
    }
#endif

    bool cmd_custom(String &cmd) {
#if USTD_FEATURE_MEMORY >= USTD_FEATURE_MEM_8K
        for (unsigned int i = 0; i < commands.length(); i++) {
            if (cmd == commands[i].command) {
                commands[i].fn(cmd, args, printer, this);
                return true;
            }
        }
#endif
        return false;
    }

    String hostname() {
#ifdef __ESP__
#if defined(__ESP32__) || defined(__ESP32_RISC__)
        return WiFi.getHostname();
#else
        return WiFi.hostname();
#endif
#else
        return "localhost";
#endif
    }

    String architecture() {
#ifdef __ESP__
#if defined(__ESP32__)
        return "ESP32 (Tensilica)";
#elif defined(__ESP32_RISC__)
        return "ESP32 (RISC-V)";
#else
    return "ESP";
#endif
#else
#ifdef __ARDUINO__
        return "Arduino";
#else
        return "Unknown";
#endif
#endif
    }

#ifdef USTD_FEATURE_FILESYSTEM
    static void manager() {
        // cleanup finished dynamic consoles
        for (unsigned int i = 0; i < consoles.length(); i++) {
            Console *pCon = Console::consoles[i];
            if (pCon->finished) {
                DBGF("\rCleaning up console %i\r\n", i);
                pCon->end();
                delete pCon;
                consoles.erase(i);
                --i;
                continue;
            }
        }
    }

    void resetparams() {
        param1 = param2 = "";
    }

    bool validLogin(String login) {
        // Usernames may contain only lower and upper case letters, digits, underscores, or dash.
        // Dashes are not allowed at the beginning of the username. Fully numeric usernames are also
        // disallowed.
        if (login.isEmpty() || login.length() > 16) {
            return false;
        }
        bool hasnums = false;
        bool haschar = false;
        bool hasdash = false;
        for (const char *pPtr = login.c_str(); *pPtr; pPtr++) {
            if (*pPtr >= '0' && *pPtr <= '9') {
                hasnums = true;
            } else if ((*pPtr >= 'A' && *pPtr <= 'Z') || (*pPtr >= 'a' && *pPtr <= 'z')) {
                haschar = true;
            } else if ((*pPtr == '-') || (*pPtr == '_')) {
                hasdash = true;
            } else {
                return false;
            }
        }
        return haschar && (login.c_str()[0] != '-') && (login.c_str()[0] != '_');
    }

    bool checkLogin(String login, String password) {
        String rawPassword = config.readString("passwd/users/" + login + "/password");
        if (rawPassword.isEmpty()) {
            // check for no password
            return password.isEmpty();
        }
        if (rawPassword.length() < 3 || rawPassword.c_str()[0] != '$' || rawPassword.c_str()[2] != '$') {
            // password stored without any encryption
            return password == rawPassword;
        }
        switch (rawPassword.c_str()[1]) {
        case '0':
            // password stored without any encryption
            return password == (rawPassword.c_str() + 3);
        // case '1':
        //     // sha1 hash
        //     // TODO: provide sha1 implementation
        //     return false;
        // case '2':
        //     // sha256 hash
        //     // TODO: provide sha256 implementation
        //     return false;
        default:
            // unknown encryption
            return false;
        }
    }

    void savePassword(String login, String password) {
        config.writeString("passwd/users/" + login + "/password", "$0$" + password);
    }

    const char *groupname(uint16_t _gid) {
        switch (_gid) {
        case 0:
            return "root";
        case 100:
            return "users";
        case 65535:
            return "nobody";
        default:
            return "other";
        }
    }

    uint16_t parseId(String arg, uint16_t defaultVal) {
        arg.trim();
        if (arg.length() == 0) {
            return defaultVal;
        }
        for (const char *pPtr = arg.c_str(); *pPtr; pPtr++) {
            if (*pPtr < '0' || *pPtr > '9') {
                return defaultVal;
            }
        }
        long result = atol(arg.c_str());
        return ((result < 0) || (result > 65535)) ? defaultVal : result;
    }

    void dfLine(const char *fs, const char *fsType, unsigned long totalBytes, unsigned long usedBytes, const char *mountPoint, bool showFsType, bool showHuman) {
        unsigned long freeBytes = totalBytes - usedBytes;
        unsigned long used = (usedBytes * 100) / totalBytes;

        outputf("%-15.15s ", fs);
        if (showFsType) {
            outputf("%-10.10s ", fsType);
        }
        if (showHuman) {
            outputf("%8s %8s %8s ", getHuman(totalBytes), getHuman(usedBytes), getHuman(freeBytes));
        } else {
            outputf("%10lu %10lu %10lu ", totalBytes / 1024, usedBytes / 1024, freeBytes / 1024);
        }
        outputf(" %2lu%% %s\r\n", used, mountPoint);
    }

    String getHuman(double value) {
        const char *szUnits = " KMGTPZ";
        char szBuffer[64];

        while (value > 1024) {
            value = value / 1024;
            szUnits++;
        }
        snprintf(szBuffer, sizeof(szBuffer) - 1, "%.1lf%c", value, *szUnits);
        szBuffer[sizeof(szBuffer) - 1] = 0;
        return szBuffer;
    }

    String normalize(String path) {
        if (path == "/") return "/";

        // remove trailing slash
#if defined(__UNIXOID__) || defined(__RP_PICO__)
        path = path.startsWith("/") ? path.substr(1) : cwd.substr(1) + path;
#else
        path = path.startsWith("/") ? path.substring(1) : cwd.substring(1) + path;
#endif
        // split in parts
        array<String> parts;
        ustd::split(path, '/', parts);
        // normaliez
        for (int i = 0; i < parts.length(); i++) {
            if (parts[i] == ".") {
                // remove reference on self
                parts.erase(i);
                --i;
            } else if (parts[i] == "..") {
                // remove self AND parent (if not root)
                parts.erase(i);
                --i;
                if (i >= 0) {
                    parts.erase(i);
                    --i;
                }
            }
        }

        // add trailing slash and rejoin parts
        return "/" + ustd::join(parts, '/');
    }

    bool isAbsolute(const char *path) {
        return *path == '/';
    }

    bool isDir(String path) {
        if (path == "/") {
            return true;
        } else if (path.startsWith("/")) {
#if defined(__UNIXOID__) || defined(__RP_PICO__)
            String test = path.endsWith("/") ? path.substr(0, path.length() - 1) : path;
#else
            String test = path.endsWith("/") ? path.substring(0, path.length() - 1) : path;
#endif
            char *pPos = strrchr(test.c_str(), '/') + 1;
#if defined(__UNIXOID__) || defined(__RP_PICO__)
            String parent = test.substr(0, pPos - test.c_str());
#else
            String parent = test.substring(0, pPos - test.c_str());
#endif
            String candidate = String(pPos);
            fs::Dir dir = fsOpenDir(parent);
            while (dir.next()) {
                if (dir.fileName() == candidate) {
                    return dir.isDirectory();
                }
            };
            return false;
        } else {
            return isDir(cwd + path);
        }
    }

    void login(String loginid) {
        String lastSuccessTime = config.readString("passwd/lastSuccessTime");
        String lastSuccessFrom = config.readString("passwd/lastSuccessFrom");
        String lastFailedTime = config.readString("passwd/lastFailedTime");
        String lastFailedFrom = config.readString("passwd/lastFailedFrom");
        long failedCount = config.readLong("passwd/failedCount", 0);

        printer->println();
        if (!lastFailedTime.isEmpty()) {
            outputf("Last failed login: %s from %s\r\n", lastFailedTime.c_str(), lastFailedFrom.c_str());
        }
        if (failedCount) {
            outputf("There were %ld failed login attempts since the last successful login.\r\n", failedCount);
        }
        if (!lastSuccessTime.isEmpty()) {
            outputf("Last login: %s from %s\r\n", lastSuccessTime.c_str(), lastSuccessFrom.c_str());
        }

        // update last successful login
        config.writeString("passwd/lastSuccessTime", getDate());
        config.writeString("passwd/lastSuccessFrom", getFrom());
        // clean failed login data
        config.remove("passwd/lastFailedTime");
        config.remove("passwd/lastFailedFrom");
        config.writeLong("passwd/failedCount", 0);

        uid = config.readLong("passwd/users/" + loginid + "/uid", 65535);
        gid = config.readLong("passwd/users/" + loginid + "/gid", 65535);
        userid = loginid;
        authState = AUTH;
    }

    void logout() {
        uid = 65535;
        gid = 65535;
        userid = "";
        authState = NAUTH;
    }

    void cd(const char *pathname) {
        if (*pathname == 0) {
            cwd = home;
            return;
        }
        String candidate = normalize(pathname);
        if (!isDir(candidate)) {
            outputf("cd: %s: No such file or directory\r\n", pathname);
            return;
        }
        cwd = candidate.endsWith("/") ? candidate : candidate + "/";
    }

    void ls(ustd::array<String> &paths, bool extended = false, bool all = false) {
        for (int i = 0; i < paths.length(); i++) {
            ls(paths[i].c_str(), extended, all);
        }
    }

    void ls(const char *path, bool extended = false, bool all = false) {
        fs::Dir dir = fsOpenDir(normalize(path));
        if (!dir.isValid()) {
            outputf("ls: cannot access '%s': No such file or directory\r\n", path);
        } else if (dir.isDirectory()) {
            while (dir.next()) {
                ls_line(dir, extended, all);
            }
        } else if (dir.isFile()) {
            ls_line(dir, extended, all);
        }
    }

    void ls_line(fs::Dir &dir, bool extended, bool all) {
        if (dir.fileName().c_str()[0] != '.' || all) {
            if (extended) {
                outputf("%crw-rw-rw-  root  root  %10u  ", (dir.isDirectory() ? 'd' : '-'), dir.fileSize());
                time_t tt = dir.fileTime();
                struct tm *lt = localtime(&tt);
                if (lt) {
                    outputf("%4.4i-%2.2i-%2.2i %2.2i:%2.2i:%2.2i  ",
                            lt->tm_year + 1900, lt->tm_mon + 1, lt->tm_mday,
                            lt->tm_hour, lt->tm_min, lt->tm_sec);
                }
            }
            printer->println(dir.fileName());
        }
    }

    void rm(ustd::array<String> &paths) {
        for (int i = 0; i < paths.length(); i++) {
            rm(paths[i].c_str());
        }
    }

    void rm(const char *path) {
        if (!fsDelete(normalize(path))) {
            outputf("rm: cannot remove '%s': No such file or directory\r\n", path);
        }
    }

    void cat(ustd::array<String> &paths, bool showError = true, bool lineNumbers = false, unsigned int startLine = 1, unsigned int endLine = (unsigned int)-1) {
        for (int i = 0; i < paths.length(); i++) {
            cat(paths[i].c_str(), showError, lineNumbers, startLine, endLine);
        }
    }

    bool cat(const char *filename, bool showError = true, bool lineNumbers = false, unsigned int startLine = 1, unsigned int endLine = (unsigned int)-1) {
        fs::File f = fsOpen(normalize(filename), "r");
        if (!f) {
            if (showError) outputf("cat: %s: No such file or directory\r\n", filename);
            return false;
        }
        if (!f.available()) {
            f.close();
            return true;
        }
        unsigned int lineNumber = 0;
        String line;
        while (f.available()) {
            ++lineNumber;
            line = f.readStringUntil('\n');
            if (lineNumber >= startLine && lineNumber <= endLine) {
                if (lineNumbers) {
                    outputf("%05d: ", lineNumber);
                }
                printer->println(line);
            }
        }
        f.close();
        printer->println();
        return true;
    }

    void mkdir(ustd::array<String> &paths, bool parents = false, bool verbose = false) {
        for (int i = 0; i < paths.length(); i++) {
            mkdir(paths[i].c_str(), parents, verbose);
        }
    }

    void mkdir(const char *path, bool parents = false, bool verbose = false) {
        if (!_mkdir(normalize(path).c_str(), parents, verbose)) {
            outputf("mkdir: cannot create directory ‘%s’: No such file or directory\r\n", path);
        }
    }

    bool _mkdir(const char *path, bool parents, bool verbose) {
        if (fsMkDir(path)) {
            if (verbose) {
                outputf("mkdir: created directory '%s'\r\n", path);
            }
            return true;
        }
        if (!parents) return false;
        // try to create parent directory
        const char *pPos = strrchr(path, '/');
        if (!pPos || pPos == path) {
            // strange - we are on root or path was not absolute - should never happen
            return false;
        }
        String candidate = path;
        // try to create parent directory
#if defined(__UNIXOID__) || defined(__RP_PICO__)
        if (_mkdir(candidate.substr(0, pPos - path).c_str(), true, verbose)) {
#else
        if (_mkdir(candidate.substring(0, pPos - path).c_str(), true, verbose)) {
#endif
            return _mkdir(path, false, verbose);
        }
        return false;
    }

    void rmdir(ustd::array<String> &paths, bool verbose = false) {
        for (int i = 0; i < paths.length(); i++) {
            rmdir(paths[i].c_str(), verbose);
        }
    }

    void rmdir(const char *path, bool verbose = false) {
        if (verbose) outputf("rmdir: removing directory, '%s'\r\n", path);
        if (!fsRmDir(normalize(path))) {
            fs::Dir dir = fsOpenDir(normalize(path));
            if (dir.isValid()) {
                if (dir.next()) {
                    outputf("rmdir: failed to remove '%s': Directory not empty\r\n", path);
                    return;
                }
            }
            outputf("rmdir: failed to remove '%s': No such file or directory\r\n", path);
        }
    }

    void motd() {
        if (!cat("/etc/motd", false)) {
            printer->println("\r\nWelcome to the machine!");
        }
    }
#else
    void motd() {
        printer->println("\r\nWelcome to the machine!");
    }
#endif

    String getDate(uint8_t format = 1) {
#ifdef USTD_FEATURE_CLK_READ
        time_t t = time(nullptr);
        return getDate(localtime(&t), t, format);
#else
        return "<unknown>";
#endif
    }

    String getDate(struct tm *lt, time_t t, uint8_t format) {
        if (lt) {
            char szBuffer[64];
            switch (format) {
            default:
            case 0:
                snprintf(szBuffer, sizeof(szBuffer) - 1,
                         "%4.4i-%2.2i-%2.2i %2.2i:%2.2i:%2.2i - epoch %lu",
                         (int)lt->tm_year + 1900, (int)lt->tm_mon + 1, (int)lt->tm_mday,
                         (int)lt->tm_hour, (int)lt->tm_min, (int)lt->tm_sec, t);
                break;
            case 1:
                snprintf(szBuffer, sizeof(szBuffer) - 1,
                         "%4.4i-%2.2i-%2.2i %2.2i:%2.2i:%2.2i",
                         (int)lt->tm_year + 1900, (int)lt->tm_mon + 1, (int)lt->tm_mday,
                         (int)lt->tm_hour, (int)lt->tm_min, (int)lt->tm_sec);
                break;
            case 2:
                snprintf(szBuffer, sizeof(szBuffer) - 1,
                         "%4.4i-%2.2i-%2.2i",
                         (int)lt->tm_year + 1900, (int)lt->tm_mon + 1, (int)lt->tm_mday);
                break;
            case 3:
                snprintf(szBuffer, sizeof(szBuffer) - 1,
                         "%2.2i:%2.2i:%2.2i",
                         (int)lt->tm_hour, (int)lt->tm_min, (int)lt->tm_sec);
                break;
            }
            szBuffer[sizeof(szBuffer) - 1] = 0;
            return szBuffer;
        }
        return "<error>";
    }

    bool addsub(String topic) {
        if (suball) {
            return false;
        } else if (topic == "#" && subsub.length()) {
            clearsub();
        }
        int iSubId =
            pSched->subscribe(tID, topic, [this](String topic, String msg, String originator) {
                printer->print("\r>> ");
#if USTD_FEATURE_MEMORY >= USTD_FEATURE_MEM_8K
                if (originator.length()) {
                    printer->print("[");
                    printer->print(originator);
                    printer->print("] ");
                }
#endif
                printer->print(topic);
                printer->print(" ");
                printer->println(msg);
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
        len = printer->write((const uint8_t *)buffer, len);
        if (buffer != temp) {
            delete[] buffer;
        }
        return len;
    }

    bool help(String &arg, const char *szHelp, bool noparams = false, bool noshort = false) {
#if defined(__UNIXOID__) || defined(__RP_PICO__)
        if ((arg == "--help") || (arg == "-h" && !noshort) || (noparams == true && arg.empty())) {
#else
        if ((arg == "--help") || (arg == "-h" && !noshort) || (noparams == true && arg.isEmpty())) {
#endif
            printer->println(szHelp);
            return true;
        }
        return false;
    }

    void stars(unsigned int count) {
        for (unsigned int i = 0; i < count; i++) {
            printer->print("*");
        }
    }
};

#ifdef USTD_FEATURE_FILESYSTEM
// static members initialisation
bool Console::auth = false;
ustd::jsonfile Console::config(true, false, "/etc/");
int Console::iManagerTask = -1;
array<Console *> Console::consoles;
#endif

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
    // initialize the serial interface
    Serial.begin( 115200 );

    // extend console
    con.extend( "hurz", []( String cmd, String args, Print *printer ) {
        printer->println( "Der Wolf... Das Lamm.... Auf der grünen Wiese....  HURZ!" );
        while ( args.length() ) {
            String arg = ustd::shift( args );
            printer->println( arg + "   HURZ!" );
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

#if USTD_FEATURE_MEMORY < USTD_FEATURE_MEM_8K
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
    SerialConsole()
        : Console("serial", &Serial) {
        /*! Instantiate a Serial Console
         */
#if MU_SERIAL_BUF_SIZE > 0
        pcur = buffer;
        memset(buffer, 0, sizeof(buffer) / sizeof(char));
#endif
    }

    void begin(Scheduler *_pSched, String initialCommand = "", unsigned long pollRate = 60) {
        /*! Starts the console on the default serial interface
         *
         * The serial interface `Serial` must be intiialized and configured (baudrate, etc.) before
         * starting the serial console.
         *
         * @param _pSched Pointer to the muwerk scheduler.
         * @param initialCommand (optional, default none) Initial command to execute.
         * @param pollRate (optional, default 60 msec) Pollrate in milliseconds of the serial
         * interface
         */
        begin(_pSched, &Serial, initialCommand, pollRate);
    }

    void begin(Scheduler *_pSched, Stream *pSerial, String initialCommand = "",
               unsigned long pollRate = 60) {
        /*! Starts the console using the specified serial interface
         *
         * The specified serial interface must be intiialized and configured (baudrate, etc.) before
         * starting the serial console.
         *
         * @param _pSched Pointer to the muwerk scheduler.
         * @param pSerial Pointer to the serial interface to use
         * @param initialCommand (optional, default none) Initial command to execute.
         * @param pollRate (optional, default 60 msec) Pollrate in milliseconds of the serial
         * interface
         */
        if (pollRate < 60) {
            pollRate = 60000L;
        } else if (pollRate > 1000) {
            pollRate = 1000000L;
        } else {
            pollRate *= 1000;
        }
        pSched = _pSched;
        printer = pSerial;
        tID = pSched->add([this]() { this->loop(); }, name, pollRate);
        printer->println();
        init();
        execute(initialCommand);
        motd();
        prompt();
    }

  protected:
#ifdef USTD_FEATURE_FILESYSTEM
    virtual String getFrom() {
        return getRemotePort();
    }

    virtual String getRemoteAddress() {
        return "-";
    }

    virtual String getRemotePort() {
        return "Serial";
    };
#endif

#if MU_SERIAL_BUF_SIZE > 0
    virtual void prompt() {
        Console::prompt();
#ifdef USTD_FEATURE_FILESYSTEM
        switch (authState) {
        case PASS:
        case PASS1:
        case PASS2:
            stars(strlen(buffer));
            break;
        default:
            printer->print(buffer);
            break;
        }
#else
        printer->print(buffer);
#endif
    }
#endif

    void loop() {
        int incomingByte;
        bool changed = false;
        int count = 0;

        while ((incomingByte = ((Stream *)printer)->read()) != -1 && count < MU_SERIAL_CHUNK_SIZE) {
            ++count;  // limit reads per cycle
            switch (incomingByte) {
            case 0:   // ignore
            case 10:  // ignore
                break;
#ifdef USTD_FEATURE_FILESYSTEM
            case 4:
                if (authState == AUTH) {
                    execute("logout");
                } else {
                    authState = NAUTH;
                }
                break;
#endif
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
                printer->println();
#if MU_SERIAL_BUF_SIZE > 0
                flush();
#endif
                processInput();
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

#ifdef USTD_FEATURE_FILESYSTEM
    virtual void cmd_logout() {
        Console::cmd_logout();
        motd();
        prompt();
    }
#endif

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