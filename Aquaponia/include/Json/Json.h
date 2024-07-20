
#ifndef JSON_OBJECT
#define JSON_OBJECT

#include <ArduinoJson.h>

class Json
{
private:

public:
    DynamicJsonDocument doc;
    JsonObject jsonObject;
    Json() : doc(5026)
    {
        jsonObject = doc.to<JsonObject>();
    }
    Json(String data) : doc(5026)
    {
        deserializeJson(doc, data);
        jsonObject = doc.as<JsonObject>();
    }
    Json(JsonArray array) : doc(5026)
    {
        jsonObject = array.createNestedObject();
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
        jsonObject[name] = value;
    }

    template <typename T, size_t N>
    void setArray(String name, const T (&values)[N])
    {
        JsonArray jsonArray = doc.createNestedArray(name);

        for (size_t i = 0; i < N; ++i) {
            jsonArray.add(values[i]);
        }
    }
    
    void set(String name, Json value)
    {
        DynamicJsonDocument perentDoc(1024);
        JsonObject parentJson = perentDoc.to<JsonObject>();
        
        DeserializationError err = deserializeJson(perentDoc, value.serializeToString());
        
        jsonObject.createNestedObject(name).set(parentJson);

    }
    
    JsonArray createArray(String name)
    {
        return doc.createNestedArray(name);

    }
    template <typename T>
    T get(String name)
    {
        return jsonObject[name];
    }

    Json get(String name)
    {
        return Json(get<String>(name));
    }
    

    // #define PRINT_VARIABLE_NAME(variable) printVariable(#variable);
    // string printVariable(const char *name)
    // {
    // return name;
    // }
};
#endif