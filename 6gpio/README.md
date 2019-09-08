# 6. GPIO
Trying to build a 6502 based computer. 

We are going to add peripherals to our computer.
In this chapter we look at various options for General Purpose Input and Output (GPIO) ports.


## 6.1. Address pins

One of the first things we did to get feedback from the 6502 is to hook up LEDs to address lines.
We used two approaches.

The first approach was just to monitor the address changes. 
We used this during the [free run](../1clock#12-clock---oscillator).
The CPU runs NOP after NOP; it starts at EAEA and runs to FFFF, wraps to 0000 and so on. 
During one loop the CPU executes 64k NOPs, each at 2 cycles, so this takes 131 072 cycles or 0.13s.
This means that the blink frequency of A15 is 8Hz, quite fast.

This first approach is ok for a free run, but as soon as we start writing real programs, the loops
are typically much smaller, so monitoring an address line no longer works.

Well, enter the second approach. We had a clever idea of having wait loops on specific address ranges.
As long as the wait loop executes in that range, some address pins have a fixed value.
As long as the wait loop executes in another range, that address pins has the opposite value.
We can use that for creating a blink with configurable.

This is what we did when our computer consisted only of a [6502 and EEPROM](../3eeprom#34-blinky).
Even after adding a [RAM](../4ram#42-adding-ram) we used this approach because we had no other peripherals.

But even the second appraoch is not very generic: JMPs, RAM accesses and ISRs tend to break the LED patterns.


## 6.2. Address decoding

To have control over LEDs, we need a "GPOI peripheral". In the 6502 world, peripherals are memory mapped.
This means we need an address decoder to activate the different peripherals.

The idea is that the decoder has an activation line ("Chip Select") for the peripheral.
The activation line is active for some memory range. 
The peripheral implements a set of registers in that range, so that the 6502 can write (or read) those registers,
and the peripherals takes action. This is how, for example, the [VIA](README.md#64-VIA) works.

In the section that [tested the decoder](../5decoder#53-testing) we had 6 activation lines for 6 address ranges 
(8xxx, 9xxx, Axxx, Bxxx, Cxxx, and Dxxx). And we simply used two activation lines per LED, one for switching it on
and one for switching it of; this was implemented with a Set/Reset latch.

Fully flexible; down-side is that we only have 3 LEDs. 
Of course, this could be extended by having another 3-to-8 decoder in e.g. the 8xxx range.
And then adding 8 Set/Reset latches. The diagram below shows a concept:

![Address decoder for GPIO](address-decode-gpio.png)

This (sub) decoder ignores line A11. So, to switch LED3 on access memory 86XX and to swicth it off access 87XX.

## 6.4. VIA
The first peripheral we add is the VIA, or 
[Versatile Interface Adapter](http://archive.6502.org/datasheets/mos_6522_preliminary_nov_1977.pdf).
This is a chip that implements two 8-bit GPIO ports, has timers and interrupts.

Since we do not know the VIA, we start small.

### 6.4.1. No RAM
How does the VIA work? The VIA has 16 registers, here we list the first four.

 | address (RS3-RS0) | register |      description                             |
 |:-----------------:|:--------:|:--------------------------------------------:|
 |     0000          |   ORB    | Output Register port B (0=low, 1=high)       |
 |     0001          |   ORA    | Output Register port A (0=low, 1=high)       |
 |     0010          |  DDRB    | Data Direction Register port B (0=in, 1=out) |
 |     0011          |  DDRA    | Data Direction Register port A (0=in, 1=out) |
 |     ..            |          |                                              |
 |     1111          |          |                                              |

So, let's connect a LED to pin PB0, which corresponds to bit 0 of port B.
This means that that we have to write a FF to DDRB to make (all pins) output.
Next, we have to write 00 and FF to ORB to have the LED blink.

``` asm
LDA #$FF # All pins output
STA $02  # DDRB

LDA #$00 # All pins low
STA $00  # ORB

LDA #$FF # All pins high
STA $00  # ORB
```

The code above is a rough sketch. 
Firstly, we used the addresses local to the chip (02 and 00).
Secondly, we would need a wait loop between the low and high, but this seem very doable.

However, we need to hook a VIA to our computer.
The problem is that the address decode we currently have can only handle
two peripherals (EEPROM and RAM).

To keep it simple, I decide to replace the RAM and add the VIA instead.
This means EEPROM is still F800-FFFF. But instead of RAM at 0000-7FFF, we now
have the VIA 0000-000F.

So the above assembler, which uses address 02 for `DDRB` 
and 00 for `ORB` matches with this memory map.

The [script](via-blinky-b0.eeprom) to program EEPROM is like the one
shown above, but then with wait (nested) loops. It was assembled
[online](https://www.masswerk.at/6502/assembler.html).

![Breadboard](eeprom-via.jpg)




