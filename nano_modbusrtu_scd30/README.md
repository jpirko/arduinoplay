# ModbusRTU SCD30 and BME280 based CO2 sensor with Arduino Nano

## Install

As a dependency, you have to have PlatformIO installed. Please see [PlatformIO installation] documentation.

```
$ platformio lib install "ModbusSerial"
$ platformio lib install "SparkFun_SCD30_Arduino_Library"
$ platformio lib install "SparkFun BME280"
$ platformio run --target upload
```

## Parts List

* [Arduino Nano v3 with ATMEGA328P]
* Sensirion SDC30 module [SDC30 module]
* BME280 module [BME280 module]
* [RS485 module]
* Bunch of wires

## Wiring

```
NANO PIN A5 ------- SCD30 module PIN SCL
NANO PIN A4 ------- SCD30 module PIN SDA
NANO PIN 5V ------- SCD30 module PIN VDD
NANO PIN GND ------ SCD30 module PIN GND

NANO PIN TX1 ------ RS485 module PIN DI
NANO PIN RX0 ------ RS485 module PIN RO
NANO PIN D2 ------- RS485 module PIN DE
NANO PIN D2 ------- RS485 module PIN RE
NANO PIN 5V ------- RS485 module PIN VCC
NANO PIN GND ------ RS485 module PIN GND

NANO PIN A5 ------- BME280 module PIN SCL
NANO PIN A4 ------- BME280 module PIN SDA
NANO PIN 5V ------- BME280 module PIN VDD
NANO PIN GND ------ BME280 module PIN GND
```
![Wiring](wiring.jpg)

[PlatformIO installation]: http://docs.platformio.org/en/latest/installation.html
[Arduino Nano v3 with ATMEGA328P]: https://www.aliexpress.com/snapshot/0.html?spm=a2g0s.9042647.0.0.57f04c4deOqsZx&orderId=100848359295850&productId=32729710918
[SDC30 module]: https://www.sensirion.com/en/environmental-sensors/carbon-dioxide-sensors-co2/
[BME280 module]: https://www.aliexpress.com/item/32847825408.html?spm=a2g0s.9042311.0.0.27424c4d4x5ZhK

