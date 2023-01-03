#if CONFIG_FREERTOS_UNICORE
  static const BaseType_t app_cpu = 0;
#else
  static const BaseType_t app_cpu = 1;
#endif

//Libraries
#include <LiquidCrystal_I2C.h>
#include <iarduino_RTC.h>
#include <DHT.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>  

//PINS
#define BUTTON_SWITCH 16
#define BUTTON_SENS 4
#define RGB_GREEN 32
#define BUZZER 18
#define DHT11_PIN 5
#define RST_PIN 27
#define CLK_PIN 25
#define DAT_PIN 26
#define DISABLE_TIME 2000

//TASKS
void buttonsTask(void *pvParameters);
void lcdClockTask(void *pvParameters);
void lcdSensorTask(void *pvParameters);
void listenSockets(void *pvParameters);
void checkTime(void *pvParameters);

//OBJECTS DECLARATION
LiquidCrystal_I2C lcd(0x27, 16, 2);
DHT dht(DHT11_PIN, DHT11);
iarduino_RTC rtc(RTC_DS1302, RST_PIN, CLK_PIN, DAT_PIN);

SemaphoreHandle_t xBtnSemaphore;
SemaphoreHandle_t xTimeSemaphore;

TaskHandle_t ClockTaskHandle = NULL;
TaskHandle_t SensTaskHandle = NULL;

//ALARM TIME PARAMS
uint32_t day = 0;
uint32_t hours = 0;
uint32_t minutes = 0;
uint32_t seconds = 0;

int tempDay;
int tempHours;
int tempMinutes;
int tempSeconds;

//WEB-SERVER SETTING
const char* ssid = "esp32";
const char* password = "123456789";

String website = "<!DOCTYPE html> <html> <head> <title>Page Title</title> </head> <body style='background-color: #100908;'> <span style='color: #e3f1f2;'> <h1>Alarm clock </h1> <form id='form'> day: <input type='number' name='day' id='days' size='5'><br><br> hours: <input type='number' name='hours' id='hours' size='5'><br><br> minutes: <input type='number' name='minutes' id='minutes' size='5'><br><br> seconds: <input type='number' name='seconds' id='seconds' size='5'><br><br> <button type='submit' id='BTN_PING_ESP32'>Set alarm time</button> </form><br> <p>Current temperature is: <span id='temperature'>-</span></p> <p>Current humidity is: <span id='humidity'>-</span></p> </span> </body> <script> var Socket; const form = document.getElementById('form'); form.addEventListener('submit', button_set); function init() { Socket = new WebSocket('ws://' + window.location.hostname + ':81/'); Socket.onmessage = function(event) { processCommand(event); }; } function button_set() { var dayValue = document.getElementById('days').value; var hoursValue = document.getElementById('hours').value; var minutesValue = document.getElementById('minutes').value; var secondsValue = document.getElementById('seconds').value; var msg = { days: '', hours: '', minutes: '', seconds: '' }; msg.days = dayValue; msg.hours = hoursValue; msg.minutes = minutesValue; msg.seconds = secondsValue; Socket.send(JSON.stringify(msg)); } function processCommand(event) { var obj = JSON.parse(event.data); document.getElementById('temperature').innerHTML = obj.temperature; document.getElementById('humidity').innerHTML = obj.humidity; console.log(obj.temperature); console.log(obj.humidity); } window.onload = function(event) { init(); } </script> </html>";

int interval = 1000;
unsigned long previousMillis = 0;

WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);

void IRAM_ATTR ISR_btn()
{
  xSemaphoreGiveFromISR(xBtnSemaphore, NULL);
}

void buttonsTask(void *pvParameter)
{
  bool isISR = true;
  bool isSleep = false; 
  bool stateEditBtn = false, stateSwitchBtn = false;
  bool switchBtnFlag = false;
  uint32_t ms_btn = 0;
  
  pinMode(BUTTON_SWITCH, INPUT);
  pinMode(BUTTON_SENS, INPUT);

  xBtnSemaphore = xSemaphoreCreateBinary();

  xSemaphoreTake(xBtnSemaphore, 100);

  attachInterrupt(BUTTON_SWITCH, ISR_btn, RISING);
  attachInterrupt(BUTTON_SENS, ISR_btn, RISING);  

  while(true)
  {
    if(isISR)
    {
      xSemaphoreTake(xBtnSemaphore, portMAX_DELAY);

      detachInterrupt(BUTTON_SWITCH);
      detachInterrupt(BUTTON_SENS);
      isISR = false;   
    }
    else {
      bool state2 = digitalRead(BUTTON_SWITCH);
      bool state3 = digitalRead(BUTTON_SENS);
       
      if(state2 != stateEditBtn)
      {
        stateEditBtn = state2;
        if(state2 == HIGH)
        {
          ms_btn = millis();
          Serial.println("Enabled state button pressed");
        }
        else
        {
          Serial.println("Enabled state button released");
          uint32_t ms_duration = millis() - ms_btn;
          Serial.println(ms_duration); 

          if(ms_duration > DISABLE_TIME && !isSleep)
          {
            analogWrite(RGB_GREEN, 0);
            lcd.noBacklight();
            Serial.println("Alarm clock off");
            isSleep = !isSleep;
          }
          else if(ms_duration > DISABLE_TIME && isSleep)
          {
            analogWrite(RGB_GREEN, 255);
            lcd.backlight();
            Serial.println("Alarm clock on");
            isSleep = !isSleep;
          }
        }
      }  
      if(state3 != stateSwitchBtn)
      {
        stateSwitchBtn = state3;
        if(state3 == HIGH && switchBtnFlag == false)
        {
          Serial.println("Switch button pressed");
          switchBtnFlag = true;

          vTaskSuspend(ClockTaskHandle);
          vTaskDelay(100);
          Serial.println("Sensor mode");
          vTaskResume(SensTaskHandle);
        }
        else if(state3 == HIGH && switchBtnFlag == true)
        {
          switchBtnFlag = false;
          vTaskSuspend(SensTaskHandle);
          vTaskDelay(100);
          Serial.println("Clock mode");
          vTaskResume(ClockTaskHandle);
          Serial.println("Switch button released");
        }
      }    
      if(state2 == LOW && state3 == LOW)
      { 
        attachInterrupt(BUTTON_SWITCH, ISR_btn, RISING);
        attachInterrupt(BUTTON_SENS, ISR_btn, RISING);   

        isISR = true;
      }
      vTaskDelay(100);
    }
  }
}

