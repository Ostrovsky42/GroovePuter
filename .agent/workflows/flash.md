---
description: Upload MiniAcid to M5Stack Cardputer
---

### Flash Workflow

This workflow uploads the compiled binary to the device.

1. Connect the Cardputer via USB.
2. Identify the serial port (default: `/dev/ttyACM0`).
// turbo
3. Run the upload script.
```bash
./upload-psram.sh /dev/ttyACM0
```

> [!TIP]
> If the port is different, pass it as an argument: `./upload-psram.sh /dev/ttyUSB0`.
