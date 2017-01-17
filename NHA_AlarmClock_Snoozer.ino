/*
 Nolando Home Automation - MQTT Snooze Button

 Designed to provide a portable snooze button for my MQTT 
 based alarm clock which replaces my cell phone.
*/
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Bounce2.h>

#define DEBUG

// Home Wifi
const char* ssid = "Abraham Linksys";
const char* password = "shiny shimmery sparkly bits";
// Office Wifi
//const char* ssid = "SkyNet";
//const char* password = "alphabetasoup";

// MQTT credentials
const char* mqtt_server = "io.adafruit.com";
const char* mqtt_user = "nolando";
const char* mqtt_pass = "f8f4d1761f9e4995afbeb5ff8af7e0b7";

WiFiClient espClient;
PubSubClient client(espClient);

// Pins based on WeMos D1 Mini board
#define BUTTON_PIN 0  // D3
#define LED_PIN 2     // D4

long lastLed = 0;
int ledState = 0;

String alarmSet = "OFF";
String alarmRinging = "OFF";

Bounce debouncer = Bounce(); 
int oldButtonValue = LOW;

void debug(String message) {
#ifdef DEBUG
  Serial.print(message);
#endif
}

void setup_wifi() {
  delay(10);
  debug("\n[WIFI] Connecting to ");
  debug(ssid);
  debug("\n");

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    debug(".");
  }
  
  debug("\n[WIFI] Connected, IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
  String feedTopic = topic;
  String feedData = "";
  for (int i = 0; i < length; i++) {
    feedData.concat((char)payload[i]);
  }
  
  if (feedTopic == "nolando/f/alarm-set") {
    alarmSet = feedData;
    debug("[DATA} set: ");
    debug(alarmSet + "\n");
  } else if (feedTopic == "nolando/f/alarm-ringing") {
    alarmRinging = feedData;
    debug("[DATA} ringing: ");
    Serial.println(alarmRinging);
  } else if (feedTopic == "nolando/f/alarm-snooze-ping") {
    if (feedData == "ping") {
      debug("[DATA} pinged...");
      client.publish("nolando/f/alarm-snooze-ping", "pong");
      Serial.println("ponged...");
      
      // Assuming LED is already off, blink fast if alarm is not running
      if (alarmRinging == "OFF") {
        for (int i = 0; i < 5; i++) {
          digitalWrite(LED_PIN, LOW);
          delay(100);
          digitalWrite(LED_PIN, HIGH);
          delay(100);
        }
      }
    }
  } else if (feedTopic == "nolando/f/alarm-snooze-poke") {
    if (feedData == "OFF") {
      digitalWrite(LED_PIN, HIGH);
    }
  }
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    debug("[MQTT] Attempting MQTT connection...");
    if (client.connect("ESP8266Snoozer", mqtt_user, mqtt_pass)) {
      Serial.println("[MQTT] Connected");
      client.subscribe("nolando/f/alarm-set");
      client.subscribe("nolando/f/alarm-ringing");
      client.subscribe("nolando/f/alarm-snooze-ping");
      client.subscribe("nolando/f/alarm-snooze-poke");
    } else {
      debug("[MQTT] failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);
  pinMode(BUTTON_PIN,INPUT_PULLUP);

  debouncer.attach(BUTTON_PIN);
  debouncer.interval(5);
  
  Serial.begin(115200);
  
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();  // Run through MQTT updater

  debouncer.update();
  int newButtonValue = debouncer.read();

  if (alarmSet == "ON") {
    if (alarmRinging == "ON") {
      // Manage blinking LED
      long now = millis();
      if (now - lastLed >= 500) {
        lastLed = now;
        digitalWrite(LED_PIN, !digitalRead(LED_PIN)); // Blink period of 1 second
      }
      // Shut off alarm if button pressed
      if (newButtonValue == HIGH && newButtonValue != oldButtonValue) {
        debug("[STAT] Pressed, alarm snoozed\n");
        alarmSet = "OFF";
        client.publish("nolando/f/alarm-set", "OFF");
        digitalWrite(LED_PIN, HIGH); // Stop blinking
      }
    }
  } else {
    // Send poke to server if button pressed
    if (newButtonValue == HIGH && newButtonValue != oldButtonValue) {
      debug("[STAT] Pressed while off\n");
      client.publish("nolando/f/alarm-snooze-poke", "ON");
      digitalWrite(LED_PIN, LOW);
      delay(500);
      digitalWrite(LED_PIN, HIGH);
    }
  }
  oldButtonValue = newButtonValue;
}
