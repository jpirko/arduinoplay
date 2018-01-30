/*
 * Controllino: MQTT client
 * Copyright (c) 2018 Jiri Pirko <jiri@resnulli.us>
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
#include <Controllino.h>
#include <EEPROM.h>
#include <SPI.h>
#include <Ethernet.h>
#include <PubSubClient.h>

EthernetClient ethClient;
PubSubClient client(ethClient);

static bool mqtt_connected;
static unsigned long mqtt_last_attempt;

#define NAME_LEN 20
#define ETH_ALEN 6
#define IP_ALEN 4

struct eeprom_name {
	char name[NAME_LEN];
};

struct eeprom_settings {
	struct eeprom_name name;
	uint8_t mac[ETH_ALEN];
	uint32_t ip;
	uint32_t server_ip;
	word server_port;
	bool debug;
};

#define EEPROM_CRC_ADDR 0
#define EEPROM_CRC_LEN sizeof(unsigned long)
#define EEPROM_PAYLOAD_ADDR (EEPROM_CRC_ADDR + EEPROM_CRC_LEN)
#define EEPROM_MAGIC_ADDR (EEPROM_CRC_ADDR + EEPROM_CRC_LEN)
#define EEPROM_MAGIC_LEN sizeof(unsigned long)
#define EEPROM_MAGIC_VAL 0xE38A2C54UL /* This needs to be changed
				       * whenever EEPROM layout is changed
				       */
#define EEPROM_SETTINGS_ADDR (EEPROM_MAGIC_ADDR + EEPROM_MAGIC_LEN)
#define EEPROM_SETTINGS_LEN sizeof(struct eeprom_settings)
#define EEPROM_ALIASES_ADDR (EEPROM_SETTINGS_ADDR + EEPROM_SETTINGS_LEN)
#define EEPROM_ALIAS_LEN sizeof(struct eeprom_name)
#define EEPROM_ALIAS_ADDR(index) (EEPROM_ALIASES_ADDR + \
				  EEPROM_ALIAS_LEN * index)

unsigned long eeprom_crc(void)
{
	const unsigned long crc_table[16] = {
		0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac,
		0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
		0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c,
		0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c
	};
	unsigned long crc = ~0L;
	unsigned int i;

	for (i = EEPROM_MAGIC_ADDR; i < EEPROM.length(); i++) {
		crc = crc_table[(crc ^ EEPROM[i]) & 0x0f] ^ (crc >> 4);
		crc = crc_table[(crc ^ (EEPROM[i] >> 4)) & 0x0f] ^ (crc >> 4);
		crc = ~crc;
	}
	return crc;
}

bool eeprom_crc_ok(void)
{
	unsigned long crc;

	EEPROM.get(EEPROM_CRC_ADDR, crc);
	return crc == eeprom_crc();
}

void eeprom_crc_store(void)
{
	EEPROM.put(EEPROM_CRC_ADDR, eeprom_crc());
}

bool eeprom_magic_ok(void)
{
	unsigned long magic;

	EEPROM.get(EEPROM_MAGIC_ADDR, magic);
	return magic == EEPROM_MAGIC_VAL;
}

void eeprom_magic_store(void)
{
	EEPROM.put(EEPROM_MAGIC_ADDR, EEPROM_MAGIC_VAL);
}

static bool eeprom_ok = false;

void eeprom_reset(void)
{
	unsigned int i;

	Serial.println("EEPROM: Performing reset");
	for (i = 0; i < EEPROM.length(); i++) {
		wdt_reset();
		EEPROM.write(i, 0);
	}
	Serial.println("EEPROM: Storing magic");
	eeprom_magic_store();
	Serial.println("EEPROM: Sroring CRC");
	eeprom_crc_store();
}

void eeprom_check(void)
{
	if (!eeprom_crc_ok()) {
		Serial.println("EEPROM: Incorrect CRC");
		goto eeprom_reset;
	}
	if (!eeprom_magic_ok()) {
		Serial.println("EEPROM: Incorrect magic");
		goto eeprom_reset;
	}
	goto all_fine;

eeprom_reset:
	eeprom_reset();
	if (!eeprom_crc_ok() || !eeprom_magic_ok()) {
		Serial.println("EEPROM: Reset was not successful, disabling EEPROM usage");
		return;
	}

all_fine:
	eeprom_ok = true;
}

