/*
Rovac clock control
version 1.0

SPI: 
10 (SS),   // chip select
11 (MOSI), // data out
12 (MISO), // not used
13 (SCK)   // clock
*/

#include <Wire.h>
#include <Time.h>
#include <TimeLib.h>
#include <DS3232RTC.h>
#include <EEPROM.h>
#include <SPI.h>

// set pin 10 as the slave select for the DAC:
const int slaveSelectPin = 10;

float offset = 0;
float gradient = 1.0;

int hours = 12;
int mins = 0;

String command = "";         // a string to hold incoming data
boolean commandComplete = false;  // whether the string is complete

char buffer[200];

void setup() 
{
   pinMode(slaveSelectPin, OUTPUT);
   digitalWrite(slaveSelectPin, HIGH);
   
   SPI.begin();
   Serial.begin(9600);

   EEPROM.get(0, offset);
   EEPROM.get(sizeof(float), gradient);
      
   PowerOnDAC();
   UpdateTime();
}

void loop() 
{ 
  tmElements_t tm;
  RTC.read(tm);  
  
  if( tm.Minute != mins)
  {
     UpdateTime();
  }

  if (commandComplete) 
  {
     if(command == "c")
     {
        Calibrate();
     }
     else if(command.startsWith("t"))
     {
        tmElements_t tm;
        tm.Hour = command.substring(1,3).toInt();      
        tm.Minute = command.substring(3,5).toInt();      
        tm.Second = command.substring(5,7).toInt();      
        tm.Day = command.substring(7,9).toInt();      
        tm.Month = command.substring(9,11).toInt();      
        tm.Year = command.substring(11,15).toInt() - 1970;   
           
        sprintf(buffer, "Setting time to: %dh %dm %ds %dd %dmon %dy",
          tm.Hour, tm.Minute, tm.Second, tm.Day, tm.Month, tm.Year + 1970); 
        Serial.println(buffer);
        
        RTC.write(tm);    
     }
     else if(command.startsWith("m"))
     {
        int m =  command.substring(1).toInt();  
        tmElements_t tm;
        RTC.read(tm);  
        tm.Minute = m;
        tm.Second = 0;
        RTC.write(tm);
     }
     else
     {
        // show help
        Serial.println("c - calibrate (follow instructions)");
        Serial.println("m20 - set minute to 20 (and 0 seconds)");
        Serial.println("t20340015022016 - set time to 20:34:00 15/02/2016 (use 24 hr format)");      
     }
     
     UpdateTime();
     
     command = "";
     commandComplete = false;
  } 
  
  delay(1000);
}

void UpdateTime()
{
   float t = GetTimeAsVoltage();
   Serial.print("Time: "); Serial.println(t);
   float voltage = ScaleVoltage( t);
   SetDACVoltage( voltage); 
}

float SerialReadFloat()
{
  while (Serial.available()==0);
  float f = Serial.parseFloat();
  while (Serial.available()){Serial.read();}

  return f;
}

int SerialReadInt()
{
  while (Serial.available()==0);
  int i = Serial.parseInt();
  while (Serial.available()){Serial.read();}

  return i;
}

float GetTimeAsVoltage()
{   
   tmElements_t tm;
   RTC.read(tm);
   hours = tm.Hour;

   if( hours > 12 ) 
      hours -= 12;
   if( hours == 0) 
      hours = 1;

   mins = tm.Minute;
  
   return hours + mins /100.0;
}

float ScaleVoltage( float t)
{
   return (t - offset) / gradient;
}

void SetDACVoltage(float voltage) 
{
  unsigned int code = voltage * 4096 / 30;
  
  byte msb = 0x10 | (code >> 8 & 0x0f);
  byte lsb = code & 0xff;
  //Serial.print("cal voltage="); Serial.println(voltage);

  // take the SS pin low to select the chip:
  digitalWrite(slaveSelectPin, LOW);
  //  send in the address and value via SPI:
  SPI.transfer(msb);
  SPI.transfer(lsb);
  // take the SS pin high to de-select the chip:
  digitalWrite(slaveSelectPin, HIGH);
}

void PowerOnDAC() 
{
  byte msb = 0x70;
  byte lsb = 0x44;

  // take the SS pin low to select the chip:
  digitalWrite(slaveSelectPin, LOW);
  //  send in the address and value via SPI:
  SPI.transfer(msb);
  SPI.transfer(lsb);

  // need a NOP after control register write
  SPI.transfer(0x00);
  SPI.transfer(0x00);
 
  // take the SS pin high to de-select the chip:
  digitalWrite(slaveSelectPin, HIGH);
}

void Calibrate()
{
  SetDACVoltage(1.0);
  Serial.println("Enter the displayed voltage");
  float v1 = SerialReadFloat();
  
  SetDACVoltage(12.0);
  Serial.println("Enter the displayed voltage");
  float v12 = SerialReadFloat();
  
  gradient = (v1 + v12) /13;
  offset = 1 - (v1 / gradient);

  EEPROM.put(0, offset);
  EEPROM.put(sizeof(float), gradient);

  Serial.print("Calibrated. o=");Serial.print(offset);Serial.print(" g=");Serial.println(gradient);
}

void serialEvent() 
{
  while (Serial.available()) 
  {
    char inChar = (char)Serial.read(); 
   
    if (inChar == '\n') 
    {
      commandComplete = true;     
    } 
    else   
      command += inChar;
  }
}


