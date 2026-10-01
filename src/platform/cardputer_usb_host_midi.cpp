#include "cardputer_usb_host_midi.h"

#if defined(ARDUINO)

#include <Arduino.h>
#include <esp_heap_caps.h>
#include <usb/usb_host.h>
#include <atomic>
#include <algorithm>

namespace GroovePuterMidi {
namespace {

std::atomic<bool> g_hostInstalled{false};
std::atomic<usb_host_client_handle_t> g_client{nullptr};
std::atomic<usb_device_handle_t> g_device{nullptr};
std::atomic<uint16_t> g_vid{0}, g_pid{0};
std::atomic<int> g_inputEndpoint{-1};
std::atomic<uint16_t> g_inputMps{64};
std::atomic<uint8_t> g_interfaceNumber{0};
std::atomic<usb_transfer_t*> g_inputTransfer{nullptr};
std::atomic<bool> g_inFlight{false}, g_closing{false}, g_claimed{false};
std::atomic<uint32_t> g_packetCount{0}, g_noteOnCount{0}, g_noteOffCount{0};
std::atomic<uint8_t> g_lastNote{0}, g_lastVelocity{0};
std::atomic<const char*> g_status{"INIT"};

UsbHostMidiCallback g_callback{nullptr};
UsbHostMemDiag g_memDiag;

void snapshot(uint32_t& freeBytes, uint32_t& largest) {
    constexpr uint32_t caps = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
    freeBytes = heap_caps_get_free_size(caps);
    largest = heap_caps_get_largest_free_block(caps);
    g_memDiag.minEverFree = heap_caps_get_minimum_free_size(caps);
}

// Single-producer (the task that runs usb_host_client_handle_events, i.e. loop) /
// single-consumer (MidiDispatchTask) ring of USB-MIDI packets. Bounded, no allocation.
constexpr uint32_t kRingSize = 64;  // packets, power of two
uint32_t g_ring[kRingSize];
std::atomic<uint32_t> g_ringHead{0}, g_ringTail{0}, g_ringDropped{0};

void ringPush(uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3) {
    const uint32_t head = g_ringHead.load(std::memory_order_relaxed);
    if (head - g_ringTail.load(std::memory_order_acquire) >= kRingSize) {
        g_ringDropped.fetch_add(1, std::memory_order_relaxed);
        return;
    }
    g_ring[head & (kRingSize - 1)] = static_cast<uint32_t>(b0) | (static_cast<uint32_t>(b1) << 8) |
                                      (static_cast<uint32_t>(b2) << 16) | (static_cast<uint32_t>(b3) << 24);
    g_ringHead.store(head + 1, std::memory_order_release);
}

void onInputTransfer(usb_transfer_t* transfer) {
    g_inFlight.store(false, std::memory_order_relaxed);
    if (g_closing.load(std::memory_order_relaxed)) return;
    if (transfer->status != USB_TRANSFER_STATUS_COMPLETED) {
        g_status.store("XFER_ERR", std::memory_order_relaxed);
        return;
    }

    const uint8_t* buf = transfer->data_buffer;
    const size_t len = static_cast<size_t>(transfer->actual_num_bytes);

    if (len >= 4) {
        for (size_t i = 0; i + 4 <= len; i += 4) {
            const uint8_t cin = buf[i] & 0x0F;
            const uint8_t status = buf[i + 1];
            const uint8_t d1 = buf[i + 2];
            const uint8_t d2 = buf[i + 3];

            if (cin == 0x09 || cin == 0x08 || (status & 0xF0) == 0x90 || (status & 0xF0) == 0x80) {
                if (!g_memDiag.packet) {
                    snapshot(g_memDiag.freePacket, g_memDiag.largestPacket);
                    g_memDiag.packet = true;
                }
                g_packetCount.fetch_add(1, std::memory_order_relaxed);
                if ((status & 0xF0) == 0x90 && d2 > 0) {
                    g_noteOnCount.fetch_add(1, std::memory_order_relaxed);
                    g_lastNote.store(d1, std::memory_order_relaxed);
                    g_lastVelocity.store(d2, std::memory_order_relaxed);
                    g_status.store("NOTE_ON", std::memory_order_relaxed);
                } else {
                    g_noteOffCount.fetch_add(1, std::memory_order_relaxed);
                    g_status.store("NOTE_OFF", std::memory_order_relaxed);
                }
            }
            // Every USB-MIDI packet with a real cable-event number goes to the single consumer
            // (the MIDI dispatch task); CIN 0 and 1 are reserved. The identity/fencing/parsing
            // is the existing input path, not this file.
            if (cin >= 2) ringPush(buf[i], buf[i + 1], buf[i + 2], buf[i + 3]);
        }
    }

    // Resubmit transfer if device still active
    if (g_inputTransfer.load(std::memory_order_relaxed) == transfer &&
        g_device.load(std::memory_order_relaxed) == transfer->device_handle) {
        esp_err_t err = usb_host_transfer_submit(transfer);
        g_inFlight.store(err == ESP_OK, std::memory_order_relaxed);
        if (err != ESP_OK) {
            g_status.store("RESUBMIT_ERR", std::memory_order_relaxed);
        }
    }
}

void onClientEvent(const usb_host_client_event_msg_t* event, void*) {
    if (event->event == USB_HOST_CLIENT_EVENT_DEV_GONE) {
        if (g_device.load(std::memory_order_relaxed) == event->dev_gone.dev_hdl) {
            g_closing.store(true, std::memory_order_relaxed);
            g_status.store("DEV_GONE", std::memory_order_relaxed);
            if (g_claimed.load(std::memory_order_relaxed) && g_inFlight.load(std::memory_order_relaxed)) {
                (void)usb_host_endpoint_halt(g_device.load(), static_cast<uint8_t>(g_inputEndpoint.load()));
                (void)usb_host_endpoint_flush(g_device.load(), static_cast<uint8_t>(g_inputEndpoint.load()));
            }
        }
        return;
    }

    if (event->event != USB_HOST_CLIENT_EVENT_NEW_DEV || g_device.load() != nullptr) return;

    usb_device_handle_t devHandle = nullptr;
    esp_err_t err = usb_host_device_open(g_client.load(), event->new_dev.address, &devHandle);
    if (err != ESP_OK) {
        g_status.store("OPEN_FAIL", std::memory_order_relaxed);
        return;
    }

    g_device.store(devHandle, std::memory_order_relaxed);

    const usb_device_desc_t* desc = nullptr;
    if (usb_host_get_device_descriptor(devHandle, &desc) == ESP_OK) {
        g_vid.store(desc->idVendor, std::memory_order_relaxed);
        g_pid.store(desc->idProduct, std::memory_order_relaxed);
    }

    const usb_config_desc_t* config = nullptr;
    if (usb_host_get_active_config_descriptor(devHandle, &config) != ESP_OK) {
        g_status.store("CFG_FAIL", std::memory_order_relaxed);
        return;
    }

    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(config);
    bool streaming = false;
    g_inputEndpoint.store(-1, std::memory_order_relaxed);

    for (size_t offset = 0; offset + 2 <= config->wTotalLength;) {
        uint8_t length = bytes[offset];
        if (length < 2 || offset + length > config->wTotalLength) break;
        const uint8_t* d = bytes + offset;
        if (d[1] == 4 && length >= 9) {
            streaming = (d[5] == 1 && d[6] == 3); // Audio class, MIDI streaming subclass
            if (streaming) g_interfaceNumber.store(d[2], std::memory_order_relaxed);
        } else if (d[1] == 5 && length >= 7 && streaming && (d[3] & 3) == 2) { // Bulk endpoint
            if (d[2] & 0x80) { // IN endpoint
                g_inputEndpoint.store(d[2], std::memory_order_relaxed);
                uint16_t mps = d[4] | (static_cast<uint16_t>(d[5]) << 8);
                if (mps < 64) mps = 64;
                g_inputMps.store(mps, std::memory_order_relaxed);
            }
        }
        offset += length;
    }

    if (g_inputEndpoint.load() >= 0) {
        err = usb_host_interface_claim(g_client.load(), devHandle, g_interfaceNumber.load(), 0);
        if (err == ESP_OK) {
            g_claimed.store(true, std::memory_order_relaxed);
            usb_transfer_t* transfer = nullptr;
            err = usb_host_transfer_alloc(g_inputMps.load(), 0, &transfer);
            if (err == ESP_OK) {
                transfer->device_handle = devHandle;
                transfer->bEndpointAddress = static_cast<uint8_t>(g_inputEndpoint.load());
                transfer->num_bytes = g_inputMps.load();
                transfer->callback = onInputTransfer;
                g_inputTransfer.store(transfer, std::memory_order_relaxed);
                err = usb_host_transfer_submit(transfer);
                g_inFlight.store(err == ESP_OK, std::memory_order_relaxed);
                g_status.store(err == ESP_OK ? "READY" : "SUBMIT_FAIL", std::memory_order_relaxed);
                if (err == ESP_OK && !g_memDiag.device) {
                    snapshot(g_memDiag.freeDevice, g_memDiag.largestDevice);
                    g_memDiag.device = true;
                }
            } else {
                g_status.store("ALLOC_FAIL", std::memory_order_relaxed);
            }
        } else {
            g_status.store("CLAIM_FAIL", std::memory_order_relaxed);
        }
    } else {
        g_status.store("NO_EP", std::memory_order_relaxed);
    }
}

} // namespace

bool CardputerUsbHostMidi::begin(UsbHostMidiCallback callback) {
    if (g_hostInstalled.load()) return true;
    g_callback = callback;

    const usb_host_config_t config = {
        .skip_phy_setup = false,
        .root_port_unpowered = false,
        .intr_flags = ESP_INTR_FLAG_LEVEL3,
    };
    snapshot(g_memDiag.freeBefore, g_memDiag.largestBefore);
    esp_err_t result = usb_host_install(&config);
    if (result != ESP_OK) return false;
    g_hostInstalled.store(true, std::memory_order_relaxed);
    snapshot(g_memDiag.freeInstalled, g_memDiag.largestInstalled);
    g_memDiag.installed = true;

    usb_host_client_config_t clientConfig{};
    clientConfig.max_num_event_msg = 5;
    clientConfig.async.client_event_callback = onClientEvent;
    usb_host_client_handle_t clientHandle = nullptr;
    result = usb_host_client_register(&clientConfig, &clientHandle);
    if (result != ESP_OK) return false;
    g_client.store(clientHandle, std::memory_order_relaxed);
    snapshot(g_memDiag.freeClient, g_memDiag.largestClient);
    g_memDiag.client = true;
    return true;
}

void CardputerUsbHostMidi::service() {
    if (!g_hostInstalled.load()) return;

    uint32_t flags = 0;
    (void)usb_host_lib_handle_events(0, &flags);
    if (g_client.load()) {
        (void)usb_host_client_handle_events(g_client.load(), 0);
    }

    // Cleanup after disconnect
    if (g_closing.load() && !g_inFlight.load() && g_device.load() != nullptr) {
        if (g_inputTransfer.load()) {
            usb_host_transfer_free(g_inputTransfer.load());
            g_inputTransfer.store(nullptr);
        }
        if (g_claimed.load()) {
            (void)usb_host_interface_release(g_client.load(), g_device.load(), g_interfaceNumber.load());
            g_claimed.store(false);
        }
        (void)usb_host_device_close(g_client.load(), g_device.load());
        g_device.store(nullptr);
        g_closing.store(false);
        g_inputEndpoint.store(-1);
    }
}

bool CardputerUsbHostMidi::isConnected() {
    return g_device.load() != nullptr && g_claimed.load();
}

uint16_t CardputerUsbHostMidi::vid() { return g_vid.load(); }
uint16_t CardputerUsbHostMidi::pid() { return g_pid.load(); }
uint32_t CardputerUsbHostMidi::packetCount() { return g_packetCount.load(); }
uint32_t CardputerUsbHostMidi::noteOnCount() { return g_noteOnCount.load(); }
uint32_t CardputerUsbHostMidi::noteOffCount() { return g_noteOffCount.load(); }
uint8_t CardputerUsbHostMidi::lastNote() { return g_lastNote.load(); }
uint8_t CardputerUsbHostMidi::lastVelocity() { return g_lastVelocity.load(); }
const char* CardputerUsbHostMidi::status() { return g_status.load(); }
const UsbHostMemDiag& CardputerUsbHostMidi::memDiag() { return g_memDiag; }

bool CardputerUsbHostMidi::popPacket(uint8_t out[4]) {
    const uint32_t tail = g_ringTail.load(std::memory_order_relaxed);
    if (tail == g_ringHead.load(std::memory_order_acquire)) return false;
    const uint32_t v = g_ring[tail & (kRingSize - 1)];
    out[0] = static_cast<uint8_t>(v);
    out[1] = static_cast<uint8_t>(v >> 8);
    out[2] = static_cast<uint8_t>(v >> 16);
    out[3] = static_cast<uint8_t>(v >> 24);
    g_ringTail.store(tail + 1, std::memory_order_release);
    return true;
}
uint32_t CardputerUsbHostMidi::droppedPackets() { return g_ringDropped.load(); }

void CardputerUsbHostMidi::stop() {
    // Teardown if necessary
}

} // namespace GroovePuterMidi

#else

// Desktop host stub
namespace GroovePuterMidi {
bool CardputerUsbHostMidi::begin(UsbHostMidiCallback) { return true; }
void CardputerUsbHostMidi::service() {}
bool CardputerUsbHostMidi::isConnected() { return false; }
uint16_t CardputerUsbHostMidi::vid() { return 0; }
uint16_t CardputerUsbHostMidi::pid() { return 0; }
uint32_t CardputerUsbHostMidi::packetCount() { return 0; }
uint32_t CardputerUsbHostMidi::noteOnCount() { return 0; }
uint32_t CardputerUsbHostMidi::noteOffCount() { return 0; }
uint8_t CardputerUsbHostMidi::lastNote() { return 0; }
uint8_t CardputerUsbHostMidi::lastVelocity() { return 0; }
const char* CardputerUsbHostMidi::status() { return "OFF"; }
const UsbHostMemDiag& CardputerUsbHostMidi::memDiag() { static UsbHostMemDiag d; return d; }
bool CardputerUsbHostMidi::popPacket(uint8_t[4]) { return false; }
uint32_t CardputerUsbHostMidi::droppedPackets() { return 0; }
void CardputerUsbHostMidi::stop() {}
} // namespace GroovePuterMidi

#endif
