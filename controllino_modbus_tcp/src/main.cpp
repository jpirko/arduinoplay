/*
 * Controllino: Modbus TCP slave
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
#include <Controllino.h>
#include <SPI.h>
#include <Ethernet.h>
#include <Modbus.h>
#include <ModbusIP.h>

ModbusIP mb;

byte mac[] = {
	0x00, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE
};

byte ip[] = {
	172, 22, 1, 100
};

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

enum pin_type {
	PIN_TYPE_RELAY,
	PIN_TYPE_DIGITAL_OUTPUT,
	PIN_TYPE_PWM_OUTPUT,
	PIN_TYPE_DIGITAL_INPUT,
	PIN_TYPE_ANALOG_INPUT,
};

struct pin {
	enum pin_type type;
	int index; /* index within the group (R,D,A) */
	uint8_t pin; /* pin number */
	word mb_index; /* index within the group on modbus */
	word state;
};

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

#define PIN_RELAY_MB_INDEX_BASE 100
#define PIN_DIGITAL_OUTPUT_MB_INDEX_BASE 200
#define PIN_PWM_OUTPUT_MB_INDEX_BASE 0
#define PIN_DIGITAL_INPUT_MB_INDEX_BASE 0
#define PIN_ANALOG_INPUT_MB_INDEX_BASE 0

/* modbus function code: 0x05
 * offset: PIN_RELAY_MB_INDEX_BASE
 * values: 0, 1
 */
#define PIN_RELAY(_index, _mb_index) {							\
	.type = PIN_TYPE_RELAY,								\
	.index = _index,								\
	.pin = CONTROLLINO_R##_index,							\
	.mb_index = _mb_index + PIN_RELAY_MB_INDEX_BASE,				\
}

/* modbus function code: 0x05
 * offset: PIN_DIGITAL_OUTPUT_MB_INDEX_BASE
 * values: 0, 1
 */
#define PIN_DIGITAL_OUTPUT(_index, _mb_index) {						\
	.type = PIN_TYPE_DIGITAL_OUTPUT,						\
	.index = _index,								\
	.pin = CONTROLLINO_D##_index,							\
	.mb_index = _mb_index + PIN_DIGITAL_OUTPUT_MB_INDEX_BASE,			\
}

/* modbus function code: 0x06
 * offset: PIN_PWM_OUTPUT_MB_INDEX_BASE
 * values: 0 - 255
 */
#define PIN_PWM_OUTPUT(_index, _mb_index) {						\
	.type = PIN_TYPE_PWM_OUTPUT,							\
	.index = _index,								\
	.pin = MY_CONTROLLINO_PWM##_index,						\
	.mb_index = _mb_index + PIN_PWM_OUTPUT_MB_INDEX_BASE,				\
}

/* modbus function code: 0x02
 * offset: PIN_DIGITAL_INPUT_MB_INDEX_BASE
 * values: 0, 1
 */
#define PIN_DIGITAL_INPUT(_index, _mb_index) {						\
	.type = PIN_TYPE_DIGITAL_INPUT,							\
	.index = _index,								\
	.pin = CONTROLLINO_A##_index,							\
	.mb_index = _mb_index + PIN_DIGITAL_INPUT_MB_INDEX_BASE,			\
}

/* modbus function code: 0x04
 * offset: PIN_ANALOG_INPUT_MB_INDEX_BASE
 * values: 0 - 1023
 */
#define PIN_ANALOG_INPUT(_index, _mb_index) {						\
	.type = PIN_TYPE_ANALOG_INPUT,							\
	.index = _index,								\
	.pin = CONTROLLINO_A##_index,							\
	.mb_index = _mb_index + PIN_ANALOG_INPUT_MB_INDEX_BASE,				\
}

