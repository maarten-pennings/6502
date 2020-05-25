// Arduino EEPROM Programmer (for the AT28C16 and AT28C64)
// Features a complete command interpreter
//   See https://github.com/maarten-pennings/6502/tree/master/3eeprom


// Todo:
// - command 'opt size <size> offset <offset>'
// - when sel pressed: flash nibble of address bits


#define PROG_NAME    "Arduino EEPROM Programmer"
#define PROG_VERSION "12"
#define PROG_DATE    "2020 may 25"
#define PROG_AUTHOR  "Maarten Pennings"


// Mega328 ==============================================================
// Get the VCC the Arduino Nano itself runs on (using built-in ref)
// Since is also powers the EEPROM, this should not be too low

long mega328_readVcc(void) { 
  // From: https://code.google.com/archive/p/tinkerit/wikis/SecretVoltmeter.wiki
  long result; // Read 1.1V reference against AVcc 
  ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1); 
  delay(2); // Wait for Vref to settle 
  ADCSRA |= _BV(ADSC); // Convert 
  while( bit_is_set(ADCSRA,ADSC) ); 
  result = ADCL; 
  result |= ADCH<<8; 
  result = 1126400L / result; 
  // Back-calculate AVcc in mV 
  return result;
}

#define MEGA328_AVERAGE_COUNT 32
long mega328_Vcc(void) { 
  long result=0; 
  for( int i=0; i<MEGA328_AVERAGE_COUNT; i++ ) result+=mega328_readVcc();
  return (result+MEGA328_AVERAGE_COUNT/2)/MEGA328_AVERAGE_COUNT;
}


// EEPROM ===============================================================
// Driver for the eeprom (and the shift registers and the MUX)

// This sketch supports two types of shift ICs: 74HTC164 and 74HC595 (the latter has less flicker).
// 74HTC164 (two ICs: Q7 of the first is chained to DSA and DSB of the second)
//   The 74HTC164's are controlled by a Nano: EEPROM_PIN_DATA to DSA and DSB and EEPROM_PIN_CLK to CP.
//   The Q0-Q7 (1st IC) and the Q0-Q2 (2nd IC) control A0-A10 of the EEPROM.
//   MR to VCC
// 74HC595 (two ICs: Q7S of the first is chained to DS of the second)
//   The 74HC595's are controlled by the Nane: EEPROM_PIN_DATA to DS, EEPROM_PIN_CLK to SHCP, EEPROM_PIN_LATCH to STCP
//   The Q0-Q7 (1st IC) and the Q0-Q2 (2nd IC) control A0-A10 of the EEPROM.
//   nOE is tied to GND, nMR is tied to VCC
// Note that the 74HC595 has one extra control line EEPROM_PIN_LATCH which, when strobed, latches all values.

// These Nano pins control the data pins of the EEPROM
#define EEPROM_PIN_D0     2
#define EEPROM_PIN_D1     3
#define EEPROM_PIN_D2     4
#define EEPROM_PIN_D3     5
#define EEPROM_PIN_D4     6
#define EEPROM_PIN_D5     7
#define EEPROM_PIN_D6     8
#define EEPROM_PIN_D7     9
// These Nano pins control the (low-active) OE (output=read enable) and WE (write enable) of the EEPROM
#define EEPROM_PIN_nOE   10
#define EEPROM_PIN_nWE   11
// These Nano pins control the shift registers
#define EEPROM_PIN_DATA  12
#define EEPROM_PIN_CLK   13
#define EEPROM_PIN_LATCH A0 // comment out when using 74HTC164 (or leave it defined: nothing will happen since pin EEPROM_PIN_LATCH is unconnected, and 164 outputs without latch pulse)
// These Nano pins control other EEPROM signals
#define EEPROM_PIN_16n64 A1 // Control the MUX for pin23 (see drawing below)
#define EEPROM_PIN_nCE   A2
#define EEPROM_PIN_RDY   A6 // not used currently


// This sketch supports two types of EEPROMS: 28C16 (2kB) and 28C64 (8kB).
// All pins of the 28C16 have identical function on the 28C64 (when the GND pins are aligned), 
// with two exceptions (boxed in the diagram below):
//                 +---+       +---+
//           28  27| 26| 25  24| 23| 22  21  20  19  18  17  16  15
//   28C64: VIN nWE| NC| A8  A9|A11|nOE A10 nCE  D7  D6  D5  D4  D3
//   28C16:        |VIN| A8  A9|nWE|nOE A10 nCE  D7  D6  D5  D4  D3
//                 +---+       +---+
//   28C16:          A7  A6  A%  A4  A3  A2  A1  A0  D0  D1  D2 GND
//   28C64: RDY A12  A7  A6  A%  A4  A3  A2  A1  A0  D0  D1  D2 GND
//           1   2   3   4   5   6   7   8   9   10  11  12  13  14 
//
// Since on the 28C64, pin 26 is 'Not Connected' it doesn't harm to put VIN there.
// Of course, we have VIN on 28, nWE on 27 and A12 on 2 (and we let output pin 1 dangle). 
// This does not impact 28C16.
// The biggest problem is pin 23. For 28C16 this needs to be nWE and for 28C64 this needs to be A11.
// Therefore, we have a MUX, controlled by EEPROM_PIN_16n64 to select between the two functions.
// The I0 of the MUX is connected to A11, the I1 of the MUX to nWE; Q to pin 23 of the EEPROM.
// This means that a 0 on EEPROM_PIN_16n64 selects A11 (28C64) and a 1 selects nWE (28C16).

// Implementation detail: represent eeprom type with its size
#define EEPROM_TYPE_NONE  0x0000U
#define EEPROM_TYPE_28C16 0x0800U
#define EEPROM_TYPE_28C64 0x2000U

unsigned int eeprom_type;

