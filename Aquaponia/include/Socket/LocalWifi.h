#include "LocalWifi.h"

LocalWifi::LocalWifi()
{
  handlerEvents();
}

void LocalWifi::openConnection()
{
  WiFi.mode(LocalWifi);
  while (!WiFi.softAP(WIFI_SSID, WIFI_PASSWORD, WIFI_CHANNEL, 0, WIFI_MAX_CONNECTIONS))
  {
    Serial.print(".");
    delay(1000);
  }

  _ip = WiFi.softAPIP();
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
      case ARDUINO_EVENT_LocalWifi_STACONNECTED:
        Serial.println("Cliente conectado");
      case ARDUINO_EVENT_ETH_CONNECTED:
        Serial.println("Ethernet conectado");
      break;
      case ARDUINO_EVENT_LocalWifi_START:
        Serial.println("Ponto de Acesso Iniciado");
      break;
      case ARDUINO_EVENT_LocalWifi_STADISCONNECTED:
        Serial.println("Cliente Desconectado");
      break;
      case ARDUINO_EVENT_LocalWifi_STAIPASSIGNED:
        Serial.println("Passingned");
      break;
      case ARDUINO_EVENT_LocalWifi_STOP:
        Serial.println("Ponto de Acesso Desconectado");
      break;
    } });
}

vector<IPAddress> LocalWifi::scanNetwork()
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
  vector<IPAddress> listConnections = scanNetwork();

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