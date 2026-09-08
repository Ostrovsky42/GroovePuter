#include <M5Cardputer.h>

#include <usb/usb_host.h>
#include "packet_diagnostics.h"

#include <atomic>
#include <esp_heap_caps.h>

namespace {

#if ARDUINO_USB_CDC_ON_BOOT
#error "H0 host requires CDCOnBoot=default"
#endif

// Atomic shared state, safe across the main loop() task (which drives USB
// host/client event handling -- see the callback-context note below, this is
// task context, not a real interrupt) and drawTask (pinned to Core 0).
std::atomic<bool> g_hostInstalled{false};
std::atomic<usb_host_client_handle_t> client{nullptr};
std::atomic<usb_device_handle_t> device{nullptr};
std::atomic<const char*> status{"BOOT"};
std::atomic<esp_err_t> lastError{ESP_OK};
std::atomic<uint16_t> vid{0}, pid{0};
std::atomic<uint32_t> attaches{0}, detaches{0};
std::atomic<int> inputEndpoint{-1}, outputEndpoint{-1};
std::atomic<uint16_t> inputMps{4};
std::atomic<uint8_t> interfaceNumber{0};
std::atomic<usb_transfer_t*> inputTransfer{nullptr};

PacketDiagnostics rx;

struct HostProbeSnapshot {
  const char* label{""};
  uint32_t free8{0};
  uint32_t min8{0};
  uint32_t largest8{0};
  uint32_t freeDefault{0};
  uint32_t minDefault{0};
  uint32_t largestDefault{0};
  uint32_t freeDma{0};
  uint32_t minDma{0};
  uint32_t largestDma{0};
  uint32_t stackHighWater{0};
};

TaskHandle_t g_drawTaskHandle{nullptr};
HostProbeSnapshot g_snapS0{};
HostProbeSnapshot g_latestSnap{};
bool g_hasS0{false};
uint32_t g_cycleCount{0};
int32_t g_lastResident8{0};
uint32_t g_lastDetachLargest8{0};

std::atomic<bool> inFlight{false}, closing{false}, claimed{false};
std::atomic<uint32_t> transferErrors{0};
std::atomic<int> lastTransferStatus{-1};
std::atomic<bool> firstNoteMemorySampled{false};

// Simple spin-mutex for protecting UI-state reads/writes
portMUX_TYPE stateMux = portMUX_INITIALIZER_UNLOCKED;

void fail(const char* stage, esp_err_t err) {
  status.store(stage, std::memory_order_relaxed);
  lastError.store(err, std::memory_order_relaxed);
}

HostProbeSnapshot captureSnapshot(const char* label) {
  HostProbeSnapshot s{};
  s.label = label;
  s.free8 = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  s.min8 = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  s.largest8 = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

  s.freeDefault = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_DEFAULT);
  s.minDefault = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_DEFAULT);
  s.largestDefault = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_DEFAULT);

  s.freeDma = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
  s.minDma = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
  s.largestDma = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);

  if (g_drawTaskHandle != nullptr) {
    s.stackHighWater = uxTaskGetStackHighWaterMark(g_drawTaskHandle);
  }
  return s;
}

void printSnapshot(const HostProbeSnapshot& s) {
  Serial.printf("[HOST] %s  8BIT free=%lu min=%lu largest=%lu\n",
                s.label, (unsigned long)s.free8, (unsigned long)s.min8, (unsigned long)s.largest8);
  Serial.printf("[HOST] %s  DEF  free=%lu min=%lu largest=%lu\n",
                s.label, (unsigned long)s.freeDefault, (unsigned long)s.minDefault, (unsigned long)s.largestDefault);
  Serial.printf("[HOST] %s  DMA  free=%lu min=%lu largest=%lu\n",
                s.label, (unsigned long)s.freeDma, (unsigned long)s.minDma, (unsigned long)s.largestDma);
  Serial.printf("[HOST] %s  STK  ui_task_hwm=%lu\n",
                s.label, (unsigned long)s.stackHighWater);
}

void sampleAndReport(const char* label) {
  HostProbeSnapshot s = captureSnapshot(label);
  portENTER_CRITICAL(&stateMux);
  g_latestSnap = s;
  portEXIT_CRITICAL(&stateMux);
  printSnapshot(s);
}