// Sets the eeprom type to `type` (which should be either EEPROM_TYPE_28C16 or EEPROM_TYPE_28C64)
void eeprom_type_set( int type ) {
  if( type==EEPROM_TYPE_28C16 ) {
    // On MUX, select 28C16, i.e. nWE 
    digitalWrite(EEPROM_PIN_16n64,HIGH); 
    eeprom_type= type;
  } else if( type==EEPROM_TYPE_28C64 ) {
    // On MUX, select 28C64, i.e. A11 
    digitalWrite(EEPROM_PIN_16n64,LOW ); 
    eeprom_type= type;
  } else {
    Serial.println(F("ERROR: unknown type"));
  }
}

// Returns the eeprom type (either EEPROM_TYPE_28C16 or EEPROM_TYPE_28C64)
unsigned int eeprom_type_get( void ) {
  return eeprom_type;
}

// Returns number of bytes (actually address locations) in EEPROM
unsigned int eeprom_size( void ) {
  return eeprom_type; // EEPROM type is coded as size
}

#define EEPROM_CE_NONE    -1
#define EEPROM_CE_ENABLE  LOW
#define EEPROM_CE_DISABLE HIGH
void eeprom_ce_set(int ce) {
  digitalWrite(EEPROM_PIN_nCE, ce );
}

int eeprom_ce_get(void) {
  return digitalRead(EEPROM_PIN_nCE);  
}
  
// Set all Nano data pins to `input`
void eeprom_input( bool input ) {
  if( input ) {
    pinMode(EEPROM_PIN_D0, INPUT);
    pinMode(EEPROM_PIN_D1, INPUT);
    pinMode(EEPROM_PIN_D2, INPUT);
    pinMode(EEPROM_PIN_D3, INPUT);
    pinMode(EEPROM_PIN_D4, INPUT);
    pinMode(EEPROM_PIN_D5, INPUT);
    pinMode(EEPROM_PIN_D6, INPUT);
    pinMode(EEPROM_PIN_D7, INPUT);
  } else {
    pinMode(EEPROM_PIN_D0, OUTPUT);
    pinMode(EEPROM_PIN_D1, OUTPUT);
    pinMode(EEPROM_PIN_D2, OUTPUT);
    pinMode(EEPROM_PIN_D3, OUTPUT);
    pinMode(EEPROM_PIN_D4, OUTPUT);
    pinMode(EEPROM_PIN_D5, OUTPUT);
    pinMode(EEPROM_PIN_D6, OUTPUT);
    pinMode(EEPROM_PIN_D7, OUTPUT);
  }
}

// Fill shift registers with `addr`
void eeprom_setaddr( uint16_t addr) {
  digitalWrite(EEPROM_PIN_CLK, LOW);
  shiftOut(EEPROM_PIN_DATA, EEPROM_PIN_CLK, MSBFIRST, addr>>8 );
  shiftOut(EEPROM_PIN_DATA, EEPROM_PIN_CLK, MSBFIRST, addr&0xff );
  #ifdef EEPROM_PIN_LATCH
  digitalWrite(EEPROM_PIN_LATCH, HIGH);
  digitalWrite(EEPROM_PIN_LATCH, LOW);
  #endif
}

// Initialize GPIO pins and shift registers
void eeprom_init() {
  // Give all output pins a defined level
  digitalWrite(EEPROM_PIN_nWE  ,HIGH); // On EEPROM, set Write Enable to "off" (low active)
  digitalWrite(EEPROM_PIN_nOE  ,HIGH); // On EEPROM, set Output Enable to "off" (low active)
  digitalWrite(EEPROM_PIN_DATA ,LOW ); // On Shift IC, set data low (not so important)
  digitalWrite(EEPROM_PIN_CLK  ,LOW ); // On Shift IC, set clock low (rising edge later will clock-in data)
  digitalWrite(EEPROM_PIN_nCE  ,HIGH); // On EEPROM, set CE to "off" (low active)
  #ifdef EEPROM_PIN_LATCH
  digitalWrite(EEPROM_PIN_LATCH,LOW ); // On Shift IC, set latch low (rising edge later will latch data)
  #endif
  digitalWrite(EEPROM_PIN_16n64,HIGH); // On MUX, select 28C16, i.e. nWE (low would be A11 for 28C64)
  
  // Give all output pins the mode output
  pinMode(EEPROM_PIN_nWE  , OUTPUT);
  pinMode(EEPROM_PIN_nOE  , OUTPUT);
  pinMode(EEPROM_PIN_DATA , OUTPUT);
  pinMode(EEPROM_PIN_CLK  , OUTPUT);
  pinMode(EEPROM_PIN_nCE  , OUTPUT);
  #ifdef EEPROM_PIN_LATCH
  pinMode(EEPROM_PIN_LATCH, OUTPUT);
  #endif
  pinMode(EEPROM_PIN_16n64, OUTPUT);

  // Select type
  eeprom_type_set(0x0800U);
  
  // On my board I have hooked red LEDs to Ax lines, and green LEDs to Dx lines; play start-up animation
  uint16_t addr=1;
  while( addr<EEPROM_TYPE_28C64 ) { eeprom_setaddr(addr); addr<<=1; delay(30); } 
  addr>>=1; 
  while( addr>1 ) { addr>>=1; eeprom_setaddr(addr); delay(30); } 

  // The default mode is to show the data contents of the last used EEPROM address. 
  // So, on EEPROM, set Output Enable to "on" (low active)
  eeprom_read(0x0000);
}

// Record the last addr shifted in (used by the physical buttons)
uint16_t eeprom_addr_last;

