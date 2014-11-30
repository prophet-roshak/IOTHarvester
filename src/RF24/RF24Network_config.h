
/*
 Copyright (C) 2011 James Coliz, Jr. <maniacbug@ymail.com>

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 version 2 as published by the Free Software Foundation.
 */

#ifndef __RF24NETWORK_CONFIG_H__
#define __RF24NETWORK_CONFIG_H__

#if defined (__linux) || defined (linux)

#else
	#if ARDUINO < 100
		#include <WProgram.h>
	#else
		#include <Arduino.h>
	#endif
#endif

#include <stddef.h>

/********** USER CONFIG **************/

//#define DUAL_HEAD_RADIO
//#define ENABLE_SLEEP_MODE
#define RF24NetworkMulticast
//#define DISABLE_FRAGMENTATION // Saves a bit of memory space by disabling fragmentation

/** System defines */
#define MAX_PAYLOAD_SIZE 128  //Size of fragmented network frames Note: With RF24ethernet, assign in multiples of 24. General minimum is 96 (a 32-byte ping from windows is 74 bytes, (Ethernet Header is 42))

//#define SERIAL_DEBUG
//#define SERIAL_DEBUG_MINIMAL
//#define SERIAL_DEBUG_ROUTING
//#define SERIAL_DEBUG_FRAGMENTATION
/*************************************/
 
#endif

#include "RF24_config.h"
// vim:ai:cin:sts=2 sw=2 ft=cpp
