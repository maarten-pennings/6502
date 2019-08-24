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
- On the left hand side we see signal names, like "ϕ0 (IN)".
- To the right of the signal name we see a curve describing that signal's behavior.
- Some signals are grouped, like RDY, nIRQ, nMNI, nRES because their curves behave identical.
- Some signals are split, for example D0-D7 _read_ behavior is split from D0-D7 _write_ behavior.
- There a vertical lines to delimit time intervals.
- Horizontal arrows between vertical lines couple a name to the interval, like tDLY.
- The interval names are detailed in the datasheet: tDLY is known as "ϕ0 Low to ϕ2 Low Skew" and is 50ns max.
- 50ns is a modest interval, compared to the "Cycle Time" tCYC of 1000ns (or 1us, recall, we run at 1MHz), but still 5%.
- Note also that (rising and falling) edges have horizontal tick lines that mark the moment the signal is considered high or low.

The 6502 is fed with a clock, this signal is the "ϕ0 (IN)" at the top of the diagram. 
In the diagram below I made the ϕ0 curve green.

![6502 timing diagram clocks](6502timing-phi0.png)

- The green ϕ0 signal is an input for the 6502 (we can not see that from the timing diagram).
- The 6502 derives two output signals from it: ϕ1 and ϕ2. 
- The ϕ2 is the basis for other timings, it is an important signal, I made it blue (i did not color ϕ1)
- Note that the ϕ2 signal is nearly identical to ϕ0; the only difference is a shift of tDLY, which is max 50 ns.
- Note that the ϕ1 signal is the _inverse_ of ϕ0; and it is also a bit delayed with respect to ϕ0.
- See the importance of ϕ2: the time of ϕ1 is specified with ϕ2 as basis (tDLY1), and not ϕ0 as basis.
- The datasheet specifies tDLY1 as "ϕ0 Low to ϕ1 High Skew" and ranges from -20 to +20 ns.
- Be alert to that: tDLY2 could be -20, which means its (rising) edge is actually _after_ the (falling) edge of ϕ2.
- I made two vertical lines red: they are the anchors for other events during a clock pulse.
