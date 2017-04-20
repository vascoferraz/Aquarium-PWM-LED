/*****************************************************************************
* Copyright (C) 2012-2014 by Vasco Ferraz. All Rights Reserved.              *
*                                                                            *
* This program is free software: you can redistribute it and/or modify       *
* it under the terms of the GNU General Public License as published by       *
* the Free Software Foundation, either version 3 of the License, or          *
* (at your option) any later version.                                        *
*                                                                            *
* This program is distributed in the hope that it will be useful,            *
* but WITHOUT ANY WARRANTY; without even the implied warranty of             *
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the               *
* GNU General Public License for more details.                               *
*                                                                            *
* You should have received a copy of the GNU General Public License          *
* along with this program. If not, see <http://www.gnu.org/licenses/>.       *
*                                                                            *
*  Author:        Vasco Ferraz                                               *
*  Contact:       http://vascoferraz.com/contact/                            *
*  Description:   http://vascoferraz.com/projects/aquarium-pwm-led/          *
*****************************************************************************/

//#include <Servo.h> <-- If you include Servo.h, which uses timer1, ShiftPWM will automatically switch to timer2

// Clock and data pins are pins from the hardware SPI, you cannot choose them yourself.
// Data pin is MOSI (Uno and earlier: 11, Leonardo: ICSP 4, Mega: 51, Teensy 2.0: 2, Teensy 2.0++: 22) 
// Clock pin is SCK (Uno and earlier: 13, Leonardo: ICSP 3, Mega: 52, Teensy 2.0: 1, Teensy 2.0++: 21)

// You can choose the latch pin yourself.
const int ShiftPWM_latchPin=8;

// ** uncomment this part to NOT use the SPI port and change the pin numbers. This is 2.5x slower **
//#define SHIFTPWM_NOSPI
//const int ShiftPWM_dataPin = 11;
//const int ShiftPWM_clockPin = 13;

// If your LED's turn on if the pin is low, set this to true, otherwise set it to false.
const bool ShiftPWM_invertOutputs = false;

// You can enable the option below to shift the PWM phase of each shift register by 8 compared to the previous.
// This will slightly increase the interrupt load, but will prevent all PWM signals from becoming high at the same time.
// This will be a bit easier on your power supply, because the current peaks are distributed.
const bool ShiftPWM_balanceLoad = false;

#include <Wire.h> // I2C and TWI library
#include <LiquidCrystal.h>
#include <RTClib.h>
#include <ShiftPWM.h> // Include ShiftPWM.h after setting the data, clock and latch pins!
#include <EEPROM.h>   // The microcontroller on the Arduino board has an EEPROM: memory whose values are kept when the board is turned off
                      // (like a tiny hard drive). This library enables you to read and write those bytes.
                      // The microcontrollers on the various Arduino boards have different amounts of EEPROM: 1024 bytes on the ATmega328P,
                      // 512 bytes on the ATmega168 and ATmega8 and 4 kB (4096 bytes) on the ATmega1280 and ATmega2560. 

#define DS1307_I2C_ADDRESS 0x68 // Each I2C object has a unique bus address, the DS1307 (Real Time Clock) is 0x68

RTC_DS1307 RTC;
LiquidCrystal lcd(7, 6, 5, 4, 3, 2);

// Create custom chars
byte degree_char[8] = {
  0b01110,
  0b10001,
  0b10001,
  0b01110,
  0b00000,
  0b00000,
  0b00000,
  0b00000
};

byte upbar_char[8] = {
  0b11111,
  0b00000,
  0b00000,
  0b00000,
  0b00000,
  0b00000,
  0b00000,
  0b00000
};

// Variables used to measure the temperature
const int analogtemp=6; // This is the analog pin which is measuring the input voltage from the LM35CAZ temperature sensor
double temp=0, Vin=0, ADCvar;
unsigned int j=0, k=0;
const double Vref=1100.0;

const int ClockMode = 1;          // Pushbutton to switch between modes
unsigned char ClockModeState = 0; // Current LCD mode state
unsigned char ClockModeFlag = 0;  // Flag used for debouncing the ClockMode pushbutton
const int SetClockPlus = 2;       // Increment pushbutton to set the clock

unsigned char second, minute, hour, dayOfWeek, day, month, year;
unsigned char presunrise; // Hour when the pre-sunrise will start
unsigned char sunrise;    // Hour when the sunrise will start
unsigned char sunset;     // Hour when the sunset will start
unsigned int presunriseMemoryBank = 0; // This is the position on ATmega328P's EEPROM where the hour of the pre-sunrise is stored.
unsigned int sunriseMemoryBank = 1;    // This is the position on ATmega328P's EEPROM where the hour of the sunrise is stored.
unsigned int sunsetMemoryBank = 2;     // This is the position on ATmega328P's EEPROM where the hour of the sunset is stored.
const unsigned int settingdelay = 200; // The higher this value is, the longer it will take to increment day, month, year, hour, minute, second, pre-sunrise, sunrise and sunset variables.


// If there is no input, automatically jump to main display when gotomain = gotomaincounter. Change this time by manipulating the gotomaincouter value.
unsigned int gotomain=0;
const int gotomaincounter=400;

// Convert normal decimal numbers to binary coded decimal
byte decToBcd(byte val) {return ( (val/10*16) + (val%10) );}

