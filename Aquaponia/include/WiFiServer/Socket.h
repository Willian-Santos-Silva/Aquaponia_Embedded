#ifndef Socket_H
#define Socket_H

#include <Wifi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncJson.h>

#include <IPAddress.h>

#include <WString.h>
#include "Base/config.h"
#include "Json/Json.h"

using namespace std;

using ActionFunction = std::function<DynamicJsonDocument(AsyncWebServerRequest *request)>;

class Socket
{
private:

public:
    AsyncWebSocket socket;
    Socket();
    void sendWsDataToClient(AsyncWebSocketClient *client, DynamicJsonDocument data);
    void onClientConnect(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);
    void onMessage(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);
    void onClientDisconnect(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);
    void onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);

    void sendWsData(String actionName, DynamicJsonDocument json);

    void init();
};

Socket::Socket() : socket("/ws")
{
    socket.onEvent([this](AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
                   { this->onWebSocketEvent(server, client, type, arg, data, len); });
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