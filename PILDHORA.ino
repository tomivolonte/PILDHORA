#include <Arduino.h>
#include <Servo.h>
#include <Adafruit_NeoPixel.h>
#include <Wire.h>
#include <RTClib.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiClientSecure.h>
#include <time.h>
#include <EEPROM.h>

// ============================================================
// 1. CONFIGURACIÓN DE HARDWARE
// ============================================================
#define PIN_SERVO    D4
#define PIN_NEOPIXEL D5
#define PIN_BTN      D6  // Pull-Down (10k a GND)
#define PIN_BUZZER   D7
#define PIN_SENSOR   D8  // Sensor (0 detecta agujero)
#define PIN_SDA      D3  // RTC SDA
#define PIN_SCL      D2  // RTC SCL

// ============================================================
// 2. CONFIGURACIÓN DE RED Y FIREBASE
// ============================================================
#define WIFI_SSID     "Informatica2043"
#define WIFI_PASSWORD "Info20432025+"

#define FIREBASE_HOST   "devices-m1947.firebaseio.com"
#define FIREBASE_SECRET "Af53UVDofDdlEN1OlTabfM4Tp98mkEK3Uuk24ZWc"
String DEVICE_ID = "TEST-DEVICE-001";

#define TIMEZONE_OFFSET -3 * 3600 
#define DAYLIGHT_OFFSET 0

// ============================================================
// 3. VARIABLES GLOBALES
// ============================================================
ESP8266WebServer server(80);
Servo servo;
Adafruit_NeoPixel strip(1, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);
RTC_DS3231 rtcModule;
WiFiClientSecure fbClient;

const int SERVO_STOP = 1500;
const int SERVO_SLOW = 1300;   
const int SERVO_FAST = 1200; 

unsigned long lastCommandPoll = 0;
unsigned long lastStateUpdate = 0;
unsigned long lastTimeSync = 0;
unsigned long lastScheduleSync = 0; 
unsigned long lastTimePrint = 0;

const String DIAS[] = {"lunes", "martes", "miercoles", "jueves", "viernes", "sabado", "domingo"};
const String TURNOS[] = {"mañana", "mediodia", "tarde", "noche"};
const int HORAS_TRIGGER[] = {8, 13, 18, 23};

String input; 

