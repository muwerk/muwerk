// net.h - the muwerk network

#pragma once

#if defined(__ESP__)

#ifdef __ESP32__
#include <WiFi.h>
#endif

#include "../ustd/array.h"
#include "../ustd/map.h"
#include "../ustd/platform.h"

#include "../muwerk/scheduler.h"

#include <ArduinoJson.h>

#ifndef __ESP32__
#include <FS.h>
#endif

namespace ustd {
class Net {
  public:
    enum Netstate { NOTDEFINED, NOTCONFIGURED, CONNECTINGAP, CONNECTED };
    enum Netmode { AP, STATION };

    Netstate state;
    Netstate oldState;
    Netmode mode;
    long conTime;
    long conTimeout = 15000;
    String SSID;
    String password;
    String localHostname;
    String ipAddress;
    Scheduler *pSched;
    unsigned long tick1sec;
    unsigned long tick10sec;
    ustd::sensorprocessor rssival;
    ustd::map<String, String> netServices;
    String macAddress;

    Net() {
        oldState = NOTDEFINED;
        state = NOTCONFIGURED;
    }

    void begin(Scheduler *_pSched, String _ssid = "", String _password = "",
               Netmode _mode = AP) {
        pSched = _pSched;
        SSID = _ssid;
        password = _password;
        mode = _mode;
        tick1sec = millis();
        tick10sec = millis();
        if (SSID == "") {
            if (readNetConfig()) {
                connectAP();
            }
        } else {
            localHostname = "";
            connectAP();
        }

        // give a c++11 lambda as callback scheduler task registration of
        // this.loop():
        std::function<void()> ft = [=]() { this->loop(); };
        pSched->add(ft);

        // give a c++11 lambda as callback for incoming mqttmessages:
        std::function<void(String, String, String)> fng =
            [=](String topic, String msg, String originator) {
                this->subsNetGet(topic, msg, originator);
            };
        pSched->subscribe("net/network/get", fng);
        std::function<void(String, String, String)> fns =
            [=](String topic, String msg, String originator) {
                this->subsNetSet(topic, msg, originator);
            };
        pSched->subscribe("net/network/set", fns);
        std::function<void(String, String, String)> fnsg =
            [=](String topic, String msg, String originator) {
                this->subsNetsGet(topic, msg, originator);
            };
        pSched->subscribe("net/networks/get", fnsg);
        std::function<void(String, String, String)> fsg =
            [=](String topic, String msg, String originator) {
                this->subsNetServicesGet(topic, msg, originator);
            };
        pSched->subscribe("net/services/+/get", fsg);
    }

    void publishNetwork() {
        String json;
        if (mode == AP) {
            json = "{\"mode\":\"ap\",";
        } else if (mode == STATION) {
            json = "{\"mode\":\"station\",";
        } else {
            json = "{\"mode\":\"undefined\",";
        }
        json += "\"mac\":\"" + macAddress + "\",";
        switch (state) {
        case NOTCONFIGURED:
            json += "\"state\":\"notconfigured\"}";
            break;
        case CONNECTINGAP:
            json += "\"state\":\"connectingap\",\"SSID\":\"" + SSID + "\"}";
            break;
        case CONNECTED:
            json += "\"state\":\"connected\",\"SSID\":\"" + SSID +
                    "\",\"hostname\":\"" + localHostname + "\",\"ip\":\"" +
                    ipAddress + "\"}";
            break;
        default:
            json += "\"state\":\"undefined\"}";
            break;
        }
        pSched->publish("net/network", json);
        if (state == CONNECTED)
            publishServices();
    }

    bool readNetConfig() {
        SPIFFS.begin();
        File f = SPIFFS.open("/net.json", "r");
        if (!f) {
            return false;
        } else {
            String jsonstr = "";
            while (f.available()) {
                // Lets read line by line from the file
                String lin = f.readStringUntil('\n');
                jsonstr = jsonstr + lin;
            }
            DynamicJsonBuffer jsonBuffer(200);
            JsonObject &root = jsonBuffer.parseObject(jsonstr);
            if (!root.success()) {
                return false;
            } else {
                SSID = root["SSID"].as<char *>();
                password = root["password"].as<char *>();
                localHostname = root["hostname"].as<char *>();
                JsonArray &servs = root["services"];
                for (int i = 0; i < servs.size(); i++) {
                    JsonObject &srv = servs[i];
                    for (auto obj : srv) {
                        netServices[obj.key] = obj.value.as<char *>();
                    }
                }
            }
            return true;
        }
    }

