// Clock6502: use a Nano to generate a clock for the 6502
// Connect D2 of Nano to phi0 of 6502. Also connect the grounds.
// You can even supply the 6502 from the 5V0 of the Nano.

// Pulse D2 at max speed: T=6.8us, f=150kHz
#define CLOCK 2

void setup() {
  pinMode(CLOCK, OUTPUT);
  digitalWrite(CLOCK, LOW);
}

void loop() {
  digitalWrite(CLOCK, HIGH);
  digitalWrite(CLOCK, LOW);
}
