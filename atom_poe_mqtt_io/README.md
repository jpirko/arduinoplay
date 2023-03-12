# Blink hello world for M5Stack Atom Lite

## Install

As a dependency, you have to have PlatformIO installed. Please see [PlatformIO installation] documentation.

```
$ platformio run --target upload
$ platformio device monitor --baud 115200
```

On the console, you should see this:
```
M5Atom initializing...OK
MQTTIO
NAME:test
MAC:aabbccddeeff
IP:172.22.1.10
MQTTIP:172.22.1.1
FILTER:16
THRESHOLD:16
PIN FLAVOURS:
G26: disabled
G32: disabled
CONNECTED
```

These are configuration defaults.

## Usage

MQTT broker port is const: 1883.

The name of the device is the MAC address without ":". The name
is used in MQTT topics. For example:

```
test/filter
```

## Configuration

The device is configurable over MQTT, the topics it subscribes to are:
```
[NAME]/config/mac (device MAC address)
[NAME]/config/ip (device IP address)
[NAME]/config/mqttip (MQTT broker IP address)
[NAME]/config/filter (digital input filter - max 32)
[NAME]/config/threshold (analog input threshold - max 255)
[NAME]/config/G26 (pin flavour, one of "dout", "pwmout", "din", "ain")
[NAME]/config/G32 (pin flavour, one of "dout", "pwmout", "din", "ain")
```

## Output control

Digital and PWM outputs could be controlled over MQTT messages.
The topics device subscribes to are:

```
[NAME]/dout/G26
[NAME]/dout/G32
[NAME]/pwmout/G26
[NAME]/pwmout/G32
```

## Input monitoring

Device publishes following topics with digital and analog input values:

```
[NAME]/din/G26
[NAME]/din/G32
[NAME]/ain/G26
[NAME]/ain/G32
```

## Examples

Execute following batch of commands with desired configuration values:

```
mosquitto_pub -h 172.22.1.1 -t test/config/mac -m aa:22:33:44:55:66
mosquitto_pub -h 172.22.1.1 -t test/config/ip -m 192.168.1.50
mosquitto_pub -h 172.22.1.1 -t test/config/mqttip -m 192.168.1.1
mosquitto_pub -h 172.22.1.1 -t test/config/filter -m 30
mosquitto_pub -h 172.22.1.1 -t test/config/threshold -m 20
mosquitto_pub -h 172.22.1.1 -t test/config/G26 -m dout
mosquitto_pub -h 172.22.1.1 -t test/config/G32 -m ain
```

Please note that "172.22.1.1" is the address of MQTT broker. Make sure you set
new addresses of the device (ip) and the MQTT broker (mqttip) correctly.

```

[PlatformIO installation]: http://docs.platformio.org/en/latest/installation.html