struct settings {
	String name;
	uint8_t mac[ETH_ALEN];
	IPAddress ip;
	IPAddress server_ip;
	word server_port;
	bool debug;
};

String eeprom_string_load(struct eeprom_name *name)
{
	name->name[NAME_LEN - 1] = '\0';
	return String(name->name);
}

void eeprom_string_store(struct eeprom_name *name, String str)
{
	if (str.length() >= NAME_LEN)
		Serial.println("EEPROM: Trimming string during store");
	str.toCharArray(name->name, NAME_LEN);
}

void eeprom_settings_load(struct settings *settings)
{
	struct eeprom_settings eeprom_settings;

	memset(settings, 0, sizeof(*settings));
	settings->name = "";
	settings->ip = INADDR_NONE;
	settings->server_ip = INADDR_NONE;
	settings->debug = false;

	if (!eeprom_ok)
		return;

	EEPROM.get(EEPROM_SETTINGS_ADDR, eeprom_settings);

	settings->name = eeprom_string_load(&eeprom_settings.name);
	memcpy(settings->mac, eeprom_settings.mac, ETH_ALEN);
	settings->ip = IPAddress(eeprom_settings.ip);
	settings->server_ip = IPAddress(eeprom_settings.server_ip);
	settings->server_port = eeprom_settings.server_port;
	settings->debug = eeprom_settings.debug;
}

void eeprom_settings_store(struct settings *settings)
{
	struct eeprom_settings eeprom_settings;

	if (!eeprom_ok)
		return;

	eeprom_string_store(&eeprom_settings.name, settings->name);
	memcpy(eeprom_settings.mac, settings->mac, ETH_ALEN);
	eeprom_settings.ip = settings->ip;
	eeprom_settings.server_ip = settings->server_ip;
	eeprom_settings.server_port = settings->server_port;
	eeprom_settings.debug = settings->debug;

	EEPROM.put(EEPROM_SETTINGS_ADDR, eeprom_settings);
	eeprom_crc_store();
}

String eeprom_alias_load(unsigned int index)
{
	struct eeprom_name name;

	if (!eeprom_ok)
		return "";

	EEPROM.get(EEPROM_ALIAS_ADDR(index), name);
	return eeprom_string_load(&name);
}

void eeprom_alias_store(unsigned int index, String alias)
{
	struct eeprom_name name;

	if (!eeprom_ok)
		return;

	eeprom_string_store(&name, alias);

	EEPROM.put(EEPROM_ALIAS_ADDR(index), name);
	eeprom_crc_store();
}

byte zero_mac[ETH_ALEN] = { 0, };

struct settings settings_dflt = {
	.name = "controllino",
	.mac = { 0x00, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE },
	.ip = IPAddress(192, 168, 1, 100),
	.server_ip = IPAddress(192, 168, 1, 1),
	.server_port = 1883,
};

struct settings settings;

void print_mac(uint8_t *mac)
{
	int i;

	for (i = 0; i < ETH_ALEN; i++) {
		if (i)
			Serial.print(":");
		if (mac[i] < 0x10)
			Serial.print(0, HEX);
		Serial.print(mac[i], HEX);
	}
}

void print_ip(IPAddress ip)
{
	int i;

	for (i = 0; i < 4; i++) {
		if (i)
			Serial.print(".");
		Serial.print(ip[i], DEC);
	}
}

void settings_print()
{
	Serial.print("name             = \"");
	Serial.print(settings.name);
	Serial.print("\"\nmac              = \"");
	print_mac(settings.mac);
	Serial.print("\"\nip               = \"");
	print_ip(settings.ip);
	Serial.print("\"\nMQTT server IP   = \"");
	print_ip(settings.server_ip);
	Serial.print("\"\nMQTT server port = \"");
	Serial.print(settings.server_port);
	Serial.print("\"\ndebug            = \"");
	Serial.print(settings.debug ? "true" : "false");
	Serial.println("\"");
}

