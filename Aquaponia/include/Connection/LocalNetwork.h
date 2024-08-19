#ifndef LOCAL_WIFI_H
#define LOCAL_WIFI_H
#include "config.h"
#include "memory.h"
#include <time.h>
#include "Clock/Clock.h"

// #include <Wifi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncJson.h>

#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_log.h"
#include "tcpip_adapter.h"

class LocalNetwork
{
private:
    Memory _memory;
public:
  const char *ssid = LOCAL_WIFI_SSID;
  const char *password = LOCAL_WIFI_PASSWORD;

  void openConnection()
  {
    // if (!_memory.read<bool>(ADDRESS_START)){
    //   ssid = _memory.read<const char*>(ADDRESS_SSID);
    //   password = _memory.read<const char*>(ADDRESS_PASSWORD);
    // }
    
    if(StartAP()){
      Serial.printf("Mode: ", (WiFi.getMode() == WIFI_MODE_AP ? "AP" : "STA") );
    }
    
    // NearbyDevices();
    
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

    // _memory.write<const char*>(ADDRESS_SSID, password);
    // _memory.write<const char*>(ADDRESS_PASSWORD, password);
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
    
  void NearbyDevices()
  {
    wifi_sta_list_t wifi_sta_list;
    tcpip_adapter_sta_list_t adapter_sta_list;
    esp_wifi_ap_get_sta_list(&wifi_sta_list);
    tcpip_adapter_get_sta_list(&wifi_sta_list, &adapter_sta_list);

    if(adapter_sta_list.num > 0)
      Serial.println(F("No networks found"));
      

    Serial.println("");
    Serial.println("DEVICES: ");
    for (int8_t i = 0; i < adapter_sta_list.num; i++) {
      tcpip_adapter_sta_info_t adapter_sta_info = adapter_sta_list.sta[i];
      // if(tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &adapter_sta_info) == ESP_OK)
      //   continue;
      Serial.printf("%02X:%02X:%02X:%02X:%02X:%02X",adapter_sta_info.mac[0], adapter_sta_info.mac[1], adapter_sta_info.mac[2], adapter_sta_info.mac[3], adapter_sta_info.mac[4], adapter_sta_info.mac[5]);
      // Serial.printf("IP: %s", ip4addr_ntoa(&adapter_sta_info.ip.addr));

      // ESP_LOGI(TAG, "got ip:%s\n", ip4addr_ntoa(&event->ip_info.ip));
    }
  }
};
#endif