# RAM
Trying to build a 6502 based computer. 

One could argue that the [previous](../3eeprom/README.md) chapter resulted in a real computer: 
oscillator, CPU, EEPROM with a running program. The problem is that the only (RAM) memory the 
program used was the (A, X and Y) registers.

In this chapter we will add RAM.
But it is time to understand ... time.
Timing of the 6502 and memories.

## Timing
The datasheet of the 6502 has timing diagrams. The figure below was taken from the R65C02 datasheet.
I did remove some non-6502 aspects.

![6502 timing diagram](6502timing.png)

Those diagrams are packed with information. Take your time to study them. Some observations:
- on the left hand side we see signal names, like "ϕ0 (IN)"  
- to the right of the signal name we see a curve describing when that signals is low or high
- some signals are grouped, like RDY, nIRQ, nMNI, nRES because there curves behave identical
- some signals are split, for example D0-D7 _read_ behavior is split from D0-D7 _write_ behavior
- there a vertical lines to delimit time intervals
- (horizontal) arrows between vertical lines couple a name to the interval, like tDLY
- the interval names are detailed in the datasheet: tDLY is known as "ϕ0 Low to ϕ2 Low Skew" and is 50ns max.

The 6502 is fed with a 1MHz clock.