// Reads one byte from address 'addr' from the EEPROM and returns it
// Recall, the default mode of the EEPROM is Output Enable is "on" (so that the data LEDs show last value)
uint8_t eeprom_read(uint16_t addr) {
  eeprom_addr_last= addr;
  // On EEPROM, set Output Enable to "off" (low active)
  digitalWrite(EEPROM_PIN_nOE,HIGH);  
  // Switch data pins of Arduino to input
  eeprom_input(true);
  // Set address lines
  eeprom_setaddr(addr);
  // Go back to default: On EEPROM, set Output Enable to "on" (low active), Nano is input
  digitalWrite(EEPROM_PIN_nOE,LOW);
  delayMicroseconds(1); // "Address to Output Delay" is 150ns, so 1000ns should be enough
  // Read data pins
  uint8_t data= 0;
  data += digitalRead(EEPROM_PIN_D0) << 0;
  data += digitalRead(EEPROM_PIN_D1) << 1;
  data += digitalRead(EEPROM_PIN_D2) << 2;
  data += digitalRead(EEPROM_PIN_D3) << 3;
  data += digitalRead(EEPROM_PIN_D4) << 4;
  data += digitalRead(EEPROM_PIN_D5) << 5;
  data += digitalRead(EEPROM_PIN_D6) << 6;
  data += digitalRead(EEPROM_PIN_D7) << 7;
  // Return result
  return data;
}

// Writes byte 'data' to address 'addr' in the EEPROM 
// Recall, the default mode of the EEPROM is Output Enable is "on" (so that the data LEDs show last value)
void eeprom_write(uint16_t addr, uint8_t data ) {
  eeprom_addr_last= addr;
  // On EEPROM, set Output Enable to "off" (low active)
  digitalWrite(EEPROM_PIN_nOE,HIGH);  
  // Switch data pins to output
  eeprom_input(false);
  // Set address lines
  eeprom_setaddr(addr);
  // Write data pins
  digitalWrite(EEPROM_PIN_D0, data & (1<<0) );
  digitalWrite(EEPROM_PIN_D1, data & (1<<1) );
  digitalWrite(EEPROM_PIN_D2, data & (1<<2) );
  digitalWrite(EEPROM_PIN_D3, data & (1<<3) );
  digitalWrite(EEPROM_PIN_D4, data & (1<<4) );
  digitalWrite(EEPROM_PIN_D5, data & (1<<5) );
  digitalWrite(EEPROM_PIN_D6, data & (1<<6) );
  digitalWrite(EEPROM_PIN_D7, data & (1<<7) );
  // On EEPROM, set Write Enable to "on" (low active)
  digitalWrite(EEPROM_PIN_nWE,LOW); 
  delayMicroseconds(1); // "Write Pulse Width " is 1000ns
  // On EEPROM, set Write Enable to "off" (low active)
  digitalWrite(EEPROM_PIN_nWE,HIGH);
  delayMicroseconds(1000); // "Write Cycle Time" is 1ms
  // Go back to default: On EEPROM, set Output Enable to "on" (low active), Nano must be input
  eeprom_input(true);
  digitalWrite(EEPROM_PIN_nOE,LOW);
  // Check write completed 
#if 1
  delayMicroseconds(10*1000); // timeout of 10ms as in https://youtu.be/K88pgWhEb1M?t=3130
#else
  // Out of the 5 chips I have, for 4 the D7 inversion is not working.
  // For a while, I thought that when read-back gives the written data the cycle would be complete.
  // But even that is not true...
  //Serial.print("[");
  uint32_t t1, t0=micros();
  while( 1 ) {
    uint8_t data2= eeprom_read(addr); 
    t1=micros();
    //Serial.print(data2,HEX);
    //Serial.write(" ");
    if( data!=0 && data==data2 ) break;
    if( t1-t0 > 10*1000 ) break; // timeout of 10ms as in https://youtu.be/K88pgWhEb1M?t=3130
  }
  //Serial.print(t1-t0);
  //Serial.println("us]");
#endif  
}


// Cmd ==================================================================
// Command interpreter

// Each command has a descriptor, containing a pointer to its handler 'main', the string 'name' that invokes it, and a short help text.
// The main function is much like c's main, it has argc and argv, but the first param is a pointer to its own descriptor.
typedef struct cmd_desc_s cmd_desc_t;
typedef void (*cmd_main_t)( cmd_desc_t * desc, int argc, char * argv[] );
struct cmd_desc_s { cmd_main_t main; const char * name; const char * shorthelp; const char * longhelp; };
extern struct cmd_desc_s cmd_descs[];

// Finds the command descriptor for a command with name `name`.
// When not found, returns 0.
struct cmd_desc_s * cmd_find(char * name ) {
  struct cmd_desc_s * d= cmd_descs;
  while( d->name!=0 && strstr(d->name,name)!=d->name ) d++;
  if( d->name!=0 ) return d;
  return 0;
}

// Parse a string to a hex number, returns false if there were errors. 
// If true is returned, *v is the parsed value.
bool cmd_parse(char*s,uint16_t*v) {
  if( v==0 ) return false;
  *v= 0;
  if( s==0 ) return false;
  if( *s==0 ) return false;
  if( strlen(s)>4 ) return false;
  while( *s!=0 ) {
    if     ( '0'<=*s && *s<='9' ) *v = (*v)*16 + *s - '0';
    else if( 'a'<=*s && *s<='f' ) *v = (*v)*16 + *s - 'a' + 10;
    else if( 'A'<=*s && *s<='F' ) *v = (*v)*16 + *s - 'A' + 10;
    else return false;
    s++;
  }
  return true;
}

// Returns true iff s represents a valid stream entry (a byte 00..ff or *)
bool cmd_streams(char*s) {
  if( strcmp(s,"*")==0 ) return true;
  uint16_t v;
  bool ok=cmd_parse(s,&v);
  ok= ok & (v<=0xFF);
  return ok;
}

// Reads 'num' bytes from the EEPROM, starting at 'addr'.
// Prints all values to Serial (in lines of 'CMD_BYTESPERLINE' bytes).
#define CMD_BYTESPERLINE 16
void cmd_read(uint16_t addr, uint16_t num) {
  char buf[8];
  for(uint16_t base= addr; base<addr+num; base+=CMD_BYTESPERLINE ) {
    snprintf(buf,sizeof(buf),"%03x:",base); Serial.print(buf);
    uint16_t a= base;
    for(int i=0; i<CMD_BYTESPERLINE && a<addr+num; i++,a++ ) {
      uint8_t data= eeprom_read(a);
      snprintf(buf,sizeof(buf)," %02x",data); Serial.print(buf);
    }
    Serial.println("");
  }
}

