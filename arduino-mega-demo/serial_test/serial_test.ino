const unsigned long INTERVALO_MS = 1000;
unsigned long ultimo = 0;
unsigned long contador = 0;

void setup() {
  Serial.begin(9600);
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.println();
  Serial.println("READY arduino-mega serial-test v1");
}

void loop() {
  unsigned long agora = millis();
  if (agora - ultimo >= INTERVALO_MS) {
    contador++;
    Serial.print("HB ");
    Serial.print(contador);
    Serial.print(" uptime_ms=");
    Serial.println(agora);
    digitalWrite(LED_BUILTIN, contador & 1);
    ultimo = agora;
  }

  if (Serial.available()) {
    String linha = Serial.readStringUntil('\n');
    linha.trim();
    Serial.print("ECO: ");
    Serial.println(linha);
  }
}