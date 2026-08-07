#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


hardware = read("src/platform/cardputer_adv_hardware.h")
tape = read("src/dsp/tape_fx.cpp")
i2s = read("src/audio/audio_out_i2s.cpp")
sketch = read("GroovePuter.ino")

# Cardputer ADV must not physically claim GPIO21 as a PA enable. The legacy
# setup call is intentionally routed to a typed no-op by ADL.
assert "GROOVEPUTER_CARDPUTER_ADV_PA_EN_PIN" not in hardware
assert "UnusedPowerAmplifierEnablePin" in hardware
assert "inline void pinMode(UnusedPowerAmplifierEnablePin" in hardware
assert "inline void digitalWrite(UnusedPowerAmplifierEnablePin" in hardware
assert "GPIO21" in hardware
assert "kPowerAmplifierEnablePin" in sketch

# Tape AGE should colour existing material/tails, not generate a permanent
# standalone noise bed when its input has reached digital silence.
assert "AGE noise is signal-coupled" in tape
assert "ageAmount_ > 0 && fabsf(output) >= kHalfPcmLsb" in tape
assert "does not create a permanent noise bed" in tape

# The very last Cardputer output stage may collapse only +/-1 LSB to zero.
assert "constexpr int16_t kIdlePcmFloorLsb = 1" in i2s
assert "rawSample >= -kIdlePcmFloorLsb" in i2s
assert "rawSample <= kIdlePcmFloorLsb" in i2s
assert "? 0" in i2s

# Periodic clicks must be diagnosable as I2S starvation/partial writes instead
# of being indistinguishable from analog AUX/ground noise.
assert "[AudioOutI2S] write err=%d bytes=%u/%u" in i2s
assert "bytesWritten != expectedBytes" in i2s
assert "[I2S] Write Timeout / Error" in sketch

print("idle AUX audio source regressions: OK")
