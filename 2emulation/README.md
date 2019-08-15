# Emulation
Trying to build a 6502 based computer.

We have (see [previous chapter](..\1clock)) a 6502 in free run: it exceutes NOP, NOP, NOP, ... and by inspecting
the address lines (either having LEDs on the high lines, or using the scope on the low lines) we observe the addresses change.

But are we sure the 6502 is doing what we expect? Is it really starting at address EAEA (the reset vector at FFFC and FFFD also 
reads EA and EA) and then progressing to EAEB, EAEC? Would be nice if we can chack that. 

Then I had a nice idea: the Nano controls the clock, so it knows when the address bus is valid, can't the Nano snoop it?
Can't we snoop the data bus as well? Can't we write the data bus? Yes, yes, and yes.

## Clock

The [first step](https://github.com/maarten-pennings/6502/tree/master/1clock#clock---nano---wiring) was taken in the 
previous chapter. We used a Nano a clock source for the 6502. Address lines are dangling, data lines are hardwired to EA 
(the code of the NOP instruction). We had a very simple sketch that flips the clock line, and behold we had a 6502 running at 160kHz.

## Addres bus

With the first experiment, we believe the 6502 is executing NOPs. Can we check that? Can we check that it starts at EAEA 
(the reset vector at FFFC and FFFD also reads EA and EA) and then progressing to EAEB, EAEC? Can we spy the address bus?

Since the Nano generates the clock, we know when we have to sample the address bus.
Does the Nano have enough inputs? It seems to have D0..D13 so 14 lines.
However D0 and D1 [double](https://www.theengineeringprojects.com/wp-content/uploads/2018/06/introduction-to-arduino-nano-13-1.png) 
as RXD and TXD and we need to to send the trace to the PC (over USB). That's 12 lines, and 16 would be nice.
But from the [pinout](https://www.theengineeringprojects.com/wp-content/uploads/2018/06/introduction-to-arduino-nano-13-1.png)
we see that analogue pins A0..A5 also have a double role, they can act as digital pins. That's 6 more digital pins.

In other words, we have 18 digital pins. We need 1 for clock and 16 far the address lines. Even one spare.

![Nano spys on the address lines](nano-address.png)

The program on the Nano, [AddrSpy6502](addrspy652) is simple. The `loop()` pulses the clock, 
reads all 16 address lines and prints them out in hex (with a time stamp).

This is the experiment I did.
- I pressed the reset button of the Nano down, and with the same hand I pressed the reset button of the 6502 down.
- I cleared the output of the Arduino terminal
- I released the reset of the Nano.
- As soon as I saw the first output of the Nano, I released the reset of the 6502.
- I disabled autoscroll and copied the trace.

This was the trace (I manually added the `<- reset released`)

```
Welcome to AddrSpy6502
      400us 3586
      828us 3586
     1264us 3586
     1704us 3586
     2928us 3586
     4460us 3586
     5988us 3586
     7520us 3586
        ...  
   406848us 3586
   408380us 3586
   409908us 3586 <- reset released
   411440us 3586
   412968us 01ee
   414500us 01ed
   416028us 01ec
   417556us fffc
   419088us fffd
   420620us eaea
   422148us eaeb
   423680us eaeb
   425208us eaec
   426740us eaec
   428268us eaed
   429800us eaed
   431328us eaee
   432860us eaee
   434388us eaef
   435916us eaef
   437448us eaf0
   438980us eaf0
   440508us eaf1

```

Note that the first ~400 000 us of the Nano trace, the 6502 was still in reset.
It had a random pattern on the address bus, in this case 3586 (hex).
Just before 409908us I released the reset of the 6502.

What we see on the address bus can be found in the 6502 datasheet, but is also very well explained
on the [6502.org site](http://www.6502.org/tutorials/interrupts.html#1.3):

```
                 // 6502 reset released
   409908us 3586 // first internal administrative operation of 6502
   411440us 3586 // second internal operation
   412968us 01ee // push of return address (PCH) on stack, decrement stack pointer (note S is 01EE)
   414500us 01ed // push the return address (PCL) on stack, decrement stack pointer (note S is 01ED)
   416028us 01ec // push the processor status register (P) on stack, decrement stack pointer (note S is 01EC)
   417556us fffc // get PCL from reset vector (FFFC)
   419088us fffd // get PCH from reset vector (FFFD)
   420620us eaea // Jump to reset vector. Executes first instruction (NOP)
   422148us eaeb // Executes 2nd instruction (NOP)
   423680us eaeb // Executes 2nd instruction (NOP)
   425208us eaec // Executes 3rd instruction (NOP)
   426740us eaec // Executes 3rd instruction (NOP)
```

Some notes
 - All three interrupts, NMI with vector at FFFA, RESET with vector at FFFC and IRQ with vector at FFFE
   have the same 7-clock interrupt sequence. 
 - One exception: for RESET the three pushes are fake: the 6502 issues a read to the memory instead of a write
 - The stackpointer (S) has a random value after reset, in the above run it happened to be 1EE.
 - A NOP is two cycles, and we see that after RESET the address bus indeed changes every other step.
 - I can not explain why the first NOP only takes one clock

---

To be written (2019 aug 12)

 - Clock, capture 16bit addresses, NOPs, reset
 - NOPs becomes JMP in hw (or)
 - Now also capture data (use JMP prog)
 - Now also emulate rom (loop with inx and stx)
 - Add irq and isr
 - Now also support data write
