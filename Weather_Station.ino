#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <Ticker.h>
#include "DHT.h"
#include <DS3231.h>
#include <Wire.h>
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"
#include "bitmapsLarge.h"

// DS3231 Clock
DS3231 Clock;
bool Century = false;
bool h12;
bool PM;
int rawDow;
String dow;
int hourNow;
String ampm;

// LCD
Ticker ticker;
#define TFT_CS        15  // CS - D8 - LCD CS Pin 7
#define TFT_RST        -1 // Or set to D3 and connect to NodeMCU RESET pin - NodeMCU Reset Pin
#define TFT_DC         12 // RS / MISO - D6 - LCD RS Pin 6

#define TFT_SCLK 14   // Clock - D5 - LCD SCK Pin 3
#define TFT_MOSI 13   // MOSI - D7 - LCD SDA Pin 4

//Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS,  TFT_DC, TFT_RST);
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);
int hg = 160, wd = 128, row, col, buffidx = 0;

// DHT
#define DHTPIN 0    // what pin we're connected to - D3 In NodeMCU
#define DHTTYPE DHT11   // DHT 11  (AM2302)
DHT dht(DHTPIN, DHTTYPE);

float c, f, h;
String dhtData;
boolean sensorError = false;

/* Adafruit Setup */
#define AIO_SERVER      "io.adafruit.com"
#define AIO_SERVERPORT  1883                   // use 8883 for SSL
#define AIO_USERNAME    "username"
#define AIO_KEY         "adafruitApiKey"

WiFiClient client;
Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);
Adafruit_MQTT_Publish temperature = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/temperature");
Adafruit_MQTT_Publish humidity = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/humidity");
void MQTT_connect();

int button = 2;    // D4
int switchState = 0; // actual read value from pin4
int oldSwitchState = 0; // last read value from pin4
bool showMainDisplay = true;
bool isBgPainted;

ESP8266WebServer server(80);

void handleRoot() {
  // Sending sample message if you try to open configured IP Address
  server.send(200, "text/html", "<h1>You are connected</h1>");
}

void setup(void) {
  Serial.begin(9600);
  Wire.begin();
  pinMode(button, INPUT);

  // LCD
  tft.initR(INITR_BLACKTAB);
  tft.fillScreen(ST7735_BLACK);
  tft.setTextColor(ST7735_WHITE);
  tft.setTextSize(1);
  tft.setCursor(25, 20);
  tft.print("Weather Station");
  tft.setCursor(50, 60);
  tft.print("Open");
  tft.setCursor(10, 100);
  tft.print("192.168.4.1");
  tft.setCursor(20, 130);
  tft.print("Connect To WiFi");
  pinMode(BUILTIN_LED, OUTPUT);
  ticker.attach(0.6, tick);
  WiFiManager wifiManager;
  // wifiManager.resetSettings();
  wifiManager.setAPCallback(configModeCallback);
  if (!wifiManager.autoConnect("Weather Station")) {
    Serial.println("failed to connect and hit timeout");
    ESP.reset();
    delay(1000);
  }
  Serial.println("connected...yeey :)");
  Serial.println(WiFi.localIP());
  ticker.detach();
  digitalWrite(BUILTIN_LED, LOW);
  tft.setTextColor(ST7735_BLACK);
  delay(1000);

  // DHT
  server.on("/", handleRoot);
  server.on("/dht", sendDhtData);
  server.begin();
  Serial.println("HTTP server started");
  Serial.println("DHT11 Sensor");
  dht.begin();
  tft.fillScreen(ST7735_BLACK);
  tft.setTextSize(2);
  showBgImg();
}