// Convert binary coded decimal to normal decimal numbers
byte bcdToDec(byte val) {return ( (val/16*10) + (val%16) );}


// Here you set the number of brightness levels, the update frequency and the number of shift registers.
// These values affect the load of ShiftPWM.
// Choose them wisely and use the PrintInterruptLoad() function to verify your load.
unsigned int maxBrightness = 240; // Don't forget that a signed char variable range is from [-128;127] and an unsigned char from [0;255].
unsigned int brightness;          // If you want to get an higher resolution, you must set maxBrightness (here) and brightness variables as integers.
unsigned int pwmFrequency = 100;
unsigned char numRegisters = 1;
const int outputEnable = 9; // If this port is LOW, shift register(s) is (are) enable. If this port is HIGH, shift register(s) is (are) disable.

// Variables used in Copyright function
unsigned char x=0, z=0, flag=0;

// Variables used in Brightness and LED_PWM functions
int y=-1;
unsigned int percent;
unsigned char WhiteString1 = 0;
unsigned char WhiteString2 = 1;
unsigned char BlueString   = 2;
unsigned char RedString    = 3;
unsigned char Brightness_WhiteString1 = 0;
unsigned char Brightness_WhiteString2 = 0;
unsigned char Brightness_BlueString   = 0;
unsigned char Brightness_RedString    = 0;

// Variable used to change between operation modes
unsigned char operationMode = 0; // Start on normal mode


void setup ()
{
  Serial.begin(9600);
  Wire.begin();
  RTC.begin();
  lcd.begin(20, 4);
  lcd.createChar(0, degree_char);
  lcd.createChar(1, upbar_char);
  analogReference(INTERNAL);
  
  pinMode(ClockMode, INPUT);      // Initialize the clock mode pushbutton as an input.
  pinMode(SetClockPlus, INPUT );  // Initialize the increment pushbutton as an input.
  pinMode(outputEnable, OUTPUT);  // Initialize the output enable shift register pin as an output.
  
  // Sets the number of 8-bit shift registers that are used.
  ShiftPWM.SetAmountOfRegisters(numRegisters); 
  // Sets the pwmFrequency and maxBrightness
  ShiftPWM.Start(pwmFrequency,maxBrightness);
  
  // This set of instructions will disable all outputs from all shift registers to prevent the random (or last saved on/off state) activation of the LEDs.
  digitalWrite(outputEnable, HIGH);
  ShiftPWM.SetAll(0);
  delay(10);
  digitalWrite(outputEnable, LOW);
  
  // following line sets the RTC to the date & time this sketch was compiled 
  //RTC.adjust(DateTime(__DATE__, __TIME__));

  // If the clock is not running, execute the following code
  if (! RTC.isrunning())
  {    
    lcd.clear();
    lcd.home();    
    lcd.print("Not Running!!");
    lcd.setCursor(0,1);
    lcd.print("Restarting...");
    delay(5000);
    lcd.clear();
    
    // Start ticking the clock
    Wire.beginTransmission(DS1307_I2C_ADDRESS);
    Wire.write(0x00); // move pointer to 0x00 byte address
    Wire.write(0x00); // sends 0x00. The whole byte is set to zero (0x00). This also means seconds will reset!! Unless you use a mask -> homework :)
    Wire.endTransmission();    
    // following line sets the RTC to the date & time to: 2014 August 01 - 00:00:00
    RTC.adjust(DateTime(2014,8,1, 0,0,0)); // sequence: year, month, day,  hour, minute, second   
  }
  
    // Pre-sunrise out of range. If for some reason the stored value (pre-sunrise) in ATmega328P's EEPROM is out of range it will be set to a valid hour
    presunrise = EEPROM.read(presunriseMemoryBank);
    if (presunrise >= 6 && presunrise <= 9) {presunrise=presunrise;}
    else (presunrise = 9); // Default pre-sunrise value    
    EEPROM.write(presunriseMemoryBank,presunrise);
    
    // Sunrise out of range. If for some reason the stored value (sunrise) in ATmega328P's EEPROM is out of range it will be set to a valid hour
    sunrise = EEPROM.read(sunriseMemoryBank);
    if (sunrise >= 10 && sunrise <= 18) {sunrise=sunrise;}
    else (sunrise = 14); // Default sunrise value
    EEPROM.write(sunriseMemoryBank,sunrise);
    
    // Sunset out of range. If for some reason the stored value (sunset) in ATmega328P's EEPROM is out of range it will be set to a valid hour
    sunset = EEPROM.read(sunsetMemoryBank);
    if (sunset >= 19 && sunset <= 23) {sunset=sunset;}
    else (sunset = 23); // Default sunset value
    EEPROM.write(sunsetMemoryBank,sunset);
}


