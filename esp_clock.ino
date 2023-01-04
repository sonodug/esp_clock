// Use only core 1 for demo purposes
#if CONFIG_FREERTOS_UNICORE
  static const BaseType_t app_cpu = 0;
#else
  static const BaseType_t app_cpu = 1;
#endif

// LIBRARIES
#include <LiquidCrystal_I2C.h>
#include <iarduino_RTC.h>
#include <DHT.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>  

// PINS
#define BUTTON_SWITCH 16
#define BUTTON_SENS 4
#define RGB_GREEN 32
#define BUZZER 18
#define DHT11_PIN 5
#define RST_PIN 27
#define CLK_PIN 25
#define DAT_PIN 26
#define DISABLE_TIME 2000

// TASKS
void buttonsTask(void *pvParameters);
void lcdClockTask(void *pvParameters);
void lcdSensorTask(void *pvParameters);
void listenSockets(void *pvParameters);
void checkTime(void *pvParameters);
void syncTime(void *pvParameters);

// OBJECTS DECLARATION
LiquidCrystal_I2C lcd(0x27, 16, 2);
DHT dht(DHT11_PIN, DHT11);
iarduino_RTC rtc(RTC_DS1302, RST_PIN, CLK_PIN, DAT_PIN);

// SEMAPHORES
SemaphoreHandle_t xBtnSemaphore;
SemaphoreHandle_t xAlarmTimeSemaphore;
SemaphoreHandle_t xTimeSyncSemaphore;

// TASK HANDLERS
TaskHandle_t ClockTaskHandle = NULL;
TaskHandle_t SensTaskHandle = NULL;

// TIMERS
TimerHandle_t syncTimer = NULL;

// TIMER'S CALLBACKS
void syncTimerCallback(TimerHandle_t xTimer) {  // Called when one of the timers expires
  Serial.println("Sync Timer expired");
}

// ALARM TIME PARAMS
uint32_t day = 0;
uint32_t hours = 0;
uint32_t minutes = 0;
uint32_t seconds = 0;

int tempDay;
int tempHours;
int tempMinutes;
int tempSeconds;

int syncYear;
int syncMonth;
int syncDay;
int syncHours;
int syncMinutes;
int syncSeconds;
int syncDayOfWeek;

// WEB-SERVER SETTINGS
const char* ssid = "esp32";
const char* password = "123456789";

// HTML & JS code
String website = "<!DOCTYPE html> <html> <head> <title>Alarm clock</title> </head> <body style='background-color: #100908;'> <span style='color: #e3f1f2;'> <h1>Alarm clock </h1> <form id='form'> day: <input type='number' name='day' id='days' size='5'><br><br> hours: <input type='number' name='hours' id='hours' size='5'><br><br> minutes: <input type='number' name='minutes' id='minutes' size='5'><br><br> seconds: <input type='number' name='seconds' id='seconds' size='5'><br><br> </form><br> <button onclick='button_set()'>Set alarm time</button><br><br> <p>Current time is: <span id='time'>-</span></p> <button onclick='syncTimeEsp()'>Sync time with alarm clock</button> <p>Current temperature is: <span id='temperature'>-</span></p> <p>Current humidity is: <span id='humidity'>-</span></p> </span> </body> <script> var Socket; function init() { Socket = new WebSocket('ws://' + window.location.hostname + ':81/'); Socket.onmessage = function (event) { processCommand(event); }; } function button_set() { var dayValue = document.getElementById('days').value; var hoursValue = document.getElementById('hours').value; var minutesValue = document.getElementById('minutes').value; var secondsValue = document.getElementById('seconds').value; var msg = { days: '', hours: '', minutes: '', seconds: '' }; msg.days = dayValue; msg.hours = hoursValue; msg.minutes = minutesValue; msg.seconds = secondsValue; Socket.send(JSON.stringify(msg)); } function processCommand(event) { var obj = JSON.parse(event.data); document.getElementById('temperature').innerHTML = obj.temperature; document.getElementById('humidity').innerHTML = obj.humidity; console.log(obj.temperature); console.log(obj.humidity); } function syncTimeEsp() { now = new Date(); y = now.getFullYear(); m = now.getMonth() + 1; d = now.getDate(); h = now.getHours(); min = now.getMinutes(); s = now.getSeconds(); dw = now.getDay(); msg = { year: '', month: '', day: '', hour: '', minutes: '', seconds: '', dayOfWeek: '' }; msg.year = y; msg.month = m; msg.day = d; msg.hour = h; msg.minutes = min; msg.seconds = s; msg.dayOfWeek = dw; Socket.send(JSON.stringify(msg)); } function outputTime() { now = new Date(); d = now.getDate(); m = now.getMonth() + 1; y = now.getFullYear(); h = now.getHours(); min = now.getMinutes(); s = now.getSeconds(); if (d <= 9 && m > 9) { document.getElementById('time').innerHTML = '0' + d + '.' + m + '.' + y + ' | ' + h + ':' + min + ':' + s; } else if (d <= 9 && m <= 9) { document.getElementById('time').innerHTML = '0' + d + '.' + '0' + m + '.' + y + ' | ' + h + ':' + min + ':' + s; } else { document.getElementById('time').innerHTML = d + '.' + m + '.' + y + ' | ' + h + ':' + min + ':' + s; } setTimeout(outputTime, 1000); } outputTime(); window.onload = function (event) { init(); } window.onload = function (event) { syncTimeEsp(); } </script> </html>";

int interval = 1000; // Interval for recording data from sensors
unsigned long previousMillis = 0;

// Port 80 for Web-server, 81 for Web-socket
WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);