void onInputTransfer(usb_transfer_t* transfer) {
  inFlight.store(false, std::memory_order_relaxed);
  lastTransferStatus.store(static_cast<int>(transfer->status), std::memory_order_relaxed);
  if (closing.load(std::memory_order_relaxed)) return;
  if (transfer->status != USB_TRANSFER_STATUS_COMPLETED) {
    transferErrors.fetch_add(1, std::memory_order_relaxed);
    fail("RX TRANSFER ERROR", ESP_FAIL);
    return;
  }
  if (transfer->actual_num_bytes < 0 ||
      static_cast<size_t>(transfer->actual_num_bytes) > transfer->data_buffer_size) {
    fail("RX LENGTH ERROR", ESP_ERR_INVALID_SIZE);
    return;
  }

  portENTER_CRITICAL(&stateMux);
  rx.observe(transfer->data_buffer,
             static_cast<size_t>(transfer->actual_num_bytes));
  const bool haveNote = rx.haveNote;
  portEXIT_CRITICAL(&stateMux);

  if (haveNote && !firstNoteMemorySampled.exchange(true, std::memory_order_relaxed)) {
    sampleAndReport("S3 rx_chord");
  }

  // Resubmit if still valid
  if (inputTransfer.load(std::memory_order_relaxed) == transfer &&
      device.load(std::memory_order_relaxed) == transfer->device_handle) {
    esp_err_t err = usb_host_transfer_submit(transfer);
    inFlight.store(err == ESP_OK, std::memory_order_relaxed);
    if (err != ESP_OK) fail("RESUBMIT ERROR", err);
  }
}

void onClientEvent(const usb_host_client_event_msg_t* event, void*) {
  if (event->event == USB_HOST_CLIENT_EVENT_DEV_GONE) {
    if (device.load(std::memory_order_relaxed) == event->dev_gone.dev_hdl) {
      closing.store(true, std::memory_order_relaxed);
      if (claimed.load(std::memory_order_relaxed) && inFlight.load(std::memory_order_relaxed)) {
        (void)usb_host_endpoint_halt(device.load(), static_cast<uint8_t>(inputEndpoint.load()));
        (void)usb_host_endpoint_flush(device.load(), static_cast<uint8_t>(inputEndpoint.load()));
      }
      detaches.fetch_add(1, std::memory_order_relaxed);
      status.store("DISCONNECTING", std::memory_order_relaxed);
    }
    return;
  }

  if (event->event != USB_HOST_CLIENT_EVENT_NEW_DEV || device.load() != nullptr) return;

  usb_device_handle_t devHandle = nullptr;
  esp_err_t err = usb_host_device_open(client.load(), event->new_dev.address, &devHandle);
  if (err != ESP_OK) { fail("OPEN ERROR", err); return; }
  device.store(devHandle, std::memory_order_relaxed);
  attaches.fetch_add(1, std::memory_order_relaxed);
  firstNoteMemorySampled.store(false, std::memory_order_relaxed);
  portENTER_CRITICAL(&stateMux);
  rx = PacketDiagnostics{};
  portEXIT_CRITICAL(&stateMux);

  const usb_device_desc_t* desc = nullptr;
  err = usb_host_get_device_descriptor(device.load(), &desc);
  if (err != ESP_OK) { fail("DESCRIPTOR ERROR", err); return; }
  vid.store(desc->idVendor, std::memory_order_relaxed);
  pid.store(desc->idProduct, std::memory_order_relaxed);

  const usb_config_desc_t* config = nullptr;
  err = usb_host_get_active_config_descriptor(device.load(), &config);
  if (err != ESP_OK) { fail("CONFIG ERROR", err); return; }

  const uint8_t* bytes = reinterpret_cast<const uint8_t*>(config);
  bool streaming = false, midi = false;
  inputEndpoint.store(-1, std::memory_order_relaxed);
  outputEndpoint.store(-1, std::memory_order_relaxed);

  for (size_t offset = 0; offset + 2 <= config->wTotalLength;) {
    uint8_t length = bytes[offset];
    if (length < 2 || offset + length > config->wTotalLength) { fail("BAD DESCRIPTOR", ESP_ERR_INVALID_SIZE); return; }
    const uint8_t* d = bytes + offset;
    if (d[1] == 4 && length >= 9) {
      streaming = d[5] == 1 && d[6] == 3;
      if (streaming) interfaceNumber.store(d[2], std::memory_order_relaxed);
      midi |= streaming;
    } else if (d[1] == 5 && length >= 7 && streaming && (d[3] & 3) == 2) {
      if (d[2] & 0x80) {
        inputEndpoint.store(d[2], std::memory_order_relaxed);
        inputMps.store(d[4] | (static_cast<uint16_t>(d[5]) << 8), std::memory_order_relaxed);
      } else {
        outputEndpoint.store(d[2], std::memory_order_relaxed);
      }
    }
    offset += length;
  }

  lastError.store(ESP_OK, std::memory_order_relaxed);
  status.store(midi ? "MIDI DETECTED" : "NOT MIDI", std::memory_order_relaxed);

  if (midi && inputEndpoint.load() >= 0) {
    uint16_t mps = inputMps.load();
    if (mps < 3) { fail("UNSUPPORTED MPS", ESP_ERR_NOT_SUPPORTED); return; }
    static const uint16_t allowed[] = {3,4,8,16,32,64};
    bool ok = false;
    for (uint16_t a : allowed) if (mps == a) { ok = true; break; }
    if (!ok) {
      uint16_t chosen = 64;
      for (uint16_t a : allowed) if (mps <= a) { chosen = a; break; }
      inputMps.store(chosen, std::memory_order_relaxed);
    }

    err = usb_host_interface_claim(client.load(), device.load(), interfaceNumber.load(), 0);
    if (err != ESP_OK) { fail("CLAIM ERROR", err); return; }
    claimed.store(true, std::memory_order_relaxed);

    usb_transfer_t* transfer = nullptr;
    err = usb_host_transfer_alloc(inputMps.load(), 0, &transfer);
    if (err != ESP_OK) { fail("ALLOC ERROR", err); return; }
    transfer->device_handle = device.load();
    transfer->bEndpointAddress = static_cast<uint8_t>(inputEndpoint.load());
    transfer->num_bytes = inputMps.load();
    transfer->callback = onInputTransfer;
    inputTransfer.store(transfer, std::memory_order_relaxed);
    err = usb_host_transfer_submit(transfer);
    if (err != ESP_OK) { fail("SUBMIT ERROR", err); return; }
    inFlight.store(true, std::memory_order_relaxed);
    status.store("MIDI RX READY", std::memory_order_relaxed);

    if (attaches.load(std::memory_order_relaxed) <= 1) {
      sampleAndReport("S2 post_enumerate");
    } else {
      sampleAndReport("S5 post_reattach");
      if (g_hasS0) {
        HostProbeSnapshot cur = captureSnapshot("S5_cycle");
        g_lastResident8 = static_cast<int32_t>(cur.free8) - static_cast<int32_t>(g_snapS0.free8);
      }
    }
  }
}

