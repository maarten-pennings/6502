# Emulation
Trying to build a 6502 based computer.

We have (see [previous chapter](..\1clock)) a 6502 in free run: it executes NOP, NOP, NOP, ... and by inspecting
the address lines (either having LEDs on the high lines, or using the scope on the low lines) we observe the addresses change.

But are we sure the 6502 is doing what we expect? Is it really starting at address EAEA (the reset vector at FFFC and FFFD also 
reads EA and EA) and then progressing to EAEB, EAEC? Would be nice if we can check that. 

Then I had a nice idea: the Nano controls the clock, so it knows when the address bus is valid, can't the Nano snoop it?
Can't we snoop (read) the data bus as well? Can't we _write_ the data bus? Yes, yes, and yes.

## Clock

The [first step](https://github.com/maarten-pennings/6502/tree/master/1clock#clock---nano---wiring) was taken in the 
previous chapter. We used a Nano as a clock source for the 6502. The address lines are dangling, the data lines are hardwired to EA 
(the opcode of the NOP instruction). We had a very simple sketch that flips the clock line, and behold we had a 6502 
"free" running at 160kHz.

## Address bus

With the first experiment, we believe the 6502 is executing NOPs. Can we check that? Can we check that it starts at EAEA 
(the reset vector at FFFC and FFFD also reads EA and EA) and then progressing to EAEB, EAEC? Can we spy the address bus?

Since the Nano generates the clock, we know when we have to sample the address bus.
Does the Nano have enough inputs? It seems to have D0..D13 so 14 lines.
However D0 and D1 [double](https://www.theengineeringprojects.com/wp-content/uploads/2018/06/introduction-to-arduino-nano-13-1.png) 
as RXD and TXD and we need those two pins to send the trace to the PC (over USB). That leaves us with 12 lines, where 16 would be nice.
From the [pinout](https://www.theengineeringprojects.com/wp-content/uploads/2018/06/introduction-to-arduino-nano-13-1.png)
we see that analog pins A0..A5 also have a double role, they can act as digital pins. That's 6 more digital pins. 

In other words, we have 18 digital pins. We need 1 for clock and 16 far the address lines. Even one spare.

![Schematic of Nano spying on the address lines](nano-address.png)

The schematics above looks like this on my breadboard:

![Board of Nano spying on the address lines](nano-address.jpg)

The program on the Nano, [AddrSpy6502](addrspy6502) is simple. The `loop()` pulses the clock, 
reads all 16 address lines and prints them out in hex (with a time stamp).
```
void loop() {
  // Send clock low
  digitalWrite(PIN_CLOCK, LOW);
  // Send clock high again
  digitalWrite(PIN_CLOCK, HIGH);
  
  // Read address bus
  uint16_t addr=0 ;
  addr += digitalRead(PIN_ADDR_0 ) << 0;
  addr += digitalRead(PIN_ADDR_1 ) << 1;
  ...
  addr += digitalRead(PIN_ADDR_15) << 15;

  // Print address bus
  char buf[32];
  sprintf(buf,"%9ldus %04x",micros(),addr);
  Serial.println(buf);
}
```

This is the experiment I did.
- I pressed the reset button of the Nano down, and with the same hand I pressed the reset button of the 6502 down.
- I cleared the output of the Arduino terminal
- I released the reset of the Nano.
- As soon as I saw the first output of the Nano, I released the reset of the 6502.
- I disabled autoscroll in the terminal and copied the trace.

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
Just before 409908us I released the reset of the 6502 (you can't see that, it's my guess).

What we see on the address bus can be found in the 6502 datasheet, but is also very well explained
on the [6502.org site](http://www.6502.org/tutorials/interrupts.html#1.3):

```
                 // 6502 reset released
   409908us 3586 // first internal administrative operation of 6502
   411440us 3586 // second internal operation
   412968us 01ee // push of return address (PCH) on stack, decrement stack pointer (note S is EE)
   414500us 01ed // push the return address (PCL) on stack, decrement stack pointer (note S is ED)
   416028us 01ec // push the processor status register (P) on stack, decrement stack pointer (note S is EC)
   417556us fffc // get PCL from reset vector (FFFC), presumably reads EA
   419088us fffd // get PCH from reset vector (FFFD), presumably reads EA
   420620us eaea // Jump to reset vector, indeed EAEA. Executes first instruction (NOP)
   422148us eaeb // Executes 2nd instruction (NOP)
   423680us eaeb // Executes 2nd instruction (NOP)
   425208us eaec // Executes 3rd instruction (NOP)
   426740us eaec // Executes 3rd instruction (NOP)
```

Some notes
 - All three interrupts, NMI with vector at FFFA, RESET with vector at FFFC and IRQ with vector at FFFE
   have the same 7-clock interrupt sequence. 
 - One exception: for RESET the three pushes are fake: the 6502 issues a _read_ to the memory instead of a _write_.
 - The stack pointer (S) has a random value after reset, in the above run it happened to be EE.
   The stack page is hardwired to 01 on the 6502.
 - A NOP is two cycles, and we see that after RESET the address bus indeed changes every other step.
 - I can not explain why the first NOP only takes one clock.
 - The time between the trace lines (one clock period) is about 1500us, so we are running at 0.7kHz


## Jump loop

The previous experiment is a success: we see the address lines increment nicely (in steps of two)
and also the reset behavior is as documented. Still, it would be nice to have a more realistic program;
a series of NOPs is not very convincing.

However, without a memory to store our program, we are limited in our possibilities.
There is one way out: go old style. Write a program in _hardware_.

I got the idea from [James Calvert's tight loop](http://mysite.du.edu/~jcalvert/tech/6504.htm).
We make some logic that emulates an 8 byte program.
We NOR together the first three address lines to create the "v-signal".
The v-signal is bound to D2, D3 and D6, the other data lines (D0, D1, D4, D5 and D7) are bound to GND.
So, the data bus is 0b 0v00 vv00. This means that 
 - if v=0 then D = 0b 0000 0000 = 0x00
 - if v=1 then D = 0b 0100 1100 = 0x4C

For the various addresses, that gives the following data reads:

  | address (hex) | address (bin) | v-signal| data |
  |:-------------:|:-------------:|:-------:|:----:|
  |     ...       |       ...     |         |      |
  |     FFFC      |    ... 100    |    0    |  00  | 
  |     FFFD      |    ... 101    |    0    |  00  |
  |     FFFE      |    ... 110    |    0    |  00  |
  |     FFFF      |    ... 111    |    0    |  00  |
  |     0000      |    ... 000    |    1    |  4C  |
  |     0001      |    ... 001    |    0    |  00  |
  |     0002      |    ... 010    |    0    |  00  |
  |     0003      |    ... 011    |    0    |  00  |
  |     ...       |       ...     |         |      |

In other words, at FFFC, the 6502 reads the start address 00 00, and at 0000 the 6502 reads 4C 00 00.
Note that 4C 00 00 means JMP 0000, since 4C is the opcode for JMP abs.

This the schematic

![nano-jmp.png](nano-jmp.png)

Here is a photo of my board. Note the _triple OR_ (4075) and the _hex inverter_ (7404) chips on the right.
Also note the three blue wires coming in (from A0, A1, A2) and the blue wire going out (to D2, d3 and D6).

![nano-jmp.jpg](nano-jmp.jpg)

With the same [sketch](addrspy6502) as the previous experiment, let's make a trace again.
We repeat our manual steps: Nano reset down, 6502 reset down, Nano reset up, wait for output, 6502 reset up.
This is the trace (I manually added the `<- reset released`) 

```
Welcome to AddrSpy6502
      400us 0001
      820us 0001
     1248us 0001
     1676us 0001
        ...
   263028us 0001
   264560us 0001
   266088us 0001
   267620us 0001
   269148us 0001
   270680us 0001 <- reset released
   272208us 0001
   273740us 01fa
   275268us 01f9
   276800us 01f8
   278328us fffc
   279856us fffd
   281388us 0000
   282920us 0001
   284448us 0002
   285980us 0000
   287508us 0001
   289040us 0002
   290568us 0000
   292100us 0001
   293628us 0002
```

Some notes
 - Before 270680us the reset is released (can't see that from the trace). 
 - We see the two internal administrative operations
 - We see the (fake) push of PCH, PCL, P.
   The stack pointer S now starts at FA, so pushes to 01FA, 01F9 and 01F8.
 - We see the reset vector load (FFFC, FFFD)
 - We see the load of 0000 (this confirms that FFFC and FFFD read 00 00)
 - We see the load of 0001, 0002
 - We see the load of 0000 again, which hints that JMP 0000 was executed
 - One clock is still about 1500us (0.7kHz)


## Data bus

In the notes on the previous experiment, we have "load of 0000 again, which hints that JMP 0000 was executed".
Wouldn't it be nice if we could not only see the address bus, but also the data bus?
We can, but we loose details on the address bus.

Recall that we have Nano D2 for the 6502 ϕ0 (clock), and Nano D4..D13 plus A0..A5 for 6502 A0..A15.
This leaves Nano D3, A6 and A7 free.
Let's redesign.

 - Connect Nano D3 to R6502 R/nW so that we can trace if the 6502 did a read or a write on the data bus.
 - Use Nano D4..D11 for 6502 data bus D0..D7. Full data trace.
 - Use Nano D12, D13, A0..A7 for 6502 A0..A9. Thus 10 bit address trace.

There is one problem: Nano A6 and A7 are analog only.
We cannot use `digitalRead()` on those pins.
We can however read them in an analog fashion by using `analogRead()` and comparing the result with half the 
maximum analog readout: 1024/2.
The drawback is that `analogRead()` is [slow](https://www.arduino.cc/reference/en/language/functions/analog-io/analogread/):

> On ATmega based boards (UNO, Nano, Mini, Mega), it takes about 100 microseconds (0.0001 s) 
> to read an analog input, so the maximum reading rate is about 10,000 times a second.

We prefer to use 10 bits, because this means we have 4 pages (an 6502 page is 256 bytes):
 - page 0, for well, zero-page addressing of the 6502
 - page 1, for the stack
 - page 2, for the code
 - page 3, for the interrupt vectors


```
Welcome to AddrDataSpy6502
      752us 0002 1 00
     1516us 0002 1 00
     2284us 0002 1 00
     3256us 0002 1 00
     5208us 0002 1 00
        ...
   276952us 0002 1 00
   278896us 0002 1 00
   280848us 0002 1 00
   282816us 0002 1 00
   284752us 0002 1 00
   286736us 0002 1 00
   288680us 0002 1 00
   290624us 01fd 1 00
   292592us 01fc 1 00
   294536us 01fb 1 00
   296488us 03fc 1 00
   298456us 03fd 1 00
   300400us 0000 1 4c
   302368us 0001 1 00
   304320us 0002 1 00
   306264us 0000 1 4c
   308240us 0001 1 00
   310176us 0002 1 00
   312128us 0000 1 4c
   314096us 0001 1 00
   316032us 0002 1 00
```

 - The leading nibble of the address is always 0 (we only capture 10 bits).
 - The second nible of the address bus is only 0, 1, 2 or 3 (we only capture 2 bits in this nibble).
 - All bytes are read as 00 except at location 0000, there it reads 4C.
 - The three push instructions (01fd, 01fc, 01fb) are indeed fake: they read instead of write (R/nW flag is 1).
 - One clock is now about 2000us (0.5kHz).



## Emulate ROM

 - Now also emulate rom (loop with inx and stx)
 - Add irq and isr

## Emulate RAM

 - Now also support data write

