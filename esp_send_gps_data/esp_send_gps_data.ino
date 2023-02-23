#define TINY_GSM_MODEM_SIM7000
#define TINY_GSM_RX_BUFFER 1024 // Set RX buffer to 1Kb

// LilyGO T-SIM7000G Pinout
#define UART_BAUD   115200
#define PIN_DTR     25
#define PIN_TX      27
#define PIN_RX      26
#define PWR_PIN     4

#define LED_PIN     12

#include <TinyGSM.h>

// Set serial for debug console (to Serial Monitor, default speed 115200)
#define SerialMon Serial
// Set serial for AT commands
#define SerialAT  Serial1

TinyGsm modem(SerialAT);


#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>

// Replace with your network credentials
const char* ssid     = "SSID_NAME";
const char* password = "PASSWORD";

// REPLACE with your Domain name and URL path or IP address with path
const char* serverName = "SERVER_NAME";

// Keep this API Key value to be compatible with the PHP code provided in the server.
String apiKeyValue = "API_KEY_VALUE";


void setup(){
  Serial.begin(115200);
  
  // ---------- GPS Setup ----------

  SerialMon.println("Place your board outside to catch satelite signal");

  // Set LED OFF
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);

  //Turn on the modem
  pinMode(PWR_PIN, OUTPUT);
  digitalWrite(PWR_PIN, HIGH);
  delay(300);
  digitalWrite(PWR_PIN, LOW);

  delay(1000);
  
  // Set module baud rate and UART pins
  SerialAT.begin(UART_BAUD, SERIAL_8N1, PIN_RX, PIN_TX);

  // Restart takes quite some time
  // To skip it, call init() instead of restart()
  SerialMon.println("Initializing modem...");
  if (!modem.restart()) {
    Serial.println("Failed to restart modem, attempting to continue without restarting");
  }
  
  // Print modem info
  String modemName = modem.getModemName();
  delay(500);
  SerialMon.println("Modem Name: " + modemName);

  String modemInfo = modem.getModemInfo();
  delay(500);
  SerialMon.println("Modem Info: " + modemInfo);
}

void loop(){
  // ---------- GPS DATA ----------

  // Set SIM7000G GPIO4 HIGH ,turn on GPS power
  // CMD:AT+SGPIO=0,4,1,1
  // Only in version 20200415 is there a function to control GPS power
  modem.sendAT("+SGPIO=0,4,1,1");
  if (modem.waitResponse(10000L) != 1) {
    SerialMon.println(" SGPIO=0,4,1,1 false ");
  }

  modem.enableGPS();
  
  delay(125);
  float lat      = 0;
  float lng      = 0;
  float speed    = 0;
  float alt      = 0;
  int   vsat     = 0;
  int   usat     = 0;
  float accuracy = 0;
  int   year     = 0;
  int   month    = 0;
  int   day      = 0;
  int   hour     = 0;
  int   min      = 0;
  int   sec      = 0;
  String reading_time = "";
  
  for (int8_t i = 15; i; i--) {
    SerialMon.println("Requesting current GPS/GNSS/GLONASS location");
    if (modem.getGPS(&lat, &lng, &speed, &alt, &vsat, &usat, &accuracy,
                     &year, &month, &day, &hour, &min, &sec)) {
      String reading_time = String(year) + "-" + String(month) + "-" + String(day) + " " + String(hour) + ":" + String(min) + ":" + String(sec);
      SerialMon.println("Latitude: " + String(lat, 8) + "\tLongitude: " + String(lng, 8));
      SerialMon.println("Year: " + String(year) + "\tMonth: " + String(month) + "\tDay: " + String(day));
      SerialMon.println("Hour: " + String(hour) + "\tMinute: " + String(min) + "\tSecond: " + String(sec));
      SerialMon.println("Reading_time: " + reading_time);
      
      break;
    } 
    else {
      SerialMon.println("Couldn't get GPS/GNSS/GLONASS location, retrying in 15s.");
      delay(15000L);
    }
  }

  // ---------- Preparing HTTP request ----------
  
  // Check WiFi connection status
  if(WiFi.status()== WL_CONNECTED){
    WiFiClient client;
    HTTPClient http;
    
    // Domain name with URL path or IP address with path
    http.begin(client, serverName);
    
    // Specify content-type header
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    
    // Preparing HTTP POST request data
    String httpRequestData = "api_key=" + apiKeyValue + "&lat=" + String(lat, 8) + "&lng=" + String(lng, 8) + "";
    Serial.print("httpRequestData: ");
    Serial.println(httpRequestData);

    // Send HTTP POST request
    int httpResponseCode = http.POST(httpRequestData);
    
    if (httpResponseCode>0) {
      Serial.print("HTTP Response code: ");
      Serial.println(httpResponseCode);
    }
    else {
      Serial.print("Error code: ");
      Serial.println(httpResponseCode);
    }
    // Free resources
    http.end();
  }
  else {
    Serial.println("WiFi Disconnected. Attempting to connect again");
    WiFi.begin(ssid, password);
    Serial.println("Connecting");
    while(WiFi.status() != WL_CONNECTED) { 
      delay(500);
      Serial.println("WL_NOT_CONNECTED");
    }
    Serial.println("");
    Serial.print("Connected to WiFi network with IP Address: ");
    Serial.println(WiFi.localIP());;
  }
}
