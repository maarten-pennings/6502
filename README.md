# 6502

Trying to build a 6502 based computer.

## 0 Introduction

Why the 6502 and which 6502, a short [introduction](0intro/README.md).

## 1 Clock

We need a clock, a source of pulses.

[Chapter one](1clock/README.md) discusses options for clocks. The hardest way is to build your own oscillator based on a crystal. I would recommend another option: a canned oscillator. This is basically the first option, but then completely done and packaged in a can. Third option, since many have it available: you could also use an Arduino Nano as clock source.

I also went a different route: slow clocks, based on a NE555 or even human button presses.

## 2 Emulation

The Nano as clock source was not only easy, it also gave me an idea. When the Nano controls the clock, it also knows when to sample the address lines. We then have a _spy_ that can follow each (clock) step.

That appears to work very well, as discussed in the [second chapter](2emulation/README.md). Would it even be possible to sample the data lines? Yes! and spoof the data lines? Yes! The Nano now emulates a memory in addition to a clock.

## 3 EEPROM

The Nano was fun, but this is a hardware project, not a software project. So, we keep the Nano to run the clock and use it to sample the address and data bus for tracing purposes. But we add a real memory. RAM would have advantages (can hold code and data), but I don't know how to fill the RAM with (initial) code. So let's first add EEPROM. With that working, we replace the Nano with a real oscillator.

The [third chapter](3eeprom/README.md) explains how to hook up an EEPROM to the 6502. And as a side project we develop an EEPROM programmer so that the 6502 has some code to work on (and the Nano has some code to spy on).

## 4 RAM

One could argue that the previous chapter resulted in a real computer: oscillator, CPU, EEPROM with a running program. The problem is that the only (RAM) memory the program used was the (A, X and Y) registers.

In chapter [four](4ram/README.md) we add a RAM.

As a side project, we study timing.

## 5 Memory decoder

We may think we have a full fledged computer -- cpu, clock, rom, ram -- but the truth is that we do not yet have any peripherals. Think GPIO ports, a UART, and maybe even things like a small keyboard or display. For that we need to add _memory mapped IO_, and for that we need an address decoder.  

When adding the RAM next to the ROM we already had a minimalistic address decoder. In chapter [five](5decoder/README.md) we add a "future proof" one.

## 6 GPIO

We have a computer without peripherals. In this [chapter](6gpio/README.md) we investigate options for "General Purpose Input and Output" or GPIO ports. We look back at solutions used in previous chapters, and introduce new ones. One of the candidates is the VIA, or Versatile Interface Adapter that was produced by MOS, the manufacturer of the 6502.

The VIA is a chip that implements two GPIO ports (each with 8 lines), has timers and interrupts.

## 7 UART

Adding the ACIA ...

## 8 Monitor

Adding a monitor program to write/read and go ...

## Other

Keyboard, 7-segment display, interrupt timer, single step, slow clock.

(end of doc)