struct pin pins[] = {
#if defined(CONTROLLINO_MAXI) || defined(CONTROLLINO_MEGA) || defined(CONTROLLINO_MAXI_AUTOMATION)
	PIN_RELAY(0, 0),
	PIN_RELAY(1, 1),
	PIN_RELAY(2, 2),
	PIN_RELAY(3, 3),
	PIN_RELAY(4, 4),
	PIN_RELAY(5, 5),
	PIN_RELAY(6, 6),
	PIN_RELAY(7, 7),
	PIN_RELAY(8, 8),
	PIN_RELAY(9, 9),
#endif
#if defined(CONTROLLINO_MEGA)
	PIN_RELAY(10, 10),
	PIN_RELAY(11, 11),
	PIN_RELAY(12, 12),
	PIN_RELAY(13, 13),
	PIN_RELAY(14, 14),
	PIN_RELAY(15, 15),
#endif
	PIN_DIGITAL_OUTPUT(0, 0),
//	PIN_PWM_OUTPUT(0, 0),
	PIN_DIGITAL_OUTPUT(1, 1),
//	PIN_PWM_OUTPUT(1, 1),
	PIN_DIGITAL_OUTPUT(2, 2),
//	PIN_PWM_OUTPUT(2, 2),
	PIN_DIGITAL_OUTPUT(3, 3),
//	PIN_PWM_OUTPUT(3, 3),
	PIN_DIGITAL_OUTPUT(4, 4),
//	PIN_PWM_OUTPUT(4, 4),
	PIN_DIGITAL_OUTPUT(5, 5),
//	PIN_PWM_OUTPUT(5, 5),
	PIN_DIGITAL_OUTPUT(6, 6),
//	PIN_PWM_OUTPUT(6, 6),
	PIN_DIGITAL_OUTPUT(7, 7),
//	PIN_PWM_OUTPUT(7, 7),
#if defined(CONTROLLINO_MAXI) || defined(CONTROLLINO_MEGA)
	PIN_DIGITAL_OUTPUT(8, 8),
//	PIN_PWM_OUTPUT(8, 8),
	PIN_DIGITAL_OUTPUT(9, 9),
//	PIN_PWM_OUTPUT(9, 9),
	PIN_DIGITAL_OUTPUT(10, 10),
//	PIN_PWM_OUTPUT(10, 10),
	PIN_DIGITAL_OUTPUT(11, 11),
//	PIN_PWM_OUTPUT(11, 11),
#endif
#if defined(CONTROLLINO_MEGA)
	PIN_DIGITAL_OUTPUT(12, 12),
	PIN_DIGITAL_OUTPUT(13, 13),
	PIN_DIGITAL_OUTPUT(14, 14),
//	PIN_PWM_OUTPUT(14, 14),
	PIN_DIGITAL_OUTPUT(15, 15),
//	PIN_PWM_OUTPUT(15, 15),
	PIN_DIGITAL_OUTPUT(16, 16),
//	PIN_PWM_OUTPUT(16, 16),
	PIN_DIGITAL_OUTPUT(17, 17),
	PIN_DIGITAL_OUTPUT(18, 18),
	PIN_DIGITAL_OUTPUT(19, 19),
	PIN_DIGITAL_INPUT(0, 0),
//	PIN_ANALOG_INPUT(0, 0),
	PIN_DIGITAL_INPUT(1, 1),
//	PIN_ANALOG_INPUT(1, 1),
	PIN_DIGITAL_INPUT(2, 2),
//	PIN_ANALOG_INPUT(2, 2),
	PIN_DIGITAL_INPUT(3, 3),
//	PIN_ANALOG_INPUT(3, 3),
	PIN_DIGITAL_INPUT(4, 4),
//	PIN_ANALOG_INPUT(4, 4),
	PIN_DIGITAL_INPUT(5, 5),
//	PIN_ANALOG_INPUT(5, 5),
	PIN_DIGITAL_INPUT(6, 6),
//	PIN_ANALOG_INPUT(6, 6),
#endif
#if defined(CONTROLLINO_MAXI) || defined(CONTROLLINO_MEGA) || defined(CONTROLLINO_MAXI_AUTOMATION)
	PIN_DIGITAL_INPUT(7, 7),
//	PIN_ANALOG_INPUT(7, 7),
	PIN_DIGITAL_INPUT(8, 8),
//	PIN_ANALOG_INPUT(8, 8),
	PIN_DIGITAL_INPUT(9, 9),
//	PIN_ANALOG_INPUT(9, 9),
#endif
#if defined(CONTROLLINO_MEGA)
	PIN_DIGITAL_INPUT(10, 10),
//	PIN_ANALOG_INPUT(10, 10),
	PIN_DIGITAL_INPUT(11, 11),
//	PIN_ANALOG_INPUT(11, 11),
	PIN_DIGITAL_INPUT(12, 12),
//	PIN_ANALOG_INPUT(12, 12),
	PIN_DIGITAL_INPUT(13, 13),
//	PIN_ANALOG_INPUT(13, 13),
	PIN_DIGITAL_INPUT(14, 14),
//	PIN_ANALOG_INPUT(14, 14),
	PIN_DIGITAL_INPUT(15, 15),
//	PIN_ANALOG_INPUT(15, 15),
#endif
};

