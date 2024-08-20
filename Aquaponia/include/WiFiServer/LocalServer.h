#ifndef LOCAL_SEVER_H
#define LOCAL_SEVER_H

#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncJson.h>
#include <ESPmDNS.h>

#include <IPAddress.h>

#include <WString.h>
#include "Base/config.h"
#include "Json/Json.h"

using namespace std;

using ActionFunction = std::function<DynamicJsonDocument(AsyncWebServerRequest *request)>;

class LocalServer
{
private:
    AsyncEventSource events;
    AsyncWebServer server;
    // AsyncServer server;
    AsyncClient *client = nullptr;

public:
    LocalServer();
    void sendWsDataToClient(AsyncWebSocketClient *client, Json data);
    void onClientConnect(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);
    void onMessage(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);
    void onClientDisconnect(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);
    void onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);
    static void onClientConnected(void *arg, AsyncClient *client);

    void sendWsData(Json json);

    void init();
    void startSocket(AsyncWebSocket *serverSocket);

    void addEndpoint(const char *pathname, ActionFunction action);
    void handlerEvents();
};

LocalServer::LocalServer() : server(80), events("/events")
{
    server.addHandler(&events);

    handlerEvents();
    
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/html", "<h1>Olá, ESP32 está no modo AP!</h1>");
    });

}

void LocalServer::addEndpoint(const char *pathname, ActionFunction action)
{
    server.on(pathname, HTTP_GET, [action](AsyncWebServerRequest *request){
        DynamicJsonDocument result = action(request);
        JsonObject jsonResult = result.as<JsonObject>();

        String resultString;
        serializeJson(result, resultString);
        Serial.println(resultString);

        request->send(static_cast<int>(jsonResult["status_code"]), "text/json", resultString);
    });
}

void LocalServer::init()
{
  if (!MDNS.begin("esp32")) {
    Serial.println("Erro ao iniciar mDNS");
    return;
  }
  
  server.begin();
  MDNS.addService("http", "tcp", 80);
}

void LocalServer::startSocket(AsyncWebSocket *serverSocket)
{
    server.addHandler(serverSocket);
}

void LocalServer::handlerEvents()
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
}

#endif