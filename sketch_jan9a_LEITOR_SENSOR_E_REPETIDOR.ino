#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <time.h>
#include <EEPROM.h>
#include <ArduinoOTA.h>

const char* SSID = "VALDIR_2G";
const char* PASSWORD = "1020304050";

const char* googleScriptURL = "https://script.google.com/macros/s/AKfycbzlyOlhIX4A6wOj4qG-pD5FBrHEJOYeMkPd9z5lm-XutdDxgkUTTdnL4AJtrVRUFtcw/exec";

const int pinoSensor = 5;
const char* idSensor = "ALB603";

WiFiClientSecure client;
HTTPClient http;
bool estadoAnterior = HIGH;

float valorInicial = 4998.0;
const float limite = 1000.0;
float proximoEnvio = (valorInicial / limite) * limite + limite;

unsigned long previousMillis = 0;
const long interval = 50;

void conectarWiFi();
void configurarOTA();
void enviarDadosParaGoogleScript(const char* dataHora, const char* id, const char* valor);
void salvarDadosLocalmente(const char* dataHora, const char* id, const char* valor);
void enviarDadosSalvos();
String obterDataHora();
String urlEncode(const char* msg);

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  EEPROM.begin(512);
  pinMode(pinoSensor, INPUT);
  WiFi.setSleep(WIFI_PS_NONE);
  WiFi.setAutoReconnect(true);
  conectarWiFi();
  configurarOTA();
}

void loop() {
  ArduinoOTA.handle();

  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    if (WiFi.status() == WL_CONNECTED) {
      bool estadoAtual = digitalRead(pinoSensor);

      if (estadoAtual == LOW && estadoAnterior == HIGH) {
        valorInicial += 1; // Incrementa contador
        Serial.println("Pulso detectado! Valor atual: " + String(valorInicial));

        if ((int(valorInicial) % (int)limite) == 0) {
          Serial.println("Enviando dados... Valor medido: " + String(valorInicial / 1000.0) + " m³");
          enviarDadosParaGoogleScript(obterDataHora().c_str(), idSensor, "LOW");
        }
        estadoAnterior = LOW;
      } else if (estadoAtual == HIGH && estadoAnterior == LOW) {
        estadoAnterior = HIGH;
      }
    }
  }
}

void conectarWiFi() {
  WiFi.begin(SSID, PASSWORD);
  int tentativas = 0;
  while (WiFi.status() != WL_CONNECTED && tentativas < 30) {
    Serial.print(".");
    delay(1000);
    tentativas++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi conectado");
    configTime(0, 0, "pool.ntp.org");

    // Configura o ESP32 como repetidor WiFi
    WiFi.softAP("LEITORES_REEDSWITCH", "Cram250102024");
    Serial.println("Repetidor WiFi configurado com sucesso");
  } else {
    Serial.println("\nErro ao conectar WiFi");
  }
}

void configurarOTA() {
  ArduinoOTA.setHostname("ESP32-OTA");
  ArduinoOTA.setPassword("OTA_password");
  ArduinoOTA.begin();
}

void enviarDadosParaGoogleScript(const char* dataHora, const char* id, const char* valor) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Erro: WiFi desconectado.");
    return;
  }

  String url = String(googleScriptURL) + "?dataHora=" + urlEncode(dataHora) + "&id=" + urlEncode(id) + "&valor=" + urlEncode(valor);
  Serial.println("URL gerada: " + url); // Log da URL gerada
  Serial.println("DataHora: " + String(dataHora)); // Log dos parâmetros individuais
  Serial.println("ID: " + String(id));
  Serial.println("Valor: " + String(valor));

  client.setInsecure();
  http.begin(client, url);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setTimeout(10000);
  int httpCode = http.GET();
  if (httpCode > 0) {
    Serial.println("Código de resposta HTTP: " + String(httpCode));
    String response = http.getString();
    Serial.println("Resposta do servidor: " + response);
  } else {
    Serial.println("Erro ao enviar dados. Código de erro: " + String(httpCode));
  }
  http.end();
}

void salvarDadosLocalmente(const char* dataHora, const char* id, const char* valor) {
  EEPROM.write(0, 1);
  int dataHoraSize = strlen(dataHora);
  int idSize = strlen(id);
  int valorSize = strlen(valor);
  for (int i = 0; i < dataHoraSize && i < 20; i++) {
    EEPROM.write(1 + i, dataHora[i]);
  }
  for (int i = 0; i < idSize && i < 20; i++) {
    EEPROM.write(21 + i, id[i]);
  }
  for (int i = 0; i < valorSize && i < 20; i++) {
    EEPROM.write(41 + i, valor[i]);
  }
  EEPROM.commit();
}

void enviarDadosSalvos() {
  char dataHora[20], id[20], valor[20];
  for (int i = 0; i < 20; i++) {
    dataHora[i] = EEPROM.read(1 + i);
    id[i] = EEPROM.read(21 + i);
    valor[i] = EEPROM.read(41 + i);
  }
  enviarDadosParaGoogleScript(dataHora, id, valor);
}

String obterDataHora() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Falha ao obter a hora");
    return "00-00-00 00:00:00";
  }
  char buffer[20];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
  return String(buffer);
}

String urlEncode(const char* msg) {
  String encodedMsg = "";
  while (*msg != '\0') {
    if (('a' <= *msg && *msg <= 'z') || ('A' <= *msg && *msg <= 'Z') || ('0' <= *msg && *msg <= '9')) {
      encodedMsg += *msg;
    } else {
      encodedMsg += '%';
      encodedMsg += String(*msg, HEX);
    }
    msg++;
  }
  return encodedMsg;
}

