/* 
 * TRABALHO PRÁTICO: DETECÇÃO DE ERRO EM TELEMETRIA SOB RUÍDO REAL
 * ALUNO B: RECEPTOR (RX)
 */

#include <Arduino.h>

HardwareSerial SerialRX(2);

// CONFIGURAÇÃO DA RODADA DE TESTES (DEVE SER IGUAL À CONFIGURAÇÃO DO TX)
const bool USAR_CRC = false; 

const uint8_t MARCADOR_INICIO = 0xAA;
const uint8_t MARCADOR_FIM    = 0xFF;

unsigned long timeout_limite = 150; // Janela de timeout de 150ms (Seção 2-C)
unsigned long tempo_ultimo_pacote = 0;
uint8_t seq_esperada = 0;

int ok = 0, corrompido = 0, perdido = 0, nao_detectado = 0;
int total_processado = 0;

bool validarChecksum(uint8_t seq, uint8_t d1, uint8_t d2, uint8_t erro_rec) {
  int soma = seq + d1 + d2;
  uint8_t calculo = ~(soma & 0xFF);
  return calculo == erro_rec;
}

bool validarCRC8(uint8_t seq, uint8_t d1, uint8_t d2, uint8_t crc_recebido) {
  uint8_t dados[] = { seq, d1, d2 };
  uint8_t crc = 0x00;
  for (int i = 0; i < 3; i++) {
    crc ^= dados[i];
    for (int bit = 0; bit < 8; bit++) {
      if (crc & 0x80) crc = (crc << 1) ^ 0x07;
      else crc <<= 1;
    }
  }
  return crc == crc_recebido;
}

void setup() {
  Serial.begin(115200);
  SerialRX.begin(115200, SERIAL_8N1, 16, 17); // Pino 16 conectado na linha de escuta
  Serial.println("sequencia,residuo,decisao,payload_confere,tempo_verificacao_us"); // Formato CSV Limpo
  tempo_ultimo_pacote = millis();
}

void loop() {
  // Interrompe o log e exibe as estatísticas finais ao atingir 1000 quadros
  if (total_processado >= 1000) {
    static bool exibiu_resumo = false;
    if (!exibiu_resumo) {
      Serial.println("\n--- EXPERIMENTO CONCLUÍDO (1000 QUADROS) ---");
      Serial.printf("OK: %d | CORROMPIDO: %d | PERDIDO: %d | NAO DETECTADO: %d\n", ok, corrompido, perdido, nao_detectado);
      float taxa = (corrompido + nao_detectado > 0) ? ((float)nao_detectado / (corrompido + nao_detectado)) * 100.0 : 0.0;
      Serial.printf("Taxa de Nao Deteccao: %.2f%%\n", taxa);
      exibiu_resumo = true;
    }
    delay(1000);
    return; 
  }

  // Verificação de Timeout do Canal (Seção 2 - Item C)
  if (millis() - tempo_ultimo_pacote > timeout_limite) {
    perdido++; total_processado++;
    Serial.printf("%d,0x00,PERDIDO,---,0\n", seq_esperada);
    seq_esperada++;
    tempo_ultimo_pacote = millis();
  }

  if (SerialRX.available() >= 6) {
    if (SerialRX.read() == MARCADOR_INICIO) {
      tempo_ultimo_pacote = millis();
      uint8_t seq = SerialRX.read();
      uint8_t d1  = SerialRX.read();
      uint8_t d2  = SerialRX.read();
      uint8_t err = SerialRX.read();
      uint8_t fim = SerialRX.read();

      if (fim != MARCADOR_FIM) {
        perdido++; total_processado++;
        Serial.printf("%d,0x%02X,PERDIDO_ESTRUTURA,---,0\n", seq, err);
        return;
      }

      // Tratamento de Lacuna de Sequência (Seção 2 - Item B)
      if (seq != seq_esperada) {
        int lacuna = (seq >= seq_esperada) ? (seq - seq_esperada) : (256 - seq_esperada + seq);
        perdido += (lacuna - 1); total_processado += (lacuna - 1);
        seq_esperada = seq;
      }

      // Captura segura da Cópia de Depuração externa (Seção 4)
      unsigned long timeout_copia = micros();
      while (SerialRX.available() < 2 && (micros() - timeout_copia < 2000)) { delayMicroseconds(50); }
      uint8_t copia_d1 = 0, copia_d2 = 0;
      if (SerialRX.available() >= 2) { copia_d1 = SerialRX.read(); copia_d2 = SerialRX.read(); }

      bool checagem_passou = false;
      unsigned long tempo_inicial = micros();
      if (USAR_CRC) checagem_passou = validarCRC8(seq, d1, d2, err);
      else checagem_passou = validarChecksum(seq, d1, d2, err);
      unsigned long tempo_verificacao = micros() - tempo_inicial;

      bool payload_bateu = (d1 == copia_d1 && d2 == copia_d2);

      // Classificação das colunas do CSV (Seção 4)
      if (checagem_passou && payload_bateu) { ok++; Serial.printf("%d,0x%02X,ACEITO,SIM,%lu\n", seq, err, tempo_verificacao); }
      else if (!checagem_passou) { corrompido++; Serial.printf("%d,0x%02X,DESCARTADO,---,%lu\n", seq, err, tempo_verificacao); }
      else if (checagem_passou && !payload_bateu) { nao_detectado++; Serial.printf("%d,0x%02X,ACEITO,NAO,%lu\n", seq, err, tempo_verificacao); }

      total_processado++; seq_esperada++;
    }
  }
}
