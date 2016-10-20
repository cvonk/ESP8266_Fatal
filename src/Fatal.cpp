/*
 * ESP8266 library to save fatal exception details to non-volatile memory
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
  This in an Arduino library to save exception details
  and stack trace to flash in case of ESP8266 crash.
  Please check repository below for details

  Repository: https://github.com/krzychb/EspSaveCrash
  File: EspSaveCrash.cpp
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

#include "Fatal.h"

#define ALIGN( type ) __attribute__((aligned( __alignof__( type ) )))
#define PACK( type )  __attribute__((aligned( __alignof__( type ) ), packed ))
#define SIZEOF4(n) ((sizeof(n) + sizeof(uint32_t) - 1) & ~(sizeof(uint32_t) - 1))  /* roundup to 4-byte boundary */

using namespace Fatal;

struct crashHeader_t {
	uint8_t count;  // number of crash sets in EEPROM
	uint16_t next;  // next place available to write a crash set, 0 if full
};

struct crashSet_t {
	uint32_t millis;          // time stamp that the crash occurred
	struct rst_info rstInfo;  // reset info
	uint32_t stackStart;      // address of stack[0] when crash occurred
	uint16_t stackLen;        // number of addresses on stack
	uint32_t stack[];         // stack values, array size is stackLen
}; 


uint8_t const
Fatal::begin(uint16_t const offsetInEeprom, uint16_t const sizeInEeprom)
{
	eeprom.offset = offsetInEeprom;
	eeprom.size = sizeInEeprom;

	if (eeprom.offset + eeprom.size > SPI_FLASH_SEC_SIZE) {
		return 1;  // doesn't fit
	}

	// reserve memory and copy EEPROM to that memory (do that now, because once we have to report
	// an exception, we might be low on memory)
	EEPROM.begin(eeprom.offset + eeprom.size);
	uint8_t * const ramPtr = EEPROM.getDataPtr();
	header = (crashHeader_t *)(ramPtr + eeprom.offset);

	return ramPtr != NULL;  // returns 0 on success
}
	

/**
  * Write crash information to EEPROM
  * called from core_esp8266_postmortem.c:__wrap_system_restart_local() when an exception occurs
  * don't use dynamic memory allocation, don't use blocking functions and remember the hardware watchdog timer is still ticking
  */
extern "C" void
custom_crash_callback(struct rst_info * rst_info, uint32_t stack, uint32_t stack_end )
{
	header = (crashHeader_t *)(EEPROM.getDataPtr() + eeprom.offset);  // mark dirty, 'cause user might have called clean()

	(void)EEPROM.getDataPtr();  

	if (header->count == 0xFF) {  // let's assume that the EEPROM wasn't initialized
		header->count = 0;
		header->next = SIZEOF4(crashHeader_t);
	}
	if (!header->next) {
		return;  // EEPROM full
	}

	// pointer to the RAM copy of the EEPROM where we should store the current crash set
	crashSet_t * const crashSet = (crashSet_t *)((uint32_t)header + header->next);
	*crashSet = {
		.millis = millis(),
		.rstInfo = *rst_info,
		.stackStart = stack,
		.stackLen = 0
	};

	for (uint32_t pos = stack; pos < stack_end; pos += sizeof(uint32_t)) {
		crashSet->stack[crashSet->stackLen++] = *(uint32_t *)pos;
	}
	header->count++;
	header->next += sizeof(crashSet_t) + crashSet->stackLen * sizeof(uint32_t);

	uint16_t const minStackDepthAcceptable = 5;
	if (header->next > Fatal::eeprom.size - SIZEOF4(crashSet_t) - minStackDepthAcceptable * sizeof(uint32_t)) {
		header->next = 0;
	}
	EEPROM.commit();
	Serial.println("**saved**");
}


/**
 * Print out crash information that has been previously saved in EEPROM
 * @param outputDev Print&    Optional. Where to print: Serial, Serial, WiFiClient, etc.
 */
void Fatal::print(Print& outputDev)
{
	uint8_t const uninitizedEepromVal = 0xFF;
	if (header->count && header->count != uninitizedEepromVal) {

		// pointer to the RAM copy of the EEPROM past the last crash set
		crashSet_t * const lastCrashSet = (crashSet_t *)((uint32)header + header->next);
		uint32_t addr = (uint32_t)header + SIZEOF4(crashHeader_t);

		bool incomplete = false;
		for (uint16_t ii = 0; ii < header->count && !incomplete; ii++) {
			crashSet_t const * const crashSet = (crashSet_t const *)addr;

			outputDev.printf("\nFatal # %d at %ld ms\nReason of restart: %d\nException cause: %d\n"
							 "epc1=0x%08x epc2=0x%08x epc3=0x%08x excvaddr=0x%08x depc=0x%08x\n"
							 ">>>stack>>>",
				ii + 1, crashSet->millis, crashSet->rstInfo.reason, crashSet->rstInfo.exccause,
				crashSet->rstInfo.epc1, crashSet->rstInfo.epc2, crashSet->rstInfo.epc3,
				crashSet->rstInfo.excvaddr, crashSet->rstInfo.depc);

			for (uint16_t jj = 0; jj < crashSet->stackLen; jj++) {

				if ((uint32_t)&crashSet->stack[jj] - (uint32_t)header - eeprom.offset > eeprom.size) {
					incomplete = true;
					break;
				}
				if (jj % 4 == 0) {  // print address
					outputDev.printf("\n%08x: ", crashSet->stackStart + jj * sizeof(uint32_t));
				}
				outputDev.printf("%08x ", crashSet->stack[jj]);  // print values
			}
			outputDev.println("\n<<<stack<<<");

			addr += SIZEOF4(crashSet_t) + crashSet->stackLen * sizeof(uint32_t);
		}
		if (incomplete) {
			outputDev.printf("Incomplete stack trace\n");
		} else if (addr - (uint32_t)header != header->next) {
			outputDev.printf("Consistency err\n");
		}
	}

	if (header->next) {
		outputDev.printf("%d bytes free\n", eeprom.size - header->next);
	} else {
		outputDev.println("Fatal full");
	}
}


/**
* Clear crash count in EEPROM
*/
void Fatal::clear(void)
{
	header->count = 0;
	header->next = SIZEOF4(crashHeader_t);
	EEPROM.commit();
}


/**
 * Get the count of crash data sets saved in EEPROM
 */
int Fatal::count()
{
	return header->count;
}