void loop () 
{
  
  while (ClockModeState == 0)
  { 
    
    PrintTimeOnLCD();
    PrintDateOnLCD();
    PrintWeekDayOnLCD();
    PrintTemperatureOnLCD();
    PrintCopyrightOnLCD();
    ReadTime();
    
    
    if(!Serial.available())
    {
     Serial.println(operationMode);
     switch(operationMode)
     {
      case 0: LED_PWM(); break; 
      case 1: LED_PWM_Debug(); break;            
      default: LED_PWM(); break;
     }
    }
    
    if(Serial.available())
    {
     operationMode = Serial.parseInt(); // read a number from the serial port to set the operation mode
     switch(operationMode)
     {
      case 0: LED_PWM(); break; 
      case 1: LED_PWM_Debug(); break;            
      default: LED_PWM(); break;
     }
    }
   
    PrintBrightnessOnLCD(); 
                   
    SwitchClockMode (1);
  }
  
  while (ClockModeState == 1)
  {
    SetHour();
  }
  
  while (ClockModeState == 2)
  {
    SetMinute();
  }

  while (ClockModeState == 3)
  {
    SetSecond();
  }

  while (ClockModeState == 4)
  {
    SetDay();
  }
  
  while (ClockModeState == 5)
  {
    SetMonth();
  }
  
  while (ClockModeState == 6)
  {
    SetYear();
  }
  
  while (ClockModeState == 7)
  {
    SetPresunrise();
  }
  
  while (ClockModeState == 8)
  {
    SetSunrise();
  }
  
  while (ClockModeState == 9)
  {
    SetSunset();
  }
  
}


// Print time on LCD
void PrintTimeOnLCD (void)
{
  lcd.home();
  DateTime now = RTC.now();
  if (now.hour() < 10)
  {
    lcd.print('0');
    lcd.print(now.hour(), DEC); 
    lcd.print(':');
  }
  else
  {
    lcd.print(now.hour(), DEC); 
    lcd.print(':');
  }

  if (now.minute() < 10)
  {
    lcd.print('0');
    lcd.print(now.minute(), DEC); 
    lcd.print(':');
  }
  else
  {
    lcd.print(now.minute(), DEC); 
    lcd.print(':');
  }

  if (now.second() < 10)
  {
    lcd.print('0');
    lcd.print(now.second(), DEC);
  }  
  else
  {
    lcd.print(now.second(), DEC);
  }
}


// Print date on LCD
void PrintDateOnLCD(void)
{

  DateTime now = RTC.now();
  lcd.setCursor(0,1);

  if (now.day() < 10)
  {
    lcd.print('0'); 
    lcd.print(now.day(), DEC); 
    lcd.print('/');
  }
  else
  {
    lcd.print(now.day(), DEC); 
    lcd.print('/');
  }


  switch(now.month())
  {
  case 1:  lcd.print("Jan/"); break;     
  case 2:  lcd.print("Feb/"); break;
  case 3:  lcd.print("Mar/"); break;
  case 4:  lcd.print("Apr/"); break;
  case 5:  lcd.print("May/"); break;
  case 6:  lcd.print("Jun/"); break;
  case 7:  lcd.print("Jul/"); break;
  case 8:  lcd.print("Aug/"); break;     
  case 9:  lcd.print("Sep/"); break;
  case 10: lcd.print("Oct/"); break;
  case 11: lcd.print("Nov/"); break;
  case 12: lcd.print("Dec/"); break;
  default: lcd.print("Err/");
  }
  
  lcd.print(now.year(), DEC); 
}


// Print weekday on LCD
void PrintWeekDayOnLCD(void)
{

  DateTime now = RTC.now();
  lcd.setCursor(11, 1);
  switch(now.dayOfTheWeek())
  {
  case 0:
    lcd.print("  Sun"); // Noise can write random chars on the LCD. Adding two blank chars before the WeekDay will prevent these random chars.
    Wire.beginTransmission(DS1307_I2C_ADDRESS); Wire.write(0x03); Wire.write(decToBcd(0)); Wire.endTransmission();
    break;
    
  case 1:
    lcd.print("  Mon"); // Noise can write random chars on the LCD. Adding two blank chars before the WeekDay will prevent these random chars.
    Wire.beginTransmission(DS1307_I2C_ADDRESS); Wire.write(0x03); Wire.write(decToBcd(1)); Wire.endTransmission();
    break;
    
  case 2:
    lcd.print("  Tue"); // Noise can write random chars on the LCD. Adding two blank chars before the WeekDay will prevent these random chars.
    Wire.beginTransmission(DS1307_I2C_ADDRESS); Wire.write(0x03); Wire.write(decToBcd(2)); Wire.endTransmission();
    break;
    
  case 3:
    lcd.print("  Wed"); // Noise can write random chars on the LCD. Adding two blank chars before the WeekDay will prevent these random chars.
    Wire.beginTransmission(DS1307_I2C_ADDRESS); Wire.write(0x03); Wire.write(decToBcd(3)); Wire.endTransmission();
    break;
    
  case 4:
    lcd.print("  Thu"); // Noise can write random chars on the LCD. Adding two blank chars before the WeekDay will prevent these random chars.
    Wire.beginTransmission(DS1307_I2C_ADDRESS); Wire.write(0x03); Wire.write(decToBcd(4)); Wire.endTransmission();
    break;
    
  case 5:
    lcd.print("  Fri"); // Noise can write random chars on the LCD. Adding two blank chars before the WeekDay will prevent these random chars.
    Wire.beginTransmission(DS1307_I2C_ADDRESS); Wire.write(0x03); Wire.write(decToBcd(5)); Wire.endTransmission();
    break;
    
  case 6:
    lcd.print("  Sat"); // Noise can write random chars on the LCD. Adding two blank chars before the WeekDay will prevent these random chars.
    Wire.beginTransmission(DS1307_I2C_ADDRESS); Wire.write(0x03); Wire.write(decToBcd(6)); Wire.endTransmission();
    break;
    
  default:
    lcd.print("  Err"); // Noise can write random chars on the LCD. Adding two blank chars before the WeekDay will prevent these random chars.
  }
  
}


