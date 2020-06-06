## ESP-01s + Single relay module

Please use ESP-01s (version 2) only because v1 has a problem on power consumption

### Support OTA update

- Arduino IDE
- Web Updater (Please go `{ip_address}/update` then upload `*.bin` as `FIRMWARE`)
- Using command line tools `python espota.py -i 10.100.1.xx -f ../{path_to_bin}/Relay.ino.generic.bin` please checkout `espota.py` from this REPO `https://raw.githubusercontent.com/esp8266/Arduino/master/tools/espota.py`

### Update OTA via command line

From this article (https://blog.stigok.com/2018/06/13/esp8266-ota-updates-command-line.html) in case of Arduino IDE and Web Updater doesn't work for update new firmware via OTA please use commandline instead

- Build tools - https://github.com/espressif/esptool
- Upload OTA - https://raw.githubusercontent.com/esp8266/Arduino/master/tools/espota.py
