/*
 * Polling 1-wire thermometers scanner
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
#include <OneWire.h>
#include <DallasTemperature.h>

#define ONE_WIRE_BUS 2

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

static void printAddress(DeviceAddress deviceAddress)
{
	for (uint8_t i = 0; i < 8; i++) {
		if (deviceAddress[i] < 16) Serial.print("0");
		Serial.print(deviceAddress[i], HEX);
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
	if (temp_value != DEVICE_DISCONNECTED_C) {
		Serial.print("Found device: ");
		printAddress(temp->device[temp->current_device].address);
		Serial.print(", temp C: ");
		Serial.println(temp_value);
	}

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

void setup()
{
	Serial.begin(9600);
	ow_temp_init();
}

#define OW_TEMP_READ_INTERVAL 5000

static struct ow_temp ow_temp;

void loop()
{
	ow_temp_process(&ow_temp, OW_TEMP_READ_INTERVAL);
}
