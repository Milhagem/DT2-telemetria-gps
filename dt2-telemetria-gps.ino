#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

SemaphoreHandle_t bufferSemaphore;
SemaphoreHandle_t displayMutex;       // Lock access to buffer and Serial
//int buffer = 0;

#define TINY_GSM_MODEM_SIM7000
#define TINY_GSM_RX_BUFFER 1024 // Set RX buffer to 1Kb

#include <TinyGSM.h>

// LilyGO T-SIM7000G Pinout
#define UART_BAUD   115200
#define PIN_DTR     25
#define PIN_TX      27
#define PIN_RX      26
#define PWR_PIN     4

#define LED_PIN     12

#include "WiFi.h"
#include "HTTPClient.h"

#include <Wire.h>
#include <Wire.h>

// Replace with your network credentials
const char* ssid     = "Milhagem";
const char* password = "ehomilhas";

// REPLACE with your Domain name and URL path or IP address with path
const char* serverName = "http://milhagemufmg.com/gps/post-data.php";

// Keep this API Key value to be compatible with the PHP code provided in the project page. 
// If you change the apiKeyValue value, the PHP file /post-data.php also needs to have the same key 
String apiKeyValue = "tPmAT5Ab3j7F9";

// Set serial for debug console (to Serial Monitor, default speed 115200)
#define SerialMon Serial
// Set serial for AT commands
#define SerialAT  Serial1

TinyGsm modem(SerialAT);

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
  int   minutos      = 0;
  int   sec      = 0;
  String reading_time = "";



void GPSTask(void *pvParameters) {
  while (true) {
    // Set SIM7000G GPIO4 HIGH ,turn on GPS power
    // CMD:AT+SGPIO=0,4,1,1
    // Only in version 20200415 is there a function to control GPS power
    modem.sendAT("+SGPIO=0,4,1,1");
    if (modem.waitResponse(10000L) != 1) {
        xSemaphoreTake(displayMutex, portMAX_DELAY);
        SerialMon.println(" SGPIO=0,4,1,1 false ");
        xSemaphoreGive(displayMutex);
    }//end_if
    modem.enableGPS();    
    
     // Take the semaphore to access the shared resource
    xSemaphoreTake(bufferSemaphore, portMAX_DELAY);
    xSemaphoreTake(displayMutex, portMAX_DELAY);

    for (int8_t i = 15; i; i--) {
            SerialMon.println("Requesting current GPS/GNSS/GLONASS location");
            if (modem.getGPS(&lat, &lng, &speed, &alt, &vsat, &usat, &accuracy,
                             &year, &month, &day, &hour, &minutos, &sec)) {
              String reading_time = String(year) + "-" + String(month) + "-" + String(day) + " " + String(hour) + ":" + String(minutos) + ":" + String(sec);
              SerialMon.println("Latitude: " + String(lat, 8) + "\tLongitude: " + String(lng, 8));
              SerialMon.println("Year: " + String(year) + "\tMonth: " + String(month) + "\tDay: " + String(day));
              SerialMon.println("Hour: " + String(hour) + "\tMinute: " + String(minutos) + "\tSecond: " + String(sec));
              SerialMon.println("Reading_time: " + reading_time);

              break;
            }//end_if 
            else {
              SerialMon.println("Couldn't get GPS/GNSS/GLONASS location, retrying in 15s.");
              delay(1000);
            }//end_else
    }//end_for
  
    // Release the semaphore
    xSemaphoreGive(displayMutex);
    xSemaphoreGive(bufferSemaphore);

    // Delay for some time
    vTaskDelay(100);
  }//end while
}//end GPS task

