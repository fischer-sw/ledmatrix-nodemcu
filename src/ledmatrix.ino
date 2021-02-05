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
#include <PubSubClient.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>
#include <Fonts/FreeSans9pt7b.h>

#include "config.h"

String mqtt_base_path = HOSTNAME;
const char* pub_msg = "Get Data";
String mqtt_pub_str = mqtt_base_path + "/Status";
int path_len = 1;

unsigned long previousMillis;

String temperatur;
String feuchtigkeit;
String zeit;
String msg_str;

#define MSG_BUFFER_SIZE  (50)
char msg[MSG_BUFFER_SIZE];

int status;
int pos_x = 0;
int pos_y = 0;
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

// PxMATRIX display(32,16,P_LAT, P_OE,P_A,P_B,P_C);
 PxMATRIX display(64,32,P_LAT, P_OE,P_A,P_B,P_C,P_D);
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
uint16_t myBLACK = display.color565(0, 0, 0);
uint16_t currentCOLOR = myWHITE;
uint16_t valueCOLOR_Temp = myWHITE;
uint16_t valueCOLOR_Hum = myWHITE;
uint16_t valueCOLOR_CO = myWHITE;

uint16 myCOLORS[8] = {myRED, myGREEN, myBLUE, myWHITE, myYELLOW, myCYAN, myMAGENTA, myBLACK};
String colorNames[8] = { "red", "green", "blue", "white", "yellow", "cyan", "magenta", "black"};

uint16 get_color(String name) {
  uint16_t color = myWHITE;
  for (int i = 0; i < 8; i++) {
    if (colorNames[i] == name) {
      color = myCOLORS[i];
      break;
    }
  }
  return color;
}

// ISR for display refresh
void display_updater() {
  display.display(20);
}

//MQTT DATEN IN VARIABLEN SCHREIBEN
void callback(char* topic, byte* payload, unsigned int length) {
  StaticJsonDocument<256> doc;
  deserializeJson(doc, payload, length);

  // Json Object processing
  String doc_x = doc["x"];
  pos_x = doc_x.toInt();
  String doc_y = doc["y"];
  pos_y = doc_y.toInt();
  String Color = doc["color"];
  String Text = doc["text"];
  int textSize = doc["textSize"];
  String br = doc["brightness"];
  brightness = br.toInt();
  String Command = doc["command"];

  if(Command != "null") {
    Serial.printf("command = %s\n", Command.c_str());    
    if(Command == "clear") {
      status = 0;
      Serial.print("Status = 0");
      display.fillRect(0, 0, 64, 32, myBLACK);
    }
    Command = "";
    send_message();
  }

  if(Color != "null") {
    Serial.printf("color = %s\n", Color.c_str());
    currentCOLOR = get_color(Color);
  }

  if(Text == "null") {
    display.drawPixel(pos_x, pos_y, currentCOLOR);
  } else {
    Serial.printf("text = %s\n", Text.c_str());
    Serial.printf("X = %d\n", pos_x);
    Serial.printf("Y = %d\n", pos_y);
    display.setCursor(pos_x, pos_y);
    display.setTextColor(currentCOLOR, myBLACK);
    if (textSize == 2){
      display.fillRect(pos_x, 0, 60 - pos_x, pos_y + 7, myBLACK);
      display.setFont(&FreeSans9pt7b);
    } else {
      display.setFont();
    }
    display.print(Text);
    // scroll_text(1,100,Text,96,96,250);
    Text = "";
  }

  if(brightness > 0) {
    Serial.printf("Helligkeit = %d\n", brightness);
    display.setBrightness(brightness);
    brightness = 0;
  }
}

void scroll_text(uint8_t ypos, unsigned long scroll_delay, String text, uint8_t colorR, uint8_t colorG, uint8_t colorB) {
    uint16_t text_length = text.length();
    int matrix_width = 64;
    display.setTextWrap(false);  // we don't wrap text so it scrolls nicely
    display.setTextColor(display.color565(colorR,colorG,colorB));

    // Asuming 5 pixel average character width
    for (int xpos=matrix_width; xpos>-(matrix_width+text_length*5); xpos--) {
      display.setTextColor(display.color565(colorR,colorG,colorB));
      display.clearDisplay();
      display.setCursor(xpos,ypos);
      display.println(text);
      delay(scroll_delay);
      yield();

      // This might smooth the transition a bit if we go slow
      // display.setTextColor(display.color565(colorR/4,colorG/4,colorB/4));
      // display.setCursor(xpos-1,ypos);
      // display.println(text);

      delay(scroll_delay/5);
      yield();
    }
}

void reconnect() {  
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    display.clearDisplay();
    display.drawPixel(0, 0, myRED);
    
    int path_len = mqtt_pub_str.length() + 1;
    char mqtt_pub_chr[path_len];
    String clientId = HOSTNAME;
    clientId += String(random(0xffff), HEX);

    if (client.connect(clientId.c_str(), MQTT_USER, MQTT_PASSWORD)) {
      Serial.println("MQTT connected");
      display.clearDisplay();
      display.drawPixel(0, 0, myGREEN);
      client.subscribe((mqtt_base_path + "/Test").c_str());

      path_len = mqtt_pub_str.length() + 1;
      mqtt_pub_str.toCharArray(mqtt_pub_chr, path_len);
      client.publish(mqtt_pub_chr,"Get Data");
      client.setCallback(callback);
      //VARIABLE status BEI START AUF 1, DAMIT DIREKT DIE DATEN ANGEZEIGT WERDEN
      status = 1;
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
  display.setRotate(true);

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

void send_message(){
  int path_len = mqtt_pub_str.length() + 1;
  char mqtt_pub_chr[path_len];

  path_len = mqtt_pub_str.length() + 1;
  mqtt_pub_str.toCharArray(mqtt_pub_chr, path_len);
  client.publish(mqtt_pub_chr,"Get Data");
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  ArduinoOTA.handle();
  client.loop();
}