void settings_load()
{
	eeprom_settings_load(&settings);

	if (!settings.name.length()) {
		Serial.println("Using default \"name\" setting");
		settings.name = settings_dflt.name;
	}
	if (!memcmp(settings.mac, zero_mac, sizeof(settings.mac))) {
		Serial.println("Using default \"mac\" setting");
		memcpy(settings.mac, settings_dflt.mac, sizeof(settings.mac));
	}
	if (settings.ip == INADDR_NONE) {
		Serial.println("Using default \"ip\" setting");
		settings.ip = settings_dflt.ip;
	}
	if (settings.server_ip == INADDR_NONE) {
		Serial.println("Using default \"server_ip\" setting");
		settings.server_ip = settings_dflt.server_ip;
	}
	if (!settings.server_port) {
		Serial.println("Using default \"server_port\" setting");
		settings.server_port = settings_dflt.server_port;
	}

	Serial.println("==========================================");
	settings_print();
	Serial.println("==========================================");
}

void settings_store()
{
	eeprom_settings_store(&settings);
}

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

enum pin_type {
	PIN_TYPE_RELAY,
	PIN_TYPE_DIGITAL_OUTPUT,
	PIN_TYPE_PWM_OUTPUT,
	PIN_TYPE_DIGITAL_INPUT,
	PIN_TYPE_ANALOG_INPUT,
};

String pin_type_subtopic[] = {
	[PIN_TYPE_RELAY] = "relay",
	[PIN_TYPE_DIGITAL_OUTPUT] = "digital_output",
	[PIN_TYPE_PWM_OUTPUT] = "pwm_output",
	[PIN_TYPE_DIGITAL_INPUT] = "digital_input",
	[PIN_TYPE_ANALOG_INPUT] = "analog_input",
};

struct pin {
	enum pin_type type;
	int index; /* index within the group (R,D,A) */
	uint8_t pin; /* pin number */
	word old_state;
	word state;
	String alias;
};

bool pin_type_output(struct pin *pin)
{
	return pin->type == PIN_TYPE_RELAY ||
	       pin->type == PIN_TYPE_DIGITAL_OUTPUT ||
	       pin->type == PIN_TYPE_PWM_OUTPUT;
}

/* Controllino library does not define the PWM pins properly */
#define MY_CONTROLLINO_PWM0	CONTROLLINO_D0
#define MY_CONTROLLINO_PWM1	CONTROLLINO_D1
#define MY_CONTROLLINO_PWM2	CONTROLLINO_D2
#define MY_CONTROLLINO_PWM3	CONTROLLINO_D3
#define MY_CONTROLLINO_PWM4	CONTROLLINO_D4
#define MY_CONTROLLINO_PWM5	CONTROLLINO_D5
#define MY_CONTROLLINO_PWM6	CONTROLLINO_D6
#define MY_CONTROLLINO_PWM7	CONTROLLINO_D7
#define MY_CONTROLLINO_PWM8	CONTROLLINO_D8
#define MY_CONTROLLINO_PWM9	CONTROLLINO_D9
#define MY_CONTROLLINO_PWM10	CONTROLLINO_D10
#define MY_CONTROLLINO_PWM11	CONTROLLINO_D11
#define MY_CONTROLLINO_PWM14	CONTROLLINO_D14
#define MY_CONTROLLINO_PWM15	CONTROLLINO_D15
#define MY_CONTROLLINO_PWM16	CONTROLLINO_D16

/* values: 0, 1 */
#define PIN_RELAY(_index) {								\
	.type = PIN_TYPE_RELAY,								\
	.index = _index,								\
	.pin = CONTROLLINO_R##_index,							\
}

/* values: 0, 1 */
#define PIN_DIGITAL_OUTPUT(_index) {							\
	.type = PIN_TYPE_DIGITAL_OUTPUT,						\
	.index = _index,								\
	.pin = CONTROLLINO_D##_index,							\
}

/* values: 0 - 255 */
#define PIN_PWM_OUTPUT(_index) {							\
	.type = PIN_TYPE_PWM_OUTPUT,							\
	.index = _index,								\
	.pin = MY_CONTROLLINO_PWM##_index,						\
}

/* values: 0, 1 */
#define PIN_DIGITAL_INPUT(_index) {							\
	.type = PIN_TYPE_DIGITAL_INPUT,							\
	.index = _index,								\
	.pin = CONTROLLINO_A##_index,							\
}

/* values: 0 - 1023 */
#define PIN_ANALOG_INPUT(_index) {							\
	.type = PIN_TYPE_ANALOG_INPUT,							\
	.index = _index,								\
	.pin = CONTROLLINO_A##_index,							\
}

