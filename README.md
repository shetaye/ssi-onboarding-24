# Sat Software Onboarding F24

Welcome to software! We put code **in space**!!

First, some context.

Long ago, SSI designed a CubeSat "framework" called the PyCubed. It was great,
because it was a "commercial off-the-shelf" satellite, meaning we could build it
quickly and reliably.

While much of the original design had changed up until this year, one design
decision stuck with us: CircuitPython for our flightcode.

Over time, the benefits of Python as a language began to pale in comparison to
its costs: implicit heap allocations caused random (catastrophic) errors, lack
of proper interrupt support meant lots of polling, lack of explicit static
memory allocation made the aforementioned heap allocations worse, etc.

This year, we decided to finally pivot and ***rewrite all flight
software in C***.

We *also* switched microcontrollers from the SAMD51 (which was mostly hidden
from us by CircuitPython) to the lovely RP2350 (A.K.A. the Raspberry Pi Pico 2).

What does that mean for ***you***? Lots of opportunity to build on what you
learned in CS 106A, 106B, 107, 107E, etc. etc. and build some **real satellite
flight code**. Not only is this a *great* learning experience, but it's
**great** on a resume! SSI software alumni have gone on to NASA, SpaceX, and
grad school ;).

Today we're going to be building a blinking LED. That sounds trivial, but it's a
good way to get all the tools you need set up.

# Part 0: Install

We recommend using VSCode because it comes with an easy-to-use plugin for the
Raspberry Pi Pico.

If you *don't* want to use VSCode and instead want to compile via CLI, come talk
to me (Joseph) since there is a small fix up you need to do to make your build
environment work with the VSCode plugin.

**IMPORTANT: If you have taken 107E or 140E you might have issues with the
plugin trying to install the ARM GCC compiler on top of the one you installed
for class - come talk to us if you're having trouble with this**

Install VSCode: https://code.visualstudio.com/
Install the plugin: https://marketplace.visualstudio.com/items?itemName=raspberry-pi.raspberry-pi-pico

# Part 1: Blinking LED

All microcontrollers come with some pins you can turn on and off, communicate
over, etc. These pins are called *general purpose input/output pins* or GPIO
pins. You can configure them from software.

Generally, these are the steps:
- Initialize the pin
- Set the direction (input or output) of the pin
- Write (if it's output) or read (if it's input) from the pin

Some GPIO pins are connected to physical wires on the board, whereas others are
routed to built-in components. For this example, we are going to use pin 25,
which is routed to an LED on the board. This magic number is provided with a
convenient name in the SDK: `PICO_DEFAULT_LED_PIN`.

Microcontrollers also come with some timing facilities. For now, all we need to
use is the sleeping methods, which are exactly like the ones from Python.

## Code

Put this in your `main.c`
```c
#include "pico/stdlib.h"

int main() {
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    while (1) {
        gpio_put(PICO_DEFAULT_LED_PIN, 0);
        sleep_ms(250);
        gpio_put(PICO_DEFAULT_LED_PIN, 1);
        sleep_ms(1000);
    }
}
```

## Build system

We use **CMake** to build our project. It is like Makefiles for Makefiles.
Honestly, it isn't pretty, but it works quite well! CMake needs to be aware of
all your source files. Each source file is tied to a single *executable* that
CMake will build.

For this example, all you need to do is go to `CMakeLists.txt` and replace `#
FIXME #` with the name of the c file you put all the prior code into.

## Building

This will be different

## Uploading

# Part 2: Scheduler

Come talk to me if you're curious!
