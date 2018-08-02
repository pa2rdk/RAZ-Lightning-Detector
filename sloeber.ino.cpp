#ifdef __IN_ECLIPSE__
//This is a automatic generated file
//Please do not modify this file
//If you touch this file your change will be overwritten during the next build
//This file has been generated on 2018-08-02 15:06:04

#include "Arduino.h"
#include "Arduino.h"
#include "PWFusion_AS3935_I2C.h"
#include <SoftwareSerial.h>
#include <RDKESP8266.h>
#include <EEPROM.h>
#define isColor
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <avr/wdt.h>

void dispData() ;
void outputCalibrationValues() ;
void printLogo() ;
void printInfo() ;
void printStat() ;
void printHist() ;
void printTime() ;
void dispActive(bool on);
void loop() ;
void saveConfig() ;
void loadConfig() ;
void printConfig() ;
void setSettings(bool doAsk) ;
void getStringValue(int length) ;
byte getCharValue() ;
byte getNumericValue() ;
void serialFlush() ;
void handleMenu(int btnValue) ;
void handleButton(int btnValue) ;
void AS3935_ISR() ;
ISR(WDT_vect) ;
ISR(TIMER1_OVF_vect)  ;
void SingleBeep(int cnt) ;
void handleLighting() ;
void moveMinutes() ;
void moveHours() ;
void moveDays() ;
void showTime() ;
void dispTime(byte line, byte dw, byte hr, byte mn, byte sc) ;
void setup() ;
void configure_wdt(void) ;
void configure_timer() ;
void InitWiFiConnection() ;
void sendToSite(byte whichInt, byte dist) ;
void getNTPData() ;
void getData(int timeout) ;
void CheckStatus() ;
void printMinutes() ;
void printHours() ;
void printDays() ;
void printGraph() ;
void printArrow() ;

#include "RAZLightning.ino"


#endif
