#include "WebApi_inverter.h"
#include "ArduinoJson.h"
#include "AsyncJson.h"
#include "Configuration.h"
#include "Hoymiles.h"
#include "helper.h"

void WebApiInverterClass::init(AsyncWebServer* server)
{
    using namespace std::placeholders;

    _server = server;

    _server->on("/api/inverter/list", HTTP_GET, std::bind(&WebApiInverterClass::onInverterList, this, _1));
    _server->on("/api/inverter/add", HTTP_POST, std::bind(&WebApiInverterClass::onInverterAdd, this, _1));
    _server->on("/api/inverter/edit", HTTP_POST, std::bind(&WebApiInverterClass::onInverterEdit, this, _1));
    _server->on("/api/inverter/del", HTTP_POST, std::bind(&WebApiInverterClass::onInverterDelete, this, _1));
}

void WebApiInverterClass::loop()
{
}

void WebApiInverterClass::onInverterList(AsyncWebServerRequest* request)
{
    AsyncJsonResponse* response = new AsyncJsonResponse();
    JsonObject root = response->getRoot();
    JsonArray data = root.createNestedArray(F("inverter"));

    CONFIG_T& config = Configuration.get();

    for (uint8_t i = 0; i < INV_MAX_COUNT; i++) {
        if (config.Inverter[i].Serial > 0) {
            JsonObject obj = data.createNestedObject();
            obj[F("id")] = i;
            obj[F("name")] = String(config.Inverter[i].Name);

            // Inverter Serial is read as HEX
            char buffer[sizeof(uint64_t) * 8 + 1];
            sprintf(buffer, "%0lx%08lx",
                ((uint32_t)((config.Inverter[i].Serial >> 32) & 0xFFFFFFFF)),
                ((uint32_t)(config.Inverter[i].Serial & 0xFFFFFFFF)));
            obj[F("serial")] = buffer;
        }
    }

    response->setLength();
    request->send(response);
}

void WebApiInverterClass::onInverterAdd(AsyncWebServerRequest* request)
{
    AsyncJsonResponse* response = new AsyncJsonResponse();
    JsonObject retMsg = response->getRoot();
    retMsg[F("type")] = F("warning");

    if (!request->hasParam("data", true)) {
        retMsg[F("message")] = F("No values found!");
        response->setLength();
        request->send(response);
        return;
    }

    String json = request->getParam("data", true)->value();

    if (json.length() > 1024) {
        retMsg[F("message")] = F("Data too large!");
        response->setLength();
        request->send(response);
        return;
    }

    DynamicJsonDocument root(1024);
    DeserializationError error = deserializeJson(root, json);

    if (error) {
        retMsg[F("message")] = F("Failed to parse data!");
        response->setLength();
        request->send(response);
        return;
    }

    if (!(root.containsKey("serial") && root.containsKey("name"))) {
        retMsg[F("message")] = F("Values are missing!");
        response->setLength();
        request->send(response);
        return;
    }

    if (root[F("serial")].as<uint64_t>() == 0) {
        retMsg[F("message")] = F("Serial must be a number > 0!");
        response->setLength();
        request->send(response);
        return;
    }

    if (root[F("name")].as<String>().length() == 0 || root[F("name")].as<String>().length() > INV_MAX_NAME_STRLEN) {
        retMsg[F("message")] = F("Name must between 1 and " STR(INV_MAX_NAME_STRLEN) " characters long!");
        response->setLength();
        request->send(response);
        return;
    }

    INVERTER_CONFIG_T* inverter = Configuration.getFreeInverterSlot();

    if (!inverter) {
        retMsg[F("message")] = F("Only " STR(INV_MAX_COUNT) " inverters are supported!");
        response->setLength();
        request->send(response);
        return;
    }

    char* t;
    // Interpret the string as a hex value and convert it to uint64_t
    inverter->Serial = strtoll(root[F("serial")].as<String>().c_str(), &t, 16);

    strncpy(inverter->Name, root[F("name")].as<String>().c_str(), INV_MAX_NAME_STRLEN);
    Configuration.write();

    retMsg[F("type")] = F("success");
    retMsg[F("message")] = F("Inverter created!");

    response->setLength();
    request->send(response);

    Hoymiles.addInverter(inverter->Name, inverter->Serial);
}