#define PINS_COUNT ARRAY_SIZE(pins)

#define for_each_pin(pin, i)								\
	for (i = 0, pin = &pins[i]; i < PINS_COUNT; pin = &pins[++i])

void pins_refresh()
{
	struct pin *pin;
	word new_state;
	unsigned int i;

	for_each_pin(pin, i) {
		switch (pin->type) {
		case PIN_TYPE_RELAY:
		case PIN_TYPE_DIGITAL_OUTPUT:
			new_state = mb.Coil(pin->mb_index);
			break;
		case PIN_TYPE_PWM_OUTPUT:
			new_state = mb.Hreg(pin->mb_index);
			break;
		case PIN_TYPE_DIGITAL_INPUT:
			new_state = digitalRead(pin->pin);
			break;
		case PIN_TYPE_ANALOG_INPUT:
			new_state = analogRead(pin->pin);
			break;
		}
		if (pin->state == new_state)
			continue;
		switch (pin->type) {
		case PIN_TYPE_RELAY:
			Serial.println((String) "Setting relay R" +
				       pin->index + " to " + new_state);
			digitalWrite(pin->pin, new_state);
			break;
		case PIN_TYPE_DIGITAL_OUTPUT:
			Serial.println((String) "Setting digital output D" +
				       pin->index + " to " + new_state);
			digitalWrite(pin->pin, new_state);
			break;
		case PIN_TYPE_PWM_OUTPUT:
			Serial.println((String) "Setting PWM output D" +
				       pin->index + " to " + new_state);
			analogWrite(pin->pin, new_state);
			break;
		case PIN_TYPE_DIGITAL_INPUT:
			mb.Ists(pin->mb_index, new_state);
			Serial.println((String) "Updating digital input A" +
				       pin->index + " to " + new_state);
			break;
		case PIN_TYPE_ANALOG_INPUT:
			mb.Ireg(pin->mb_index, new_state);
			Serial.println((String) "Updating analog input A" +
				       pin->index + " to " + new_state);
			break;
		}
		pin->state = new_state;
	}
}

void pins_init(void)
{
	struct pin *pin;
	unsigned int i;

	for_each_pin(pin, i) {
		switch (pin->type) {
		case PIN_TYPE_RELAY:
		case PIN_TYPE_DIGITAL_OUTPUT:
			pinMode(pin->pin, OUTPUT);
			mb.addCoil(pin->mb_index);
			break;
		case PIN_TYPE_PWM_OUTPUT:
			pinMode(pin->pin, OUTPUT);
			mb.addHreg(pin->mb_index);
			break;
		case PIN_TYPE_DIGITAL_INPUT:
			pinMode(pin->pin, INPUT);
			mb.addIsts(pin->mb_index);
			break;
		case PIN_TYPE_ANALOG_INPUT:
			pinMode(pin->pin, INPUT);
			mb.addIreg(pin->mb_index);
			break;
		}
	}
}

void setup() {
	Serial.begin(9600);
	Serial.println("Controllino Modbus TCP slave");
	mb.config(mac, ip);
	pins_init();
}

void loop() {
	mb.task();
	pins_refresh();
}
