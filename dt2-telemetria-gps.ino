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

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <INA.h> // INA Library (by Zanshin)
#include <SD.h>
#include <math.h>

//-----------------------------------Replace with your network credentials----------------------------------------
// Replace with your network credentials
const char* ssid     = "Milhagem";
const char* password = "ehomilhas";

// REPLACE with your Domain name and URL path or IP address with path
const char* serverName = "http://milhagemufmg.com/gps/post-data.php";

// Keep this API Key value to be compatible with the PHP code provided in the project page.
// If you change the apiKeyValue value, the PHP file /post-data.php also needs to have the same key
String apiKeyValue = "tPmAT5Ab3j7F9";
//----------------------------------------------------------------------------------------------------------------

// Vamos calibrar o divisor de tensão e o sensor de corrente para o INA 226
// R5 = 20 kΩ
// R6 = 68 kΩ
// Divisor de tensão INA = R6/(R5 + R6) = 0,7727272... kΩ
// Divisor de tensão INA^-1 = (R5 + R6)/R6 = 1,29411764706
// fatorCorreçãoV = 1.01626

// Resistor do shunt = 1 mΩ
// FatorCorreçãoC = 0,482625

//#define R5 20000
//#define R6 68000
//#define valorShunt 0.001
//#define fatorCorrecaoV 1.01626
//#define fatorCorrecaoC 0.482625

#define fatorMili 0.001
#define fatorMicro 0.000001

#define ADC_VREF_mV    3300.0  // in millivolt
#define ADC_RESOLUTION 4096.0
#define AMPOP_OUT        32    // ESP32 pin connected to LM358P
#define GAIN              3.89 // AMPOP_OUT = LM35 output * GAIN

 // ---------- Energy data  ----------
  // Variaveis do sensor INA226
  INA_Class INA; // Construct a power monitor object names "ina"
  int32_t rawCurrent = -666;
  float corrente_motor_INA;
  uint16_t rawVoltage = 666;
  float tensao_bat_INA;
  int32_t rawShunt = -666;
  float shunt_motor_INA;
  float valorPotencia = 0;
  float valorConsumo = 0;
  float consumoParcial = 0;

  //Variaveis de tempo
  long tempoAtual = 0;
  long tempoAnterior = 0;
  long tempoDelta = 0;

 // ---------- GPS data ----------
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
  int   minutos  = 0;
  int   sec      = 0;
  String reading_time = "";

 // ---------- Temperature data ----------
  int adcValAmpOp;
  int adcVal;
  float milliVolt;
  int celcius;
  int farenheits;

SemaphoreHandle_t bufferSemaphore;
SemaphoreHandle_t displayMutex;       // Lock access to buffer and Serial
//----------------------------------------------------------------------------------------------------------------

void setup() {
  Serial.begin(115200);

  // Wait a moment to start (so we don't miss Serial output)
  vTaskDelay(1000 / portTICK_PERIOD_MS);

  INA.begin(80, 1000, 0x40);             // Begin calibration for an expected 80 Amps maximum current and for a 0.0O1hm resistor
  INA.setAveraging(10);                  // Average each reading n-times
  INA.setBusConversion(10000);           // Maximum conversion time 8.244ms
  INA.setShuntConversion(10000);         // Maximum conversion time 8.244ms
  INA.setMode(INA_MODE_CONTINUOUS_BOTH); // Bus/shunt measured continuously

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
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println("WL_NOT_CONNECTED");
  }//end_while
  Serial.println("");
  Serial.print("Connected to WiFi network with IP Address: ");
  Serial.println(WiFi.localIP());

  // Create the semaphore
  bufferSemaphore = xSemaphoreCreateBinary();
  displayMutex = xSemaphoreCreateMutex();

  //Set the semaphore
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
  xTaskCreatePinnedToCore(EnvioDeDadosTask, "Envio De Dados Task", 10000, NULL, 4, NULL, PRO_CPU_NUM);
  xTaskCreatePinnedToCore(INATask, "INA Task", 10000, NULL, 4, NULL, APP_CPU_NUM);
  xTaskCreatePinnedToCore(TemperaturaTask, "Temperatura Task", 10000, NULL, 4, NULL, APP_CPU_NUM);

  // Notify that all tasks have been created (lock Serial with mutex)
  xSemaphoreTake(displayMutex, portMAX_DELAY);
  Serial.println("All tasks created");
  xSemaphoreGive(displayMutex);

}//end setup
//----------------------------------------------------------------------------------------------------------------

void TemperaturaTask(void *pvParameters) {
  while (true) {
    // read the ADC value from the temperature OpAmp
    adcValAmpOp = analogRead(AMPOP_OUT);
    // converts de ADC value read from the OpAmp into the LM35 original value
    adcVal = adcValAmpOp / GAIN;
    // convert the ADC value to voltage in millivolt
    milliVolt = adcVal * (ADC_VREF_mV / ADC_RESOLUTION);
    // convert the voltage to the temperature in °C
    celcius = milliVolt / 10;
    // convert the °C to °F
    farenheits = celcius * 9 / 5 + 32;
  }//while
}//end Tempertura Task
//----------------------------------------------------------------------------------------------------------------