struct pin pins[] = {
#if defined(CONTROLLINO_MAXI) || defined(CONTROLLINO_MEGA) || defined(CONTROLLINO_MAXI_AUTOMATION)
	PIN_RELAY(0),
	PIN_RELAY(1),
	PIN_RELAY(2),
	PIN_RELAY(3),
	PIN_RELAY(4),
	PIN_RELAY(5),
	PIN_RELAY(6),
	PIN_RELAY(7),
	PIN_RELAY(8),
	PIN_RELAY(9),
#endif
#if defined(CONTROLLINO_MEGA)
	PIN_RELAY(10),
	PIN_RELAY(11),
	PIN_RELAY(12),
	PIN_RELAY(13),
	PIN_RELAY(14),
	PIN_RELAY(15),
#endif
	PIN_DIGITAL_OUTPUT(0),
	PIN_PWM_OUTPUT(0),
	PIN_DIGITAL_OUTPUT(1),
	PIN_PWM_OUTPUT(1),
	PIN_DIGITAL_OUTPUT(2),
	PIN_PWM_OUTPUT(2),
	PIN_DIGITAL_OUTPUT(3),
	PIN_PWM_OUTPUT(3),
	PIN_DIGITAL_OUTPUT(4),
	PIN_PWM_OUTPUT(4),
	PIN_DIGITAL_OUTPUT(5),
	PIN_PWM_OUTPUT(5),
	PIN_DIGITAL_OUTPUT(6),
	PIN_PWM_OUTPUT(6),
	PIN_DIGITAL_OUTPUT(7),
	PIN_PWM_OUTPUT(7),
#if defined(CONTROLLINO_MAXI) || defined(CONTROLLINO_MEGA)
	PIN_DIGITAL_OUTPUT(8),
	PIN_PWM_OUTPUT(8),
	PIN_DIGITAL_OUTPUT(9),
	PIN_PWM_OUTPUT(9),
	PIN_DIGITAL_OUTPUT(10),
	PIN_PWM_OUTPUT(10),
	PIN_DIGITAL_OUTPUT(11),
	PIN_PWM_OUTPUT(11),
#endif
#if defined(CONTROLLINO_MEGA)
	PIN_DIGITAL_OUTPUT(12),
	PIN_DIGITAL_OUTPUT(13),
	PIN_DIGITAL_OUTPUT(14),
	PIN_PWM_OUTPUT(14),
	PIN_DIGITAL_OUTPUT(15),
	PIN_PWM_OUTPUT(15),
	PIN_DIGITAL_OUTPUT(16),
	PIN_PWM_OUTPUT(16),
	PIN_DIGITAL_OUTPUT(17),
	PIN_DIGITAL_OUTPUT(18),
	PIN_DIGITAL_OUTPUT(19),
	PIN_DIGITAL_INPUT(0),
	PIN_ANALOG_INPUT(0),
	PIN_DIGITAL_INPUT(1),
	PIN_ANALOG_INPUT(1),
	PIN_DIGITAL_INPUT(2),
	PIN_ANALOG_INPUT(2),
	PIN_DIGITAL_INPUT(3),
	PIN_ANALOG_INPUT(3),
	PIN_DIGITAL_INPUT(4),
	PIN_ANALOG_INPUT(4),
	PIN_DIGITAL_INPUT(5),
	PIN_ANALOG_INPUT(5),
	PIN_DIGITAL_INPUT(6),
	PIN_ANALOG_INPUT(6),
#endif
#if defined(CONTROLLINO_MAXI) || defined(CONTROLLINO_MEGA) || defined(CONTROLLINO_MAXI_AUTOMATION)
	PIN_DIGITAL_INPUT(7),
	PIN_ANALOG_INPUT(7),
	PIN_DIGITAL_INPUT(8),
	PIN_ANALOG_INPUT(8),
	PIN_DIGITAL_INPUT(9),
	PIN_ANALOG_INPUT(9),
#endif
#if defined(CONTROLLINO_MEGA)
	PIN_DIGITAL_INPUT(10),
	PIN_ANALOG_INPUT(10),
	PIN_DIGITAL_INPUT(11),
	PIN_ANALOG_INPUT(11),
	PIN_DIGITAL_INPUT(12),
	PIN_ANALOG_INPUT(12),
	PIN_DIGITAL_INPUT(13),
	PIN_ANALOG_INPUT(13),
	PIN_DIGITAL_INPUT(14),
	PIN_ANALOG_INPUT(14),
	PIN_DIGITAL_INPUT(15),
	PIN_ANALOG_INPUT(15),
#endif
};

