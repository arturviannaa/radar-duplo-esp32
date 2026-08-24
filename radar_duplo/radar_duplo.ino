// Monitor de radar duplo: dois HC-SR04 -> barras de proximidade numa TFT ST7789.
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>

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

SPIClass hspi(HSPI);
Adafruit_ST7789 tft = Adafruit_ST7789(&hspi, TFT_CS, TFT_DC, TFT_RST);

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

  delay(100);
}
