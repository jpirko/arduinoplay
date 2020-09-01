/*
 * Arduino Nano: MQTT inputs client
 * Copyright (c) 2020 Jiri Pirko <jiri@resnulli.us>
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
#include <SPI.h>
#include <UIPEthernet.h>
#include <PubSubClient.h>
#include <EEPROM.h>

EthernetClient ethClient;
PubSubClient client(ethClient);

#define NAME_SIZE 20
char name[NAME_SIZE];

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

enum pin_type {
	PIN_TYPE_DIGITAL_INPUT,
	PIN_TYPE_DIGITAL_A_INPUT,
	PIN_TYPE_DIGITAL_AX_INPUT,
};

const char *pin_type_subtopic[] = {
	[PIN_TYPE_DIGITAL_INPUT] = "/din/D",
	[PIN_TYPE_DIGITAL_A_INPUT] = "/din/A",
	[PIN_TYPE_DIGITAL_AX_INPUT] = "/din/A",
};

struct pin {
	uint8_t type;
	uint8_t index; /* index within the group */
	uint8_t pin; /* pin number */
	uint8_t old_state:1,
		state:1,
		new_state_cnt:6;
};

/* values: 0, 1 */
#define PIN_DIGITAL_INPUT(_index, _pin) {						\
	.type = PIN_TYPE_DIGITAL_INPUT,							\
	.index = _index,								\
	.pin = _pin,									\
}

#define PIN_DIGITAL_A_INPUT(_index, _pin) {						\
	.type = PIN_TYPE_DIGITAL_A_INPUT,						\
	.index = _index,								\
	.pin = _pin,									\
}

#define PIN_DIGITAL_AX_INPUT(_index, _pin) {						\
	.type = PIN_TYPE_DIGITAL_AX_INPUT,						\
	.index = _index,								\
	.pin = _pin,									\
}

struct pin pins[] = {
	PIN_DIGITAL_INPUT(2, 2), /* D2 */
	PIN_DIGITAL_INPUT(3, 3), /* D3 */
	PIN_DIGITAL_INPUT(4, 4), /* D4 */
	PIN_DIGITAL_INPUT(5, 5), /* D5 */
	PIN_DIGITAL_INPUT(6, 6), /* D6 */
	PIN_DIGITAL_INPUT(7, 7), /* D7 */
	PIN_DIGITAL_INPUT(8, 8), /* D8 */
	PIN_DIGITAL_INPUT(9, 9), /* D9 */
	PIN_DIGITAL_A_INPUT(0, A0), /* A0 */
	PIN_DIGITAL_A_INPUT(1, A1), /* A1 */
	PIN_DIGITAL_A_INPUT(2, A2), /* A2 */
	PIN_DIGITAL_A_INPUT(3, A3), /* A3 */
	PIN_DIGITAL_A_INPUT(4, A4), /* A4 */
	PIN_DIGITAL_A_INPUT(5, A5), /* A5 */
	PIN_DIGITAL_AX_INPUT(6, A6), /* A6 */
	PIN_DIGITAL_AX_INPUT(7, A7), /* A7 */
};

#define PINS_COUNT ARRAY_SIZE(pins)

#define for_each_pin(pin, i)								\
	for (i = 0, pin = &pins[i]; i < PINS_COUNT; pin = &pins[++i])

#define TMP_BUF_LEN 64
char tmp_buf[TMP_BUF_LEN];

char *pin_topic(struct pin *pin)
{
	snprintf(tmp_buf, TMP_BUF_LEN, "%s/%s/%d", name,
		 pin_type_subtopic[pin->type], pin->index);
	return tmp_buf;
}

char *config_topic(const char *item)
{
	snprintf(tmp_buf, TMP_BUF_LEN, "%s/config/%s", name, item);
	return tmp_buf;
}

void pin_publish(struct pin *pin)
{
	char state_buf[16];

	sprintf(state_buf, "%u", pin->state);
	client.publish(pin_topic(pin), state_buf);
}

