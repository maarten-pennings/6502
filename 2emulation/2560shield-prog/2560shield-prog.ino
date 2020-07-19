// 2560shield-prog.ino - Full command driven 6502 shield

#include "isa.h"
#include "cmd.h"
#include "cmdman.h"
#include "cmdread.h"
#include "cmdwrite.h"
#include "cmddasm.h"
#include "cmdasm.h"
#include "cmdprog.h"

// Pins on the MEGA256 and where they are connected to on the 6502.
// Bare number is digital pin, so 15 means D15.
//   Port A: 22,23,24,25,26,27,28,29        DATA (7=MD7..0=MD0)
//   Port B: 10,11,12                       LED (not to 6502: 6=LED2 5=LED1 4=LED0)
//   Port C: 37                             CLOCK (0=MPHI0)
//   Port D: 18,19,20,21, 38                OUT (7=MO4NCML 3=MO3PHI2 2=MO2RW 1=MO1SYNC 0=MO0PHI1)
//   Port E: 0,1                            UART (not to 6502: 1=TX 0=RX)
//   Port F: A0,A1,A2,A3,A4,A5,A6,A7        ADDR (7=MA7..0=MA0)
//   Port G: 40,41                          EXTRA (not to 6502: 1=MD40 0=MD41)
//   Port H: 6,7,8                          BUT (not to 6502: 5=SW2 4=SW1 3=SW0)
//   Port J: 15                             POW (0=MVCC)
//   Port K: A8,A9,A10,A11,A12,A13,A14,A15  ADDR (7=MA15..0=MA8)
//   Port L: 42,43,44,45,46,47              INP (7=MI5NCBE 6=MI4NMI 5=MI3IRQ 4=MI2RDY 3=MI1SO 2=MI0RES)

// BUTTONS =======================================
//   Port H: 6,7,8                          BUT (not to 6502: 5=SW2 4=SW1 3=SW0)

#define BUT_0 (1<<3)
#define BUT_1 (1<<4)
#define BUT_2 (1<<5)
#define BUT_ALL (BUT_0|BUT_1|BUT_2)

int but_cur, but_prev;
int but_scan() {
  but_prev= but_cur;
  but_cur= PINH & BUT_ALL;
  return but_cur!=but_prev;
}

void but_init() {
  DDRH = 0x00; // all pins input (some have switches)
  but_scan();
  but_scan();
  Serial.println("but : init");
}

int but_wasdown(int buts) {
  return but_prev & buts;  
}

int but_isdown(int buts) {
  return but_cur & buts;  
}

int but_wentdown(int buts) {
  return but_isdown(buts) & ~but_wasdown(buts);  
}

int but_wentup(int buts) {
  return ~but_isdown(buts) & but_wasdown(buts);  
}

// LEDS =======================================
//   Port B: 10,11,12                       LED (not to 6502: 6=LED2 5=LED1 4=LED0)

#define LED_0 (1<<4)
#define LED_1 (1<<5)
#define LED_2 (1<<6)
#define LED_ALL (LED_0|LED_1|LED_2)

void led_init() {
  DDRB = LED_ALL; // LED pins as output
  PORTB = 0x00; // LEDs off
  Serial.println("led : init");
}

void led_on(int leds) {
  PORTB |= leds;
}

void led_off(int leds) {
  PORTB &= ~ leds;
}

int led_get(int leds) {
  return PORTB & leds & LED_ALL;
}

void led_tgl(int leds) {
  PORTB = PORTB ^ leds;
}

// MEM ================================================

#define MEM_SIZE 1024
const uint16_t mem_size= MEM_SIZE;
uint8_t mem[MEM_SIZE];

void mem_init() {
  // Fill entire memory with NOP
  for(uint16_t addr=0; addr<MEM_SIZE; addr++ ) mem[addr]=0xEA; // NOP

  // RAM is preloaded with a simple program: after zeroing 0033 and 0022, it steps 0033 in a loop
  //   https://www.masswerk.at/6502/assembler.html
  // * = $0200
  mem[0x3fc]= 0x00;
  mem[0x3fd]= 0x02;

  // 0200        CLI             58
  mem[0x200]= 0x58;
  // 0201        LDA #$00        A9 00
  mem[0x201]= 0xA9;
  mem[0x202]= 0x00;
  // 0203        STA *$33        85 33
  mem[0x203]= 0x85;
  mem[0x204]= 0x33;
  // 0205        STA *$22        85 22
  mem[0x205]= 0x85;
  mem[0x206]= 0x22;
  // 0207 LOOP   
  // 0207        INC *$33        E6 33
  mem[0x207]= 0xE6;
  mem[0x208]= 0x33;
  // 0209        JMP LOOP        4C 07 02
  mem[0x209]= 0x4C;
  mem[0x20A]= 0x07;
  mem[0x20B]= 0x02;

  // The RAM is also preloaded with an ISR: it steps 0022
  // * = $0300
  mem[0x3fe]= 0x00;
  mem[0x3ff]= 0x03;

  // 0300        INC *$22        E6 22
  mem[0x300]= 0xE6;
  mem[0x301]= 0x22;
  // 0302        RTI             40
  mem[0x302]= 0x40;

  Serial.println("mem : init");
}

