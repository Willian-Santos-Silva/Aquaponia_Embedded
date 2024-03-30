
#ifndef JSON_OBJECT
#define JSON_OBJECT

#include <ArduinoJson.h>

class Json
{
private:
    DynamicJsonDocument doc;
    JsonObject json;

public:
    Json() : doc(1024)
    {
        json = doc.to<JsonObject>();
    }
    Json(String data) : doc(1024)
    {
        deserializeJson(doc, data);
        json = doc.as<JsonObject>();
    }
    String serializeToString() 
    {
        String data;
        serializeJson(doc, data);
        return data;
    }
    template <typename T>
    void set(String name, T value)
    {
        json[name] = value;
    }
    void set(String name, Json value)
    {
        DynamicJsonDocument perentDoc(1024);
        JsonObject parentJson = perentDoc.to<JsonObject>();
        
        DeserializationError err = deserializeJson(perentDoc, value.serializeToString());
        
        json.createNestedObject(name).set(parentJson);

    }
    
    template <typename T>
    T get(String name)
    {
        return json[name];
    }

    Json get(String name)
    {
        return Json(get<String>(name));
    }
};
#endif