#define PINS_COUNT ARRAY_SIZE(pins)

#define for_each_pin(pin, i)								\
	for (i = 0, pin = &pins[i]; i < PINS_COUNT; pin = &pins[++i])

String pin_topic(struct pin *pin)
{
	return settings.name + "/" +
	       pin_type_subtopic[pin->type] + "/" +
	       pin->index;
}

String pin_topic_set(struct pin *pin)
{
	return pin_topic(pin) + "/set";
}

String pin_topic_alias(struct pin *pin)
{
	return pin_topic(pin) + "/alias";
}

bool pin_alias_exists(struct pin *pin)
{
	return pin->alias != "";
}

String pin_alias_topic(struct pin *pin)
{
	return settings.name + "/" + pin->alias;
}

String pin_alias_topic_set(struct pin *pin)
{
	return pin_alias_topic(pin) + "/set";
}

void pin_publish(struct pin *pin)
{
	char state_buf[16];

	String(pin->state).toCharArray(state_buf, sizeof(state_buf));
	client.publish(pin_topic(pin).c_str(), state_buf);
	if (pin_alias_exists(pin))
		client.publish(pin_alias_topic(pin).c_str(), state_buf);
}

void input_pins_update_state()
{
	struct pin *pin;
	word new_state;
	unsigned int i;

	for_each_pin(pin, i) {
		switch (pin->type) {
		case PIN_TYPE_DIGITAL_INPUT:
			new_state = digitalRead(pin->pin);
			break;
		case PIN_TYPE_ANALOG_INPUT:
			new_state = analogRead(pin->pin);
			break;
		default:
			continue;
		}
		pin->state = new_state;
	}
}

#define ANALOG_THRESHOLD 10

void input_pins_publish(bool changed_only)
{
	struct pin *pin;
	unsigned int i;

	for_each_pin(pin, i) {
		switch (pin->type) {
		case PIN_TYPE_DIGITAL_INPUT:
			if (pin->old_state == pin->state && changed_only)
				continue;
			pin_publish(pin);
			if (settings.debug)
				Serial.println((String) "Updating digital input D" +
					       pin->index + " to " + pin->state);
			pin->old_state = pin->state;
			break;
		case PIN_TYPE_ANALOG_INPUT:
			if (abs((int) pin->old_state - (int) pin->state) < ANALOG_THRESHOLD &&
			    changed_only)
				continue;
			pin_publish(pin);
			if (settings.debug)
				Serial.println((String) "Updating analog input A" +
					       pin->index + " to " + pin->state);
			pin->old_state = pin->state;
			break;
		default:
			continue;
		}
	}
}

void output_pin_update_state(struct pin *pin, word new_state)
{
	if (pin->state == new_state)
		return;
	switch (pin->type) {
	case PIN_TYPE_RELAY:
		pin->state = new_state;
		if (settings.debug)
			Serial.println((String) "Setting relay R" +
				       pin->index + " to " + pin->state);
		digitalWrite(pin->pin, pin->state);
		pin_publish(pin);
		break;
	case PIN_TYPE_DIGITAL_OUTPUT:
		pin->state = new_state;
		if (settings.debug)
			Serial.println((String) "Setting digital output D" +
				       pin->index + " to " + pin->state);
		digitalWrite(pin->pin, pin->state);
		pin_publish(pin);
		break;
	case PIN_TYPE_PWM_OUTPUT:
		pin->state = new_state;
		if (settings.debug)
			Serial.println((String) "Setting PWM output D" +
				       pin->index + " to " + pin->state);
		analogWrite(pin->pin, pin->state);
		pin_publish(pin);
		break;
	default:
		break;
	}
}

void pin_alias_set(struct pin *pin, unsigned int pin_index, String alias)
{
	if (pin_alias_exists(pin))
		client.unsubscribe((pin_alias_topic_set(pin)).c_str());
	pin->alias = alias;
	eeprom_alias_store(pin_index, pin->alias);
	client.subscribe((pin_alias_topic_set(pin)).c_str());
	if (settings.debug)
		Serial.println((String) "Set alias \"" + pin_topic(pin) +
			       "\" -> \"" + pin_alias_topic(pin) + "\"");
}

