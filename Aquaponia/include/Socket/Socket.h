#ifndef Socket_H
#define Socket_H
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

using namespace std;

using ActionFunction = std::function<DynamicJsonDocument(AsyncWebServerRequest *request)>;

class Socket
{
private:
    AsyncWebServer server;
    AsyncWebSocket socket;
    AsyncEventSource events;

public:
    Socket();
    void sendWsDataToClient(AsyncWebSocketClient *client, DynamicJsonDocument data);
    void onClientConnect(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);
    void onMessage(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);
    void onClientDisconnect(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);
    void onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);

    void sendWsData(String actionName, DynamicJsonDocument json);

    void init();

    void addEndpoint(const char *pathname, ActionFunction action);
};

Socket::Socket() : server(80), socket("/ws"), events("/events")
{
    events.onConnect([](AsyncEventSourceClient *client) {});

    socket.onEvent([this](AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
                   { this->onWebSocketEvent(server, client, type, arg, data, len); });
    server.addHandler(&events);
    server.addHandler(&socket);
}

void Socket::addEndpoint(const char *pathname, ActionFunction action)
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


void Socket::init()
{
    server.begin();
}

void Socket::sendWsDataToClient(AsyncWebSocketClient *client, DynamicJsonDocument data)
{
    String resultString;
    serializeJson(data, resultString);

    socket.text(client->id(), resultString);
}

void Socket::sendWsData(String actionName, DynamicJsonDocument data)
{
    data["action"] = actionName;
    data["data"] = data;

    String resultString;
    serializeJson(data, resultString);

    socket.textAll(resultString);
}

void Socket::onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
    switch (type)
    {
    case WS_EVT_CONNECT:
        onClientConnect(server, client, type, arg, data, len);
        break;
    case WS_EVT_DISCONNECT:
        onClientDisconnect(server, client, type, arg, data, len);
        break;
    case WS_EVT_DATA:
        onMessage(server, client, type, arg, data, len);
        break;
    }
}

void Socket::onClientConnect(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
    Serial.println("WebSocket client connected");
}
void Socket::onClientDisconnect(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
    Serial.println("WebSocket client disconnected");
}

void Socket::onMessage(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
    AwsFrameInfo *info = (AwsFrameInfo *)arg;
    if (info->opcode == WS_TEXT)
    {
        Serial.printf("WebSocket received message: %.*s\n", len, data);
    }
    DynamicJsonDocument doc(len);
    JsonObject responseData = doc.to<JsonObject>();
    responseData["data"] = data;
    sendWsDataToClient(client, responseData);
}
#endif