
#define RST  8
#define CLK  9
#define DATA 10

#define PSC1 0xff
#define PSC2 0xff

uint8_t card_data[] = {

0xff, 0xf7, 0x5c, 0xff, 0xff, 0xff, 0xff, 0xff, 
0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x47, 0x38, 
0x78, 0x28, 0x07, 0x8c, 0xc1, 0x03, 0xfe, 0xff, 
0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00,

};

void setup()
{
  digitalWrite(RST, LOW);
  pinMode(RST, OUTPUT);
  digitalWrite(CLK, LOW);
  pinMode(CLK, OUTPUT);
  pinMode(DATA, INPUT_PULLUP);
  digitalWrite(DATA, HIGH);

  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for Leonardo only
  }
  Serial.readStringUntil('\m');
}

void loop()
{
again:
  Serial.println("Press 'a' for ATR, 'r' to read card, 'w' to program card, 'u' to unlock card");
  Serial.println("-> You must read card ('r') before you are able to write to card.");
  while (42) {
    if (Serial.available() == 0)
      continue;
    char inChar = Serial.read();
    if (inChar == 'a') {
      Serial.println("ATR...");
      card_atr(52);
      goto again;
    } else if (inChar == 'r') {
      Serial.println("reading...");
      card_read(52, 0);
      goto again;
    } else if (inChar == 'w') {
      Serial.println("writing...");
      card_write(card_data, sizeof(card_data), 0);
      goto again;
    } else if (inChar == 'u') {
      Serial.println("unlocking...");
      card_unlock(PSC1, PSC2);
      goto again;
    } else {
      goto again;
    }
  }
}

void card_atr(int num)
{
  uint8_t data[num];
  
  digitalWrite(RST, HIGH);
  delayMicroseconds(100);
  digitalWrite(CLK, HIGH);
  delayMicroseconds(100);
  digitalWrite(CLK, LOW);
  delayMicroseconds(100);
  digitalWrite(RST, LOW);
  delayMicroseconds(100);
  card_read_bytes(data, num);
}

void card_read(int num, int address)
{
  uint8_t data[num];
  
  card_command(((address >> 2) & 0xc0) | 0x0e, address & 0xff, 0);
  digitalWrite(CLK, HIGH);
  delayMicroseconds(100);
  digitalWrite(CLK, LOW);
  delayMicroseconds(100);
  card_read_bytes(data, num);
}

void card_write(uint8_t *data, int num, int address)
{
  int i, cnt;
  
  for (i = 0; i < num; i++) {
    card_command(((address >> 2) & 0xc0) | 0x33, address & 0xff, *data);
    cnt = card_erase_and_write();
    if (cnt <= 2) {
      Serial.println("write failed!");
//      break;
    }
    address++;
    data++;
  }
}

void card_unlock(uint8_t psc1, uint8_t psc2)
{
  int i;
  uint8_t data[3];
  
  // read error counter
  card_command(0xce, 253, 0);
  digitalWrite(CLK, HIGH);
  delayMicroseconds(100);
  digitalWrite(CLK, LOW);
  delayMicroseconds(100);
  Serial.println("3 bytes: error counter mask, first PSC code, second psc code");
  card_read_bytes(data, 3);
  // check bit to erase
  for (i = 0; i < 8; i++) {
     if ((data[0] & (1 << i)))
       break;
  }
  if (i == 8) {
    Serial.println("SORRY NO MORE BITS TO ERASE TO UNLOCK, YOUR CARD IS BRICKED!");
    return;
  }
  data[0] = data[0] - (1 << i);
  // ease bit to unlock
  card_command(0xf2, 253, data[0]);
  card_erase_and_write(); 
  // unlock
  Serial.println("unlock bit has been erased, sending PSC code");
  card_command(0xcd, 254, psc1);
  card_erase_and_write(); 
  card_command(0xcd, 255, psc2);
  card_erase_and_write(); 
  // read error counter mask
  card_command(0xce, 253, 0);
  digitalWrite(CLK, HIGH);
  delayMicroseconds(100);
  digitalWrite(CLK, LOW);
  delayMicroseconds(100);
  Serial.println("checking error counter mask, one bit shall be erased...");
  card_read_bytes(data, 1);
  // reset unlock mask
  Serial.println("PSC code has been sent, resetting error counter mask");
  card_command(0xf3, 253, 0xff);
  card_erase_and_write();
  // read error counter mask
  card_command(0xce, 253, 0);
  digitalWrite(CLK, HIGH);
  delayMicroseconds(100);
  digitalWrite(CLK, LOW);
  delayMicroseconds(100);
  Serial.println("reading: error counter mask, all bits should be reset");
  card_read_bytes(data, 1);
}

void card_command(uint8_t c1, uint8_t c2, uint8_t c3)
{
  int i, j;
  uint8_t c[3];

  c[0] = c1;
  c[1] = c2;
  c[2] = c3;
  Serial.println("card command:");
  Serial.println(c1);
  Serial.println(c2);
  Serial.println(c3);
  
  digitalWrite(RST, HIGH);
  delayMicroseconds(100);
  pinMode(DATA, OUTPUT);
  for (i = 0; i < 3; i++) {
    for (j = 0; j < 8; j++) {
      digitalWrite(DATA, ((c[i] >> j) & 1) ? HIGH : LOW);
      delayMicroseconds(100);
      digitalWrite(CLK, HIGH);
      delayMicroseconds(100);
      digitalWrite(CLK, LOW);
      delayMicroseconds(100);
    }
  }
  pinMode(DATA, INPUT_PULLUP);
  digitalWrite(DATA, HIGH);
  digitalWrite(RST, LOW);
  delayMicroseconds(100);
}

void card_read_bytes(uint8_t *data, int num) {
  int i, j;
  
  for (i = 0; i < num; i++) {
    for (j = 0; j < 8; j++) {
      data[i] = (data[i] >> 1) | ((digitalRead(DATA) != LOW) ? 128 : 0);
      Serial.print((digitalRead(DATA) != LOW) ? '1' : '0');
      digitalWrite(CLK, HIGH);
      delayMicroseconds(100);
      digitalWrite(CLK, LOW);
      delayMicroseconds(100);
    }
    Serial.print(" 0x");
    Serial.println(data[i], HEX);
  }
}

int card_erase_and_write(void)
{
  int i;

  for (i = 0; i < 203; i++) {
    digitalWrite(CLK, HIGH);
    delayMicroseconds(100);
    digitalWrite(CLK, LOW);
    delayMicroseconds(100);
if (0)    if (digitalRead(DATA) == LOW) {
      Serial.print(" -> write pulses:");
      Serial.println(i);
      break;
    }
  }
  return i;
}

