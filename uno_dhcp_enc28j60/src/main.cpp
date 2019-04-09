/*
 * UNO: DHCP hello world with ENC28J60
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
#include <SPI.h>
#include <UIPEthernet.h>

EthernetClient client;

void print_address()
{
	int i;

	Serial.print("IP address: ");
	for (i = 0; i < 4; i++) {
		if (i)
			Serial.print(".");
		Serial.print(Ethernet.localIP()[i], DEC);
	}
	Serial.println();
}

byte mac[] = {
	0x00, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE
};

void setup() {
	Serial.begin(9600);
	Serial.println("UNO DHCP hello world");

	if (Ethernet.begin(mac) == 0) {
		Serial.println("Failed to get IP address using DHCP");
		return;
	}
	print_address();
}

void loop() {
	switch (Ethernet.maintain()) {
	case 1:
		Serial.println("Renew failed");
		break;
	case 2:
		Serial.println("Renew success");
		print_address();
		break;
	case 3:
		Serial.println("Rebind failed");
		break;
	case 4:
		Serial.println("Rebind success");
		print_address();
		break;
	default:
		break;

	}
}
