// SAM2695 H0 — hardware UART smoke test.
//
// Proves only the physical Cardputer ADV <-> M5Stack Unit MIDI path and
// audible synth output. See docs/hardware/SAM2695_H0.md for the contract and
// for the list of things this deliberately does not prove.
//
// This is a standalone sketch. It shares no code with GroovePuter and cannot
// affect production musical semantics, routing, or USB behavior. Flashing it
// replaces GroovePuter on the device; reflash GroovePuter afterwards.
//
//   Cardputer GPIO1  UART RX
//   Cardputer GPIO2  UART TX
//   31250 baud, 8N1
//   Unit MIDI switch: SEPARATE
//   Audio from the Unit MIDI 3.5 mm AUDIO output

#include <M5Cardputer.h>

namespace {

constexpr int kUartRxPin = 1;
constexpr int kUartTxPin = 2;
constexpr uint32_t kMidiBaud = 31250;

constexpr uint8_t kChannel = 0;  // MIDI channel 1
constexpr uint8_t kNote = 0x3C;  // middle C
constexpr uint8_t kVelocity = 0x64;

constexpr uint32_t kNoteHoldMs = 750;
constexpr uint32_t kCyclePeriodMs = 2000;

HardwareSerial g_midi(1);

uint32_t g_cycle = 0;
uint32_t g_nextCycleMs = 0;
uint32_t g_noteOffDueMs = 0;
bool g_noteHeld = false;

void sendMidi3(uint8_t status, uint8_t data1, uint8_t data2) {
  const uint8_t bytes[3] = {status, data1, data2};
  g_midi.write(bytes, sizeof(bytes));
}

void drawScreen() {
  auto& d = M5Cardputer.Display;
  d.fillRect(0, 40, d.width(), 60, TFT_BLACK);
  d.setCursor(4, 44);
  d.setTextColor(TFT_WHITE, TFT_BLACK);
  d.printf("cycle %lu", (unsigned long)g_cycle);
  d.setCursor(4, 62);
  d.setTextColor(g_noteHeld ? TFT_GREEN : TFT_DARKGREY, TFT_BLACK);
  d.print(g_noteHeld ? "NOTE ON " : "NOTE OFF");
  d.setCursor(4, 80);
  d.setTextColor(TFT_DARKGREY, TFT_BLACK);
  d.printf("up %lus", (unsigned long)(millis() / 1000));
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n\n[SAM2695-H0] boot");

  auto cfg = M5.config();
  // GroovePuter disables both internal audio channels; mirror that so this
  // sketch cannot contend for the ES8311/I2S bus.
  cfg.internal_mic = false;
  cfg.internal_spk = false;
  M5Cardputer.begin(cfg);

  auto& d = M5Cardputer.Display;
  d.setRotation(1);
  d.fillScreen(TFT_BLACK);
  d.setTextSize(2);
  d.setCursor(4, 4);
  d.setTextColor(TFT_CYAN, TFT_BLACK);
  d.print("SAM2695 H0");
  d.setTextSize(1);
  d.setCursor(4, 26);
  d.setTextColor(TFT_DARKGREY, TFT_BLACK);
  d.printf("UART1 %lu 8N1 RX=%d TX=%d",
           (unsigned long)kMidiBaud, kUartRxPin, kUartTxPin);

  g_midi.begin(kMidiBaud, SERIAL_8N1, kUartRxPin, kUartTxPin);
  Serial.printf("[SAM2695-H0] uart1 %lu 8N1 rx=%d tx=%d\n",
                (unsigned long)kMidiBaud, kUartRxPin, kUartTxPin);

  // Defensive startup reset: clears a note left hanging by a previous run so
  // that "no stuck note remains" is a statement about this run.
  sendMidi3(0xB0 | kChannel, 123, 0);
  Serial.println("[SAM2695-H0] sent CC123 all-notes-off");

  g_nextCycleMs = millis() + 500;
  drawScreen();
}

void loop() {
  M5Cardputer.update();
  const uint32_t now = millis();

  if (g_noteHeld && static_cast<int32_t>(now - g_noteOffDueMs) >= 0) {
    sendMidi3(0x80 | kChannel, kNote, 0x00);
    g_noteHeld = false;
    Serial.printf("[SAM2695-H0] cycle=%lu note-off ms=%lu\n",
                  (unsigned long)g_cycle, (unsigned long)now);
    drawScreen();
  }

  if (!g_noteHeld && static_cast<int32_t>(now - g_nextCycleMs) >= 0) {
    ++g_cycle;
    sendMidi3(0x90 | kChannel, kNote, kVelocity);
    g_noteHeld = true;
    g_noteOffDueMs = now + kNoteHoldMs;
    g_nextCycleMs = now + kCyclePeriodMs;
    Serial.printf("[SAM2695-H0] cycle=%lu note-on  ms=%lu up=%lus\n",
                  (unsigned long)g_cycle, (unsigned long)now,
                  (unsigned long)(now / 1000));
    drawScreen();
  }

  delay(2);
}
