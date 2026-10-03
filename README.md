# ESP32 IR Parking Assist — ESP-IDF project

Two IR obstacle sensors (left + right) on an ESP32. The board hosts a small
web page on your Wi-Fi showing a top-down car; the matching side lights up red
when that sensor detects an obstacle. No distance measurement — just presence,
left or right.

This is a proper **ESP-IDF** project that pulls the Arduino core in as a
managed component (so `setup()` / `loop()` work exactly like an Arduino sketch,
while you still get `idf.py` and the full IDF toolchain).

## Layout

```
esp32_ir_parking/
├── CMakeLists.txt          # root project file
├── sdkconfig.defaults      # autostart Arduino + 4 MB flash + custom partitions
├── partitions.csv          # ~3 MB app partition (Arduino core + Wi-Fi needs room)
├── README.md
└── main/
    ├── CMakeLists.txt
    ├── idf_component.yml   # pulls espressif/arduino-esp32
    └── main.cpp            # the application (web page embedded)
```

## Requirements

- ESP-IDF **v5.3 or newer** (arduino-esp32 v3.x requires it).
- Internet access on first build — the IDF Component Manager downloads
  `espressif/arduino-esp32` and its dependencies from the ESP Registry.

## Wiring

| Sensor   | ESP32 |
|----------|-------|
| IR LEFT  | VCC -> 3V3, GND -> GND, OUT -> GPIO 4 |
| IR RIGHT | VCC -> 3V3, GND -> GND, OUT -> GPIO 5 |

## Configure and build

Edit `main/main.cpp` and set:

```cpp
static const char* WIFI_SSID     = "YOUR_WIFI_NAME";
static const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
```

Then, with the IDF environment sourced (`$IDF_PATH/export.sh`):

```bash
idf.py set-target esp32          # or esp32c3 / esp32s3, matching your board
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

The monitor prints an IP like `http://192.168.1.42`. Open that in a browser on
the same Wi-Fi. The page auto-switches to "LIVE · ESP32 CONNECTED" and the car
reacts to the real sensors.

## Notes

- If the sensors light up **backwards** (on when clear, off when blocked), set
  `ACTIVE_LOW` to `false` in `main.cpp`.
- Change `IR_LEFT` / `IR_RIGHT` if you wired to different pins.
- The HTML page is embedded in `main.cpp` as a PROGMEM string; edit it there to
  restyle. The same page is also shipped as `parking.html` so you can preview
  it in a browser (tap the zones to simulate).
- Adding a dependency later: `idf.py add-dependency "..."` writes to
  `main/idf_component.yml`.
