/*
 * Simple motor drive
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
#include <Wire.h>
#include <Grove_I2C_Motor_Driver.h>

#define I2C_ADDR 0x0f
#define POTENTIOMETER_PIN 0
#define POTENTIOMETER_THRESHOLD 15
#define LED_BUILTIN 13

void setup()
{
	pinMode(LED_BUILTIN, OUTPUT);
	Serial.begin(9600);
	Motor.begin(I2C_ADDR);
}

void loop()
{
	int potval = analogRead(POTENTIOMETER_PIN);
	int speed = (potval - 512) / 50 * 10; /* to get -100~100 */

	digitalWrite(LED_BUILTIN, HIGH);
	delay(100);
	Serial.print("setting speed: ");
	Serial.println(speed);
	Motor.speed(MOTOR1, speed);
	digitalWrite(LED_BUILTIN, LOW);
	delay(100);
}