void EnvioDeDadosTask(void *pvParameters) {
  while (true) {
          //Check WiFi connection status
          if(WiFi.status()== WL_CONNECTED){
            WiFiClient client;
            HTTPClient http;

            // Your Domain name with URL path or IP address with path
            http.begin(client, serverName);

            // Specify content-type header
            http.addHeader("Content-Type", "application/x-www-form-urlencoded");

            // Prepare your HTTP POST request data
                  
            // Take the semaphore to access the shared resource
            xSemaphoreTake(bufferSemaphore, portMAX_DELAY);
            String httpRequestData = "api_key=" + apiKeyValue + "&lat=" + String(lat, 8) + "&lng=" + String(lng, 8) + "";
            // Release the semaphore
            xSemaphoreGive(bufferSemaphore);
                  
            xSemaphoreTake(displayMutex, portMAX_DELAY);
            Serial.print("httpRequestData: ");
            Serial.println(httpRequestData);
            xSemaphoreGive(displayMutex);
            
            // Send HTTP POST request
            int httpResponseCode = http.POST(httpRequestData);

            if (httpResponseCode>0) {
              xSemaphoreTake(displayMutex, portMAX_DELAY);
              Serial.print("HTTP Response code: ");
              Serial.println(httpResponseCode);
              xSemaphoreGive(displayMutex);
            }//end_if
            else {
              xSemaphoreTake(displayMutex, portMAX_DELAY);
              Serial.print("Error code: ");
              Serial.println(httpResponseCode);
              xSemaphoreGive(displayMutex);
            }//end_else
            // Free resources
            http.end();
          }//end_if
          else {
            xSemaphoreTake(displayMutex, portMAX_DELAY);
            Serial.println("WiFi Disconnected. Attempting to connect again");
            WiFi.begin(ssid, password);
            Serial.println("Connecting");
            xSemaphoreGive(displayMutex);
            while(WiFi.status() != WL_CONNECTED) { 
              delay(500);
              xSemaphoreTake(displayMutex, portMAX_DELAY);
              Serial.println("WL_NOT_CONNECTED");
              xSemaphoreGive(displayMutex);
            }//end_while
            xSemaphoreTake(displayMutex, portMAX_DELAY);
            Serial.println("");
            Serial.print("Connected to WiFi network with IP Address: ");
            Serial.println(WiFi.localIP());;
            xSemaphoreGive(displayMutex);
        }//end_else
    // Delay for some time
    vTaskDelay(100);
  }//end while
}//end EnvioDeDados

void setup() {
  Serial.begin(115200);
  
  // Wait a moment to start (so we don't miss Serial output)
  vTaskDelay(1000 / portTICK_PERIOD_MS);
        
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
  }//end_if
  
  // Print modem info
  String modemName = modem.getModemName();
  delay(500);
  SerialMon.println("Modem Name: " + modemName);

  String modemInfo = modem.getModemInfo();
  delay(500);
  SerialMon.println("Modem Info: " + modemInfo);

  Serial.println("All done with GPS! Starting HTTP config...");
  
  WiFi.begin(ssid, password);
  Serial.println("Connecting");
  while(WiFi.status() != WL_CONNECTED) { 
    delay(500);
    Serial.println("WL_NOT_CONNECTED");
  }//end_while
  Serial.println("");
  Serial.print("Connected to WiFi network with IP Address: ");
  Serial.println(WiFi.localIP());
  
  // Create the semaphore
  bufferSemaphore = xSemaphoreCreateBinary();
  displayMutex = xSemaphoreCreateMutex();
  
  xSemaphoreGive(bufferSemaphore);
  xSemaphoreGive(displayMutex);

  // Create the tasks
  xTaskCreatePinnedToCore(
    GPSTask,          // Task function
    "GPS Task",       // Task name
    10000,           // Stack size
    NULL,            // Task parameters
    4,               // Priority
    NULL,            // Task handle
    APP_CPU_NUM               // Core number (0 or 1)
  );
  xTaskCreatePinnedToCore(EnvioDeDadosTask, "Envio De Dados Task", 10000, NULL, 8, NULL, PRO_CPU_NUM);
  
  // Notify that all tasks have been created (lock Serial with mutex)
  xSemaphoreTake(displayMutex, portMAX_DELAY);
  Serial.println("All tasks created");
  xSemaphoreGive(displayMutex);
  
}//end setup

void loop() {

  // Do nothing but allow yielding to lower-priority tasks
  vTaskDelay(1000 / portTICK_PERIOD_MS);
}