// This is the helper function for the commands write, verify and program.
// Writes the 'argc' bytes in the hex ascii strings in 'argv[]'.
// The bytes are written to address in global var 'cmd_stream_addr'.
// The reason for the global var is that this function has a stream mode (after "w 200 *" hex numbers can be passed on multiple lines).
// Stream mode is disabled if 'cmd_stream_mode' is '>' otherwise enabled.
// This function not only supports write, but also verify and program, the actual command is stored in 'cmd_stream_desc' 
// (again, global to support stream mode). Variable cmd_stream_errors counts the (verify) errors.
char                cmd_stream_mode= '>';
struct cmd_desc_s * cmd_stream_desc;
uint16_t            cmd_stream_addr;
int                 cmd_stream_errors= 0;
void cmd_write_verify_prog(int argc, char * argv[] ) {
  char buf[8];
  snprintf(buf,sizeof(buf),"%03x:", cmd_stream_addr ); Serial.print(buf);
  int ix=0;
  while( ix<argc ) {
    char * arg= argv[ix++];
    if( strcmp(arg,"*")==0 ) { 
      if( cmd_stream_mode=='>' ) { cmd_stream_mode=cmd_stream_desc->name[0]; Serial.print(F(" (*")); } else { Serial.print(F(" *)")); cmd_stream_mode='>'; }
      continue; 
    }
    if( cmd_stream_addr>=eeprom_size() ) { Serial.println(); Serial.print(F("ERROR: ")); Serial.print(cmd_stream_desc->name ); Serial.println(F(": <addr> out of range")); return; }
    uint16_t data1;
    if( !cmd_parse(arg,&data1) ) { Serial.println(); Serial.print(F("ERROR: ")); Serial.print(cmd_stream_desc->name ); Serial.println(F(": <data> must be hex")); return; }
    if( data1>=256 ) { Serial.println(); Serial.print(F("ERROR: ")); Serial.print(cmd_stream_desc->name ); Serial.println(F(": <data> must be 00..FF")); return; }
    // Start the action (depending on w/v/p)
    if( cmd_stream_desc->name[0]=='w' || cmd_stream_desc->name[0]=='p' ) { 
      eeprom_write(cmd_stream_addr,data1); 
    }
    snprintf(buf,sizeof(buf)," %02x", data1 ); Serial.print( buf );
    if( cmd_stream_desc->name[0]=='v' || cmd_stream_desc->name[0]=='p' ) { 
      uint8_t data2=eeprom_read(cmd_stream_addr); 
      if( data1!=data2 ) { snprintf(buf,sizeof(buf),"~%02x",data2); Serial.print(buf); cmd_stream_errors++; }
    }
    cmd_stream_addr++;
  }
  Serial.println("");
}

// The handler for the "read" command
void cmd_main_read( struct cmd_desc_s * desc, int argc, char * argv[] ) {
  (void)desc; // unused
  if( eeprom_ce_get()!=EEPROM_CE_ENABLE ) { Serial.println(F("ERROR: use 'opt' to enable EEPROM and select type")); return; }
  // read [ <addr> [ <num> ] ]
  if( argc==1 ) { cmd_read(0x000, eeprom_size()); return; }
  // Parse addr
  uint16_t addr;
  if( !cmd_parse(argv[1],&addr) ) { Serial.println(F("ERROR: read: <addr> must be hex")); return; }
  if( addr>=eeprom_size() ) { Serial.println(F("ERROR: read <addr>: <addr> out of range")); return; }
  if( argc==2 ) { cmd_read(addr,1); return; }
  // Parse num
  uint16_t num;
  if( !cmd_parse(argv[2],&num) ) { Serial.println(F("ERROR: read: <num> must be hex")); return; }
  if( addr+num>eeprom_size() ) { Serial.println(F("ERROR: read: <addr>+<num> out of range")); return; }
  if( argc==3 ) { cmd_read(addr,num); return; }
  Serial.println(F("ERROR: read: too many arguments"));
}

const char cmd_read_longhelp[] PROGMEM = 
  "SYNTAX: read [ <addr> [ <num> ] ]\n"
  "- reads <num> bytes from EEPROM, starting at location <addr>\n"
  "- when <num> is absent, it defaults to 1\n"
  "- when <addr> and <num> are absent, reads entire EEPROM\n"
  "NOTE:\n"
  "- <addr> and <num> are in hex\n"
;
  
// The handler for the "write" command, but also for "verify" and "program"
void cmd_main_write(struct cmd_desc_s * desc, int argc, char * argv[] ) {
  if( eeprom_ce_get()!=EEPROM_CE_ENABLE ) { Serial.println(F("ERROR: use 'opt' to enable EEPROM and select type")); return; }
  if( argc<3 ) { Serial.print(F("ERROR: ")); Serial.print(desc->name ); Serial.println(F(": expected <addr> <data>...")); return; }
  // Parse addr
  uint16_t addr;
  if( !cmd_parse(argv[1],&addr) ) { Serial.println(F("ERROR: write/verify : <addr> must be hex")); return; }
  if( addr>=eeprom_size() ) { Serial.print(F("ERROR: ")); Serial.print(desc->name ); Serial.println(F(": <addr> out of range")); return; }
  // Write data('s)
  cmd_stream_addr= addr;
  cmd_stream_desc= desc;
  cmd_write_verify_prog(argc-2, argv+2 );
}

