# ModbusRTU SCD30, BME280 and SHT31 sensor with Arduino Nano

## Install

As a dependency, you have to have PlatformIO installed. Please see [PlatformIO installation] documentation.

```
$ platformio run --target upload
```

## Parts List

* [Arduino Nano v3 with ATMEGA328P]
* Sensirion SDC30 module [SDC30 module]
* BME280 module [BME280 module]
* SHT31 module
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

NANO PIN A5 ------- SHT31 module PIN SCL
NANO PIN A4 ------- SHT31 module PIN SDA
NANO PIN 5V ------- SHT31 module PIN VDD
NANO PIN GND ------ SHT31 module PIN GND
```

## Modbus holding registers - configuration

```
0 .. WO .. enable configuration changes by writing value 0x00FF
1 .. RW .. address, the default is 0x31
```

Note that the serial configuration is 9600 8N1.

## Modbus input registers - reading the sensors

### SCD30

```
0     .. temperature valid - 0 in case the temperature register values are valid
1     .. temperature value in degress Celsius * 10
2,3   .. temperature value in degress Celsius, in IEEE 754 format
4     .. humidity valid - 0 in case the humidity register values are valid
5     .. humidity value * 10
6,7   .. humidity value in IEEE 754 format
8     .. CO2 concentration value valid - 0 in case the CO2 register value is valid
9     .. CO2 concentration
```

### BME280

```
10    .. temperature valid - 0 in case the temperature register values are valid
11    .. temperature value in degress Celsius * 10
12,13 .. temperature value in degress Celsius, in IEEE 754 format
14    .. humidity valid - 0 in case the humidity register values are valid
15    .. humidity value * 10
16,17 .. humidity value in IEEE 754 format
18    .. air pressure value valid - 0 in case the air pressure register values are valid
19    .. air pressure value
20,21 .. air pressure value in IEEE 754 format
```

### SHT31

```
22    .. temperature valid - 0 in case the temperature register values are valid
23    .. temperature value in degress Celsius * 10
24,25 .. temperature value in degress Celsius, in IEEE 754 format
26    .. humidity valid - 0 in case the humidity register values are valid
27    .. humidity value * 10
28,29 .. humidity value in IEEE 754 format
```

[PlatformIO installation]: http://docs.platformio.org/en/latest/installation.html
[Arduino Nano v3 with ATMEGA328P]: https://www.aliexpress.com/item/32729710918.html?spm=a2g0s.12269583.0.0.2fbb2fc0ndvQ7C
[SDC30 module]: https://www.sensirion.com/en/environmental-sensors/carbon-dioxide-sensors-co2/
[BME280 module]: https://www.aliexpress.com/item/32847825408.html?spm=a2g0s.9042311.0.0.27424c4d4x5ZhK

