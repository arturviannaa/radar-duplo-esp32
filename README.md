# Monitor de Radar Duplo — ESP32 + TFT ST7789

Sensor de estacionamento (radar) visual. Lê a distância de dois sensores
ultrassônicos independentes e desenha barras de proximidade dinâmicas e
coloridas numa tela TFT de 2.8". Opcionalmente publica as leituras por MQTT
para uma stack FIWARE (IoT Agent JSON + Orion Context Broker).

Projeto acadêmico, desenvolvido para fins de estudo.

## Integrantes

- Artur Figueiredo Vianna
- Cassiano Rocha Assumpção
- Pedro Perraro John

---

## Como funciona

O ESP32 dispara um pulso ultrassônico de 10 µs em cada sensor e mede, com
`pulseIn()`, quanto tempo o eco demora para voltar. Esse tempo de voo vira
distância pela velocidade do som (~0,034 cm/µs), dividida por dois — o som faz
o caminho de ida e volta.

Cada distância vira uma barra horizontal na tela: **quanto mais perto o
obstáculo, mais comprida a barra**. A cor é o alerta:

| Distância | Cor | Significado |
|-----------|-----|-------------|
| `< 20 cm` | 🔴 Vermelho | Perigo — pare |
| `20–49 cm` | 🟡 Amarelo | Atenção |
| `≥ 50 cm` | 🟢 Verde | Livre |

A moldura (título, linha divisória, rótulos `ESQ:` e `DIR:`) é desenhada uma
única vez no `setup()`. O `loop()` repinta apenas o retângulo da barra e o
número em centímetros, o que elimina o flicker de limpar a tela inteira a cada
ciclo.

---

## Componentes

| Componente | Função |
|------------|--------|
| **ESP32** (38 pinos, ESP32-D0WD-V3) | O cérebro. Processa o tempo de voo do som e renderiza a interface gráfica. |
| **HC-SR04 ×2** | Os olhos. Emitem ondas sonoras e leem os obstáculos da esquerda e da direita. |
| **TFT 2.8" ST7789** (240×320) | Traduz os números brutos em barras de alerta coloridas. |
| **Módulo HW-131** | Alimentação dedicada. Recebe 9 V da tomada, estabiliza para 5 V e alimenta a protoboard — protege a porta USB do PC e garante corrente de sobra para tela e sensores operarem juntos. |
| **Resistores 1 kΩ e 2 kΩ ×2** | Divisor de tensão em cada pino Echo (ver abaixo). |

---

## Esquema de ligação

O circuito usa uma protoboard principal e uma auxiliar menor, para organizar as
ligações e manter os divisores de tensão perto de cada sensor.

### Alimentação

- A **HW-131** fornece **5 V** e **GND** para as linhas da protoboard.
- O **ESP32 fornece 3.3 V** para a tela TFT.
- ⚠️ **O GND do ESP32 tem que estar ligado ao GND da protoboard.** Sem terra
  comum, o pulso de Echo não tem referência e as leituras saem sempre `0`,
  mesmo com tudo o mais correto.

### Proteção dos pinos Echo (obrigatório)

O HC-SR04 devolve **5 V** no pino Echo, mas os GPIOs do ESP32 toleram no
máximo **3.3 V**. Ligar direto degrada ou queima o pino. Para cada sensor:

```
Echo (5V) ──[ 1kΩ ]──┬── GPIO do ESP32  (~3.3 V)
                     │
                   [ 2kΩ ]
                     │
                    GND
```

Divisor de 1 kΩ / 2 kΩ: `5 V × 2k/(1k+2k) = 3,33 V`.

### Pinagem

**Sensor 1 — Direita** (protoboard principal)

| Sensor | ESP32 |
|--------|-------|
| VCC | 5 V (HW-131) |
| GND | GND comum |
| Trig | GPIO 23 |
| Echo | GPIO 22 *(via divisor)* |

**Sensor 2 — Esquerda** (protoboard auxiliar)

