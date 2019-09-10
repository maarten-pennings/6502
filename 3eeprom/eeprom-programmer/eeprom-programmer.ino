// Arduino EEPROM Programmer (for the AT28C16)
// Features a complete command interpreter
//   See https://github.com/maarten-pennings/6502/tree/master/3eeprom


// Todo:
// - Add shift registers with latches so that we don't see the LEDs flicker
// - detect lost characters in uart transmission (how?)
// - IO to control 2k or 8k chip
// - command 'opt size <size> offset <offset>'
// - allow 'read 56k 1p' with factors k for 1024 and p for 256


#define PROG_NAME    "Arduino EEPROM Programmer"
#define PROG_EEPROM  "AT28C16 2k*8b"
#define PROG_VERSION "8a"
#define PROG_DATE    "2019 sep 10"
#define PROG_AUTHOR  "Maarten Pennings"


// Voltage ==============================================================
// Get own VCC the Arduino Nano runs on
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
// Driver for the eeprom

// Number of bytes (actually address locations) in EEPROM
#define EEPROM_SIZE 0x800

// This sketch supports two types of shift ICs: 74HTC164 and 74HC595 (the latter has less flicker).
// 74HTC164 (two ICs: Q7 of the first is chained to DSA and DSB of the second)
//   The 74HTC164's are controlled by a Nano: EEPROM_PIN_DATA to DSA and DSB and EEPROM_PIN_CLK to CP.
//   The Q0-Q7 (1st IC) and the Q0-Q2 (2nd IC) control A0-A10 of the EEPROM.
//   MR to VCC
// 74HC595 (two ICs: Q7S of the first is chained to DS of the second)
//   The 74HC595's are controlled by the Nane: EEPROM_PIN_DATA to DS, EEPROM_PIN_CLK to SHCP, EEPROM_PIN_LATCH to STCP
//   The Q0-Q7 (1st IC) and the Q0-Q2 (2n IC) control A0-A10 of the EEPROM
//   nOE is tied to GND, nMR is tied to VCC
// Note that the 74HC595 had one extra control line EEPROM_PIN_LATCH which, when strobed, latches all values.

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
#define EEPROM_PIN_LATCH A0

// Undefine next macro when using 74HTC164 
// But you could leave it defined: nothing will happen since pin EEPROM_PIN_LATCH is unconnected.
//#undef EEPROM_PIN_LATCH 


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
  #ifdef EEPROM_PIN_LATCH
  digitalWrite(EEPROM_PIN_LATCH,LOW ); // On Shift IC, set latch low (rising edge later will latch data)
  #endif
  
  // Set all control pins as output
  pinMode(EEPROM_PIN_nWE  , OUTPUT);
  pinMode(EEPROM_PIN_nOE  , OUTPUT);
  pinMode(EEPROM_PIN_DATA , OUTPUT);
  pinMode(EEPROM_PIN_CLK  , OUTPUT);
  #ifdef EEPROM_PIN_LATCH
  pinMode(EEPROM_PIN_LATCH, OUTPUT);
  #endif

  // On my board I have hooked red LEDs to Ax lines, and green LEDs to Dx lines; play start-up animation
  uint16_t addr=1;
  while( addr<EEPROM_SIZE ) { eeprom_setaddr(addr); addr<<=1; delay(30); } 
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
  //   Out of the 5 chips I have, for 4 the D7 inversion is not working.
  //   The EEPROM simply returns 00 for D0-F7 until write cycle is completed and the real data appears.
  //   The 1 good EEPROM returns random bits (but MSB inversed) for D0-F7 until write cycle is completed and the real data appears.
  //   So, I poll for the correct 8 bits to appear. Unfortunately, this does not work when writing 00.
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
}


// Cmd ==================================================================
// Command interpreter

