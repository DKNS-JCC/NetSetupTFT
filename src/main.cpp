#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ESPmDNS.h>
#include <DNSServer.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>

String generateHTML();

// Define the pins used for the display
const int TFT_CS = 14;
const int TFT_RST = 4;
const int TFT_DC = 2;
const int TFT_LED = 27;

String ssids[50];
String selectedSSID = "";
String wifiPassword = "";

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);
AsyncWebServer server(80);
DNSServer dnsServer;
IPAddress apIP(192, 168, 4, 1); // IP del ESP32 en modo AP

class CaptivePortalHandler : public AsyncWebHandler {
public:
  CaptivePortalHandler() {}
  virtual ~CaptivePortalHandler() {}

  bool canHandle(AsyncWebServerRequest *request){
    return request->url() == "/";
  }

  void handleRequest(AsyncWebServerRequest *request) {
    if (request->method() == HTTP_GET && request->url() == "/") {
      request->send(200, "text/html", generateHTML());
    } else {
      request->send(200, "text/html", generateHTML());
    }
  }
};

void setup()
{
  Serial.begin(115200);
  Serial.println("Starting...");
  pinMode(TFT_LED, OUTPUT);
  digitalWrite(TFT_LED, HIGH);
  tft.init(240, 320);
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(1);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  Serial.println("Scan start");
  tft.setCursor(0, 0);
  tft.println("Scan start");

  int n = WiFi.scanNetworks();
  
  for (int i = 0; i < n; ++i) {
    ssids[i] = WiFi.SSID(i);
  }
  Serial.println("Scan done");
  tft.fillScreen(ST77XX_BLACK);
  tft.setCursor(0, 0);
  if (n == 0)
  {
    Serial.println("no networks found");
    tft.setCursor(0, 3);
    tft.println("no networks found");
  }
  else
  {
    Serial.print(n);
    tft.setCursor(0, 3);
    tft.print(n);
    tft.println(" networks found");
    Serial.println(" networks found");
    Serial.println("Nr | SSID                             | RSSI | CH | Encryption");
    tft.println("Nr | SSID | RSSI |");
    for (int i = 0; i < n; ++i)
    {
      Serial.printf("%2d", i + 1);
      tft.printf("%2d", i + 1);
      Serial.print(" | ");
      tft.print(" | ");
      Serial.printf("%-32.32s", WiFi.SSID(i).c_str());
      tft.printf("%-15.15s", WiFi.SSID(i).c_str());
      Serial.print(" | ");
      tft.print(" | ");
      Serial.printf("%4d", WiFi.RSSI(i));
      tft.printf("%4d", WiFi.RSSI(i));
      Serial.print(" | ");
      tft.print(" | ");
      Serial.printf("%2d", WiFi.channel(i));
      Serial.print(" | ");
      switch (WiFi.encryptionType(i))
      {
      case WIFI_AUTH_OPEN:
        Serial.print("open");
        break;
      case WIFI_AUTH_WEP:
        Serial.print("WEP");
        break;
      case WIFI_AUTH_WPA_PSK:
        Serial.print("WPA");
        break;
      case WIFI_AUTH_WPA2_PSK:
        Serial.print("WPA2");
        break;
      case WIFI_AUTH_WPA_WPA2_PSK:
        Serial.print("WPA+WPA2");
        break;
      case WIFI_AUTH_WPA2_ENTERPRISE:
        Serial.print("WPA2-EAP");
        break;
      case WIFI_AUTH_WPA3_PSK:
        Serial.print("WPA3");
        break;
      case WIFI_AUTH_WPA2_WPA3_PSK:
        Serial.print("WPA2+WPA3");
        break;
      case WIFI_AUTH_WAPI_PSK:
        Serial.print("WAPI");
        break;
      default:
        Serial.print("unknown");
      }
      Serial.println();
      tft.println();
      delay(10);
    }
  }
  Serial.println("");
  tft.println("");
  WiFi.scanDelete();
  tft.println("Scan done");
  
  // Configurar el AP (punto de acceso)
  WiFi.mode(WIFI_AP);
  WiFi.softAP("NetSetup");

  dnsServer.start(53, "*", apIP);
  server.addHandler(new CaptivePortalHandler()).setFilter(ON_AP_FILTER);

  if (!MDNS.begin("esp32")) {
    Serial.println("Error setting up MDNS responder!");
  } else {
    Serial.println("mDNS responder started. You can access ESP32 at http://esp32.local");
    tft.println("Connect to ESP WiFi, then you can access ESP32 at http://esp32.local");
  }

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", generateHTML());
  });

  // Manejar la recepción del formulario con el SSID y la contraseña
  server.on("/submit", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasParam("ssid", true) && request->hasParam("password", true)) {
      selectedSSID = request->getParam("ssid", true)->value();
      wifiPassword = request->getParam("password", true)->value();
      Serial.println("SSID selected: " + selectedSSID);
      Serial.println("Password received: " + wifiPassword);
      String response = "<html><body><h2>WiFi Configuration</h2>";
      response += "<p>SSID: " + selectedSSID + "</p>";
      response += "<p>Password: " + wifiPassword + "</p>";
      response += "</body></html>";
      request->send(200, "text/html", response);
    } else {
      request->send(400, "text/plain", "Missing SSID or password");
    }
  });

  server.onNotFound([](AsyncWebServerRequest *request) {
    request->redirect("/");
  });

  server.begin();
  Serial.println("HTTP server started");
  tft.println("HTTP server started on port 80 ");

  while (selectedSSID == "" || wifiPassword == "") {
    delay(1000);
  }
  
  tft.println("Connecting to " + selectedSSID + "...");
  WiFi.begin(selectedSSID.c_str(), wifiPassword.c_str());
  int retries = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi..");
    retries++;
    if (retries > 10) {
      Serial.println("Connection failed");
      tft.println("Connection failed, reseting in 5 seconds");
      delay(5000);
      ESP.restart();
      break;
    }
  }
  Serial.println("Connected to WiFi");
}

String generateHTML() {
  String html = "<html><body><h2>Select WiFi Network</h2><form action='/submit' method='POST'>";
  html += "<label for='ssid'>Choose an SSID:</label><br><select name='ssid' id='ssid'>";
  
  for (int i = 0; i < sizeof(ssids) / sizeof(ssids[0]); i++) {
    html += "<option value='" + String(ssids[i]) + "'>" + String(ssids[i]) + "</option>";
  }
  
  html += "</select><br><br>";
  html += "<label for='password'>Enter WiFi Password:</label><br><input type='password' name='password' id='password'><br><br>";
  html += "<input type='submit' value='Submit'>";
  html += "</form></body></html>";
  return html;
}

void loop()
{
  dnsServer.processNextRequest();
}
