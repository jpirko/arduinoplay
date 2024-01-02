/*
 * M5Stack Station Modbus-RTU 1-wire temperature sensors client
 * Copyright (c) 2024 Jiri Pirko <jiri@resnulli.us>
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

#include <M5Station.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <EEPROM.h>
#include <FastLED.h>
#include <ModbusSerial.h>

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

enum pin_type {
	PIN_TYPE_G,
};
		
struct ow_temp_device;

struct pin {
	uint8_t type;
	uint8_t pin; /* pin number */
	uint8_t index; /* filled-up during init */
	bool printed;
	struct {
		OneWire *oneWire;
		DallasTemperature *sensors;
		struct ow_temp_device *temp_device;
	};
};

#define PIN_G(_pin) {								\
	.type = PIN_TYPE_G,							\
	.pin = _pin,								\
}

struct pin pins[] = {
	PIN_G(32), /* G32 */
	PIN_G(25), /* G25 */
	PIN_G(14), /* G14 */
	PIN_G(13), /* G13 */
	PIN_G(33), /* G33 */
	PIN_G(26), /* G26 */
	PIN_G(16), /* G16 */
	PIN_G(17), /* G17 */
};

#define PINS_COUNT ARRAY_SIZE(pins)

#define for_each_pin(pin, i)								\
	for (i = 0, pin = &pins[i]; i < PINS_COUNT; pin = &pins[++i])

static void ow_temp_pin_init(struct pin *pin);
static void mb_pin_init(struct pin *pin);
static void mb_pin_update(struct pin *pin);

void pins_init(void)
{
	struct pin *pin;
	uint8_t i;

	for_each_pin(pin, i) {
		pin->index = i;
		ow_temp_pin_init(pin);
		mb_pin_init(pin);
	}
}

static void ow_temp_pin_init(struct pin *pin)
{
	pin->oneWire = new OneWire(pin->pin);
	pin->sensors = new DallasTemperature(pin->oneWire);
	pin->sensors->begin();
	pin->sensors->setWaitForConversion(false);
}
	
#define OW_TEMP_INVALID DEVICE_DISCONNECTED_C

struct ow_temp_device {
	DeviceAddress address;
	struct pin *pin;
	float value;
};

static float ow_temp_pin_value(struct pin *pin)
{
	return pin->temp_device ? pin->temp_device->value : OW_TEMP_INVALID;
}

#define OW_TEMP_DEVICE_MAX 16

struct ow_temp {
	bool read_in_progress;
	unsigned long last_read_millis;
	bool waiting_on_conversion;
	unsigned long start_conversion_millis;
	int current_device;
	uint8_t device_count;
	struct ow_temp_device device[OW_TEMP_DEVICE_MAX];
};

static struct ow_temp_device *
ow_temp_device_get(struct ow_temp *temp, uint8_t index)
{
	return &temp->device[index];
}

void mb_pin_update(struct pin *pin);

static void ow_temp_scan(struct ow_temp *temp, unsigned int interval)
{
	struct ow_temp_device *temp_device;
	unsigned long now = millis();
	DeviceAddress address;
	struct pin *pin;
	uint8_t i;

	if (temp->last_read_millis &&
	    (now - temp->last_read_millis < interval))
		return;

	memset(temp, 0, sizeof(*temp));
	temp->last_read_millis = now;
	
	for_each_pin(pin, i) {
		temp_device = NULL;
		pin->oneWire->reset_search();
		while (pin->oneWire->search(address)) {
			if (OneWire::crc8(address, 7) != address[7] ||
			    temp->device_count == OW_TEMP_DEVICE_MAX)
				continue;
			temp_device = ow_temp_device_get(temp,
							 temp->device_count++);
			memcpy(temp_device->address, address, sizeof(address));
			temp_device->pin = pin;
		}
		pin->temp_device = temp_device; /* save the last one if any */
		if (!pin->temp_device) {
			pin->printed = false;
			mb_pin_update(pin);
		}
	}
	if (temp->device_count)
		temp->read_in_progress = true;
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
	struct ow_temp_device *temp_device;
	struct pin *pin;

next_device:
	temp_device = ow_temp_device_get(temp, temp->current_device);
	pin = temp_device->pin;
	if (!pin->sensors->requestTemperaturesByAddress(temp_device->address)) {
		if (!ow_temp_device_next(temp))
			return;
		goto next_device;
	}
	temp->waiting_on_conversion = true;
	temp->start_conversion_millis = millis();;
}

