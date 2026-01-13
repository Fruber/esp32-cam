# ESP32-S3 Camera with RGB Band Detection

An ESP-IDF 5.5 project for ESP32-S3-WROOM-CAM featuring OV2640 camera with color detection, MJPEG streaming, and Wi-Fi provisioning.

## Features

- **Camera**: OV2640 with RGB565 output at VGA resolution
- **Color Detection**: Detects three adjacent RGB bands in order (Red-Green-Blue)
- **MJPEG Streaming**: VLC-compatible stream at `/stream` endpoint
- **Wi-Fi Provisioning**: ESP SoftAP provisioning with POP `abcd1234`
- **Web UI**: Italian language interface for adjusting HSV thresholds and detection parameters
- **Configuration**: Persistent storage in NVS with REST API (`/api/config`)
- **LED Indicator**: WS2812B LED (red when objects detected, green otherwise)
- **Performance**: ~2 FPS processing via configurable frame decimation

## Hardware

- **Board**: ESP32-S3-WROOM-CAM (ESP32-S3-N16R8)
- **Camera**: OV2640
- **LED**: WS2812B on GPIO48
- **PSRAM**: 8MB enabled

### Pin Configuration

See `main/pin_config.h` for complete pinout:
- Camera: PWDN=38, XCLK=15, SIOD=4, SIOC=5, Y2-Y9=11,9,8,10,12,18,17,16, VSYNC=6, HREF=7, PCLK=13
- LED: GPIO48

## Prerequisites

- ESP-IDF 5.5 or later
- Python 3.8+
- ESP32-S3 development tools

## Building

```bash
# Set up ESP-IDF environment
. $HOME/esp/esp-idf/export.sh

# Configure the project (optional, uses sdkconfig.defaults)
idf.py set-target esp32s3

# Build
idf.py build

# Flash and monitor
idf.py -p /dev/ttyUSB0 flash monitor
```

## Usage

1. **Provisioning**: On first boot, device creates AP `PROV_XXXXXX`. Use ESP SoftAP provisioning app with POP `abcd1234` to configure Wi-Fi.

2. **Access Web UI**: After connecting to Wi-Fi, access `http://<device-ip>/` in browser.

3. **View Stream**: MJPEG stream available at `http://<device-ip>/stream` (compatible with VLC).

4. **Configure Detection**: Use web UI to adjust HSV thresholds for red/green/blue colors, minimum area, confidence, and frame decimation.

5. **REST API**:
   - GET `/api/config` - Get current configuration
   - POST `/api/config` - Update configuration (JSON body)

## Configuration Parameters

- **HSV Thresholds**: H/S/V min/max for each color (R/G/B) in 0-255 range
- **Min Area**: Minimum pixels for color blob (~30x30 = 900 default)
- **Min Confidence**: Detection confidence threshold (0-100%, default 60%)
- **Frame Decimation**: Process every Nth frame (default 15 for ~2 FPS)

## License

MIT License - See LICENSE file for details.

## Author

Fruber (2026)