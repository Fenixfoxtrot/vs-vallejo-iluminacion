#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <TimeLib.h>
#include <DHT.h>
#include <FS.h>

#define RESET_INTERVAL 7200000  // Tiempo en milisegundos (1 minuto)
#define DHTPIN A0     // Pin donde está conectado el sensor DHT11
#define DHTTYPE DHT11 // Tipo de sensor (DHT11)
DHT dht(DHTPIN, DHTTYPE);  // Instancia del sensor DHT

// Configuración del servidor web
ESP8266WebServer server(80);

// Pines de los relés
const int relayPins[] = {16, 5, 4, 3, 14, 12, 13, 10, 15}; // GPIO16, GPIO5, GPIO4, etc.
bool relayStates[9] = {false, false, false, false, false, false, false, false, false}; // Estados iniciales de los relés

// Variables para manejar la hora de encendido y apagado (hora, minuto, segundo)
int hourOn = 18;     // Hora de encendido por defecto (6:48 PM)
int minuteOn = 40;   // Minuto de encendido por defecto
int secondOn = 0;   // Segundo de encendido por defecto
int hourOff = 21;   // Hora de apagado por defecto (10:00 PM)
int minuteOff = 50;  // Minuto de apagado por defecto
int secondOff = 0;  // Segundo de apagado por defecto

// Credenciales Wi-Fi
const char* ssid = "xxxxxxx";       // Cambia por tu SSID
const char* password = "xxxxxxxxx"; // Cambia por tu contraseña
// Dirección IP estática
IPAddress staticIP(192, 168, 100, 300);  // Cambia a la IP que desees usar
IPAddress gateway(192, 168, 100, 1);     // IP de tu router
IPAddress subnet(255, 255, 255, 0);    // Máscara de subred
IPAddress dns1(8, 8, 8, 8);            // DNS primario (Google DNS) 
IPAddress dns2(8, 8, 4, 4);            // DNS secundario (Google DNS)*/

// Configuración de NTP para sincronización horaria (hora estándar de la Ciudad de México: UTC -6)
WiFiUDP udp;
NTPClient timeClient(udp, "pool.ntp.org", -21600, 60000); // UTC -6, que equivale a -21600 segundos (6 horas)

// Variables de lectura del sensor DHT
float humidity = 0;
float temperature = 0;

// Maneja la página principal
void handleRoot() {
  temperature = dht.readTemperature();  // Leer temperatura
  humidity = dht.readHumidity();        // Leer humedad

  File file = SPIFFS.open("/index.html", "r");
  if (!file) {
    server.send(500, "text/plain", "File not found");
    return;
  }

  String html = file.readString();
  file.close();

// Obtener la dirección MAC
  String macAddress = WiFi.macAddress();

  // Reemplazar los marcadores de posición con los valores actuales
  html.replace("{{hour}}", String(hour()));
  html.replace("{{minute}}", String(minute()));
  html.replace("{{second}}", String(second()));
  html.replace("{{temperature}}", String(temperature));
  html.replace("{{humidity}}", String(humidity));
  html.replace("{{hourOn}}", String(hourOn));
  html.replace("{{minuteOn}}", String(minuteOn));
  html.replace("{{secondOn}}", String(secondOn));
  html.replace("{{hourOff}}", String(hourOff));
  html.replace("{{minuteOff}}", String(minuteOff));
  html.replace("{{secondOff}}", String(secondOff));

  for (int i = 0; i < 9; i++) {
    html.replace("{{relayState" + String(i) + "}}", relayStates[i] ? "on" : "off");
  }

  server.send(200, "text/html", html);
}

// Maneja el encendido/apagado de relés
void handleToggleRelay() {
  if (server.hasArg("relay")) {
    int relay = server.arg("relay").toInt();
    relayStates[relay] = !relayStates[relay];
    digitalWrite(relayPins[relay], relayStates[relay] ? HIGH : LOW);  // Invertir el estado de los relés
    Serial.println("Relé " + String(relay) + " " + (relayStates[relay] ? "encendido" : "apagado"));
    server.send(200, "text/plain", relayStates[relay] ? "on" : "off");
  }
}

