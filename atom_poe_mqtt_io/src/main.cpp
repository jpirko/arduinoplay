/*
 * AtomPoE: MQTT I/O client
 * Copyright (c) 2023 Jiri Pirko <jiri@resnulli.us>
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

#include <M5Atom.h>
#include <SPI.h>
#include <Ethernet.h>
#include <PubSubClient.h>
#include <EEPROM.h>

#define ETHERNET_SCK 22
#define ETHERNET_MISO 23
#define ETHERNET_MOSI 33
#define ETHERNET_CS 19

EthernetClient ethClient;
PubSubClient client(ethClient);

#define NAME_SIZE 20
char name[NAME_SIZE];

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

enum pin_type {
	PIN_TYPE_G,
};

enum pin_flavour {
	PIN_FLAVOUR_DISABLED,
	PIN_FLAVOUR_DIGITAL_INPUT,
	PIN_FLAVOUR_ANALOG_INPUT,
	PIN_FLAVOUR_DIGITAL_OUTPUT,
	PIN_FLAVOUR_PWM_OUTPUT,
};

const char *pin_type_subtopic[] = {
	[PIN_TYPE_G] = "G",
};

const char *pin_flavour_subtopic[] = {
	[PIN_FLAVOUR_DISABLED] = "disabled",
	[PIN_FLAVOUR_DIGITAL_INPUT] = "din",
	[PIN_FLAVOUR_ANALOG_INPUT] = "ain",
	[PIN_FLAVOUR_DIGITAL_OUTPUT] = "dout",
	[PIN_FLAVOUR_PWM_OUTPUT] = "pwmout",
};

struct pin {
	uint8_t type;
	uint8_t index; /* index within the group */
	uint8_t pin; /* pin number */
	uint16_t old_state;
	uint16_t state;
	uint8_t new_state_cnt;
	uint8_t flavour;
};

bool is_pin_output(struct pin *pin)
{
	return pin->flavour == PIN_FLAVOUR_DIGITAL_OUTPUT ||
	       pin->flavour == PIN_FLAVOUR_PWM_OUTPUT;
}

#define PIN_G(_index, _pin) {								\
	.type = PIN_TYPE_G,								\
	.index = _index,								\
	.pin = _pin,									\
}

struct pin pins[] = {
	PIN_G(26, 26), /* G26 */
	PIN_G(32, 32), /* G32 */
};

#define PINS_COUNT ARRAY_SIZE(pins)

struct pin_flavours {
	uint8_t pin_flavours[PINS_COUNT];
};

#define for_each_pin(pin, i)								\
	for (i = 0, pin = &pins[i]; i < PINS_COUNT; pin = &pins[++i])

#define TMP_BUF_LEN 64
char tmp_buf[TMP_BUF_LEN];

char *pin_topic(struct pin *pin)
{
	snprintf(tmp_buf, TMP_BUF_LEN, "%s/%s/%s%u", name,
		 pin_flavour_subtopic[pin->flavour],
		 pin_type_subtopic[pin->type], pin->index);
	return tmp_buf;
}

char *pin_config_topic(struct pin *pin)
{
	snprintf(tmp_buf, TMP_BUF_LEN, "%s/config/%s%u", name,
		 pin_type_subtopic[pin->type], pin->index);
	return tmp_buf;
}

struct pin_flavours pin_flavours;

void load_print_pin_flavours(void)
{
	struct pin *pin;
	uint8_t i;

	for_each_pin(pin, i) {
		Serial.print(pin_type_subtopic[pin->type]);
		Serial.print(pin->index);
		Serial.print(": ");
		pin->flavour = pin_flavours.pin_flavours[i];
		Serial.println(pin_flavour_subtopic[pin->flavour]);
	}
}

void pin_publish(struct pin *pin)
{
	char state_buf[16];

	sprintf(state_buf, "%u", pin->state);
	client.publish(pin_topic(pin), state_buf);
}

void input_pins_update_state()
{
	uint16_t new_state;
	struct pin *pin;
	uint8_t i;

	for_each_pin(pin, i) {
		switch (pin->flavour) {
		case PIN_FLAVOUR_DIGITAL_INPUT:
			new_state = digitalRead(pin->pin);
			break;
		case PIN_FLAVOUR_ANALOG_INPUT:
			new_state = analogRead(pin->pin);
			break;
		default:
			continue;
		}
		pin->state = new_state;
	}
}

uint8_t input_filter;
uint8_t input_threshold;

