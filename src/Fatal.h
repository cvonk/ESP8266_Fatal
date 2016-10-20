/*
  This in an Arduino library to save exception details
  and stack trace to flash in case of ESP8266 crash.
  Please check repository below for details

  Repository: https://github.com/krzychb/EspSaveCrash
  File: EspSaveCrash.h
  Revision: 1.0.2
  Date: 18-Aug-2016
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

#pragma once
#include <Arduino.h>
#include <EEPROM.h>
#include <user_interface.h>

struct crashHeader_t;

namespace Fatal
{
	uint8_t const begin(uint16_t const offsetInEeprom, uint16_t const sizeInEeprom);
	void print(Print& outDevice = Serial);
	void clear(void);
	int count(void);

	static struct {
		uint16_t offset;  // number of bytes to skip in EEPROM
		uint16_t size;    // number of bytes to use in EEPROM
	} eeprom;
	static crashHeader_t * header; // pointer to our section of the RAM copy of the EEPROM
};
