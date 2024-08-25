#ifndef LOCAL_WIFI_H
#define LOCAL_WIFI_H
#include "Base/config.h"
#include "Base/memory.h"
#include <time.h>
#include "Clock/Clock.h"

#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <vector>

struct Hotposts{
  String ssid;
  int32_t rssi;
  int32_t channel;
  String encryptionType;
};

class LocalNetwork
{
private:
  Memory _memory;
  const char *_ssid = LOCAL_WIFI_SSID;
  const char *_password = LOCAL_WIFI_PASSWORD;
public:
  bool StartSTA(std::string ssid, std::string password)
  {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), password.c_str());

    Serial.println("Connecting to WiFi..");
    Serial.printf("SSID: %s - Senha: %s\r\n\n", ssid.c_str(), password.c_str());

    if (WiFi.waitForConnectResult() != WL_CONNECTED) {
      Serial.printf("WiFi Failed!\n");
      return false;
    }

    WiFi.setAutoReconnect(true);
    Serial.printf("Conectado");    
    Serial.printf("IP: %s\r\n", WiFi.localIP().toString());
    

    _memory.write<const char*>(ADDRESS_SSID, password.c_str());
    _memory.write<const char*>(ADDRESS_PASSWORD, password.c_str());

    int timezone = -3;
    Clock clk;
    clk.setTime(timezone);

    return true;
  }

  bool StartAP()
  {
    WiFi.mode(WIFI_AP);

    IPAddress primaryDNS(8, 8, 8, 8);
    IPAddress secondaryDNS(8, 8, 4, 4);

    while (!WiFi.softAP(WIFI_SSID, WIFI_PASSWORD))
    {
      Serial.println(".");
      delay(1000);
    }

    if (!WiFi.config(WiFi.softAPIP(), WiFi.gatewayIP(), WiFi.subnetMask(), primaryDNS, secondaryDNS)) {
      Serial.println("STA Failed to configure");
    }
    
    Serial.printf("IP: %s\r\n\r\n", WiFi.softAPIP().toString());

    return true;
  }

  std::vector<Hotposts> sanningHotpost()
  {
    WiFi.disconnect();
    
    std::vector<Hotposts> listHotposts;

    Serial.println(F("Starting WiFi scan..."));
    int hotposts = WiFi.scanNetworks();


    if (hotposts <= 0) {
      Serial.println(F("No networks found"));
      return listHotposts;
    } 
    
    Serial.printf(PSTR("%d networks found\n"), hotposts);

    for (int i = 0; i < hotposts; i++) {
      Hotposts h;
      h.ssid = WiFi.SSID(i);
      h.rssi = WiFi.RSSI(i);
      h.channel = WiFi.channel(i);
      h.encryptionType = getEncryptionType(WiFi.encryptionType(i));

      listHotposts.push_back(h);
      
      // Serial.printf("Rede %d: SSID: %s, RSSI: %d dBm, Canal: %d, Encriptacao: %s\r\n",
      //                     i,
      //                     WiFi.SSID(i).c_str(),
      //                     WiFi.RSSI(i),
      //                     WiFi.channel(i),
      //                     WiFi.dnsIP(i),
      //                     getEncryptionType(WiFi.encryptionType(i)));
    }
    return listHotposts;
  }

  String getEncryptionType(wifi_auth_mode_t authMode) {
    switch (authMode) {
        case WIFI_AUTH_OPEN:
            return "Open";
        case WIFI_AUTH_WEP:
            return "WEP";
        case WIFI_AUTH_WPA_PSK:
            return "WPA_PSK";
        case WIFI_AUTH_WPA2_PSK:
            return "WPA2_PSK";
        case WIFI_AUTH_WPA_WPA2_PSK:
            return "WPA_WPA2_PSK";
        case WIFI_AUTH_WPA2_ENTERPRISE:
            return "WPA2_ENTERPRISE";
        default:
            return "Unknown";
    }
  }

  String GetIp()
  {
    return WiFi.localIP().toString() == IPAddress().toString()? WiFi.localIP().toString() : "";
  }
};

#endif