void input_pins_publish(bool changed_only)
{
	unsigned int delta;
	struct pin *pin;
	uint8_t i;

	for_each_pin(pin, i) {
		switch (pin->flavour) {
		case PIN_FLAVOUR_DIGITAL_INPUT:
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
		case PIN_FLAVOUR_ANALOG_INPUT:
			delta = abs((int) pin->old_state - (int) pin->state);
			if (delta < input_threshold && changed_only)
				continue;
			pin_publish(pin);
			pin->old_state = pin->state;
			pin->new_state_cnt = 0;
			break;
		default:
			continue;
		}
	}
}

void output_pin_update_state(struct pin *pin, uint8_t new_state)
{
	pin->state = new_state;
	switch (pin->flavour) {
	case PIN_FLAVOUR_DIGITAL_OUTPUT:
		digitalWrite(pin->pin, pin->state);
		break;
	case PIN_FLAVOUR_PWM_OUTPUT:
		analogWrite(pin->pin, pin->state);
		break;
	}
}

void pins_msg_process(const char *topic, const char *value)
{
	struct pin *pin;
	unsigned int i;

	for_each_pin(pin, i) {
		if (is_pin_output(pin) && !strcmp(topic, pin_topic(pin)))
			output_pin_update_state(pin, strtol(value, NULL, 10));
	}
}

void pins_subscribe(void)
{
	struct pin *pin;
	unsigned int i;

	for_each_pin(pin, i) {
		client.subscribe(pin_config_topic(pin));
		if (is_pin_output(pin))
			client.subscribe(pin_topic(pin));
	}
}

void pins_init(void)
{
	struct pin *pin;
	uint8_t i;

	for_each_pin(pin, i) {
		switch (pin->flavour){
		case PIN_FLAVOUR_DIGITAL_INPUT:
			pinMode(pin->pin, INPUT_PULLUP);
			break;
		case PIN_FLAVOUR_ANALOG_INPUT:
			pinMode(pin->pin, INPUT);
			break;
		case PIN_FLAVOUR_DIGITAL_OUTPUT: /* fall-through */
		case PIN_FLAVOUR_PWM_OUTPUT:
			pinMode(pin->pin, OUTPUT);
			break;
		}
	}
}

char *config_topic(const char *item)
{
	snprintf(tmp_buf, TMP_BUF_LEN, "%s/config/%s", name, item);
	return tmp_buf;
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

uint32_t eeprom_magic = 0x6a3b6fac;
char eeprom_default_name[] = "test";
byte eeprom_default_mac[] = {0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};
IPAddress eeprom_default_ip = IPAddress(172, 22, 1, 10);
IPAddress eeprom_default_mqttip = IPAddress(172, 22, 1, 1);
uint8_t eeprom_default_filter = 16;
uint8_t eeprom_default_threshold = 16;
#define EEPROM_FILTER_MAX 32
#define EEPROM_THRESHOLD_MAX 255
static struct pin_flavours eeprom_default_pin_flavours; /* all zeroes means all are disabled */


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

#define EEPROM_THRESHOLD_OFFSET EEPROM_FILTER_OFFSET + EEPROM_FILTER_SIZE
#define EEPROM_THRESHOLD_SIZE sizeof(eeprom_default_threshold)

#define EEPROM_PIN_FLAVOURS_OFFSET EEPROM_THRESHOLD_OFFSET + EEPROM_THRESHOLD_SIZE
#define EEPROM_PIN_FLAVOURS_SIZE sizeof(eeprom_default_pin_flavours)

#define EEPROM_SIZE EEPROM_PIN_FLAVOURS_OFFSET + EEPROM_PIN_FLAVOURS_SIZE

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
	EEPROM.put(EEPROM_THRESHOLD_OFFSET, eeprom_default_threshold);
	EEPROM.put(EEPROM_PIN_FLAVOURS_OFFSET, eeprom_default_pin_flavours);
	EEPROM.commit();
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
	EEPROM.commit();
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
	EEPROM.commit();
}

bool check_pin_flavours_to_eeprom(char *topic, byte *payload,
				  unsigned int length)
{
	struct pin *pin;
	uint8_t i;

	for_each_pin(pin, i) {
		if (strcmp(topic, pin_config_topic(pin)))
			continue;
		if (!strcmp((const char *) payload,
			    pin_flavour_subtopic[PIN_FLAVOUR_DIGITAL_INPUT]))
			pin_flavours.pin_flavours[i] = PIN_FLAVOUR_DIGITAL_INPUT;
		else if (!strcmp((const char *) payload,
				 pin_flavour_subtopic[PIN_FLAVOUR_ANALOG_INPUT]))
			pin_flavours.pin_flavours[i] = PIN_FLAVOUR_ANALOG_INPUT;
		else if (!strcmp((const char *) payload,
				 pin_flavour_subtopic[PIN_FLAVOUR_DIGITAL_OUTPUT]))
			pin_flavours.pin_flavours[i] = PIN_FLAVOUR_DIGITAL_OUTPUT;
		else if (!strcmp((const char *) payload,
				 pin_flavour_subtopic[PIN_FLAVOUR_PWM_OUTPUT]))
			pin_flavours.pin_flavours[i] = PIN_FLAVOUR_PWM_OUTPUT;
		else
			continue;
		EEPROM.put(EEPROM_PIN_FLAVOURS_OFFSET, pin_flavours);
		EEPROM.commit();
		return true;
	}
	return false;
}

