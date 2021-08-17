/**
 * @file THPII_NODE.ino
 * @author Chen Yijun
 * @brief Please complete the location of node and wifi auth information b4 uploading
 * @version 0.1
 * @date 2021-07-20
 */

#include <Wire.h>
#include <BH1750.h>
#include <BME280I2C.h>

#include <Arduino.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <ir_Gree.h>

#include "EspMQTTClient.h"

#include <ArduinoJson.h>

//-----------------------------------
#define measureInterval 5
#define timesB4Upload 24
const String location = "LivingRoom";//Bedroom,LivingRoom,HostBedroom,StudyRoom
//-----------------------------------

BH1750 lm;
BME280I2C bme;

const uint16_t kIrLed = 12;  // ESP8266 GPIO pin to use. 12 (D6).
IRGreeAC ac(kIrLed);  // Set the GPIO to be used for sending messages.

String lowerLoc = location;

const String strHWID = "THPII-" + location;
const char *HWID = strHWID.c_str();
const String state_topic = "homeassistant/sensor/sensor" + location + "/state";
const String config_topicT = "homeassistant/sensor/sensor" + location + "T/config";
const String config_topicH = "homeassistant/sensor/sensor" + location + "H/config";
const String config_topicP = "homeassistant/sensor/sensor" + location + "P/config";
const String config_topicI = "homeassistant/sensor/sensor" + location + "I/config";

EspMQTTClient client(
  "ssid",
  "passwd",
  "server addr", // MQTT Broker server ip
  //"MQTTUsername",   // Can be omitted if not needed
  //"MQTTPassword",   // Can be omitted if not needed
  HWID, // Client name that uniquely identify your device
  1883  // The MQTT port, default to 1883. this line can be omitted
);

float bmeData[3] = {0, 0, 0};       //bme data array
float cleanedData[4] = {0, 0, 0, 0}; //cleaned data array
int cleanedCnt = 1;
unsigned long lastTime = 0;

void setup()
{
  ac.begin();

  Serial.begin(115200);
  while (!Serial)
  {
    delay(100);
  }
  Wire.begin();
  pinMode(2, OUTPUT); //pins init
  lm.begin();
  while (!bme.begin()) //sensors init
  {
    digitalWrite(2, LOW);
    delay(500);
    digitalWrite(2, HIGH);
    delay(500);
  }
  digitalWrite(2, HIGH);

  client.setMaxPacketSize(512);
  client.enableHTTPWebUpdater(); //OTA
  client.enableDebuggingMessages();
}

void getBmeData()
{
  float temp(NAN), hum(NAN), pres(NAN);

  BME280::TempUnit tempUnit(BME280::TempUnit_Celsius);
  BME280::PresUnit presUnit(BME280::PresUnit_Pa);

  bme.read(pres, temp, hum, tempUnit, presUnit);
  bmeData[0] = temp;
  bmeData[1] = hum;
  bmeData[2] = pres;
}

float getLux()
{
  float lux = lm.readLightLevel();
  return lux;
}

void dataCleaning()
{
  getBmeData();
  float lux = getLux();
  if (cleanedCnt == 1)
  {
    cleanedData[0] = bmeData[0];
    cleanedData[1] = bmeData[1];
    cleanedData[2] = bmeData[2];
    cleanedData[3] = lux;
  }
  else
  {
    cleanedData[0] = (bmeData[0] + cleanedData[0]) / 2;
    cleanedData[1] = (bmeData[1] + cleanedData[1]) / 2;
    cleanedData[2] = (bmeData[2] + cleanedData[2]) / 2;
    cleanedData[3] = (lux + cleanedData[3]) / 2;
  }
  cleanedCnt++;
}

void pubState() //sensors data to json
{
  int i = 0;
  String jsonMsg;
  StaticJsonDocument<500> doc;

  cleanedData[0] = ((float)((int)((cleanedData[0] + 0.005) * 100))) / 100;
  cleanedData[1] = ((float)((int)((cleanedData[1] + 0.005) * 100))) / 100;
  cleanedData[3] = ((float)((int)((cleanedData[3] + 0.005) * 100))) / 100;

  doc["temp"] = cleanedData[0];
  doc["hum"] = cleanedData[1];
  doc["pres"] = cleanedData[2];
  doc["lux"] = cleanedData[3];
  serializeJson(doc, jsonMsg);
  client.publish(state_topic, jsonMsg);

  for (i = 0; i < 4; i++)
  {
    cleanedData[i] = 0;
  }
  cleanedCnt = 1;
}

