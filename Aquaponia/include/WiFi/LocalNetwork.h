#ifndef LOCAL_WIFI_H
#define LOCAL_WIFI_H
#include "Base/config.h"
#include "Base/memory.h"
#include <time.h>
#include "Clock/Clock.h"

#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncJson.h>

class LocalNetwork
{
private:
    Memory _memory;
public:
  const char *ssid = LOCAL_WIFI_SSID;
  const char *password = LOCAL_WIFI_PASSWORD;

  void openConnection()
  {
    if(StartAP()){
      Serial.printf("Mode: ", (WiFi.getMode() == WIFI_MODE_AP ? "AP" : "STA") );
    }
    
    int timezone = -3;
    Clock clk;
    clk.setTime(timezone);

    delay(100);
  }

  void printLocalTime(){
    struct tm timeinfo;
    if(!getLocalTime(&timeinfo)){
      Serial.println("Falha ao obter horário");
      return;
    }
    Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S zone %Z %z ");
  }
  void setNetwork(const char* _ssid, const char* _password){
    ssid = _ssid;
    password = _password;

    _memory.write<const char*>(ADDRESS_SSID, password);
    _memory.write<const char*>(ADDRESS_PASSWORD, password);
  }

  bool StartSTA()
  {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);


    Serial.println("Connecting to WiFi..");
    Serial.printf("SSID: %s - Senha: %s", ssid, password);

    if (WiFi.waitForConnectResult() != WL_CONNECTED) {
      Serial.printf("WiFi Failed!\n");
      return false;
    }

    Serial.printf("Conectado");    
    Serial.printf("IP: %s\r\n", WiFi.localIP().toString());
    return true;
  }

  bool StartAP()
  {
    WiFi.mode(WIFI_AP);

    IPAddress primaryDNS(8, 8, 8, 8);
    IPAddress secondaryDNS(8, 8, 4, 4);

    while (!WiFi.softAP(WIFI_SSID, WIFI_PASSWORD))
    {
      ESP_LOGW(TAG, ".");
      delay(1000);
    }

    if (!WiFi.config(WiFi.softAPIP(), WiFi.gatewayIP(), WiFi.subnetMask(), primaryDNS, secondaryDNS)) {
      Serial.println("STA Failed to configure");
    }
    
    Serial.printf("IP: %s\r\n\r\n", WiFi.softAPIP().toString());

    return true;
  }
    
    

  String GetIp()
  {
    return WiFi.localIP().toString();
  }
};
#endif