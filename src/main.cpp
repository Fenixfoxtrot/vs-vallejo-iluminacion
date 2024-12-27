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

  server.send(200, "text/html", html);
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