// Each command has a descriptor, containing a pointer to its handler 'main', the string 'name' that invokes it, and a short help text.
// The main function is much like c's main, it has argc and argv, but the first param is a pointer to its own descriptor.
typedef struct cmd_desc_s cmd_desc_t;
typedef void (*cmd_main_t)( cmd_desc_t * desc, int argc, char * argv[] );
struct cmd_desc_s { cmd_main_t main; char * name; char * shorthelp; const PROGMEM char * longhelp; };
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
    if( cmd_stream_addr>=EEPROM_SIZE ) { Serial.println(); Serial.print(F("ERROR: ")); Serial.print(cmd_stream_desc->name ); Serial.println(F(": <addr> out of range")); return; }
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
  // read [ <addr> [ <num> ] ]
  if( argc==1 ) { cmd_read(0x000, EEPROM_SIZE); return; }
  // Parse addr
  uint16_t addr;
  if( !cmd_parse(argv[1],&addr) ) { Serial.println(F("ERROR: read: <addr> must be hex")); return; }
  if( addr>=EEPROM_SIZE ) { Serial.println(F("ERROR: read <addr>: <addr> out of range")); return; }
  if( argc==2 ) { cmd_read(addr,1); return; }
  // Parse num
  uint16_t num;
  if( !cmd_parse(argv[2],&num) ) { Serial.println(F("ERROR: read: <num> must be hex")); return; }
  if( addr+num>EEPROM_SIZE ) { Serial.println(F("ERROR: read: <addr>+<num> out of range")); return; }
  if( argc==3 ) { cmd_read(addr,num); return; }
  Serial.println(F("ERROR: read: too many arguments"));
}

const char cmd_read_longhelp[] PROGMEM = 
  "SYNAX: read [ <addr> [ <num> ] ]\n"
  "- reads <num> bytes from EEPROM, starting at location <addr>\n"
  "- when <num> is absent, it defaults to 1\n"
  "- when <addr> and <num> are absent, reads entire EEPROM\n"
  "NOTE:\n"
  "- <addr> and <num> are in hex\n"
;
  
// The handler for the "write" command, but also for "verify" and "program"
void cmd_main_write(struct cmd_desc_s * desc, int argc, char * argv[] ) {
  if( argc<3 ) { Serial.print(F("ERROR: ")); Serial.print(desc->name ); Serial.println(F(": expected <addr> <data>...")); return; }
  // Parse addr
  uint16_t addr;
  if( !cmd_parse(argv[1],&addr) ) { Serial.println(F("ERROR: write/verify : <addr> must be hex")); return; }
  if( addr>=EEPROM_SIZE ) { Serial.print(F("ERROR: ")); Serial.print(desc->name ); Serial.println(F(": <addr> out of range")); return; }
  // Write data('s)
  cmd_stream_addr= addr;
  cmd_stream_desc= desc;
  cmd_write_verify_prog(argc-2, argv+2 );
}

const char cmd_write_longhelp[] PROGMEM = 
  "SYNAX: write <addr> <data>...\n"
  "- writes <data> byte to EEPROM location <addr>\n"
  "- multiple <data> bytes allowed (auto increment of <addr>)\n"
  "- <data> may be *, this toggles streaming mode\n"
  "NOTE:\n"
  "- <addr> and <data> are in hex\n"
  "STREAMING:\n"
  "- when streaming mode is 'on', the end-of-line does not terminate the command\n"
  "- next lines having 0 or more <data> will also be written\n"
  "- the prompt for next lines show the streaming mode (write, program, or verify)\n"
  "- the prompt for next lines also show target address\n"
  "- a line with * or a command will stop streaming mode\n"
;
  
const char cmd_program_longhelp[] PROGMEM = 
  "SYNAX: program <addr> <data>...\n"
  "- performs write followed by verify\n"
  "- see help for those commands for details\n"
;