void mem_write(uint16_t addr, uint8_t data) {
  mem[ addr % MEM_SIZE ] = data;  
}

uint8_t  mem_read(uint16_t addr) {
  return mem[ addr % MEM_SIZE ];  
}

// MAIN ===============================================

void pins6502_init() {
  // ADDR pins: input
  DDRF = 0x00; // all pins input (LSB)
  DDRK = 0x00; // all pins input (MSB)

  // OUT pins: input
  DDRD = 0x00; // all pins input (only 7,3,2,1,0 in use)
  
  // POW pin: output and on
  DDRJ = 0x01;
  PORTJ = 0x00; // low active

  // INP pins: output and pulled high
  DDRL = 0xFC; // all pins output (except lower 1,0: not in use)
  PORTL = 0xFC; // all pulled high

  // DATA pins: input by default
  DDRA = 0x00;  // all pins input
  
  // CLOCK pin: oscillating in loop()
  DDRC = 0x01; // pin 0 only pinMode(37, OUTPUT);
  PORTC= 0x01; // CLOCK HIGH
}

void reset6502() {
  PORTL = 0xF8;
  PORTC= 0x00; // CLOCK LOW
  delay(10);
  PORTC= 0x01; // CLOCK HIGH
  delay(10);
  PORTC= 0x00; // CLOCK LOW
  delay(10);
  PORTC= 0x01; // CLOCK HIGH
  delay(10);
  PORTL = 0xFC;
}

void cmd_init() {
  cmd_begin();
  // Register in alphabetical order
  cmdasm_register();  
  cmddasm_register();  
  cmdecho_register();
  cmdhelp_register();
  cmdman_register();
  cmdprog_register();
  cmdread_register(); 
  cmdwrite_register();
  
}

void banner() {
  // http://patorjk.com/software/taag/#p=display&f=Big&t=isa6502                     
  Serial.println( );
  Serial.println( F("                         __ _____  ___ ___  ") );
  Serial.println( F("                        / /| ____|/ _ \\__ \\ ") );
  Serial.println( F(" _ __  _ __ ___   __ _ / /_| |__ | | | | ) |") );
  Serial.println( F("| '_ \\| '__/ _ \\ / _` |  _ \\___ \\| | | |/ / ") );
  Serial.println( F("| |_) | | | (_) | (_| | (_) |__) | |_| / /_ ") );
  Serial.println( F("|  __/|_|  \\___/ \\__, |\\___/____/ \\___/____|") );
  Serial.println( F("| |               __/ |                     ") );
  Serial.println( F("|_|              |___/                      ") );
  Serial.println();
  Serial.print( F("Welcome to isa6502prog, using is6502 lib V") ); Serial.println(ISA_VERSION);
  Serial.println( );
  Serial.println( F("Type 'help' for help") );
}


#define SPEED_PERIOD_MS 100
uint32_t speed_last;
int pause;

void control() {
  if( but_wentdown(BUT_0) ) {
    pause = !pause;
    if( pause ) {
      Serial.println("but : pause");
      led_on( LED_0 ); 
    } else {
      led_off( LED_0 );
      Serial.println("but : continue");
    }
  }
  
  if( but_wentdown(BUT_1) ) {
    led_on( LED_1 );
    PORTL = PORTL & ~0x20;
    Serial.println("but : IRQ asserted");
  }
  if( but_wentup(BUT_1) ) {
    PORTL = PORTL | 0x20;
    led_off( LED_1 );
    Serial.println("but : IRQ released");
  }
  
  if( but_wentdown(BUT_2) ) {
    led_on( LED_2 );
    PORTL = PORTL & ~0x04;
    Serial.println("but : RST asserted");
  }
  if( but_wentup(BUT_2) ) {
    PORTL = PORTL | 0x04;
    led_off( LED_2 );
    Serial.println("but : RST released");
  }
}

void setup() {
  Serial.begin(115200);
  banner();
  led_init();
  but_init();
  pins6502_init();
  mem_init();
  cmd_init();
  Serial.println();
  
  speed_last= millis();
  pause= 0;
  reset6502();
}

void loop() {
  // Are buttons pressed?
  if( but_scan() ) control();

  // Feed command interpreter with chars from serial, if there are any
  cmd_pollserial(); 

  // Give the 6502 clock ticks
  if( !pause && millis()-speed_last>SPEED_PERIOD_MS) {
    char buf[16];
    PORTC= 0x00; // CLOCK LOW
    delayMicroseconds(2); // Wait a bit - spec says 125ns ... but 1us is not enough
    uint16_t addr= PINK*256 + PINF;
    uint8_t rnw= (PIND&0x4)!=0;
    PORTC= 0x01; // CLOCK HIGH
    uint8_t data;
    if( rnw ) { // "r" case: R/nW==1, so 6502 reads, so Mega writes
      data= mem_read(addr);
      DDRA= 0xFF; // output
      delayMicroseconds(2); 
      PORTA= data;
    } else { // "W" case: R/nW==0, so 6502 writes, so Mega reads
      DDRA= 0x00; // input
      delayMicroseconds(2); 
      data= PINA;
      mem_write(addr, data);
    }
    snprintf(buf,sizeof buf,"%04X %c %02X\n",addr,rnw?'r':'W',data); Serial.print(buf);
    speed_last= millis();
  }
  
}