// ============================================================
// HTML
// ============================================================
const char MAIN_page[] PROGMEM = R"=====( 
<!DOCTYPE html>
<html lang="es">
<head>
  <meta charset="UTF-8">
  <title>PILDHORA</title>
  <style>
    body { font-family: sans-serif; background: #111827; color: #fff; display: flex; justify-content: center; padding-top: 20px; }
    .card { background: #1f2937; padding: 20px; border-radius: 15px; width: 90%; max-width: 400px; text-align: center; }
    button { width: 100%; padding: 10px; margin: 5px 0; border: none; border-radius: 8px; font-weight: bold; cursor: pointer; }
    .btn-g { background: #22c55e; color: white; }
    .btn-r { background: #ef4444; color: white; }
    .btn-b { background: #3b82f6; color: white; }
    #status { margin-top: 15px; font-size: 12px; color: #ccc; }
  </style>
</head>
<body>
  <div class="card">
    <h1>PILDHORA</h1>
    <p>Sistema Automático + Test</p>
    <button class="btn-g" onclick="req('/girar')">Test Giro</button>
    <button class="btn-r" onclick="req('/luzroja')">Test Luz</button>
    <button class="btn-b" onclick="req('/fortu')">Test FORTU</button>
    <button class="btn-r" style="background:#f97316" onclick="req('/topo')">Test TOPO</button>
    <div id="status">Listo</div>
  </div>
  <script>
    function req(p) {
      document.getElementById("status").innerText = "Enviando...";
      fetch(p).then(r => r.text()).then(t => document.getElementById("status").innerText = "OK: " + t);
    }
  </script>
</body>
</html>
)=====";

// ============================================================
// PROTOTIPOS
// ============================================================
void topo();
void fortu();
void girar();
void stopServo();
void luzRoja();
void luzVerde();
void syncTime();
void syncSchedule(); 
void checkAlarm();   
void handleFirebase();
void resetFbCmd(String cmd);
void processDispenseRequests();
void leerSerial();
void procesarComando(String cmd);

bool getScheduleFromEEPROM(int dayIdx, int shiftIdx);
void setLastExecuted(int day, int hour);
bool wasExecuted(int day, int hour);
void getTestAlarmFromEEPROM(int &h, int &m, bool &mode);
void setLastTestExecuted(int day, int hour, int min);
bool wasTestExecuted(int day, int hour, int min);

// ============================================================
// SETUP
// ============================================================
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n--- PILDHORA: SYSTEM START ---");

  EEPROM.begin(512);

  Wire.begin(PIN_SDA, PIN_SCL);
  if (!rtcModule.begin()) Serial.println("ERROR: RTC no detectado");
  else Serial.println("RTC OK");

  servo.attach(PIN_SERVO); servo.writeMicroseconds(SERVO_STOP);
  strip.begin(); strip.show();
  pinMode(PIN_BUZZER, OUTPUT); digitalWrite(PIN_BUZZER, LOW);
  pinMode(PIN_BTN, INPUT); 
  pinMode(PIN_SENSOR, INPUT_PULLUP);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("WiFi: ");
  int retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < 20) { 
    delay(500); Serial.print("."); retries++; 
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(" OK.");
    configTime(TIMEZONE_OFFSET, DAYLIGHT_OFFSET, "pool.ntp.org");
    syncTime();      
    syncSchedule();  
  } else {
    Serial.println(" OFFLINE. Usando EEPROM.");
  }

  fbClient.setInsecure();
  fbClient.setBufferSizes(1024, 1024);

  server.on("/", []() { server.send_P(200, "text/html", MAIN_page); });
  server.on("/topo", []() { topo(); server.send(200, "text/plain", "topo"); });
  server.on("/fortu", []() { fortu(); server.send(200, "text/plain", "fortu"); });
  server.on("/girar", []() { girar(); server.send(200, "text/plain", "ok"); });
  server.on("/luzroja", []() { luzRoja(); server.send(200, "text/plain", "ok"); });
  server.begin();
}

// ============================================================
// LOOP PRINCIPAL
// ============================================================
void loop() {
  // 1. Chequear alarmas con máxima prioridad
  checkAlarm();

  // 2. Imprimir hora (debug)
  if (millis() - lastTimePrint >= 1000) {
    lastTimePrint = millis();
    DateTime now = rtcModule.now();
    char buf[30];
    sprintf(buf, "HORA: %02d:%02d:%02d", now.hour(), now.minute(), now.second());
    Serial.println(buf);
  }

  // 3. Tareas de red
  server.handleClient();
  handleFirebase(); 

  // 4. Sincronizaciones
  if (millis() - lastTimeSync > 3600000) syncTime(); // 1 hora
  
  // ¡OJO! Sincronizamos AGENDA cada 1 MINUTO (60000) para pruebas rápidas
  if (millis() - lastScheduleSync > 60000) syncSchedule(); 
  
  leerSerial();
}

// ============================================================
// CEREBRO DE ALARMAS
// ============================================================
void checkAlarm() {
  DateTime now = rtcModule.now();
  int curH = now.hour();
  int curM = now.minute();
  int curD = (now.dayOfTheWeek() == 0) ? 6 : now.dayOfTheWeek() - 1; 

  // ----------------------------------------------------
  // 1. TEST (PROFES)
  // ----------------------------------------------------
  int testH, testM;
  bool testMode;
  getTestAlarmFromEEPROM(testH, testM, testMode);

  // Si hora coincide y minuto es igual o mayor (ventana de seguridad)
  if (curH == testH && curM >= testM) {
    // Si NO se ejecutó ya
    if (!wasTestExecuted(curD, testH, testM)) {
      Serial.print(">>> ALARMA TEST DETECTADA [");
      Serial.print(testH); Serial.print(":"); Serial.print(testM);
      Serial.print("] Modo: "); Serial.println(testMode ? "TOPO" : "FORTU");
      
      if (testMode) topo();
      else fortu();

      setLastTestExecuted(curD, testH, testM);
      return; 
    }
  }

  // ----------------------------------------------------
  // 2. TURNOS NORMALES
  // ----------------------------------------------------
  int shiftIdx = -1;
  for (int i = 0; i < 4; i++) {
    if (curH == HORAS_TRIGGER[i]) {
      shiftIdx = i;
      break;
    }
  }

  if (shiftIdx != -1) {
    if (!wasExecuted(curD, curH)) {
      Serial.print(">>> TURNO NORMAL DETECTADO: ");
      Serial.println(TURNOS[shiftIdx]);

      bool isTopo = getScheduleFromEEPROM(curD, shiftIdx);
      if (isTopo) topo();
      else fortu();

      setLastExecuted(curD, curH);
    }
  }
}

// ============================================================
// GESTIÓN DE EEPROM
// ============================================================
void setLastExecuted(int day, int hour) {
  EEPROM.write(30, day); EEPROM.write(31, hour); EEPROM.commit();
}
bool wasExecuted(int day, int hour) {
  return (EEPROM.read(30) == day && EEPROM.read(31) == hour);
}
void setLastTestExecuted(int day, int hour, int min) {
  EEPROM.write(50, day); EEPROM.write(51, hour); EEPROM.write(52, min); EEPROM.commit();
}
bool wasTestExecuted(int day, int hour, int min) {
  return (EEPROM.read(50) == day && EEPROM.read(51) == hour && EEPROM.read(52) == min);
}
bool getScheduleFromEEPROM(int dayIdx, int shiftIdx) {
  int addr = (dayIdx * 4) + shiftIdx;
  return (EEPROM.read(addr) == 1);
}
void getTestAlarmFromEEPROM(int &h, int &m, bool &mode) {
  h = EEPROM.read(40);
  m = EEPROM.read(41);
  mode = (EEPROM.read(42) == 1);
  // Validar basura
  if (h > 23) h = 99; 
  if (m > 59) m = 99;
}

// ============================================================
// SINCRONIZACIÓN
// ============================================================
void syncSchedule() {
  if (WiFi.status() != WL_CONNECTED) return;
  // Serial.println("Sync Agenda & Test..."); // Comentar para no ensuciar log
  lastScheduleSync = millis();

  if (fbClient.connect(FIREBASE_HOST, 443)) {
    
    // --- A. Agenda Normal (Simplificada para no bloquear) ---
    // (Si la necesitas descomenta el loop de arriba del código anterior)
    // Para la demo, nos centramos en que TEST funcione rápido.
    
    // --- B. TEST TIME (turnotest) ---
    String urlT = "/devices/" + DEVICE_ID + "/commands/turnotest.json?auth=" + FIREBASE_SECRET;
    fbClient.print(String("GET ") + urlT + " HTTP/1.1\r\nHost: " + FIREBASE_HOST + "\r\nConnection: close\r\n\r\n");
    
    String timeStr = "";
    unsigned long tOut = millis();
    while (fbClient.connected() || fbClient.available()) {
       if (millis() - tOut > 2000) break;
       if (fbClient.available()) {
          String line = fbClient.readStringUntil('\n');
          if (line == "\r") { 
            timeStr = fbClient.readString(); 
            break; 
          }
       }
    }
    fbClient.stop(); 

    // --- C. TEST BOOL (testbool) ---
    String urlB = "/devices/" + DEVICE_ID + "/commands/testbool.json?auth=" + FIREBASE_SECRET;
    if (fbClient.connect(FIREBASE_HOST, 443)) {
        fbClient.print(String("GET ") + urlB + " HTTP/1.1\r\nHost: " + FIREBASE_HOST + "\r\nConnection: close\r\n\r\n");
        bool tBool = false;
        tOut = millis();
        while (fbClient.connected() || fbClient.available()) {
           if (millis() - tOut > 2000) break;
           if (fbClient.available()) {
              String line = fbClient.readStringUntil('\n');
              if (line == "\r") { 
                 String val = fbClient.readString(); 
                 tBool = (val.indexOf("true") >= 0); 
                 break; 
              }
           }
        }
        fbClient.stop();

        // --- PROCESAR Y GUARDAR ---
        // Limpiar basura de JSON (comillas, espacios)
        timeStr.trim();
        timeStr.replace("\"", ""); 
        
        // Buscar separador (. o :)
        int dotIdx = timeStr.indexOf('.');
        if (dotIdx == -1) dotIdx = timeStr.indexOf(':');

        if (dotIdx > 0) {
          int h = timeStr.substring(0, dotIdx).toInt();
          int m = timeStr.substring(dotIdx + 1).toInt();
          
          // DEBUG IMPORTANTE PARA VER SI LEYÓ BIEN
          Serial.print("[SYNC] Recibido Firebase: "); Serial.print(timeStr);
          Serial.print(" -> Guardando H:"); Serial.print(h); Serial.print(" M:"); Serial.println(m);

          if (EEPROM.read(40) != h) EEPROM.write(40, h);
          if (EEPROM.read(41) != m) EEPROM.write(41, m);
          if (EEPROM.read(42) != (tBool?1:0)) EEPROM.write(42, tBool?1:0);
          
          EEPROM.commit();
        }
    }
  }
}

void syncTime() {
  if (WiFi.status() == WL_CONNECTED) {
    time_t now = time(nullptr);
    if (now > 1600000000) {
      struct tm * t = localtime(&now);
      rtcModule.adjust(DateTime(t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec));
      lastTimeSync = millis();
    }
  }
}

// ============================================================
// ACCIONES FÍSICAS
// ============================================================
void topo() {
  Serial.println("EJECUTANDO TOPO");
  if (digitalRead(PIN_BTN) == HIGH) { while(digitalRead(PIN_BTN) == HIGH) delay(10); }

  unsigned long start = millis();
  while (digitalRead(PIN_BTN) == LOW) {
    if (millis() - start > 120000) { digitalWrite(PIN_BUZZER, LOW); return; } 
    strip.setPixelColor(0, strip.Color(255, 0, 0)); strip.show();
    digitalWrite(PIN_BUZZER, HIGH); delay(150);
    strip.setPixelColor(0, 0); strip.show();
    digitalWrite(PIN_BUZZER, LOW); delay(150);
  }
  delay(50); while (digitalRead(PIN_BTN) == HIGH) delay(10);
  digitalWrite(PIN_BUZZER, LOW); strip.clear(); strip.show(); delay(200);

  servo.writeMicroseconds(SERVO_FAST); delay(65); 
  start = millis();
  while (true) { if (digitalRead(PIN_SENSOR) == 0) break; if (millis() - start > 10000) break; delay(1); }
  servo.writeMicroseconds(SERVO_STOP);

  strip.setPixelColor(0, strip.Color(255, 140, 0)); strip.show();
  start = millis();
  while (digitalRead(PIN_BTN) == LOW) { if (millis() - start > 120000) break; delay(50); }
  delay(50); while (digitalRead(PIN_BTN) == HIGH) delay(10);
  
  unsigned long t0 = millis(); bool s = false;
  while (millis() - t0 < 3000) { s = !s; strip.setPixelColor(0, s ? strip.Color(0, 255, 0) : 0); strip.show(); delay(200); }
  strip.clear(); strip.show();
}

void fortu() {
  Serial.println("EJECUTANDO FORTU");
  servo.writeMicroseconds(SERVO_FAST); delay(65); 
  unsigned long start = millis();
  while (true) { if (digitalRead(PIN_SENSOR) == 0) break; if (millis() - start > 10000) break; delay(1); }
  servo.writeMicroseconds(SERVO_STOP);
}

void girar() {
  servo.writeMicroseconds(SERVO_SLOW); delay(65); 
  unsigned long start = millis();
  while (digitalRead(PIN_SENSOR) == 1) { if (millis()-start > 8000) break; delay(1); }
  stopServo();
}

void stopServo() { servo.writeMicroseconds(SERVO_STOP); }
void luzRoja() { strip.setPixelColor(0, strip.Color(255,0,0)); strip.show(); delay(1000); strip.clear(); strip.show(); }
void luzVerde() { strip.setPixelColor(0, strip.Color(0,255,0)); strip.show(); delay(1000); strip.clear(); strip.show(); }
void leerSerial() { while (Serial.available()) { char c=Serial.read(); if(c=='\n') { procesarComando(input); input=""; } else input+=c; } }
void procesarComando(String cmd) {
  cmd.trim();
  if (cmd == "topo") topo(); else if (cmd == "fortu") fortu(); else if (cmd == "girar") girar();
  else if (cmd == "rtc") {
      DateTime now = rtcModule.now();
      char buf[20]; sprintf(buf, "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
      Serial.println(buf);
  }
}

void handleFirebase() {
  unsigned long now = millis();
  if (now - lastCommandPoll >= 2500) { 
    lastCommandPoll = now;
    if (WiFi.status() == WL_CONNECTED && fbClient.connect(FIREBASE_HOST, 443)) {
      fbClient.print(String("GET /devices/") + DEVICE_ID + "/commands.json?auth=" + FIREBASE_SECRET + " HTTP/1.1\r\nHost: " + FIREBASE_HOST + "\r\nConnection: close\r\n\r\n");
      unsigned long to = millis(); while (fbClient.available() == 0) { if (millis()-to > 2000) { fbClient.stop(); break; } delay(10); }
      while (fbClient.connected()) { String l = fbClient.readStringUntil('\n'); if (l == "\r") break; }
      String p = fbClient.readString(); fbClient.stop();

      if (p.indexOf("\"topo\":true") >= 0) { topo(); resetFbCmd("topo"); }
      if (p.indexOf("\"fortu\":true") >= 0) { fortu(); resetFbCmd("fortu"); }
      if (p.indexOf("\"buzzer\":true") >= 0) { digitalWrite(PIN_BUZZER,HIGH); delay(500); digitalWrite(PIN_BUZZER,LOW); resetFbCmd("buzzer"); }
    }
  }
  if (millis() % 2000 < 50) processDispenseRequests();
  
  if (now - lastStateUpdate >= 60000) {
    lastStateUpdate = now;
    if (WiFi.status() == WL_CONNECTED && fbClient.connect(FIREBASE_HOST, 443)) {
       String b = "{\"online\":true,\"ts\":" + String(millis()) + "}";
       fbClient.print(String("PATCH /devices/") + DEVICE_ID + "/state.json?auth=" + FIREBASE_SECRET + " HTTP/1.1\r\nHost: " + FIREBASE_HOST + "\r\nContent-Length: " + b.length() + "\r\nConnection: close\r\n\r\n" + b);
       fbClient.stop();
    }
  }
}

void resetFbCmd(String cmd) {
  if (fbClient.connect(FIREBASE_HOST, 443)) {
    fbClient.print(String("PUT /devices/") + DEVICE_ID + "/commands/" + cmd + ".json?auth=" + FIREBASE_SECRET + " HTTP/1.1\r\nHost: " + FIREBASE_HOST + "\r\nContent-Length: 5\r\nConnection: close\r\n\r\nfalse");
    fbClient.stop();
  }
}

void processDispenseRequests() {
  if (WiFi.status() != WL_CONNECTED || !fbClient.connect(FIREBASE_HOST, 443)) return;
  fbClient.print(String("GET /devices/") + DEVICE_ID + "/dispenseRequests.json?auth=" + FIREBASE_SECRET + " HTTP/1.1\r\nHost: " + FIREBASE_HOST + "\r\nConnection: close\r\n\r\n");
  unsigned long to = millis(); while (fbClient.available() == 0) { if (millis()-to > 2000) { fbClient.stop(); return; } delay(10); }
  while (fbClient.connected()) { String l = fbClient.readStringUntil('\n'); if (l == "\r") break; }
  String p = fbClient.readString(); fbClient.stop();
  if (p.length() > 10 && p.indexOf("error") < 0) {
    int q1 = p.indexOf('"'); int q2 = p.indexOf('"', q1+1);
    if (q1 >= 0 && q2 > q1) {
      String id = p.substring(q1+1, q2); topo(); 
      if (fbClient.connect(FIREBASE_HOST, 443)) {
        fbClient.print(String("DELETE /devices/") + DEVICE_ID + "/dispenseRequests/" + id + ".json?auth=" + FIREBASE_SECRET + " HTTP/1.1\r\nHost: " + FIREBASE_HOST + "\r\nConnection: close\r\n\r\n"); fbClient.stop();
      }
    }
  }
}