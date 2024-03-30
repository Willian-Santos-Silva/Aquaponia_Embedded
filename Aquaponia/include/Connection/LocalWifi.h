#ifndef LOCAL_WIFI_H
#define LOCAL_WIFI_H

#include <WiFi.h>

class LocalWiFi
{
public:
  const char *ssid = "SUELI";
  const char *password = "santos1965";

  void openConnection()
  {
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED)
    {
      delay(1000);
      Serial.println("Connecting to WiFi..");
    }
    Serial.println(GetIp());
    delay(100);
  }

  String GetIp()
  {
    return WiFi.localIP().toString();
  }
};
#endif