// Print temperature and miliVolt on LCD
void PrintTemperatureOnLCD(void)
{
  
  if (k == 0)
  {
   ADCvar=0;
   Vin=0;
   temp=0;
   
   for(j=0 ; j<=99 ; j++) // Do "j" readings
   {ADCvar = ADCvar + (analogRead(analogtemp));} // Each sample is a value from 0 to 1023. Reading "j" values will help making the reading more accurate.
    
     ADCvar=ADCvar/100; // Calculate the average value from all "j" readings.
     Vin = ADCvar * (Vref/1024.0);  // Here you convert from ADC to milivolt -> Voltage = ADCvar*Vref/1024. ADC = [0;1023]
     // For a 10-bit ADC with an 1100mV reference, the most you can measure without saturating the ADC would be (1100mV - 1100mV/1024) = 1098.9mV.
     // In other words, a reading of 1023 from your ADC equals (1023 * 1100/1024mV) equals 1098.9 mV.
     // Remember, 10 bits can only represent a value as large as b1111111111 or decimal 1023.
       
    lcd.setCursor(12, 3); lcd.print(Vin, 2); lcd.print("mV"); // Print miliVolt on LCD
    
    temp = (Vin/10.0); // Convert Vin into temperature.

    if ( (temp >=0) & (temp <10) )
    {
      lcd.setCursor(8, 0);
      lcd.print("   ");
      lcd.print(temp, 1);   
    }

    if ( (temp >=10) & (temp <100) )
    {
      lcd.setCursor(8, 0);
      lcd.print("  ");
      lcd.print(temp, 1);
    }
    
    if (temp >=100)
    {
      lcd.setCursor(8, 0);
      lcd.print("    Hi");
    }
    
    if ( (temp >-10) & (temp <0) )
    {
      lcd.setCursor(8, 0);
      lcd.print("  ");
      lcd.print(temp, 1);
    }
    
     if (temp <=-10)
    {
      lcd.setCursor(8, 0);
      lcd.print("    Lo"); 
    }
    
    lcd.setCursor(14, 0);
    lcd.write((uint8_t)0);
    lcd.print("C");    
    
  }
 
 k++; if (k >= 120){k=0;} 
}


// Print brightness on LCD
void PrintBrightnessOnLCD(void)
{
   y++;
   
   if (y == 0)
   {
   lcd.setCursor(0, 3);
  
   if (Brightness_WhiteString1 >= 0 && Brightness_WhiteString1 < 10)
   {lcd.print("W1:00"); lcd.print(Brightness_WhiteString1); lcd.print("/"); lcd.print(maxBrightness);}
   
   else if (Brightness_WhiteString1 >= 10 && Brightness_WhiteString1 < 100)
   {lcd.print("W1:0"); lcd.print(Brightness_WhiteString1); lcd.print("/"); lcd.print(maxBrightness);}
   
   else if (Brightness_WhiteString1 >= 100 && Brightness_WhiteString1 < 1000)
   {lcd.print("W1:"); lcd.print(Brightness_WhiteString1); lcd.print("/"); lcd.print(maxBrightness);}  
   }
   
   if (y == 100)
   {
   lcd.setCursor(0, 3);   
   percent = 100*(Brightness_WhiteString1)/(maxBrightness);
   
   if (percent >= 0 && percent < 10)           
   {lcd.print("W1:00"); lcd.print(percent); lcd.print("%   ");}
   
   else if (percent >= 10 && percent < 100)
   {lcd.print("W1:0"); lcd.print(percent); lcd.print("%   ");}
   
   else if (percent >= 100 && percent < 1000)
   {lcd.print("W1:"); lcd.print(percent); lcd.print("%   ");} 
   }
   
      
   if (y == 200)
   {
   lcd.setCursor(0, 3);
   
   if (Brightness_WhiteString2 >= 0 && Brightness_WhiteString2 < 10)
   {lcd.print("W2:00"); lcd.print(Brightness_WhiteString2); lcd.print("/"); lcd.print(maxBrightness);}
   
   else if (Brightness_WhiteString2 >= 10 && Brightness_WhiteString2 < 100)
   {lcd.print("W2:0"); lcd.print(Brightness_WhiteString2); lcd.print("/"); lcd.print(maxBrightness);}
   
   else if (Brightness_WhiteString2 >= 100 && Brightness_WhiteString2 < 1000)
   {lcd.print("W2:"); lcd.print(Brightness_WhiteString2); lcd.print("/"); lcd.print(maxBrightness);}
   }
      
   if (y == 300)
   {
   lcd.setCursor(0, 3);   
   percent = 100*(Brightness_WhiteString2)/(maxBrightness);
   
   if (percent >= 0 && percent < 10)           
   {lcd.print("W2:00"); lcd.print(percent); lcd.print("%   ");}
   
   else if (percent >= 10 && percent < 100)
   {lcd.print("W2:0"); lcd.print(percent); lcd.print("%   ");}
   
   else if (percent >= 100 && percent < 1000)
   {lcd.print("W2:"); lcd.print(percent); lcd.print("%   ");} 
   }
   
   
   
   if (y == 400)
   {
   lcd.setCursor(0, 3);
   
   if (Brightness_BlueString >= 0 && Brightness_BlueString < 10)
   {lcd.print("BL:00"); lcd.print(Brightness_BlueString); lcd.print("/"); lcd.print(maxBrightness);}
   
   else if (Brightness_BlueString >= 10 && Brightness_BlueString < 100)
   {lcd.print("BL:0"); lcd.print(Brightness_BlueString); lcd.print("/"); lcd.print(maxBrightness);}
   
   else if (Brightness_BlueString >= 100 && Brightness_BlueString < 1000)
   {lcd.print("BL:"); lcd.print(Brightness_BlueString); lcd.print("/"); lcd.print(maxBrightness);}
   }
      
   if (y == 500)
   {
   lcd.setCursor(0, 3);   
   percent = 100*(Brightness_BlueString)/(maxBrightness);
   
   if (percent >= 0 && percent < 10)           
   {lcd.print("BL:00"); lcd.print(percent); lcd.print("%   ");}
   
   else if (percent >= 10 && percent < 100)
   {lcd.print("BL:0"); lcd.print(percent); lcd.print("%   ");}
   
   else if (percent >= 100 && percent < 1000)
   {lcd.print("BL:"); lcd.print(percent); lcd.print("%   ");} 
   }
   
   
   if (y == 600)
   {
   lcd.setCursor(0, 3);
   
   if (Brightness_RedString >= 0 && Brightness_RedString < 10)
   {lcd.print("YL:00"); lcd.print(Brightness_RedString); lcd.print("/"); lcd.print(maxBrightness);}
   
   else if (Brightness_RedString >= 10 && Brightness_RedString < 100)
   {lcd.print("YL:0"); lcd.print(Brightness_RedString); lcd.print("/"); lcd.print(maxBrightness);}
   
   else if (Brightness_RedString >= 100 && Brightness_RedString < 1000)
   {lcd.print("YL:"); lcd.print(Brightness_RedString); lcd.print("/"); lcd.print(maxBrightness);}   
   } 
   
   if (y == 700)
   {
   lcd.setCursor(0, 3);   
   percent = 100*(Brightness_RedString)/(maxBrightness);
   
   if (percent >= 0 && percent < 10)           
   {lcd.print("YL:00"); lcd.print(percent); lcd.print("%   ");}
   
   else if (percent >= 10 && percent < 100)
   {lcd.print("YL:0"); lcd.print(percent); lcd.print("%   ");}
   
   else if (percent >= 100 && percent < 1000)
   {lcd.print("YL:"); lcd.print(percent); lcd.print("%   ");}
   
   y=-1; // Reset counter. As "PrintBrightnessOnLCD()" runs in a pace of 100x, variable "y" must be set to -1 so when it rolls over it will be printed immediately.
   }
   
}


