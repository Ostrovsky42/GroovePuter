#!/usr/bin/env bash
# Download and optionally flash the current official M5 Launcher for Cardputer.
set -euo pipefail

readonly REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
readonly RELEASE_API="https://api.github.com/repos/bmorcelli/Launcher/releases/latest"
readonly ASSET_NAME="Launcher-m5stack-cardputer.bin"

flash=false
port="/dev/ttyACM0"

usage() {
    cat <<'EOF'
Usage: scripts/restore_m5launcher.sh [--flash [PORT]]

Downloads the latest official M5 Launcher release for Cardputer to:
  build/m5launcher/Launcher-m5stack-cardputer-<version>.bin

The release SHA-256 is verified before the image is accepted.

Options:
  --flash [PORT]  Flash the verified merged image at 0x0.
                  PORT defaults to /dev/ttyACM0.
  -h, --help      Show this help.
EOF
}

while (($#)); do
    case "$1" in
        --flash)
            flash=true
            if (($# > 1)) && [[ "$2" == /dev/* ]]; then
                port="$2"
                shift
            fi
            ;;
        /dev/*)
            port="$1"
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "Unknown argument: $1" >&2
            usage >&2
            exit 2
            ;;
    esac
    shift
done

command -v curl >/dev/null 2>&1 || {
    echo "Error: curl is required to download Launcher." >&2
    exit 1
}
command -v sha256sum >/dev/null 2>&1 || {
    echo "Error: sha256sum is required to verify Launcher." >&2
    exit 1
}

metadata="$(curl -fsSL --retry 3 --connect-timeout 15 "$RELEASE_API")"
read -r version download_url expected_sha < <(
    printf '%s\n' "$metadata" | awk -v asset="$ASSET_NAME" '
        /"tag_name":/ && tag == "" {
            value = $0
            sub(/.*"tag_name": "/, "", value)
            sub(/".*/, "", value)
            tag = value
        }
        $0 ~ "\"name\": \"" asset "\"" { in_asset = 1 }
        in_asset && /"digest":/ {
            value = $0
            sub(/.*sha256:/, "", value)
            sub(/".*/, "", value)
            digest = value
        }
        in_asset && /"browser_download_url":/ {
            value = $0
            sub(/.*"browser_download_url": "/, "", value)
            sub(/".*/, "", value)
            print tag, value, digest
            exit
        }
    '
)

if [[ -z "${version:-}" || -z "${download_url:-}" || ! "$expected_sha" =~ ^[0-9a-f]{64}$ ]]; then
    echo "Error: could not find SHA-256 metadata for $ASSET_NAME." >&2
    exit 1
fi

output_dir="$REPO_ROOT/build/m5launcher"
image="$output_dir/Launcher-m5stack-cardputer-${version}.bin"
mkdir -p "$output_dir"

if [[ -f "$image" ]] && [[ "$(sha256sum "$image" | awk '{print $1}')" == "$expected_sha" ]]; then
    echo "Using verified cached image: $image"
else
    echo "Downloading M5 Launcher $version for Cardputer..."
    curl -fL --retry 3 --connect-timeout 15 -o "$image" "$download_url"
fi

actual_sha="$(sha256sum "$image" | awk '{print $1}')"
if [[ "$actual_sha" != "$expected_sha" ]]; then
    rm -f "$image"
    echo "Error: SHA-256 verification failed; image was removed." >&2
    echo "Expected: $expected_sha" >&2
    echo "Actual:   $actual_sha" >&2
    exit 1
fi

echo "Verified: $image"
echo "SHA-256:  $actual_sha"

if ! "$flash"; then
    echo "Download complete. Flash with:"
    echo "  bash scripts/restore_m5launcher.sh --flash $port"
    exit 0
fi

# Official Launcher release assets are merged flash images (bootloader + partition table + app).
echo "Flashing M5 Launcher $version to $port at 0x0..."
exec "$REPO_ROOT/scripts/upload_launcher.sh" "$port" "$image" 0x0