// Maneja el encendido/apagado de todos los relés
void handleToggleAllRelays() {
  if (server.hasArg("state")) {
    bool state = server.arg("state") == "true";
    for (int i = 0; i < 9; i++) {
      relayStates[i] = state;
      digitalWrite(relayPins[i], state ? HIGH : LOW);  // Invertir el estado de los relés
    }
    Serial.println(state ? "Todos los relés encendidos" : "Todos los relés apagados");
    server.send(200, "text/plain", state ? "on" : "off");
  } else {
    server.send(400, "text/plain", "Bad Request");
  }
}

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

// Maneja la actualización del estado de los relés
void handleGetRelayStates() {
  String states = "";
  for (int i = 0; i < 9; i++) {
    states += relayStates[i] ? "on" : "off";
    if (i < 7) states += ",";
  }
  server.send(200, "text/plain", states);
}

// Maneja la página de inicio de sesión
void handleLogin() {
  if (server.hasArg("username") && server.hasArg("password")) {
    String username = server.arg("username");
    String password = server.arg("password");
    if (username == "Fenix Foxtrot" && password == "Fenix Foxtrot 11") {
      server.sendHeader("Location", "/");
      server.send(303);
    } else {
      server.send(401, "text/plain", "Unauthorized");
    }
  } else {
    server.send(400, "text/plain", "Bad Request");
  }
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
  WiFi.config(staticIP, gateway, subnet, dns1, dns2); // Configurar IP estática
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Conectado a Wi-Fi");

  // Imprimir la dirección IP
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Inicializar NTP
  timeClient.begin();

  // Configurar los pines de los relés como salidas
  for (int i = 0; i < 9; i++) {
    pinMode(relayPins[i], OUTPUT);
    digitalWrite(relayPins[i], LOW);  // Relés apagados por defecto (suponiendo que son activos bajos)
  }

  // Inicializar servidor web
  server.on("/", handleRoot);
  server.on("/toggle", handleToggleRelay);
  server.on("/toggleAll", handleToggleAllRelays);
  server.on("/setTime", handleSetTime);
  server.on("/getRelayStates", handleGetRelayStates);
  server.on("/login", HTTP_POST, handleLogin);
  server.serveStatic("/background1.jpg", SPIFFS, "/background1.jpg");
  server.serveStatic("/index.html", SPIFFS, "/index.html");
  server.begin();
}

void loop() {
  static unsigned long lastReset = millis();

  if (millis() - lastReset >= RESET_INTERVAL) {
      Serial.println("Reiniciando ESP...");
      ESP.restart();
  }
  timeClient.update();  // Actualizar la hora desde el NTP
  setTime(timeClient.getEpochTime()); // Establecer la hora en TimeLib

  // Verificar los horarios de encendido/apagado y controlar los relés
  int currentHour = hour();
  int currentMinute = minute();
  int currentSecond = second();

  // Encender los relés si la hora actual coincide con la hora de encendido
  if (currentHour == hourOn && currentMinute == minuteOn && currentSecond == secondOn) {
    for (int i = 0; i < 9; i++) {
      relayStates[i] = true;
      digitalWrite(relayPins[i], HIGH); // Enciende todos los relés
    }
    Serial.println("Todos los relés encendidos");
  }

  // Apagar los relés si la hora actual coincide con la hora de apagado
  if (currentHour == hourOff && currentMinute == minuteOff && currentSecond == secondOff) {
    for (int i = 0; i < 9; i++) {
      relayStates[i] = false;
      digitalWrite(relayPins[i], LOW); // Apaga todos los relés
    }
    Serial.println("Todos los relés apagados");
  }

  server.handleClient(); // Manejar las solicitudes del servidor
}