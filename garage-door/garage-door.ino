#include <ESP8266WiFi.h>          // Replace with WiFi.h for ESP32
//#include <ESP8266WebServer.h>     // Replace with WebServer.h for ESP32
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager WiFi Configuration Magic
#include <EasyButton.h>
#include "EspMQTTClient.h"

// Arduino pin where the Flash button is connected to
#define BUTTON_FLASH 0
#define OUTPUT_24V_LED D2

//ESP8266WebServer Server;          // Replace with WebServer for ESP32
WiFiManager wifiManager;
EasyButton flashButton(BUTTON_FLASH);
EspMQTTClient mqttClient(
  "192.168.20.68",
  1883,            // It is mandatory here to allow these constructors to be distinct from those with the Wifi handling parameters
  "DVES_USER",     // Omit this parameter to disable MQTT authentification
  "DVES_PASS",     // Omit this parameter to disable MQTT authentification
  "GarageDoor");

char printbuf[128];
char mqtt_server[40];
WiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqtt_server, 40);

//void rootPage() {
//  char content[] = "Hello, world";
//  Server.send(200, "text/plain", content);
//}

void buttonISR()
{
  flashButton.read(); 
}

// Callback function to be called when the button is pressed.
void onPressed() {
  Serial.println("Button has been pressed!");
  WiFiManager wifiManager;
  ESP.eraseConfig();
  delay(500);
  ESP.restart();
}

void onConnectionEstablished()
{
  // Here you are sure that everything is connected.
  // Subscribe to "mytopic/test" and display received message to Serial
  mqttClient.subscribe("garage-door/command", [](const String & payload) {
    Serial.println(payload);
    digitalWrite(OUTPUT_24V_LED, HIGH);
    delay(500);
    digitalWrite(OUTPUT_24V_LED, LOW);
  });

  // Publish a message to "mytopic/test"
  mqttClient.publish("garage-door/state", "open"); // You can activate the retain flag by setting the third parameter to true
}

void setup() {
  pinMode(BUTTON_FLASH, INPUT_PULLUP);
  pinMode(LED_BUILTIN, OUTPUT);     // Initialize the LED_BUILTIN pin as an output
  pinMode(OUTPUT_24V_LED, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW); // Turn the LED on

  Serial.begin(115200);
  String deviceName = "GarageDoor-" + String(ESP.getChipId(), HEX);
  Serial.print("\nHello! Device name = ");
  Serial.println(deviceName);

  //Server.on("/", rootPage);

  // Captive portal for first-time WiFi setup
  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.autoConnect(deviceName.c_str(), NULL);
  WiFi.hostname(deviceName);
  Serial.println("WiFi connected: IP = " + WiFi.localIP().toString());

  // Store additional parameters
  //mqtt_server = custom_mqtt_server.getValue();
//  if (strlen(mqtt_server) > 0) {
//    sprintf(printbuf, "MQTT server: %s", mqtt_server);
//    Serial.println(printbuf);
//  }
  mqttClient.enableDebuggingMessages();

  // Flash button to reset
  flashButton.begin();
  flashButton.onPressed(onPressed);
  if (flashButton.supportsInterrupt())
  {
    Serial.println("interrupt enabled");
    flashButton.enableInterrupt(buttonISR);
  }

  // Always run web portal
  wifiManager.startWebPortal();

  // Flash to indicate startup complete
  flashLed(3); // really 2 as LED already on
}

void flashLed(int times) {
  for(int i=0; i<times; i++) {
    digitalWrite(LED_BUILTIN, LOW);
    delay(200);
    digitalWrite(LED_BUILTIN, HIGH);
    if (i < times) delay(200);
  }
}

// the loop function runs over and over again forever
void loop() {
  wifiManager.process();
  flashButton.read();
  mqttClient.loop();
}
