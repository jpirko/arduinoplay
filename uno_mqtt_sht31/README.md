# Arduino UNO CO2 sensor SCD30 accessible over MQTT

## Install

As a dependency, you have to have PlatformIO installed. Please see [PlatformIO installation] documentation.

```
$ pio lib install "Adafruit SHT31 Library"
$ pio lib install "PubSubClient"
$ pio lib install "OneWire"
$ platformio run --target upload
$ platformio device monitor
```

## Usage

IP address is obtained using DHCP. The gateway IP is used as
MQTT broker (with default port 1883).

The name of the device is the MAC address without ":". The name
is used in MQTT topics. For example:

```
fe77d1a71603/temperature
```

The device published following topics with measured values:

```
[NAME]/humidity
[NAME]/temperature
```

The device is configurable over MQTT, the topics it subscribes to are:
```
[NAME]/interval (2 to 1800 seconds)
[NAME]/altitude (altitude compenstation)
[NAME]/pressure (pressure compensation, 700 to 1200 mbar)
```

## Parts List

* Arduino UNO (or clone, I'm using [XDRuino UNO])
* [Ethernet Shield] Ethernet Shield
* Sensirion SHT31 module [SHT31 module]
* DS2401 ID chip [DS2401] - Using value from this a a base for MAC address
* Bunch of wires

## Wiring

```
UNO PIN A5 ------- SHT31 module PIN SCL
UNO PIN A4 ------- SHT31 module PIN SDA
UNO PIN 5V ------- SHT31 module PIN VDD
UNO PIN GND ------ SHT31 module PIN GND

UNO PIN 3 -------- DS2401 PIN 2 (DQ)
UNO PIN 5V ------- 4k7 Ohm pull-up resistor --------  DS2401 PIN 2 (DQ)
UNO PIN GND ------ DS2401 PIN 1 (GND)
```

[PlatformIO installation]: http://docs.platformio.org/en/latest/installation.html
[XDRuino UNO]: http://www.dx.com/p/uno-r3-development-board-microcontroller-mega328p-atmega16u2-compat-for-arduino-blue-black-215600#.Wdil7hdBoUE
[Ethernet Shield]: https://www.arduino.cc/en/Guide/ArduinoEthernetShield
[SHT31 module]: https://www.adafruit.com/product/2857
[DS2401]: https://datasheets.maximintegrated.com/en/ds/DS2431.pdf