void loop() {
  // Time
  rawDow = (Clock.getDoW());
  Serial.println(rawDow);
  switch (rawDow)
  {
    case 1:
      dow = "Monday";
      break;
    case 2:
      dow = "Tuesday";
      break;
    case 3:
      dow = "Wednesday";
      break;
    case 4:
      dow = "Thursday";
      break;
    case 5:
      dow = "Friday";
      break;
    case 6:
      dow = "Saturday";
      break;
    case 7:
      dow = "Sunday";
      break;
    default:
      dow = "Wrong Date";
  }

  Serial.print(Clock.getDate(), DEC);
  Serial.print('/');
  Serial.print(Clock.getMonth(Century), DEC);
  Serial.print('/');
  Serial.print(Clock.getYear(), DEC);
  Serial.print(' ');
  if (Clock.getHour(h12, PM) > 12 )
  {
    hourNow = Clock.getHour(h12, PM) - 12;
    ampm = "PM";
  }
  else
  {
    hourNow = Clock.getHour(h12, PM);
    if (hourNow == 0) {
      hourNow = 12;
    }
    ampm = "AM";
  }
  Serial.print(hourNow);
  Serial.print(':');
  Serial.print(Clock.getMinute(), DEC);
  Serial.print(':');
  Serial.print(Clock.getSecond(), DEC);
  Serial.print(" ");
  Serial.print(ampm);
  Serial.println();

  // DHT
  server.handleClient();
  c = dht.readTemperature();
  f = dht.readTemperature(true);
  h = dht.readHumidity();
  // check if returns are valid, if they are NaN (not a number) then something went wrong!
  if (isnan(c) || isnan(h) || isnan(f)) {
    Serial.println("Sensor Not Connected");
    sensorError = true;
  } else {
    Serial.println("Temperature In Celsuis: ");
    Serial.print(c);
    Serial.print(" C");
    Serial.println("Temperature In Fahrenheit: ");
    Serial.println(f);
    Serial.println(" *F");
    Serial.println("Humidity: ");
    Serial.println(h);
    Serial.println(" %");
    digitalWrite(LED_BUILTIN, LOW);
    delay(100);
    digitalWrite(LED_BUILTIN, HIGH);
  }
  // If there is any issue in sensor connections, it will send 000 as String.
  if (sensorError) {
    dhtData = "sensorError";
  }
  else {
    dhtData = String(c) + ' ' + String(f) + ' ' + String(h);
  }
  
  switchState = digitalRead(button); // read the pushButton State
  if (switchState != oldSwitchState) // catch change
  {
    oldSwitchState = switchState;
    if (switchState == HIGH)
    {
      showMainDisplay = !showMainDisplay;
    }
  }
  if (showMainDisplay)
  {
    if(!isBgPainted) {
      tft.fillScreen(ST7735_WHITE);
      tft.setTextSize(2);
      showBgImg();
    }
    else {
      isBgPainted=true;
      showDataToDisplay();
    }
  } else {
    showIpAddress();
  }
  
  // MQTT
  MQTT_connect();
  Serial.print(F("\nSending Data... "));
  if (!temperature.publish(c)) {
    Serial.println(F("Failed"));
  } else {
    Serial.println(F("Sent"));
  }
  if (!humidity.publish(h)) {
    Serial.println(F("Failed"));
  } else {
    Serial.println(F("Sent"));
  }

  delay(5000);
}

void showBgImg() {
  isBgPainted = true;
  ESP.wdtDisable();
  for (row = 0; row < hg; row++) { // For each scanline...
    for (col = 0; col < wd; col++) { // For each pixel...
      tft.drawPixel(col, row, pgm_read_word(evive_in_hand + buffidx));
      buffidx++;
    } // end pixel
  }
  ESP.wdtEnable(10);
  delay(200);
}

void showIpAddress() {
  isBgPainted = false;
  tft.fillScreen(ST7735_BLACK);
  tft.setTextColor(ST7735_WHITE);
  tft.setTextSize(1);
  tft.setCursor(10, 70);
  tft.print(WiFi.localIP());
}

void showDataToDisplay() {
  tft.setTextSize(2);
  tft.setTextColor(ST7735_BLACK, ST7735_WHITE);
  tft.setCursor(28, 25);
  tft.print(dow);
  tft.setCursor(28, 52);
  if (Clock.getDate() < 10)
    tft.print("0");
  tft.print(Clock.getDate(), DEC);
  tft.print('/');
  if (Clock.getMonth(Century) < 10)
    tft.print("0");
  tft.print(Clock.getMonth(Century), DEC);
  tft.print('/');
  tft.print(Clock.getYear(), DEC);
  tft.setCursor(28, 80);
  if (hourNow < 10)
    tft.print("0");
  tft.print(hourNow);
  tft.print(':');
  if (Clock.getMinute() < 10)
    tft.print("0");
  tft.print(Clock.getMinute(), DEC);
  tft.print(" ");
  tft.print(ampm);
  tft.setTextColor(ST7735_BLACK);
  tft.setCursor(28, 109);

  // DHT
  tft.setTextColor(ST7735_BLACK, ST7735_WHITE);
  tft.print(c);
  tft.print(" C");
  tft.setCursor(28, 137);
  tft.print(h);
  tft.print(" %");
  tft.setTextColor(ST7735_BLACK);
}

void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
  Serial.println(myWiFiManager->getConfigPortalSSID());
  ticker.attach(0.2, tick);
}

void tick()
{
  int state = digitalRead(BUILTIN_LED);
  digitalWrite(BUILTIN_LED, !state);
}

void sendDhtData() {
  server.send(200, "text/plain", dhtData);
}

void MQTT_connect() {
  int8_t ret;
  if (mqtt.connected()) {
    return;
  }
  Serial.print("Connecting to MQTT... ");
  uint8_t retries = 3;
  while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
    Serial.println(mqtt.connectErrorString(ret));
    Serial.println("Retrying MQTT connection in 5 seconds...");
    mqtt.disconnect();
    delay(5000);  // wait 5 seconds
    retries--;
    if (retries == 0) {
      while (1);
    }
  }
  Serial.println("MQTT Connected!");
}