String buildHassDiscoveryMsgT()
{
  String jsonMsg;
  StaticJsonDocument<500> doc;
  doc["device_class"] = "temperature";
  doc["unique_id"] = "temperature_" + lowerLoc;
  doc["name"] = "Temperature" + location;
  doc["state_topic"] = state_topic;
  doc["unit_of_measurement"] = "°C";
  doc["value_template"] = "{{ value_json.temp}}";
  serializeJson(doc, jsonMsg);
  return jsonMsg;
}

String buildHassDiscoveryMsgH()
{
  String jsonMsg;
  StaticJsonDocument<500> doc;
  doc["device_class"] = "humidity";
  doc["unique_id"] = "humidity_" + lowerLoc;
  doc["name"] = "Humidity" + location;
  doc["state_topic"] = state_topic;
  doc["unit_of_measurement"] = "%";
  doc["value_template"] = "{{ value_json.hum}}";
  serializeJson(doc, jsonMsg);
  return jsonMsg;
}

String buildHassDiscoveryMsgP()
{
  String jsonMsg;
  StaticJsonDocument<500> doc;
  doc["device_class"] = "pressure";
  doc["unique_id"] = "pressure_" + lowerLoc;
  doc["name"] = "Pressure" + location;
  doc["state_topic"] = state_topic;
  doc["unit_of_measurement"] = "Pa";
  doc["value_template"] = "{{ value_json.pres}}";
  serializeJson(doc, jsonMsg);
  return jsonMsg;
}

String buildHassDiscoveryMsgI()
{
  String jsonMsg;
  StaticJsonDocument<500> doc;
  doc["device_class"] = "illuminance";
  doc["unique_id"] = "illmunance_" + lowerLoc;
  doc["name"] = "Illuminance" + location;
  doc["state_topic"] = state_topic;
  doc["unit_of_measurement"] = "lx";
  doc["value_template"] = "{{ value_json.lux}}";
  serializeJson(doc, jsonMsg);
  return jsonMsg;
}

void pubHassDiscoveryMsg()
{
  lowerLoc.toLowerCase();
  client.publish(config_topicT, buildHassDiscoveryMsgT(), true);
  client.publish(config_topicH, buildHassDiscoveryMsgH(), true);
  client.publish(config_topicP, buildHassDiscoveryMsgP(), true);
  client.publish(config_topicI, buildHassDiscoveryMsgI(), true);
}

void onConnectionEstablished()
{
  pubHassDiscoveryMsg();

  client.subscribe(location + "/command", [](const String & payload) {
    if (payload == "reset") {
      ESP.reset();
    }
    if (payload == "led_on") {
      digitalWrite(2, LOW);
    }
    if (payload == "led_off") {
      digitalWrite(2, HIGH);
    }
    acRemote(payload);
  });
}

void acRemote(String payload) {
  if (payload == "gree_ac_on") {
    ac.on();
    ac.setFan(1);
    // kGreeAuto, kGreeDry, kGreeCool, kGreeFan, kGreeHeat
    ac.setMode(kGreeCool);
    ac.setTemp(25);  // 16-30C
    ac.setSwingVertical(false, kGreeSwingAuto);
    ac.setXFan(false);
    ac.setLight(false);
    ac.setSleep(false);
    ac.setTurbo(false);
    ac.send();
  }
  if (payload == "gree_ac_off") {
    ac.off();
    ac.setFan(1);
    // kGreeAuto, kGreeDry, kGreeCool, kGreeFan, kGreeHeat
    ac.setMode(kGreeCool);
    ac.setTemp(25);  // 16-30C
    ac.setSwingVertical(false, kGreeSwingAuto);
    ac.setXFan(false);
    ac.setLight(false);
    ac.setSleep(false);
    ac.setTurbo(false);
    ac.send();
  }
}

void loop()
{
  if (millis() - lastTime > measureInterval * 1000)
  {
    lastTime = millis();
    dataCleaning();
  }

  if (cleanedCnt == timesB4Upload)
  {
    pubState();
  }

  client.loop();
}
