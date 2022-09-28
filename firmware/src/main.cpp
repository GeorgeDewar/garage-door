#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <EasyButton.h>
#include <EspMQTTClient.h>

// NodeMCU board pins
#define BUTTON_FLASH        D3
#define OUTPUT_PUSH_BUTTON  D1
#define OUTPUT_24V_LED      D2
#define INPUT_DOOR_OPEN     D5
#define INPUT_DOOR_CLOSED   D6

// Flashing speeds
#define FLASHING_SLOW_MS    500
#define FLASHING_FAST_MS    250

// Possible states for the door to be in, based on the microswitch positions
enum DoorState {
  OPEN,
  DOOR_CLOSED,
  PARTIAL,      // neither open nor closed, which means opening, closing, or just stopped part way
  INVALID,      // i.e. both open and closed, shouldn't be possible
  UNKNOWN       // the default state prior to reading it for the first time, or to force an update
};

// Objects
WiFiManager wifiManager;
EasyButton flashButton(BUTTON_FLASH);
EspMQTTClient mqttClient(
  "192.168.20.68",
  1883,            // It is mandatory here to allow these constructors to be distinct from those with the Wifi handling parameters
  "DVES_USER",     // Omit this parameter to disable MQTT authentification
  "DVES_PASS",     // Omit this parameter to disable MQTT authentification
  "GarageDoor");

// Variables
DoorState door_state = UNKNOWN;
unsigned long lastBlinkMillis = 0; // when the loop function last ran
int lastFlashingState = LOW;

// Interrupt service routine for the interrupt triggered by the Flash button
void flashButtonISR()
{
  flashButton.read();
}

// Callback function to be called when the Flash button is pressed. Erase all configuration.
void onFlashButtonPressed() {
  Serial.println("Flash button has been pressed! Resetting...");
  ESP.eraseConfig();
  delay(500);
  ESP.restart();
}

// Simulate a press of the open/close button
void pressDoorButton() {
  digitalWrite(OUTPUT_PUSH_BUTTON, HIGH); // depress the button
  delay(250);
  digitalWrite(OUTPUT_PUSH_BUTTON, LOW); // back to high impedance
}

// Called when MQTT connection established
void onConnectionEstablished()
{
  // Subscribe to commands. For any command, we press the button. That's all we can do.
  mqttClient.subscribe("garage-door/command", [](const String & payload) {
    Serial.println(payload);
    pressDoorButton();
  });

  // Announce that we are online
  mqttClient.publish("garage-door/availability", "online");

  // Force a status update, in case we've been offline for a while
  door_state = DoorState::UNKNOWN;
}

// Flash the internal LED the specified number of times
void flashBuiltInLed(int times) {
  for(int i=0; i<times; i++) {
    digitalWrite(LED_BUILTIN, LOW);
    delay(200);
    digitalWrite(LED_BUILTIN, HIGH);
    if (i < times) delay(200);
  }
}

// Called once on startup
void setup() {
  // Initialise our inputs and outputs
  pinMode(BUTTON_FLASH, INPUT_PULLUP);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(OUTPUT_PUSH_BUTTON, OUTPUT);    // note: when low, the external transistor is high impedance (floating)
  pinMode(OUTPUT_24V_LED, OUTPUT);        // note: when low, the external transistor is high impedance (floating)
  pinMode(INPUT_DOOR_OPEN, INPUT);
  pinMode(INPUT_DOOR_CLOSED, INPUT);

  // Write the initial states of our outputs
  digitalWrite(LED_BUILTIN, LOW);         // Turn the LED on
  digitalWrite(OUTPUT_PUSH_BUTTON, LOW);  // Don't be pressing the button
  digitalWrite(OUTPUT_24V_LED, LOW);      // Main LED off to start with

  // Begin serial communication for debugging
  Serial.begin(115200);

  // Work out and announce our device name
  String deviceName = "GarageDoor-" + String(ESP.getChipId(), HEX);
  Serial.print("\nHello! Device name = ");
  Serial.println(deviceName);

  // Captive portal for first-time WiFi setup
  wifiManager.setConfigPortalTimeout(120); // after two minutes, give up on connecting
  if (wifiManager.autoConnect(deviceName.c_str(), NULL)) {
    // We're connected, so we set the host name and announce our IP address
    WiFi.hostname(deviceName);
    Serial.println("WiFi connected: IP = " + WiFi.localIP().toString());

    // Always run the web portal so the device can be reconfigured
    wifiManager.startWebPortal();
  } else {
    Serial.println("Failed to connect to WiFi; in offline mode");
  }

  // Configure the MQTT client
  mqttClient.enableDebuggingMessages();
  mqttClient.enableLastWillMessage("garage-door/availability", "offline");

  // Configure the Flash button
  flashButton.begin();
  flashButton.onPressed(onFlashButtonPressed);
  flashButton.enableInterrupt(flashButtonISR);

  // Flash built-in LED to indicate startup complete
  flashBuiltInLed(3); // really 2 flashes as LED already on
}

// the loop function runs over and over again forever
void loop() {
  // Let the libraries do their processing
  flashButton.read();
  wifiManager.process();
  mqttClient.loop();

  // Determine the state of the door based on the microswitches
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

  // Handle the LED output
  if (new_state == DoorState::DOOR_CLOSED) {
    digitalWrite(OUTPUT_24V_LED, LOW);
  } else if (new_state == DoorState::OPEN) {
    digitalWrite(OUTPUT_24V_LED, HIGH);
  } else {
    // we are flashing
    unsigned long currentTime = millis();
    if (currentTime > lastBlinkMillis + FLASHING_SLOW_MS) {
      lastBlinkMillis = currentTime;
      lastFlashingState = 1 - lastFlashingState;
      digitalWrite(OUTPUT_24V_LED, lastFlashingState);
    }
  }

  // If nothing has changed, then there's nothing else we need to do
  if (door_state == new_state) return;

  // Figure out what message to publish based on the old and new state
  // Publishing will do nothing if the client is not connected
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

  // Now update door_state since we don't need the old value any more
  door_state = new_state;
}
