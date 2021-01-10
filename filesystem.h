// fs_helper.h - the muwerk fs helper routines

#pragma once

#include "platform.h"

namespace ustd {

bool fsInited = false;

#ifdef __USE_SPIFFS_FS__
#define FSNAME "SPIFFS"
#else
#define FSNAME "LittleFS"
#endif

bool fsBegin() {
    if (!fsInited) {
#ifdef __USE_SPIFFS_FS__
        fsInited = SPIFFS.begin(false);
#else
        fsInited = LittleFS.begin();
#endif
        if (!fsInited) {
            DBG("Failed to initialize " FSNAME " filesystem");
        }
    }
    return fsInited;
}

void fsEnd() {
    if (fsInited) {
        LittleFS.end();
        fsInited = false;
    }
}

bool fsdelete(String filename) {
#ifdef __USE_SPIFFS_FS__
    bool ret = fsBegin() && SPIFFS.remove(filename);
#else
    bool ret = fsBegin() && LittleFS.remove(filename);
#endif
    if (!ret) {
        DBG("Failed to delete file " + filename);
    }
    return ret;
}

fs::File fsOpen(String filename, String mode) {
    if (!fsBegin()) {
        return (fs::File)0;
    }
#ifdef __USE_SPIFFS_FS__
    fs::File f = SPIFFS.open(filename.c_str(), mode.c_str());
#else
    fs::File f = LittleFS.open(filename.c_str(), mode.c_str());
#endif
    if (!f) {
        DBG("Failed to open " + filename + " on " FSNAME " filesystem");
    }
    return f;
}

fs::Dir fsOpenDir(String path) {
    if (!fsBegin()) {
        return (fs::Dir)0;
    }
#ifdef __USE_SPIFFS_FS__
    return SPIFFS.openDir(path.c_str());
#else
    return LittleFS.openDir(path.c_str());
#endif
}

}  // namespace ustd