| Sensor | ESP32 |
|--------|-------|
| VCC | 5 V (HW-131) |
| GND | GND comum |
| Trig | GPIO 19 |
| Echo | GPIO 18 *(via divisor)* |

**Display TFT ST7789 — barramento HSPI**

| TFT | ESP32 |
|-----|-------|
| CS | GPIO 15 |
| RST | GPIO 4 |
| DC | GPIO 2 |
| MOSI / SDA | GPIO 13 |
| CLK / SCK | GPIO 14 |
| LED / BLK | 3.3 V (direto, luz de fundo sempre acesa) |
| VCC | 3.3 V |
| GND | GND comum |

> O display usa o **HSPI** (`SPIClass hspi(HSPI)`), o barramento SPI alternativo
> do ESP32, deixando o VSPI padrão livre.

---

## Dependências

Instale pela Library Manager da Arduino IDE, ou via `arduino-cli`:

```bash
arduino-cli core install esp32:esp32
arduino-cli lib install "Adafruit GFX Library"
arduino-cli lib install "Adafruit ST7735 and ST7789 Library"
arduino-cli lib install "PubSubClient"
```

> `PubSubClient` é o cliente MQTT. Se você não vai usar a parte FIWARE, pode
> instalar mesmo assim: o sketch continua funcionando offline (ver abaixo).

---

## Configuração local (WiFi + MQTT)

Antes de compilar, copie o template de credenciais e preencha com os valores
da sua rede e do seu backend FIWARE:

```bash
cp radar_duplo/secrets.h.example radar_duplo/secrets.h
```

Edite `radar_duplo/secrets.h`:

```c
#define WIFI_SSID       "sua-rede"
#define WIFI_PASSWORD   "sua-senha"
#define MQTT_HOST       "192.168.0.100"   // IP da maquina que roda o docker-compose
#define MQTT_PORT       1883
#define FIWARE_APIKEY   "radar123"
#define FIWARE_DEVICE   "radar001"
```

O arquivo `secrets.h` fica fora do git (listado no `.gitignore`), para não
vazar credenciais. Se a rede WiFi não estiver disponível no boot, o sketch
registra o erro no Serial e continua rodando offline — a tela segue
funcionando normalmente.

## Compilar e gravar

```bash
arduino-cli compile --fqbn esp32:esp32:esp32 radar_duplo
arduino-cli upload  --fqbn esp32:esp32:esp32 -p /dev/ttyUSB0 radar_duplo
```

Pela Arduino IDE: abra `radar_duplo/radar_duplo.ino`, selecione a placa
**ESP32 Dev Module** e a porta correspondente.

Pra debug, abra o Serial Monitor em **115200 baud** — é a taxa configurada em
`Serial.begin()` no sketch.

---

## Calibragem

Os valores ficam agrupados no topo do sketch, em `--- calibragem ---`:

| Constante | Padrão | O que faz |
|-----------|--------|-----------|
| `VEL_SOM` | `0.034` | Velocidade do som em cm/µs. Varia com a temperatura (~0,0343 a 20 °C, ~0,0331 a 0 °C). Se as leituras vierem consistentemente longas ou curtas, ajuste aqui. |
| `DIST_MAX` | `100` | Fim de escala da barra, em cm. Distâncias maiores saturam. |
| `ECHO_TIMEOUT` | `25000` | Timeout do `pulseIn()`, em µs (~4 m ida e volta). Sem ele, um sensor sem eco travaria o loop por 1 segundo. |
| `LIMITE_VERMELHO` | `20` | Abaixo disso, barra vermelha. |
| `LIMITE_AMARELO` | `50` | Abaixo disso, barra amarela. |

Ajuste `tft.invertDisplay(true)` se as cores do seu painel saírem invertidas —
é comum entre lotes diferentes de ST7789.

---

## Solução de problemas

