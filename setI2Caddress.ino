// setI2Caddress
// Melexis MLX90614 Infra Red Thermometer in TO-39
// sets I2C slave address 
//
// adapted by Wolfgang Korosec
// Nick Gammon code and suggestions put together from discussion on 
// http://forum.arduino.cc/index.php?topic=58509.20
//
// tested on Teensy 3.1


#include <Wire.h>

// CRC-8 with polynomial X8+X2+X1+1. 
// Written by Nick Gammon
// 22 April 2011

byte crc8(byte *addr, byte len)
{
 byte crc = 0;
 while (len--) {
   byte inbyte = *addr++;
   for (byte i = 8; i; i--) 
   {
     byte carry = (crc ^ inbyte) & 0x80;
     crc <<= 1;
     if (carry) 
       crc ^= 0x7;
     inbyte <<= 1;
   }
 }
 return crc;
}  // end of crc8


#define ORIGINAL_ADDRESS 0x0               // original address (any device responds to 0x00)
#define NEW_ADDRESS 0x5B                    // address to change to (factory default is 0x5A)
#define WRITE_ADDRESS (0b00100000 | 0x0E)   // command to change the address

void setup ()
{
byte crc;
int status;

  Serial.begin (115200);
  while (!Serial);
  Serial.println ("Starting...");
  Wire.begin ();

  byte buf [4] = { 
    ORIGINAL_ADDRESS << 1, WRITE_ADDRESS, 0, 0  };
    
  crc = crc8 (buf, sizeof buf);

  // erase EEPROM
  Wire.beginTransmission(ORIGINAL_ADDRESS);  // original device address
  Wire.write (buf [1]);              // write to EEPROM address 0x0E (device address)
  Wire.write (buf [2]);              // data byte, low byte
  Wire.write (buf [3]);              // data byte, high byte
  Wire.write (crc);                  // PEC (packet error code)
  status = Wire.endTransmission();
  delay (5);  // give it time to write

  Serial.print ("New I2C address: ");
  Serial.println (NEW_ADDRESS, HEX);
  Serial.print ("Status from erase was: ");
  Serial.println (status, DEC);


  buf [2] = NEW_ADDRESS;
    
  crc = crc8 (buf, sizeof buf);
  
  // write new address
  Wire.beginTransmission(ORIGINAL_ADDRESS);  // original device address
  Wire.write (buf [1]);              // write to EEPROM address 0x0E (device address)
  Wire.write (buf [2]);              // data byte, low byte
  Wire.write (buf [3]);              // data byte, high byte
  Wire.write (crc);                  // PEC (packet error code)
  status = Wire.endTransmission();
  delay (5);  // give it time to write
  
  Serial.print ("Status from address change was: ");
  Serial.println (status, DEC);
  
}  // end of setup

void loop() {
  // put your main code here, to run repeatedly:
  Serial.println ("+++ finished +++");
  Serial.println ("==================================================================");  
  delay (3000);
  while (true){
    Serial.println ("if status from erase and change was '0' the new I2C adress is set");
    Serial.println ("switch OFF and ON the MLX90614 device to make the change active");
    Serial.println ("==================================================================");
    delay (10000);
  }

}
