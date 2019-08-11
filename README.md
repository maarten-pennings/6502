# 6502
Trying to build a 6502 based computer

## Introduction
I took up the challenge of building my own computer. I grew up with the [Commodore 64](https://en.wikipedia.org/wiki/Commodore_64).
It has a [6502](https://en.wikipedia.org/wiki/MOS_Technology_6502) processor. This processor was also used by Apple, Atari, Nintendo. 
It is simple, still available, and many hobbiest before me [documented](http://6502.org/) their projects. So 6502 it was.

## Hardware
I started by ordering hardware. I already had breadboards, jumper wires, resistors, LEDs. The main (first) purchase was the 6502 itself.
The [first](https://www.aliexpress.com/item/32929325067.html) variant (left) I got was an original one from MOS from 1985. 
I had problems resetting it (on power up it runs, but after a reset if freezes - is it broken?). 
Then I [learned](http://wilsonminesco.com/NMOS-CMOSdif/) that the old ones are NMOS, and that there are new CMOS versions. 
I got my [second](https://www.aliexpress.com/item/32990938828.html) variant (middle), which works well. 
A [third](https://www.aliexpress.com/item/32841499879.html) variant (right) seems to be an old one again, 
although the logo and time stamp look different from the first.

[![6502 variant 1](6502-1s.jpg)](6502-1.png) [![6502 variant 2](6502-2s.jpg)](6502-2.png) [![6502 variant 3](6502-3s.jpg)](6502-3.png)

## Clock
We need a clock, a source of pulses. There are several options. Remember that a classical 6502 runs at 1MHz. 
It seems that not match variantion is allowed in the clock frequency for the old variants.
It seems that the new variants allow much more variation. 
Anyhow, I don't need speed (that only causes all kinds of electrical problems).
I prefer slow, as in hand clocked, to see in detail what is going on.

The hardest way is to [build](https://www.grappendorf.net/projects/6502-home-computer/clock-generation.html) your own oscillator 
based on a [crystal](https://www.aliexpress.com/item/32869213435.html). Tried. Works. Do not recommend.

Much easier is to get a ["can"](https://www.aliexpress.com/item/32887401548.html) that presumably contains the crystal
and the passives around it. The 1MHz versions are a bit hard to get. Works well. Do recommend.

The third way is more the software approach: use an Arduino Nano to generate the clock. This is especially nice at the start 
of your project. You might already have it laying around. You do not need to order any other special components, and it gives 
you a nice road to other experiments: let the Nano spy on the address bus, or even spoof the databus!

