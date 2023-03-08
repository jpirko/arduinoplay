/*
 * Atom lite: led blink hello world
 * Copyright (c) 2023 Jiri Pirko <jiri@resnulli.us>
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

#include <M5Atom.h>

void setup() {
	M5.begin(true, false, true);
	M5.dis.clear();
}

void loop() {
	Serial.println("Hello blink");
	M5.dis.setBrightness(10);
	M5.dis.drawpix(0, CRGB::Red);
	delay(500);
	M5.dis.clear();
	delay(500);
}