void INATask(void *pvParameters) {
  while (true) {
    // ______Sensor de Tensão INA__________ /

    float R5 = 2.2; // Resistência em kΩ
    float R6 = 2.2; // Resistência em kΩ
    double valorShunt = 0.001;
    double fatorCorrecaoV = 1.01626;
    double fatorCorrecaoC = 0.482625;
    
    xSemaphoreTake(bufferSemaphore, portMAX_DELAY);
    rawVoltage = INA.getBusMilliVolts();
    tensao_bat_INA = (float)rawVoltage * ((R5 + R6) / R6) * fatorCorrecaoV * fatorMili; // Esse valor que está sendo multiplicado pelo valor da tensão tem a função de calibrar o sensor.
    xSemaphoreGive(bufferSemaphore);
    
    xSemaphoreTake(displayMutex, portMAX_DELAY);
    Serial.print("Tensão Bateria :");
    Serial.println(tensao_bat_INA);
    xSemaphoreGive(displayMutex);

    // tensao_bat_INA = (float)rawVoltage * 0.00131516;
    //  Vbat = Vina *      1.29411764706       *    1.01626    *   0.001
    //  Vbat = Vina * fatorDivisorTensao * fatorCorreçãoV * fatorMili

    // _______Sensor de Corrente INA___________ /
    
    xSemaphoreTake(bufferSemaphore, portMAX_DELAY);
    rawCurrent = INA.getShuntMicroVolts();
    corrente_motor_INA = (float)rawCurrent * (fatorMicro / valorShunt) * fatorCorrecaoC;
    xSemaphoreGive(bufferSemaphore);
    
    xSemaphoreTake(displayMutex, portMAX_DELAY);
    Serial.print("Corrente Bateria :");
    Serial.println(corrente_motor_INA);
    xSemaphoreGive(displayMutex);

    // _______Medição de potência e consumo INA___________ /

    xSemaphoreTake(bufferSemaphore, portMAX_DELAY);
    valorPotencia = (float)INA.getBusMicroWatts() * fatorMicro;
    valorPotencia *= fatorCorrecaoV;
    xSemaphoreGive(bufferSemaphore);
    
    xSemaphoreTake(displayMutex, portMAX_DELAY);
    Serial.print("Potencia Instantanea :");
    Serial.println(valorPotencia);
    xSemaphoreGive(displayMutex);
    
    xSemaphoreTake(bufferSemaphore, portMAX_DELAY);
    tempoAnterior = tempoAtual;
    tempoAtual = millis();
    tempoDelta = tempoAtual - tempoAnterior;
    consumoParcial = valorPotencia * (tempoDelta) * fatorMili;
    valorConsumo = valorConsumo + consumoParcial;
    xSemaphoreGive(bufferSemaphore);
    
    xSemaphoreTake(displayMutex, portMAX_DELAY);
    Serial.print("Consumo :");
    Serial.println(valorConsumo);
    xSemaphoreGive(displayMutex);
  }//end_while
  // Imotor = (Iina * 10^-6 / 0.001) * 0,482625
  // Imotor = (Iina * fatorMicroVolt / valorShunt) * fatorCorreçãoC
  // ________________________ /
  // Serial.println(corrente_motor_INA);
  /*
    if (corrente_motor_INA >= corrente_limite)
    {
      digitalWrite(sinal_corrente, HIGH); // A variável sinal_corrente representa uma saída que vai para no mega/due e atua como um dispositivo de segurança que permite ou proíbe o acionamento
    }
    else
    {
      digitalWrite(sinal_corrente, LOW);
    }
    }
  */
}//end_INATask
//----------------------------------------------------------------------------------------------------------------


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
        SerialMon.println("Couldn't get GPS/GNSS/GLONASS location, retrying in 1s.");
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
//----------------------------------------------------------------------------------------------------------------

void EnvioDeDadosTask(void *pvParameters) {
  while (true) {
    //Check WiFi connection status
    if (WiFi.status() == WL_CONNECTED) {
      WiFiClient client;
      HTTPClient http;

      // Your Domain name with URL path or IP address with path
      http.begin(client, serverName);

      // Specify content-type header
      http.addHeader("Content-Type", "application/x-www-form-urlencoded");

      // Prepare your HTTP POST request data
      xSemaphoreTake(bufferSemaphore, portMAX_DELAY);
      String httpRequestData = "api_key=" + apiKeyValue + "&lat=" + String(lat, 8) + "&lng=" + String(lng, 8) + "&tensao_bat_INA=" + String(tensao_bat_INA, 2) +
      "&corrente_motor_INA=" + String(corrente_motor_INA, 2) + "&valorPotencia=" + String(valorPotencia, 2) + "&valorConsumo=" + String(valorConsumo, 2) + 
      "&celcius=" + String(celcius) + "&farenheits=" + String(farenheits) +"" ;
      xSemaphoreGive(bufferSemaphore);

      xSemaphoreTake(displayMutex, portMAX_DELAY);
      Serial.print("httpRequestData: ");
      Serial.println(httpRequestData);
      xSemaphoreGive(displayMutex);

      // Send HTTP POST request
      int httpResponseCode = http.POST(httpRequestData);

      if (httpResponseCode > 0) {
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
      while (WiFi.status() != WL_CONNECTED) {
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
//----------------------------------------------------------------------------------------------------------------


void loop() {

  // Do nothing but allow yielding to lower-priority tasks
  vTaskDelay(1000 / portTICK_PERIOD_MS);
}
