#include <Adafruit_AHT10.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager

#include <WiFi.h>
#include <PubSubClient.h>

#define botonA 5
#define botonB 18
#define SENSORAGUAPIN 19
#define TIEMPOENVIOINFORMACION 2000//milisegundos
#define BUFFERDELMENSAJE  (14)
#define TOPICODATOS "/datos/estacion/"
#define TOPICOCONEXION "/conexion/estacion/"
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3c ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
#define MQTTSERVER "broker.mqtt-dashboard.com"

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Adafruit_AHT10 aht;
sensors_event_t humidity, temp;


char msg[BUFFERDELMENSAJE];
String valorPH;

unsigned long lastMsg = 0;

long lastReconnectAttempt = 0;

OneWire oneWire(SENSORAGUAPIN);

DallasTemperature sensorAgua(&oneWire);


WiFiClient espClient;
PubSubClient client(espClient);

float temperaturaAgua = 0;
float tempC = 0;

int estados = 0;

boolean sensoresVerificados = false;

void vaciarPantalla() {
  display.clearDisplay();
  display.setCursor(0, 0);
}

void customString(String dataIn) {
  display.println(dataIn);
  Serial.print(dataIn);
  display.display();
}
void customInt(int datoIn) {
  display.print(datoIn);
  Serial.print(datoIn);
  display.display();
}


void configurarWifi() {
  WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP
  WiFiManager wm;
  bool res;
  res = wm.autoConnect("Chaac AP", ""); // password protected ap
  if (!res) {
    vaciarPantalla();
    customString("no se encontro WiFi");
    delay(5000);
    ESP.restart();
  }
  else {
    //if you get here you have connected to the WiFi
    vaciarPantalla();
    customString("Conectado Al Wifi");
  }
  estados = 4;

}


void verificarPantalla() {

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;); // Don't proceed, loop forever
  }
  display.setTextSize(1);      // Normal 1:1 pixel scale
  display.setTextColor(SSD1306_WHITE); // Draw white text

}

boolean verificarSensores() {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.print("Probando sensores");

  display.display();
  delay(1000);
  display.clearDisplay();
  display.setCursor(0, 0);
  display.print("sensor de ambiente");

  display.display();
  sensorAgua.begin();
  if (! aht.begin()) {
    delay(1000);
    display.print(" no encontrado");

    display.display();
    Serial.println("no hay sensor de ambiente");
    return false;
  }
  Serial.println("AHT10 found");
  display.print(" encontrado");
  display.display();
  delay(1000);
  display.clearDisplay();
  display.setCursor(0, 0);
  display.print("sensor de agua");
  display.display();
  sensorAgua.requestTemperatures(); // Send the command to get temperatures
  tempC = sensorAgua.getTempCByIndex(0);
  if (tempC == DEVICE_DISCONNECTED_C)
  {
    delay(1000);
    display.print(" no encontrado");
    Serial.println("No hay Sensor de Agua");

    display.display();
    return false;
  }
  display.print(" encontrado");
  display.display();

  Wire.requestFrom(8, 6);    // request 6 bytes from slave device #8
  valorPH = "";
  while (Wire.available()) { // slave may send less than requested
    valorPH += char(Wire.read()); // receive a byte as character
  }
  Serial.println("lectura de verificacion pH");
  Serial.println(valorPH);
  if (valorPH == "0.000") {
    return false;
  }


  estados = 2;
  return true;
}

void leerSensorDePH() {
  Wire.requestFrom(8, 6);    // request 6 bytes from slave device #8
  valorPH = "";
  while (Wire.available()) { // slave may send less than requested
    valorPH += char(Wire.read()); // receive a byte as character
  }
  Serial.print("PH: ");
  Serial.println(valorPH);

}


void leerSensorDeAmbiente() {

  aht.getEvent(&humidity, &temp);// populate temp and humidity objects with fresh data
  Serial.print("tAmb: "); Serial.print(temp.temperature); Serial.println(" C");
  Serial.print("HAmb: "); Serial.print(humidity.relative_humidity); Serial.println("% Hr");
}

void leerSensorDeAgua() {
  sensorAgua.requestTemperatures(); // Send the command to get temperatures
  tempC = sensorAgua.getTempCByIndex(0);
  // Check if reading was successful
  if (tempC != DEVICE_DISCONNECTED_C)
  {
    Serial.print("Temperatura del agua ");
    Serial.println(tempC);
  }
  else
  {
    Serial.println("no se pudo leer el sensor de agua");
  }
}

void idle() {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.print("presione A para verificar sensores");
  display.display();
  if (!digitalRead(botonA)) {

    delay(300);
    estados = 1;
  }
}
void idleVerificado() {

  display.clearDisplay();
  display.setCursor(0, 0);
  display.print("presione A para enviar");
  display.print("presione B para Guardar");
  display.display();
  if (!digitalRead(botonA)) {
    delay(300);
    estados = 3;
  } if (!digitalRead(botonB)) {
    delay(300);
    estados = 4;
  }
}



void callback(char* topic, byte* payload, unsigned int length) {

}


boolean reconnect() {
  String clientId = "ESP8266Client-";
  clientId += String(random(0xffff), HEX);
  // Attempt to connect
  if (client.connect(clientId.c_str())) {
    // Once connected, publish an announcement...
    client.publish(TOPICOCONEXION, "0");
    // ... and resubscribe
  }
  return client.connected();
}


void enviarInformacion() {
  customString("presione B para regresar");
  if (!client.connected()) {
    long now = millis();
    if (now - lastReconnectAttempt > 5000) {
      lastReconnectAttempt = now;
      // Attempt to reconnect
      if (reconnect()) {
        lastReconnectAttempt = 0;
      }
    }
  } else {
    leerSensorDeAmbiente();
    leerSensorDeAgua();
    leerSensorDePH();
    unsigned long now = millis();
    if (now - lastMsg > TIEMPOENVIOINFORMACION) {
      lastMsg = now;
      snprintf (msg, BUFFERDELMENSAJE, "%d,%d,%d,%d",
                int(tempC), int( temp.temperature),
                int(humidity.relative_humidity), valorPH.toInt()); //temperatura del agua, temperatura del ambiente, humedad del ambiente, ph del agua
      Serial.print("Publish message: ");
      Serial.println(msg);
      client.publish(TOPICODATOS, msg);

      client.loop();
    }
  }


  if (!digitalRead(botonB)) {
    delay(300);
    estados = 2;
  }
}

void guardarInformacion() {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.print("guardando informacion");
  display.print("presione B para regresar");
  display.display();
  if (!digitalRead(botonB)) {
    delay(300);
    estados = 2;
  }
}

void setup() {
  delay(2000);

  Serial.begin(115200);
  verificarPantalla();
  pinMode(botonA, INPUT_PULLUP);
  pinMode(botonB, INPUT_PULLUP);

  client.setServer(MQTTSERVER, 1883);
  client.setCallback(callback);
}
void loop() {
  switch (estados) {

    case 0://idle
      idle();
      break;
    case 1:
      verificarSensores();
      break;
    case 2:
      idleVerificado();
      break;
    case 3:
      configurarWifi();
      break;
    case 4:
      enviarInformacion();
      break;
    case 5:
      guardarInformacion();
      break;

  }

}