void pins_msg_process(String topic, String value)
{
	struct pin *pin;
	unsigned int i;

	if (settings.debug)
		Serial.println((String) "Processing topic \"" + topic +
			       "\" with value \"" + value + "\"");
	for_each_pin(pin, i) {
		if (pin_type_output(pin) &&
		    (pin_topic_set(pin) == topic ||
		     (pin_alias_exists(pin) &&
		      pin_alias_topic_set(pin) == topic)))
			output_pin_update_state(pin, value.toInt());
		else if (pin_topic_alias(pin) == topic)
			pin_alias_set(pin, i, value);
	}
}

void pins_subscribe(void)
{
	struct pin *pin;
	unsigned int i;

	for_each_pin(pin, i) {
		if (pin_type_output(pin)) {
			client.subscribe((pin_topic_set(pin)).c_str());
			if (pin_alias_exists(pin))
				client.subscribe((pin_alias_topic_set(pin)).c_str());
		}
		client.subscribe((pin_topic_alias(pin)).c_str());
	}
}

void pins_init(void)
{
	struct pin *pin;
	unsigned int i;

	for_each_pin(pin, i) {
		pin->alias = eeprom_alias_load(i);
		pinMode(pin->pin, pin_type_output(pin) ? OUTPUT : INPUT);
	}
}

void callback(char *topic, byte *payload, unsigned int length)
{
	payload[length] = '\0';
	pins_msg_process(String(topic), String((char *) payload));
}

void delay_with_wdt(unsigned int ms)
{
	unsigned int sleep;

	while (ms > 0) {
		wdt_reset();
		sleep = ms < 1000 ? ms : 1000;
		delay(sleep);
		ms -= sleep;
	}
}

String cmdline_cut(String *cmdline)
{
	int space_index = (*cmdline).indexOf(' ');
	String first;
	String rest;

	if (space_index == -1) {
		first = *cmdline;
		*cmdline = "";
	} else {
		first = (*cmdline).substring(0, space_index);
		first.trim();
		rest = (*cmdline).substring(space_index);
		rest.trim();
		*cmdline = rest;
	}
	return first;
}

void cmd_reset(void)
{
	Serial.println("Performing system reset");
	delay(200);
	wdt_enable(WDTO_15MS);
	for(;;);
}

void cmd_factory_reset(void)
{
	eeprom_reset();
	cmd_reset();
}

void cmd_status(String cmdline)
{
	String cmd;

	cmd = cmdline_cut(&cmdline);
	if (cmd == "" || cmd == "print")
		Serial.println(mqtt_connected ? "connected" : "disconnected");
	else
		Serial.println((String) "Unknown command \"" + cmd + "\"");
}

void cmd_settings_set(String cmdline)
{
	String cmd;

	cmd = cmdline_cut(&cmdline);
	if (cmd == "") {
		Serial.println("Failed to parse command line");
		return;
	}
	if (cmd == "name") {
		if (cmdline == "") {
			Serial.println("Invalid name");
			return;
		}
		settings.name = cmdline;
	} else if (cmd == "ip") {
		IPAddress ip;

		if (!ip.fromString(cmdline)) {
			Serial.println("Invalid IP");
			return;
		}
		settings.ip = ip;
	} else if (cmd == "server_ip") {
		IPAddress ip;

		if (!ip.fromString(cmdline)) {
			Serial.println("Invalid IP");
			return;
		}
		settings.server_ip = ip;
	} else if (cmd == "server_port") {
		long port = cmdline.toInt();

		if (port < 0 || port > 0xffff) {
			Serial.println("Invalid port");
			return;
		}
		settings.server_port = port;
	} else if (cmd == "debug") {
		if (cmdline == "true")
			settings.debug = true;
		else if (cmdline == "false")
			settings.debug = false;
		else
			Serial.println("Invalid value (not \"true\" or \"false\"");
	} else {
		Serial.println((String) "Unknown command \"" + cmd + "\"");
	}
}

void cmd_settings(String cmdline)
{
	String cmd;

	cmd = cmdline_cut(&cmdline);
	if (cmd == "" || cmd == "print")
		settings_print();
	else if (cmd == "store")
		settings_store();
	else if (cmd == "set")
		cmd_settings_set(cmdline);
	else
		Serial.println((String) "Unknown command \"" + cmd + "\"");
}

