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
Typically this is a memory location, but it could also be a "special function register" in a memory mapped device.

## 5.1.1. A single device

Let us start simple and examine a (simplified) memory.

![A single memory chip](decoder8x4.png)

We see here a memory chip with 3 address lines (A0, A1, A2). This means the chip can have (and indeed it has) 8 locations.
Each location in this (simplified) memory chip stores 4 bits. Memory designers call this an 8×4bit memory.


## 5.2. Implementation

### 5.2.1. Description

An address decoder looks at the address lines and decides which chip to enable: ROM, RAM, GPIO, UART etc.
In the previous section we already had an address decoder, but since it only had to choose between two chips, 
it was simple: A15 low selects RAM, A15 high selects ROM.

The following address decoder is prepared for the "future".
It has a place for an 32k RAM, an 8k ROM and 6 peripherals, like 
GPIOs (implemented by e.g. a VIA chip - Versatile Interface Adapter) or 
UARTS (implemented by e.g. a ACIA Asynchronous Communication Interface).

The [74138](https://www.onsemi.com/pub/Collateral/MC74AC138-D.PDF) decodes 3 ("binary") address lines into 1-of-8.
To my surprise, the outputs of the 74138 are low active, which happens to map perfectly to the nCE of most peripherals.

![New address decoder](address-decode.png)

We use A15 to select (when 0) the RAM, or (when 1) the _decoder_.
The decoder splits the next 3 address lines (A14-A12) to 8 lines, each representing a 4k block in the memory map.
So each decoder output line corresponds with the highest nible of the address.

I plan to use an 8k ROM (not the 2k we have been using until now), so I needed to AND the upper two lines of the demux.
I used one NAND and one NAND as inverter. Those two were "left over" from the [two](README.md#4-2-1-Simple-address-decoding) 
that create the OE and WE for the memory chips.


### 5.2.2. Address decode schematic

The schematic of our 6502/ROM/RAM with future proof decoder is as follows.

![Schematics](eeprom-ram-decode.png)

Find below a photo of my breadboard, with labels added.

![Breadboard](eeprom-ram-decode.jpg)

### 5.2.3. Conclusion

We are running the same [sketch](../4ram/blinky-top.eeprom) as in the previous chapter.
And we get the same [result](https://youtu.be/LvaN9udekvI).

We have now room to add one or two VIAs for LEDs, 7-segment display, switches, or maybe even a scanning keyboard.
Or we could add a serial port. That is for the next chapters.


## 5.3. Testing

