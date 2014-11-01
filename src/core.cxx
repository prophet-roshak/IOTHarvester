#include <string.h>
#include <unistd.h>
#include <iostream>
#include <signal.h>

#include <mraa.hpp>

#include "RF24\RF24.h"
#include "util\UtilTime.h"

int running = 0;
int waiting = 0;

RF24 *comm = NULL;
mraa::Gpio *statusLed = NULL;

/*
uint8_t harvAddress[5]			= {0xE1, 0xF0, 0xF0, 0xF0, 0xF0};
uint8_t remoteNodeAddress[5]	= {0xD2, 0xF0, 0xF0, 0xF0, 0xF0};

uint8_t broadcast_address[5] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
*/

const uint64_t pipes[2] = { 0xF0F0F0F0E1LL, 0xF0F0F0F0D2LL };

typedef enum {HARV_RX, HARV_TX} HARV_MODE;
HARV_MODE mode = HARV_TX;

void
sig_handler(int signo)
{
    printf("got signal\n");
    if (signo == SIGINT) {
        printf("exiting application\n");
        running = 1;
    }
}

void
ledBlink(mraa::Gpio* led, uint8_t iAmount, uint16_t iDelay)
{
	uint8_t i;
	for (i=0;i<iAmount;i++)
	{
		led->write(1);
		delay(iDelay);
		led->write(0);
		delay(iDelay/2);
	}
}
/*
//! [Interesting]
void
nrf_handler () {
	uint32_t recievedUs = 0;

	memcpy (&recievedUs, comm->m_rxBuffer, sizeof (recievedUs));
    std::cout << "Reciever :: roundtrip: " << (millis() - recievedUs) << "us" << std::endl;
    waiting = 0;
    ledBlink(statusLed, 1, 100);
}
//! [Interesting]
*/

void
globalInit()
{
	timeInit();

	//! [Interesting]
	comm = new RF24(7, 8);

	if (mode == HARV_TX)
	{
		comm->openWritingPipe(pipes[0]);
		comm->openReadingPipe(1, pipes[1]);
	}
	else
	{
		comm->openWritingPipe(pipes[1]);
		comm->openReadingPipe(1, pipes[0]);
	}

	// Start listening
	comm->startListening();


	/*
	comm->setPayload (MAX_BUFFER);
	comm->setSpeedRate (upm::NRF_1MBPS);
	comm->setChannel (76);

	comm->setSourceAddress ((uint8_t *) harvAddress);
	comm->setDestinationAddress ((uint8_t *) remoteNodeAddress);
	comm->setBroadcastAddress ((uint8_t *) remoteNodeAddress);

	// radio is basically in receive mode
	comm->dataRecievedHandler = nrf_handler;
	comm->configure ();
	*/

	signal(SIGINT, sig_handler);

	statusLed = new mraa::Gpio(3, true, true);
	statusLed->dir(mraa::DIR_OUT);
}

void rolePingOutExecute(void)
{
	// First, stop listening so we can talk.
	comm->stopListening();

	// Take the time, and send it.  This will block until complete
	unsigned long time = millis();
	std::cout << "pinging... [" << time << "]";
	bool ok = comm->write(&time, sizeof(unsigned long));

	if (ok)
	{
		std::cout << " ok." << std::endl;
	}
	else
	{
		std::cout << " failed." << std::endl;
	}

	// Now, continue listening
	comm->startListening();

	// Wait here until we get a response, or timeout (500ms)
	unsigned long started_waiting_at = millis();
	bool timeout = false;
	while (!comm->available() && !timeout)
		if (millis() - started_waiting_at > 500)
			timeout = true;

	// Describe the results
	if (timeout)
	{
		std::cout << " timed out ..." << std::endl;
		// Timed out, blink error
		ledBlink(statusLed, 5, 200);
	}
	else
	{
		// Grab the response, compare, and send to debugging spew
		unsigned long got_time;
		comm->read(&got_time, sizeof(unsigned long));

		std::cout << " got response " << got_time << ", round-trip delay: " << (millis()-got_time) << std::endl;
	}

	// Try again 1s later
	delay(1000);
}

void rolePongBackExecute(void)
{
	// if there is data ready
	if (comm->available())
	{
		// Dump the payloads until we've gotten everything
		unsigned long got_time;
		bool done = false;
		while (!done)
		{
			// Fetch the payload, and see if this was the last one.
			done = comm->read(&got_time, sizeof(unsigned long));
			// Delay just a little bit to let the other unit make the transition to receiver
			delay(20);
		}

		// First, stop listening so we can talk
		comm->stopListening();

		// Send the final one back.
		comm->write(&got_time, sizeof(unsigned long));
		ledBlink(statusLed, 1, 200);

		// Now, resume listening so we catch the next packets.
		comm->startListening();
	}
}

int
main(int argc, char **argv)
{
	globalInit();

	while (!running)
	{
		if (mode == HARV_TX)
			rolePingOutExecute();
		else if (mode == HARV_RX)
			rolePongBackExecute();
	}

	std::cout << "exiting application" << std::endl;

	delete comm;
	//! [Interesting]
	return 0;
}
