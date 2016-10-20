/*
* Demonstrates ESP8266_Fatal library saving exception to non-volatile memory and sharing it through HTTP
*
* Platform: ESP8266 using Arduino IDE
* Documentation: https://github.com/cvonk/ESP8266_Fatal
* Tested with: Arduino IDE 1.6.11, board package esp8266 2.3.0, Adafruit huzzah (feather) esp8266
*
* GNU GENERAL PUBLIC LICENSE Version 3, check the file LICENSE.md for more information
* (c) Copyright 2016, Coert Vonk
* All text above must be included in any redistribution
*
* Based and inspired by krzychb's https://github.com/krzychb/EspSaveCrash (see below)
* The main difference is that my implementation calls EEPROM.begin() when the sketch is
* started, instead of when an exception occurs.  Train of thought is that once an exception
* occurs the system might be low on memory causing EEPROM.begin() to fail.
*/


/*
  Example application to show how to save crash information
  to ESP8266's flash using EspSaveCrash library
  Please check repository below for details

  Repository: https://github.com/krzychb/EspSaveCrash
  File: RemoteCrashCheck.ino
  Revision: 1.0.2
  Date: 17-Aug-2016
  Author: krzychb at gazeta.pl

  Copyright (c) 2016 Krzysztof Budzynski. All rights reserved.

  This application is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This application is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
*/

#include <Fatal.h>
#include <ESP8266WiFi.h>

const char* ssid = "*****";
const char* password = "******";

WiFiServer server(80);

void _menu(void)
{
	Serial.println(
		"Press a key + <enter>\n"
		"p : print crash information\n"
		"c : clear crash information\n"
		"r : reset module\n"
		"t : restart module\n"
		"s : cause software WDT exception\n"
		"h : cause hardware WDT exception\n"
		"0 : cause divide by 0 exception\n"
		"n : cause null pointer read exception\n"
		"w : cause null pointer write exception\n");
}

void setup(void)
{
	Serial.begin(115200);  // 2*38400, seems to be the default when booting from serial boot
	while (!Serial) {
		; // wait for serial connection
	}
	Serial.println("\nRemoteCrashTester.ino");
	Fatal::begin(0x0010, 0x0200);

	Serial.printf("Connecting to %s ", ssid);
	WiFi.persistent(false);
	WiFi.begin(ssid, password);
	while (WiFi.status() != WL_CONNECTED) {
		delay(500);
		Serial.print(".");
	}
	Serial.println(" connected");

	server.begin();
	Serial.printf("Web server started, open %s in a web browser\n", WiFi.localIP().toString().c_str());
	_menu();
}


void loop(void)
{
	// read line by line what the client (web browser) is requesting

	WiFiClient client = server.available();
	if (client) {
		Serial.println("\n[Client connected]");
		while (client.connected()) {
			// read line by line what the client (web browser) is requesting
			if (client.available()) {
				String line = client.readStringUntil('\r');
				// look for the end of client's request, that is marked with an empty line
				if (line.length() == 1 && line[0] == '\n')
				{
					// send response header to the web browser
					client.print("HTTP/1.1 200 OK\r\n");
					client.print("Content-Type: text/plain\r\n");
					client.print("Connection: close\r\n");
					client.print("\r\n");
					// send crash information to the web browser
					Fatal::print(client);
					Fatal::clear();
					break;
				}
			}
		}
		delay(1); // give the web browser time to receive the data
				  // close the connection:
		client.stop();
		Serial.println("[Client disconnected]");
	}

	// read the keyboard

	if (Serial.available()) {
		switch (Serial.read()) {
			case 'p':
				Fatal::print();
				break;
			case 'c':
				Fatal::clear();
				Serial.println("All clear");
				break;
			case 'r':
				Serial.println("Reset");
				ESP.reset();
				// nothing will be saved in EEPROM
				break;
			case 't':
				Serial.println("Restart");
				ESP.restart();
				// nothing will be saved in EEPROM
				break;
			case 'h':
				Serial.printf("Hardware WDT");
				ESP.wdtDisable();
				while (true) {
					; // block forever
				}
				// nothing will be saved in EEPROM
				break;
			case 's':
				Serial.printf("Software WDT (exc.4)");
				while (true) {
					; // block forever
				}
				break;
			case '0':
				Serial.println("Divide by zero (exc.0)");
				{
					int zero = 0;
					volatile int result = 1 / 0;
				}
				break;
			case 'e':
				Serial.println("Read from NULL ptr (exc.28)");
				{
					volatile int result = *(int *)NULL;
				}
				break;
			case 'w':
				Serial.println("Write to NULL ptr (exc.29)");
				*(int *)NULL = 0;
				break;
			case '\n':
			case '\r':
				break;
			default:
				_menu();
				break;
		}
	}
}
