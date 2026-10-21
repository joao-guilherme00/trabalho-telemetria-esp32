/*
 * TRABALHO PRÁTICO: DETECÇÃO DE ERRO EM TELEMETRIA SOB RUÍDO REAL
 * ALUNO C: GERADOR DE RUÍDO (NOISE)
 */

#include <Arduino.h>

const int PIN_TRANSISTOR = 25; // Base do transistor de injeção
const int PIN_ESCUTA_TX  = 26; // Pino de escuta conectado na linha de dados
int modo_ruido = 0;       
unsigned long frameCount = 0;
const float TEMPO_BIT = 8.68;  // Tempo teórico de 1 bit a 115200 baud

// Aguarda microssegundos via timer de hardware nativo do ESP32 (Seção 3 - Proibido delayMicroseconds)
void aguardarMicros(uint32_t us) {
    uint64_t start = esp_timer_get_time();
    while ((esp_timer_get_time() - start) < us) { /* Aguarda via hardware */ }
}

void forcarBitZero(uint32_t microsegundos) {
    digitalWrite(PIN_TRANSISTOR, HIGH);
    aguardarMicros(microsegundos); 
    digitalWrite(PIN_TRANSISTOR, LOW);
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_TRANSISTOR, OUTPUT);
  digitalWrite(PIN_TRANSISTOR, LOW);
  pinMode(PIN_ESCUTA_TX, INPUT); 
  Serial.println("--- INJETOR DE RUIDO (ALUNO C) PRONTO ---");
}

void loop() {
  if (Serial.available() > 0) {
    char ch = Serial.read();
    if (ch >= '0' && ch <= '3') {
      modo_ruido = ch - '0';
      Serial.printf("[MODO ALTERADO] Ruido configurado para o Modo: %d\n", modo_ruido);
    }
  }

  if (modo_ruido == 0) return;

  // Sincroniza pelo Start Bit do quadro (Borda de descida na linha)
  if (digitalRead(PIN_ESCUTA_TX) == LOW) {
    frameCount++;
    uint64_t timestamp_injecao = esp_timer_get_time();
    int bit_escolhido1 = 0, bit_escolhido2 = 0;
    bool mudou_bit = true;

    if (modo_ruido == 1) {
      bit_escolhido1 = random(20, 36); // Inversão em posição aleatória dentro do quadro
      aguardarMicros(bit_escolhido1 * TEMPO_BIT);
      mudou_bit = (digitalRead(PIN_ESCUTA_TX) == HIGH); // Verifica se o bit original era 1 (Seção 1)
      forcarBitZero(9);
      Serial.printf("Quadro: %lu | Modo: 1 | Posicao Bit: %d | Timestamp: %llu us | Mudou: %s\n", 
                    frameCount, bit_escolhido1, timestamp_injecao, mudou_bit ? "SIM" : "NAO");
    } 
    else if (modo_ruido == 2) {
      bit_escolhido1 = random(15, 25); bit_escolhido2 = random(26, 38);
      // Primeiro tiro de ruído
      aguardarMicros(bit_escolhido1 * TEMPO_BIT);
      bool mudou1 = (digitalRead(PIN_ESCUTA_TX) == HIGH); forcarBitZero(9);
      // Segundo tiro de ruído
      aguardarMicros((bit_escolhido2 - bit_escolhido1 - 1) * TEMPO_BIT);
      bool mudou2 = (digitalRead(PIN_ESCUTA_TX) == HIGH); forcarBitZero(9);
      
      Serial.printf("Quadro: %lu | Modo: 2 | Posicao Bits: %d e %d | Timestamp: %llu us | Mudou: %s\n", 
                    frameCount, bit_escolhido1, bit_escolhido2, timestamp_injecao, (mudou1 || mudou2) ? "SIM" : "NAO");
    } 
    else if (modo_ruido == 3) {
      int bits_rajada = random(3, 9); bit_escolhido1 = random(10, 25);
      aguardarMicros(bit_escolhido1 * TEMPO_BIT);
      forcarBitZero(bits_rajada * TEMPO_BIT); // Rajada contígua que corrompe múltiplos bits
      Serial.printf("Quadro: %lu | Modo: 3 | Rajada de %d bits (Bit %d) | Timestamp: %llu us | Mudou: SIM\n", 
                    frameCount, bits_rajada, bit_escolhido1, timestamp_injecao);
    }

    while (digitalRead(PIN_ESCUTA_TX) == LOW) { aguardarMicros(1000); } // Evita auto-feedback
    delay(90); // Aguarda o fim total da passagem do pacote atual
  }
}
