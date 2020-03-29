# MQTT IO client for Controllino

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
[NAME]/threshold (analog input threshold - max 128)

## Output control

Relays and digital outputs could be controlled over MQTT messages.
The topics device subscribes to are:

```
[NAME]/relay/0
[NAME]/relay/1
..
[NAME]/relay/15
[NAME]/dout/0
[NAME]/dout/1
...
[NAME]/dout/23
[NAME]/pwmout/0
[NAME]/pwmout/1
...
[NAME]/pwmout/11
[NAME]/pwmout/14
[NAME]/pwmout/16
```

Note that "dout/X" and "pwmout/X" manipulate the same output.

## Input monitoring

Device publishes following topics with digital and analog input values:

```
[NAME]/din/0
[NAME]/din/1
..
[NAME]/din/15
[NAME]/ain/0
[NAME]/ain/1
..
[NAME]/ain/15
```

Note that "din/X" and "ain/X" carry input value for the same input.

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

* [Controllino]

[PlatformIO installation]: http://docs.platformio.org/en/latest/installation.html
[Controllino]: https://controllino.biz/

