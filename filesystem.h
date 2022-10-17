// fs_helper.h - the muwerk fs helper routines

#pragma once

#include "ustd_platform.h"

#if defined(__ESP32__) || defined(__ESP32_RISC__)
namespace fs {
class Dir {
  public:
    Dir(int _) {
    }
    Dir(File fd)
        : fd(fd) {
    }

    File openFile(const char *mode) {
        return (File)0;
    }

    String fileName() {
        return ff.name();
    }
    size_t fileSize() {
        return ff.size();
    }
    time_t fileTime() {
        return ff.getLastWrite();
    }
    time_t fileCreationTime() {
        return (size_t)0;
    }
    bool isFile() {
        return ff.isDirectory() ? false : true;
    }
    bool isDirectory() {
        return ff.isDirectory() ? true : false;
    }

    bool next() {
        ff = fd.openNextFile();
        return ff != (File)0;
    }
    bool rewind() {
        fd.rewindDirectory();
        return next();
    }

  protected:
    File fd;
    File ff;
};
}  // namespace fs
#endif

namespace ustd {

bool fsInited = false;

#ifdef __USE_SPIFFS_FS__
#define FSNAME "SPIFFS"
#else
#define FSNAME "LittleFS"
#endif

bool fsBegin() {
    /*! This function initializes the choosen file system. The
    Usually it is not needed to call this function explicitely since
    all file system abstration functions check that the file system
    has been initialized and in case invoke it.
    @return true on success
    */
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
#ifdef __USE_SPIFFS_FS__
        SPIFFS.end();
#else
        LittleFS.end();
#endif
        fsInited = false;
    }
}

bool fsDelete(String filename) {
    /*! This function deletes the spcified file from the filesystem.
    @param filename Absolute filename of the file to be deleted
    @return true on sucess
    */
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
    /*! This function opens the specified file and returns a file object.
    @param filename Absolute filename of the file to be opened
    @param mode File access mode (r, r+, w, w+, a, a+)
    @return File object of the speicified file. To check whether the file
            was opened successfully, use the boolean operator.
    */
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
    /*! This function opens a directory given its absolute path.
    @param path Absolute path to open
    @return Dir object of the specified directory.
    */
    if (!fsBegin()) {
        return (fs::Dir)0;
    }
#if defined(__ESP32__) || defined(__ESP32_RISC__)
#ifdef __USE_SPIFFS_FS__
    fs::File fd = SPIFFS.open(path);
#else
    fs::File fd = LittleFS.open(path);
#endif
    fs::Dir dir(fd);
    return dir;
#else
#ifdef __USE_SPIFFS_FS__
    return SPIFFS.openDir(path.c_str());
#else
    return LittleFS.openDir(path.c_str());
#endif
#endif  // __ESP32__ || __ESP32_RISC__
}

}  // namespace ustd