void cmd_topics_print(void)
{
	struct pin *pin;
	unsigned int i;

	for_each_pin(pin, i) {
		if (pin_type_output(pin)) {
			Serial.println((String) "S \"" + pin_topic_set(pin) + "\"");
			if (pin_alias_exists(pin))
				Serial.println((String) "S \"" + pin_alias_topic_set(pin) + "\"");
		}
		Serial.println((String) "S \"" + pin_topic_alias(pin) + "\"");
		Serial.println((String) "P \"" + pin_topic(pin) + "\"");
		if (pin_alias_exists(pin))
			Serial.println((String) "P \"" + pin_alias_topic(pin) + "\"");
	}
}

void cmd_topics(String cmdline)
{
	String cmd;

	cmd = cmdline_cut(&cmdline);
	if (cmd == "" || cmd == "print")
		cmd_topics_print();
	else
		Serial.println((String) "Unknown command \"" + cmd + "\"");
}

void cmd_aliases_print(void)
{
	struct pin *pin;
	unsigned int i;

	for_each_pin(pin, i) {
		if (!pin_alias_exists(pin))
			continue;
		Serial.println((String) "\"" + pin_topic(pin) +
			       "\" -> \"" + pin_alias_topic(pin) + "\"");
	}
}

void cmd_aliases(String cmdline)
{
	String cmd;

	cmd = cmdline_cut(&cmdline);
	if (cmd == "" || cmd == "print")
		cmd_aliases_print();
	else
		Serial.println((String) "Unknown command \"" + cmd + "\"");
}

void cmd_help(void)
{
	Serial.println("Available commands:");
	Serial.println("  help");
	Serial.println("  reset");
	Serial.println("  factory_reset");
	Serial.println("  status print");
	Serial.println("  settings print");
	Serial.println("  settings store");
	Serial.println("  settings set name NAME");
	Serial.println("  settings set ip IPADDRESS");
	Serial.println("  settings set server_ip IPADDRESS");
	Serial.println("  settings set server_port PORT");
	Serial.println("  topics print");
	Serial.println("  aliases print");
}

void cmdline_exec(String cmdline)
{
	String cmd;

	cmd = cmdline_cut(&cmdline);
	if (cmd == "")
		return;
	if (cmd == "help" || cmd == "?")
		cmd_help();
	else if (cmd == "reset")
		cmd_reset();
	else if (cmd == "factory_reset")
		cmd_factory_reset();
	else if (cmd == "status")
		cmd_status(cmdline);
	else if (cmd == "settings")
		cmd_settings(cmdline);
	else if (cmd == "topics")
		cmd_topics(cmdline);
	else if (cmd == "aliases")
		cmd_aliases(cmdline);
	else
		Serial.println((String) "Unknown command \"" + cmd + "\"");
}

String cmdline = "";

void cmdline_process(void)
{
	char c;

	if (!Serial.available())
		return;

	c = Serial.read();
	Serial.print(c);
	cmdline += c;
	if (c != '\n')
		return;
	cmdline.trim();
	cmdline_exec(cmdline);
	cmdline = "";
	Serial.print("$ ");
}

void setup(void)
{
	wdt_enable(WDTO_8S);
	Serial.begin(9600);
	Serial.println("Controllino MQTT client");

	eeprom_check();
	settings_load();
	wdt_reset();

	pins_init();
	wdt_reset();

	client.setServer(settings.server_ip, settings.server_port);
	client.setCallback(callback);
	Ethernet.begin(settings.mac, settings.ip);
}

#define MQTT_RETRY_TIMEOUT 5000

void loop(void)
{
	unsigned long now = millis();

	wdt_reset();

	if (!client.connected()) {
		if (mqtt_connected) {
			mqtt_last_attempt = 0;
			if (settings.debug)
				Serial.println("Disconnected from MQTT server");
			mqtt_connected = false;
		}
		if (!mqtt_last_attempt ||
		    mqtt_last_attempt + MQTT_RETRY_TIMEOUT < now ||
		    mqtt_last_attempt > now) {
			mqtt_last_attempt = now;
			if (client.connect(settings.name.c_str())) {
				if (settings.debug)
					Serial.println("Connected to MQTT server");
				mqtt_connected = true;
				input_pins_update_state();
				input_pins_publish(false);
				pins_subscribe();
			}
		}
	} else {
		input_pins_update_state();
		input_pins_publish(true);
		client.loop();
	}

	cmdline_process();
}
