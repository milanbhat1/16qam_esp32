#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

// ================= USER CONFIG =================

// MAC address of RECEIVER ESP32 (replace with RX MAC)
uint8_t receiverMAC[] = { 0x78, 0x42, 0x1C, 0x6C, 0x08, 0x58 };

// Button pins
const int BTN_0_PIN = 18;   // sends bit 0
const int BTN_1_PIN = 19;   // sends bit 1

// ================= QAM CONFIG =================

const int qamLevels[4] = { -3, -1, 1, 3 };
const int MAX_SYMBOLS_PER_PACKET = 4;   // 4 symbols = 16 bits

typedef struct {
  uint16_t packetId;
  uint8_t  numSymbols;
  int8_t   I[MAX_SYMBOLS_PER_PACKET];
  int8_t   Q[MAX_SYMBOLS_PER_PACKET];
} QAMPacket;

uint16_t packetCounter = 0;

// ================= BUTTON STATE =================

bool lastBtn0State = HIGH;
bool lastBtn1State = HIGH;

uint16_t bitBuffer = 0;   // MSB-first bit buffer
uint8_t  bitCount  = 0;   // counts up to 16

// ================= QAM MAPPING =================

// 4 bits → (I, Q)
void bitsToQAM(uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4,
               int8_t &I, int8_t &Q)
{
  uint8_t iIndex = (b1 << 1) | b2;
  uint8_t qIndex = (b3 << 1) | b4;

  I = qamLevels[iIndex];
  Q = qamLevels[qIndex];
}

// ================= ESP-NOW CALLBACK =================

void onDataSent(const wifi_tx_info_t *info, esp_now_send_status_t status)
{
  Serial.print("[TX] Send status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
}

// ================= SEND 16 BITS =================

void send16Bits(uint16_t bitPattern)
{
  int8_t Is[4], Qs[4];

  for (int s = 0; s < 4; s++) {
    int shift = (3 - s) * 4;
    uint8_t nibble = (bitPattern >> shift) & 0x0F;

    bitsToQAM(
      (nibble >> 3) & 1,
      (nibble >> 2) & 1,
      (nibble >> 1) & 1,
      (nibble >> 0) & 1,
      Is[s], Qs[s]
    );
  }

  QAMPacket pkt;
  pkt.packetId   = packetCounter++;
  pkt.numSymbols = 4;

  for (int i = 0; i < 4; i++) {
    pkt.I[i] = Is[i];
    pkt.Q[i] = Qs[i];
  }

  esp_now_send(receiverMAC, (uint8_t *)&pkt, sizeof(pkt));

  Serial.print("[TX] Sent 0x");
  Serial.println(bitPattern, HEX);
}

// ================= SETUP =================

void setup()
{
  Serial.begin(115200);
  delay(1000);

  pinMode(BTN_0_PIN, INPUT_PULLUP);
  pinMode(BTN_1_PIN, INPUT_PULLUP);

  WiFi.mode(WIFI_STA);
  Serial.print("[TX] MAC: ");
  Serial.println(WiFi.macAddress());

  esp_now_init();
  esp_now_register_send_cb(onDataSent);

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, receiverMAC, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  esp_now_add_peer(&peerInfo);

  Serial.println("[TX] Ready");
}

// ================= LOOP =================

void loop()
{
  bool btn0 = digitalRead(BTN_0_PIN);
  bool btn1 = digitalRead(BTN_1_PIN);

  if (lastBtn0State == HIGH && btn0 == LOW) {
    bitBuffer = (bitBuffer << 1);
    bitCount++;
    delay(50);
  }

  if (lastBtn1State == HIGH && btn1 == LOW) {
    bitBuffer = (bitBuffer << 1) | 1;
    bitCount++;
    delay(50);
  }

  if (bitCount >= 16) {
    send16Bits(bitBuffer);
    bitBuffer = 0;
    bitCount = 0;
    delay(200);
  }

  lastBtn0State = btn0;
  lastBtn1State = btn1;

  delay(10);
}
