// Arduino EEPROM programmer (for the AT28C16)
// Features a complete command interpreter

#define PROG_NAME    "Arduino EEPROM programmer"
#define PROG_EEPROM  "AT28C16 2k*8b"
#define PROG_VERSION "3"
#define PROG_DATE    "2019 aug 11"
#define PROG_AUTHOR  "Maarten Pennings"


// EEPROM ===============================================================
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

#define MEGA328_COUNT 32
long mega328_Vcc(void) { 
  long result=0; 
  for( int i=0; i<MEGA328_COUNT; i++ ) result+=mega328_readVcc();
  return (result+MEGA328_COUNT/2)/MEGA328_COUNT;
}


// EEPROM ===============================================================
// Driver for the eeprom

// Number of bytes (actually address locations) in EEPROM
#define EEPROM_SIZE 0x800

// It is assumed that two 74HTC164 shift registers are cascaded.
// They are controlled via EEPROM_PIN_DATA, EEPROM_PIN_CLK of the Nano; they control A0-A10 of the EEPROM
#define EEPROM_PIN_DATA 12
#define EEPROM_PIN_CLK  13
// These pins control the (low-active) WE (write enable) and OE (output=read enable). CE is always enabled
#define EEPROM_PIN_nWE  11
#define EEPROM_PIN_nOE  10
// The data pins of the EEPROM are hooked to these GPIO pins of the Arduino Nano
#define EEPROM_PIN_D0 2
#define EEPROM_PIN_D1 3
#define EEPROM_PIN_D2 4
#define EEPROM_PIN_D3 5
#define EEPROM_PIN_D4 6
#define EEPROM_PIN_D5 7 
#define EEPROM_PIN_D6 8
#define EEPROM_PIN_D7 9
// On my board I have hooked 11 red LEDs to A0-A10, and 8 green LEDs to D0-D7


// Set all data pins to input or output (cached to save some time - is this needed?)
bool eeprom_input_prev;
void eeprom_input( bool input ) {
  if( input==eeprom_input_prev ) return;
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
  eeprom_input_prev= input;
}

// Initialize GPIO pins and shift registers
void eeprom_init() {
  // On EEPROM, set Write Enable to "off" (low active)
  digitalWrite(EEPROM_PIN_nWE,HIGH);
  // On EEPROM, set Output Enable to "off" (low active)
  digitalWrite(EEPROM_PIN_nOE,HIGH);  

  // Set all control pins as output
  pinMode(EEPROM_PIN_DATA, OUTPUT);
  pinMode(EEPROM_PIN_CLK, OUTPUT);
  pinMode(EEPROM_PIN_nWE, OUTPUT);
  pinMode(EEPROM_PIN_nOE, OUTPUT);
  
  // All data pins input
  eeprom_input_prev= false; 
  eeprom_input(true);

  // Go to default: on EEPROM, set Output Enable to "on" (low active) so that data bus LEDs are active
  digitalWrite(EEPROM_PIN_nOE,LOW);

  // Initialize the shift registers (Clears address LEDs)
  eeprom_read(0x000); 
}

// Record the last addr shifted in
uint16_t eeprom_addr_last;

// Reads one byte from address 'addr' from the EEPROM and return it
// Recall, the default mode of the EEPROM is Output Enable is "on" (so that the data LEDs show last value)
uint8_t eeprom_read(uint16_t addr) {
  eeprom_addr_last= addr;
  // On EEPROM, set Output Enable to "off" (low active)
  digitalWrite(EEPROM_PIN_nOE,HIGH);  
  // Switch data pins to input
  eeprom_input(true);
  // Set address lines
  digitalWrite(EEPROM_PIN_CLK, LOW);
  shiftOut(EEPROM_PIN_DATA, EEPROM_PIN_CLK, MSBFIRST, addr>>8 );
  shiftOut(EEPROM_PIN_DATA, EEPROM_PIN_CLK, MSBFIRST, addr&0xff );
  // Go back to default: On EEPROM, set Output Enable to "on" (low active)
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
  digitalWrite(EEPROM_PIN_CLK, LOW);
  shiftOut(EEPROM_PIN_DATA, EEPROM_PIN_CLK, MSBFIRST, addr>>8 );
  shiftOut(EEPROM_PIN_DATA, EEPROM_PIN_CLK, MSBFIRST, addr&0xff );
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
  delayMicroseconds(10000); // "Write Cycle Time" is 1ms, but we need 10: https://youtu.be/K88pgWhEb1M?t=3130
  // Go back to default: On EEPROM, set Output Enable to "on" (low active)
  digitalWrite(EEPROM_PIN_nOE,LOW);
}


