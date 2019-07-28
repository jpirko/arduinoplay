/*
 * Arduino UNO temperature and humidity sensor SHT31 accessible over MQTT
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
#include <Ethernet.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_SHT31.h>
#include <OneWire.h>

#define ONEWIRE_ID_PIN 3

OneWire ds(ONEWIRE_ID_PIN);
Adafruit_SHT31 sht31 = Adafruit_SHT31();
EthernetClient ethClient;
PubSubClient client(ethClient);

char name[13];

#define topic_gen(item) (String(name) + "/" + (item))

static unsigned long last_measurement;
static unsigned int measurement_interval = 5000;

static void callback(char *topic, byte *payload, unsigned int length)
{
	long value;
	payload[length] = '\0';

	value = String((char *) payload).toInt();

	if (String(topic) == topic_gen("interval")) {
		measurement_interval = value;
		last_measurement = 0;
	}
}

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

void setup() {
	byte addr[8];
	byte *mac;

	Serial.begin(9600);
	Serial.println("UNO SHT31 sensor");

	/* Find a first available device on OneWire bus and take it's serial
	 * number as a base for MAC address.
	 */
	if (!ds.search(addr) || OneWire::crc8(addr, 7) != addr[7]) {
		Serial.println("Fatal - Failed to get OneWire ID\n");
		return;
	}

	mac = addr + 1;
	mac[0] &= 0xfe; /* Clear multicast bit. */
	mac[0] |= 0x02; /* Set local assignment bit. */

	if (Ethernet.begin(mac) == 0) {
		Serial.println("Fatal - Failed to get IP address using DHCP\n");
		return;
	}
	print_address();

	sprintf(name, "%02x%02x%02x%02x%02x%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	Serial.println("Name: " + String(name));

	Wire.begin();

	if (!sht31.begin(0x44))
		Serial.println("Couldn't find SHT31");

	/* Use assigned gateway IP as MQTT broker address with default port */
	client.setServer(Ethernet.gatewayIP(), 1883);
	client.setCallback(callback);
}

static bool timeout_reached(unsigned long *last, unsigned long interval)
{
	unsigned long now = millis();

	if (!*last || *last + interval < now || *last > now) {
		*last = now;
		return true;
	}
	return false;
}

static void measurements_publish(void)
{
	float temp;
	float hum;

	if (!timeout_reached(&last_measurement, measurement_interval))
		return;

	temp = sht31.readTemperature();
	hum = sht31.readHumidity();

	if (!isnan(temp))
		client.publish(topic_gen("temperature").c_str(),
			       String(temp).c_str());
	if (!isnan(hum))
		client.publish(topic_gen("humidity").c_str(),
			       String(hum).c_str());
}

static bool mqtt_connected;
static unsigned long mqtt_last_attempt;

#define MQTT_RETRY_TIMEOUT 5000

void loop() {
	if (!client.connected()) {
		if (mqtt_connected) {
			mqtt_last_attempt = 0;
			Serial.println("Disconnected from MQTT server");
			mqtt_connected = false;
		}

		if (!timeout_reached(&mqtt_last_attempt, MQTT_RETRY_TIMEOUT)) {
			if (client.connect(name)) {
				Serial.println("Connected to MQTT server");
				mqtt_connected = true;
				measurements_publish();
				client.subscribe(topic_gen("interval").c_str());
			}
		}
	} else {
		measurements_publish();
		client.loop();
	}
}
