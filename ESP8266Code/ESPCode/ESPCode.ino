// ══════════════════════════════════════════
// ESP8266 NodeMCU — Greenova
// ══════════════════════════════════════════

#define BLYNK_TEMPLATE_ID   "TMPL6l9AZ94ca"
#define BLYNK_TEMPLATE_NAME "Smart Plant"
#define BLYNK_AUTH_TOKEN    "HyWjEw0cETDV9RYnLRfLMYo8OismjTY6"
#define BLYNK_PRINT Serial

#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <DHT.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SoftwareSerial.h>

char ssid[] = "Ishmam";
char pass[] = "hellaDOPE2809";

#define DHTPIN      D7
#define DHTTYPE     DHT22
#define RELAY_PIN   D3
#define TRIG_PIN    D5
#define ECHO_PIN    D6
#define UNO_RX      D10
#define UNO_TX      D9

#define SCREEN_W 128
#define SCREEN_H  64
Adafruit_SSD1306 display(SCREEN_W, SCREEN_H, &Wire, -1);

DHT dht(DHTPIN, DHTTYPE);
SoftwareSerial unoSerial(UNO_RX, UNO_TX);
BlynkTimer timer;

bool wifiConnected     = false;
bool moistureAlertSent = false;
bool tempAlertSent     = false;
bool waterAlertSent    = false;
bool humidityAlertSent = false;

float temperature  = 0.0;
float humidity     = 0.0;
int   soilMoisture = 50;
int   waterLevel   = 100;

BLYNK_WRITE(V4) {
  int val = param.asInt();
  if (val == 1) {
    digitalWrite(RELAY_PIN, LOW);
    Serial.println("Pump: ON");
  } else {
    digitalWrite(RELAY_PIN, HIGH);
    Serial.println("Pump: OFF");
  }
}

int getWaterLevel() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long dur = pulseIn(ECHO_PIN, HIGH, 30000);
  if (dur == 0) return waterLevel;
  float distCm = dur * 0.034 / 2.0;
  int level = map((int)(distCm * 10), 30, 145, 100, 2);
  return constrain(level, 0, 100);
}

int readSoilFromUNO() {
  if (unoSerial.available() > 0) {
    int val = unoSerial.parseInt();
    if (val >= 0 && val <= 100) return val;
  }
  return soilMoisture;
}

void updateOLED() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);
  display.print("Temp:  ");
  display.print(temperature, 1);
  display.println(" C");

  display.setCursor(0, 12);
  display.print("Humid: ");
  display.print(humidity, 1);
  display.println(" %");

  display.setCursor(0, 24);
  display.print("Soil:  ");
  display.print(soilMoisture);
  display.println(" %");

  display.setCursor(0, 36);
  display.print("Water: ");
  display.print(waterLevel);
  display.println(" %");

  display.setCursor(0, 48);
  display.println("----------------");

  display.setCursor(0, 56);
  if (soilMoisture < 30 && waterLevel < 20) {
    display.println("THIRSTY+LOW TANK!");
  } else if (soilMoisture < 30) {
    display.println("Plant is thirsty!");
  } else if (temperature > 35.0) {
    display.println("Too hot! Move me!");
  } else if (waterLevel < 20) {
    display.println("Refill water tank");
  } else if (humidity < 30.0) {
    display.println("Air is too dry!");
  } else {
    display.println("Plant is happy :)");
  }

  display.display();
}

void sendToBlynk() {
  if (!wifiConnected) return;
  Blynk.virtualWrite(V0, temperature);
  Blynk.virtualWrite(V1, humidity);
  Blynk.virtualWrite(V2, soilMoisture);
  Blynk.virtualWrite(V3, waterLevel);
}

void checkAlerts() {
  if (!wifiConnected) return;

  if (soilMoisture < 30 && !moistureAlertSent) {
    Blynk.logEvent("low_moisture", "Plant needs water! Soil is dry.");
    moistureAlertSent = true;
  }
  if (soilMoisture >= 35) moistureAlertSent = false;

  if (temperature > 35.0 && !tempAlertSent) {
    Blynk.logEvent("high_temp", "Too hot! Move the plant.");
    tempAlertSent = true;
  }
  if (temperature <= 33.0) tempAlertSent = false;

  if (waterLevel < 20 && !waterAlertSent) {
    Blynk.logEvent("low_water", "Water tank almost empty! Please refill.");
    waterAlertSent = true;
  }
  if (waterLevel >= 25) waterAlertSent = false;

  if (humidity < 30.0 && !humidityAlertSent) {
    Blynk.logEvent("low_humidity", "Air is too dry around the plant!");
    humidityAlertSent = true;
  }
  if (humidity >= 35.0) humidityAlertSent = false;
}

void updateSensors() {
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (!isnan(t) && !isnan(h)) {
    temperature = t;
    humidity    = h;
  } else {
    Serial.println("DHT22 read failed");
  }

  waterLevel   = getWaterLevel();
  soilMoisture = readSoilFromUNO();

  updateOLED();
  sendToBlynk();
  checkAlerts();

  Serial.println("--- Update ---");
  Serial.print("Temp: ");   Serial.println(temperature);
  Serial.print("Humid: ");  Serial.println(humidity);
  Serial.print("Soil: ");   Serial.println(soilMoisture);
  Serial.print("Water: ");  Serial.println(waterLevel);
  Serial.print("WiFi: ");   Serial.println(wifiConnected ? "YES" : "NO");
}

void setup() {
  Wire.begin(D2, D1);
  Serial.begin(115200);
  delay(500);
  Serial.println("\nGreenova starting...");

  unoSerial.begin(9600);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  dht.begin();

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("Trying OLED 0x3D...");
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3D)) {
      Serial.println("OLED failed — check SDA D2 and SCL D1");
    } else {
      Serial.println("OLED found at 0x3D");
    }
  } else {
    Serial.println("OLED found at 0x3C");
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(15, 8);
  display.println("Greenova");
  display.setCursor(15, 20);
  display.println("Starting");
  display.setCursor(5, 38);
  display.println("Connecting WiFi...");
  display.display();

  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, pass);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    Serial.println("\nWiFi Connected!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    Blynk.config(BLYNK_AUTH_TOKEN);
    Blynk.connect();
    display.clearDisplay();
    display.setCursor(10, 20);
    display.println("WiFi Connected!");
    display.setCursor(10, 34);
    display.println("Blynk Connected!");
    display.display();
    delay(2000);
  } else {
    wifiConnected = false;
    Serial.println("\nWiFi FAILED!");
    Serial.println("Turn on hotspot first then press RST");
    display.clearDisplay();
    display.setCursor(5, 20);
    display.println("WiFi Failed!");
    display.setCursor(5, 34);
    display.println("Running offline");
    display.display();
    delay(2000);
  }

  timer.setInterval(3000L, updateSensors);
  Serial.println("Setup complete. Reading sensors every 3 seconds...");
}

void loop() {
  if (wifiConnected) {
    Blynk.run();
  }
  timer.run();
}