#define OW_TEMP_CONVERSION_TIMEOUT 950

static void ow_temp_conversion_wait_check(struct ow_temp *temp)
{
	struct ow_temp_device *temp_device;
	unsigned long now = millis();
	float temp_value;
	struct pin *pin;

	temp_device = ow_temp_device_get(temp, temp->current_device);
	pin = temp_device->pin;
	if (!pin->sensors->isConversionComplete()) {
		if (now - temp->start_conversion_millis > OW_TEMP_CONVERSION_TIMEOUT)
			goto next;
		return;
	}

	temp_device->value = pin->sensors->getTempC(temp_device->address);
	temp_device->pin->printed = false;
	mb_pin_update(pin);

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

#define HALF_PINS_COUNT (PINS_COUNT / 2)

#define LCD_TEXT_MARGIN 3
#define LCD_WIDTH M5.Lcd.width()
#define LCD_PIN_WIDTH 30
#define LCD_PIN_HEIGHT 25
#define LCD_TEMP_WIDTH 90
#define LCD_TEMP_HEIGHT 25
#define LCD_PIN_X1 0
#define LCD_PIN_X2 (LCD_WIDTH - LCD_PIN_WIDTH)
#define LCD_PIN_X(i) (i < HALF_PINS_COUNT) ? LCD_PIN_X1 : LCD_PIN_X2
#define LCD_PIN_Y(i) (i % HALF_PINS_COUNT) * LCD_PIN_HEIGHT
#define LCD_PIN_X_TEXT(i) LCD_PIN_X(i) + LCD_TEXT_MARGIN
#define LCD_PIN_Y_TEXT(i) LCD_PIN_Y(i) + LCD_TEXT_MARGIN
#define LCD_TEMP_X1 LCD_PIN_WIDTH
#define LCD_TEMP_X2 (LCD_WIDTH - LCD_PIN_WIDTH - LCD_TEMP_WIDTH)
#define LCD_TEMP_X(i) (i < HALF_PINS_COUNT) ? LCD_TEMP_X1 : LCD_TEMP_X2
#define LCD_TEMP_Y(i) (i % HALF_PINS_COUNT) * LCD_TEMP_HEIGHT
#define LCD_TEMP_X_TEXT(i) LCD_TEMP_X(i) + LCD_TEXT_MARGIN
#define LCD_TEMP_Y_TEXT(i) LCD_TEMP_Y(i) + LCD_TEXT_MARGIN

void pins_print(void)
{
	struct pin *pin;
	int i;
	
	for_each_pin(pin, i) {
		pinMode(pin->pin, INPUT_PULLUP);
		M5.Lcd.fillRect(LCD_PIN_X(i), LCD_PIN_Y(i),
				LCD_PIN_WIDTH, LCD_PIN_HEIGHT, BLACK);
		M5.Lcd.setTextColor(WHITE);
		M5.Lcd.setTextSize(2);
		M5.Lcd.setCursor(LCD_PIN_X_TEXT(i), LCD_PIN_Y_TEXT(i));
		M5.Lcd.printf("%u", pin->pin);
	}
}

void ow_temp_print(struct ow_temp *temp)
{
	struct pin *pin;
	float value;
	int i;
	
	for_each_pin(pin, i) {
		if (pin->printed)
			continue;
		pin->printed = true;

		value = ow_temp_pin_value(pin);
	
		M5.Lcd.fillRect(LCD_TEMP_X(i), LCD_TEMP_Y(i),
				LCD_TEMP_WIDTH, LCD_TEMP_HEIGHT, BLACK);
		if (value == OW_TEMP_INVALID)
			continue;
		M5.Lcd.setTextColor(GREEN);
		M5.Lcd.setTextSize(2);
		M5.Lcd.setCursor(LCD_TEMP_X_TEXT(i), LCD_TEMP_Y_TEXT(i));
		M5.Lcd.printf("%2.2f C", value);
	}
}

#define LCD_STATUS_WIDTH M5.Lcd.width()
#define LCD_STATUS_HEIGHT 20
#define LCD_STATUS_X 0
#define LCD_STATUS_Y (M5.Lcd.height() - LCD_STATUS_HEIGHT)

void print_status(String status)
{
	M5.Lcd.fillRect(LCD_STATUS_X, LCD_STATUS_Y,
			LCD_STATUS_WIDTH, LCD_STATUS_HEIGHT, BLACK);
	M5.Lcd.setTextColor(YELLOW);
	M5.Lcd.setTextSize(2);
	M5.Lcd.setCursor(LCD_STATUS_X, LCD_STATUS_Y);
	M5.Lcd.print(status);
}

static struct ow_temp ow_temp;

uint32_t eeprom_magic = 0x7a3b1fac;
uint8_t eeprom_default_mb_address = 8;
uint32_t eeprom_default_temp_interval = 1000;

#define EEPROM_MAGIC_OFFSET 0
#define EEPROM_MAGIC_SIZE sizeof(eeprom_magic)

#define EEPROM_MB_ADDRESS_OFFSET EEPROM_MAGIC_OFFSET + EEPROM_MAGIC_SIZE
#define EEPROM_MB_ADDRESS_SIZE sizeof(eeprom_default_mb_address)

#define EEPROM_TEMP_INTERVAL_OFFSET EEPROM_MB_ADDRESS_OFFSET + EEPROM_MB_ADDRESS_SIZE
#define EEPROM_TEMP_INTERVAL_SIZE sizeof(eeprom_default_temp_interval)

#define EEPROM_SIZE EEPROM_TEMP_INTERVAL_OFFSET + EEPROM_TEMP_INTERVAL_SIZE

void eeprom_load_defaults(void)
{
	EEPROM.put(EEPROM_MAGIC_OFFSET, eeprom_magic);
	EEPROM.put(EEPROM_MB_ADDRESS_OFFSET, eeprom_default_mb_address);
	EEPROM.put(EEPROM_TEMP_INTERVAL_OFFSET, eeprom_default_temp_interval);
	EEPROM.commit();
}

void eeprom_check(void)
{
	uint32_t magic;

	EEPROM.get(EEPROM_MAGIC_OFFSET, magic);
	if (magic != eeprom_magic) {
		eeprom_load_defaults();
		print_status("EEPROM DEFAULTS!");
		delay(1000);
	}
}

#define MB_CONFIG_ENABLE_HREG 0
#define MB_CONFIG_ADDRESS_HREG 1

#define MB_PIN_TEMP_IREG_START 0
#define MB_PIN_TEMP_IREG_COUNT 8

#define __MB_PIN_TEMP_IREG(pin, reg)		\
	MB_PIN_TEMP_IREG_START + MB_PIN_TEMP_IREG_COUNT * (pin)->index + (reg)

#define MB_PIN_TEMP_VALID_IREG(pin) __MB_PIN_TEMP_IREG(pin, 0)
#define MB_PIN_TEMP_VALUE_HI_IREG(pin) __MB_PIN_TEMP_IREG(pin, 1)
#define MB_PIN_TEMP_VALUE_LO_IREG(pin) __MB_PIN_TEMP_IREG(pin, 2)
#define MB_PIN_TEMP_ID_0_IREG(pin) __MB_PIN_TEMP_IREG(pin, 3)
#define MB_PIN_TEMP_ID_1_IREG(pin) __MB_PIN_TEMP_IREG(pin, 4)
#define MB_PIN_TEMP_ID_2_IREG(pin) __MB_PIN_TEMP_IREG(pin, 5)
#define MB_PIN_TEMP_ID_3_IREG(pin) __MB_PIN_TEMP_IREG(pin, 6)
#define MB_PIN_TEMP_UNUSED_IREG(pin) __MB_PIN_TEMP_IREG(pin, 7)

#define MB_SERIAL Serial
#define MB_SERIAL_BAUDRATE 9600
#define MB_SERIAL_TXPIN 2

uint8_t mb_address;

ModbusSerial mb(MB_SERIAL, mb_address, MB_SERIAL_TXPIN);

static void mb_init()
{
  	MB_SERIAL.begin(MB_SERIAL_BAUDRATE, SERIAL_8N1);
	mb.config(MB_SERIAL_BAUDRATE);
	mb.setSlaveId(mb_address);
	
	mb.addHreg(MB_CONFIG_ENABLE_HREG, 0);
	mb.addHreg(MB_CONFIG_ADDRESS_HREG, mb_address);
}

static void mb_pin_init(struct pin *pin)
{
	mb.addIreg(MB_PIN_TEMP_VALID_IREG(pin), 0);
	mb.addIreg(MB_PIN_TEMP_VALUE_HI_IREG(pin), 0);
	mb.addIreg(MB_PIN_TEMP_VALUE_LO_IREG(pin), 0);
	mb.addIreg(MB_PIN_TEMP_ID_0_IREG(pin), 0);
	mb.addIreg(MB_PIN_TEMP_ID_1_IREG(pin), 0);
	mb.addIreg(MB_PIN_TEMP_ID_2_IREG(pin), 0);
	mb.addIreg(MB_PIN_TEMP_ID_3_IREG(pin), 0);
	mb.addIreg(MB_PIN_TEMP_UNUSED_IREG(pin), 0);
}

static void mb_pin_update(struct pin *pin)
{
	uint16_t *tmp;
	float value;
	
	value = ow_temp_pin_value(pin);
	if (value == OW_TEMP_INVALID) {
		mb.Ireg(MB_PIN_TEMP_VALID_IREG(pin), 0);
		mb.Ireg(MB_PIN_TEMP_VALUE_HI_IREG(pin), 0);
		mb.Ireg(MB_PIN_TEMP_VALUE_LO_IREG(pin), 0);
		mb.Ireg(MB_PIN_TEMP_ID_0_IREG(pin), 0);
		mb.Ireg(MB_PIN_TEMP_ID_1_IREG(pin), 0);
		mb.Ireg(MB_PIN_TEMP_ID_2_IREG(pin), 0);
		mb.Ireg(MB_PIN_TEMP_ID_3_IREG(pin), 0);
		return;
	}

	tmp = (uint16_t *) &value;
	mb.Ireg(MB_PIN_TEMP_VALUE_HI_IREG(pin), tmp[0]);
	mb.Ireg(MB_PIN_TEMP_VALUE_LO_IREG(pin), tmp[1]);

	tmp = (uint16_t *) pin->temp_device->address;
	mb.Ireg(MB_PIN_TEMP_ID_0_IREG(pin), tmp[0]);
	mb.Ireg(MB_PIN_TEMP_ID_1_IREG(pin), tmp[1]);
	mb.Ireg(MB_PIN_TEMP_ID_2_IREG(pin), tmp[2]);
	mb.Ireg(MB_PIN_TEMP_ID_3_IREG(pin), tmp[3]);
	
	mb.Ireg(MB_PIN_TEMP_VALID_IREG(pin), 1);
}

uint32_t temp_interval;

static void print_default_status(void)
{
	print_status("A:" + String(mb_address) + " I:" + String(temp_interval) + "ms");
}

static void mb_check_config_change(void)
{
	word new_address = mb.Hreg(MB_CONFIG_ADDRESS_HREG);
	bool config_enabled = mb.Hreg(MB_CONFIG_ENABLE_HREG);

	if (!config_enabled) {
		mb.Hreg(MB_CONFIG_ADDRESS_HREG, mb_address);
		return;
	}

	if (new_address == mb_address)
		return;
	mb_address = new_address;
	mb.setSlaveId(mb_address);
	EEPROM.put(EEPROM_MB_ADDRESS_OFFSET, mb_address);
	EEPROM.commit();
	
	print_default_status();
}

#define LED_PIN 4
#define LED_COUNT 7
CRGB leds[LED_COUNT];

static void leds_init(void)
{
	int i;

    	FastLED.addLeds<SK6812, LED_PIN, GRB>(leds, LED_COUNT);
	for (i = 0; i < LED_COUNT; i++)
		leds[i] = CRGB::Black;
	FastLED.setBrightness(60);
	FastLED.show();
}

void setup(void)
{
	M5.begin();
	M5.Lcd.fillScreen(BLACK);

	EEPROM.begin(EEPROM_SIZE);
	eeprom_check();

	EEPROM.get(EEPROM_MB_ADDRESS_OFFSET, mb_address);
	EEPROM.get(EEPROM_TEMP_INTERVAL_OFFSET, temp_interval);

	leds_init();
	mb_init();
	pins_init();
	pins_print();

	print_default_status();
}

#define FACTORY_RESET_BUTTON_TIME 5000

void loop(void)
{
	unsigned long now = millis();

	M5.update();
	if (M5.BtnC.isPressed() &&
	    M5.BtnC.pressedFor(FACTORY_RESET_BUTTON_TIME)) {
		print_status("FACTORY RESET");
		eeprom_load_defaults();
		ESP.restart();
	} else if (M5.BtnC.wasReleased()) {
		print_status("RESET");
		ESP.restart();
	}

	ow_temp_process(&ow_temp, temp_interval);
	ow_temp_print(&ow_temp);
	mb.task();
	mb_check_config_change();
}