const char cmd_write_longhelp[] PROGMEM = 
  "SYNTAX: write <addr> <data>...\n"
  "- writes <data> byte to EEPROM location <addr>\n"
  "- multiple <data> bytes allowed (auto increment of <addr>)\n"
  "- <data> may be *, this toggles streaming mode\n"
  "NOTE:\n"
  "- <addr> and <data> are in hex\n"
  "STREAMING:\n"
  "- when streaming is active, the end-of-line does not terminate the command\n"
  "- next lines having 0 or more <data> will also be written\n"
  "- the prompt for next lines show the streaming mode (write, program, or verify)\n"
  "- the prompt for next lines also show target address\n"
  "- a line with * or a command will stop streaming mode\n"
;
  
const char cmd_program_longhelp[] PROGMEM = 
  "SYNTAX: program <addr> <data>...\n"
  "- performs write followed by verify\n"
  "- see help for those commands for details\n"
;

// The handler for the "verify" command
const char * s_clear = "clear";
const char * s_print = "print";
uint32_t cmd_verify_ms;
int cmd_verify_uartoverflow;
void cmd_main_verify(struct cmd_desc_s * desc, int argc, char * argv[] ) {
  if( eeprom_ce_get()!=EEPROM_CE_ENABLE ) { Serial.println(F("ERROR: use 'opt' to enable EEPROM and select type")); return; }
  // verify clear
  if( argc==2 ) {
    if( strstr(s_clear,argv[1])==s_clear) { cmd_verify_ms= millis(); cmd_verify_uartoverflow=0; cmd_stream_errors=0; if( argv[0][0]!='@') Serial.println(F("verify: cleared")); }
    else if( strstr(s_print,argv[1])==s_print) { Serial.print(F("verify: ")); Serial.print(cmd_stream_errors); Serial.print(F(" errors, ")); Serial.print(cmd_verify_uartoverflow); Serial.print(F(" uart overflows, ")); Serial.print(millis()-cmd_verify_ms);Serial.println(F(" ms"));}
    else Serial.println(F("ERROR: verify: expected <addr> <data>... or 'clear' or 'print'"));
  } else {
    cmd_main_write(desc,argc,argv);
  }
}

const char cmd_verify_longhelp[] PROGMEM = 
  "SYNTAX: verify <addr> <data>...\n"
  "- reads byte from EEPROM location <addr> and compares to <data>\n"
  "- prints <data> if equal, otherwise '<data>~<read>', where <read> is read data\n"
  "- unequal values increment global error counter\n"
  "- multiple <data> bytes allowed (auto increment of <addr>)\n"
  "- <data> may be *, this toggles streaming mode (see 'write' command)\n"
  "SYNTAX: verify print\n"
  "- prints global error counter, uart overflow counter and stopwatch\n"
  "SYNTAX: [@]verify clear\n"
  "- sets global error counter, uart overflow counter, and stopwatch to 0\n"
  "- with @ present, no feedback is printed\n"
  "NOTE:\n"
  "- <addr> and <data> are in hex\n"
;

// Helper for cmd_main_erase() to erase part of EEPROM
// Erases the memory by writing <num> bytes starting from <addr>.
// The written value is <data>, but it is stepped by one every <step> addresses.
// Note that a "big" value for <step> disables stepping (and 0 is the biggest value, since there is pre-increment)
void cmd_erase(uint16_t addr, uint16_t num, uint8_t data, uint16_t step ) {
  uint16_t count=0;
  char buf[8];
  for(uint16_t base= addr; base<addr+num; base+=CMD_BYTESPERLINE ) {
    snprintf(buf,sizeof(buf),"%03x:",base); Serial.print(buf);
    uint16_t a= base;
    for(int i=0; i<CMD_BYTESPERLINE && a<addr+num; i++,a++ ) {
      eeprom_write(a,data);
      uint8_t data2= eeprom_read(a);
      snprintf(buf,sizeof(buf)," %02x",data); Serial.print(buf);
      if( data!=data2 ) { snprintf(buf,sizeof(buf),"~%02x",data2); Serial.print(buf); cmd_stream_errors++; }
      count++;
      if( count==step ) { data+=1; /*might wrap*/ count=0; }
    }
    Serial.println("");
  }
}

// The handler for the "erase" command
void cmd_main_erase( struct cmd_desc_s * desc, int argc, char * argv[] ) {
  (void)desc; // unused
  if( eeprom_ce_get()!=EEPROM_CE_ENABLE ) { Serial.println(F("ERROR: use 'opt' to enable EEPROM and select type")); return; }
  if( strcmp(argv[0],"erase")!=0 ) { Serial.println(F("ERROR: erase: command must be spelled in full to prevent accidental erase")); return; }
  // erase [ <addr> [ <num> [ <data> [ <step> ] ] ] ]
  if( argc==1 ) { cmd_erase(0x000, eeprom_size(), 0xFF, 0); return; }
  // Parse addr
  uint16_t addr;
  if( !cmd_parse(argv[1],&addr) ) { Serial.println(F("ERROR: erase: <addr> must be hex")); return; }
  if( addr>=eeprom_size() ) { Serial.println(F("ERROR: erase <addr>: <addr> out of range")); return; }
  if( addr+0x100>eeprom_size() )  { Serial.println(F("ERROR: erase: <addr>+100 out of range")); return; }
  if( argc==2 ) { cmd_erase(addr,0x100,0xFF,0); return; }
  // Parse num
  uint16_t num;
  if( !cmd_parse(argv[2],&num) ) { Serial.println(F("ERROR: erase: <num> must be hex")); return; }
  if( addr+num>eeprom_size() ) { Serial.println(F("ERROR: erase: <addr>+<num> out of range")); return; }
  if( argc==3 ) { cmd_erase(addr,num,0xFF,0); return; }
  // Parse data
  uint16_t data;
  if( !cmd_parse(argv[3],&data) ) { Serial.println(F("ERROR: erase: <data> must be hex")); return; }
  if( data>=256 ) { Serial.print(F("ERROR: erase: <data> must be 00..FF")); return; }
  if( argc==4 ) { cmd_erase(addr,num,data,0); return; }
  uint16_t step;
  if( !cmd_parse(argv[4],&step) ) { Serial.println(F("ERROR: erase: <step> must be hex")); return; }
  if( argc==5 ) { cmd_erase(addr,num,data,step); return; }
  Serial.println(F("ERROR: erase: too many arguments"));
}

