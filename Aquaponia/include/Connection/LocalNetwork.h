#ifndef LOCAL_WIFI_H
#define LOCAL_WIFI_H
#include "config.h"
#include "memory.h"
#include <time.h>
#include "Clock/Clock.h"
#include <WiFi.h>

class LocalNetwork
{
private:
    Memory _memory;
public:
  const char *ssid = LOCAL_WIFI_SSID;
  const char *password = LOCAL_WIFI_PASSWORD;

  void openConnection()
  {
    if (!_memory.read<bool>(ADDRESS_START)){
      ssid = _memory.read<const char*>(ADDRESS_SSID);
      password = _memory.read<const char*>(ADDRESS_PASSWORD);
    }
    
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED)
    {
      delay(1000);
      Serial.println("Connecting to WiFi..");
      Serial.println(ssid);
      Serial.println(password);
    }
    
    Serial.println(GetIp());
    
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

  String GetIp()
  {
    return WiFi.localIP().toString();
  }
};
#endif