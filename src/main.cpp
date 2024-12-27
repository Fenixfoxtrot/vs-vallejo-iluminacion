#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <TimeLib.h>
#include <DHT.h>
#include <FS.h>

#define DHTPIN 10     // Pin donde está conectado el sensor DHT11
#define DHTTYPE DHT11 // Tipo de sensor (DHT11)

DHT dht(DHTPIN, DHTTYPE);  // Instancia del sensor DHT

// Configuración del servidor web
ESP8266WebServer server(80);

// Pines de los relés
const int relayPins[] = {16, 5, 4, 2, 14, 12, 13, 15}; // GPIO16, GPIO5, GPIO4, etc.
bool relayStates[8] = {false, false, false, false, false, false, false, false}; // Estados iniciales de los relés

// Variables para manejar la hora de encendido y apagado (hora, minuto, segundo)
int hourOn = 6;     // Hora de encendido por defecto (6:00 AM)
int minuteOn = 0;   // Minuto de encendido por defecto
int secondOn = 0;   // Segundo de encendido por defecto
int hourOff = 22;   // Hora de apagado por defecto (10:00 PM)
int minuteOff = 0;  // Minuto de apagado por defecto
int secondOff = 0;  // Segundo de apagado por defecto

// Credenciales Wi-Fi
const char* ssid = "INFINITUM1ABB";       // Cambia por tu SSID
const char* password = "ecykj4tDxu"; // Cambia por tu contraseña

// Configuración de NTP para sincronización horaria (hora estándar de la Ciudad de México: UTC -6)
WiFiUDP udp;
NTPClient timeClient(udp, "pool.ntp.org", -21600, 60000); // UTC -6, que equivale a -21600 segundos (6 horas)

// Variables de lectura del sensor DHT
float humidity = 0;
float temperature = 0;

// Página HTML con formulario para cambiar horarios
String getHTML() {
  String html = "<!DOCTYPE html><html lang='es'><head>";
  html += "<meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1'>";  
  html += "<title>Panel de Control luminaria Col. Vallejo</title>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; color: #ffffff; text-align: center; padding: 20px; background-image: url('/background.jpg'); background-size: cover; background-position: center; background-repeat: repeat; height: 100vh; margin: 0; color: #333; }";
  html += ".title-container, .content-container { background-color: rgba(128, 128, 128, 0.4); padding: 20px; border-radius: 5px; margin-bottom: 40px; color: #ffffff; }";
  html += "h1 { color: #ffffff; margin: 0; }";
  html += ".button-panel { display: grid; grid-template-columns: repeat(2, 1fr); gap: 10px; justify-content: center; }";
  html += "button { padding: 10px 30px; font-size: 18px; border: none; cursor: pointer; border-radius: 5px; transition: background-color 0.3s, transform 0.3s; opacity: 0.7; }";
  html += "button:hover { transform: scale(1.05); opacity: 1; }";
  html += ".on { background-color: green; color: white; }";
  html += ".off { background-color: red; color: white; }";
  html += ".form-container { margin-top: 40px; }";
  html += "input { padding: 8px; font-size: 16px; margin: 5px; }";
  html += ".button-container { display: flex; justify-content: space-between; margin-top: 40px; }";
  html += ".button-container button { width: 45%; }";  // Hacer los botones más grandes y rectangulares
  html += ".sensor-container { margin-top: 20px; display: flex; justify-content: center; align-items: center; gap: 20px; }";
  html += ".sensor-container h3 { margin: 0; }";
  html += ".end-container { margin-top: 40px; display: flex; justify-content: space-between; }";  // Botones de encender y apagar
  html += "</style>";
  html += "</head><body>";

  html += "<div class='title-container'><h1>Panel de Control Focos Col. Vallejo</h1></div>";

  // Mostrar la hora de CDMX en formato HH:mm:ss
  html += "<div class='content-container'><h2>Hora actual: " + String(hour()) + ":" + String(minute()) + ":" + String(second()) + "</h2></div>";

  html += "<div class='content-container button-panel'>";
  for (int i = 0; i < 8; i++) {
    html += "<button id='relay" + String(i) + "' class='" + (relayStates[i] ? "on" : "off") + "' onclick='toggleRelay(" + String(i) + ")'>Foco " + String(i + 1) + "</button>";
  }
  html += "</div>";

  // Mostrar temperatura y humedad por encima de los botones de "Encender" y "Apagar"
  html += "<div class='content-container sensor-container'>";
  html += "<h3>Temperatura: " + String(temperature) + " &#8451;</h3>";
  html += "<h3>Humedad: " + String(humidity) + " %</h3>";
  html += "</div>";

  // Configuración de horarios
  html += "<div class='content-container form-container'>";
  html += "<h2>Configurar Horarios de Encendido/Apagado</h2>";
  html += "<form action='/setTime' method='get'>";
  html += "<label>Hora Encendido:</label><input type='number' name='hourOn' value='" + String(hourOn) + "' min='0' max='23' required>";
  html += "<label>Minuto Encendido:</label><input type='number' name='minuteOn' value='" + String(minuteOn) + "' min='0' max='59' required>";
  html += "<label>Segundo Encendido:</label><input type='number' name='secondOn' value='" + String(secondOn) + "' min='0' max='59' required><br>";
  html += "<label>Hora Apagado:</label><input type='number' name='hourOff' value='" + String(hourOff) + "' min='0' max='23' required>";
  html += "<label>Minuto Apagado:</label><input type='number' name='minuteOff' value='" + String(minuteOff) + "' min='0' max='59' required>";
  html += "<label>Segundo Apagado:</label><input type='number' name='secondOff' value='" + String(secondOff) + "' min='0' max='59' required><br>";
  html += "<button type='submit'>Guardar</button>";
  html += "</form>";
  html += "</div>";

  // Botones para encender y apagar todos los relés a los lados de los horarios
  html += "<div class='content-container end-container'>";
  html += "<button onclick='toggleAllRelays(true)' class='on'>Encender Todos</button>";
  html += "<button onclick='toggleAllRelays(false)' class='off'>Apagar Todos</button>";
  html += "</div>";

  html += "<script>";
  html += "function toggleRelay(relay) {";
  html += "fetch('/toggle?relay=' + relay);";
  html += "setTimeout(() => location.reload(), 500);";
  html += "}";
  
  html += "function toggleAllRelays(state) {";
  html += "fetch('/toggleAll?state=' + state);";
  html += "setTimeout(() => location.reload(), 500);";
  html += "}";
  html += "</script>";

  html += "</body></html>";
  return html;
}