// The handler for the "verify" command
char * s_clear = "clear";
char * s_print = "print";
uint32_t cmd_verify_ms;
int cmd_verify_uartoverflow;
void cmd_main_verify(struct cmd_desc_s * desc, int argc, char * argv[] ) {
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
  "SYNAX: verify <addr> <data>...\n"
  "- reads byte from EEPROM location <addr> and compares to <data>\n"
  "- prints <data> if equal, otherwise '<data>~<read>', where <read> is read data\n"
  "- unequal values increment global error counter\n"
  "- multiple <data> bytes allowed (auto increment of <addr>)\n"
  "- <data> may be *, this toggles streaming mode (see `write` command)\n"
  "SYNAX: verify print\n"
  "- prints global error counter, uart overflow counter and stopwatch\n"
  "SYNAX: [@]verify clear\n"
  "- sets global error counter, uart overflow counter, and stopwatch to 0\n"
  "- with @ present, no feedback is printed\n"
  "NOTE:\n"
  "- <addr> and <data> are in hex\n"
;

// helper for cmd_main_erase() to erase part of EEPROM
void cmd_erase(uint16_t addr, uint16_t num, uint8_t data ) {
  char buf[8];
  for(uint16_t base= addr; base<addr+num; base+=CMD_BYTESPERLINE ) {
    snprintf(buf,sizeof(buf),"%03x:",base); Serial.print(buf);
    uint16_t a= base;
    for(int i=0; i<CMD_BYTESPERLINE && a<addr+num; i++,a++ ) {
      eeprom_write(a,data);
      uint8_t data2= eeprom_read(a);
      snprintf(buf,sizeof(buf)," %02x",data); Serial.print(buf);
      if( data!=data2 ) { snprintf(buf,sizeof(buf),"~%02x",data2); Serial.print(buf); cmd_stream_errors++; }
    }
    Serial.println("");
  }
}

// The handler for the "erase" command
void cmd_main_erase( struct cmd_desc_s * desc, int argc, char * argv[] ) {
  // erase [ <addr> [ <num> [ <data> ] ] ]
  if( argc==1 ) { cmd_erase(0x000, EEPROM_SIZE, 0xFF); return; }
  // Parse addr
  uint16_t addr;
  if( !cmd_parse(argv[1],&addr) ) { Serial.println(F("ERROR: erase: <addr> must be hex")); return; }
  if( addr>=EEPROM_SIZE ) { Serial.println(F("ERROR: erase <addr>: <addr> out of range")); return; }
  if( addr+0x100>EEPROM_SIZE )  { Serial.println(F("ERROR: erase: <addr>+100 out of range")); return; }
  if( argc==2 ) { cmd_erase(addr,0x100,0xFF); return; }
  // Parse num
  uint16_t num;
  if( !cmd_parse(argv[2],&num) ) { Serial.println(F("ERROR: erase: <num> must be hex")); return; }
  if( addr+num>EEPROM_SIZE ) { Serial.println(F("ERROR: erase: <addr>+<num> out of range")); return; }
  if( argc==3 ) { cmd_erase(addr,num,0xFF); return; }
  // Parse data
  uint16_t data;
  if( !cmd_parse(argv[3],&data) ) { Serial.println(F("ERROR: erase: <data> must be hex")); return; }
  if( data>=256 ) { Serial.print(F("ERROR: erase: <data> must be 00..FF")); return; }
  if( argc==4 ) { cmd_erase(addr,num,data); return; }
  Serial.println(F("ERROR: erase: too many arguments"));
}

const char cmd_erase_longhelp[] PROGMEM = 
  "SYNAX: erase [ <addr> [ <num> [ <data> ] ] ]\n"
  "- erase <num> bytes in EEPROM, starting at location <addr>, by writing <data>\n"
  "- when <data> is absent, erase by writing 1's (<data>=FF)\n"
  "- when <num> is absent, erase one page (<num>=100)\n"
  "- when <addr> is absent, erase entire EEPROM\n"
  "- erase is always verified\n"
  "NOTE:\n"
  "- <addr>, <num>, and <data> are in hex\n"
;
  

// The handler for the "info" command
void cmd_main_info(struct cmd_desc_s * desc, int argc, char * argv[] ) {
  Serial.println(F("info: name   : " PROG_NAME));
  Serial.println(F("info: author : " PROG_AUTHOR));
  Serial.println(F("info: version: " PROG_VERSION));
  Serial.println(F("info: date   : " PROG_DATE));
  Serial.println(F("info: eeprom : " PROG_EEPROM));
  Serial.print  (F("info: voltage: ")); Serial.print( mega328_Vcc() ); Serial.println(F("mV"));
  Serial.print  (F("info: cpufreq: ")); Serial.print( F_CPU ); Serial.println(F("Hz"));
  Serial.print  (F("info: uartbuf: ")); Serial.print( SERIAL_RX_BUFFER_SIZE ); Serial.println(F(" bytes"));
}

const char cmd_info_longhelp[] PROGMEM = 
  "SYNAX: info\n"
  "- shows application information (name, author, version, date)\n"
  "- shows supported EEPROM(s)\n"
  "- shows cpu info (cpu voltage, cpu speed, uart rx buf size)\n"
;

// The handler for the "echo" command
extern bool cmd_echo;
void cmd_echo_print() { Serial.print("echo: "); Serial.println(cmd_echo?"enabled":"disabled"); }
char * s_enable = "enable";
char * s_disable = "disable";
char * s_line = "line";
void cmd_main_echo(struct cmd_desc_s * desc, int argc, char * argv[] ) {
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
  "SYNAX: echo [line] <word>...\n"
  "- echo's (prints) all words (useful in scripts)\n"
  "SYNAX: [@]echo [ enable | disable ]\n"
  "- without arguments shows status of terminal echoing\n"
  "- with arguments enables/disables terminal echoing\n"
  "- with @ present, no feedback is printed\n"
  "- useful in scripts; output is relevant, but input much less\n"
  "NOTES:\n"
  "- 'echo line' prints a white line (there are no <word>s)\n"
  "- 'echo line enable' prints 'enable'\n"
  "- 'echo line disable' prints 'disable'\n"
  "- 'echo line line' prints 'line'\n"
;

// The handler for the "help" command
extern struct cmd_desc_s cmd_descs[];
void cmd_main_help(struct cmd_desc_s * desc, int argc, char * argv[] ) {
  if( argc==1 ) {
    Serial.println("Available commands");
    for( struct cmd_desc_s * d= cmd_descs; d->name!=0; d++ ) {
      char buf[80];
      snprintf(buf,sizeof(buf)," %s - %s", d->name, d->shorthelp );
      Serial.println(buf);
    }
  } else if( argc==2 ) {
    struct cmd_desc_s * d= cmd_find(argv[1]);
    if( d==0 ) {
      Serial.println("ERROR: help: command not found (try 'help')");    
    } else {
      // longhelp is in PROGMEM so we need to get the chars one by one...
      for(int i=0; i<strlen_P(d->longhelp); i++) 
        Serial.print((char)pgm_read_byte_near(d->longhelp+i));
    }
  } else {
    Serial.println("ERROR: help: too many arguments");
  }
}

const char cmd_help_longhelp[] PROGMEM = 
  "SYNAX: help\n"
  "- lists all commands\n"
  "SYNAX: help <cmd>\n"
  "- gives detailed help on command <cmd>\n"
  "NOTES:\n"
  "- all commands may be shortened, for example 'help', 'hel', 'he', 'h'\n"
  "- sub commands may be shortened, for example 'verify clear' to 'verify c'\n"
  "- normal promp is >>, other prompt indicates streaming mode (see write)\n"
;
  
// All command descriptors
struct cmd_desc_s cmd_descs[] = {
  { cmd_main_help  , "help", "gives help (try 'help help')", cmd_help_longhelp },
  { cmd_main_info  , "info", "application info", cmd_info_longhelp },
  { cmd_main_echo  , "echo", "echo a message (or en/disables echoing)", cmd_echo_longhelp },
  { cmd_main_read  , "read", "read EEPROM memory", cmd_read_longhelp },
  { cmd_main_write , "write", "write EEPROM memory", cmd_write_longhelp },
  { cmd_main_verify, "verify", "verify EEPROM memory", cmd_verify_longhelp },
  { cmd_main_write , "program", "write and verify EEPROM memory", cmd_program_longhelp },
  { cmd_main_erase , "erase", "erases EEPROM memory", cmd_erase_longhelp },
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
  Serial.print("> ");
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
  Serial.println("Error: command not found (try help)"); 
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
      if( cmd_echo ) Serial.print("\b \b");
      cmd_ix--;
    } else {
      // backspace with no more chars in buf; ignore
    }
  } else {
    if( cmd_ix<CMD_BUFSIZE-1 ) {
      cmd_buf[cmd_ix++]= ch;
      if( cmd_echo ) Serial.print((char)ch);
    } else {
      if( cmd_echo ) Serial.print("_");
      // input buffer full
    }
  }
}


