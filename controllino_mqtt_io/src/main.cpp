/*
 * Controllino: MQTT I/O client
 * Copyright (c) 2018-2020 Jiri Pirko <jiri@resnulli.us>
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
#include <Controllino.h>
#include <SPI.h>
#include <Ethernet.h>
#include <PubSubClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <EEPROM.h>

EthernetClient ethClient;
PubSubClient client(ethClient);

#define ONE_WIRE_BUS 20

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

#define NAME_SIZE 20
char name[NAME_SIZE];

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

enum pin_type {
	PIN_TYPE_RELAY,
	PIN_TYPE_DIGITAL_OUTPUT,
	PIN_TYPE_PWM_OUTPUT,
	PIN_TYPE_DIGITAL_INPUT,
	PIN_TYPE_ANALOG_INPUT,
	PIN_TYPE_DIGITAL_INPUT_IN,
};

const char *pin_type_subtopic[] = {
	[PIN_TYPE_RELAY] = "relay",
	[PIN_TYPE_DIGITAL_OUTPUT] = "dout",
	[PIN_TYPE_PWM_OUTPUT] = "pwmout",
	[PIN_TYPE_DIGITAL_INPUT] = "din",
	[PIN_TYPE_ANALOG_INPUT] = "ain",
	[PIN_TYPE_DIGITAL_INPUT_IN] = "dinin",
};

struct pin {
	enum pin_type type;
	int index; /* index within the group (R,D,A) */
	uint8_t pin; /* pin number */
	uint8_t new_state_cnt;
	word old_state;
	word state;
	unsigned long on_millis;
};

/* It is not possible to control D20-D23 using digitalWrite()
 * and pinMode() so use PORT register directly. */

bool is_port_pin(uint8_t pin)
{
	return pin >= CONTROLLINO_D20 && pin <= CONTROLLINO_D23;
}

void port_digitalWrite(uint8_t pin, word state)
{
	volatile uint8_t *port;
	uint8_t port_bit;

	switch (pin) {
	case CONTROLLINO_D20:
		port = &PORTD;
		port_bit = 4;
		break;
	case CONTROLLINO_D21:
		port = &PORTD;
		port_bit = 5;
		break;
	case CONTROLLINO_D22:
		port = &PORTD;
		port_bit = 6;
		break;
	case CONTROLLINO_D23:
		port = &PORTJ;
		port_bit = 4;
		break;
	}
	if (state)
		bitSet(*port, port_bit);
	else
		bitClear(*port, port_bit);
}

void port_pinModeOutput(uint8_t pin)
{
	switch (pin) {
	case CONTROLLINO_D20:
		DDRD |= 4;
		break;
	case CONTROLLINO_D21:
		DDRD |= 5;
		break;
	case CONTROLLINO_D22:
		DDRD |= 6;
		break;
	case CONTROLLINO_D23:
		DDRJ |= 4;
		break;
	}
}

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

#define MY_CONTROLLINO_AI0	CONTROLLINO_A0
#define MY_CONTROLLINO_AI1	CONTROLLINO_A1
#define MY_CONTROLLINO_AI2	CONTROLLINO_A2
#define MY_CONTROLLINO_AI3	CONTROLLINO_A3
#define MY_CONTROLLINO_AI4	CONTROLLINO_A4
#define MY_CONTROLLINO_AI5	CONTROLLINO_A5
#define MY_CONTROLLINO_AI6	CONTROLLINO_A6
#define MY_CONTROLLINO_AI7	CONTROLLINO_A7
#define MY_CONTROLLINO_AI8	CONTROLLINO_A8
#define MY_CONTROLLINO_AI9	CONTROLLINO_A9
#define MY_CONTROLLINO_AI10	CONTROLLINO_A10
#define MY_CONTROLLINO_AI11	CONTROLLINO_A11
#define MY_CONTROLLINO_AI12	CONTROLLINO_A12
#define MY_CONTROLLINO_AI13	CONTROLLINO_A13
#define MY_CONTROLLINO_AI14	CONTROLLINO_A14
#define MY_CONTROLLINO_AI15	CONTROLLINO_A15
#define MY_CONTROLLINO_AI16	CONTROLLINO_I16
#define MY_CONTROLLINO_AI17	CONTROLLINO_I17
#define MY_CONTROLLINO_AI18	CONTROLLINO_I18

