#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

/*
  =========================================================
   Projeto: Bambu P1S – Luz Externa Sincronizada por MQTT
   Hardware: ESP32 + MOSFET + fita LED
   Comunicação: MQTT over TLS (LAN)
  =========================================================
*/

// ==== CONFIGURAÇÃO ====

// Credenciais WiFi
const char* WIFI_SSID     = "";                // Your wifi network
const char* WIFI_PASSWORD = "";                // your wifi network password

// Dados da impressora Bambu
const char* PRINTER_IP      = ""; // Printer IP
const int   PRINTER_PORT    = 8883;           // MQTT PORT
const char* PRINTER_USER    = "bblp";         // no change - STATIC
const char* ACCESS_CODE     = "";             // LAN Access Code (From printer)
const char* PRINTER_SERIAL  = "";             // Serial number (From printer)

// Tópico MQTT onde a impressora publica o estado
String topicReport = String("device/") + PRINTER_SERIAL + "/report";

// Pino que controla o MOSFET (fita LED externa)
const int LED_CTRL_PIN = D5;

// Pino do LED de estado (status do sistema)
int STATUS_CTRL_PIN = D6;

// Ativar/desativar mensagens de debug
bool DEBUG = false;

// Guarda o último estado da luz para evitar ações repetidas
String lastLightMode = "";

// Clientes WiFi e MQTT
WiFiClientSecure espClient;
PubSubClient mqttClient(espClient);

// =========================================================
// CALLBACK MQTT
// É chamado sempre que chega uma mensagem MQTT
// =========================================================
void mqttCallback(char* topic, byte* payload, unsigned int length) {

  // Converter payload (bytes) numa String
  String jsonStr;
  jsonStr.reserve(length + 1);
  for (unsigned int i = 0; i < length; i++) {
    jsonStr += (char)payload[i];
  }

  // Debug opcional: mostra a mensagem MQTT completa
  if (DEBUG) {
    // Debug bruto
    Serial.println("=== MQTT message ===");
    Serial.print("Topic: ");
    Serial.println(topic);
    Serial.println(jsonStr);
  }

  // Documento JSON (tamanho grande para mensagens da Bambu)
  StaticJsonDocument<4096> doc;
  DeserializationError err = deserializeJson(doc, jsonStr);
  if (err) {
    Serial.print("Erro JSON: ");
    Serial.println(err.c_str());
    return;
  }

  // A informação relevante está dentro do objeto "print"
  JsonVariant print = doc["print"];
  if (print.isNull()) return;
  
  // Dentro de "print", existe o array "lights_report"
  JsonArray lights = print["lights_report"].as<JsonArray>();
  if (lights.isNull()) return;


  // Procurar especificamente o estado da "chamber_light"
  String chamberMode = "";
  for (JsonObject l : lights) {
    const char* node = l["node"];
    const char* mode = l["mode"];
    
    // Estamos interessados apenas na luz interna da câmara
    if (node && mode && String(node) == "chamber_light") {
      chamberMode = String(mode);
      break;
    }
  }

  if (chamberMode == "") return;

  // Só agir se o estado mudou (evita animações repetidas)
  if (chamberMode != lastLightMode) {
    lastLightMode = chamberMode;
    Serial.print("Luz interna (chamber_light): ");
    Serial.println(chamberMode);

    if (chamberMode == "on") {
      // Liga a fita LED externa com fade-in
      animateStripOn();       // fade in
      Serial.println(">> FITA: LIGADA");
    } else {
      // Desliga a fita LED externa com fade-out
      animateStripOff();     // fade out
      Serial.println(">> FITA: DESLIGADA");
    }
  }
}
// =========================================================
// FUNÇÃO DE LIGAÇÃO AO MQTT DA BAMBU
// =========================================================

void connectMQTT() {
  while (!mqttClient.connected()) {
    Serial.print("A ligar ao MQTT da Bambu... ");

    // Client ID único baseado no MAC do ESP32
    String clientId = "ESP32-Bambu-";
    clientId += String((uint32_t)ESP.getEfuseMac(), HEX);

    // Autenticação MQTT com user fixo + access code
    if (mqttClient.connect(clientId.c_str(), PRINTER_USER, ACCESS_CODE)) {
      Serial.println("Ligado!");
      mqttClient.subscribe(topicReport.c_str());
      Serial.print("Subscrito em: ");
      Serial.println(topicReport);
    } else {
      Serial.print("falhou, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" ; nova tentativa em 5s");
      delay(5000);
    }
  }
}

// =========================================================
// ANIMAÇÕES DA FITA LED
// =========================================================

// Fade-in suave
void animateStripOn() {
  Serial.println("Animacao: ligar fita");
  for (int b = 0; b <= 255; b++) {
    analogWrite(LED_CTRL_PIN, b);  // PWM 0..255
    delay(5);                      // velocidade do fade
  }
}

// Fade-out suave
void animateStripOff() {
  Serial.println("Animacao: desligar fita");
  for (int b = 255; b >= 0; b--) {
    analogWrite(LED_CTRL_PIN, b);
    delay(5);
  }
}

// =========================================================
// SETUP
// =========================================================
void setup() {
  Serial.begin(115200);
  delay(1000);

  // Configuração dos pinos
  pinMode(LED_CTRL_PIN, OUTPUT);
  pinMode(STATUS_CTRL_PIN, OUTPUT);
  digitalWrite (STATUS_CTRL_PIN, LOW);
  analogWrite(LED_CTRL_PIN, 0);
  
  Serial.println();
  Serial.println("Bambu MQTT - Luz interna");

  // Ligação WiFi
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(); // garante estado limpo
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  WiFi.setTxPower(WIFI_POWER_8_5dBm);

  Serial.print("A ligar ao WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    // Pisca o LED de status enquanto liga
    digitalWrite (STATUS_CTRL_PIN, HIGH);
    delay(300);
    digitalWrite (STATUS_CTRL_PIN, LOW);
    delay(300);
    Serial.print(".");
  }

  Serial.println();
  digitalWrite (STATUS_CTRL_PIN, HIGH);
  Serial.print("WiFi ligado. IP: ");
  Serial.println(WiFi.localIP());

  // Esperar alguns segundos para deixar a impressora assentar e 
  // caso o LED esteja aceso, acender também 
  delay (10000);

  // TLS sem verificar certificado (simples em LAN)
  espClient.setInsecure();
  mqttClient.setServer(PRINTER_IP, PRINTER_PORT);
  mqttClient.setCallback(mqttCallback);
  
  connectMQTT();
}

// =========================================================
// LOOP PRINCIPAL
// =========================================================
void loop() {
  // Garante ligação MQTT ativa
  if (!mqttClient.connected()) {
    connectMQTT();
  }
  
  // Processa mensagens MQTT
  mqttClient.loop();
}
