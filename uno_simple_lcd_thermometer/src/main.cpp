/*
 * Simple LCD thermometer
 * Copyright (c) 2017 Jiri Pirko <jiri@resnulli.us>
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
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#define LED_BUILTIN 13
#define ONE_WIRE_BUS 2

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

#define I2C_ADDR 0x3f

LiquidCrystal_I2C lcd(I2C_ADDR, 16, 2);

void setup()
{
	pinMode(LED_BUILTIN, OUTPUT);

	Serial.begin(9600);

	sensors.begin();

	Wire.begin();

	lcd.init();
	lcd.backlight();
	lcd.home();
	lcd.clear();
	lcd.print("HELLO!");
	delay(1000);
	lcd.clear();
}

void loop()
{
	digitalWrite(LED_BUILTIN, HIGH);

	sensors.requestTemperatures();
	Serial.print("Temperature is: ");
	Serial.println(sensors.getTempCByIndex(0));

	lcd.home();
	lcd.print(sensors.getTempCByIndex(0));
	lcd.print("\xDF");
	lcd.print("C   ");

	digitalWrite(LED_BUILTIN, LOW);

	delay(1000);
}
