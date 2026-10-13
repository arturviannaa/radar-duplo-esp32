// Monitor de radar duplo: dois HC-SR04 -> barras de proximidade numa TFT ST7789.
// Opcionalmente publica as distancias por MQTT para o IoT Agent JSON do FIWARE.
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "secrets.h"

// --- sensores ---
const int trigPin1 = 23;   // sensor da direita
const int echoPin1 = 22;
const int trigPin2 = 19;   // sensor da esquerda
const int echoPin2 = 18;

// --- display (HSPI) ---
#define TFT_CS   15
#define TFT_DC   2
#define TFT_RST  4
#define TFT_MOSI 13
#define TFT_CLK  14

// --- calibragem ---
// VEL_SOM varia com a temperatura (~0.0343 a 20 C, ~0.0331 a 0 C). Se as
// leituras vierem consistentemente longas ou curtas, ajuste aqui.
const float VEL_SOM = 0.034;                  // cm por microssegundo
const int   DIST_MAX = 100;                   // cm: fim de escala da barra
const unsigned long ECHO_TIMEOUT = 25000UL;   // us (~4 m ida e volta)
const int LIMITE_VERMELHO = 20;               // cm
const int LIMITE_AMARELO  = 50;               // cm

// --- MQTT ---
// Intervalo entre publicacoes. O loop le/desenha varias vezes por segundo,
// mas o broker nao precisa receber tudo isso: 1 s basta para o Orion.
const unsigned long MQTT_INTERVALO_MS = 1000;

SPIClass hspi(HSPI);
Adafruit_ST7789 tft = Adafruit_ST7789(&hspi, TFT_CS, TFT_DC, TFT_RST);

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

// Topico do IoT Agent JSON (sintaxe fixada pelo agente):
//   /json/<API_KEY>/<DEVICE_ID>/attrs
String topicoAttrs = String("/json/") + FIWARE_APIKEY + "/" + FIWARE_DEVICE + "/attrs";

unsigned long ultimaPublicacao = 0;

void conectarWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("WiFi: conectando a ");
  Serial.print(WIFI_SSID);
  // Timeout curto: se a rede nao estiver disponivel, seguimos offline
  // e o display continua funcionando normalmente.
  unsigned long inicio = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - inicio < 10000) {
    delay(250);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print(" ok, IP ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println(" falhou (segue offline)");
  }
}

void garantirMqtt() {
  if (WiFi.status() != WL_CONNECTED) return;
  if (mqtt.connected()) return;

  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  String clientId = String("esp32-") + FIWARE_DEVICE;
  if (mqtt.connect(clientId.c_str())) {
    Serial.print("MQTT: conectado em ");
    Serial.print(MQTT_HOST);
    Serial.print(" como ");
    Serial.println(clientId);
  } else {
    Serial.print("MQTT: falha rc=");
    Serial.println(mqtt.state());
  }
}

void publicarDistancias(long esq, long dir) {
  if (!mqtt.connected()) return;
  // Nomes curtos (d1/d2) sao os "object_id" do provisionamento no IoT Agent;
  // la viram atributos "distanciaEsquerda" / "distanciaDireita" no Orion.
  char payload[64];
  snprintf(payload, sizeof(payload), "{\"d1\":%ld,\"d2\":%ld}", esq, dir);
  mqtt.publish(topicoAttrs.c_str(), payload);
}

void setup() {
  Serial.begin(115200);

  pinMode(trigPin1, OUTPUT);
  pinMode(echoPin1, INPUT);
  pinMode(trigPin2, OUTPUT);
  pinMode(echoPin2, INPUT);

  hspi.begin(TFT_CLK, -1, TFT_MOSI, TFT_CS);
  tft.init(240, 320);
  tft.setRotation(3);
  tft.invertDisplay(false);   // se as cores sairem invertidas, troque para true
  tft.fillScreen(ST77XX_BLACK);

  // moldura estatica: desenhada uma vez, o loop so repinta as barras
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(55, 15);
  tft.println("MONITOR DE RADAR");
  tft.drawLine(0, 40, 320, 40, ST77XX_WHITE);

  tft.setCursor(15, 65);
  tft.print("ESQ:");
  tft.setCursor(15, 155);
  tft.print("DIR:");

  conectarWiFi();
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
}

long lerDistancia(int pinoTrig, int pinoEcho) {
  digitalWrite(pinoTrig, LOW);
  delayMicroseconds(2);
  digitalWrite(pinoTrig, HIGH);
  delayMicroseconds(10);
  digitalWrite(pinoTrig, LOW);

  long duracao = pulseIn(pinoEcho, HIGH, ECHO_TIMEOUT);
  // sem eco dentro do timeout = nada dentro do alcance, nao "obstaculo a 0 cm"
  if (duracao == 0) return DIST_MAX;
  return duracao * VEL_SOM / 2;
}

// Repinta so a barra e o numero: evita o flicker de limpar a tela inteira.
void desenharBarra(int x, int y, int distancia) {
  if (distancia > DIST_MAX) distancia = DIST_MAX;

  // quanto mais perto o obstaculo, mais comprida a barra
  int larguraBarra = map(distancia, 0, DIST_MAX, 200, 0);

  uint16_t corBarra;
  if (distancia < LIMITE_VERMELHO) {
    corBarra = ST77XX_RED;
  } else if (distancia < LIMITE_AMARELO) {
    corBarra = ST77XX_YELLOW;
  } else {
    corBarra = ST77XX_GREEN;
  }

  tft.fillRect(x, y, larguraBarra, 30, corBarra);
  tft.fillRect(x + larguraBarra, y, 200 - larguraBarra, 30, ST77XX_BLACK);

  tft.fillRect(x, y + 40, 100, 20, ST77XX_BLACK);
  tft.setTextSize(2);
  tft.setTextColor(corBarra);
  tft.setCursor(x, y + 40);
  tft.print(distancia);
  tft.print(" cm");
}

void loop() {
  long distEsquerda = lerDistancia(trigPin2, echoPin2);
  delay(50);   // deixa o eco dissipar antes de disparar o outro sensor
  long distDireita = lerDistancia(trigPin1, echoPin1);

  desenharBarra(80, 55, distEsquerda);
  desenharBarra(80, 145, distDireita);

  garantirMqtt();
  mqtt.loop();

  unsigned long agora = millis();
  if (agora - ultimaPublicacao >= MQTT_INTERVALO_MS) {
    publicarDistancias(distEsquerda, distDireita);
    ultimaPublicacao = agora;
  }

  delay(100);
}