// Button interrupt
void IRAM_ATTR ISR_btn()
{
  xSemaphoreGiveFromISR(xBtnSemaphore, NULL);
}

// Task for handle button clicks
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

// Task for listen sockets
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
    vTaskDelay(100);
  }
}

// Task to validate incoming alarm data from the client and assign it to global variables for verification by the buzzer
void checkTime(void *pvParameters)
{
  while(true)
  {
    if (xSemaphoreTake(xAlarmTimeSemaphore, portMAX_DELAY))
    {
      if (tempDay <= 31 && tempDay >= 1 && tempHours <= 24 && tempHours >= 0 && tempMinutes <= 60 && tempMinutes >= 0 && tempSeconds <= 60 && tempSeconds >= 0)
      {
        day = tempDay;
        hours = tempHours;
        minutes = tempMinutes;
        seconds = tempSeconds;

        Serial.println("Validation was successful.");
        Serial.println("");
        Serial.println("The alarm will ring in:");
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
    vTaskDelay(100);
  }
}

// Task for synchronize time with rtc
void syncTime(void *pvParameters)
{
  while(true)
  {
    if (xSemaphoreTake(xTimeSyncSemaphore, portMAX_DELAY))
    {
      rtc.settime(syncSeconds, syncMinutes, syncHours, syncDay, syncMonth, syncYear, syncDayOfWeek);
      Serial.println("Sync time data processed.");
    }
    vTaskDelay(100);
  }
}

// Task for checking buzzer time
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

// Task of displaying the current time
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

// Task of displaying the current sensors
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

// Event that is executed when Socket.send()
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
        JsonObject object = doc.as<JsonObject>();
        if (object.size() == 4)
        {
          tempDay = doc["days"];
          tempHours = doc["hours"];
          tempMinutes = doc["minutes"];
          tempSeconds = doc["seconds"];
          Serial.println("Received alarm params from client №" + String(num));
          xSemaphoreGive(xAlarmTimeSemaphore);
        }
        else if (object.size() == 7)
        {
          syncYear = doc["year"];
          syncMonth = doc["month"];
          syncDay = doc["day"];
          syncHours = doc["hour"];
          syncMinutes = doc["minutes"];
          syncSeconds = doc["seconds"];
          syncDayOfWeek = doc["dayOfWeek"];
          Serial.println("Received time sync params.");
          xSemaphoreGive(xTimeSyncSemaphore);
        }
      }
    Serial.println("");
      break;
  }
}

// Initialization function
void setup()
{
  Serial.begin(115200); // Initiates a serial connection and sets the data transfer rate in bits/s (baud)

  // Sets Web-server to access point mode
  Serial.print("Setting up Access Point ... ");

  Serial.print("Starting Access Point ... ");
  Serial.println(WiFi.softAP(ssid, password) ? "Ready" : "Failed!");

  Serial.print("IP address = ");
  Serial.println(WiFi.softAPIP());
  
  server.on("/", []() {
    server.send(200, "text/html", website); // Send html code
  });

  server.begin();
  
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

  xAlarmTimeSemaphore = xSemaphoreCreateBinary();
  xTimeSyncSemaphore = xSemaphoreCreateBinary();

  pinMode(RGB_GREEN, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  
  analogWrite(RGB_GREEN, 255);

  lcd.init();                         // Initiating the LCD1602 module   
  lcd.backlight();                    // Turn on the backlight
  
  rtc.begin();                        // Initiating the RTC module
  rtc.settime(0, 0, 0, 0, 0, 0, 0);   // Setting the time: seconds, minutes, hours, month, day, year, day of week

  dht.begin();                        // Initiating the DHT11 module

  Serial.print("Setting AP (Access Point)…");

  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);
  Serial.print("[+] AP Created with IP Gateway ");
  Serial.println(WiFi.softAPIP());

  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);
  server.begin();

  syncTimer = xTimerCreate(                // Timer initializing
              "Sync-reload timer",         // Name of timer
              1000 / portTICK_PERIOD_MS,   // Period of timer (in ticks)
              pdTRUE,                      // Auto-reload
              (void *)1,                   // Timer ID
              syncTimerCallback);          // Callback function

  if (syncTimer == NULL) {                                 // Check to make sure timers were created
    Serial.println("Could not create one of the timers");
  }
  else {
    vTaskDelay(1000 / portTICK_PERIOD_MS);                 // Wait and then print out a message
    Serial.println("Starting timer...");

    xTimerStart(syncTimer, portMAX_DELAY);                 // Start timer (max block time if command queue if full)
  }

  xTaskCreate(            // xTaskCreate() in vanilla FreeRTOS, one more parameter if we would use xTaskCreatePinnedToCore()
      listenSockets,      // Function to be called
      "WI-FI",            // Name of task
      8000,               // Stack size (bytes in ESP32, words in FreeRTOS)
      NULL,               // Parameter to pass to function
      3,                  // Task priority (0 to configMAX_PRIORITIES - 1)
      NULL);              // Task handle
  
  xTaskCreate(buzzerTask, "Buzzer", 8000, NULL, 1, NULL);
  xTaskCreate(checkTime, "AlarmTime", 4000, NULL, 2, NULL);
  xTaskCreate(syncTime, "SyncTime", 4000, NULL, 2, NULL);
  xTaskCreate(buttonsTask, "BUTTONS", 8192, NULL, 2, NULL);
  xTaskCreate(lcdClockTask, "LCD_CLOCK", 4096, NULL, 2, &ClockTaskHandle);
  xTaskCreate(lcdSensorTask, "LCD_SENSOR", 5096, NULL, 2, &SensTaskHandle);
  vTaskSuspend(SensTaskHandle);
}

// Super-cycle function (not used)
void loop() {
  
}