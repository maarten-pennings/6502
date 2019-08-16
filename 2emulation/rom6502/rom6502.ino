// Rom6502: use a Nano to generate a clock for the 6502 and act as a ROM (on addr/data lines).

// Connect the grounds. Supply the 6502 from the 5V0 of the Nano.
// Connect D2 of Nano to phi0 of 6502, and D3 of Nano to R/nW of 6502.
// Connect data lines D0..D7 of the 6502 to Nano D4..D11.
// Connect address lines A0..A9 of 6502 to Nano D12, D13, A0..A7.

#define PIN_CLOCK    2
#define PIN_RnW      3

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

uint8_t mem[1024];
// Note, the Arduino emulates 1k of memory (10 address lines)
// This memory block is mirrored 64 times.
// The block consists of 4 pages of 256 bytes
// Page 0 (000..0ff) is for data (zero page addressing) 
// Page 1 (100..1ff) is for the stack
// Page 2 (200..2ff) is for the program code
// Page 3 (300..3ff) is for the vectors
// Note that page 3 is mirrored at ff00..ffff
void load() {
  // Fill entire memory with NOP
  for(int i=0; i<1024; i++ ) mem[i]=0xEA; // NOP

  // https://www.masswerk.at/6502/assembler.html
  // * = $0200
  // 0200        LDX #$00        A2 00
  // 0202 LOOP:  INX             E8
  // 0203        STX $0155       8E 55 01
  // 0206        JMP LOOP:       4C 02 02

  // * = $0200
  mem[0x3fc]= 0x00;
  mem[0x3fd]= 0x02;
  // 0200        LDX #$00        A2 00
  mem[0x200]= 0xA2;
  mem[0x201]= 0x00;
  // 0202 LOOP:  INX             E8
  mem[0x202]= 0xE8;
  // 0203        STX $0155       8E 55 01
  mem[0x203]= 0x8E;
  mem[0x204]= 0x55;
  mem[0x205]= 0x01;
  // 0206        JMP LOOP:       4C 02 02
  mem[0x206]= 0x4C;
  mem[0x207]= 0x02;
  mem[0x208]= 0x02;
}

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("Welcome to Rom6502");

  load();
  
  pinMode(PIN_CLOCK, OUTPUT);
  digitalWrite(PIN_CLOCK, HIGH);

  // We do not use the data lines to spy, but to feed the 6502 with instructions
  pinMode(PIN_DATA_0, OUTPUT);
  pinMode(PIN_DATA_1, OUTPUT);
  pinMode(PIN_DATA_2, OUTPUT);
  pinMode(PIN_DATA_3, OUTPUT);
  pinMode(PIN_DATA_4, OUTPUT);
  pinMode(PIN_DATA_5, OUTPUT);
  pinMode(PIN_DATA_6, OUTPUT);
  pinMode(PIN_DATA_7, OUTPUT);

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

  // Read R/nW
  uint16_t rnw=0 ;
  rnw += digitalRead(PIN_RnW) << 0;
  
  // Write data bus
  uint16_t data= mem[addr] ;
  digitalWrite(PIN_DATA_0, (data&(1<<0))?HIGH:LOW );
  digitalWrite(PIN_DATA_1, (data&(1<<1))?HIGH:LOW );
  digitalWrite(PIN_DATA_2, (data&(1<<2))?HIGH:LOW );
  digitalWrite(PIN_DATA_3, (data&(1<<3))?HIGH:LOW );
  digitalWrite(PIN_DATA_4, (data&(1<<4))?HIGH:LOW );
  digitalWrite(PIN_DATA_5, (data&(1<<5))?HIGH:LOW );
  digitalWrite(PIN_DATA_6, (data&(1<<6))?HIGH:LOW );
  digitalWrite(PIN_DATA_7, (data&(1<<7))?HIGH:LOW );

  // Send clock high again
  digitalWrite(PIN_CLOCK, HIGH);

  // Print address bus
  char buf[32];
  sprintf(buf,"%9ldus %03x %0x %02x",micros(),addr,rnw,data);
  Serial.println(buf);
}