void WebApiInverterClass::onInverterEdit(AsyncWebServerRequest* request)
{
    AsyncJsonResponse* response = new AsyncJsonResponse();
    JsonObject retMsg = response->getRoot();
    retMsg[F("type")] = F("warning");

    if (!request->hasParam("data", true)) {
        retMsg[F("message")] = F("No values found!");
        response->setLength();
        request->send(response);
        return;
    }

    String json = request->getParam("data", true)->value();

    if (json.length() > 1024) {
        retMsg[F("message")] = F("Data too large!");
        response->setLength();
        request->send(response);
        return;
    }

    DynamicJsonDocument root(1024);
    DeserializationError error = deserializeJson(root, json);

    if (error) {
        retMsg[F("message")] = F("Failed to parse data!");
        response->setLength();
        request->send(response);
        return;
    }

    if (!(root.containsKey("id") && root.containsKey("serial") && root.containsKey("name"))) {
        retMsg[F("message")] = F("Values are missing!");
        response->setLength();
        request->send(response);
        return;
    }

    if (root[F("id")].as<uint8_t>() > INV_MAX_COUNT - 1) {
        retMsg[F("message")] = F("Invalid ID specified!");
        response->setLength();
        request->send(response);
        return;
    }

    if (root[F("serial")].as<uint64_t>() == 0) {
        retMsg[F("message")] = F("Serial must be a number > 0!");
        response->setLength();
        request->send(response);
        return;
    }

    if (root[F("name")].as<String>().length() == 0 || root[F("name")].as<String>().length() > INV_MAX_NAME_STRLEN) {
        retMsg[F("message")] = F("Name must between 1 and " STR(INV_MAX_NAME_STRLEN) " characters long!");
        response->setLength();
        request->send(response);
        return;
    }

    INVERTER_CONFIG_T& inverter = Configuration.get().Inverter[root[F("id")].as<uint8_t>()];

    char* t;
    // Interpret the string as a hex value and convert it to uint64_t
    inverter.Serial = strtoll(root[F("serial")].as<String>().c_str(), &t, 16);
    strncpy(inverter.Name, root[F("name")].as<String>().c_str(), INV_MAX_NAME_STRLEN);
    Configuration.write();

    retMsg[F("type")] = F("success");
    retMsg[F("message")] = F("Inverter changed!");

    response->setLength();
    request->send(response);

    std::shared_ptr<InverterAbstract> inv = Hoymiles.getInverterByPos(root[F("id")].as<uint64_t>());
    inv->setName(inverter.Name);
    inv->setSerial(inverter.Serial);
}

void WebApiInverterClass::onInverterDelete(AsyncWebServerRequest* request)
{
    AsyncJsonResponse* response = new AsyncJsonResponse();
    JsonObject retMsg = response->getRoot();
    retMsg[F("type")] = F("warning");

    if (!request->hasParam("data", true)) {
        retMsg[F("message")] = F("No values found!");
        response->setLength();
        request->send(response);
        return;
    }

    String json = request->getParam("data", true)->value();

    if (json.length() > 1024) {
        retMsg[F("message")] = F("Data too large!");
        response->setLength();
        request->send(response);
        return;
    }

    DynamicJsonDocument root(1024);
    DeserializationError error = deserializeJson(root, json);

    if (error) {
        retMsg[F("message")] = F("Failed to parse data!");
        response->setLength();
        request->send(response);
        return;
    }

    if (!(root.containsKey("id"))) {
        retMsg[F("message")] = F("Values are missing!");
        response->setLength();
        request->send(response);
        return;
    }

    if (root[F("id")].as<uint8_t>() > INV_MAX_COUNT - 1) {
        retMsg[F("message")] = F("Invalid ID specified!");
        response->setLength();
        request->send(response);
        return;
    }

    uint8_t inverter_id = root[F("id")].as<uint8_t>();
    INVERTER_CONFIG_T& inverter = Configuration.get().Inverter[inverter_id];
    inverter.Serial = 0;
    strncpy(inverter.Name, "", 0);
    Configuration.write();

    retMsg[F("type")] = F("success");
    retMsg[F("message")] = F("Inverter deleted!");

    response->setLength();
    request->send(response);

    Hoymiles.removeInverterByPos(inverter_id);
}