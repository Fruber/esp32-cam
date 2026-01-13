# Project Implementation Summary

## Overview
Complete ESP-IDF 5.5 project for ESP32-S3-WROOM-CAM with OV2640 camera, WS2812B LED, color detection, MJPEG streaming, and Wi-Fi provisioning.

## Project Structure

```
esp32-cam/
├── CMakeLists.txt              # Root CMake configuration
├── sdkconfig.defaults          # ESP-IDF configuration defaults
├── partitions.csv              # Flash partition table
├── LICENSE                     # MIT License
├── README.md                   # User documentation
├── .gitignore                  # Git ignore rules
└── main/
    ├── CMakeLists.txt          # Component CMake configuration
    ├── idf_component.yml       # Component dependencies
    ├── pin_config.h            # Hardware pin definitions
    ├── app_main.c              # Main application entry point
    ├── camera_driver.c/h       # OV2640 camera driver
    ├── ws2812_led.c/h          # WS2812B LED control
    ├── config_store.c/h        # NVS configuration storage
    ├── color_detect.c/h        # RGB band detection algorithm
    ├── http_server.c/h         # HTTP server with MJPEG streaming
    └── web_ui.h                # Italian language web interface
```

## Key Features Implemented

### 1. Camera Driver (`camera_driver.c/h`)
- **Sensor**: OV2640
- **Format**: RGB565 (no JPEG conversion in camera)
- **Resolution**: VGA (640x480)
- **Frame Buffers**: 2 buffers in PSRAM
- **Pin Configuration**: As specified in requirements (GPIO 4-18, 38, 48)

### 2. Color Detection (`color_detect.c/h`)
- **Algorithm**: HSV-based blob detection
- **Process**: 
  1. Convert RGB565 → RGB888 → HSV
  2. Detect red, green, blue blobs using configurable thresholds
  3. Check spatial ordering (R-G-B left to right)
  4. Calculate confidence based on vertical alignment
  5. Draw yellow bounding box when detected
- **Performance**: Frame decimation (default 15 = ~2 FPS at 30 FPS input)
- **Robustness**: Configurable HSV ranges, minimum area (~30x30 px), confidence threshold

### 3. WS2812B LED (`ws2812_led.c/h`)
- **GPIO**: 48
- **States**:
  - Red: Objects detected (rgb_detected = true)
  - Green: No objects (rgb_detected = false)
- **Driver**: ESP led_strip component with RMT

### 4. HTTP Server (`http_server.c/h`)
- **Port**: 80
- **Endpoints**:
  - `/` - Italian web UI (HTML/CSS/JavaScript)
  - `/stream` - MJPEG stream (VLC-compatible)
  - `/api/config` GET - Retrieve configuration JSON
  - `/api/config` POST - Update configuration JSON
- **Streaming**: RGB565 → JPEG conversion → multipart/x-mixed-replace
- **Processing**: Color detection runs on every frame in stream

### 5. Configuration Storage (`config_store.c/h`)
- **Storage**: NVS namespace "color_cfg"
- **Format**: Binary blob of `color_config_t` structure
- **Parameters**:
  - HSV thresholds for R/G/B (h_min, h_max, s_min, s_max, v_min, v_max)
  - Minimum area (pixels)
  - Minimum confidence (0-100%)
  - Frame decimation factor
- **Defaults**: Loaded on first boot if NVS empty

### 6. Wi-Fi Provisioning (`app_main.c`)
- **Method**: ESP SoftAP Provisioning
- **Security**: WIFI_PROV_SECURITY_1
- **POP**: "abcd1234"
- **SSID**: "PROV_XXXXXX" (last 3 bytes of MAC)
- **Flow**:
  1. Check if already provisioned
  2. If not, start SoftAP provisioning
  3. After provisioning, connect to configured Wi-Fi
  4. Start HTTP server when connected

### 7. Web UI (`web_ui.h`)
- **Language**: Italian
- **Features**:
  - Live MJPEG stream display
  - HSV threshold adjustment for each color (R/G/B)
  - General parameters (area, confidence, decimation)
  - Load/Save buttons
  - Status feedback
- **API Integration**: Calls `/api/config` for GET/POST

## Configuration Details

### HSV Color Ranges (0-255 scale)
- **Red**: H: 0-10, S: 100-255, V: 100-255
- **Green**: H: 40-80, S: 100-255, V: 100-255
- **Blue**: H: 100-140, S: 100-255, V: 100-255

### Performance Settings
- **Minimum Area**: 900 pixels (~30x30)
- **Minimum Confidence**: 60%
- **Frame Decimation**: 15 (process every 15th frame)

### ESP-IDF Configuration Highlights
- **Target**: ESP32-S3
- **PSRAM**: Octal mode, 80MHz, enabled for malloc
- **Camera**: Core 0, 32KB DMA buffers
- **Wi-Fi**: Enhanced buffers for streaming
- **HTTP**: Increased header/URI limits
- **Optimization**: Performance mode

## Technical Notes

### RGB565 to HSV Conversion
The color detection uses a simplified HSV model with all channels in 0-255 range:
- **Hue**: 0-255 (wraps around, red at 0/255)
- **Saturation**: 0-255 (color purity)
- **Value**: 0-255 (brightness)

### Detection Algorithm
1. **Blob Detection**: Scan entire frame pixel-by-pixel
2. **Color Matching**: Check if HSV values within thresholds
3. **Bounding Box**: Track min/max X/Y for each color
4. **Area Check**: Verify each color meets minimum pixel count
5. **Ordering Check**: Red centroid < Green centroid < Blue centroid
6. **Alignment Check**: Vertical alignment for confidence score
7. **Threshold**: Only report if confidence ≥ min_confidence

### MJPEG Streaming
- RGB565 frames processed in-place
- Bounding box drawn directly on RGB565 buffer
- frame2jpg() converts to JPEG with quality 80
- Multipart boundary: "123456789000000000000987654321"
- Compatible with VLC, ffplay, web browsers

## Dependencies (from idf_component.yml)

```yaml
espressif/esp32-camera: ^2.0.0    # Camera driver
espressif/led_strip: ^2.5.0       # WS2812B LED
espressif/mdns: ^1.2.0            # mDNS (optional)
```

## Memory Usage Estimates

- **PSRAM**: ~2 VGA RGB565 frame buffers (640*480*2*2 = ~1.2 MB)
- **Heap**: Camera driver, HTTP server, Wi-Fi stack (~200-300 KB)
- **Stack**: 
  - Main task: 8192 bytes
  - HTTP handlers: 8192 bytes per connection
  - Event loop: 4096 bytes

## Build Requirements

- ESP-IDF 5.5 or later
- Python 3.8+
- CMake 3.16+
- ESP32-S3 toolchain

## Testing Recommendations

1. **Camera**: Verify RGB565 frame capture at VGA
2. **LED**: Check red/green states during detection
3. **Detection**: Test with printed R-G-B color bands
4. **Streaming**: Verify MJPEG in VLC (`vlc http://<ip>/stream`)
5. **Web UI**: Test threshold adjustments and persistence
6. **Provisioning**: Test Wi-Fi setup with ESP provisioning app

## Known Limitations

1. **Frame Rate**: ~2 FPS processing (adjustable via decimation)
2. **Detection**: Single RGB band set (no multiple objects)
3. **Rotation**: Best performance with horizontal bands
4. **Distance**: Optimized for ~30px object size
5. **Lighting**: Sensitive to ambient light (HSV thresholds may need tuning)

## License

MIT License - All source files include SPDX-License-Identifier: MIT header