| Sintoma | Causa provável |
|---------|----------------|
| Leituras sempre `0 cm` | GND do ESP32 não está ligado ao GND da protoboard. |
| Barra sempre no máximo (verde, `100 cm`) | Nenhum eco chegando: Echo desligado, divisor mal montado ou sensor sem 5 V. |
| Tela branca / apagada | Verifique pino LED em 3.3 V e as ligações de CS, DC e RST. |
| Cores invertidas | Troque `tft.invertDisplay(false)` por `true`. |
| Erro de upload `No more data to read from the serial port` | Outro processo está segurando `/dev/ttyUSB0`. Cheque com `fuser -v /dev/ttyUSB0` e feche o monitor serial. |
| Leituras instáveis com os dois sensores | Aumente o `delay(50)` entre as duas leituras, para o eco de um não ser captado pelo outro. |
| `MQTT: falha rc=-2` no Serial | O ESP32 não alcança o broker. Verifique se `MQTT_HOST` em `secrets.h` é o IP (não `localhost`) da máquina do Docker, e se ela está na mesma rede do ESP32. |
| Entidade não aparece no Orion | Confira se `./provision.sh` rodou sem erros e se o `FIWARE_APIKEY` do `secrets.h` bate com o do script. |

---

## Integração FIWARE + MQTT

A stack em `fiware/` sobe tudo que o ESP32 precisa para alimentar o
**Orion Context Broker** com as leituras dos radares, usando o
**IoT Agent JSON** como tradutor MQTT ⇄ NGSI.

```
ESP32 ──MQTT──> Mosquitto ──MQTT──> IoT Agent JSON ──NGSI──> Orion
                                                               │
                                                   consultas HTTP (REST)
```

### Subir a stack

Pré-requisito: Docker e Docker Compose. Numa máquina acessível pelo ESP32
(normalmente na mesma rede WiFi):

```bash
cd fiware
docker compose up -d
./provision.sh      # cria service group e provisiona o device "radar001"
```

Portas expostas:

| Porta | Serviço | Para quê |
|-------|---------|----------|
| `1883` | Mosquitto (MQTT) | O ESP32 publica aqui. |
| `1026` | Orion NGSI v2 | Consultar o estado atual dos radares. |
| `4041` | IoT Agent (admin) | Provisionar devices e service groups. |

### Mapeamento das leituras

O sketch publica no tópico `/json/<APIKEY>/<DEVICE>/attrs`:

```json
{"d1": 42, "d2": 87}
```

O IoT Agent (configurado em `provision.sh`) traduz `d1`/`d2` nos atributos
`distanciaEsquerda` e `distanciaDireita` da entidade NGSI
`urn:ngsi-ld:Radar:radar001`.

### Verificar que chegou no Orion

```bash
curl -s http://localhost:1026/v2/entities/urn:ngsi-ld:Radar:radar001 \
  -H 'fiware-service: radar' \
  -H 'fiware-servicepath: /' | jq
```

Resposta esperada (resumida):

```json
{
  "id": "urn:ngsi-ld:Radar:radar001",
  "type": "Radar",
  "distanciaEsquerda": { "type": "Number", "value": 42 },
  "distanciaDireita":  { "type": "Number", "value": 87 }
}
```

Para debugar o fluxo MQTT, escute o tópico com `mosquitto_sub`:

```bash
mosquitto_sub -h localhost -t '/json/radar123/radar001/attrs' -v
```

> ⚠️ A `mosquitto.conf` aceita conexões anônimas — é adequado para o
> laboratório mas não para produção. Para expor na internet, configure
> usuários, TLS e um firewall na frente do broker.

## Estrutura

```
radar-duplo-esp32/
├── radar_duplo/
│   ├── radar_duplo.ino      # sketch completo
│   └── secrets.h.example    # template de credenciais WiFi/MQTT
├── fiware/
│   ├── docker-compose.yml   # Orion + IoT Agent JSON + Mosquitto + MongoDB
│   ├── mosquitto.conf       # config do broker MQTT
│   └── provision.sh         # cria service group e provisiona o device
├── README.md
├── LICENSE
└── .gitignore
```

---

## Licença

MIT — veja [LICENSE](LICENSE).
