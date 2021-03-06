// 2560shield-ctrl.ino - The simple free-running NOP loop, but SW0 is pause and SW2 is reset

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
  Serial.println("but : init");
  DDRH = 0x00; // all pins input (some have switches)
  but_scan();
  but_scan();
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
  Serial.println("led : init");
  DDRB = LED_ALL; // LED pins as output
  PORTB = 0x00; // LEDs off
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

// MAIN ===============================================

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("2560shield-ctrl");
  Serial.println();
  but_init();
  led_init();
  Serial.println();

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

  // DATA pins: instruction NOP
  DDRA = 0xFF;  // all pins output
  PORTA = 0xEA; // NOP
  
  // CLOCK pin: oscillating in loop()
  DDRC = 0x01; // pin 0 only pinMode(37, OUTPUT);
  PORTC= 0x01; // CLOCK HIGH

  // Reset
  led_on(LED_2);  
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
  led_off(LED_2);  
}

int pause=0;

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

void loop() {
  if( but_scan() ) control();
  if( !pause ) {
    char buf[16];
    PORTC= 0x00; // CLOCK LOW
    delay(50);
    PORTC= 0x01; // CLOCK HIGH
    snprintf(buf,sizeof buf,"%02X%02X %c %02X\n",PINK,PINF,PIND&0x4?'r':'W',PINA); Serial.print(buf);
    delay(50);
  }
}