// UI drawing runs on a separate FreeRTOS task (Core 0)
void drawTask(void*) {
  for (;;) {
    portENTER_CRITICAL(&stateMux);
    const char* curStatus = status.load();
    esp_err_t curErr = lastError.load();
    uint16_t curVid = vid.load();
    uint16_t curPid = pid.load();
    int curIn = inputEndpoint.load();
    int curOut = outputEndpoint.load();
    const PacketDiagnostics rxSnapshot = rx;
    const HostProbeSnapshot snap = g_latestSnap;
    uint32_t errCnt = transferErrors.load();
    int lastStat = lastTransferStatus.load();
    uint32_t att = attaches.load();
    uint32_t det = detaches.load();
    uint32_t cyc = g_cycleCount;
    int32_t res8 = g_lastResident8;
    uint32_t detLg = g_lastDetachLargest8;
    portEXIT_CRITICAL(&stateMux);

    auto& d = M5Cardputer.Display;
    d.fillScreen(TFT_BLACK);
    d.setTextSize(1);
    d.setTextColor(TFT_WHITE, TFT_BLACK);
    d.setCursor(2, 2);
    d.println("nanoKEY2 H0 [M1]");
    d.printf("%s err:%s\n", curStatus, esp_err_to_name(curErr));
    d.printf("VID:PID %04X:%04X IN:%d\n", curVid, curPid, curIn);
    d.printf("xfer:%lu bytes:%lu MPS:%u\n", (unsigned long)rxSnapshot.transfers, (unsigned long)rxSnapshot.bytes, inputMps.load());
    d.printf("pkt:%lu ON:%lu OFF:%lu\n", (unsigned long)rxSnapshot.packets, (unsigned long)rxSnapshot.noteOns, (unsigned long)rxSnapshot.noteOffs);
    if (rxSnapshot.haveNote) d.printf("NOTE:%02X %02X %02X %02X\n", rxSnapshot.lastNote[0], rxSnapshot.lastNote[1], rxSnapshot.lastNote[2], rxSnapshot.lastNote[3]);
    else d.println("NOTE: -- waiting note");
    d.printf("cyc:%lu att:%lu det:%lu\n", (unsigned long)cyc, (unsigned long)att, (unsigned long)det);
    d.printf("mem:%s\n", snap.label ? snap.label : "none");
    d.printf("i8:%lu min:%lu lg:%lu\n", (unsigned long)snap.free8, (unsigned long)snap.min8, (unsigned long)snap.largest8);
    d.printf("dma:%lu lg:%lu uiHWM:%lu\n", (unsigned long)snap.freeDma, (unsigned long)snap.largestDma, (unsigned long)snap.stackHighWater);
    d.printf("res8:%ld cycLg8:%lu\n", (long)res8, (unsigned long)detLg);
    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

} // namespace

void setup() {
  Serial.begin(115200);
  auto cfg = M5.config();
  cfg.internal_spk = false;
  cfg.internal_mic = false;
  M5Cardputer.begin(cfg);
  M5Cardputer.Display.setRotation(1);
  M5Cardputer.Display.setBrightness(128);

  // Create UI task before any USB activity
  xTaskCreatePinnedToCore(drawTask, "drawTask", 4096, nullptr, 1, &g_drawTaskHandle, 0);
  vTaskDelay(pdMS_TO_TICKS(50));

  // S0: pre_install baseline
  g_snapS0 = captureSnapshot("S0 pre_install");
  g_hasS0 = true;
  portENTER_CRITICAL(&stateMux);
  g_latestSnap = g_snapS0;
  portEXIT_CRITICAL(&stateMux);
  printSnapshot(g_snapS0);

  const usb_host_config_t config = {
      .skip_phy_setup = false,
      .root_port_unpowered = false,
      .intr_flags = ESP_INTR_FLAG_LEVEL3,
  };
  esp_err_t result = usb_host_install(&config);
  if (result != ESP_OK) { fail("HOST INSTALL ERROR", result); return; }

  g_hostInstalled.store(true, std::memory_order_relaxed);
  usb_host_client_config_t clientConfig{};
  clientConfig.max_num_event_msg = 5;
  clientConfig.async.client_event_callback = onClientEvent;
  usb_host_client_handle_t clientHandle = nullptr;
  esp_err_t err = usb_host_client_register(&clientConfig, &clientHandle);
  if (err != ESP_OK) { fail("CLIENT ERROR", err); return; }
  client.store(clientHandle, std::memory_order_relaxed);
  status.store("WAITING USB", std::memory_order_relaxed);

  // S1: post_install
  sampleAndReport("S1 post_install");
}

void loop() {
  if (g_hostInstalled.load()) {
    uint32_t flags = 0;
    esp_err_t err = usb_host_lib_handle_events(0, &flags);
    if (err != ESP_OK && err != ESP_ERR_TIMEOUT) fail("HOST EVENT ERROR", err);
    if (client.load()) {
      err = usb_host_client_handle_events(client.load(), 0);
      if (err != ESP_OK && err != ESP_ERR_TIMEOUT) fail("CLIENT EVENT ERROR", err);
    }
  }

  // Cleanup after disconnect
  if (closing.load() && !inFlight.load() && device.load() != nullptr) {
    if (inputTransfer.load()) {
      usb_host_transfer_free(inputTransfer.load());
      inputTransfer.store(nullptr);
    }
    esp_err_t err = ESP_OK;
    if (claimed.load()) {
      err = usb_host_interface_release(client.load(), device.load(), interfaceNumber.load());
      if (err == ESP_OK) claimed.store(false);
    }
    if (err == ESP_OK) err = usb_host_device_close(client.load(), device.load());
    if (err == ESP_OK) {
      device.store(nullptr);
      closing.store(false);
      inputEndpoint.store(-1);
      outputEndpoint.store(-1);
      status.store("WAITING USB", std::memory_order_relaxed);
      lastError.store(ESP_OK, std::memory_order_relaxed);

      // S4: post_detach
      sampleAndReport("S4 post_detach");

      g_cycleCount++;
      HostProbeSnapshot curS4 = captureSnapshot("S4_detach");
      g_lastDetachLargest8 = curS4.largest8;
      if (g_hasS0) {
        g_lastResident8 = static_cast<int32_t>(curS4.free8) - static_cast<int32_t>(g_snapS0.free8);
      }
      Serial.printf("[HOST] CYCLE %lu resident8=%ld min8=%lu largest8=%lu attaches=%lu detaches=%lu\n",
                    (unsigned long)g_cycleCount, (long)g_lastResident8,
                    (unsigned long)curS4.min8, (unsigned long)g_lastDetachLargest8,
                    (unsigned long)attaches.load(std::memory_order_relaxed),
                    (unsigned long)detaches.load(std::memory_order_relaxed));
    } else {
      fail("CLEANUP ERROR", err);
    }
  }
  vTaskDelay(1);
}