// Print Copyright on LCD
void PrintCopyrightOnLCD (void)
{     
      x++; 
      if (z == 5){flag=1;}
      if (z == 0){flag=0;}
      
      if (x == 1 && flag == 0)
      {
      lcd.setCursor(0, 2); lcd.print("                    ");
      lcd.setCursor(z, 2);
      lcd.print("VascoFerraz.com");
      z++;
      }
     
      if (x == 1 && flag == 1)
      {
      lcd.setCursor(0, 2); lcd.print("                    ");
      lcd.setCursor(z, 2);
      lcd.print("VascoFerraz.com");
      z--;
      }

}


// Debounce ClockMode pushbutton and jump between modes
void SwitchClockMode (unsigned char x)
{
 if (analogRead(ClockMode) >= 1004)
    { ClockModeFlag = 1; }
 
 if (analogRead(ClockMode) <= 20 && ClockModeFlag == 1)
    { ClockModeFlag=0; gotomain=0; lcd.clear(); ClockModeState=x; k=0; } 
}


// This is the function which does the following:
// if there is no input, automatically jump to main display when gotomain = gotomaincounter. Change this time by manipulating the gotomaincouter value.
void gotomainfunction(void)
{
 gotomain++; if (gotomain == gotomaincounter) {gotomain=0; lcd.clear(); ClockModeState=0; k=0; x=0; y=-1;}
// As "copyright()" won't run each cycle, variable "x" must be zeroed (0) so when you jump into the main screen it will be printed immediately.
// As "PrintBrightnessOnLCD()" runs in a pace of 100x, variable "y" must be set to -1 so when it rolls over it will be printed immediately.
}


// Read Time
void ReadTime(void)
{
  Wire.beginTransmission(DS1307_I2C_ADDRESS);
  Wire.write(0x00);
  Wire.endTransmission();
  Wire.requestFrom(DS1307_I2C_ADDRESS, 7);
  second = bcdToDec(Wire.read());
  minute = bcdToDec(Wire.read());
  hour = bcdToDec(Wire.read());
  dayOfWeek = bcdToDec(Wire.read());
  day = bcdToDec(Wire.read());
  month = bcdToDec(Wire.read());
  year = bcdToDec(Wire.read());
}


