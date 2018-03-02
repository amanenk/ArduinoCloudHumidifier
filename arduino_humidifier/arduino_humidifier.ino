/**************************************************************
sketch writed to controll arduino powered humidifier
 **************************************************************/
#include <SoftwareReset.h>

// Select your modem:
#define TINY_GSM_MODEM_ESP8266

//setup pins for your board
#define LED_STRIP_PIN 5
#define TOGGLE_PIN 4
#define STATE_PIN 2
#define ESP_ENABLE_PIN 6

#define LED_NUM 8
uint32_t color;

#include <Adafruit_NeoPixel.h>
Adafruit_NeoPixel strip = Adafruit_NeoPixel(LED_NUM, LED_STRIP_PIN, NEO_GRB + NEO_KHZ800);

#include <TinyGsmClient.h>
#include <PubSubClient.h>

const char ssid[]  = "ssid";
const char pass[] = "password";

//// Use Hardware Serial on Mega, Leonardo, Micro
#define SerialAT Serial1

TinyGsm modem(SerialAT);
TinyGsmClient client(modem);
PubSubClient mqtt(client);

//setup the broker
const char* broker = "test.mosquitto.org";

const char* mqttDeviceId = "yourDeviceId";
const char* mqttUsername = "";
const char* mqttPass = "";

//setup the topics
const char* topicTurnOn = "humidifier/on";
const char* topicTurnOff =  "humidifier/off";
const char* topicReportState =  "humidifier/reportState";
const char* topicStateReport = "humidifier/stateReport";

const char* topicReportColorState =  "humidifier/reportColorState";
const char* topicSetColor =  "humidifier/color";
const char* topicColorStateReport = "humidifier/colorStateReport";

long lastReconnectAttempt = 0;

//set the color of the strip or any ws2812 nodule
void setStripColor(uint32_t c) {
  // Fill the dots one after the other with a color
  for (uint16_t i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, c);
  }
  strip.show();
}

// turn on or turn off humidifier
void ToggleHumidifier() {
  digitalWrite(TOGGLE_PIN, LOW);
  delay(100);
  digitalWrite(TOGGLE_PIN, HIGH);
}


void setup() {
  pinMode(TOGGLE_PIN, OUTPUT);
  digitalWrite(TOGGLE_PIN, HIGH);

  pinMode(ESP_ENABLE_PIN, OUTPUT);
  digitalWrite(ESP_ENABLE_PIN, HIGH);
  
  pinMode(STATE_PIN, INPUT);

  // Set console baud rate
  Serial.begin(115200);
  delay(100);

  // Set GSM module baud rate
  SerialAT.begin(115200);
  delay(100);

  modem.restart();

  Serial.print("Connecting to ");
  Serial.print(ssid);
  if (!modem.networkConnect(ssid, pass)) {
    Serial.println(" fail");
    SWRST_STD();
  }
  Serial.println(" OK");

  // MQTT Broker setup
  mqtt.setServer(broker, 1883);
  mqtt.setCallback(mqttCallback);

  strip.begin();
  strip.show(); // Initialize all pixels to 'off'

  setStripColor(strip.Color(0, 0, 0));
}


//connect to broker
boolean mqttConnect() {
  Serial.print("Connecting to ");
  Serial.print(broker);
  if (strlen(mqttUsername) > 0 ) {
    if (!mqtt.connect(mqttDeviceId, mqttUsername, mqttPass )) {
      Serial.println(" fail");
      return false;
    }
  } else {
    if (!mqtt.connect(mqttDeviceId)) {
      Serial.println(" fail");
      return false;
    }
  }
  Serial.println(" OK");

  //subscribing to topics
  mqtt.subscribe(topicTurnOn);
  mqtt.subscribe(topicTurnOff);
  mqtt.subscribe(topicSetColor);
  mqtt.subscribe(topicReportState);
  mqtt.subscribe(topicReportColorState);
  return mqtt.connected();
}

//main loop
void loop() {
  if (mqtt.connected()) {
    //handle mqtt connection
    mqtt.loop();
  } else {
    // Reconnect every 10 seconds
    unsigned long t = millis();
    if (t - lastReconnectAttempt > 10000L) {
      lastReconnectAttempt = t;
      if (mqttConnect()) {
        lastReconnectAttempt = 0;
      }
    }
  }

}


//handle mqtt messages
void mqttCallback(char* topic, byte * payload, unsigned int len) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("]: ");
  Serial.write(payload, len);
  Serial.println();

  // Only proceed if incoming message's topic matches
  if (String(topic) == topicTurnOn) {
    if (!digitalRead(STATE_PIN)) {
      Serial.println("turning on");
      ToggleHumidifier();
      delay(100);
    }
    mqtt.publish(topicStateReport, digitalRead(STATE_PIN) ? "1" : "0");
  } else if (String(topic) == topicTurnOff) {
    if (digitalRead(STATE_PIN)) {
      Serial.println("turning off");
      ToggleHumidifier();
      delay(100);
    }
    mqtt.publish(topicStateReport, digitalRead(STATE_PIN) ? "1" : "0");
  } else if (String(topic) == topicSetColor) {
    if (len = 3) {
      uint8_t r = payload[0];
      uint8_t g = payload[1];
      uint8_t b  = payload[2];
      Serial.print("red ");
      Serial.println(r);
      Serial.print("green ");
      Serial.println(g);
      Serial.print("blue ");
      Serial.println(b);
      color = strip.Color(r, g, b);
      setStripColor(color);
    }    
    mqtt.publish(topicColorStateReport, String(color).c_str());
  } else if (String(topic) == topicReportColorState) {
    mqtt.publish(topicColorStateReport, String(color).c_str());
  }   else if (String(topic) == topicReportState) {
    mqtt.publish(topicStateReport, digitalRead(STATE_PIN) ? "1" : "0");
  }
}

