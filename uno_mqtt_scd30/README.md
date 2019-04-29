# Arduino UNO CO2 sensor SCD30 accessible over MQTT

## Install

As a dependency, you have to have PlatformIO installed. Please see [PlatformIO installation] documentation.

```
$ pio lib install "Ethernet"
$ pio lib install "SparkFun_SCD30_Arduino_Library"
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
fe77d1a71603/co2
```

The device published following topics with measured values:

```
[NAME]/co2
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
* Sensirion SDC30 module [SDC30 module]
* DS2401 ID chip [DS2401] - Using value from this a a base for MAC address
* Bunch of wires

## Wiring

```
UNO PIN A5 ------- SCD30 module PIN SCL
UNO PIN A4 ------- SCD30 module PIN SDA
UNO PIN 5V ------- SCD30 module PIN VDD
UNO PIN GND ------ SCD30 module PIN GND

UNO PIN 3 -------- DS2401 PIN 2 (DQ)
UNO PIN 5V ------- 4k7 Ohm pull-up resistor --------  DS2401 PIN 2 (DQ)
UNO PIN GND ------ DS2401 PIN 1 (GND)
```

[PlatformIO installation]: http://docs.platformio.org/en/latest/installation.html
[XDRuino UNO]: http://www.dx.com/p/uno-r3-development-board-microcontroller-mega328p-atmega16u2-compat-for-arduino-blue-black-215600#.Wdil7hdBoUE
[Ethernet Shield]: https://www.arduino.cc/en/Guide/ArduinoEthernetShield
[SDC30 module]: https://www.sensirion.com/en/environmental-sensors/carbon-dioxide-sensors-co2/
[DS2401]: https://datasheets.maximintegrated.com/en/ds/DS2431.pdf
