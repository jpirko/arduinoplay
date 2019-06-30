/*
 * Arduino Nano: ModbusRTU SCD30 and BME280 based CO2 sensor
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
#include <Modbus.h>
#include <Wire.h>
#include <SparkFun_SCD30_Arduino_Library.h>
#include <SparkFunBME280.h>
#include <ModbusSerial.h>

SCD30 air_sensor;
BME280 pressure_sensor;
ModbusSerial mb;

const int LED_COIL = 100;
const int CO2_IREG = 100;
const int TEMP_HI_IREG = 101;
const int TEMP_LO_IREG = 102;
const int HUMIDITY_HI_IREG = 103;
const int HUMIDITY_LO_IREG = 104;
const int PRESSURE_HI_IREG = 105;
const int PRESSURE_LO_IREG = 106;
const int TEMP2_HI_IREG = 107;
const int TEMP2_LO_IREG = 108;
const int HUMIDITY2_HI_IREG = 109;
const int HUMIDITY2_LO_IREG = 110;
const int CO2_AVAILABLE_ISTS = 100;
const int TEMP_AVAILABLE_ISTS = 101;
const int HUMIDITY_AVAILABLE_ISTS = 102;
const int PRESSURE_AVAILABLE_ISTS = 103;
const int TEMP2_AVAILABLE_ISTS = 104;
const int HUMIDITY2_AVAILABLE_ISTS = 105;

const int sensorPin = A0;

#define TXPIN 2
#define LED_BUILTIN 13

void setup()
{
	pinMode(LED_BUILTIN, OUTPUT);

	mb.config(&Serial, 115200, SERIAL_8N1, TXPIN);
	mb.setSlaveId(0x31);

	mb.addIreg(CO2_IREG);
	mb.addIreg(TEMP_HI_IREG);
	mb.addIreg(TEMP_LO_IREG);
	mb.addIreg(HUMIDITY_HI_IREG);
	mb.addIreg(HUMIDITY_LO_IREG);
	mb.addIreg(PRESSURE_HI_IREG);
	mb.addIreg(PRESSURE_LO_IREG);
	mb.addIreg(TEMP2_HI_IREG);
	mb.addIreg(TEMP2_LO_IREG);
	mb.addIreg(HUMIDITY2_HI_IREG);
	mb.addIreg(HUMIDITY2_LO_IREG);
	mb.addIsts(CO2_AVAILABLE_ISTS);
	mb.addIsts(TEMP_AVAILABLE_ISTS);
	mb.addIsts(HUMIDITY_AVAILABLE_ISTS);
	mb.addIsts(PRESSURE_AVAILABLE_ISTS);
	mb.addIsts(TEMP2_AVAILABLE_ISTS);
	mb.addIsts(HUMIDITY2_AVAILABLE_ISTS);
	mb.addCoil(LED_COIL);

	Wire.begin();

	air_sensor.begin();
	air_sensor.setMeasurementInterval(2);

	pressure_sensor.setI2CAddress(0x76);
	pressure_sensor.beginI2C();
}

static void Iregs_float_set(word hi_offset, word lo_offset, float value)
{
	uint32_t *tmp;

	tmp = (uint32_t *) &value;
	mb.Ireg(hi_offset, (*tmp) >> 16);
	mb.Ireg(lo_offset, (*tmp) && 0xFFFF);
}

static int voidmeasures = 0;
#define VOIDMEASURES_NUM 3

static void air_sensor_process(void)
{
	float temperature;
	float humidity;
	uint16_t co2;

	if (!air_sensor.dataAvailable())
		return;

	co2 = air_sensor.getCO2();
	temperature = air_sensor.getTemperature();
	humidity = air_sensor.getHumidity();

	if (voidmeasures < 3) {
		voidmeasures++;
		return;
	}

	mb.Ireg(CO2_IREG, co2);
	mb.Ists(CO2_AVAILABLE_ISTS, true);

	Iregs_float_set(TEMP_HI_IREG, TEMP_LO_IREG, temperature);
	mb.Ists(TEMP_AVAILABLE_ISTS, true);

	Iregs_float_set(HUMIDITY_HI_IREG, HUMIDITY_LO_IREG, humidity);
	mb.Ists(HUMIDITY_AVAILABLE_ISTS, true);
}

unsigned long last_measurement = 0;

#define MEASURE_INTERVAL 2000 /* ms */

static void pressure_sensor_process(void)
{
	unsigned long now = millis();
	float temperature;
	float humidity;
	float pressure;

	if (last_measurement + MEASURE_INTERVAL > now)
		return;
	last_measurement = now;

	pressure = pressure_sensor.readFloatPressure() / 100.0F;
	temperature = pressure_sensor.readTempC();
	humidity = pressure_sensor.readFloatHumidity();

	Iregs_float_set(PRESSURE_HI_IREG, PRESSURE_LO_IREG, pressure);
	mb.Ists(PRESSURE_AVAILABLE_ISTS, true);

	air_sensor.setAmbientPressure((uint16_t) pressure);

	Iregs_float_set(TEMP2_HI_IREG, TEMP2_LO_IREG, temperature);
	mb.Ists(TEMP2_AVAILABLE_ISTS, true);

	Iregs_float_set(HUMIDITY2_HI_IREG, HUMIDITY2_LO_IREG, humidity);
	mb.Ists(HUMIDITY2_AVAILABLE_ISTS, true);
}

void loop()
{
	mb.task();
	digitalWrite(LED_BUILTIN, mb.Coil(LED_COIL));

	air_sensor_process();
	pressure_sensor_process();
}
