#ifndef LOCAL_SEVER_H
#define LOCAL_SEVER_H
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

AsyncWebServer server(80);
AsyncEventSource events("/events");


using ActionFunction = std::function<Json()>;

class LocalServer
{
public:
    LocalServer();
    void sendWsDataToClient(AsyncWebSocketClient *client, Json data);
    void onClientConnect(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);
    void onMessage(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);
    void onClientDisconnect(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);
    void onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);

    void sendWsData(Json json);

    void init();

    void addEndpoint(const char *pathname, ActionFunction action);
};

LocalServer::LocalServer()
{
    events.onConnect([](AsyncEventSourceClient *client) {});

    server.addHandler(&events);
}

void LocalServer::addEndpoint(const char *pathname, ActionFunction action)
{
    server.on(pathname, HTTP_GET, [action](AsyncWebServerRequest *request)
              { request->send(200, "application/json", action().serializeToString()); });
}

void LocalServer::init()
{
    server.begin();
}
void LocalServer::onLocalServerEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
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

void LocalServer::onClientConnect(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
    Serial.println("WebSocket client connected");
}
void LocalServer::onClientDisconnect(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
    Serial.println("WebSocket client disconnected");
}

void LocalServer::onMessage(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
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