const char cmd_erase_longhelp[] PROGMEM = 
  "SYNTAX: erase [ <addr> [ <num> [ <data> [ <step> ] ] ] ]\n"
  "- erase <num> bytes in EEPROM, starting at location <addr>, by writing <data>\n"
  "- <data> it is stepped by one every <step> addresses.\n"
  "- when <step> is absent, <data> is never stepped\n"
  "- when <data> is absent, erase by writing 1's (<data>=FF)\n"
  "- when <num> is absent, erase one page (<num>=100)\n"
  "- when <addr> is absent, erase entire EEPROM\n"
  "- erase is always verified\n"
  "- command name 'erase' must be spelled in full to prevent accidental erase\n"
  "NOTE:\n"
  "- <addr>, <num>, <data>, and <step> are in hex\n"
;
  

// The handler for the "info" command
void cmd_main_info(struct cmd_desc_s * desc, int argc, char * argv[] ) {
  (void)desc; // unused
  (void)argc; // unused
  (void)argv; // unused
  Serial.println(F("info: name   : " PROG_NAME));
  Serial.println(F("info: author : " PROG_AUTHOR));
  Serial.println(F("info: version: " PROG_VERSION));
  Serial.println(F("info: date   : " PROG_DATE));
  Serial.print  (F("info: voltage: ")); Serial.print( mega328_Vcc() ); Serial.println(F("mV"));
  Serial.print  (F("info: cpufreq: ")); Serial.print( F_CPU ); Serial.println(F("Hz"));
  Serial.print  (F("info: uartbuf: ")); Serial.print( SERIAL_RX_BUFFER_SIZE ); Serial.println(F(" bytes"));
}

const char cmd_info_longhelp[] PROGMEM = 
  "SYNTAX: info\n"
  "- shows application information (name, author, version, date)\n"
  "- shows cpu info (cpu voltage, cpu speed, uart rx buf size)\n"
;

// The handler for the "echo" command
extern bool cmd_echo;
void cmd_echo_print() { Serial.print(F("echo: ")); Serial.println(cmd_echo?F("enabled"):F("disabled")); }
const char * s_enable = "enable";
const char * s_disable = "disable";
const char * s_line = "line";
void cmd_main_echo(struct cmd_desc_s * desc, int argc, char * argv[] ) {
  (void)desc; // unused
  if( argc==1 ) {
    cmd_echo_print();
    return;
  }
  if( argc==2 && strstr(s_enable,argv[1])==s_enable ) {
    cmd_echo= true;
    if( argv[0][0]!='@') cmd_echo_print();
    return;
  }
  if( argc==2 && strstr(s_disable,argv[1])==s_disable ) {
    cmd_echo= false;
    if( argv[0][0]!='@') cmd_echo_print();
    return;
  }
  int start= 1;
  if( strstr(s_line,argv[1])==s_line ) { start=2; }
  // This tries to restore the command line (put spaces back on the '\0's)
  //char * s0=argv[start-1]+strlen(argv[start-1])+1;
  //char * s1=argv[argc-1];
  //for( char * p=s0; p<s1; p++ ) if( *p=='\0' ) *p=' ';
  //Serial.println(s0); 
  for( int i=start; i<argc; i++) { if(i>start) Serial.print(" "); Serial.print(argv[i]);  }
  Serial.println();
}

const char cmd_echo_longhelp[] PROGMEM = 
  "SYNTAX: echo [line] <word>...\n"
  "- prints all words (useful in scripts)\n"
  "SYNTAX: [@]echo [ enable | disable ]\n"
  "- with arguments enables/disables terminal echoing\n"
  "- (disabled is useful in scripts; output is relevant, but input much less)\n"
  "- with @ present, no feedback is printed\n"
  "- without arguments shows status of terminal echoing\n"
  "NOTES:\n"
  "- 'echo line' prints a white line (there are no <word>s)\n"
  "- 'echo line enable' prints 'enable'\n"
  "- 'echo line disable' prints 'disable'\n"
  "- 'echo line line' prints 'line'\n"
;

// The handler for the "help" command
extern struct cmd_desc_s cmd_descs[];
void cmd_main_help(struct cmd_desc_s * desc, int argc, char * argv[] ) {
  (void)desc; // unused
  if( argc==1 ) {
    Serial.println(F("Available commands"));
    for( struct cmd_desc_s * d= cmd_descs; d->name!=0; d++ ) {
      char buf[80];
      snprintf(buf,sizeof(buf)," %s - %s", d->name, d->shorthelp );
      Serial.println(buf);
    }
  } else if( argc==2 ) {
    struct cmd_desc_s * d= cmd_find(argv[1]);
    if( d==0 ) {
      Serial.println(F("ERROR: help: command not found (try 'help')"));    
    } else {
      // longhelp is in PROGMEM so we need to get the chars one by one...
      for(unsigned i=0; i<strlen_P(d->longhelp); i++) 
        Serial.print((char)pgm_read_byte_near(d->longhelp+i));
    }
  } else {
    Serial.println(F("ERROR: help: too many arguments"));
  }
}

const char cmd_help_longhelp[] PROGMEM = 
  "SYNTAX: help\n"
  "- lists all commands\n"
  "SYNTAX: help <cmd>\n"
  "- gives detailed help on command <cmd>\n"
  "NOTES:\n"
  "- all commands may be shortened, for example 'help', 'hel', 'he', 'h'\n"
  "- sub commands may be shortened, for example 'verify clear' to 'verify c'\n"
  "- commands may be suffixed with a comment starting with #\n"
  "- normal prompt is >>, other prompt indicates streaming mode (see write)\n"