void listenSockets(void *pvParameters)
{
  while(true)
  {
    server.handleClient();
    webSocket.loop(); 

    unsigned long now = millis();
    if ((unsigned long)(now - previousMillis) >= interval) {
      
      String jsonString = "";
      StaticJsonDocument<200> doc;
      JsonObject object = doc.to<JsonObject>();
      float t = dht.readTemperature();
      float h = dht.readHumidity();
      object["temperature"] = t;
      object["humidity"] = h;
      serializeJson(doc, jsonString);

      webSocket.broadcastTXT(jsonString);
      
      previousMillis = now;
    }
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

void checkTime(void *pvParameters)
{
  while(true)
  {
    if (xSemaphoreTake(xTimeSemaphore, portMAX_DELAY))
    {
      if (tempDay <= 31 && tempDay >= 1 && tempHours <= 24 && tempHours >= 0 && tempMinutes <= 60 && tempMinutes >= 0 && tempSeconds <= 60 && tempSeconds >= 0)
      {
        day = tempDay;
        hours = tempHours;
        minutes = tempMinutes;
        seconds = tempSeconds;

        Serial.print("The alarm will ring in:");
        Serial.println("Day: " + String(day));
        Serial.println("Hours: " + String(hours));
        Serial.println("Minutes: " + String(minutes));
        Serial.println("Seconds: " + String(seconds));
      }
      else
      {
        Serial.println("Incorrect input, try again.");
      }
    }
  }
}

void buzzerTask(void *pvParameters)
{
  while(true)
  {
    if (hours == rtc.Hours && minutes == rtc.minutes  && seconds == rtc.seconds)
    {
      tone(BUZZER, 1000, 1000);
      Serial.println("ALARM! WAKE UP!");
      lcd.clear();
    }
    vTaskDelay(1000);
  } 
}

void lcdClockTask(void *pvParameters)
{
  while(true)
  {
    lcd.setCursor(0,0);
    lcd.print(rtc.gettime("d M Y, D"));
    lcd.setCursor(0,1);
    lcd.print(rtc.gettime("H:i:s        "));
    vTaskDelay(1000);
  } 
}

void lcdSensorTask(void *pvParameters)
{
  while(true)
  {
    float h = dht.readHumidity();
    float t = dht.readTemperature();
    if (isnan(h) || isnan(t)) {
      Serial.println("Ошибка считывания параметров с датчика");
      return;
    }
    lcd.setCursor(0,0);
    lcd.print("Hum: " + String(h) + String("       "));
    lcd.setCursor(0,1);
    lcd.print("Temp: " + String(t) + " *C");
    vTaskDelay(4000);
  }
}

void webSocketEvent(byte num, WStype_t type, uint8_t * payload, size_t length) 
{
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.println("Client " + String(num) + " disconnected");
      break;
    case WStype_CONNECTED:
      Serial.println("Client " + String(num) + " connected");

      break;
    case WStype_TEXT:
      StaticJsonDocument<200> doc;
      DeserializationError error = deserializeJson(doc, payload);
      if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
        return;
      }
      else {
        tempDay = doc["days"];
        tempHours = doc["hours"];
        tempMinutes = doc["minutes"];
        tempSeconds = doc["seconds"];
        Serial.println("Received alarm params from client №" + String(num));
        xSemaphoreGive(xTimeSemaphore);
      }
    Serial.println("");
      break;
  }
}

void setup()
{
  Serial.begin(115200);

  Serial.print("Setting up Access Point ... ");

  Serial.print("Starting Access Point ... ");
  Serial.println(WiFi.softAP(ssid, password) ? "Ready" : "Failed!");

  Serial.print("IP address = ");
  Serial.println(WiFi.softAPIP());
  
  server.on("/", []() {
    server.send(200, "text/html", website);
  });

  server.begin();
  
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

  xTimeSemaphore = xSemaphoreCreateBinary();

  pinMode(RGB_GREEN, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  
  analogWrite(RGB_GREEN, 255);

  lcd.init();                    
  lcd.backlight();
  
  rtc.begin();
  rtc.settime(0, 0, 3, 20, 12, 22, 2);

  dht.begin();

  Serial.print("Setting AP (Access Point)…");

  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);
  Serial.print("[+] AP Created with IP Gateway ");
  Serial.println(WiFi.softAPIP());

  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);
  server.begin();

  xTaskCreate(listenSockets, "WI-FI", 8000, NULL, 3, NULL);
  xTaskCreate(buzzerTask, "Buzzer", 8000, NULL, 1, NULL);
  xTaskCreate(checkTime, "Time", 8000, NULL, 2, NULL);
  xTaskCreate(buttonsTask, "BUTTONS", 8192, NULL, 2, NULL);
  xTaskCreate(lcdClockTask, "LCD_CLOCK", 4096, NULL, 2, &ClockTaskHandle);
  xTaskCreate(lcdSensorTask, "LCD_SENSOR", 4096, NULL, 2, &SensTaskHandle);

  vTaskSuspend(SensTaskHandle);
}

void loop() {
  
}