/*
 * Arduino UNO PWM driver accessible over MQTT
 * Copyright (c) 2019 Jiri Pirko <jiri@resnulli.us>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "Arduino.h"
#include <avr/wdt.h>
#include <SPI.h>
#include <Ethernet.h>
#include <PubSubClient.h>
#include <OneWire.h>

#define ONEWIRE_ID_PIN 2

OneWire ds(ONEWIRE_ID_PIN);
EthernetClient ethClient;
PubSubClient client(ethClient);

char name[13];

struct pin {
	uint8_t pin; /* pin number */
	int index;
	word state;
};

#define PIN(_index, _pin) {				\
	.pin = _pin,					\
	.index = _index,				\
}

struct pin pins[] = {
	PIN(0, 3),
	PIN(1, 6),
	PIN(2, 9),
};

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#define PINS_COUNT ARRAY_SIZE(pins)

#define for_each_pin(pin, i)							\
	for (i = 0, pin = &pins[i]; i < PINS_COUNT; pin = &pins[++i])

String pin_topic(struct pin *pin)
{
	return String(name) + "/pwm" + pin->index;
}

String pin_topic_set(struct pin *pin)
{
	return pin_topic(pin) + "/set";
}

void pin_publish(struct pin *pin)
{
	char state_buf[16];

	String(pin->state).toCharArray(state_buf, sizeof(state_buf));
	client.publish(pin_topic(pin).c_str(), state_buf);
}

void pins_msg_process(String topic, String value)
{
	word new_state = value.toInt();
	struct pin *pin;
	unsigned int i;

	for_each_pin(pin, i) {
		if (pin_topic_set(pin) == topic &&
		    pin->state != new_state) {
			pin->state = new_state;
			Serial.println("setting pin " + String(pin->pin) + " to " + String(pin->state));
			analogWrite(pin->pin, pin->state);
			pin_publish(pin);
		}
	}
}

void pins_subscribe(void)
{
	struct pin *pin;
	unsigned int i;

	for_each_pin(pin, i) {
		Serial.println(pin_topic_set(pin));
		client.subscribe((pin_topic_set(pin)).c_str());
	}
}

void pins_init(void)
{
	struct pin *pin;
	unsigned int i;

	for_each_pin(pin, i)
		pinMode(pin->pin, OUTPUT);
}

void print_address()
{
	int i;

	Serial.print("IP address: ");
	for (i = 0; i < 4; i++) {
		if (i)
			Serial.print(".");
		Serial.print(Ethernet.localIP()[i], DEC);
	}
	Serial.println();
}

void callback(char *topic, byte *payload, unsigned int length)
{
	payload[length] = '\0';
	pins_msg_process(String(topic), String((char *) payload));
}

void setup()
{
	wdt_enable(WDTO_8S);
	byte addr[8];
	byte *mac;

	Serial.begin(9600);
	Serial.println("NANO PWM driver");

	/* Find a first available device on OneWire bus and take it's serial
	 * number as a base for MAC address.
	 */
	if (!ds.search(addr) || OneWire::crc8(addr, 7) != addr[7]) {
		Serial.println("Fatal - Failed to get OneWire ID\n");
		return;
	}

	mac = addr + 1;
	mac[0] &= 0xfe; /* Clear multicast bit. */
	mac[0] |= 0x02; /* Set local assignment bit. */

	wdt_reset();

	if (Ethernet.begin(mac) == 0) {
		Serial.println("Fatal - Failed to get IP address using DHCP\n");
		return;
	}
	print_address();

	sprintf(name, "%02x%02x%02x%02x%02x%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	Serial.println("Name: " + String(name));

	pins_init();

	/* Use assigned gateway IP as MQTT broker address with default port */
	client.setServer(Ethernet.gatewayIP(), 1883);
	client.setCallback(callback);
}

#define MQTT_RETRY_TIMEOUT 5000

static bool mqtt_connected;
static unsigned long mqtt_last_attempt;

void loop() {
	unsigned long now = millis();

	wdt_reset();

	if (!client.connected()) {
		if (mqtt_connected) {
			mqtt_last_attempt = 0;
			Serial.println("Disconnected from MQTT server");
			mqtt_connected = false;
		}
		if (!mqtt_last_attempt ||
		    mqtt_last_attempt + MQTT_RETRY_TIMEOUT < now ||
		    mqtt_last_attempt > now) {
			mqtt_last_attempt = now;
			if (client.connect(name)) {
				Serial.println("Connected to MQTT server");
				mqtt_connected = true;
				pins_subscribe();
			}
		}
	} else {
		client.loop();
	}
}
