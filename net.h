// net.h - the muwerk network

#pragma once

#if defined(__ESP__)
#include "../ustd/platform.h"
#include "../ustd/array.h"
#include "../ustd/map.h"

#include <ESP8266WiFi.h>
#include <FS.h>
#include <ArduinoJson.h>

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
    unsigned long tick1sec;
    unsigned long tick10sec;
    ustd::sensorprocessor rssival;
    ustd::map<String, String> netServices;  // XXX: ustdification
    String macAddress;

    Net() {
        oldState = NOTDEFINED;
        state = NOTCONFIGURED;
        mode = AP;
        tick1sec = millis();
        tick10sec = millis();
        if (readNetConfig()) {
            connectAP();
        }
        subscribe("net/network/get", subNetGet);
        subscribe("net/network/set", subNetSet);
        subscribe("net/networks/get", subNetsGet);
        subscribe("net/servicees/+/get", subNetServicesGet);
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
        publish("net/network", json);
        log(T_LOGLEVEL::INFO, json);
        if (state == CONNECTED)
            publishServices();
    }

    bool readNetConfig() {
        SPIFFS.begin();
        File f = SPIFFS.open("/net.json", "r");
        if (!f) {
            DBG("SPIFFS needs to contain a net.json file!");
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
                DBG("Invalid JSON received, check SPIFFS file net.json!");
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

            // for ( auto s : netServices ) {
            //    DBG( "***" + s.first + "->" + s.second );
            //}
            return true;
        }
    }

    void connectAP() {
        DBG("Connecting to: " + SSID);
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
            publish("net/networks", "{}");  // "{\"state\":\"error\"}");
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
        publish("net/networks", netlist);
    }

    void publishServices() {
        for (for int i=0; i<netServices.length(); i++) {
            publish("net/services/" + netServices.keys[i],
                    "{\"server\":\"" + netServices.values[i] + "\"}");
        }
    }
    virtual void loop() override {
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
            if (util::timebudget::delta(conTime, millis()) > conTimeout) {
                DBG("Timeout connecting to: " + SSID);
                state = NOTCONFIGURED;
            }
            break;
        case CONNECTED:
            if (timeDiff(tick1sec, millis()) > 1000) {
                tick1sec = millis();
                if (WiFi.status() == WL_CONNECTED) {
                    long rssi = WiFi.RSSI();
                    if (rssival.filter(&rssi)) {
                        publish("net/rssi", "{\"rssi\":" + String(rssi) + "}");
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

    void subsNetGet(String topic, String msg) {
        publishNetwork();
    }
    void subNetsGet(String topic, String msg) {
        publishNetworks();
    }
    void subNetSet(String topic, String msg) {
        // XXX: not yet implemented.
    }
    void subNetServicesGet(String topic, String msg) {
        for (for int i=0; i<netServices.length(); i++) {
            if (topic == "net/services/" + netServices.keys[i] + "/get") {
                publish("net/services/" + netServices.keys[i],
                        "{\"server\":\"" + netServices.values[i] + "\"}");
            }
        }
    }
};
}  // namespace ustd

#endif  // defined(__ESP__)