    void connectAP() {
        WiFi.mode(WIFI_STA);
        WiFi.begin(SSID.c_str(), password.c_str());
        macAddress = WiFi.macAddress();

        if (localHostname != "")
            WiFi.hostname(localHostname.c_str());
        state = CONNECTINGAP;
        conTime = millis();
    }

    String strEncryptionType(int thisType) {
        // read the encryption type and print out the name:
        switch (thisType) {
        case ENC_TYPE_WEP:
            return "WEP";
            break;
        case ENC_TYPE_TKIP:
            return "WPA";
            break;
        case ENC_TYPE_CCMP:
            return "WPA2";
            break;
        case ENC_TYPE_NONE:
            return "None";
            break;
        case ENC_TYPE_AUTO:
            return "Auto";
            break;
        default:
            return "unknown";
            break;
        }
    }
    void publishNetworks() {
        int numSsid = WiFi.scanNetworks();
        if (numSsid == -1) {
            pSched->publish("net/networks", "{}");  // "{\"state\":\"error\"}");
            return;
        }
        String netlist = "{";
        for (int thisNet = 0; thisNet < numSsid; thisNet++) {
            if (thisNet > 0)
                netlist += ",";
            netlist += "\"" + WiFi.SSID(thisNet) +
                       "\":{\"rssi\":" + String(WiFi.RSSI(thisNet)) +
                       ",\"enc\":\"" +
                       strEncryptionType(WiFi.encryptionType(thisNet)) + "\"}";
        }
        netlist += "}";
        pSched->publish("net/networks", netlist);
    }

    void publishServices() {
        for (int i = 0; i < netServices.length(); i++) {
            pSched->publish("net/services/" + netServices.keys[i],
                            "{\"server\":\"" + netServices.values[i] + "\"}");
        }
    }

    void subsNetGet(String topic, String msg, String originator) {
        publishNetwork();
    }
    void subsNetsGet(String topic, String msg, String originator) {
        publishNetworks();
    }
    void subsNetSet(String topic, String msg, String originator) {
        // XXX: not yet implemented.
    }

    void subsNetServicesGet(String topic, String msg, String originator) {
        for (int i = 0; i < netServices.length(); i++) {
            if (topic == "net/services/" + netServices.keys[i] + "/get") {
                pSched->publish("net/services/" + netServices.keys[i],
                                "{\"server\":\"" + netServices.values[i] +
                                    "\"}");
            }
        }
    }

    void loop() {
        switch (state) {
        case NOTCONFIGURED:
            if (timeDiff(tick10sec, millis()) > 10000) {
                tick10sec = millis();
                publishNetworks();
            }
            break;
        case CONNECTINGAP:
            if (WiFi.status() == WL_CONNECTED) {
                state = CONNECTED;
                IPAddress ip = WiFi.localIP();
                ipAddress = String(ip[0]) + '.' + String(ip[1]) + '.' +
                            String(ip[2]) + '.' + String(ip[3]);
            }
            if (ustd::timeDiff(conTime, millis()) > conTimeout) {
                state = NOTCONFIGURED;
            }
            break;
        case CONNECTED:
            if (timeDiff(tick1sec, millis()) > 1000) {
                tick1sec = millis();
                if (WiFi.status() == WL_CONNECTED) {
                    long rssi = WiFi.RSSI();
                    if (rssival.filter(&rssi)) {
                        pSched->publish("net/rssi",
                                        "{\"rssi\":" + String(rssi) + "}");
                    }
                } else {
                    state = NOTCONFIGURED;
                }
            }
            break;
        default:
            break;
        }
        if (state != oldState) {
            oldState = state;
            publishNetwork();
        }
    }
};
}  // namespace ustd

#endif  // defined(__ESP__)