void input_pins_update_state()
{
	struct pin *pin;
	uint8_t new_state;
	uint8_t i;

	for_each_pin(pin, i) {
		switch (pin->type) {
		case PIN_TYPE_DIGITAL_INPUT: /* fall-through */
		case PIN_TYPE_DIGITAL_A_INPUT:
			new_state = digitalRead(pin->pin);
			break;
		case PIN_TYPE_DIGITAL_AX_INPUT:
			new_state = analogRead(pin->pin) < 500 ? 0 : 1;
			break;
		default:
			continue;
		}
		pin->state = new_state;
	}
}

uint8_t input_filter;

void input_pins_publish(bool changed_only)
{
	struct pin *pin;
	uint8_t i;

	for_each_pin(pin, i) {
		switch (pin->type) {
		case PIN_TYPE_DIGITAL_INPUT:
		case PIN_TYPE_DIGITAL_A_INPUT: /* fall-through */
		case PIN_TYPE_DIGITAL_AX_INPUT:  /* fall-through */
			if (changed_only) {
				if (pin->old_state == pin->state) {
					pin->new_state_cnt = 0;
					continue;
				}
				if (pin->new_state_cnt++ < input_filter)
					continue;
			}
			pin_publish(pin);
			pin->old_state = pin->state;
			pin->new_state_cnt = 0;
			break;
		}
	}
}

void pins_init(void)
{
	struct pin *pin;
	uint8_t i;

	for_each_pin(pin, i)
		pinMode(pin->pin, INPUT);
}

void print_ip(IPAddress ip)
{
	int i;

	for (i = 0; i < 4; i++) {
		if (i)
			Serial.print(".");
		Serial.print(ip[i], DEC);
	}
	Serial.println();
}

uint32_t eeprom_magic = 0x3b1e2e8a;
char eeprom_default_name[] = "test";
byte eeprom_default_mac[] = {0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};
IPAddress eeprom_default_ip = IPAddress(172, 22, 1, 10);
IPAddress eeprom_default_mqttip = IPAddress(172, 22, 1, 1);
uint8_t eeprom_default_filter = 16;
#define EEPROM_FILTER_MAX 32

#define EEPROM_MAGIC_OFFSET 0
#define EEPROM_MAGIC_SIZE sizeof(eeprom_magic)

#define MAC_LEN sizeof(eeprom_default_mac)
#define IP_LEN sizeof(IPAddress)

#define EEPROM_NAME_OFFSET EEPROM_MAGIC_OFFSET + EEPROM_MAGIC_SIZE
#define EEPROM_NAME_SIZE NAME_SIZE

#define EEPROM_MAC_OFFSET EEPROM_NAME_OFFSET + EEPROM_NAME_SIZE
#define EEPROM_MAC_SIZE MAC_LEN

#define EEPROM_IP_OFFSET EEPROM_MAC_OFFSET + EEPROM_MAC_SIZE
#define EEPROM_IP_SIZE IP_LEN

#define EEPROM_MQTTIP_OFFSET EEPROM_IP_OFFSET + EEPROM_IP_SIZE
#define EEPROM_MQTTIP_SIZE IP_LEN

#define EEPROM_FILTER_OFFSET EEPROM_MQTTIP_OFFSET + EEPROM_MQTTIP_SIZE
#define EEPROM_FILTER_SIZE sizeof(eeprom_default_filter)

void eeprom_check(void)
{
	uint32_t magic;

	EEPROM.get(EEPROM_MAGIC_OFFSET, magic);
	if (magic == eeprom_magic)
		return;

	Serial.println("DEFAULTS!");
	EEPROM.put(EEPROM_MAGIC_OFFSET, eeprom_magic);
	EEPROM.put(EEPROM_NAME_OFFSET, eeprom_default_name);
	EEPROM.put(EEPROM_MAC_OFFSET, eeprom_default_mac);
	EEPROM.put(EEPROM_IP_OFFSET, eeprom_default_ip);
	EEPROM.put(EEPROM_MQTTIP_OFFSET, eeprom_default_mqttip);
	EEPROM.put(EEPROM_FILTER_OFFSET, eeprom_default_filter);
}

void payload_mac_to_eeprom(int offset, int size, byte *payload, int length)
{
	char *pos = (char *) payload;
	uint8_t i;

	for (i = 0; i < 6; i++) {
		EEPROM.write(offset + i, strtol(pos, &pos, 16));
		pos++;
		if (pos >= (char *) payload + length)
			return;
	}
}