// Cmd ==================================================================
// Command interpreter

// Each command has a descriptor, containing a pointer to its handler 'main', the string 'name' that invokes it, and a short help text.
// The main function is much like c's main, it has argc and argv, but the first param is a pointer to its own descriptor.
typedef struct cmd_desc_s cmd_desc_t;
typedef void (*cmd_main_t)( cmd_desc_t * desc, int argc, char * argv[] );
struct cmd_desc_s { cmd_main_t main; char * name; char * help; };

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

// Writes the 'argc' bytes in the hex ascii strings in 'argv[]'.
// The bytes are written to address in global var 'cmd_stream_addr'.
// The reason for the global var is that this function has a stream mode (after "w 200 *" hex numbers can be passed on multiple lines).
// Stream mode is disabled if 'cmd_stream_mode' is '>' otherwise enabled.
// This function not only supports write, but also verify and program, the actual command is stored in 'cmd_stream_desc' 
// (again, global to support stream mode).
char cmd_stream_mode= '>';
struct cmd_desc_s * cmd_stream_desc;
uint16_t cmd_stream_addr;
void cmd_write_verify_prog(int argc, char * argv[] ) {
  int errorcount= 0;
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
      if( data1==data2 ) { Serial.print(F("=")); } else { snprintf(buf,sizeof(buf),"~%02x",data2); Serial.print(buf); errorcount++; }
    }
    cmd_stream_addr++;
  }
  Serial.println("");
  if( cmd_stream_desc->name[0]=='v' && errorcount>0 ) { Serial.print(F("ERROR: ")); Serial.print(cmd_stream_desc->name ); Serial.print(F(": ")); Serial.print(errorcount); Serial.println(F(" fails"));}
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

// The handler for the "write" command, but also for "verify" and "program"
void cmd_main_write(struct cmd_desc_s * desc, int argc, char * argv[] ) {
  if( argc<3 ) { Serial.print(F("ERROR: ")); Serial.print(cmd_stream_desc->name ); Serial.println(F(": must have at least 2 args")); return; }
  // Parse addr
  uint16_t addr;
  if( !cmd_parse(argv[1],&addr) ) { Serial.println(F("ERROR: write/verify : <addr> must be hex")); return; }
  if( addr>=EEPROM_SIZE ) { Serial.print(F("ERROR: ")); Serial.print(cmd_stream_desc->name ); Serial.println(F(": <addr> out of range")); return; }
  // Write data('s)
  cmd_stream_addr= addr;
  cmd_stream_desc= desc;
  cmd_write_verify_prog(argc-2, argv+2 );
}

// The handler for the "info" command
void cmd_main_info(struct cmd_desc_s * desc, int argc, char * argv[] ) {
  Serial.println(F("info: name   : " PROG_NAME));
  Serial.println(F("info: version: " PROG_VERSION));
  Serial.println(F("info: eeprom : " PROG_EEPROM));
  Serial.println(F("info: date   : " PROG_DATE));
  Serial.println(F("info: author : " PROG_AUTHOR));
  Serial.print  (F("info: voltage: ")); Serial.print( mega328_Vcc() ); Serial.println(F("mV"));
  Serial.print  (F("info: cpufreq: ")); Serial.print( F_CPU ); Serial.println(F("Hz"));
}

