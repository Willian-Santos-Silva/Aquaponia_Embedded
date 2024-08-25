#ifndef LOCAL_SEVER_H
#define LOCAL_SEVER_H

#include <ESPAsyncWebServer.h>
#include <AsyncJson.h>

#include "Base/config.h"

using namespace std;

class LocalServer
{
private:
    AsyncEventSource events;
    AsyncWebServer server;
    AsyncClient *client = nullptr;

public:
    LocalServer();
    void onClientConnect(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);
    void onMessage(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);
    void onClientDisconnect(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);
    void onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);
    static void onClientConnected(void *arg, AsyncClient *client);


    void init();
    void startSocket(AsyncWebSocket *serverSocket);

    void addEndpoint(const char *pathname, std::function<DynamicJsonDocument (DynamicJsonDocument* request)> action);
    void handlerEvents();
};

LocalServer::LocalServer() : server(80), events("/events")
{
    server.addHandler(&events);

    handlerEvents();
    
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/html", "<h1>Olá, ESP32 está no modo STA!</h1>");
    });

}

void LocalServer::addEndpoint(const char *pathname, std::function<DynamicJsonDocument (DynamicJsonDocument* request)> action)
{
    server.on(pathname, HTTP_GET, [action](AsyncWebServerRequest *request){
      JsonObject jsonResult;
      String resultString;

      try{
        const char* data = "{}";
        if(request->hasParam("data")){
          data = request->getParam("data")->value().c_str();
        }

        DynamicJsonDocument docRequest(request->getParam("data")->size());
        deserializeJson(docRequest, data);
        delete data;

        DynamicJsonDocument doc = action(&docRequest);
        jsonResult = doc.as<JsonObject>();
        serializeJson(doc, resultString);
      }
      catch (const std::exception &e)
      {
        DynamicJsonDocument doc(500);
        jsonResult = doc.to<JsonObject>();
        jsonResult["status_code"] = 505;
        jsonResult["status_description"] = e.what();
        serializeJson(doc, resultString);
      }

      Serial.printf("===================================\n%s\n===================================", resultString.c_str());

      request->send(200, "text/json", resultString);
    });
}

void LocalServer::init()
{
  server.begin();
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