;

// The handler for the "options" command
void cmd_options_print() { 
  int mux=digitalRead(EEPROM_PIN_16n64); 
  Serial.print(F("options: type: ")); Serial.print( mux?F("28c16 (2k*8b = "):F("28c64 (8k*8b = dec ") ); Serial.print( eeprom_size() ); Serial.println( F("B) ") ); 
  Serial.print(F("options: chip: ")); Serial.println( digitalRead(EEPROM_PIN_nCE)==LOW?F("enabled"):F("disabled")); 
}
const char * s_type = "type";
const char * s_chip = "chip";
const char * s_28c16 = "28c16";
const char * s_28c64 = "28c64";
void cmd_main_options(struct cmd_desc_s * desc, int argc, char * argv[] ) {
  (void)desc; // unused
  if( argc==1 ) {
    cmd_options_print();
    return;
  }
  int opt_type= EEPROM_TYPE_NONE;
  int opt_ce= EEPROM_CE_NONE;
  int argix= 1;
  while( argix<argc ) {
    if( strstr(s_type,argv[argix])==s_type ) {
      // There is a 'type' subcommand. Was there already one?
      if( opt_type!=EEPROM_TYPE_NONE ) { Serial.println(F("ERROR: options: 'type' already passed")); return; }
      // Does it have a value
      argix++;    
      if( argix==argc ) { Serial.println(F("ERROR: options: 'type' needs value")); return; }
      // Is the value acceptable
      if( strstr(s_28c16,argv[argix])==s_28c16 ) { 
        opt_type= EEPROM_TYPE_28C16;
      } else if( strstr(s_28c64,argv[argix])==s_28c64 ) {
        opt_type= EEPROM_TYPE_28C64;
      } else {
        Serial.println(F("ERROR: options: 'type' needs '28c16' or '28c64'")); return;
      }
      argix++;    
    } else if( strstr(s_chip,argv[argix])==s_chip ) {
      // There is a 'ce' subcommand. Was there already one?
      if( opt_ce!=EEPROM_CE_NONE ) { Serial.println(F("ERROR: options: 'chip' already passed")); return; }
      // Does it have a value
      argix++;    
      if( argix==argc ) { Serial.println(F("ERROR: options: 'chip' needs value")); return; }
      // Is the value acceptable
      if( strstr(s_enable,argv[argix])==s_enable ) { 
        opt_ce= EEPROM_CE_ENABLE;
      } else if( strstr(s_disable,argv[argix])==s_disable ) {
        opt_ce= EEPROM_CE_DISABLE;
      } else {
        Serial.println(F("ERROR: options: 'chip' needs 'enable' or 'disable'")); return;
      }
      argix++;    
    } else { 
      Serial.println(F("ERROR: options: expected 'type' or 'chip'")); return; 
    }
  }
  // Now activate the options (chip enable as last)
  if( opt_type!=EEPROM_TYPE_NONE ) eeprom_type_set(opt_type);
  if( opt_ce!=EEPROM_CE_NONE ) eeprom_ce_set(opt_ce);
  // feedback
  cmd_options_print();
}

const char cmd_options_longhelp[] PROGMEM = 
  "SYNTAX: options ( type <val> | chip <val> )*\n"
  "- without arguments, shows configured options\n"
  "- type 28c16 | 28c64: configures programmer for EEPROM type\n"
  "- chip enable | disable: configures chip-enable line of EEPROM\n"
;

// All command descriptors
struct cmd_desc_s cmd_descs[] = {
  { cmd_main_help   , "help"   , "gives help (try 'help help')", cmd_help_longhelp },
  { cmd_main_info   , "info"   , "application info", cmd_info_longhelp },
  { cmd_main_echo   , "echo"   , "echo a message (or en/disables echoing)", cmd_echo_longhelp },
  { cmd_main_read   , "read"   , "read EEPROM memory", cmd_read_longhelp },
  { cmd_main_write  , "write"  , "write EEPROM memory", cmd_write_longhelp },
  { cmd_main_verify , "verify" , "verify EEPROM memory", cmd_verify_longhelp },
  { cmd_main_write  , "program", "write and verify EEPROM memory", cmd_program_longhelp },
  { cmd_main_erase  , "erase"  , "erases EEPROM memory", cmd_erase_longhelp },
  { cmd_main_options, "options", "select options for the programmer", cmd_options_longhelp },
  { 0,0,0,0 }
};

// The state machine for receiving characters via Serial
#define CMD_BUFSIZE 128
#define CMD_MAXARGS 32
char cmd_buf[CMD_BUFSIZE];
int  cmd_ix;
bool cmd_echo;

// Print the prompt when waiting for input (special prefix when in streaming mode)
void cmd_prompt() {
  char buf[8];
  Serial.print(cmd_stream_mode);
  if( cmd_stream_mode!='>' ) {
    snprintf(buf,sizeof(buf),"%03x",  cmd_stream_addr );
    Serial.print( buf );
  }
  Serial.print(F("> "));
}

// Initializes the command interpreter
void cmd_init() {
  cmd_ix=0;
  cmd_prompt();
  cmd_echo= true;
}