/* values: 0, 1 */
#define PIN_RELAY(_index) {								\
	.type = PIN_TYPE_RELAY,								\
	.index = _index,								\
	.pin = CONTROLLINO_R##_index,							\
},

/* values: 0, 1 */
#define PIN_DIGITAL_OUTPUT(_index) {							\
	.type = PIN_TYPE_DIGITAL_OUTPUT,						\
	.index = _index,								\
	.pin = CONTROLLINO_D##_index,							\
},

/* values: 0 - 255 */
#define PIN_PWM_OUTPUT(_index) {							\
	.type = PIN_TYPE_PWM_OUTPUT,							\
	.index = _index,								\
	.pin = MY_CONTROLLINO_PWM##_index,						\
},

/* values: 0, 1 */
#define PIN_DIGITAL_INPUT(_index) {							\
	.type = PIN_TYPE_DIGITAL_INPUT,							\
	.index = _index,								\
	.pin = MY_CONTROLLINO_AI##_index,						\
},

#if 0
/* values: 0 - 1023 */
#define PIN_ANALOG_INPUT(_index) {							\
	.type = PIN_TYPE_ANALOG_INPUT,							\
	.index = _index,								\
	.pin = MY_CONTROLLINO_AI##_index,						\
},
#else
#define PIN_ANALOG_INPUT(_index)
#endif

/* values: 0, 1 */
#define PIN_DIGITAL_INPUT_IN(_index) {							\
	.type = PIN_TYPE_DIGITAL_INPUT_IN,						\
	.index = _index,								\
	.pin = CONTROLLINO_IN##_index,							\
},

struct pin pins[] = {
#if defined(CONTROLLINO_MAXI) || defined(CONTROLLINO_MEGA) || defined(CONTROLLINO_MAXI_AUTOMATION)
	PIN_RELAY(0)
	PIN_RELAY(1)
	PIN_RELAY(2)
	PIN_RELAY(3)
	PIN_RELAY(4)
	PIN_RELAY(5)
	PIN_RELAY(6)
	PIN_RELAY(7)
	PIN_RELAY(8)
	PIN_RELAY(9)
#endif
#if defined(CONTROLLINO_MEGA)
	PIN_RELAY(10)
	PIN_RELAY(11)
	PIN_RELAY(12)
	PIN_RELAY(13)
	PIN_RELAY(14)
	PIN_RELAY(15)
#endif
	PIN_DIGITAL_OUTPUT(0)
	PIN_PWM_OUTPUT(0)
	PIN_DIGITAL_OUTPUT(1)
	PIN_PWM_OUTPUT(1)
	PIN_DIGITAL_OUTPUT(2)
	PIN_PWM_OUTPUT(2)
	PIN_DIGITAL_OUTPUT(3)
	PIN_PWM_OUTPUT(3)
	PIN_DIGITAL_OUTPUT(4)
	PIN_PWM_OUTPUT(4)
	PIN_DIGITAL_OUTPUT(5)
	PIN_PWM_OUTPUT(5)
	PIN_DIGITAL_OUTPUT(6)
	PIN_PWM_OUTPUT(6)
	PIN_DIGITAL_OUTPUT(7)
	PIN_PWM_OUTPUT(7)
#if defined(CONTROLLINO_MAXI) || defined(CONTROLLINO_MEGA)
	PIN_DIGITAL_OUTPUT(8)
	PIN_PWM_OUTPUT(8)
	PIN_DIGITAL_OUTPUT(9)
	PIN_PWM_OUTPUT(9)
	PIN_DIGITAL_OUTPUT(10)
	PIN_PWM_OUTPUT(10)
	PIN_DIGITAL_OUTPUT(11)
	PIN_PWM_OUTPUT(11)
#endif
#if defined(CONTROLLINO_MEGA)
	PIN_DIGITAL_OUTPUT(12)
	PIN_DIGITAL_OUTPUT(13)
	PIN_DIGITAL_OUTPUT(14)
	PIN_PWM_OUTPUT(14)
	PIN_DIGITAL_OUTPUT(15)
	PIN_PWM_OUTPUT(15)
	PIN_DIGITAL_OUTPUT(16)
	PIN_PWM_OUTPUT(16)
	PIN_DIGITAL_OUTPUT(17)
	PIN_DIGITAL_OUTPUT(18)
	PIN_DIGITAL_OUTPUT(19)
	PIN_DIGITAL_OUTPUT(20)
	PIN_DIGITAL_OUTPUT(21)
	PIN_DIGITAL_OUTPUT(22)
	PIN_DIGITAL_OUTPUT(23)
	PIN_DIGITAL_INPUT(0)
	PIN_ANALOG_INPUT(0)
	PIN_DIGITAL_INPUT(1)
	PIN_ANALOG_INPUT(1)
	PIN_DIGITAL_INPUT(2)
	PIN_ANALOG_INPUT(2)
	PIN_DIGITAL_INPUT(3)
	PIN_ANALOG_INPUT(3)
	PIN_DIGITAL_INPUT(4)
	PIN_ANALOG_INPUT(4)
	PIN_DIGITAL_INPUT(5)
	PIN_ANALOG_INPUT(5)
	PIN_DIGITAL_INPUT(6)
	PIN_ANALOG_INPUT(6)
	PIN_DIGITAL_INPUT_IN(0)
	PIN_DIGITAL_INPUT_IN(1)
#endif
#if defined(CONTROLLINO_MAXI) || defined(CONTROLLINO_MEGA) || defined(CONTROLLINO_MAXI_AUTOMATION)
	PIN_DIGITAL_INPUT(7)
	PIN_ANALOG_INPUT(7)
	PIN_DIGITAL_INPUT(8)
	PIN_ANALOG_INPUT(8)
	PIN_DIGITAL_INPUT(9)
	PIN_ANALOG_INPUT(9)
#endif
#if defined(CONTROLLINO_MEGA)
	PIN_DIGITAL_INPUT(10)
	PIN_ANALOG_INPUT(10)
	PIN_DIGITAL_INPUT(11)
	PIN_ANALOG_INPUT(11)
	PIN_DIGITAL_INPUT(12)
	PIN_ANALOG_INPUT(12)
	PIN_DIGITAL_INPUT(13)
	PIN_ANALOG_INPUT(13)
	PIN_DIGITAL_INPUT(14)
	PIN_ANALOG_INPUT(14)
	PIN_DIGITAL_INPUT(15)
	PIN_ANALOG_INPUT(15)
	PIN_DIGITAL_INPUT(16)
	PIN_DIGITAL_INPUT(17)
	PIN_DIGITAL_INPUT(18)
#endif
};

