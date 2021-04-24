/*******************************************************************
 Daten per MQTT empfangen und über eine 64x32 LED Matrix ausgeben. 
 Ansprechen der LED Matrix umgesetzt von Brian Laugh
 Gesamter Sketch umgesetzt von https://zukunftathome.de
 *******************************************************************/

#include <Ticker.h>
#include <PxMatrix.h>
// The library for controlling the LED Matrix
// Needs to be manually downloaded and installed
// https://github.com/2dom/PxMatrix
#include <ESP8266WiFi.h>
// increase MQTT message size
#define MQTT_MAX_PACKET_SIZE 512
#include <PubSubClient.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>
#include <Fonts/FreeSans9pt7b.h>

#include "config.h"

String mqtt_base_path = HOSTNAME;
String mqtt_topic_cmd = mqtt_base_path + "/cmd";
String mqtt_topic_state = mqtt_base_path + "/state";

int pos_x = 0;
int pos_y = 0;
int pos_x2 = 0;
int pos_y2 = 0;
int textSize = 1;
uint8_t brightness = 40;
float value = 0;

WiFiClient espClient;
PubSubClient client(espClient);

void setup_wifi() {
  delay(100);

  Serial.print("Connecting to ");
  Serial.println(WLAN_SSID);
  WiFi.begin(WLAN_SSID, WLAN_KEY);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  randomSeed(micros());
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

Ticker display_ticker;

// Pins for LED MATRIX
#define P_LAT 16
#define P_A 5
#define P_B 4
#define P_C 15
#define P_OE 2
#define P_D 12
#define P_E 0
// Pin for button (P_E not used)
#define P_BTN 0

// PxMATRIX display(32,16,P_LAT, P_OE,P_A,P_B,P_C);
PxMATRIX display(64, 32, P_LAT, P_OE, P_A, P_B, P_C, P_D);
//PxMATRIX display(64, 32, P_LAT, P_OE, P_A, P_B, P_C, P_D, P_E);

// Some standard colors
uint16_t myRED = display.color565(255, 0, 0);
uint16_t myGREEN = display.color565(0, 255, 0);
uint16_t myBLUE = display.color565(0, 0, 255);
uint16_t myLIGHTBLUE = display.color565(0, 153, 255);
uint16_t myWHITE = display.color565(255, 255, 255);
uint16_t myYELLOW = display.color565(255, 255, 0);
uint16_t myCYAN = display.color565(0, 255, 255);
uint16_t myMAGENTA = display.color565(255, 0, 255);
uint16_t myORANGE = display.color565(255, 127, 127);
uint16_t myBLACK = display.color565(0, 0, 0);
uint16_t currentCOLOR = myWHITE;
uint16_t valueCOLOR_Temp = myWHITE;
uint16_t valueCOLOR_Hum = myWHITE;
uint16_t valueCOLOR_CO = myWHITE;

uint16 myCOLORS[9] = {myRED, myGREEN, myBLUE, myWHITE, myYELLOW, myCYAN, myMAGENTA, myORANGE, myBLACK};
String colorNames[9] = { "red", "green", "blue", "white", "yellow", "cyan", "magenta", "orange", "black"};

uint16 get_color(String name) {
  uint16_t color = 255;
  for (uint16_t i = 0; i < 9; i++) {
    if ((colorNames[i] == name) || (colorNames[i].substring(0,1) == name)) {
      color = i;
      break;
    }
  }
  if (color == 255) {
    unsigned int r, g, b;
    sscanf(name.c_str(), "#%02X%02X%02X", &r, &g, &b);
    color = display.color565(r, g, b);
  } else {
    color = myCOLORS[color];
  }
  return color;
}

// ISR for display refresh
void display_updater() {
  display.display(20);
}

//MQTT DATEN IN VARIABLEN SCHREIBEN
void callback(char* topic, byte* payload, unsigned int length) {
  StaticJsonDocument<512> doc;
  deserializeJson(doc, payload, length);

  Serial.printf("doc size = %d, len = %d\n", doc.size(), length);    

  // Json Object processing
  for (int i = 0; i < 5; i++) {
    String Command = doc["cmd"][i];
    if (Command == "null") {
      break;
    }
    Serial.printf("cmd = %s\n", Command.c_str());    
    String doc_x = doc["x"][i];
    pos_x = 0;
    if (doc_x != "null") {
      pos_x = doc_x.toInt();
    }
    pos_y = 0;
    String doc_y = doc["y"][i];
    if (doc_y != "null") {
      pos_y = doc_y.toInt();
    }
    String doc_x2 = doc["x2"][i];
    if (doc_x2 != "null") {
      pos_x2 = doc_x2.toInt();
    }
    String doc_y2 = doc["y2"][i];
    if (doc_y2 != "null") {
      pos_y2 = doc_y2.toInt();
    }
    String Color = doc["col"][i];
    if(Color != "null") {
      Serial.printf("color = %s\n", Color.c_str());
      currentCOLOR = get_color(Color);
    }
    String doc_textSize = doc["size"][i];
    if (doc_textSize != "null") {
      textSize = doc_textSize.toInt();
      Serial.printf("size = %d\n", textSize);
    }
    String Text = doc["txt"][i];
    String Value = doc["val"][i];
    int value = 0;
    if (Value != "null") {
      value = Value.toInt();
    }

    if(Command == "clr") {
      display.fillRect(0, 0, 64, 32, myBLACK);
    } else if(Command == "brt") {
      Serial.printf("brightness = %d\n", value);
      display.setBrightness(value);
    }
    if(Command == "pnt") {
      Serial.printf("pnt(%d,%d)\n", pos_x, pos_y);    
      display.drawPixel(pos_x, pos_y, currentCOLOR);
    } else if (Command == "fil") {
      Serial.printf("fil(%d,%d,%d,%d)\n", pos_x, pos_y, pos_x2 - pos_x, pos_y2 - pos_y);
      display.fillRect(pos_x, pos_y, pos_x2 - pos_x, pos_y2 - pos_y, currentCOLOR);
    } else if (Command == "gau") {
      Serial.printf("gauge(%d,%d,%d,%d)\n", pos_x, pos_y, pos_x2, pos_y2);
      display.fillRect(pos_x, pos_y, pos_x2 - pos_x + 1, pos_y2 - pos_y + 1, myBLACK);
      display.fillRect(pos_x, pos_y, value, pos_y2 - pos_y + 1, currentCOLOR);
    } else if (Command == "line") {
      Serial.printf("lin(%d,%d,%d,%d)\n", pos_x, pos_y, pos_x2, pos_y2);
      display.drawLine(pos_x, pos_y, pos_x2, pos_y2, currentCOLOR);
    } else if (Command == "rct") {
      Serial.printf("rct(%d,%d,%d,%d)\n", pos_x, pos_y, pos_x2 - pos_x, pos_y2 - pos_y);
      display.drawRect(pos_x, pos_y, pos_x2 - pos_x, pos_y2 - pos_y, currentCOLOR);
    } else if (Command == "txt") {
      Serial.printf("txt(%d,%d,%s)\n", pos_x, pos_y, Text.c_str());
      display.setCursor(pos_x, pos_y);
      display.setTextColor(currentCOLOR, myBLACK);
      if (textSize == 2) {
        display.fillRect(pos_x, 0, 60 - pos_x, pos_y + 7, myBLACK);
        display.setFont(&FreeSans9pt7b);
      } else {
        display.setFont();
      }
      display.print(Text);
    }
  }
}

void reconnect() {  
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    display.clearDisplay();
    display.drawPixel(0, 0, myRED);
    
    int path_len = mqtt_topic_state.length() + 1;
    char mqtt_pub_chr[path_len];
    String clientId = HOSTNAME;
    clientId += String(random(0xffff), HEX);

    if (client.connect(clientId.c_str(), MQTT_USER, MQTT_PASSWORD)) {
      Serial.println("MQTT connected");
      display.clearDisplay();
      display.drawPixel(0, 0, myGREEN);
      client.subscribe(mqtt_topic_cmd.c_str());

      path_len = mqtt_topic_state.length() + 1;
      mqtt_topic_state.toCharArray(mqtt_pub_chr, path_len);
      client.publish(mqtt_pub_chr,"ready");
      client.setCallback(callback);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(6000);
    }
  }
} 

void setup() {
  Serial.begin(9600);
  
  setup_wifi();
  client.setServer(MQTT_HOST, MQTT_PORT);
  
  display.begin(16);
  display.clearDisplay();
  display.setRotate(DISPLAY_ROTATE);

  Serial.print("Pixel draw latency in us: ");
  unsigned long start_timer = micros();
  display.drawPixel(1, 1, 0);
  unsigned long delta_timer = micros() - start_timer;
  Serial.println(delta_timer);

  Serial.print("Display update latency in us: ");
  start_timer = micros();
  display.display(0);
  delta_timer = micros() - start_timer;
  Serial.println(delta_timer);

  display_ticker.attach(0.002, display_updater);
  yield();
  display.clearDisplay();
  delay(500);
  display.setBrightness(brightness);
  display.setTextSize(1);
  //display.setFont(&FreeSans9pt7b);
  //OTA Programmierung, damit der NodeMCU auch per WLAN programmiert werden kann
  ArduinoOTA.setHostname(HOSTNAME);
  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  ArduinoOTA.handle();
  client.loop();
}
