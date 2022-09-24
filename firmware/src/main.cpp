#include <ESP8266WiFi.h>          // Replace with WiFi.h for ESP32
//#include <ESP8266WebServer.h>     // Replace with WebServer.h for ESP32
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager WiFi Configuration Magic
#include <EasyButton.h>
#include "EspMQTTClient.h"

// Arduino pin where the Flash button is connected to
#define BUTTON_FLASH 0
#define OUTPUT_PUSH_BUTTON  D1
#define OUTPUT_24V_LED      D2
#define INPUT_DOOR_OPEN     D5
#define INPUT_DOOR_CLOSED   D6

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

enum DoorState {
  OPEN,
  DOOR_CLOSED,
  PARTIAL,
  INVALID,
  UNKNOWN
};

DoorState door_state = UNKNOWN;

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

void pressDoorButton() {
  digitalWrite(OUTPUT_PUSH_BUTTON, HIGH); // depress the button
  delay(500);
  digitalWrite(OUTPUT_PUSH_BUTTON, LOW); // back to high impedance
}

void onConnectionEstablished()
{
  // Here you are sure that everything is connected.
  // Subscribe to "mytopic/test" and display received message to Serial
  mqttClient.subscribe("garage-door/command", [](const String & payload) {
    Serial.println(payload);
    pressDoorButton();
  });

  // Publish a message to "mytopic/test"
  mqttClient.publish("garage-door/availability", "online");
  door_state = DoorState::UNKNOWN;
  //mqttClient.publish("garage-door/state", "open"); // You can activate the retain flag by setting the third parameter to true
}

void flashLed(int times) {
  for(int i=0; i<times; i++) {
    digitalWrite(LED_BUILTIN, LOW);
    delay(200);
    digitalWrite(LED_BUILTIN, HIGH);
    if (i < times) delay(200);
  }
}

void setup() {
  pinMode(BUTTON_FLASH, INPUT_PULLUP);
  pinMode(LED_BUILTIN, OUTPUT);     // Initialize the LED_BUILTIN pin as an output

  pinMode(OUTPUT_PUSH_BUTTON, OUTPUT);
  pinMode(OUTPUT_24V_LED, OUTPUT);
  pinMode(INPUT_DOOR_OPEN, INPUT);
  pinMode(INPUT_DOOR_CLOSED, INPUT);

  digitalWrite(LED_BUILTIN, LOW); // Turn the LED on
  digitalWrite(OUTPUT_PUSH_BUTTON, LOW); // Don't be pressing the button
  digitalWrite(OUTPUT_24V_LED, LOW); // Main LED off to start with

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
  mqttClient.enableLastWillMessage("garage-door/availability", "offline");

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

// the loop function runs over and over again forever
void loop() {
  wifiManager.process();
  flashButton.read();
  mqttClient.loop();

  int doorClosed = digitalRead(INPUT_DOOR_CLOSED);
  int doorOpen = digitalRead(INPUT_DOOR_OPEN);
  DoorState new_state;
  if (doorClosed == LOW && doorOpen == HIGH) {
    new_state = DoorState::DOOR_CLOSED;
  } else if (doorClosed == HIGH && doorOpen == LOW) {
    new_state = DoorState::OPEN;
  } else if (doorClosed == HIGH && doorOpen == HIGH) {
    new_state = DoorState::PARTIAL;
  } else {
    new_state = DoorState::INVALID;
  }

  // Can only proceed if MQTT is connected
  if (!mqttClient.isConnected()) return;

  // Nothing has changed
  if (door_state == new_state) return;

  if (new_state == DoorState::DOOR_CLOSED) {
    Serial.println("Door has closed");
    mqttClient.publish("garage-door/state", "closed");
  } else if (new_state == DoorState::OPEN) {
    Serial.println("Door has opened");
    mqttClient.publish("garage-door/state", "open");
  } else if (new_state == DoorState::PARTIAL && door_state == DoorState::OPEN) {
    // transition from OPEN to PARTIAL means we are closing
    Serial.println("Door is closing");
    mqttClient.publish("garage-door/state", "closing");
  } else if (new_state == DoorState::PARTIAL && door_state == DoorState::DOOR_CLOSED) {
    // transition from CLOSED to PARTIAL means we are opening
    Serial.println("Door is opening");
    mqttClient.publish("garage-door/state", "opening");
  } else if (new_state == DoorState::PARTIAL && door_state == DoorState::UNKNOWN) {
    // it must have been partially open on start-up
    Serial.println("Door is partially open, assuming closing");
    mqttClient.publish("garage-door/state", "closing");
  } else if (new_state == DoorState::INVALID) {
    // we have a problem
    Serial.println("Door state is invalid (both microswitches LOW)");
    mqttClient.publish("garage-door/state", "closing");
  }

  door_state = new_state;

  delay(100);
}
