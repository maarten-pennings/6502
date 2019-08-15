// AddrDataSpy6502: use a Nano to generate a clock for the 6502 and to spy its address and data lines.
// Connect the grounds. Aupply the 6502 from the 5V0 of the Nano.
// Connect D2 of Nano to phi0 of 6502, and D3 of nano to R/nW of 6502.
// Connect data lines D0..D7 of the 6502 to Nano D4..D11.
// Connect address lines A0..A9 of 6502 to Nano D12, D13, A0..A7.

#define PIN_CLOCK  2
#define PIN_RnW    3

#define PIN_DATA_0   4
#define PIN_DATA_1   5
#define PIN_DATA_2   6
#define PIN_DATA_3   7
#define PIN_DATA_4   8
#define PIN_DATA_5   9
#define PIN_DATA_6  10
#define PIN_DATA_7  11

#define PIN_ADDR_0  12
#define PIN_ADDR_1  13
#define PIN_ADDR_2  A0
#define PIN_ADDR_3  A1
#define PIN_ADDR_4  A2
#define PIN_ADDR_5  A3
#define PIN_ADDR_6  A4
#define PIN_ADDR_7  A5
#define VAL_ADDR_8 (analogRead(A6)>512)
#define VAL_ADDR_9 (analogRead(A7)>512)

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("Welcome to AddrDataSpy6502");
  
  pinMode(PIN_CLOCK, OUTPUT);
  digitalWrite(PIN_CLOCK, HIGH);
  
  pinMode(PIN_DATA_0, INPUT);
  pinMode(PIN_DATA_1, INPUT);
  pinMode(PIN_DATA_2, INPUT);
  pinMode(PIN_DATA_3, INPUT);
  pinMode(PIN_DATA_4, INPUT);
  pinMode(PIN_DATA_5, INPUT);
  pinMode(PIN_DATA_6, INPUT);
  pinMode(PIN_DATA_7, INPUT);

  pinMode(PIN_ADDR_0, INPUT);
  pinMode(PIN_ADDR_1, INPUT);
  pinMode(PIN_ADDR_2, INPUT);
  pinMode(PIN_ADDR_3, INPUT);
  pinMode(PIN_ADDR_4, INPUT);
  pinMode(PIN_ADDR_5, INPUT);
  pinMode(PIN_ADDR_6, INPUT);
  pinMode(PIN_ADDR_7, INPUT);
//pinMode(PIN_ADDR_8, INPUT);
//pinMode(PIN_ADDR_9, INPUT);
}

void loop() {
  // Send clock low
  digitalWrite(PIN_CLOCK, LOW);
  // Send clock high again
  digitalWrite(PIN_CLOCK, HIGH);
  
  // Read address bus
  uint16_t addr=0 ;
  addr += digitalRead(PIN_ADDR_0) << 0;
  addr += digitalRead(PIN_ADDR_1) << 1;
  addr += digitalRead(PIN_ADDR_2) << 2;
  addr += digitalRead(PIN_ADDR_3) << 3;
  addr += digitalRead(PIN_ADDR_4) << 4;
  addr += digitalRead(PIN_ADDR_5) << 5;
  addr += digitalRead(PIN_ADDR_6) << 6;
  addr += digitalRead(PIN_ADDR_7) << 7;
  addr +=            (VAL_ADDR_8) << 8;
  addr +=            (VAL_ADDR_9) << 9;

  // Read rnw
  uint16_t rnw=0 ;
  rnw += digitalRead(PIN_RnW) << 0;
  
  // Read data bus
  uint16_t data=0 ;
  data += digitalRead(PIN_DATA_0) << 0;
  data += digitalRead(PIN_DATA_1) << 1;
  data += digitalRead(PIN_DATA_2) << 2;
  data += digitalRead(PIN_DATA_3) << 3;
  data += digitalRead(PIN_DATA_4) << 4;
  data += digitalRead(PIN_DATA_5) << 5;
  data += digitalRead(PIN_DATA_6) << 6;
  data += digitalRead(PIN_DATA_7) << 7;

  // Print address bus
  char buf[32];
  sprintf(buf,"%9ldus %04x %0x %02x",micros(),addr,rnw,data);
  Serial.println(buf);

}
