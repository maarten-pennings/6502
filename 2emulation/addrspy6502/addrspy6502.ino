// AddrSpy6502: use a Nano to generate a clock for the 6502 and to spy its address lines
// Connect D2 of Nano to phi0 of 6502. Also connect the grounds. Aupply the 6502 from the 5V0 of the Nano.
// Connect address lines: 6502 A0..A9 to Nano D4..D13, 6502 A10..A15 to Nano A0..A5

#define PIN_CLOCK  2

#define PIN_ADDR_0   4
#define PIN_ADDR_1   5
#define PIN_ADDR_2   6
#define PIN_ADDR_3   7
#define PIN_ADDR_4   8
#define PIN_ADDR_5   9
#define PIN_ADDR_6  10
#define PIN_ADDR_7  11
#define PIN_ADDR_8  12
#define PIN_ADDR_9  13
#define PIN_ADDR_10 A0
#define PIN_ADDR_11 A1
#define PIN_ADDR_12 A2
#define PIN_ADDR_13 A3
#define PIN_ADDR_14 A4
#define PIN_ADDR_15 A5

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("Welcome to AddrSpy6502");
  
  pinMode(PIN_CLOCK, OUTPUT);
  digitalWrite(PIN_CLOCK, HIGH);
  
  pinMode(PIN_ADDR_0, INPUT);
  pinMode(PIN_ADDR_1, INPUT);
  pinMode(PIN_ADDR_2, INPUT);
  pinMode(PIN_ADDR_3, INPUT);
  pinMode(PIN_ADDR_4, INPUT);
  pinMode(PIN_ADDR_5, INPUT);
  pinMode(PIN_ADDR_6, INPUT);
  pinMode(PIN_ADDR_7, INPUT);
  pinMode(PIN_ADDR_8, INPUT);
  pinMode(PIN_ADDR_9, INPUT);
  pinMode(PIN_ADDR_10, INPUT);
  pinMode(PIN_ADDR_11, INPUT);
  pinMode(PIN_ADDR_12, INPUT);
  pinMode(PIN_ADDR_13, INPUT);
  pinMode(PIN_ADDR_14, INPUT);
  pinMode(PIN_ADDR_15, INPUT);
}

void loop() {
  // Send clock low
  digitalWrite(PIN_CLOCK, LOW);
  // Send clock high again
  digitalWrite(PIN_CLOCK, HIGH);
  
  // Read address bus
  uint16_t addr=0 ;
  addr += digitalRead(PIN_ADDR_0 ) << 0;
  addr += digitalRead(PIN_ADDR_1 ) << 1;
  addr += digitalRead(PIN_ADDR_2 ) << 2;
  addr += digitalRead(PIN_ADDR_3 ) << 3;
  addr += digitalRead(PIN_ADDR_4 ) << 4;
  addr += digitalRead(PIN_ADDR_5 ) << 5;
  addr += digitalRead(PIN_ADDR_6 ) << 6;
  addr += digitalRead(PIN_ADDR_7 ) << 7;
  addr += digitalRead(PIN_ADDR_8 ) << 8;
  addr += digitalRead(PIN_ADDR_9 ) << 9;
  addr += digitalRead(PIN_ADDR_10) << 10;
  addr += digitalRead(PIN_ADDR_11) << 11;
  addr += digitalRead(PIN_ADDR_12) << 12;
  addr += digitalRead(PIN_ADDR_13) << 13;
  addr += digitalRead(PIN_ADDR_14) << 14;
  addr += digitalRead(PIN_ADDR_15) << 15;

  // Print address bus
  char buf[32];
  sprintf(buf,"%9ldus %04x",micros(),addr);
  Serial.println(buf);

}