#define PINS_COUNT ARRAY_SIZE(pins)

#define for_each_pin(pin, i)								\
	for (i = 0, pin = &pins[i]; i < PINS_COUNT; pin = &pins[++i])

#define TMP_BUF_LEN 128
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
	word new_state;
	unsigned int i;

	for_each_pin(pin, i) {
		switch (pin->type) {
		case PIN_TYPE_DIGITAL_INPUT:
		case PIN_TYPE_DIGITAL_INPUT_IN:
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

uint8_t input_filter;
uint8_t input_threshold;
uint32_t temp_interval;
uint32_t emerg_off_timeout;

void input_pins_publish(bool changed_only)
{
	unsigned int delta;
	struct pin *pin;
	unsigned int i;

	for_each_pin(pin, i) {
		switch (pin->type) {
		case PIN_TYPE_DIGITAL_INPUT:
		case PIN_TYPE_DIGITAL_INPUT_IN:
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
		case PIN_TYPE_ANALOG_INPUT:
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

#define NEW_STATE_EMERG_OFF_MAGIC 0x11

void pin_state_set(struct pin *pin)
{
	if (is_port_pin(pin->pin))
		port_digitalWrite(pin->pin, pin->state);
	else
		digitalWrite(pin->pin, pin->state);
}

void output_pin_update_state(struct pin *pin, word new_state)
{
	pin->state = new_state;
	switch (pin->type) {
	case PIN_TYPE_RELAY: /* fall-through */
	case PIN_TYPE_DIGITAL_OUTPUT:
		pin_state_set(pin);
		if (new_state == NEW_STATE_EMERG_OFF_MAGIC)
			pin->on_millis = millis();
		break;
	case PIN_TYPE_PWM_OUTPUT:
		analogWrite(pin->pin, pin->state);
		break;
	default:
		break;
	}
}

void pins_emerg_off_timeout_check(unsigned long now, uint32_t timeout)
{
	struct pin *pin;
	unsigned int i;

	for_each_pin(pin, i) {
		switch (pin->type) {
		case PIN_TYPE_RELAY: /* fall-through */
		case PIN_TYPE_DIGITAL_OUTPUT: /* fall-through */
			if (pin->state == NEW_STATE_EMERG_OFF_MAGIC &&
			    pin->on_millis + timeout < now) {
				pin->state = 0;
				pin_state_set(pin);
			}
			break;
		default:
			break;
		}
	}
}

void pins_msg_process(const char *topic, const char *value)
{
	struct pin *pin;
	unsigned int i;

	for_each_pin(pin, i) {
		if (pin_type_output(pin) && !strcmp(topic, pin_topic(pin)))
			output_pin_update_state(pin, strtol(value, NULL, 10));
	}
}

void pins_subscribe(void)
{
	struct pin *pin;
	unsigned int i;

	for_each_pin(pin, i) {
		if (pin_type_output(pin))
			client.subscribe(pin_topic(pin));
	}
}

void pins_init(void)
{
	struct pin *pin;
	unsigned int i;

	for_each_pin(pin, i) {
		if (is_port_pin(pin->pin))
			port_pinModeOutput(pin->pin);
		else
			pinMode(pin->pin, pin_type_output(pin) ? OUTPUT : INPUT);
	}
}

static void ow_temp_init(void)
{
	sensors.begin();
	sensors.setWaitForConversion(false);
}

struct ow_temp_device {
	DeviceAddress address;
};

#define OW_TEMP_DEVICE_MAX 16

struct ow_temp {
	bool read_in_progress;
	unsigned long last_read_millis;
	bool waiting_on_conversion;
	unsigned long start_conversion_millis;
	bool had_devices;
	int current_device;
	uint8_t device_count;
	struct ow_temp_device device[OW_TEMP_DEVICE_MAX];
};

static void ow_temp_scan(struct ow_temp *temp, unsigned int interval)
{
	unsigned long now = millis();
	DeviceAddress address;

	if (temp->last_read_millis &&
	    (now - temp->last_read_millis < interval))
		return;

	memset(temp, 0, sizeof(*temp));
	oneWire.reset_search();
	while (oneWire.search(address)) {
		if (OneWire::crc8(address, 7) != address[7] ||
		    temp->device_count == OW_TEMP_DEVICE_MAX)
			continue;
		memcpy(temp->device[temp->device_count++].address, address,
		       sizeof(address));
	}
	if (temp->device_count) {
		temp->read_in_progress = true;
		temp->had_devices = true;
	} else if (temp->had_devices) {
		ow_temp_init();
	}
}

static bool ow_temp_device_next(struct ow_temp *temp)
{
	unsigned long now = millis();

	temp->current_device++;
	if (temp->device_count == temp->current_device) {
		temp->read_in_progress = false;
		temp->last_read_millis = now;
		return false;
	}
	return true;
}

static void ow_temp_conversion_start(struct ow_temp *temp)
{
	while (!sensors.requestTemperaturesByAddress(temp->device[temp->current_device].address)) {
		if (!ow_temp_device_next(temp))
			return;
	}
	temp->waiting_on_conversion = true;
	temp->start_conversion_millis = millis();;
}

char *ow_temp_topic(DeviceAddress deviceAddress)
{
	int offset = 0;
	int i;

	offset = snprintf(tmp_buf, TMP_BUF_LEN, "%s/temp/", name);
	for (i = 0; i < 8; i++)
		offset += snprintf(tmp_buf + offset, TMP_BUF_LEN - offset,
				   "%02x", deviceAddress[i]);
	Serial.println(tmp_buf);
	return tmp_buf;
}

void ow_temp_publish(DeviceAddress deviceAddress, float temp_value)
{
	char temp_buf[16];

//	sprintf(temp_buf, "%f", temp_value);
	dtostrf(temp_value, 4, 2, temp_buf);
	client.publish(ow_temp_topic(deviceAddress), temp_buf);
}

#define OW_TEMP_CONVERSION_TIMEOUT 950

static void ow_temp_conversion_wait_check(struct ow_temp *temp)
{
	unsigned long now = millis();
	float temp_value;

	if (!sensors.isConversionComplete()) {
		if (now - temp->start_conversion_millis > OW_TEMP_CONVERSION_TIMEOUT)
			goto next;
		return;
	}

	temp_value = sensors.getTempC(temp->device[temp->current_device].address);
	if (temp_value != DEVICE_DISCONNECTED_C)
		ow_temp_publish(temp->device[temp->current_device].address,
				temp_value);

next:
	temp->waiting_on_conversion = false;
	ow_temp_device_next(temp);
}

static void ow_temp_process(struct ow_temp *temp, unsigned int interval)
{
	if (!temp->read_in_progress)
		ow_temp_scan(temp, interval);
	else {
		if (!temp->waiting_on_conversion)
			ow_temp_conversion_start(temp);
		else
			ow_temp_conversion_wait_check(temp);
	}
}

static struct ow_temp ow_temp;

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

uint32_t eeprom_magic = 0xa511c31c;
char eeprom_default_name[] = "test";
byte eeprom_default_mac[] = {0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};
IPAddress eeprom_default_ip = IPAddress(172, 22, 1, 10);
IPAddress eeprom_default_mqttip = IPAddress(172, 22, 1, 1);
uint8_t eeprom_default_filter = 16;
uint8_t eeprom_default_threshold = 16;
uint32_t eeprom_default_temp_interval = 60000;
uint32_t eeprom_default_emerg_off_timeout = 30000;
#define EEPROM_FILTER_MAX 32
#define EEPROM_THRESHOLD_MAX 128

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

#define EEPROM_TEMP_INTERVAL_OFFSET EEPROM_THRESHOLD_OFFSET + EEPROM_THRESHOLD_SIZE
#define EEPROM_TEMP_INTERVAL_SIZE sizeof(eeprom_default_temp_interval)

#define EEPROM_EMERG_OFF_TIMEOUT_OFFSET EEPROM_TEMP_INTERVAL_OFFSET + EEPROM_TEMP_INTERVAL_SIZE
#define EEPROM_EMERG_OFF_TIMEOUT_SIZE sizeof(eeprom_default_emerg_off_timeout)

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
	EEPROM.put(EEPROM_TEMP_INTERVAL_OFFSET, eeprom_default_temp_interval);
	EEPROM.put(EEPROM_EMERG_OFF_TIMEOUT_OFFSET, eeprom_default_emerg_off_timeout);
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
			EEPROM.put(EEPROM_FILTER_OFFSET, (uint8_t) filter);
	} else if (!strcmp(topic, config_topic("threshold"))) {
		uint32_t threshold = strtol((const char *) payload, NULL, 10);

		if (threshold < EEPROM_THRESHOLD_MAX)
			EEPROM.put(EEPROM_THRESHOLD_OFFSET, (uint8_t) threshold);
	} else if (!strcmp(topic, config_topic("temp_interval"))) {
		uint32_t temp_interval = strtol((const char *) payload, NULL, 10);

		EEPROM.put(EEPROM_TEMP_INTERVAL_OFFSET, temp_interval);
	} else if (!strcmp(topic, config_topic("emerg_off_timeout"))) {
		uint32_t emerg_off_timeout = strtol((const char *) payload, NULL, 10);

		EEPROM.put(EEPROM_EMERG_OFF_TIMEOUT_OFFSET, emerg_off_timeout);
	} else {
		pins_msg_process(topic, (const char *) payload);
	}
}

void setup(void)
{
	byte mac[MAC_LEN];
	IPAddress mqttip;
	char macstr[13];
	IPAddress ip;

	Serial.begin(9600);
	Serial.println("MQTTIO");

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

	EEPROM.get(EEPROM_TEMP_INTERVAL_OFFSET, temp_interval);
	Serial.print("TEMP_INTERVAL:");
	Serial.println(temp_interval);

	EEPROM.get(EEPROM_EMERG_OFF_TIMEOUT_OFFSET, emerg_off_timeout);
	Serial.print("EMERG_OFF_TIMEOUT:");
	Serial.println(emerg_off_timeout);

	Ethernet.begin(mac, ip);
	pins_init();
	ow_temp_init();

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
				client.subscribe(config_topic("threshold"));
				client.subscribe(config_topic("temp_interval"));
				client.subscribe(config_topic("emerg_off_timeout"));
				pins_subscribe();
			}
		}
	} else {
		input_pins_update_state();
		input_pins_publish(true);
		ow_temp_process(&ow_temp, temp_interval);
		client.loop();
	}
	pins_emerg_off_timeout_check(now, emerg_off_timeout);
}
