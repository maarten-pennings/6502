// RamIrq6502: use a Nano to generate a clock for the 6502 and act as a RAM (on addr/data lines).

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

// This is the 1kB RAM memory that the Nano emulates
uint8_t mem[1024];
// Note, the Arduino emulates 1k of memory (10 address lines)
// This memory block is mirrored 64 times.
// The block consists of 4 pages of 256 bytes
// Page 0 (000..0ff) is for data (zero page addressing) 
// Page 1 (100..1ff) is for the stack
// Page 2 (200..2ff) is for the program code
// Page 3 (300..3ff) is for the vectors
// Note that page 3 is mirrored at ff00..ffff
void mem_load() {
  // Fill entire memory with NOP
  for(int i=0; i<1024; i++ ) mem[i]=0xEA; // NOP

  // RAM is preloaded with a simple programm
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
  // 0205        STA *$44        85 44
  mem[0x205]= 0x85;
  mem[0x206]= 0x44;
  // 0207 LOOP   
  // 0207        INC *$33        E6 33
  mem[0x207]= 0xE6;
  mem[0x208]= 0x33;
  // 0209        JMP LOOP        4C 07 02
  mem[0x209]= 0x4C;
  mem[0x20A]= 0x07;
  mem[0x20B]= 0x02;



  // The RAM is also preloaded with an ISR
  // * = $0300
  mem[0x3fe]= 0x00;
  mem[0x3ff]= 0x03;
  // 0300        INC *$44        E6 44
  mem[0x300]= 0xE6;
  mem[0x301]= 0x44;
  // 0302        RTI             40
  mem[0x302]= 0x40;
}

// Print mem content
#define MEM_BYTESPERLINE 16
void mem_dump(int addr, int num ) {
  char buf[8];
  for(uint16_t base= addr; base<addr+num; base+=MEM_BYTESPERLINE ) {
    snprintf(buf,sizeof(buf),"%03x:",base); Serial.print(buf);
    uint16_t a= base;
    for(int i=0; i<MEM_BYTESPERLINE && a<addr+num; i++,a++ ) {
      uint8_t data= mem[a];
      snprintf(buf,sizeof(buf)," %02x",data); Serial.print(buf);
    }
    Serial.println("");
  }
}


// When the 6502 issues a read on the bus, the Nano writes 
void data_write( uint8_t data ) {
  // First configure data pins as output
  pinMode(PIN_DATA_0, OUTPUT);
  pinMode(PIN_DATA_1, OUTPUT);
  pinMode(PIN_DATA_2, OUTPUT);
  pinMode(PIN_DATA_3, OUTPUT);
  pinMode(PIN_DATA_4, OUTPUT);
  pinMode(PIN_DATA_5, OUTPUT);
  pinMode(PIN_DATA_6, OUTPUT);
  pinMode(PIN_DATA_7, OUTPUT);

  // Next, output `data` on data pins
  digitalWrite(PIN_DATA_0, (data&(1<<0))?HIGH:LOW );
  digitalWrite(PIN_DATA_1, (data&(1<<1))?HIGH:LOW );
  digitalWrite(PIN_DATA_2, (data&(1<<2))?HIGH:LOW );
  digitalWrite(PIN_DATA_3, (data&(1<<3))?HIGH:LOW );
  digitalWrite(PIN_DATA_4, (data&(1<<4))?HIGH:LOW );
  digitalWrite(PIN_DATA_5, (data&(1<<5))?HIGH:LOW );
  digitalWrite(PIN_DATA_6, (data&(1<<6))?HIGH:LOW );
  digitalWrite(PIN_DATA_7, (data&(1<<7))?HIGH:LOW );
}

// When the 6502 issues a write on the bus, the Nano reads
uint8_t data_read( void ) {
  // First configure data pins as output
  pinMode(PIN_DATA_0, INPUT);
  pinMode(PIN_DATA_1, INPUT);
  pinMode(PIN_DATA_2, INPUT);
  pinMode(PIN_DATA_3, INPUT);
  pinMode(PIN_DATA_4, INPUT);
  pinMode(PIN_DATA_5, INPUT);
  pinMode(PIN_DATA_6, INPUT);
  pinMode(PIN_DATA_7, INPUT);
  
  // Next, output `data` on data pins
  uint8_t data= 0;
  data += digitalRead(PIN_DATA_0) << 0;
  data += digitalRead(PIN_DATA_1) << 1;
  data += digitalRead(PIN_DATA_2) << 2;
  data += digitalRead(PIN_DATA_3) << 3;
  data += digitalRead(PIN_DATA_4) << 4;
  data += digitalRead(PIN_DATA_5) << 5;
  data += digitalRead(PIN_DATA_6) << 6;
  data += digitalRead(PIN_DATA_7) << 7;
  return data;
}


void setup() {
  // Configure serial port
  Serial.begin(115200);
  Serial.println();
  Serial.println("Welcome to RamIrq6502");
  
  // Setup emulated RAM
  mem_load();
  mem_dump(0x200,16);
  mem_dump(0x300,16);
  mem_dump(0x3F0,16);
  Serial.println("Memory loaded");  
  
  // Configure clock
  pinMode(PIN_CLOCK, OUTPUT);
  digitalWrite(PIN_CLOCK, HIGH);
  
  // Configure R/nW
  pinMode(PIN_RnW, INPUT);
  
  // Configure data lines
  /* skipped, done per memory transaction, see data_write, data_read */
  
  // Configure address lines
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
  uint8_t rnw=0 ;
  rnw += digitalRead(PIN_RnW) << 0;

  // Send clock high again
  digitalWrite(PIN_CLOCK, HIGH);

  // Write or read depends on R/nW
  uint8_t data;
  if( rnw ) { // R/nW==1, so 6502 reads, so Nano writes
    data= mem[addr];
    data_write(data);
  } else { // R/nW==0, so 6502 writes, so Nano reads
    data= data_read();
    mem[addr]= data;
  }

  // Print address bus
  char buf[32];
  sprintf(buf,"%9ldus %03x %0x %02x",micros(),addr,rnw,data);
  Serial.println(buf);
}
