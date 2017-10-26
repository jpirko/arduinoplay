/*
 * Controllino: blink hello world
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
#include <Controllino.h>

void setup() {
	pinMode(CONTROLLINO_D0, OUTPUT);
	pinMode(CONTROLLINO_D1, OUTPUT);
	pinMode(CONTROLLINO_D2, OUTPUT);
	pinMode(CONTROLLINO_D3, OUTPUT);
}

void loop() {
	digitalWrite(CONTROLLINO_D0, HIGH);
	delay(200);
	digitalWrite(CONTROLLINO_D0, LOW);
	delay(200);
	digitalWrite(CONTROLLINO_D1, HIGH);
	delay(200);
	digitalWrite(CONTROLLINO_D1, LOW);
	delay(200);
	digitalWrite(CONTROLLINO_D2, HIGH);
	delay(200);
	digitalWrite(CONTROLLINO_D2, LOW);
	delay(200);
	digitalWrite(CONTROLLINO_D3, HIGH);
	delay(200);
	digitalWrite(CONTROLLINO_D3, LOW);
	delay(200);
}