// All command descriptors
struct cmd_desc_s cmd_descs[] = {
  { cmd_main_help , "help", "list all commands" },
  { cmd_main_read , "read", "read memory locations: read [ <addr> [ <num> ] ]" },
  { cmd_main_write, "write", "write memory locations: write <addr> <val>+ , <val> may be *" },
  { cmd_main_write, "verify", "verify memory locations: verify <addr> <val>+ , <val> may be *" },
  { cmd_main_write, "program", "program (write and verify): program <addr> <val>+ , <val> may be *" },
  { cmd_main_info , "info", "application info" },
  { cmd_main_stty , "stty", "stty settings (echo)" },
};
#define CMD_NUMCMDS (sizeof(cmd_descs)/sizeof(struct cmd_desc_s))

// The handler for the "help" command
void cmd_main_help(struct cmd_desc_s * desc, int argc, char * argv[] ) {
  for( int i=0; i<CMD_NUMCMDS; i++ ) {
    char buf[80];
    snprintf(buf,sizeof(buf),"%-7s - %s", cmd_descs[i].name, cmd_descs[i].help );
    Serial.println(buf);
  }
}

bool cmd_echo= true;
void cmd_main_stty(struct cmd_desc_s * desc, int argc, char * argv[] ) {
  if( argc>2 ) { Serial.println("ERROR: stty: too many arguments"); return; }
  if( argc==2) {
    if(      strcmp(argv[1],"on" )==0 ) cmd_echo= true;
    else if( strcmp(argv[1],"off")==0 ) cmd_echo= false;
    else { Serial.println("ERROR: stty: on|off"); return; }
  }
  Serial.print("stty: "); Serial.println(cmd_echo?"on":"off");
}

// The state machine for receiving characters via Serial
#define CMD_BUFSIZE 128
#define CMD_MAXARGS 32
char cmd_buf[CMD_BUFSIZE];
int cmd_ix;

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
  // Find command
  if( argc==0 ) return; // empty command entered
  ix= 0;
  while( ix<CMD_NUMCMDS && strstr(cmd_descs[ix].name,argv[0])!=cmd_descs[ix].name ) ix++;
  if( ix<CMD_NUMCMDS ) {
    // Found a command. Execute it 
    cmd_stream_mode= '>'; // (clear streaming mode)
    cmd_descs[ix].main(&cmd_descs[ix], argc, argv ); // Execute handler of command
  } else if( cmd_stream_mode!='>' ) {
    // Streamingmode is active
    cmd_write_verify_prog(argc, argv );
  } else { 
    Serial.println("Error: command not found (try help)"); 
    return; 
  }
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

#define KEY_UP_PIN A7
#define KEY_DN_PIN A6

#define KEY_UP 1
#define KEY_DN 2

int key_prv;
int key_cur;

void key_scan( void ) {
  key_prv= key_cur;
  key_cur= 0;
  if( analogRead(KEY_UP_PIN)>=512 ) key_cur|= KEY_UP;
  if( analogRead(KEY_DN_PIN)>=512 ) key_cur|= KEY_DN;
}

void key_init( void ) {
  key_scan();
  key_scan();
}


int key_pressed( void ) {
  return ( key_cur ^ key_prv ) & key_cur;
}


// Main =================================================================


void setup() {
  Serial.begin(115200);
  Serial.println(F("   _____  _____________________"));
  Serial.println(F("  /  _  \\ \\_   _____/\\______   \\"));
  Serial.println(F(" /  /_\\  \\ |    __)_  |     ___/"));
  Serial.println(F("/    |    \\|        \\ |    |"));
  Serial.println(F("\\____|__  /_______  / |____|")); 
  Serial.println(F("        \\/        \\/")); 
  Serial.println("");
  Serial.println(F(PROG_NAME " V" PROG_VERSION));
  Serial.println(F("Type 'help' for help"));
  Serial.println("");
  cmd_init();
  key_init();
  eeprom_init();
}

void loop() {
  // Serial
  while(1) {
    int ch= Serial.read();
    if( ch==-1 ) break;
    cmd_add(ch);
  }
  // Keys
  key_scan();
  if( key_pressed() & KEY_UP ) eeprom_read( (eeprom_addr_last+1)%EEPROM_SIZE );
  if( key_pressed() & KEY_DN ) eeprom_read( (eeprom_addr_last+EEPROM_SIZE-1)%EEPROM_SIZE );
}
