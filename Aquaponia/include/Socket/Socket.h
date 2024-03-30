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

using ActionFunction = std::function<Json(AsyncWebServerRequest *request)>;

class Socket
{
private:
    AsyncWebServer server;
    AsyncWebSocket socket;
    AsyncEventSource events;

public:
    Socket();
    void sendWsDataToClient(AsyncWebSocketClient *client, Json data);
    void onClientConnect(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);
    void onMessage(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);
    void onClientDisconnect(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);
    void onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);

    void sendWsData(String actionName, Json json);

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
        Json result = action(request);

        request->send(result.get<int>("status_code"), "text/json", result.serializeToString());
      });
}

void Socket::init()
{
    server.begin();
}

void Socket::sendWsDataToClient(AsyncWebSocketClient *client, Json data)
{
    socket.text(client->id(), data.serializeToString());
}

void Socket::sendWsData(String actionName, Json data)
{
    Json responseData;
    responseData.set("action", actionName);
    responseData.set("data", data);

    socket.textAll(responseData.serializeToString());
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

    Json responseData;
    sendWsDataToClient(client, responseData);
}
#endif