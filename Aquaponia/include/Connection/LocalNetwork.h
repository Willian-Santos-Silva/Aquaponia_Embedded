#ifndef LOCAL_WIFI_H
#define LOCAL_WIFI_H
#include "config.h"
#include <time.h>
#include "Clock/Clock.h"
#include <WiFi.h>

class LocalNetwork
{
public:
  const char *ssid = LOCAL_WIFI_SSID;
  const char *password = LOCAL_WIFI_PASSWORD;

  void openConnection()
  {
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
  void setNetwork(String ssid, String password){
    ssid = ssid;
    password = password;
  }

  String GetIp()
  {
    return WiFi.localIP().toString();
  }
};
#endif