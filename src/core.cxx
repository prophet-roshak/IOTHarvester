#include <string.h>
#include <unistd.h>
#include <iostream>
#include <signal.h>
#include <time.h>

#include <mraa.hpp>

#include "RF24\RF24.h"
#include "RF24\RF24Network.h"
#include "util\UtilTime.h"

#include "sensornet.hpp"
uint16_t this_node;                                  // Our node address

volatile int running = 0;

RF24 *comm = NULL;
RF24Network *network = NULL;
mraa::Gpio *statusLed = NULL;

struct SensorData {
	float temp;
	float humidity;
	long vcc;
};

const short max_active_nodes = 10;                    // Array of nodes we are aware of
uint16_t active_nodes[max_active_nodes];
short num_active_nodes = 0;
short next_ping_node_index = 0;

unsigned long awakeTime = 500;
unsigned long sleepTime = 0;

bool send_D(uint16_t to);                              // Prototypes for functions to send & handle messages
void handle_D(RF24NetworkHeader& header);
void outputTimestamp(void);

bool send_N(uint16_t to);
void handle_N(RF24NetworkHeader& header);
void add_node(uint16_t node);

/*
uint8_t pipes[][6] = { {0xF0, 0xF0, 0xF0, 0xF0, 0xE1}, {0xF0, 0xF0, 0xF0, 0xF0, 0xD2} };

typedef enum {HARV_RX, HARV_TX} HARV_MODE;
HARV_MODE mode = HARV_RX;
*/
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

	//! [Interesting]
	comm = new RF24(7, 8);
	comm->begin();
	network = new RF24Network(*comm);

	this_node = node_address_set[NODE_ADDRESS];
	network->begin(/*channel*/ 100, /*node address*/ this_node );
	/*
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
	*/
	comm->printDetails();


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
		SensorData data;
		bool done = false;
		while (!done)
		{
			// Fetch the payload, and see if this was the last one.
			comm->read(&data, sizeof(SensorData));
			// Delay just a little bit to let the other unit make the transition to receiver
			delay(20);

			if (!comm->available())
				done = true;
		}

		std::cout << "Recieved: T: " << data.temp;
		std::cout << " H: " << data.humidity;
		std::cout << " VCC: " << data.vcc << std::endl;

		// First, stop listening so we can talk
		comm->stopListening();

		// Send the final one back.
		//comm->write(&got_time, sizeof(unsigned long));
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
/*		if (mode == HARV_TX)
			rolePingOutExecute();
		else if (mode == HARV_RX)
			rolePongBackExecute();*/

		network->update();

		// do network part
		while (network->available())
		{
			RF24NetworkHeader header; // If network available, take a look at it
			network->peek(header);

			switch (header.type) { // Dispatch the message to the correct handler.

				case 'N':
					handle_N(header);
					break;

				case 'D':
					handle_D(header);
					break;

					/************* SLEEP MODE *********/
					// Note: A 'sleep' header has been defined, and should only need to be ignored if a node is routing traffic to itself
					// The header is defined as:  RF24NetworkHeader sleepHeader(/*to node*/ 00, /*type*/ 'S' /*Sleep*/);
				case 'S': /*This is a sleep payload, do nothing*/
					break;

				default:
					printf_P(PSTR("*** WARNING *** Unknown message type %c\n\r"),
							header.type);
					network->read(header, 0, 0);
					break;
			};
		}
	}


	std::cout << "finalizing application" << std::endl;

	delete comm;
	//! [Interesting]
	return 0;
}

/**
 * Send an 'N' message, the active node list
 */
bool send_D(uint16_t to)
{
  RF24NetworkHeader header(/*to node*/ to, /*type*/ 'N' /*Time*/);

  printf_P(PSTR("---------------------------------\n\r"));
  printf_P(PSTR("%lu: APP Sending active nodes to 0%o...\n\r"),millis(),to);
  return network->write(header,active_nodes,sizeof(active_nodes));
}

/**
 * Handle an 'N' message, the active node list
 */
void handle_D(RF24NetworkHeader& header)
{
	static SensorData data;
	network->read(header,&data,sizeof(SensorData));

	// get timestamp
	outputTimestamp();

	// Output data
	std::cout << " T: " << data.temp;
	std::cout << " H: " << data.humidity;
	std::cout << " VCC: " << data.vcc << std::endl;

	ledBlink(statusLed, 1, 200);
}

void outputTimestamp(void)
{
	time_t rawtime;
	struct tm * timeinfo;
	char buffer [80];

	time (&rawtime);
	timeinfo = localtime (&rawtime);

	strftime (buffer,80,"%c",timeinfo);
	std::cout << buffer << ":";
}

/**
 * Send an 'N' message, the active node list
 */
bool send_N(uint16_t to)
{
  RF24NetworkHeader header(/*to node*/ to, /*type*/ 'N' /*Time*/);

  printf_P(PSTR("---------------------------------\n\r"));
  printf_P(PSTR("%lu: APP Sending active nodes to 0%o...\n\r"),millis(),to);
  return network->write(header,active_nodes,sizeof(active_nodes));
}

/**
 * Handle an 'N' message, the active node list
 */
void handle_N(RF24NetworkHeader& header)
{
  static uint16_t incoming_nodes[max_active_nodes];

  network->read(header,&incoming_nodes,sizeof(incoming_nodes));
  printf_P(PSTR("%lu: APP Received nodes from 0%o\n\r"),millis(),header.from_node);

  int i = 0;
  while ( i < max_active_nodes && incoming_nodes[i] > 00 )
    add_node(incoming_nodes[i++]);
}

/**
 * Add a particular node to the current list of active nodes
 */
void add_node(uint16_t node){

  short i = num_active_nodes;                                    // Do we already know about this node?
  while (i--)  {
    if ( active_nodes[i] == node )
        break;
  }

  if ( i == -1 && num_active_nodes < max_active_nodes ){         // If not, add it to the table
      active_nodes[num_active_nodes++] = node;
      printf_P(PSTR("%lu: APP Added 0%o to list of active nodes.\n\r"),millis(),node);
  }
}

