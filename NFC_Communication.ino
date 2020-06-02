/*
 * Author(s): Cédric Barré
 * 
 * This file contains the methods and steps to connect the BM019 module to a continuous glucose sensor such as the Dexcom G6 or 
 * the Freestyle Libre. The BM019 module implements the STMicroelectronics CR95HF IC for contactless application and manages the
 * interactions with NFC tags. The connections on the BM019 all go directly to the CR95HF so we will refer to the communication
 * module as the CR95HF. We decided to use SPI communication instead of UART for this project. 
 * 
 * Last Updated: June 2nd 2020
 */


#include <SPI.h>

const int SS_0 = 7; // Pin number to set the communication mode as SPI
const int DIN = 6; // Pin number for the interrupt of the CR95HF

const byte cmdctl= 0; // Control byte to tell the CR95HF that we are sending a command
const byte pollctl = 3; // Control byte to poll the CR95HF to know when the response from the command is ready
const byte readctl = 2; // Control byte to tell the CR95HF to start sending the response 
const byte rstctl = 1; // Control byte to reset the CR95HF

/*
 * This method is used to communicate with the CR95HF from an arduino. It takes in the command for the module to execute,
 * the length in number of bytes of the data that needs to be transmitted and the data attached to the commande 
 * as an array of bytes. It returns the pointer to an array containing the response received by the module after executing 
 * the command. In the array returned, the index 0 contains the response code as a single byte, the index 1 contains the 
 * length in number of bytes of the data from the response and the rest of the array contains the byte of data from the 
 * response.
 */

byte * communicate(byte cmd, byte len, byte data[]){
  bool ready = false;
  byte resp = 0;
  
  digitalWrite(SS, LOW); // Set the Slave Select low to enable the CR95HF module
  SPI.transfer(cmdctl); // Tell the module we are about to send a command
  SPI.transfer(cmd); // Transmit the command 
  SPI.transfer(len); // Transmit the length of the data that we are about to send
  for(byte i = 0; i < len; i++){
    SPI.transfer(data[i]); // Send the data byte by byte 
  }
  digitalWrite(SS, HIGH); // Set the Slave Select high in between interactions with the module
  
  delay(1);
  digitalWrite(SS, LOW); // Set Slave Select low for further interaction with the module
  SPI.transfer(pollctl); // Tell the module we will start polling
  while(!ready){ // As long as the module is not ready to send the response, keep polling
    resp = SPI.transfer(pollctl); // Ask the module if it is ready to respond 
    resp = resp >> 3; // Bit shift the byte received to the right by 3 since the the module will return a '1' at bit 3 of its responde byte when it is ready to respond 
    if(resp == 1){
      ready = true; // Set condition to exit the loop
    }
  }
  digitalWrite(SS, HIGH); // Set the Slave Select high in between interactions with the module
  
  delay(1);
  digitalWrite(SS, LOW); // Set Slave Select low for further interaction with the module
  SPI.transfer(readctl); // Tell the module to send the response
  resp = SPI.transfer(0); // Receive the response code
  byte dlen = SPI.transfer(0); // Receive the length of the data to receive in number of bytes
  byte respData[dlen + 2]; // Make the array to store the response code at index 0, the length of the data at index 1 and the data in the rest of the array
  respData[0] = resp; // Store the response code at index 0 
  respData[1] = dlen; // Store the length of the data at index 1
  
  for(byte i = 2; i < dlen + 2; i++){
    respData[i] = SPI.transfer(0); // Receive the data and store it in the array
  }
  digitalWrite(SS, HIGH); // Set Slave Select high when we are done communicating for the specified command
  return respData;
  
}

void setup() {

  Serial.begin(9600);
  
  pinMode(DIN, OUTPUT); // Set the Interrupt line as an output
  pinMode(SS_0, OUTPUT); // Set the communication select line as an output
  
  // Set the interrupt and communication select lines high 
  digitalWrite(DIN, HIGH); 
  digitalWrite(SS_0, HIGH);

  // Give the CR95HF module a low pulse of 10 microseconds to jumpstart communication selection
  delay(0.1);
  digitalWrite(DIN, LOW);
  delay(0.01);
  digitalWrite(DIN, HIGH);

  // Start the communication with the CR95HF
  SPI.beginTransaction(SPISettings(2000000, MSBFIRST, SPI_MODE0));
  digitalWrite(SS, HIGH);

  Serial.print("SPI set up/n");
  
  // Set the NFC communication protocol for the ISO/IEC 15693 (communication protocol for the Dexcom G6 and Freestyle Libre)
  bool key = false;
  byte *resp; 
  byte protodata[] = {0x01, 0x01};
  while(!key){ // Loop until the protocol has properly been established
    resp = communicate(0x02, 0x02, protodata); // Send the command to set the protocol
    if(*(resp + 1) == 0) {
      key = true; // Check the received data for feedback on the completion of protocol setting
      Serial.print("Protocol setting successful\n");
    }
    else Serial.print("Protocol setting failed, running proceedure again\n");
    delay(1);
  }
  
  key = false;
  // Instantiate the data to send to the module for tag detection 
  byte tagdetdata[] = {0x0B, 0x21, 0x00, 0x79, 0x01, 0x18, 0x00, 0x20, 0x60, 0x60, 0x64, 0x74, 0x3F, 0x08}
  while(!key){ // Loop until a tag has been detected
    resp = communicate(0x07, 0x0E, tagdetdatat);
    if(*(resp + 2) == 0x02){
      key = true; // Check the received data for feedback on the completion of tag detection
      Serial.print("Tag detection successful\n");
    }
    else {
      Serial.print("No tags detected, running proceedure again\n");
      delay(3000);
    }
  }

  key = false;
  // Let's now poll the tag for information and extract its UID and its memory size
  byte infodata[] = {0x02, 0x2B}; // Data for sending information to a tag
  while(!key){ // Loop until there are no errors in the communication
    resp = communicate(0x04, 0x02, infodata);
    if(*(resp + 2) & 0b00001001 == 1){ // Check to make sure there are no errors in the communication due to noise or disturbances
      Serial.print("Errors in communication for information, running procedure again\n");
      delay(2000);
    }
    else if (*(resp + 2) & 0b00001001 == 8 || *(resp + 2) & 0b00001001 == 9){ // Check if the tag has a larger memory than expected
      Serial.print("Protocol was extended, changing the flags for larger memory since the tag has large memory\n");
      infodata[0] = 0x0A; // Change the data sent to the tag for the next loop to accommodate larger memory 
    }
    else { // Else, there are no error or complications
      key = true;
      Serial.print("No errors during communication\n");
      
      // Print the UID
      Serial.print("UID of the tag:\n");
      for(int i = 9, i > 1, i--){
        Serial.print("0x%d", *(resp + i + 2));
      }
      Serial.print("\n");

      // Print the memory size if the information was available
      if(*(resp + 3) & 0b00000100 != 0){
        if(infodata[0] == 2){
          Serial.print("The device has %d blocks of %d bytes of memory", (*(resp + 14) + 1), (*(resp + 15) + 1));
        }
        else Serial.print("The device has %d blocks of %d bytes of memory", (*(resp + 15) * sq(16) + *(resp + 14) + 1), (*(resp + 16) + 1));
      }
      else Serial.print("Could not access the memory size of the device");
    }
  }
  


}

void loop() {

}
