/*
 * Author: Brendan Le Foll <brendan.le.foll@intel.com>
 * Author: Thomas Ingleby <thomas.c.ingleby@intel.com>
 * Author: Matthias Hahn <matthias.hahn@intel.com>
 * Copyright (c) 2014 Intel Corporation.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "mraa.hpp"

static int iopin = 13;
int running = 0;

void
sig_handler(int signo)
{
    if (signo == SIGINT) {
        printf("closing IO%d nicely\n", iopin);
        running = -1;
    }
}

int main (int argc, char **argv)
{
    mraa_platform_t platform = mraa_get_platform_type();
    switch (platform) {
        case MRAA_INTEL_GALILEO_GEN1:
            iopin = 3;
            break;
        case MRAA_INTEL_GALILEO_GEN2:
            iopin = 13;
            break ;
        default:
            iopin = 13;
    }

	if (argc < 2) {
        printf("Provide an int arg if you want to flash an LED on some other IO port than IO %d\n", iopin);
    } else {
        iopin = strtol(argv[1], NULL, 10);
    }

    signal(SIGINT, sig_handler);

//! [Interesting]
    mraa::Gpio* gpio = NULL;
    switch (platform) {
        case MRAA_INTEL_GALILEO_GEN1:
        	gpio = new mraa::Gpio(iopin,true,true);
            break;
        case MRAA_INTEL_GALILEO_GEN2:
        	gpio = new mraa::Gpio(iopin,true,false);
            break;
        default:
        	gpio = new mraa::Gpio(iopin,true,false);
    }
    if (gpio == NULL) {
        return MRAA_ERROR_UNSPECIFIED;
    }
    int response = gpio->dir(mraa::DIR_OUT);
    if (response != MRAA_SUCCESS)
        mraa_result_print((mraa_result_t) MRAA_SUCCESS);

    while (running == 0) {
        response = gpio->write(1);
        sleep(1);
        response = gpio->write(0);
        sleep(1);
    }
    delete gpio;
    return response;
//! [Interesting]
}
