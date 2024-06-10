#ifndef LOCAL_WIFI_H
#define LOCAL_WIFI_H
#include "config.h"
#include <time.h>
#include "Clock/Clock.h"
#include <WiFi.h>

class LocalWiFi
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
      Serial.println(LOCAL_WIFI_SSID);
      Serial.println(LOCAL_WIFI_PASSWORD);
    }
    
    initTime(-3);
    Serial.println(GetIp());
    delay(100);
  }

  void initTime(int timezone){
    struct tm timeinfo;

    configTime(timezone * 3600, 0, "pool.ntp.org");

    if(!getLocalTime(&timeinfo)){
      Serial.println("Falha ao obter horário");
      return;
    }

    Clock clk;
    clk.setClock(timeinfo);
    Serial.print(clk.getDateTime().getFullDate());
  }

  void printLocalTime(){
    struct tm timeinfo;
    if(!getLocalTime(&timeinfo)){
      Serial.println("Falha ao obter horário");
      return;
    }
    Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S zone %Z %z ");
  }

  String GetIp()
  {
    return WiFi.localIP().toString();
  }
};
#endif