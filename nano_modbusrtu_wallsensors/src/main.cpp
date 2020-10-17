/*
 * Arduino Nano: ModbusRTU SCD30, BME280 and SHT31 sensor
 * CO2, air pressure and multiple values of temperature and humidity.
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
#include <Modbus.h>
#include <Wire.h>
#include <SparkFun_SCD30_Arduino_Library.h>
#include <SparkFunBME280.h>
#include <SHT31.h>
#include <EEPROM.h>

uint32_t eeprom_magic = 0x3b1e2e8a;
byte eeprom_default_address = 0x31;

#define EEPROM_MAGIC_OFFSET 0
#define EEPROM_MAGIC_SIZE sizeof(eeprom_magic)

#define EEPROM_ADDRESS_OFFSET EEPROM_MAGIC_OFFSET + EEPROM_MAGIC_SIZE
#define EEPROM_ADDRESS_SIZE sizeof(eeprom_default_address)

void eeprom_check(void)
{
	uint32_t magic;

	EEPROM.get(EEPROM_MAGIC_OFFSET, magic);
	if (magic == eeprom_magic)
		return;

	EEPROM.put(EEPROM_MAGIC_OFFSET, eeprom_magic);
	EEPROM.put(EEPROM_ADDRESS_OFFSET, eeprom_default_address);
}

SCD30 scd30;
BME280 bme280;
SHT31 sht31;

static bool config_enabled = false;

const int LED_COIL = 1;

#define VALUE_CONFIG_ENABLE 0x00ff

const int CONFIG_ENABLE_HREG = 0;
const int CONFIG_ADDRESS_HREG = 1;

#define VALUE_VALID_MAGIC 0
#define VALUE_INVALID_MAGIC 0xffff

const int SCD30_TEMP_VALID_IREG = 0;
const int SCD30_TEMP_IREG = 1;
const int SCD30_TEMP_HI_IREG = 2;
const int SCD30_TEMP_LO_IREG = 3;
const int SCD30_HUMIDITY_VALID_IREG = 4;
const int SCD30_HUMIDITY_IREG = 5;
const int SCD30_HUMIDITY_HI_IREG = 6;
const int SCD30_HUMIDITY_LO_IREG = 7;
const int SCD30_CO2_VALID_IREG = 8;
const int SCD30_CO2_IREG = 9;

const int BME280_TEMP_VALID_IREG = 10;
const int BME280_TEMP_IREG = 11;
const int BME280_TEMP_HI_IREG = 12;
const int BME280_TEMP_LO_IREG = 13;
const int BME280_HUMIDITY_VALID_IREG = 14;
const int BME280_HUMIDITY_IREG = 15;
const int BME280_HUMIDITY_HI_IREG = 16;
const int BME280_HUMIDITY_LO_IREG = 17;
const int BME280_PRESSURE_VALID_IREG = 18;
const int BME280_PRESSURE_IREG = 19;
const int BME280_PRESSURE_HI_IREG = 20;
const int BME280_PRESSURE_LO_IREG = 21;

const int SHT31_TEMP_VALID_IREG = 22;
const int SHT31_TEMP_IREG = 23;
const int SHT31_TEMP_HI_IREG = 24;
const int SHT31_TEMP_LO_IREG = 25;
const int SHT31_HUMIDITY_VALID_IREG = 26;
const int SHT31_HUMIDITY_IREG = 27;
const int SHT31_HUMIDITY_HI_IREG = 28;
const int SHT31_HUMIDITY_LO_IREG = 29;

static byte mb_address;

//#define SERIAL_DEBUG

#ifdef SERIAL_DEBUG
class ModbusSerial {
    public:
        ModbusSerial();

        void addHreg(word offset, word value = 0);
        bool Hreg(word offset, word value);
        word Hreg(word offset);

        void addCoil(word offset, bool value = false);
        void addIreg(word offset, word value = 0);

        bool Ireg(word offset, word value);

        bool Coil(word offset);
};
ModbusSerial::ModbusSerial()
{
}

void ModbusSerial::addHreg(word offset, word value)
{
	if (offset == CONFIG_ADDRESS_HREG)
		mb_address = value;
	Serial.print("add Hreg ");
	Serial.print(offset);
	Serial.print(", value ");
	Serial.println(value);
}

bool ModbusSerial::Hreg(word offset, word value)
{
	if (offset == CONFIG_ADDRESS_HREG)
		mb_address = value;
	return true;
}

word ModbusSerial::Hreg(word offset)
{
	if (offset == CONFIG_ADDRESS_HREG)
		return mb_address;
	return 0;
}

void ModbusSerial::addCoil(word offset, bool value)
{
	Serial.print("add Coil ");
	Serial.print(offset);
	Serial.print(", value ");
	Serial.println(value);
}

void ModbusSerial::addIreg(word offset, word value)
{
	Serial.print("add Ireg ");
	Serial.print(offset);
	Serial.print(", value ");
	Serial.println(value);
}

bool ModbusSerial::Ireg(word offset, word value)
{
	Serial.print("set Ireg ");
	Serial.print(offset);
	Serial.print(", value ");
	Serial.println(value);
	return true;
}

bool ModbusSerial::Coil(word offset)
{
	return true;
}
#else
#include <ModbusSerial.h>
#endif

ModbusSerial mb;

const int sensorPin = A0;

#define TXPIN 2
#define LED_BUILTIN 13

void setup()
{
	pinMode(LED_BUILTIN, OUTPUT);

	eeprom_check();

	EEPROM.get(EEPROM_ADDRESS_OFFSET, mb_address);

#ifdef SERIAL_DEBUG
	Serial.begin(9600);
#else
	mb.config(&Serial, 9600, SERIAL_8N1, TXPIN);
	mb.setSlaveId(mb_address);
#endif

	mb.addHreg(CONFIG_ENABLE_HREG);
	mb.addHreg(CONFIG_ADDRESS_HREG, mb_address);

	mb.addIreg(SCD30_TEMP_VALID_IREG, VALUE_INVALID_MAGIC);
	mb.addIreg(SCD30_TEMP_IREG);
	mb.addIreg(SCD30_TEMP_HI_IREG);
	mb.addIreg(SCD30_TEMP_LO_IREG);
	mb.addIreg(SCD30_HUMIDITY_VALID_IREG, VALUE_INVALID_MAGIC);
	mb.addIreg(SCD30_HUMIDITY_IREG);
	mb.addIreg(SCD30_HUMIDITY_HI_IREG);
	mb.addIreg(SCD30_HUMIDITY_LO_IREG);
	mb.addIreg(SCD30_CO2_VALID_IREG, VALUE_INVALID_MAGIC);
	mb.addIreg(SCD30_CO2_IREG);

	mb.addIreg(BME280_TEMP_VALID_IREG, VALUE_INVALID_MAGIC);
	mb.addIreg(BME280_TEMP_IREG);
	mb.addIreg(BME280_TEMP_HI_IREG);
	mb.addIreg(BME280_TEMP_LO_IREG);
	mb.addIreg(BME280_HUMIDITY_VALID_IREG, VALUE_INVALID_MAGIC);
	mb.addIreg(BME280_HUMIDITY_IREG);
	mb.addIreg(BME280_HUMIDITY_HI_IREG);
	mb.addIreg(BME280_HUMIDITY_LO_IREG);
	mb.addIreg(BME280_PRESSURE_VALID_IREG, VALUE_INVALID_MAGIC);
	mb.addIreg(BME280_PRESSURE_IREG);
	mb.addIreg(BME280_PRESSURE_HI_IREG);
	mb.addIreg(BME280_PRESSURE_LO_IREG);

	mb.addIreg(SHT31_TEMP_VALID_IREG);
	mb.Ireg(SHT31_TEMP_VALID_IREG, VALUE_INVALID_MAGIC);
	mb.addIreg(SHT31_TEMP_IREG);
	mb.addIreg(SHT31_TEMP_HI_IREG);
	mb.addIreg(SHT31_TEMP_LO_IREG);
	mb.addIreg(SHT31_HUMIDITY_VALID_IREG);
	mb.Ireg(SHT31_HUMIDITY_VALID_IREG, VALUE_INVALID_MAGIC);
	mb.addIreg(SHT31_HUMIDITY_IREG);
	mb.addIreg(SHT31_HUMIDITY_HI_IREG);
	mb.addIreg(SHT31_HUMIDITY_LO_IREG);

	mb.addCoil(LED_COIL);

	Wire.begin();
	Wire.setClock(100000);

	scd30.begin();
	scd30.setMeasurementInterval(5);

	bme280.setI2CAddress(0x76);
	bme280.beginI2C();

	sht31.begin(0x44);

}

static void Iregs_int_set(word int_offset, word valid_offset, uint16_t value)
{
	mb.Ireg(valid_offset, VALUE_VALID_MAGIC);
	mb.Ireg(int_offset, value);
}

static void Iregs_float_set(word hi_offset, word lo_offset, word int_offset,
			    word valid_offset, float value)
{
	uint32_t *tmp;

	mb.Ireg(valid_offset, VALUE_VALID_MAGIC);
	mb.Ireg(int_offset, (int) (value * 10));
	tmp = (uint32_t *) &value;
	mb.Ireg(hi_offset, (*tmp) >> 16);
	mb.Ireg(lo_offset, (*tmp) && 0xFFFF);
}

static int voidmeasures = 0;
#define VOIDMEASURES_NUM 2

static void scd30_process(void)
{
	float temperature;
	float humidity;
	uint16_t co2;

	if (!scd30.dataAvailable()) {
		mb.Ireg(SCD30_TEMP_VALID_IREG, VALUE_INVALID_MAGIC);
		mb.Ireg(SCD30_HUMIDITY_VALID_IREG, VALUE_INVALID_MAGIC);
		mb.Ireg(SCD30_CO2_VALID_IREG, VALUE_INVALID_MAGIC);
		voidmeasures = 0;
		return;
	}

	co2 = scd30.getCO2();
	temperature = scd30.getTemperature();
	humidity = scd30.getHumidity();

	if (voidmeasures < VOIDMEASURES_NUM) {
		voidmeasures++;
		return;
	}

	Iregs_float_set(SCD30_TEMP_HI_IREG, SCD30_TEMP_LO_IREG,
			SCD30_TEMP_IREG, SCD30_TEMP_VALID_IREG,
			temperature);
	Iregs_float_set(SCD30_HUMIDITY_HI_IREG, SCD30_HUMIDITY_LO_IREG,
			SCD30_HUMIDITY_IREG, SCD30_HUMIDITY_VALID_IREG,
			humidity);
	Iregs_int_set(SCD30_CO2_IREG, SCD30_CO2_VALID_IREG, co2);
}


static void bme280_process(void)
{
	float temperature;
//	float humidity;
	float pressure;

	if (!bme280.isMeasuring()) {
		mb.Ireg(BME280_TEMP_VALID_IREG, VALUE_INVALID_MAGIC);
		mb.Ireg(BME280_HUMIDITY_VALID_IREG, VALUE_INVALID_MAGIC);
		mb.Ireg(BME280_PRESSURE_VALID_IREG, VALUE_INVALID_MAGIC);
		return;
	}
	pressure = bme280.readFloatPressure() / 100.0F;
	temperature = bme280.readTempC();
//	humidity = bme280.readFloatHumidity();

	scd30.setAmbientPressure((uint16_t) pressure);

	Iregs_float_set(BME280_TEMP_HI_IREG, BME280_TEMP_LO_IREG,
			BME280_TEMP_IREG, BME280_TEMP_VALID_IREG,
			temperature);
//	Iregs_float_set(BME280_HUMIDITY_HI_IREG, BME280_HUMIDITY_LO_IREG,
//			BME280_HUMIDITY_IREG, BME280_HUMIDITY_VALID_IREG,
//			humidity);
	Iregs_float_set(BME280_PRESSURE_HI_IREG, BME280_PRESSURE_LO_IREG,
			BME280_PRESSURE_IREG, BME280_PRESSURE_VALID_IREG,
			pressure);
}

#define SHT31_INVALID_TEMP 130

static void sht31_process(void)
{
	float temperature;
	float humidity;

	if (!sht31.dataReady())
		return;

	sht31.readData();
	sht31.requestData();
	temperature = sht31.getTemperature();
	humidity = sht31.getHumidity();

	if (temperature == SHT31_INVALID_TEMP) {
		mb.Ireg(SHT31_TEMP_VALID_IREG, VALUE_INVALID_MAGIC);
		mb.Ireg(SHT31_HUMIDITY_VALID_IREG, VALUE_INVALID_MAGIC);
		return;
	}

	Iregs_float_set(SHT31_TEMP_HI_IREG, SHT31_TEMP_LO_IREG,
			SHT31_TEMP_IREG, SHT31_TEMP_VALID_IREG,
			temperature);
	Iregs_float_set(SHT31_HUMIDITY_HI_IREG, SHT31_HUMIDITY_LO_IREG,
			SHT31_HUMIDITY_IREG, SHT31_HUMIDITY_VALID_IREG,
			humidity);
}

#define MEASURE_INTERVAL 5000 /* ms */

static void check_mb_config_change(void)
{
	word new_address = mb.Hreg(CONFIG_ADDRESS_HREG);

	config_enabled = mb.Hreg(CONFIG_ENABLE_HREG) == VALUE_CONFIG_ENABLE;

	if (!config_enabled) {
		mb.Hreg(CONFIG_ADDRESS_HREG, mb_address);
		return;
	}

	if (new_address == mb_address)
		return;
	mb_address = new_address;
	EEPROM.put(EEPROM_ADDRESS_OFFSET, mb_address);
}

unsigned long last_measurement = 0;

void loop()
{
	unsigned long now = millis();

#ifndef SERIAL_DEBUG
	mb.task();
#endif

	check_mb_config_change();

	digitalWrite(LED_BUILTIN, mb.Coil(LED_COIL));

	if (last_measurement + MEASURE_INTERVAL > now)
		return;
	last_measurement = now;

	scd30_process();
	bme280_process();
	sht31_process();
}
