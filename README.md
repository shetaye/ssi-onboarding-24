# Sat Software Onboarding F24

Welcome to software! We put code **in space**!!

# Part -1: History

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
grad school ;)

Today we're going to be building a blinking LED. That sounds trivial, but it's a
good way to get all the tools you need set up.

# Part 0: Install

### VSCode

Install VSCode: https://code.visualstudio.com/
Install the plugin: https://marketplace.visualstudio.com/items?itemName=raspberry-pi.raspberry-pi-pico

### CLI

Lovingly taken from
[CS140e](https://github.com/dddrrreee/cs140e-24win/blob/main/labs/1-compile)

#### Windows

Use VSCode, sorry!

#### macOS

**UPDATE: The Internet Archive (where those cs107e notes are hosted) is down
because of a cyber attack. Great timing. I'm pretty sure all they have you do
is:**
```
brew install cs107e/cs107e/arm-none-eabi-test
```

Use the [cs107e install notes](https://web.archive.org/web/20210414133806/http://cs107e.github.io/guides/install/mac/).
Note: do not install the python stuff. We will use their custom brew formula!

If you get an error that it can't find `string.h`, you want to set `CPATH`
to the empty string (see a TA for help if you need it).

#### Linux

For [ubuntu/linux](https://askubuntu.com/questions/1243252/how-to-install-arm-none-eabi-gdb-on-ubuntu-20-04-lts-focal-fossa), ARM recently
changed their method for distributing the tool change. Now you
must manually install. As of this lab, the following works:

        wget https://developer.arm.com/-/media/Files/downloads/gnu-rm/10.3-2021.10/gcc-arm-none-eabi-10.3-2021.10-x86_64-linux.tar.bz2

        sudo tar xjf gcc-arm-none-eabi-10.3-2021.10-x86_64-linux.tar.bz2 -C /usr/opt/

We want to get these binaries on our `$PATH` so we don't have to type the
full path to them every time. There's a fast and messy option, or a slower
and cleaner option.

The fast and messy option is to add symlinks to these in your system `bin`
folder:

        sudo ln -s /usr/opt/gcc-arm-none-eabi-10.3-2021.10/bin/* /usr/bin/

The cleaner option is to add `/usr/opt/gcc-arm-none-eabi-10.3-2021.10/bin` to
your `$PATH` variable in your shell configuration file (e.g., `.zshrc` or
`.bashrc`), save it, and `source` the configuration. When you run:

        arm-none-eabi-gcc
        arm-none-eabi-ar
        arm-none-eabi-objdump

You should not get a "Command not found" error.

If gcc can't find header files, try:

       sudo apt-get install libnewlib-arm-none-eabi

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

This will be different depending on whether you use VSCode or the CLI.

With the CLI, make a directory called `build`, `cd` into it, run `cmake ..`,
then run `make`.

## Uploading

Unplug your pi, hold the button on your pi, plug it in, then move
`build/onboarding.uf2` into the drive that shows up.

# Part 1.5: Github

Now we're going to write code within the actual SSI codebase.

## Git

We will get each of you set up on the GitHub. If you don't already have an
account, make one [here](https://github.com).

If you have no or little experience with Git, I recommend installing [Github
Desktop](https://desktop.github.com/download/). We will be running a special
onboarding session for Git later on.

## Repository

Clone the [SAMWISE FSW
repository](https://github.com/stanford-ssi/samwise-flight-software) via
whatever Git client you want

# Part 2: Scheduler

The flight software is organized into a state machine. The satellite can be in
any of several states e.g. "running," "low power," etc.

Each state is associated with tasks that are run regularily (e.g. every half a
second) while the satellite is in that state e.g. the running state might be
associated with a "beacon" task.

If a task is associated with the active state, its `task_dispatch` function is
called at most every `dispatch_period_ms` milliseconds. Additionally, its
`task_init` function is called as part of satellite boot.

You will also see references to a "slate." The slate is a huge block of
statically allocated memory. 

For this part, you are going to write your own task.

## Code Navigation

The scheduler code is located in `scheduler/`. It isn't necessary for this part,
but it might be interesting to you.

The state machine is located in `state_machine/`, with tasks in
`state_machine/tasks` and states in `state_machine/states`. There is a README in
`state_machine` that explains how to add new tasks and states.

## Your task

Make it do whatever you want!