// Execute the entered command (terminated with a press on RETURN key)
void cmd_exec() {
  char * argv[ CMD_MAXARGS ];
  // Cut a trailing comment
  char * cmt= strchr(cmd_buf,'#');
  if( cmt!=0 ) { *cmt='\0'; cmd_ix= cmt-cmd_buf; } // trim comment
  // Find the arguments (set up argv/argc)
  int argc= 0;
  int ix=0;
  while( ix<cmd_ix ) {
    // scan for begin of word (ie non-space)
    while( (ix<cmd_ix) && ( cmd_buf[ix]==' ' || cmd_buf[ix]=='\t' ) ) ix++;
    if( !(ix<cmd_ix) ) break;
    argv[argc]= &cmd_buf[ix];
    argc++;
    // scan for end of word (ie space)
    while( (ix<cmd_ix) && ( cmd_buf[ix]!=' ' && cmd_buf[ix]!='\t' ) ) ix++;
    cmd_buf[ix]= '\0';
    ix++;
  }
  //for(ix=0; ix<argc; ix++) { Serial.print(ix); Serial.print("='"); Serial.print(argv[ix]); Serial.print("'"); Serial.println(""); }
  // Execute command
  if( argc==0 ) {
    // Empty command entered
    return; 
  }
  if( cmd_stream_mode!='>' && cmd_streams(argv[0]) ) {
    // Streaming mode is active and input is valid in stream, so stream byte(s)
    cmd_write_verify_prog(argc, argv );
    return;
  }
  char * s= argv[0];
  if( *s=='@' ) s++;
  struct cmd_desc_s * d= cmd_find(s);
  if( d!=0 ) {
    // Found a command. Execute it 
    cmd_stream_mode= '>'; // (clear streaming mode)
    d->main(d, argc, argv ); // Execute handler of command
    return;
  } 
  Serial.println(F("Error: command not found (try help)")); 
}

// Receiving characters via Serial (and update the statemachine)
void cmd_add(int ch) {
  if( ch=='\n' || ch=='\r' ) {
    if( cmd_echo ) Serial.println("");
    cmd_buf[cmd_ix]= '\0'; // Terminate (make cmd_buf a c-string)
    cmd_exec();
    cmd_ix=0;
    if( cmd_echo ) cmd_prompt();
  } else if( ch=='\b' ) {
    if( cmd_ix>0 ) {
      if( cmd_echo ) Serial.print(F("\b \b"));
      cmd_ix--;
    } else {
      // backspace with no more chars in buf; ignore
    }
  } else {
    if( cmd_ix<CMD_BUFSIZE-1 ) {
      cmd_buf[cmd_ix++]= ch;
      if( cmd_echo ) Serial.print((char)ch);
    } else {
      if( cmd_echo ) Serial.print("_\b");
      // input buffer full
    }
  }
}


// Buttons =============================================================

// Pins for the keys
#define BUT_PIN_PLS  A5 // Plus or Up
#define BUT_PIN_MIN  A4 // Minus or Down
#define BUT_PIN_SEL  A3 // Change selection

// Code for the keys
#define BUT_PLS  1
#define BUT_MIN  2
#define BUT_SEL  4

int but_prv;
int but_cur;

void but_init( void ) {
  pinMode(BUT_PIN_PLS, INPUT);
  pinMode(BUT_PIN_MIN, INPUT);
  pinMode(BUT_PIN_SEL, INPUT);
  but_scan();
  but_scan();
}

void but_scan( void ) {
  but_prv= but_cur;
  but_cur= 0;
  if( digitalRead(BUT_PIN_PLS) ) but_cur|= BUT_PLS;
  if( digitalRead(BUT_PIN_MIN) ) but_cur|= BUT_MIN;
  if( digitalRead(BUT_PIN_SEL) ) but_cur|= BUT_SEL;
}

int but_wentdown( void ) {
  return ( but_cur ^ but_prv ) & but_cur;
}


// Main =================================================================


void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println(F("   _____  _____________________"));
  Serial.println(F("  /  _  \\ \\_   _____/\\______   \\"));
  Serial.println(F(" /  /_\\  \\ |    __)_  |     ___/"));
  Serial.println(F("/    |    \\|        \\ |    |"));
  Serial.println(F("\\____|__  /_______  / |____|")); 
  Serial.println(F("        \\/        \\/")); 
  Serial.println("");
  Serial.println(F(PROG_NAME " V" PROG_VERSION));
  Serial.println(F("Type 'help' for help"));
  Serial.println();
  cmd_init();
  but_init();
  eeprom_init();
}

void loop() {
  
  // Check incoming serial chars
  int n=0; // Counts number of bytes read, this is roughly the number of bytes in the UART buffer
  while(1) {
    int ch= Serial.read();
    if( ch==-1 ) break;
    if( ++n==SERIAL_RX_BUFFER_SIZE ) { // Possible UART buffer overflow
      cmd_verify_uartoverflow++; 
      Serial.println(); Serial.println( F("WARNING: uart overflow") ); Serial.println(); 
    }
    // Process read char by feeding it to command interpreter
    cmd_add(ch);
  }
  
  // Check for button presses
  static unsigned long inc=1;
  but_scan();
  if(  eeprom_ce_get()==EEPROM_CE_ENABLE ) {
    // EEPROM is enabled, button presses select address
    if( but_wentdown() & BUT_MIN ) { eeprom_read( (eeprom_addr_last+eeprom_size()-inc)%eeprom_size() ); }
    if( but_wentdown() & BUT_PLS ) { eeprom_read( (eeprom_addr_last              +inc)%eeprom_size() ); }
    if( but_wentdown() & BUT_SEL  ) { 
      inc<<=4; 
      if( inc>=eeprom_size() ) inc=1; 
      uint16_t last= eeprom_addr_last;
      unsigned long mask= 15*inc;
      eeprom_ce_set(EEPROM_CE_DISABLE);
      for(int i=0; i<5; i++) {
        eeprom_read( (eeprom_addr_last & ~mask )%eeprom_size() ); 
        delay(75);
        eeprom_read( (eeprom_addr_last |  mask )%eeprom_size() );
        delay(75);
      }
      eeprom_read( last );
      eeprom_ce_set(EEPROM_CE_ENABLE);
    }  
  } else {
    // EEPROM is disabled, button presses select type
    if( but_wentdown() & BUT_MIN ) { eeprom_type_set(EEPROM_TYPE_28C64); }
    if( but_wentdown() & BUT_PLS ) { eeprom_type_set(EEPROM_TYPE_28C16); }
    if( but_wentdown() & BUT_SEL ) { eeprom_ce_set(EEPROM_CE_ENABLE); }
  }
  
}