void callback(char *topic, byte *payload, unsigned int length)
{
	payload[length] = '\0';

	M5.dis.drawpix(0, CRGB::Blue);
	if (!strcmp(topic, config_topic("name"))) {
		str_to_eeprom(EEPROM_NAME_OFFSET, EEPROM_NAME_SIZE,
			      payload, length);
	} else if (!strcmp(topic, config_topic("mac"))) {
		payload_mac_to_eeprom(EEPROM_MAC_OFFSET, EEPROM_MAC_SIZE,
				      payload, length);
	} else if (!strcmp(topic, config_topic("ip"))) {
		IPAddress ip;

		if (ip.fromString((const char *) payload)) {
			EEPROM.put(EEPROM_IP_OFFSET, ip);
			EEPROM.commit();
		}
	} else if (!strcmp(topic, config_topic("mqttip"))) {
		IPAddress mqttip;

		if (mqttip.fromString((const char *) payload)) {
			EEPROM.put(EEPROM_MQTTIP_OFFSET, mqttip);
			EEPROM.commit();
		}
	} else if (!strcmp(topic, config_topic("filter"))) {
		uint32_t filter = strtol((const char *) payload, NULL, 10);

		if (filter < EEPROM_FILTER_MAX) {
			EEPROM.put(EEPROM_FILTER_OFFSET, filter);
			EEPROM.commit();
		}
	} else if (!strcmp(topic, config_topic("threshold"))) {
		uint32_t threshold = strtol((const char *) payload, NULL, 10);

		if (threshold < EEPROM_THRESHOLD_MAX) {
			EEPROM.put(EEPROM_THRESHOLD_OFFSET, (uint8_t) threshold);
			EEPROM.commit();
		}
	} else if (check_pin_flavours_to_eeprom(topic, payload, length)) {
	} else {
		pins_msg_process(topic, (const char *) payload);
	}
	M5.dis.drawpix(0, CRGB::Green);
}

void setup(void)
{
	byte mac[MAC_LEN];
	IPAddress mqttip;
	char macstr[13];
	IPAddress ip;

	SPI.begin(ETHERNET_SCK, ETHERNET_MISO, ETHERNET_MOSI, -1);
	Ethernet.init(ETHERNET_CS);

	M5.begin(true, false, true);
	M5.dis.setBrightness(10);
	M5.dis.drawpix(0, CRGB::Red);

	Serial.println("MQTTIO");

	EEPROM.begin(EEPROM_SIZE);
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
	
	EEPROM.get(EEPROM_THRESHOLD_OFFSET, input_threshold);
	Serial.print("THRESHOLD:");
	Serial.println(input_threshold);

	EEPROM.get(EEPROM_PIN_FLAVOURS_OFFSET, pin_flavours);
	Serial.println("PIN FLAVOURS:");
	load_print_pin_flavours();

	Ethernet.begin(mac, ip);
	pins_init();

	client.setServer(mqttip, 1883);
	client.setCallback(callback);
	M5.dis.drawpix(0, CRGB::Orange);
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
			M5.dis.drawpix(0, CRGB::Orange);
		}
		if (!mqtt_last_attempt ||
		    mqtt_last_attempt + MQTT_RETRY_TIMEOUT < now ||
		    mqtt_last_attempt > now) {
			mqtt_last_attempt = now;

			if (client.connect(name)) {
				Serial.println("CONNECTED");
				mqtt_connected = true;
				M5.dis.drawpix(0, CRGB::Green);
				input_pins_update_state();
				input_pins_publish(false);
				client.subscribe(config_topic("name"));
				client.subscribe(config_topic("mac"));
				client.subscribe(config_topic("ip"));
				client.subscribe(config_topic("mqttip"));
				client.subscribe(config_topic("filter"));
				client.subscribe(config_topic("threshold"));
				pins_subscribe();
			}
		}
	} else {
		input_pins_update_state();
		input_pins_publish(true);
		client.loop();
	}
}
