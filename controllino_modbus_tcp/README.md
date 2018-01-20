# Modbus TCP slave for Controllino

## Install

As a dependency, you have to have PlatformIO installed. Please see [PlatformIO installation] documentation.

```
$ pio lib install "Controllino"
$ pio lib install "ModbusIP"
$ mkdir lib
$ cd lib
$ git clone https://github.com/jpirko/Ethernet.git
$ cd ..
$ platformio run --target upload
$ platformio device monitor
```
Note that the forked Ethernet library is needed in other for Ethernet to work. the reason is Controllino uses different SPI CS pin comparing to the original Arduino UNO Ethernet shield. At the time you read this, this might be resolved already by upstream Ethernet library.

## Parts List

* [Controllino]

[PlatformIO installation]: http://docs.platformio.org/en/latest/installation.html
[Controllino]: https://controllino.biz/