// Maneja la página principal
void handleRoot() {
  temperature = dht.readTemperature();  // Leer temperatura
  humidity = dht.readHumidity();        // Leer humedad
  server.send(200, "text/html", getHTML());
}

// Maneja el encendido/apagado de relés
void handleToggleRelay() {
  if (server.hasArg("relay")) {
    int relay = server.arg("relay").toInt();
    relayStates[relay] = !relayStates[relay];
    digitalWrite(relayPins[relay], relayStates[relay] ? LOW : HIGH);  // Invertir el estado de los relés
    Serial.println("Relé " + String(relay) + " " + (relayStates[relay] ? "encendido" : "apagado"));
  }
  server.sendHeader("Location", "/");
  server.send(303);
}

// Maneja el encendido/apagado de todos los relés
void handleToggleAllRelays() {
  if (server.hasArg("state")) {
    bool state = server.arg("state") == "true";
    for (int i = 0; i < 8; i++) {
      relayStates[i] = state;
      digitalWrite(relayPins[i], state ? LOW : HIGH);  // Invertir el estado de los relés
    }
    Serial.println(state ? "Todos los relés encendidos" : "Todos los relés apagados");
  }
  server.sendHeader("Location", "/");
  server.send(303);
}

// Maneja la configuración de horarios
void handleSetTime() {
  if (server.hasArg("hourOn")) hourOn = server.arg("hourOn").toInt();
  if (server.hasArg("minuteOn")) minuteOn = server.arg("minuteOn").toInt();
  if (server.hasArg("secondOn")) secondOn = server.arg("secondOn").toInt();
  if (server.hasArg("hourOff")) hourOff = server.arg("hourOff").toInt();
  if (server.hasArg("minuteOff")) minuteOff = server.arg("minuteOff").toInt();
  if (server.hasArg("secondOff")) secondOff = server.arg("secondOff").toInt();
  server.sendHeader("Location", "/");
  server.send(303);
}

void setup() {
  Serial.begin(115200);
  dht.begin();

  // Inicializar SPIFFS
  if (!SPIFFS.begin()) {
    Serial.println("Error al montar SPIFFS");
    return;
  }

  // Conexión Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("Conectado a Wi-Fi");

  // Imprimir la dirección IP
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Inicializar NTP
  timeClient.begin();

  // Configurar los pines de los relés como salidas
  for (int i = 0; i < 8; i++) {
    pinMode(relayPins[i], OUTPUT);
    digitalWrite(relayPins[i], HIGH);  // Relés apagados por defecto (suponiendo que son activos bajos)
  }

  // Inicializar servidor web
  server.on("/", handleRoot);
  server.on("/toggle", handleToggleRelay);
  server.on("/toggleAll", handleToggleAllRelays);
  server.on("/setTime", handleSetTime);
  server.serveStatic("/background.jpg", SPIFFS, "/background.jpg");
  server.begin();
}

void loop() {
  timeClient.update();  // Actualizar la hora desde el NTP
  setTime(timeClient.getEpochTime()); // Establecer la hora en TimeLib

  // Verificar los horarios de encendido/apagado y controlar los relés
  int currentHour = hour();
  int currentMinute = minute();
  int currentSecond = second();

  // Encender los relés si la hora actual coincide con la hora de encendido
  if (currentHour == hourOn && currentMinute == minuteOn && currentSecond == secondOn) {
    for (int i = 0; i < 8; i++) {
      relayStates[i] = true;
      digitalWrite(relayPins[i], LOW); // Enciende todos los relés
    }
    Serial.println("Todos los relés encendidos");
  }

  // Apagar los relés si la hora actual coincide con la hora de apagado
  if (currentHour == hourOff && currentMinute == minuteOff && currentSecond == secondOff) {
    for (int i = 0; i < 8; i++) {
      relayStates[i] = false;
      digitalWrite(relayPins[i], HIGH); // Apaga todos los relés
    }
    Serial.println("Todos los relés apagados");
  }

  server.handleClient(); // Manejar las solicitudes del servidor
}