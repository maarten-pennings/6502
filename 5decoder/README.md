# 5. Memory decoder
Trying to build a 6502 based computer. 

We may think we have a full fledged computer -- cpu, clock, rom, ram -- but the truth is that we do not yet have 
any peripherals. Think GPIO ports, a UART, and maybe even things like a small keyboard or display. For that we need
to add memory mapped IO, and for that we need an address decoder. 

The steps in this chapter
 - [5.1. Theory](README.md#51-Theory) - We see what an address decoder is
 - [5.2. Implementation](README.md#52-Implementation) - We implement our future proof decoder
 - [5.3. Test](README.md#53-Testing) - We add some "fake" peripherals to test our decoder
 

## 5.1. Theory

An address decoder is a piece of logic that uses the state of all address lines to activate a "location".
Typically this is a "memory location", but it could also be a "special function register" in a memory mapped device.

## 5.1.1. A single device

Let us start simple and examine a (simplified) memory.

![A single memory chip](decoder8x4.png)

We see here a memory chip with 3 address lines (A0, A1, A2). This means the chip can have (and indeed it has) 8 locations
(the grey columns consisting of four boxes). Each location in this (simplified) memory chip stores 4 bits 
(a box represents a single bit store). Memory designers call this an 8×4bit memory.

The important part to note here has a blue box labeled "address decoding". We see that every address line splits in two, 
one being high when the address lines is 0 and one being high when the address line is 1. Each location has an AND port 
that selects a unique address pattern from 000 to 111. The locations are labeled with these patterns.

Note the design pattern: address lines, inverters and multi input AND gates to activate locations. That implements address decoding.

Also note that the complete address decoding is internal in the chip. No external address decoding is needed when 
you have a single peripheral to the CPU. This is why in our [EEPROM only](../3eeprom/README.md#33-6502-with-eeprom-and-oscillator)
computer there was no (visible, external) address decoder.

## 5.1.2. A second device

Let us assume that we want to have a bigger memory in our computer. If you were a chip designer you might consider 
designing a bigger chip. Let's give it an extra address line and double the number of memory locations.

![Doubled the memory chip](decoder16x4.png)

When we look at the above memory chip, we see that it is a simple extension of the previous one.
The chip designer just added an A3 line, added the low and high select lines, and added a fourth input on each AND.
This extends the 8×4bit memory into an 16×4bit memory (16 locations of each 4 bit). Address decoding is (still) completely 
inside the 16×4bit memory chip.

But, if we are not a chip designer but a chip user, we could also just buy two 8×4bit memory chips. 
That gives us the same amount of memory.
The trick is that every chip has one extra control line that was not yet drawn in the 8×4bit memory of the previous section: 
the chip select or CS line. Please be aware that in this theoretical story I use "positive" logic, many chips have 
low-active CS lines.

![Two small memory chips](decoder2x8x4.png)

We see that the CS line is an input for every AND gate. Basically, when CS is low, the whole chip is disabled.
And that is precisely the point. When A3=0, the left chip is active, when A3=1, the right chip is active.

Note that a large part of the address decode is inside the two chips, but a small part is outside: the green part,
in this case a single NOT gate.

This is precisely the situation in [EEPROM/RAM](../4ram/README.md#421-simple-address-decoding): we had two chips,
and EEPROM and a RAM, and we used A15 with an inverter to select between the two.

## 5.1.3. Four peripherals

You might wonder if the "chip select line" approach scales. What happens if we have multiple memories or peripherals?
Yes, the CS architecture works, but the external address decoding logic increases. For example take the case of 4 
memory chips (A,B, C, and D).

![Four peripheral chips](decoder4x8x4.png)

We see the AND-gate pattern that was _inside_ the chip, now also emerges _outside_ the chip.

This is the architecture that we aim for in the [next section](README.md#52-Implementation).

## 5.1.4. Memory map

The address decoding logic relates one-to-one to a _memory map_.

The memory decoder with the four memories (A, B, C and D) leads to this memory map.

![memory map of the four peripheral chips](map4x8x4.png)

Device A runs from 0b00000-0b00111, or 0x00-0x07. The upper two bit are 0b00, so device A is could be called device 0.

Device B runs from 0b01000-0b01111, or 0x08-0x0F. The upper two bit are 0b01, so device B is could be called device 1.

Device C runs from 0b10000-0b10111, or 0x10-0x17. The upper two bit are 0b10, so device C is could be called device 2.

Device D runs from 0b11000-0b11111, or 0x18-0x1F. The upper two bit are 0b11, so device D is could be called device 3.



## 5.2. Implementation

After all the theory, let's build an address decoder for our 6502 computer.

In the previous chapter we already had an address decoder, but since it only had to choose between two chips, 
it was simple: A15 low selects RAM, A15 high selects ROM. Let's build one that not only decodes for a
ROM and a RAM, but also for a couple of peripherals.

### 5.2.1. Introduction

Does the below picture look familiar?

![74138](74138.png)

It should. It is drawn in a different style, but it is actually the inverters, AND gates and chip select we
have seen in the above theory on address decoding.

I did not draw that picture. It is the logic diagram representing the 74ACT138 chip, as
given in its [datasheet](https://www.onsemi.com/pub/Collateral/MC74AC138-D.PDF).

This is a so-called 1-of-8 decoder (or 3-to-8 decoder). One chip does all.

What is even more surprising is that the _outputs_ of the 74138 are low-active, and that happens
to match the memory chips I use: they all have a low-active chip select. No extra chips needed.

### 5.2.2. Description

The following address decoder is prepared for the "future".
It has a place for a 32k RAM, an 8k ROM and 6 peripherals, like 
GPIOs (implemented by e.g. a VIA chip - Versatile Interface Adapter) or 
UARTS (implemented by e.g. a ACIA Asynchronous Communication Interface Adapter).

![New address decoder](address-decode.png)

We use A15 to select (when 0) the RAM, or (when 1) the _decoder_.
The decoder splits the next 3 address lines (A14-A12) to 8 lines, each representing a 4k block in the memory map.
So each output line of the decoder corresponds with the highest nible of the address.

I plan to use an 8k ROM (not the 2k we have been using until now), so I needed to combine the upper two lines of the demux.
I used one NAND and one NAND as inverter. Those two were "left over" from the [two](../4ram/README.md#421-Simple-address-decoding) 
that create the OE and WE for the memory chips.


### 5.2.3. Address decode schematic

The schematic of our 6502/ROM/RAM with future proof decoder is as follows 
(ref for [R and C](../1clock/README.md#12-clock---oscillator) values).

![Schematics](eeprom-ram-decode.png)

Find below a photo of my breadboard, with labels added.

![Breadboard](eeprom-ram-decode.jpg)

### 5.2.4. Conclusion

We are running the same [sketch](../4ram/blinky-top.eeprom) as in the previous chapter.
And we get the same [result](https://youtu.be/LvaN9udekvI).

We have now room to add one or two VIAs for LEDs, 7-segment display, switches, or maybe even a scanning keyboard.
Or we could add a serial port. That is for the next chapters.


## 5.3. Testing

I wanted to test the address decode, especially the unused lines.

The idea I had was to hookup [SR-latches](https://www.ti.com/lit/ds/symlink/sn54ls279a.pdf). 
The _Set_ and _Reset_ inputs happen to be low-active. 
So by hooking S to nO0, and R to nO1 of the 3-to-8 decoder, I have made my own LED control. 
By accessing address 8xxx, the _Set_ would fire (LED on), and by accessing addres 9xxx, the _Reset_ would fire (LED off).

![SR-latch for LED control](sr-gpio.png)

I hooked a second SR-latch and LED to Axxx and Bxxx, and a third to Cxxx and Dxxx. See below for the schematic
(ref for [R and C](../1clock/README.md#12-clock---oscillator) values).

![Decoder test schematic](decoder-test.png).

This is the [6502 program](decoder-test.eeprom) I wrote (and flashed with the Arduino EEPROM programmer).

```
F800 MAIN
F800        LDA $8000 # 'SET' LED0
F803        JSR WAIT
F806        LDA $9000 # 'RESET' LED0
F809        JSR WAIT
F80C        LDA $A000 # 'SET' LED1
F80F        JSR WAIT
F812        LDA $B000 # 'RESET' LED1
F815        JSR WAIT
F818        LDA $C000 # 'SET' LED2
F81B        JSR WAIT
F81E        LDA $D000 # 'RESET' LED2
F821        JSR WAIT
F824        JMP MAIN

F827 WAIT
F827        LDX #$00
F829 WAIT1
F829        LDY #$00
F82B WAIT2
F82B        DEY
F82C        BNE WAIT2
F82E        DEX
F82F        BNE WAIT1
F831        RTS
```

Since the code is in ROM, the stack is in RAM (for the `JSR WAIT`), and the SR-latches are connected to the other pins 
of the address decoder, this [movie](https://youtu.be/w8RHORfYriM) qualifies as a passed test.

