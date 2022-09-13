#include <ESP8266WiFi.h>          // Replace with WiFi.h for ESP32
#include <ESP8266WebServer.h>     // Replace with WebServer.h for ESP32
#include <AutoConnect.h>
#include <EasyButton.h>

// Arduino pin where the Flash button is connected to
#define BUTTON_FLASH 0

ESP8266WebServer Server;          // Replace with WebServer for ESP32
AutoConnect      Portal(Server);
AutoConnectConfig acConfig;
EasyButton flashButton(BUTTON_FLASH);

void rootPage() {
  char content[] = "Hello, world";
  Server.send(200, "text/plain", content);
}

void buttonISR()
{
  flashButton.read(); 
}

// Callback function to be called when the button is pressed.
void onPressed() {
    Serial.println("Button has been pressed!");
    acConfig.immediateStart = true;
    acConfig.autoRise = true;
}

void connect() {
  if (Portal.begin()) {
    Serial.println("WiFi connected: IP = " + WiFi.localIP().toString());
  }
}

void setup() {
  pinMode(BUTTON_FLASH, INPUT_PULLUP);
  pinMode(LED_BUILTIN, OUTPUT);     // Initialize the LED_BUILTIN pin as an output

  Serial.begin(115200);
  String deviceName = "GarageDoor-" + String(ESP.getChipId(), HEX);
  Serial.println("\nHello! Device name = " + deviceName);

  Server.on("/", rootPage);

  // Captive portal for first-time WiFi setup
  acConfig.apid = deviceName;
  acConfig.hostName = deviceName;
  //acConfig.immediateStart = true;
  Portal.config(acConfig);
  connect();

  // Flash button to reset
  flashButton.begin();
  flashButton.onPressed(onPressed);
  if (flashButton.supportsInterrupt())
  {
    Serial.println("interrupt enabled");
    flashButton.enableInterrupt(buttonISR);
  }
}

// the loop function runs over and over again forever
void loop() {
  Portal.handleClient();
  flashButton.read();
  
  digitalWrite(LED_BUILTIN, LOW);   // Turn the LED on (Note that LOW is the voltage level
  // but actually the LED is on; this is because
  // it is active low on the ESP-01)
  delay(1000);                      // Wait for a second
  digitalWrite(LED_BUILTIN, HIGH);  // Turn the LED off by making the voltage HIGH
  delay(2000);                      // Wait for two seconds (to demonstrate the active low LED)
}
