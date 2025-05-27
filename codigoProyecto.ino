#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <HTTPClient.h>

// === CONFIGURACIÓN DE WIFI Y THINGSPEAK ===
const char* ssid = "INTERNET";
const char* password = "Fer2024*";
const char* apiKey = "63Z8IIPAGN11XXYF";
const char* server = "http://api.thingspeak.com/update";

// === CONFIGURACIÓN DE OLED ===
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_ADDR 0x3C
#define SDA_PIN 21
#define SCL_PIN 22

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
bool displayInitialized = false;

// === PINS ANALÓGICOS ===
const int PIN_SENSOR_V = 34;
const int PIN_SENSOR_I = 35;

// === CONSTANTES DE CALIBRACIÓN ===
const float VOLT_REF = 3.3;       // Voltaje de referencia del ADC

// === CALIBRACIÓN AUTOMÁTICA DE VOLTAJE ===
// Puntos de calibración conocidos (ADC, Voltaje real)
const float ADC_VOLT_LOW = 0.886;   // Lectura ADC para 5.1V
const float REAL_VOLT_LOW = 5.1;    // Voltaje real correspondiente
const float ADC_VOLT_HIGH = 2.1;    // Lectura ADC para 12V (estimado)
const float REAL_VOLT_HIGH = 12.0;  // Voltaje real correspondiente

// === CALIBRACIÓN DE CORRIENTE ===
const float CURRENT_OFFSET = 1.65;     // Offset de corriente
const float CURRENT_SENSITIVITY = 0.066; // Sensibilidad del sensor
const float CURRENT_SCALE_FACTOR = 0.001; // Factor para convertir de mA a mA

// === FUNCIÓN DE MAPEO DE VOLTAJE ===
float mapVoltage(float adcVoltage) {
  // Calcular pendiente y punto de corte para la ecuación lineal
  float slope = (REAL_VOLT_HIGH - REAL_VOLT_LOW) / (ADC_VOLT_HIGH - ADC_VOLT_LOW);
  float intercept = REAL_VOLT_LOW - (slope * ADC_VOLT_LOW);
  
  // Aplicar la ecuación lineal
  return (slope * adcVoltage) + intercept;
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n\n=== SISTEMA DE MONITOREO INICIADO ===");
  Serial.println("Calibración automática activada");

  // Inicializar I2C
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(100000);

  // Verificar pantalla OLED
  if (display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    displayInitialized = true;
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("OLED Iniciada");
    display.display();
    delay(2000);
  } else {
    Serial.println("Fallo al iniciar la pantalla OLED.");
  }

  // Conectar WiFi
  Serial.print("Conectando a WiFi");
  WiFi.begin(ssid, password);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  Serial.println();

  // Mostrar estado de WiFi
  if (displayInitialized) {
    display.clearDisplay();
    display.setCursor(0, 0);
    if (WiFi.status() == WL_CONNECTED) {
      display.println("WiFi Conectado!");
      display.setCursor(0, 10);
      display.println(WiFi.localIP());
      Serial.println("WiFi Conectado!");
      Serial.print("IP: ");
      Serial.println(WiFi.localIP());
    } else {
      display.println("WiFi Falló!");
      Serial.println("WiFi Falló!");
    }
    display.display();
    delay(3000);
  }
}

void loop() {
  // Reconexión si se pierde WiFi
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.begin(ssid, password);
    delay(1000);
  }

  // === LECTURA DE SENSORES ===
  // Medir voltaje
  int lecturaADC_V = analogRead(PIN_SENSOR_V);
  float voltajeADC = (lecturaADC_V * VOLT_REF / 4095.0);
  float voltaje = mapVoltage(voltajeADC);

  // Medir corriente
  int lecturaADC_I = analogRead(PIN_SENSOR_I);
  float voltajeADC_I = (lecturaADC_I * VOLT_REF / 4095.0);
  float corriente_A = (voltajeADC_I - CURRENT_OFFSET) / CURRENT_SENSITIVITY;
  float corriente_mA = abs(corriente_A * 1000.0) * CURRENT_SCALE_FACTOR;

  // Calcular la potencia en mW
  float potencia_mW = voltaje * corriente_mA;

  // === MOSTRAR DATOS EN EL MONITOR SERIE ===
  Serial.print("Voltaje: ");
  Serial.print(voltaje, 2);
  Serial.print(" V, Corriente: ");
  Serial.print(corriente_mA, 2);
  Serial.print(" mA, Potencia: ");
  Serial.print(potencia_mW, 2);
  Serial.println(" mW");

  // === MOSTRAR VOLTAJE EN OLED ===
  if (displayInitialized) {
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(0, 10);
    display.print("V: ");
    display.print(voltaje, 1);
    display.print(" V");
    display.display();
    delay(1000);
  }

  // === MOSTRAR CORRIENTE EN OLED ===
  if (displayInitialized) {
    display.clearDisplay();
    display.setCursor(0, 10);
    display.print("I: ");
    display.print(corriente_mA, 1);
    display.print(" mA");
    display.display();
    delay(1000);
  }

  // === MOSTRAR POTENCIA EN OLED ===
  if (displayInitialized) {
    display.clearDisplay();
    display.setCursor(0, 10);
    display.print("P: ");
    display.print(potencia_mW, 1);
    display.print(" mW");
    display.display();
    delay(1000);
  }

  // === ENVÍO A THINGSPEAK ===
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = String(server) + "?api_key=" + apiKey +
                 "&field1=" + String(voltaje) +
                 "&field2=" + String(corriente_mA) +
                 "&field3=" + String(potencia_mW);
    http.begin(url);
    int httpCode = http.GET();
    http.end();
  }

  delay(12000); // espera antes del siguiente ciclo
}