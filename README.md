# About

Control LoRa via serial interface using AT commands. Baud rate: 115200.

# Supported commands

All request commands must start with "AT". All responses must end with "OK" or "ERROR". If command returned "ERROR" then the message before contained the error reason.

Sometimes response might contain lines with "[I]" or "[E]". This is lora-at internal logging. These messages can be ignored.

Full list of supported commands can be found here: [AT-commands](https://github.com/dernasherbrezon/lora-at/wiki/AT-commands)

# Supported boards

 * [ttgo-lora32-v2](https://docs.platformio.org/en/latest/boards/espressif32/ttgo-lora32-v2.html)
 * [ttgo-lora32-v1](https://docs.platformio.org/en/latest/boards/espressif32/ttgo-lora32-v1.html)
 * [ttgo-lora32-v21](https://docs.platformio.org/en/latest/boards/espressif32/ttgo-lora32-v21.html)
 * [heltec_wifi_lora_32](https://docs.platformio.org/en/latest/boards/espressif32/heltec_wifi_lora_32.html)
 * [heltec_wifi_lora_32_V2](https://docs.platformio.org/en/latest/boards/espressif32/heltec_wifi_lora_32_V2.html)
 * [ttgo-t-beam](https://docs.platformio.org/en/latest/boards/espressif32/ttgo-t-beam.html)

# How to install / compile

1. Install [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/)

2. Clone this repository:

```bash
git clone https://github.com/dernasherbrezon/lora-at.git
```

3. Configure the application. All relevant configuration can be found under "LoRa-AT" menu option

```bash
idf.py menuconfig
```

4. Build the application.

```bash
idf.py build
pio run -t upload -e board
```

5. Upload the application. Replace ```/dev/cu.usbserial-0001``` with your device's port/path.

```bash
idf.py -p /dev/cu.usbserial-0001 flash
```

6. Connect to the device and send AT commands.

```bash
idf.py -p /dev/cu.usbserial-0001 monitor
```

They can be also sent programmatically.