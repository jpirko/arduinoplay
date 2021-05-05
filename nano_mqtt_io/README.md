# Arduino Nano v3 ATMEGA328P MQTT IO client

## Install

As a dependency, you have to have PlatformIO installed. Please see [PlatformIO installation] documentation.

```
$ platformio run --target upload
$ platformio device monitor

```

On the console, you should see this:
```
MQTTIO
NAME:aabbccddeeff
IP:172.22.1.10
MQTTIP:172.22.1.1
FILTER:16
THRESHOLD:16
CONNECTED
```

These are configuration defaults.

## Usage

MQTT broker port is const: 1883.

The name of the device is the MAC address without ":". The name
is used in MQTT topics. For example:

```
aabbccddeeff/filter
```

## Configuration

The device is configurable over MQTT, the topics it subscribes to are:
```
[NAME]/mac (device MAC address)
[NAME]/ip (device IP address)
[NAME]/mqttip (MQTT broker IP address)
[NAME]/filter (input filter - max 32)

## Output control

Digital and PWM outputs could be controlled over MQTT messages.
The topics device subscribes to are:

```
[NAME]/dout/D2
[NAME]/dout/D3
..
[NAME]/dout/D6
[NAME]/dout/D9
[NAME]/dout/A0
[NAME]/dout/A1
[NAME]/dout/A7
```

In vase of PWM configured pins:

```
[NAME]/pwmout/D2
[NAME]/pwmout/D3
..
[NAME]/pwmout/D6
[NAME]/pwmout/D9
[NAME]/pwmout/A0
[NAME]/pwmout/A1
[NAME]/pwmout/A7
```

## Input monitoring

Device publishes following topics with digital and analog input values:

```
[NAME]/din/D2
[NAME]/din/D3
..
[NAME]/din/D6
[NAME]/din/D9
[NAME]/din/A0
[NAME]/din/A1
[NAME]/din/A7
```

## Examples

Execute following batch of commands with desired configuration values:

```
mosquitto_pub -h 172.22.1.1 -t aabbccddeeff/config/mac -m aa:22:33:44:55:66
mosquitto_pub -h 172.22.1.1 -t aabbccddeeff/config/ip -m 192.168.1.50
mosquitto_pub -h 172.22.1.1 -t aabbccddeeff/config/mqttip -m 192.168.1.1
mosquitto_pub -h 172.22.1.1 -t aabbccddeeff/config/filter -m 30
mosquitto_pub -h 172.22.1.1 -t aabbccddeeff/config/threshold -m 20
```

Please note that "172.22.1.1" is the address of MQTT broker. Make sure you set
new addresses of the device (ip) and the MQTT broker (mqttip) correctly.

To turn on a relay number "10", execute following command:

```
mosquitto_pub -h 172.22.1.1 -t aabbccddeeff/relay/10 -m 1

```

## Parts List

* [Arduino Nano v3 with ATMEGA328P]
* [Arduino Nano ethernet shield]

[PlatformIO installation]: http://docs.platformio.org/en/latest/installation.html
[Arduino Nano v3 with ATMEGA328P]: https://www.aliexpress.com/item/32729710918.html?spm=a2g0s.12269583.0.0.2fbb2fc0ndvQ7C
[Arduino Nano ethernet shield]: https://robotdyn.com/catalog/arduino/shields/nano-v3-ethernet-shield-enc28j60.html
