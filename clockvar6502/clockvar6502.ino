// clockvar6502 - a variable clock for the 6502 
// Connect D2 of Nano to phi0 of 6502. Also connect the grounds.
// You can even supply the 6502 from the 5V0 of the Nano (and stub the inputs).

// Pulse D2. When wait_us=1 the clock is ~100kHz
#define CLOCK 2

// Each period has a length of approximately wait_us 
uint32_t wait_us= 4096;

void wait_print() {
  // The fixed part of loop() takes about 10us
  Serial.print("10+"); Serial.print(wait_us); Serial.print("us, "); 
  Serial.print(1000000.0/(10+wait_us)); Serial.println("Hz");
}

void wait() {
  uint32_t w= wait_us;
  // Unfortunately, delayMicroseconds() takes a unit16_t, not a uint32_t
  while( w>=10000L) { delayMicroseconds(10000L); w-=10000L; }
  delayMicroseconds(w); 
}

void wait_adjust() {
  int ch=Serial.read();
  if( ch==-1 ) return;
  if( ch=='+' ) {
    wait_us= wait_us / 2L;
    if( wait_us<1 ) wait_us= 1;  
    wait_print();
  } else if( ch=='-' ) {
    wait_us= wait_us * 2L;
    if( wait_us>16777216L ) wait_us= 16777216L;  
    wait_print();
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("Welcom to clockvar6502");
  Serial.println("press + or - to speed up or slow dow");
  
  pinMode(CLOCK, OUTPUT);
  digitalWrite(CLOCK, HIGH);

  wait_print();
}

void loop() {
  wait_adjust();
  digitalWrite(CLOCK, LOW);
  // no wait, low period needs to be short
  digitalWrite(CLOCK, HIGH);
  wait();
}
