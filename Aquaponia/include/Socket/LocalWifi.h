#ifndef LOCALWIFI_H
#define LOCALWIFI_H
#include <ESPmDNS.h>
#include <vector>
#include <functional>

#include <Wifi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncJson.h>

#include <IPAddress.h>

#include <WString.h>
#include "config.h"
#include "Json/Json.h"
#include "esp_wifi.h"

using namespace std;

class LocalWifi
{
  public:
    LocalWifi();
    void openConnection();
    void closeConnection();
    void handlerEvents();
    vector<IPAddress> connectedDevices();
    void printNetworks();
    IPAddress getIp();
    void NearbyDevices();
    const char* getEncryptionType(wifi_auth_mode_t authMode);
  private:
    IPAddress _ip;
};


LocalWifi::LocalWifi()
{
  handlerEvents();
}

void LocalWifi::openConnection()
{
  WiFi.mode(WIFI_AP_STA);
  WiFi.disconnect(); 
  // NearbyDevices();

  if (!MDNS.begin("esp32")) { // ou "esp8266" dependendo do dispositivo
    Serial.println("Erro ao iniciar mDNS");
    return;
  }
  IPAddress local_IP(192, 168, 0, 184);
  IPAddress gateway(192, 168, 0, 1);
  IPAddress subnet(255, 255, 255, 0);
  
  WiFi.setHostname("AQP");
  WiFi.config(local_IP, gateway, subnet);

  while (!WiFi.softAP(WIFI_SSID, WIFI_PASSWORD, WIFI_CHANNEL, 0, WIFI_MAX_CONNECTIONS))
  {
    Serial.print(".");
    delay(1000);
  }

  LocalWifi::_ip = WiFi.softAPIP();
  handlerEvents();
  
}

void LocalWifi::closeConnection()
{
  WiFi.disconnect();
}

void LocalWifi::handlerEvents()
{
  using namespace std::placeholders;
  // WiFi.onEvent(std::bind(&LocalWifi::handlerClientConnection, this, _1, _2), ARDUINO_EVENT_LocalWifi_STACONNECTED);

  WiFi.onEvent([&](WiFiEvent_t event, WiFiEventInfo_t info)
               {
    Serial.print("Novo Evento: ");
    Serial.println(event);

    switch(event){
      case ARDUINO_EVENT_WIFI_AP_STACONNECTED:
        Serial.printf("CLIENTE CONECTADO\nMAC: %02x:%02x:%02x:%02x:%02x:%02x\n", 
                          info.wifi_ap_staconnected.mac[0], info.wifi_ap_staconnected.mac[1], info.wifi_ap_staconnected.mac[2],
                          info.wifi_ap_staconnected.mac[3], info.wifi_ap_staconnected.mac[4], info.wifi_ap_staconnected.mac[5]);
      break;
      case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED:
        Serial.printf("CLIENTE DESCONECTADO\nMAC: %02x:%02x:%02x:%02x:%02x:%02x\n", 
                          info.wifi_ap_staconnected.mac[0], info.wifi_ap_staconnected.mac[1], info.wifi_ap_staconnected.mac[2],
                          info.wifi_ap_staconnected.mac[3], info.wifi_ap_staconnected.mac[4], info.wifi_ap_staconnected.mac[5]);
      break;
      case ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED:
        Serial.printf("IP: %s\r\n", IPAddress(info.wifi_ap_staipassigned.ip.addr).toString().c_str());
        Serial.println("Passingned");
      break;
      case ARDUINO_EVENT_WIFI_STA_CONNECTED:
        Serial.println("Cliente conectado");
      break;
      case ARDUINO_EVENT_ETH_CONNECTED:
        Serial.println("Ethernet conectado");
      break;
      case ARDUINO_EVENT_WIFI_STA_START:
        Serial.println("Ponto de Acesso Iniciado");
      break;
      case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
        Serial.println("Cliente Desconectado");
      break;
      case ARDUINO_EVENT_WIFI_STA_STOP:
        Serial.println("Ponto de Acesso Desconectado");
      break;
    } });
    // int n  = WiFi.scanNetworks();
    // int c  = WiFi.scanComplete();
    // bool d  = WiFi.scanDelete();
    // bool  i = WiFi.getNetworkInfo();

}

vector<IPAddress> LocalWifi::connectedDevices()
{
  int nClients = WiFi.softAPgetStationNum();

  Serial.println();
  Serial.print(nClients);
  Serial.println(" dispositivos conectado(s).");

  vector<IPAddress> listClients;
  for (int i = 0; i < nClients; i++)
  {
    IPAddress ip;
    listClients.push_back(ip);
  }
  return listClients;
}

void LocalWifi::printNetworks()
{
  vector<IPAddress> listConnections = connectedDevices();

  for (IPAddress ip : listConnections)
  {
    Serial.print("IP: ");
    Serial.println(ip);
    Serial.println("\n");
  }
}

IPAddress LocalWifi::getIp()
{
  return _ip;
}

void LocalWifi::NearbyDevices()
{
  wifi_sta_list_t wifi_sta_list;
  tcpip_adapter_sta_list_t adapter_sta_list;
  esp_wifi_ap_get_sta_list(&wifi_sta_list);
  tcpip_adapter_get_sta_list(&wifi_sta_list, &adapter_sta_list);

  if(adapter_sta_list.num > 0)
    Serial.println(F("No networks found"));
    

  for (int8_t i = 0; i < adapter_sta_list.num; i++) {
    tcpip_adapter_sta_info_t adapter_sta_info = adapter_sta_list.sta[i];
    Serial.printf("%02X:%02X:%02X:%02X:%02X:%02X",adapter_sta_info.mac[0], adapter_sta_info.mac[1], adapter_sta_info.mac[2], adapter_sta_info.mac[3], adapter_sta_info.mac[4], adapter_sta_info.mac[5]);
    // Serial.printf("IP" + ip4addr_ntoa(&(adapter_sta_info.ip)));
  }
  /*WiFi.disconnect();
  delay(1000);
  String ssid;
  int32_t rssi;
  uint8_t encryptionType;
  uint8_t *bssid;
  int32_t channel;
  bool hidden;
  int scanResult;

  Serial.println(F("Starting WiFi scan..."));

  scanResult = WiFi.scanNetworks();

  if (scanResult == 0) {
    Serial.println(F("No networks found"));
  } else if (scanResult > 0) {
    Serial.printf(PSTR("%d networks found:\n"), scanResult);

    for (int8_t i = 0; i < scanResult; i++) {
      Serial.printf("Rede %d: SSID: %s, RSSI: %d dBm, Canal: %d, Encriptacao: %s\n",
                          i,
                          WiFi.SSID(i).c_str(),
                          WiFi.RSSI(i),
                          WiFi.channel(i),
                          getEncryptionType(WiFi.encryptionType(i)));
    }
  } else {
    Serial.printf(PSTR("WiFi scan error %d"), scanResult);
  }*/
  delay(5000);
}

const char* LocalWifi::getEncryptionType(wifi_auth_mode_t authMode) {
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
#endif