// Set hour
void SetHour(void)
{
  lcd.home();
  PrintTimeOnLCD();
  lcd.setCursor(0, 1);
  lcd.write((uint8_t)1); lcd.write((uint8_t)1);
  
  // If there is no input, automatically jump to main display when gotomain = gotomaincounter. Change this time by manipulating the gotomaincouter value.
  gotomainfunction();
    
  if(analogRead(SetClockPlus) < 20)
  {  
  gotomain=0;  
  Wire.beginTransmission(DS1307_I2C_ADDRESS);
  Wire.write(0x02);
  Wire.endTransmission();
  Wire.requestFrom(DS1307_I2C_ADDRESS, 1);
  hour = bcdToDec(Wire.read());
  hour++; if (hour == 24){hour=0;}   
  Wire.beginTransmission(DS1307_I2C_ADDRESS);
  Wire.write(0x02);
  Wire.write(decToBcd(hour));
  Wire.endTransmission();
  delay(settingdelay);
  }
  
  SwitchClockMode (2);
  
}


// Set minute
void SetMinute(void)
{ 
 
  lcd.home();
  PrintTimeOnLCD();
  lcd.setCursor(3, 1);
  lcd.write((uint8_t)1); lcd.write((uint8_t)1);
  
  // If there is no input, automatically jump to main display when gotomain = gotomaincounter. Change this time by manipulating the gotomaincouter value.
  gotomainfunction();
  
  if(analogRead(SetClockPlus) < 20)
  {   
  gotomain=0; 
  Wire.beginTransmission(DS1307_I2C_ADDRESS);
  Wire.write(0x01);
  Wire.endTransmission();
  Wire.requestFrom(DS1307_I2C_ADDRESS, 1);
  minute = bcdToDec(Wire.read());
  minute++; if (minute == 60){minute=0;}   
  Wire.beginTransmission(DS1307_I2C_ADDRESS);
  Wire.write(0x01);
  Wire.write(decToBcd(minute));
  Wire.endTransmission();
  delay(settingdelay);
  }
  
  SwitchClockMode (3);
  
}


// Set second
void SetSecond(void)
{
  lcd.home();
  PrintTimeOnLCD();
  lcd.setCursor(6, 1);
  lcd.write((uint8_t)1); lcd.write((uint8_t)1);
  
  // If there is no input, automatically jump to main display when gotomain = gotomaincounter. Change this time by manipulating the gotomaincouter value.
  gotomainfunction();
  
  if(analogRead(SetClockPlus) < 20)
  {
  gotomain=0;
  Wire.beginTransmission(DS1307_I2C_ADDRESS);
  Wire.write(0x00);
  Wire.endTransmission();
  Wire.requestFrom(DS1307_I2C_ADDRESS, 1);
  second = bcdToDec(Wire.read());
  second++; if (second == 60){second=0;}   
  Wire.beginTransmission(DS1307_I2C_ADDRESS);
  Wire.write(0x00);
  Wire.write(decToBcd(second));
  Wire.endTransmission();
  delay(settingdelay);
  }
  
  SwitchClockMode (4);
  
}


// Set day
void SetDay(void)
{  
  lcd.setCursor(0, 1);
  PrintDateOnLCD();
  lcd.setCursor(0, 2);
  lcd.write((uint8_t)1); lcd.write((uint8_t)1);
  
  // If there is no input, automatically jump to main display when gotomain = gotomaincounter. Change this time by manipulating the gotomaincouter value.
  gotomainfunction();
  
  if(analogRead(SetClockPlus) < 20)  
  {
  gotomain=0;  
  Wire.beginTransmission(DS1307_I2C_ADDRESS);
  Wire.write(0x04);
  Wire.endTransmission();
  Wire.requestFrom(DS1307_I2C_ADDRESS, 3);
  day = bcdToDec(Wire.read());
  month = bcdToDec(Wire.read());
  year = bcdToDec(Wire.read());
  day++; 

  if (day == 32 && (month == 1 || month == 3 || month == 5 || month == 7 || month == 8 || month == 10 || month == 12)){day=1;}
  if (day == 31 && (month == 4 || month == 6 || month == 9 || month == 11)){day=1;}
  if (day == 30 && month == 2 && year%4 == 0){day=1;}
  if (day == 29 && month == 2 && year%4 != 0){day=1;}
  
  Wire.beginTransmission(DS1307_I2C_ADDRESS);
  Wire.write(0x04);
  Wire.write(decToBcd(day));
  Wire.endTransmission();
  delay(settingdelay);
  }
   
  SwitchClockMode (5);
  
}


// Set month
void SetMonth(void)
{
  lcd.setCursor(0, 1);
  PrintDateOnLCD();
  lcd.setCursor(3, 2);
  lcd.write((uint8_t)1); lcd.write((uint8_t)1); lcd.write((uint8_t)1);
  
  // If there is no input, automatically jump to main display when gotomain = gotomaincounter. Change this time by manipulating the gotomaincouter value.
  gotomainfunction();
  
  if(analogRead(SetClockPlus) < 20)
  {
  gotomain=0;
  Wire.beginTransmission(DS1307_I2C_ADDRESS);
  Wire.write(0x05);
  Wire.endTransmission();
  Wire.requestFrom(DS1307_I2C_ADDRESS, 1);
  month = bcdToDec(Wire.read()); 

       if (month == 1 && day == 31){month=3;}
  else if (month == 1 && day == 30){month=3;}  
  else if (month == 1 && day == 29 && year%4 != 0){month=3;}
  else if (month == 1 && day == 29 && year%4 == 0){month=2;}  
  else if (month == 1 && day <= 28){month=2;}  
  else if (month == 2){month=3;}
  else if (month == 3 && day == 31){month=5;}
  else if (month == 3 && day <= 30){month=4;}  
  else if (month == 4){month=5;}
  else if (month == 5 && day == 31){month=7;}
  else if (month == 5 && day <= 30){month=6;}  
  else if (month == 6){month=7;}
  else if (month == 7){month=8;}
  else if (month == 8 && day == 31){month=10;}  
  else if (month == 8 && day <= 30){month=9;} 
  else if (month == 9){month=10;}
  else if (month == 10 && day == 31){month=12;}  
  else if (month == 10 && day <= 30){month=11;} 
  else if (month == 11){month=12;}
  else if (month == 12){month=1;}
 
  Wire.beginTransmission(DS1307_I2C_ADDRESS);
  Wire.write(0x05);
  Wire.write(decToBcd(month));
  Wire.endTransmission();
  delay(settingdelay);
  }
  
  SwitchClockMode (6);
  
}


