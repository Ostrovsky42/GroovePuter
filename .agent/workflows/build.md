---
description: Build MiniAcid for M5Stack Cardputer (DRAM-only)
---

### Build Workflow

This workflow compiles the MiniAcid project using `arduino-cli`.

1. Ensure documentation and code are saved.
// turbo
2. Run the build script.
```bash
./build.sh
```

> [!NOTE]
> The build script uses the FQBN `m5stack:esp32:m5stack_cardputer:PSRAM=disabled,PartitionScheme=huge_app`.
> This enforces DRAM-only mode as required for the Cardputer hardware.