void str_to_eeprom(int offset, int size, byte *payload, int length)
{
	char *pos = (char *) payload;
	uint8_t i;

	for (i = 0; i < size; i++) {
		if (i + 1 == size || i >= length)
			EEPROM.write(offset + i, 0);
		else
			EEPROM.write(offset + i, *pos++);
	}
}

void callback(char *topic, byte *payload, unsigned int length)
{
	payload[length] = '\0';

	Serial.println("callback");
	digitalWrite(LED_BUILTIN, HIGH);
	if (!strcmp(topic, config_topic("name"))) {
		str_to_eeprom(EEPROM_NAME_OFFSET, EEPROM_NAME_SIZE,
			      payload, length);
	} else if (!strcmp(topic, config_topic("mac"))) {
		payload_mac_to_eeprom(EEPROM_MAC_OFFSET, EEPROM_MAC_SIZE,
				      payload, length);
	} else if (!strcmp(topic, config_topic("ip"))) {
		IPAddress ip;

		if (ip.fromString((const char *) payload))
			EEPROM.put(EEPROM_IP_OFFSET, ip);
	} else if (!strcmp(topic, config_topic("mqttip"))) {
		IPAddress mqttip;

		if (mqttip.fromString((const char *) payload))
			EEPROM.put(EEPROM_MQTTIP_OFFSET, mqttip);
	} else if (!strcmp(topic, config_topic("filter"))) {
		uint32_t filter = strtol((const char *) payload, NULL, 10);

		if (filter < EEPROM_FILTER_MAX)
			EEPROM.put(EEPROM_FILTER_OFFSET, filter);
	}
	digitalWrite(LED_BUILTIN, LOW);
}

void setup(void)
{
	byte mac[MAC_LEN];
	IPAddress mqttip;
	char macstr[13];
	IPAddress ip;

	Serial.begin(9600);
	Serial.println("MQTTINPUTS");

	eeprom_check();

	EEPROM.get(EEPROM_NAME_OFFSET, name);
	Serial.print("NAME:");
	Serial.println(name);

	EEPROM.get(EEPROM_MAC_OFFSET, mac);
	mac[0] &= 0xfe; /* Clear multicast bit. */
	mac[0] |= 0x02; /* Set local assignment bit. */

	sprintf(macstr, "%02x%02x%02x%02x%02x%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	Serial.print("MAC:");
	Serial.println(macstr);

	EEPROM.get(EEPROM_IP_OFFSET, ip);
	Serial.print("IP:");
	print_ip(ip);

	EEPROM.get(EEPROM_MQTTIP_OFFSET, mqttip);
	Serial.print("MQTTIP:");
	print_ip(mqttip);

	EEPROM.get(EEPROM_FILTER_OFFSET, input_filter);
	Serial.print("FILTER:");
	Serial.println(input_filter);

	Ethernet.begin(mac, ip);
	pins_init();

	client.setServer(mqttip, 1883);
	client.setCallback(callback);
}

#define MQTT_RETRY_TIMEOUT 5000

static bool mqtt_connected;
static unsigned long mqtt_last_attempt;

void loop(void)
{
	unsigned long now = millis();

	if (!client.connected()) {
		if (mqtt_connected) {
			mqtt_last_attempt = 0;
			Serial.println("DISCONNECTED");
			mqtt_connected = false;
		}
		if (!mqtt_last_attempt ||
		    mqtt_last_attempt + MQTT_RETRY_TIMEOUT < now ||
		    mqtt_last_attempt > now) {
			mqtt_last_attempt = now;

			if (client.connect(name)) {
				Serial.println("CONNECTED");
				mqtt_connected = true;
				input_pins_update_state();
				input_pins_publish(false);
				client.subscribe(config_topic("name"));
				client.subscribe(config_topic("mac"));
				client.subscribe(config_topic("ip"));
				client.subscribe(config_topic("mqttip"));
				client.subscribe(config_topic("filter"));
			}
		}
	} else {
		input_pins_update_state();
		input_pins_publish(true);
		client.loop();
	}
}