// Set year
void SetYear(void)
{
  lcd.setCursor(0, 1);
  PrintDateOnLCD();
  lcd.setCursor(7, 2);
  lcd.write((uint8_t)1); lcd.write((uint8_t)1); lcd.write((uint8_t)1); lcd.write((uint8_t)1);
  
  // If there is no input, automatically jump to main display when gotomain = gotomaincounter. Change this time by manipulating the gotomaincouter value.
  gotomainfunction();
  
  if(analogRead(SetClockPlus) < 20)
  {
  gotomain=0;
  Wire.beginTransmission(DS1307_I2C_ADDRESS);
  Wire.write(0x06);
  Wire.endTransmission();
  Wire.requestFrom(DS1307_I2C_ADDRESS, 1);
  year = bcdToDec(Wire.read());
  
  if (day == 29 && year%4 == 0 && month == 2){year=year+4;}
  else year++; 
  if (year >= 31){year=8;}
  
  Wire.beginTransmission(DS1307_I2C_ADDRESS);
  Wire.write(0x06);
  Wire.write(decToBcd(year));
  Wire.endTransmission();
  delay(settingdelay);
  }
  
  SwitchClockMode (7);  

}


// Set Pre-sunrise
void SetPresunrise(void)
{   
  presunrise = EEPROM.read(presunriseMemoryBank);
  
  lcd.setCursor(0, 0);
  lcd.print("Pre-sunrise:");
  lcd.setCursor(14, 0);
  lcd.print(":00");
  lcd.setCursor(12, 1);
  lcd.write((uint8_t)1); lcd.write((uint8_t)1);
  
  lcd.setCursor(12, 0);
  if (presunrise >= 10)
  lcd.print(presunrise, DEC); 
  if (presunrise < 10)
  {
  lcd.print ("0");
  lcd.print(presunrise, DEC);
  }
  
  // If there is no input, automatically jump to main display when gotomain = gotomaincounter. Change this time by manipulating the gotomaincouter value.
  gotomainfunction();
  
  if(analogRead(SetClockPlus) < 20)
  {
  gotomain=0;
  presunrise++;
  if (presunrise >= 10){presunrise=6;}
  EEPROM.write(presunriseMemoryBank, presunrise);
  delay(settingdelay);  
  }
    
  SwitchClockMode (8);

}


// Set sunrise
void SetSunrise(void)
{   
  sunrise = EEPROM.read(sunriseMemoryBank);
  
  lcd.setCursor(0, 0);
  lcd.print("Sunrise:");
  lcd.setCursor(14, 0);
  lcd.print(":00");
  lcd.setCursor(12, 1);
  lcd.write((uint8_t)1); lcd.write((uint8_t)1);
  
  lcd.setCursor(12, 0);
  if (sunrise >= 10)
  lcd.print(sunrise, DEC); 
  if (sunrise < 10)
  {
  lcd.print ("0");
  lcd.print(sunrise, DEC);
  }
  
  // If there is no input, automatically jump to main display when gotomain = gotomaincounter. Change this time by manipulating the gotomaincouter value.
  gotomainfunction();
  
  if(analogRead(SetClockPlus) < 20)
  {
  gotomain=0;
  sunrise++;
  if (sunrise >= 19){sunrise=10;}
  EEPROM.write(sunriseMemoryBank, sunrise);
  delay(settingdelay);  
  }
    
  SwitchClockMode (9);

}


// Set sunset
void SetSunset(void)
{   
  sunset = EEPROM.read(sunsetMemoryBank);
  
  lcd.setCursor(0, 0);
  lcd.print("Sunset:");
  lcd.setCursor(14, 0);
  lcd.print(":00");
  lcd.setCursor(12, 1);
  lcd.write((uint8_t)1); lcd.write((uint8_t)1);
  
  lcd.setCursor(12, 0);
  if (sunset >= 10)
  lcd.print(sunset, DEC); 
  if (sunset < 10)
  {
  lcd.print ("0");
  lcd.print(sunset, DEC);
  }
  
  // If there is no input, automatically jump to main display when gotomain = gotomaincounter. Change this time by manipulating the gotomaincouter value.
  gotomainfunction();
  
  if(analogRead(SetClockPlus) < 20)
  {
  gotomain=0;
  sunset++;
  if (sunset >= 24){sunset=19;}
  EEPROM.write(sunsetMemoryBank, sunset);
  delay(settingdelay);  
  }
  
  SwitchClockMode (0);
  x=0;  // As "copyright()" won't run each cycle, variable "x" must be zeroed (0), so when it jumps into the main screen it will be printed immediately.
  y=-1; // As "PrintBrightnessOnLCD()" runs in a pace of 100x, variable "y" must be set to -1 so when it rolls over it will be printed immediately.
}


