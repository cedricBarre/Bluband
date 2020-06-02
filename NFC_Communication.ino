#include <SPI.h>

const int IRQ_IN = 7;
const int DIN = 6;

const byte cmdctl= 0;
const byte pollctl = 3;
const byte readctl = 2;
const byte rstctl = 1;

byte * communicate(byte cmd, byte len, byte data[]){
  bool ready = false;
  byte resp = 0;
  
  digitalWrite(SS, LOW);
  SPI.transfer(cmdctl);
  SPI.transfer(cmd);
  SPI.transfer(len);
  for(byte i = 0; i < len; i++){
    SPI.transfer(data[i]);
  }
  digitalWrite(IRQ_IN, HIGH);
  
  delay(1);
  digitalWrite(IRQ_IN, LOW);
  while(!ready){
    resp = SPI.transfer(pollctl);
    resp = resp >> 3;
    if(resp == 1){
      ready = true; 
    }
  }
  digitalWrite(IRQ_IN, HIGH);
  
  delay(1);
  digitalWrite(IRQ_IN, LOW);
  SPI.transfer(readctl);
  resp = SPI.transfer(0);
  byte dlen = SPI.transfer(0);
  byte respData[dlen + 2];
  respData[0] = resp;
  respData[1] = dlen;
  
  for(byte i = 2; i < dlen + 2; i++){
    respData[i] = SPI.transfer(0);
  }
  digitalWrite(IRQ_IN, HIGH);
  return respData;
  
}

void setup() {
  // put your setup code here, to run once:
  
  pinMode(DIN, OUTPUT);
  pinMode(IRQ_IN, OUTPUT);
  digitalWrite(DIN, HIGH);
  digitalWrite(IRQ_IN, HIGH);

  delay(0.1);
  digitalWrite(DIN, LOW);
  delay(0.01);
  digitalWrite(DIN, HIGH);

  SPI.beginTransaction(SPISettings(2000000, MSBFIRST, SPI_MODE0));
  digitalWrite(SS, HIGH);

  // Set protocol
  bool protoSet = false;
  byte *resp;
  byte data[] = {0x01, 0x01};
  while(!protoSet){
    resp = communicate(0x02, 0x02, data);
    if(*(resp + 1) == 0) protoSet = true;
  }


}

void loop() {
  // put your main code here, to run repeatedly:

}
