#include <ESP8266WiFi.h>          // Replace with WiFi.h for ESP32
//#include <ESP8266WebServer.h>     // Replace with WebServer.h for ESP32
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager WiFi Configuration Magic
#include <EasyButton.h>

// Arduino pin where the Flash button is connected to
#define BUTTON_FLASH 0

//ESP8266WebServer Server;          // Replace with WebServer for ESP32
WiFiManager wifiManager;
EasyButton flashButton(BUTTON_FLASH);

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

void setup() {
  pinMode(BUTTON_FLASH, INPUT_PULLUP);
  pinMode(LED_BUILTIN, OUTPUT);     // Initialize the LED_BUILTIN pin as an output

  Serial.begin(115200);
  String deviceName = "GarageDoor-" + String(ESP.getChipId(), HEX);
  Serial.print("\nHello! Device name = ");
  Serial.println(deviceName);

  //Server.on("/", rootPage);

  // Captive portal for first-time WiFi setup
  //wifiManager.setHostname(deviceName);
  WiFi.hostname(deviceName);
  wifiManager.autoConnect(deviceName.c_str(), NULL);
  WiFi.hostname(deviceName);
  Serial.println("WiFi connected: IP = " + WiFi.localIP().toString());

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
}

// the loop function runs over and over again forever
void loop() {
  wifiManager.process();
  flashButton.read();
  
  digitalWrite(LED_BUILTIN, LOW);   // Turn the LED on (Note that LOW is the voltage level
  // but actually the LED is on; this is because
  // it is active low on the ESP-01)
  delay(250);                      // Wait for a second
  digitalWrite(LED_BUILTIN, HIGH);  // Turn the LED off by making the voltage HIGH
  delay(500);                      // Wait for two seconds (to demonstrate the active low LED)
}