// Below you will see the function where you can configure the PWM cycle: sunrise, daylight, sunset and moonlight.
void LED_PWM(void)
{
  
    // Pre-sunrise 1
    if (hour == presunrise)
    {
    Brightness_WhiteString1 = 0.5*minute; 
    Brightness_WhiteString2 = 0.5*minute;
    Brightness_BlueString   = 18 + (0.2*minute);
    Brightness_RedString    = 4*minute;
    
    ShiftPWM.SetOne(WhiteString1,Brightness_WhiteString1);
    ShiftPWM.SetOne(WhiteString2,Brightness_WhiteString2);
    ShiftPWM.SetOne(BlueString,Brightness_BlueString);
    ShiftPWM.SetOne(RedString,Brightness_RedString);
    }
    
    
    // Pre-sunrise 2
    if (hour >= presunrise+1 && hour <= sunrise-1)
    {
    Brightness_WhiteString1 = 30; 
    Brightness_WhiteString2 = 30;
    Brightness_BlueString   = 30;
    Brightness_RedString    = 240; //equals to maxBrightness
    
    ShiftPWM.SetOne(WhiteString1,Brightness_WhiteString1);
    ShiftPWM.SetOne(WhiteString2,Brightness_WhiteString2);
    ShiftPWM.SetOne(BlueString,Brightness_BlueString);
    ShiftPWM.SetOne(RedString,Brightness_RedString);
    }
    
      
    // Sunrise
    if (hour == sunrise)
    {
    Brightness_WhiteString1 = 30 + (0.875*4*minute); 
    Brightness_WhiteString2 = 30 + (0.875*4*minute);
    Brightness_BlueString   = 30 + (0.875*4*minute);
    Brightness_RedString    = 240; //equals to maxBrightness
    
    ShiftPWM.SetOne(WhiteString1,Brightness_WhiteString1);
    ShiftPWM.SetOne(WhiteString2,Brightness_WhiteString2);
    ShiftPWM.SetOne(BlueString,Brightness_BlueString);
    ShiftPWM.SetOne(RedString,Brightness_RedString);
    }
 
       
    // Sunlight
    if (hour >= sunrise+1 && hour <= sunset-1)
    {
    Brightness_WhiteString1 = maxBrightness; 
    Brightness_WhiteString2 = maxBrightness;
    Brightness_BlueString   = maxBrightness;
    Brightness_RedString    = maxBrightness;
  
    ShiftPWM.SetOne(WhiteString1,Brightness_WhiteString1); 
    ShiftPWM.SetOne(WhiteString2,Brightness_WhiteString2); 
    ShiftPWM.SetOne(BlueString,Brightness_BlueString);   
    ShiftPWM.SetOne(RedString,Brightness_RedString );                      
    }
  
      
    // Sunset
    if (hour == sunset)
    {
    Brightness_WhiteString1 = maxBrightness - (4*minute);
    Brightness_WhiteString2 = maxBrightness - (4*minute);
    Brightness_BlueString   = maxBrightness - (0.925*4*minute);
    Brightness_RedString    = maxBrightness - (4*minute);
  
    ShiftPWM.SetOne(WhiteString1,Brightness_WhiteString1);
    ShiftPWM.SetOne(WhiteString2,Brightness_WhiteString2);
    ShiftPWM.SetOne(BlueString,Brightness_BlueString);
    ShiftPWM.SetOne(RedString,Brightness_RedString);
    }
  
  
    // Moonlight 1
    if (hour >= sunset+1 && hour <= 23)
    {
    Brightness_WhiteString1 = 0; 
    Brightness_WhiteString2 = 0;
    Brightness_BlueString   = 18;
    Brightness_RedString    = 0;
  
    ShiftPWM.SetOne(WhiteString1,Brightness_WhiteString1);
    ShiftPWM.SetOne(WhiteString2,Brightness_WhiteString2);
    ShiftPWM.SetOne(BlueString,Brightness_BlueString);
    ShiftPWM.SetOne(RedString,Brightness_RedString);
    }
    
    
    // Moonlight 2
    if (hour >= 0 && hour <= presunrise-1)
    {
    Brightness_WhiteString1 = 0; 
    Brightness_WhiteString2 = 0;
    Brightness_BlueString   = 18;
    Brightness_RedString    = 0;
  
    ShiftPWM.SetOne(WhiteString1,Brightness_WhiteString1);
    ShiftPWM.SetOne(WhiteString2,Brightness_WhiteString2);
    ShiftPWM.SetOne(BlueString,Brightness_BlueString);
    ShiftPWM.SetOne(RedString,Brightness_RedString);
    }
    
    
}


// This is a function with debugging purposes
void LED_PWM_Debug(void)
{
 ShiftPWM.SetOne(WhiteString1,50);
 ShiftPWM.SetOne(WhiteString2,50);
 ShiftPWM.SetOne(BlueString,50);
 ShiftPWM.SetOne(RedString,50); 
}
