#include <string.h>
#include <unistd.h>
#include <iostream>
#include <signal.h>
#include <time.h>

#include <mraa.hpp>

#include "RF24\RF24.h"
#include "RF24\RF24Network.h"
#include "util\UtilTime.h"

volatile int running = 0;

RF24 *comm = NULL;
mraa::Gpio *statusLed = NULL;

const uint8_t pipes[][6] = { {0xF0, 0xF0, 0xF0, 0xF0, 0xE1}, {0xF0, 0xF0, 0xF0, 0xF0, 0xD2} };

enum HARV_MODE {HARV_RX, HARV_TX};
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

void
globalInit()
{
	timeInit();

	comm = new RF24(7, 8);
	comm->begin();
	comm->stopListening();
	comm->setPALevel(RF24_PA_HIGH);

	signal(SIGINT, sig_handler);

	statusLed = new mraa::Gpio(3, true, true);
	statusLed->dir(mraa::DIR_OUT);
}

void pingPongInit(HARV_MODE mode)
{
	//comm->enableDynamicAck(); // [! if you uncomment this line, my devices will not clean TX_FIFO, like AUTOACK is disabled]
	comm->enableDynamicPayloads();

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

	comm->printDetails();

	// Start listening
	comm->startListening();
}


void rolePingOutExecute(void)
{
	unsigned long time = millis();
	std::cout << "pinging... [" << time << "]";

	// First, stop listening so we can talk.
	comm->stopListening();

	// Take the time, and send it.  This will block until complete
	bool ok = comm->writeFast(&time, sizeof(unsigned long));
	if (ok)
		ok = comm->txStandBy(30);

	// Now, continue listening
	comm->startListening();

	if (ok)
	{
		std::cout << " ok: " << millis() << std::endl;
	}
	else
	{
		std::cout << " failed: " << millis() << std::endl;
	}

	// Wait here until we get a response, or timeout (500ms)
	unsigned long started_waiting_at = millis();
	bool timeout = false;
	while (!comm->available() && !timeout)
		if (millis() - started_waiting_at > 500)
			timeout = true;

	// Describe the results
	if (timeout)
	{
		std::cout << millis() << ": timed out ..." << std::endl;
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
			comm->read(&got_time, sizeof(unsigned long));
			if (!comm->available())
				done = true;
		}

		std::cout << millis() << ": APP got time " << got_time << std::endl;

		// Delay just a little bit to let the other unit make the transition to receiver
		delay(20);

		// First, stop listening so we can talk
		comm->stopListening();

		// Send the final one back.
		bool ok = comm->writeFast(&got_time, sizeof(unsigned long));
		if (ok)
			ok = comm->txStandBy(30);

		// Now, resume listening so we catch the next packets.
		comm->startListening();

		if (ok)
			ledBlink(statusLed, 1, 200);
	}
}

int
main(int argc, char **argv)
{
	// Init time module, GPIOs, RF24
	globalInit();

	pingPongInit(mode);

	while (!running)
	{
		if (mode == HARV_TX)
			rolePingOutExecute();
		else if (mode == HARV_RX)
			rolePongBackExecute();
	}

	std::cout << "finalizing application" << std::endl;

	delete comm;

	return 0;
}

