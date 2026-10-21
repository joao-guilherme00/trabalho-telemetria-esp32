/* 
 * TRABALHO PRÁTICO: DETECÇÃO DE ERRO EM TELEMETRIA SOB RUÍDO REAL
 * ALUNO A: TRANSMISSOR (TX) 
 */

#define SERIAL_PC Serial 
#define SERIAL_UART2 Serial2 

// CONFIGURAÇÃO DA RODADA DE TESTES
// Mude para false para a Rodada 1 (Checksum) | Mude para true para a Rodada 2 (CRC)
const bool USAR_CRC = false; 

const uint8_t MARCADOR_INICIO = 0xAA;
const uint8_t MARCADOR_FIM    = 0xFF;
int contador_geral = 0;

// Função da Rodada 1: Checksum (Soma Modular com complemento de 1)
uint8_t calcularChecksum(uint8_t seq, uint8_t dado1, uint8_t dado2) { 
    int soma = seq + dado1 + dado2; 
    return ~(soma & 0xFF);
}

// Função da Rodada 2: CRC-8 (Polinômio padrão 0x07)
uint8_t calcularCRC8(uint8_t seq, uint8_t dado1, uint8_t dado2) {
    uint8_t dados[] = { seq, dado1, dado2 };
    uint8_t crc = 0x00;
    for (int i = 0; i < 3; i++) {
        crc ^= dados[i];
        for (int bit = 0; bit < 8; bit++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x07;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

void setup() { 
    SERIAL_PC.begin(115200); 
    Serial2.begin(115200, SERIAL_8N1, 16, 17); // RX: 16, TX: 17
    delay(2000); 
    SERIAL_PC.println("--- TRANSMISSOR ESP32 PRONTO ---");
}

void loop() { 
    // Trava rígida de 1000 quadros exigida no Passo 3 do experimento
    if (contador_geral >= 1000) { 
        SERIAL_PC.println("\n[FIM] 1000 pacotes enviados para este modo!"); 
        while(1) { delay(1000); } 
    } 

    uint8_t payload_byte1 = 0xFF; 
    uint8_t payload_byte2 = 0xFF; 
    uint8_t seq_byte = contador_geral & 0xFF; // Wrap-around automático em 255
    uint8_t codigo_erro = 0;

    if (USAR_CRC) {
        codigo_erro = calcularCRC8(seq_byte, payload_byte1, payload_byte2);
    } else {
        codigo_erro = calcularChecksum(seq_byte, payload_byte1, payload_byte2);
    }

    // Envio do Quadro Protegido (6 Bytes) - Seção 2 do Roteiro
    SERIAL_UART2.write(MARCADOR_INICIO); 
    SERIAL_UART2.write(seq_byte); 
    SERIAL_UART2.write(payload_byte1); 
    SERIAL_UART2.write(payload_byte2); 
    SERIAL_UART2.write(codigo_erro); 
    SERIAL_UART2.write(MARCADOR_FIM); 

    // Envio da Cópia de Depuração (Verdade de Referência) - Seção 4 do Roteiro
    delayMicroseconds(500); 
    SERIAL_UART2.write(payload_byte1); 
    SERIAL_UART2.write(payload_byte2); 

    contador_geral++; 
    delay(100); // Ritmo estável de 10 pacotes por segundo
}