// Keyboard =============================================================

// Pins for the keys
#define KEY_UP_PIN A5
#define KEY_DN_PIN A4

// Code for the keys
#define KEY_UP 1
#define KEY_DN 2

int key_prv;
int key_cur;

void key_init( void ) {
  pinMode(KEY_UP_PIN, INPUT);
  pinMode(KEY_DN_PIN, INPUT);
  key_scan();
  key_scan();
}

void key_scan( void ) {
  key_prv= key_cur;
  key_cur= 0;
  if( digitalRead(KEY_UP_PIN) ) key_cur|= KEY_UP;
  if( digitalRead(KEY_DN_PIN) ) key_cur|= KEY_DN;
}

int key_pressed( void ) {
  return ( key_cur ^ key_prv ) & key_cur;
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
  key_init();
  eeprom_init();
}

void loop() {
  // Serial
  int n=0;
  while(1) {
    int ch= Serial.read();
    if( ch==-1 ) break;
    cmd_add(ch);
    if( ++n==SERIAL_RX_BUFFER_SIZE ) { 
      cmd_verify_uartoverflow++; 
      Serial.println(); 
      Serial.println( F("WARNING: uart overflow") ); 
      Serial.println(); 
    }
  }
  // Keys
  key_scan();
  if( key_pressed() & KEY_UP ) eeprom_read( (eeprom_addr_last+1)%EEPROM_SIZE );
  if( key_pressed() & KEY_DN ) eeprom_read( (eeprom_addr_last+EEPROM_SIZE-1)%EEPROM_SIZE );
}
