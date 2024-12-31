#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <TimeLib.h>
#include <DHT.h>
#include <FS.h> // Include the SPIFFS library
#include <EEPROM.h>

#define DHTPIN D4     // Pin donde está conectado el sensor DHT11
#define DHTTYPE DHT11 // Tipo de sensor (DHT11)

DHT dht(DHTPIN, DHTTYPE);  // Instancia del sensor DHT

// Configuración del servidor web
ESP8266WebServer server(80);

// Pines de los relés
const int relayPins[] = {D0, D1, D2, D3, D5, D6, D7, D8}; // GPIO16, GPIO5, GPIO4, etc.
bool relayStates[8] = {false, false, false, false, false, false, false, false}; // Estados iniciales de los relés
bool isAuthenticated = false; // Variable para manejar la autenticación

// Variables para manejar la hora de encendido y apagado (hora, minuto, segundo)
int hourOn = 6;     // Hora de encendido por defecto (6:00 AM)
int minuteOn = 0;   // Minuto de encendido por defecto
int secondOn = 0;   // Segundo de encendido por defecto
int hourOff = 22;   // Hora de apagado por defecto (10:00 PM)
int minuteOff = 0;  // Minuto de apagado por defecto
int secondOff = 0;  // Segundo de apagado por defecto

// Configuración de Wi-Fi
const char* ssid = "INFINITUM1ABB";
const char* password = "ecykj4tDxu";

// Configuración de NTP
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

// Variables de sensor
float humidity = 0;
float temperature = 0;

// Declaración anticipada de funciones
void saveTimeSettings();

// Función para manejar el inicio de sesión
void handleLogin() {
  if (server.hasArg("username") && server.hasArg("password")) {
    String username = server.arg("username");
    String password = server.arg("password");

    // Eliminar espacios en blanco y caracteres invisibles
    username.trim();
    password.trim();

    Serial.print("Username: ");
    Serial.println(username);
    Serial.print("Password: ");
    Serial.println(password);

    if (username.equals("admin") && password.equals("123")) {
      isAuthenticated = true;
      EEPROM.write(0, 1);
      EEPROM.commit();
      server.sendHeader("Set-Cookie", "session=1; Max-Age=3600"); // Cookie válida por 1 hora
      server.sendHeader("Location", "/index.html");
      server.send(303);
    } else {
      isAuthenticated = false;
      server.send(401, "text/plain", "Unauthorized");
    }
  } else {
    server.send(400, "text/plain", "Bad Request");
  }
}

// Función para manejar el panel de control
void handleControlPanel() {
  if (server.hasHeader("Cookie") && server.header("Cookie").indexOf("session=1") != -1) {
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

    for (int i = 0; i < 8; i++) {
      html.replace("{{relayState" + String(i) + "}}", relayStates[i] ? "on" : "off");
    }

    server.send(200, "text/html", html);
  } else {
    server.send(403, "text/plain", "Acceso denegado");
  }
}

// Función para manejar la configuración de horarios
void handleSetTime() {
  if (!server.hasHeader("Cookie") || server.header("Cookie").indexOf("session=1") == -1) {
    server.sendHeader("Location", "/index.html");
    server.send(303);
    return;
  }

  if (server.hasArg("hourOn")) hourOn = server.arg("hourOn").toInt();
  if (server.hasArg("minuteOn")) minuteOn = server.arg("minuteOn").toInt();
  if (server.hasArg("secondOn")) secondOn = server.arg("secondOn").toInt();
  if (server.hasArg("hourOff")) hourOff = server.arg("hourOff").toInt();
  if (server.hasArg("minuteOff")) minuteOff = server.arg("minuteOff").toInt();
  if (server.hasArg("secondOff")) secondOff = server.arg("secondOff").toInt();

  saveTimeSettings(); // Guardar la configuración de horarios

  server.send(200, "text/plain", "Horarios actualizados");
}

// Función para manejar el estado de los relés
void handleToggleRelay() {
  if (server.hasArg("relay")) {
    int relay = server.arg("relay").toInt();
    relayStates[relay] = !relayStates[relay];
    digitalWrite(relayPins[relay], relayStates[relay] ? LOW : HIGH);  // Invertir el estado de los relés
    Serial.println("Relé " + String(relay) + " " + (relayStates[relay] ? "encendido" : "apagado"));
    server.send(200, "text/plain", relayStates[relay] ? "on" : "off");
  }
}

// Función para manejar el estado de todos los relés
void handleToggleAllRelays() {
  if (server.hasArg("state")) {
    bool state = server.arg("state") == "true";
    for (int i = 0; i < 8; i++) {
      relayStates[i] = state;
      digitalWrite(relayPins[i], state ? LOW : HIGH);  // Invertir el estado de los relés
    }
    Serial.println(state ? "Todos los relés encendidos" : "Todos los relés apagados");
    server.send(200, "text/plain", state ? "on" : "off");
  } else {
    server.send(400, "text/plain", "Bad Request");
  }
}

// Función para obtener el estado de los relés
void handleGetRelayStates() {
  String states = "";
  for (int i = 0; i < 8; i++) {
    states += relayStates[i] ? "on" : "off";
    if (i < 7) states += ",";
  }
  server.send(200, "text/plain", states);
}

// Función para guardar el estado de los relés
void saveRelayStates() {
  File file = SPIFFS.open("/relays.txt", "w");
  if (file) {
    for (int i = 0; i < 8; i++) {
      file.println(relayStates[i] ? "1" : "0");
    }
    file.close();
  }
}

// Función para cargar el estado de los relés
void loadRelayStates() {
  File file = SPIFFS.open("/relays.txt", "r");
  if (file) {
    for (int i = 0; i < 8 && file.available(); i++) {
      relayStates[i] = file.readStringUntil('\n').toInt();
      digitalWrite(relayPins[i], relayStates[i] ? LOW : HIGH);
    }
    file.close();
  }
}

// Función para guardar la configuración de horarios
void saveTimeSettings() {
  File file = SPIFFS.open("/time.txt", "w");
  if (file) {
    file.printf("%d,%d,%d,%d,%d,%d\n", hourOn, minuteOn, secondOn, hourOff, minuteOff, secondOff);
    file.close();
  }
}

// Función para cargar la configuración de horarios
void loadTimeSettings() {
  File file = SPIFFS.open("/time.txt", "r");
  if (file) {
    if (file.available()) {
      String line = file.readStringUntil('\n');
      sscanf(line.c_str(), "%d,%d,%d,%d,%d,%d", &hourOn, &minuteOn, &secondOn, &hourOff, &minuteOff, &secondOff);
    }
    file.close();
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

  loadRelayStates();
  loadTimeSettings();

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
  server.on("/", handleControlPanel);
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
