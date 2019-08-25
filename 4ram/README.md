# RAM
Trying to build a 6502 based computer. 

One could argue that the [previous](../3eeprom/README.md) chapter resulted in a real computer: 
oscillator, CPU, EEPROM with a running program. The problem is that the only (RAM) memory the 
program used was the (A, X and Y) registers.

In this chapter we will add RAM.
But it is time to understand ... time.
Timing of the 6502 and memories.

## Timing
The datasheet of the 6502 has timing diagrams. In this section we'll examine them.

### Timing diagrams

The figure below was taken from the [R65C02 datasheet](http://archive.6502.org/datasheets/rockwell_r65c00_microprocessors.pdf) .
I did remove some non-6502 aspects, and I use 1MHz version as context.

![6502 timing diagram](6502timing.png)

Those diagrams are packed with information. Take your time to study them. Some observations:
- On the left hand side we see signal names, like "ϕ0 (IN)".
- To the right of the signal name we see a curve describing that signal's behavior.
- Some signals are grouped, like RDY, nIRQ, nMNI, nRES because their curves behave identical.
- Some signals are split, for example D0-D7 _read_ behavior is split from D0-D7 _write_ behavior.
- There are vertical lines to delimit time intervals.
- Horizontal arrows between vertical lines couple a name to the interval, like tDLY.
- The interval names are detailed in the datasheet: tDLY is known as "ϕ0 Low to ϕ2 Low Skew" and is 50ns max.
- 50ns is a modest interval, compared to the "Cycle Time" tCYC of 1000ns (or 1us, recall, we run at 1MHz), but still 5%.
- Note also that (rising and falling) edges have horizontal tick lines that mark the moment the signal is considered high or low.

### 6502 Clocks

The 6502 is fed with a clock, this signal is the "ϕ0 (IN)" at the top of the diagram. 
In the diagram below I made the ϕ0 curve green.

![6502 timing diagram clocks](6502timing-phi0.png)

- The green ϕ0 signal is an input for the 6502 (we can not see that from the timing diagram).
- The 6502 only uses ϕ0 to generate ϕ1 and ϕ2. The 6502 itself uses ϕ1 and ϕ2 for timing purposes, not ϕ0. 
  ϕ1 and ϕ2 are made available to external devices so that they can synchronize with the 6502.
  Also ϕ1 and ϕ2 are can drive higher loads than what the oscillator (which drives ϕ0) may drive.
- Especially ϕ2 is the basis for timing external chips. I made it blue (I did not color ϕ1)
- Note that the ϕ2 signal is nearly identical to ϕ0; the only difference is a shift of tDLY, which is max 50 ns.
- Note that the ϕ1 signal is the _inverse_ of ϕ0; and it is also a bit delayed with respect to ϕ0.
- See the importance of ϕ2: the time of ϕ1 is specified with ϕ2 as basis (tDLY1), and not ϕ0 as basis.
- The datasheet specifies tDLY1 as "ϕ0 Low to ϕ1 High Skew" and ranges from -20ns to +20ns.
- Be alert to that: tDLY2 could be _negative_, which means its (rising) edge is actually _after_ the (falling) edge of ϕ2.
- I made two vertical lines red: they are the anchors for other events during a 6502 clock pulse.
- Finally note the timing aspects of that one pulse: tCL (ϕ2 Low Pulse Width) of minimally 430ns, tCH (ϕ2 High Pulse Width) 
  of minimally 450ns, and even the "Clock Rise and Fall Times" (tR and tF) of maximally 25ns.

### 6502 Read

In the diagram below I have removed most of the clock clutter and other signals, so that we can focus on the timing behavior 
of memory access. Let's first have a look at the _read_ process.

![6502 timing diagram read](6502timing-read.png)

- The key observation is that the 6502 clocks in the data when ϕ2 goes low. This is the red circle in the diagram.
  In other words, irrespective of the cycle time: **data read is on falling edge of ϕ2**. 
- Of course the data lines must be stable around that moment. That is the "diamond" enclosed in blue interval lines.
- "Read Data **Setup** Time" (tDSU) specifies that the data must be available at least 100ns before the falling edge.
- "Read Data **Hold** Time" (tHR) specifies that the data must be stable until at least 10ns after the falling edge.
- The memory chip must supply that data (by raising/lowering) the data lines, but can only do that after the address 
  of the read location is know.
- The 6502 guarantees that the _address_ lines are available after ϕ2 goes low 
  with a delay of at most tADS "Address Delay Time" (125ns, green, left).
- The 6502 guarantees that the _address_ lines are stable until after ϕ2 goes low again 
  with a delay of at least tHA "Address Hold Time" (15ns, green, right).
- On top of that, the memory chip must also know that the action is a read action. The 6502 must set the R/nW line to 1 (read). 
- The 6502 guarantees that the _R/nW_ line is available after ϕ2 goes low 
  with a delay of at most tRWS "R/nW Delay Time" (125ns, green, left).
- The 6502 guarantees that the _R/nW_ line is stable until after ϕ2 goes low again 
  with a delay of at least tHRW "R/nW Hold Time" (15ns, green, right).
- Note that the "hatched" section in row "D0-D7 (READ)" shows the interval in which the data lines vary (hence the hatching). 
  The memory chip is raising/lowering the data lines given the states of the address lines and R/nW. 
  The memory chip has the complete clock cycle (1000ns) for that minus tADS (125ns) and tDUS (100ns).
- When the cycle time grows (lower clock frequency), the time for the memory chips grows since the tADS and tDUS stay equal.
  When the cycle time shrinks (higher clock frequency), the time for the memory chip shrinks as well. When the cycle 
  time approaches tADS+tDUS the system will no longer work.

> **My conclusion for a read cycle.** The 6502 will put address on A0-A15, and 1 on R/nW.
> If the memory chip is enabled (address decoding), and output is enabled (from R/nW), the memory chip will put data on D0-D7.
> The data lines must remain stable till tHR after the falling edge of ϕ2 so that the 6502 can clock them in.

> Only gate _output enable_ of the memory chip with R/nW, and _not_ with ϕ2 (because then we miss tHR).

This last conclusion contradicts [Grant's 6502 computer](http://searle.hostei.com/grant/6502/Simple6502.html)
and that scares me. He's much better than me at this.


### 6502 Write

The diagram below focusses on the timing behavior of the _write_ process.

![6502 timing diagram write](6502timing-write.png)

- The key observation: when ϕ2 is low the 6502 is setting up A0-A15, R/nW _and_ D0-D7.
- At least "Write Data Delay Time" (tWDS, 200ns) after the rising edge of ϕ2, D0-D7 is available.
- The 6502 will keep D0-D7 stable until at least "Write Data Hold Time" (tHW, 30ns) after the falling edge of ϕ2.

> **My conclusion for a write cycle.** The 6502 will put address on A0-A15, 0 on R/nW and data on D0-D7.
> The memory chip should be enabled (address decoding), and write should be enabled (from R/nW) until ϕ2 falls.
> Not longer, because soon after that the address lines will change.

> Gate _write enable_ of the memory chip with R/nW, _and_ with ϕ2.


### Memory read

We should also have a look at the other side, the memory, for example the 
[AT28C16 datasheet](http://ww1.microchip.com/downloads/en/DeviceDoc/doc0540.pdf).
This is the timing diagram of read.

![AT28C16 read timing](AT28C16timing-read.png)

- The memory needs tACC of maximum 150ns after the address lines become valid to produce output on the datalines.
- The memory needs tCE of maximum 150ns after the "chip enable" becomes valid to produce output on the datalines.
- The memory needs tOE of maximum 700ns after the "output enable" becomes valid to produce output on the datalines.
- With the proposed wiring, all those signals become available (tADS ns) after the start of ϕ2 low, until ϕ2 goes low again.
- However, tOH "Output Hold" is specified to be 0ns. And this starts "from nOE, nCE or Address, whichever occurred first".
  So, if we would gate OE of the memory chip with ϕ2 going low, the memory chip would stop outputting data, 
  while the 6502 would stil nee dit for tHR


### Memory write
This is the timing diagram of the write process of the memory asd found in the 
[AT28C16 datasheet](http://ww1.microchip.com/downloads/en/DeviceDoc/doc0540.pdf).

![AT28C16 write timing](AT28C16timing-write.png)

- The core observation is that the memory chip uses the rising edge of nWE to clock in data.
- The falling edge of ϕ2 is an ideal candidate.


### Wiring
The conclusions for the read and write scenarios leads to following schematic.
Do note that most memory chips have the control signals low active: _ouput enable_ and _write enable_ and yes, even _chip enable_.

![memory read/write wiring](mem-rw-enable.png)


## Adding RAM

### Memory map

### Address decoding

### Schematic

### New firmware

