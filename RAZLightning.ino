//RAZLightning.ino v2.6 08/04/2017
//Placed on GITHUB Aug. 1 2018
//By R.J. de Kok - (c) 2018

#include "Arduino.h"

#include "PWFusion_AS3935_I2C.h"
#include <SoftwareSerial.h>
#include <RDKESP8266.h>
#include <EEPROM.h>

#define isColor

#ifdef isColor
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#else
#include <PCD8544.h>
#endif

#include <avr/wdt.h>

#ifdef isColor
#define LED                  12
#define CE                    9
#define DC                    7
#define RST                  10
#define lineHeight			 8
#define LINECLR          0xF800
#define GRFCLR           0x07FF
#define startPix              5
#define bckLightOff           0
#define bckLightOn            1
#else
#define CLK                   7 //8 (7 = final, 8 = proto)
#define DIN                  12 //12 
#define DC                   11 //11
#define CE                    9  //9
#define RST                  10 //10
#define LED                  13
#define lineHeight			 1
#define bckLightOff           1
#define bckLightOn            0
#endif

#define beepOn                1
#define beepOff               0
// interrupt trigger global var
volatile int8_t AS3935_ISR_Trig = 0;

// defines for hardware config
#define SI_PIN               9
#define IRQ_PIN              2
#define AS3935_ADD           0x03     // x03 - standard PWF SEN-39001-R01 config

#define AS3935_INDOORS       0
#define AS3935_OUTDOORS      1
#define AS3935_DIST_DIS      0
#define AS3935_DIST_EN       1
#define BUTTON               0
#define BEEPER               4

#define btnNone              0
#define btnRight             1
#define btnDown              2
#define btnLeft              3
#define btnUp                4

#define FROMTIME             0
#define FROMMENU             1
#define FROMLIGHTNING        2
#define FROMNOTHING          3

#define HOST                "onweer.pi4raz.nl"     // Host to contact
#define PAGE                "/AddEvent.php?Callsign=PA2RDK&IntType=1&Distance=18" // Web page to request
#define PORT                80

#define dispStat             0
#define dispHist             1
#define dispTijd             2
#define dispLogo             3
#define dispInfo				 4
#ifdef isColor
#define dispMinute           5
#define dispHour             6
#define dispDay              7
#define dispMax              7
#else
#define dispMax              4
#endif

#define offsetEEPROM       0x0    //offset config

struct StoreStruct {
	byte chkDigit;
	char ESP_SSID[16];
	char ESP_PASS[27];
	char MyCall[10];
	byte doorMode;
	byte distMode;
	byte beeperCnt;
	byte AS3935_Capacitance;
	byte AS3935_NoiseFloorLvl;
	byte AS3935_SpikeRejection;
	int timeCorrection;
	byte dispScreen;
	byte contrast;
};

StoreStruct storage = {
		'#',
		"MARODEKExtender",
		"0919932003",
		"PA2RDK",
		AS3935_OUTDOORS,
		AS3935_DIST_EN,
		2,
		112,
		2,
		2,
		1,
		0,
		66
};

bool isConnected = false;
bool isAsleep = false;
char receivedString[128];
char IPNo[18];
byte debug = 1;
int myButton = 0;
char chkGS[3] = "GS";

byte minutes[60];
byte maxMinute = 0;
byte divMinute = 1;

byte hours[48];
byte maxHour = 0;
byte divHour = 1;
byte heartBeatCounter = 55;

byte days[30];
byte maxDay = 0;
byte divDay = 1;
uint16_t pulses = 0;
uint16_t distPulses = 0;
byte minuteBeeped = 0;
unsigned long buttonTime = 0;
bool exitLoop = false;
bool handleTime = 0;
int majorVersion=2;
int minorVersion=6; //Eerste uitlevering 15/11/2017
int wdtCounter = 0;
bool manualGetTime = false;
bool hbSend = 0;

struct histData {
	byte dw;
	byte hr;
	byte mn;
	byte sc;
	byte dt;
};

histData lastData[10];

byte second = 0;
byte doubleSecond = 0;
byte minute = 0;
byte lastMinute = 0;
byte hour = 0;
byte lastHour = 0;
byte dayOfWeek = 1;
byte lastDayOfWeek = 1;
byte fromSource = 0;
byte dispSide = 0;
byte startPos = 0;
byte height;
byte btnPressed = 0;

// prototypes
void AS3935_ISR();

PWF_AS3935_I2C  lightning0((uint8_t)IRQ_PIN, (uint8_t)SI_PIN, (uint8_t)AS3935_ADD);
//Adafruit_PCD8544 display = Adafruit_PCD8544(CLK, DIN, DC, CE, RST);
//Nokia_5110 display = Nokia_5110(RST, CE, DC, DIN, CLK);


#ifdef isColor
Adafruit_ST7735 display = Adafruit_ST7735(CE,  DC, RST);
#else
PCD8544 display = PCD8544(CLK, DIN, DC, RST, CE);
#endif

SoftwareSerial esp8266(5, 3); // RX, TX
RDKESP8266 wifi(&esp8266, &Serial, 6);

const unsigned char PROGMEM lightning_bmp[32] = {
		0x01, 0xE0, 0x02, 0x20, 0x0C, 0x18, 0x12, 0x24, 0x21, 0x06, 0x10, 0x02, 0x1F, 0xFC, 0x01, 0xF0,
		0x01, 0xC0, 0x03, 0x80, 0x07, 0xF8, 0x00, 0xF0, 0x00, 0xC0, 0x01, 0x80, 0x01, 0x00, 0x01, 0x00
};

void dispData() {
	if (storage.dispScreen == dispStat) {
		printStat();
	}
	if (storage.dispScreen == dispHist) {
		printHist();
	}
	if (storage.dispScreen == dispTijd) {
		printTime();
	}
	if (storage.dispScreen == dispLogo) {
		printLogo();
	}
	if (storage.dispScreen == dispInfo) {
		printInfo();
	}
#ifdef isColor
	if (storage.dispScreen == dispMinute) {
		printMinutes();
	}
	if (storage.dispScreen == dispHour) {
		printHours();
	}
	if (storage.dispScreen == dispDay) {
		printDays();
	}
#endif
}

void outputCalibrationValues() {
	// output the frequencies that the different capacitor values set:
	delay(50);
	Serial.println();
	int bestFreq = 999;
	int bestI=0;
	for (byte i = 0; i <= 0x0F; i++) {
		int frequency = lightning0.AS3935_tuneAntenna(i);
		if (frequency < bestFreq) {
			bestFreq = frequency;
			bestI = i;
		}
		Serial.print(F("tune antenna to capacitor "));
		Serial.print(i);
		Serial.print(F("\t gives frequency: "));
		Serial.print(frequency);
		Serial.print(" = ");
		long fullFreq = (long) frequency * 160; // multiply with clock-divider, and 10 (because measurement is for 100ms)
		Serial.print(fullFreq, DEC);
		Serial.println(" Hz");
		delay(20);
	}
	Serial.print(F("Best setting:")); Serial.print(bestI, DEC); Serial.print(F(" Capacity:")); Serial.println(bestI * 8, DEC);
	display.setCursor(0, 0*lineHeight);
	display.print(F("Best cap.")); display.print(bestI * 8, DEC);
}

void printLogo() {
	display.clear();
#ifdef isColor
	display.setTextColor(ST7735_YELLOW);
	display.setTextSize(2);
#endif
	display.setCursor(0, 0*lineHeight);
	display.print(F("   PI4RAZ"));
#ifdef isColor
	display.setTextSize(1);
	display.setTextColor(ST7735_WHITE);
#endif
	display.setCursor(0,2*lineHeight);
	display.print(F("  Lightning"));
	display.setCursor(0, 3*lineHeight);
	display.print(F(" Detector v"));
	display.print(majorVersion);
	display.print(F("."));
	display.print(minorVersion);
	//display.display();
}

void printInfo() {
	display.clear();
#ifdef isColor
	display.setTextColor(ST7735_CYAN);
	display.setTextSize(2);
#endif
	display.setCursor(0, 0*lineHeight);
	display.print(F("Info:"));
#ifdef isColor
	display.setTextSize(1);
	display.setTextColor(ST7735_WHITE);
#endif
	display.print(F("  HBC="));
	if (hbSend==1) display.print(heartBeatCounter); else display.print(F("*"));
	display.setCursor(0, 2*lineHeight);
	if (isConnected==1) display.print(IPNo);	else display.print(F("No connection"));
	display.setCursor(0, 3*lineHeight);
	display.print(F("Call:"));
	display.print(storage.MyCall);
	display.setCursor(0, 4*lineHeight);
	display.print(F("Mode:"));
	if (storage.doorMode == AS3935_OUTDOORS) display.print(F("Outdoor")); else display.print(F("Indoors"));
	display.setCursor(0, 5*lineHeight);
	display.print(F("Dist:"));
	if (storage.distMode == AS3935_DIST_EN) display.print(F("Enabled")); else display.print(F("Disabled"));

	//display.display();
}

void printStat() {
	display.clear();
#ifdef isColor
	display.setTextColor(ST7735_GREEN);
	display.setTextSize(2);
#endif
	display.setCursor(0, 0*lineHeight);
	display.print(F("Stats:"));
#ifdef isColor
	display.setTextSize(1);
	display.setTextColor(ST7735_WHITE);
#endif
	display.setCursor(0, 2*lineHeight);
	display.print(F("Minute:"));
	display.print(minutes[0]);
	display.setCursor(0, 3*lineHeight);
	display.print(F("Hour  :"));
	display.print(hours[0]);
	display.setCursor(0, 4*lineHeight);
	display.print(F("Day   :"));
	display.print(days[0]);
	display.setCursor(0, 5*lineHeight);
	display.print(F("Pulses:"));
	display.print(pulses);
	display.print(F("/"));
	display.print(distPulses);
	//display.display();
}

void printHist() {
	display.clear();
#ifdef isColor
	display.setTextColor(ST7735_BLUE);
	display.setTextSize(2);
#endif
	display.setCursor(0, 0*lineHeight);
	display.print(F("History:"));
#ifdef isColor
	display.setTextSize(1);
	display.setTextColor(ST7735_WHITE);
#endif
	int j;
	if (dispSide == 2) j = 2; else j = 4;
#ifdef isColor
	j=10;
	dispSide == 0;
#endif
	for (int i = 0; i < j; i++) {
		dispTime(i+2, lastData[i + (dispSide * 4)].dw, lastData[i + (dispSide * 4)].hr, lastData[i + (dispSide * 4)].mn, 100);
		display.setCursor(57, (i+2)*lineHeight);
		if (lastData[i].dt < 10) display.print(F(" "));
		display.print(lastData[i + (dispSide * 4)].dt); display.print(F("KM"));
	}
}


void printTime() {
	display.clear();
	display.setCursor(0, 1*lineHeight);
#ifdef isColor
	display.setTextColor(ST7735_RED);
	display.setTextSize(2);
#endif
	if (hour < 10)
	{
		display.print('0');
	}
	display.print(hour, DEC);
	display.print(':');
	if (minute < 10)
	{
		display.print('0');
	}
	display.print(minute, DEC);
	display.print(':');
	if (second < 10)
	{
		display.print('0');
	}

	display.print(second, DEC);

	if (dispSide == 2) {
		display.setCursor(0,0*lineHeight);
		display.print(F(" *"));
	}
	if (dispSide == 3) {
		display.setCursor(0,0*lineHeight);
		display.print(F("    *"));
	}
	if (dispSide == 4) {
		display.setCursor(0,0*lineHeight);
		display.print(F("       *"));
	}
#ifdef isColor
	display.setTextSize(1);
	display.setTextColor(ST7735_WHITE);
#endif

	display.setCursor(0, 0*lineHeight);

	if (dispSide < 2) {
		switch (dayOfWeek) {
		case 1:
			display.print(F("Sunday"));
			break;
		case 2:
			display.print(F("Monday"));
			break;
		case 3:
			display.print(F("Tuesday"));
			break;
		case 4:
			display.print(F("Wednesday"));
			break;
		case 5:
			display.print(F("Thursday"));
			break;
		case 6:
			display.print(F("Friday"));
			break;
		case 7:
			display.print(F("Saturday"));
			break;
		}
		if (dispSide == 1) {
			display.print(F(" *"));
		}
	}

	display.setCursor(0, 3*lineHeight);
	display.print(F("Minute:"));
	display.print(minutes[0]);
	display.setCursor(0, 4*lineHeight);
	display.print(F("Hour  :"));
	display.print(hours[0]);
	display.setCursor(0, 5*lineHeight);
	display.print(F("Day   :"));
	display.print(days[0]);
}

void dispActive(bool on){
	if (on==1){
		digitalWrite(LED, bckLightOn);
		display.setPower(true);
	}
	else{
		digitalWrite(LED, bckLightOff);
		display.setPower(false);
	}
}


void loop()
{
	if (isAsleep == false) {
		isAsleep = wifi.deepSleep(0);
	}

	attachInterrupt(digitalPinToInterrupt(2), AS3935_ISR, RISING);
	lightning0.AS3935_GetInterruptSrc();
	AS3935_ISR_Trig = 0;

	fromSource = FROMNOTHING;

	while (0 == AS3935_ISR_Trig && analogRead(BUTTON) > 500 && minute == lastMinute && !exitLoop) {
		if (buttonTime > 0 && millis() - buttonTime > 10000) {
			exitLoop = true;
			buttonTime = 0;
		}
	}
	wdt_reset();
	wdtCounter=0;

	exitLoop = false;
	dispActive(0);

	if (AS3935_ISR_Trig > 0){
		fromSource = FROMLIGHTNING;
	}

	delay(5);
	detachInterrupt(digitalPinToInterrupt(2));

	myButton = analogRead(BUTTON);

	if (lastMinute != minute)
	{
		heartBeatCounter++;
		lastMinute = minute;
		fromSource = FROMTIME;
		if (handleTime == 0) {
			moveMinutes();

			if (hour != lastHour) {
				lastHour = hour;
				moveHours();
			}
			if (dayOfWeek != lastDayOfWeek) {
				lastDayOfWeek = dayOfWeek;
				moveDays();
			}
		}
	}

	if (manualGetTime == true){
		manualGetTime = false;
		heartBeatCounter = 60;
	}

	if (heartBeatCounter == 60) {
		hbSend = 0;
		heartBeatCounter = 0;
		InitWiFiConnection();
		if (isConnected == 1) {
			hbSend = 1;
			getNTPData();
			sendToSite(0, 0);
		}
	}

	if (myButton < 500) {
		fromSource = FROMMENU;
		handleMenu(myButton);
	}


	Serial.print(F("Escaped from loop:")); Serial.print(fromSource); Serial.print(F("  Buttontime:")); Serial.println(buttonTime);

	if (fromSource < FROMNOTHING) {
		if (fromSource == FROMLIGHTNING) handleLighting();
		dispData();
		buttonTime = millis();
	}
}

void saveConfig() {
	for (unsigned int t = 0; t < sizeof(storage); t++)
		EEPROM.write(offsetEEPROM + t, *((char*)&storage + t));
}

void loadConfig() {
	if (EEPROM.read(offsetEEPROM + 0) == storage.chkDigit)
		for (unsigned int t = 0; t < sizeof(storage); t++)
			*((char*)&storage + t) = EEPROM.read(offsetEEPROM + t);
}

void printConfig() {
	if (EEPROM.read(offsetEEPROM + 0) == storage.chkDigit)
		for (unsigned int t = 0; t < sizeof(storage); t++)
			Serial.write(EEPROM.read(offsetEEPROM + t));
	Serial.println();
	setSettings(0);
}

void setSettings(bool doAsk) {
	int i = 0;
	Serial.print(F("SSID ("));
	Serial.print(storage.ESP_SSID);
	Serial.print(F("):"));
	if (doAsk == 1) {
		getStringValue(15);
		if (receivedString[0] != 0) {
			storage.ESP_SSID[0] = 0;
			strcat(storage.ESP_SSID, receivedString);
		}
	}
	Serial.println();

	Serial.print(F("Password ("));
	Serial.print(storage.ESP_PASS);
	Serial.print(F("):"));
	if (doAsk == 1) {
		getStringValue(26);
		if (receivedString[0] != 0) {
			storage.ESP_PASS[0] = 0;
			strcat(storage.ESP_PASS, receivedString);
		}
	}
	Serial.println();

	Serial.print(F("Call ("));
	Serial.print(storage.MyCall);
	Serial.print(F("):"));
	if (doAsk == 1) {
		getStringValue(9);
		if (receivedString[0] != 0) {
			storage.MyCall[0] = 0;
			strcat(storage.MyCall, receivedString);
		}
	}
	Serial.println();

	Serial.print(F("Indoors=0 or Outdoors=1 ("));
	if (storage.doorMode == 0) {
		Serial.print(F("Indoors"));
	} else {
		Serial.print(F("Outdoors"));
	}
	Serial.print(F("): "));
	if (doAsk == 1) {
		i = getNumericValue();
		if (receivedString[0] != 0) storage.doorMode = i;
	}
	Serial.println();

	Serial.print(F("Disturber (0/1) ("));
	if (storage.distMode == 0) {
		Serial.print(F("Disabled"));
	} else {
		Serial.print(F("Enabled"));
	}
	Serial.print(F("): "));
	if (doAsk == 1) {
		i = getNumericValue();
		if (receivedString[0] != 0) storage.distMode = i;
	}
	Serial.println();

	Serial.print(F("#Beeper ("));
	Serial.print(storage.beeperCnt);
	Serial.print(F("):"));
	if (doAsk == 1) {
		i = getNumericValue();
		if (receivedString[0] != 0) storage.beeperCnt = i;
	}
	Serial.println();

	Serial.print(F("Capacity ("));
	Serial.print(storage.AS3935_Capacitance);
	Serial.print(F("):"));
	if (doAsk == 1) {
		i = getNumericValue();
		if (receivedString[0] != 0) storage.AS3935_Capacitance = i;
	}
	Serial.println();

	Serial.print(F("Noiselevel (1 - 8) ("));
	Serial.print(storage.AS3935_NoiseFloorLvl);
	Serial.print(F("):"));
	if (doAsk == 1) {
		i = getNumericValue();
		if (receivedString[0] != 0) storage.AS3935_NoiseFloorLvl = i;
	}
	Serial.println();

	Serial.print(F("Spike rejection level (0 - 15) ("));
	Serial.print(storage.AS3935_SpikeRejection);
	Serial.print(F("):"));
	if (doAsk == 1) {
		i = getNumericValue();
		if (receivedString[0] != 0) storage.AS3935_SpikeRejection = i;
	}
	Serial.println();

	Serial.print(F("Time correction ("));
	Serial.print(storage.timeCorrection);
	Serial.print(F("):"));
	if (doAsk == 1) {
		i = getNumericValue();
		if (receivedString[0] != 0) storage.timeCorrection = i;
	}
	Serial.println();

	Serial.print(F("Screen 1 (0 - 3) ("));
	Serial.print(storage.dispScreen);
	Serial.print(F("):"));
	if (doAsk == 1) {
		i = getNumericValue();
		if (receivedString[0] != 0) storage.dispScreen = i;
	}
	Serial.println();

	Serial.print(F("Contrast("));
	Serial.print(storage.contrast);
	Serial.print(F("):"));
	if (doAsk == 1) {
		i = getNumericValue();
		if (receivedString[0] != 0) storage.contrast = i;
	}
	Serial.println();

	if (doAsk == 1) {
		saveConfig();
		loadConfig();
	}
}

void getStringValue(int length) {
	serialFlush();
	receivedString[0] = 0;
	int i = 0;
	while (receivedString[i] != 13 && i < length) {
		if (Serial.available() > 0) {
			receivedString[i] = Serial.read();
			if (receivedString[i] == 13 || receivedString[i] == 10) {
				i--;
			}
			else {
				Serial.write(receivedString[i]);
			}
			i++;
		}
	}
	receivedString[i] = 0;
	serialFlush();
}

byte getCharValue() {
	serialFlush();
	receivedString[0] = 0;
	int i = 0;
	while (receivedString[i] != 13 && i < 2) {
		if (Serial.available() > 0) {
			receivedString[i] = Serial.read();
			if (receivedString[i] == 13 || receivedString[i] == 10) {
				i--;
			}
			else {
				Serial.write(receivedString[i]);
			}
			i++;
		}
	}
	receivedString[i] = 0;
	serialFlush();
	return receivedString[i - 1];
}

byte getNumericValue() {
	serialFlush();
	byte myByte = 0;
	byte inChar = 0;
	bool isNegative = false;
	receivedString[0] = 0;

	int i = 0;
	while (inChar != 13) {
		if (Serial.available() > 0) {
			inChar = Serial.read();
			if (inChar > 47 && inChar < 58) {
				receivedString[i] = inChar;
				i++;
				Serial.write(inChar);
				myByte = (myByte * 10) + (inChar - 48);
			}
			if (inChar == 45) {
				Serial.write(inChar);
				isNegative = true;
			}
		}
	}
	receivedString[i] = 0;
	if (isNegative == true) myByte = myByte * -1;
	serialFlush();
	return myByte;
}

void serialFlush() {
	for (int i = 0; i < 10; i++)
	{
		while (Serial.available() > 0) {
			Serial.read();
		}
	}
}

void handleMenu(int btnValue) {
	dispActive(1);
	if (btnValue < 1000) delay(200);

	btnPressed = btnNone;
	if (btnValue < 400) {
		btnPressed = btnRight;
	}
	if (btnValue < 300) {
		btnPressed = btnDown;
	}
	if (btnValue < 200) {
		btnPressed = btnLeft;
	}
	if (btnValue < 50) {
		btnPressed = btnUp;
	}
	Serial.print(F("Display:")); Serial.println(storage.dispScreen);
	Serial.print(F("Button pressed:")); Serial.println(btnPressed);
	handleButton(btnPressed);
}

void handleButton(int btnValue) {
	cli(); // disable all interrupts
	if (storage.dispScreen == dispTijd && dispSide > 0) handleTime = 1; else handleTime = 0;
	if (btnValue == btnUp) {
		if (storage.dispScreen == dispTijd && dispSide > 0) {
			if (dispSide == 1) dayOfWeek++;
			if (dispSide == 2) hour++;
			if (dispSide == 3) minute++;
			if (dispSide == 4) second++;
			if (dayOfWeek > 7) dayOfWeek = 1;
			if (hour > 23) hour = 0;
			if (minute > 59) minute = 0;
			if (second > 59) second = 0;
		} else {
			storage.dispScreen++;
			if (storage.dispScreen > dispMax) storage.dispScreen = 0;
			dispSide = 0;
		}
	}
	if (btnValue == btnDown) {
		if (storage.dispScreen == dispTijd && dispSide > 0) {
			if (dispSide == 1) dayOfWeek--;
			if (dispSide == 2) hour--;
			if (dispSide == 3) minute--;
			if (dispSide == 4) second--;
			if (dayOfWeek > 7) dayOfWeek = 7;
			if (hour > 23) hour = 23;
			if (minute > 59) minute = 59;
			if (second > 59) second = 59;
		} else {
			storage.dispScreen--;
			if (storage.dispScreen > dispMax) storage.dispScreen = dispMax;
			dispSide = 0;
		}
	}
	if (btnValue == btnLeft) {
		dispSide--;
		if (storage.dispScreen == dispHist) if (dispSide > 2) dispSide = 2;
		if (storage.dispScreen == dispTijd) if (dispSide > 4) dispSide = 4;
		if (storage.dispScreen == dispStat) manualGetTime = true;
	}
	if (btnValue == btnRight) {
		dispSide++;
		if (storage.dispScreen == dispHist) if (dispSide > 2) dispSide = 0;
		if (storage.dispScreen == dispTijd) if (dispSide > 4) dispSide = 0;
	}
	sei(); // enable all interrupts
}

void AS3935_ISR() {
	AS3935_ISR_Trig = 1;
}

void(* resetFunc) (void) = 0; //declare reset function @ address 0

ISR(WDT_vect)
{
	// not hanging, just waiting
	// reset the watchdog
	wdt_reset();
	wdtCounter++;
	Serial.print(F("Watchdog received (max 10) "));Serial.println(wdtCounter);
	if (wdtCounter==10) resetFunc();  //asm volatile("jmp 0");
}

ISR(TIMER1_OVF_vect) // Timer1_COMPA_vect timer compare interrupt service routine
{
	//digitalWrite(LED, digitalRead(LED) ^ 1); // toggle LED pin
	TCNT1 = 34286;            // preload timer
	doubleSecond++;
	if (doubleSecond > 121)
		doubleSecond = 0;
	second = doubleSecond / 2;
	if (second == 60) {
		doubleSecond = 0;
		second = 0;
		minute++;
		Serial.print(".");
		if (minute == 60) {
			minute = 0;
			hour++;
			if (hour == 24) {
				hour = 0;
				dayOfWeek++;
				if (dayOfWeek == 8) {
					dayOfWeek = 1;
				}
			}
		}
	}
}

void SingleBeep(int cnt) {
	int tl = 200;
	for (int i = 0; i < cnt; i++) {
		digitalWrite(BEEPER, beepOn);
		delay(tl);
		digitalWrite(BEEPER, beepOff);
		delay(tl);
	}
}

void handleLighting() {
	dispActive(1);
	showTime();
	display.clear();
	uint8_t int_src = lightning0.AS3935_GetInterruptSrc();
	AS3935_ISR_Trig = 0;
	if (0 == int_src)
	{
		Serial.println(F("interrupt source result not expected"));
		display.clear();
		display.setCursor(0,0*lineHeight);
		display.print(F("interrupt"));
		display.setCursor(0,1*lineHeight);
		display.print(F(" source result"));
		display.setCursor(0,2*lineHeight);
		display.print(F(" not expected"));
		//display.display();
	}
	else if (1 == int_src)
	{
		uint8_t lightning_dist_km = lightning0.AS3935_GetLightningDistKm();
		Serial.print(F("Lightning detected! Distance to strike: "));
		Serial.print(lightning_dist_km);
		Serial.println(F(" kilometers"));
		display.clear();
		display.setCursor(0,0*lineHeight);
		display.print(F("Lightning"));
		display.setCursor(0,1*lineHeight);
		display.print(F("  Dist. "));
		display.print(lightning_dist_km);
		display.print(F(" KM"));
		//display.display();
		pulses++;
		minutes[0]++;
		hours[0]++;
		days[0]++;
		for (int i = 0; i < 9; i++) {
			lastData[9 - i] = lastData[8 - i];
		}
		lastData[0].dw = dayOfWeek;
		lastData[0].hr = hour;
		lastData[0].mn = minute;
		lastData[0].sc = second;
		lastData[0].dt = lightning_dist_km;

		sendToSite(1,lightning_dist_km);
		heartBeatCounter = 0;

		for (int i = 0; i < 10; i++) {
			Serial.print(lastData[i].dw); Serial.print(F(" "));
			Serial.print(lastData[i].hr); Serial.print(F(":"));
			Serial.print(lastData[i].mn); Serial.print(F(":"));
			Serial.print(lastData[i].sc); Serial.print(F(" Dist:"));
			Serial.print(lastData[i].dt); Serial.println(F(" KM"));
		}
		if (storage.beeperCnt > 0 && minutes[0] >= storage.beeperCnt && minuteBeeped == 0) {
				SingleBeep(5);
				minuteBeeped++;
		}
		buttonTime = millis();
	}
	else if (2 == int_src)
	{
		Serial.println(F("Disturber detected"));
		display.clear();
		display.setCursor(0,0*lineHeight);
		display.print(F("Disturber"));
		display.setCursor(0,1*lineHeight);
		display.print(F("  detected"));
		distPulses++;
		//display.display();
	}
	else if (3 == int_src)
	{
		display.clear();
		display.setCursor(0,0*lineHeight);
		Serial.println(F("Noise level too high"));
		display.print(F("Noise level"));
		display.setCursor(0,1*lineHeight);
		display.print(F(" too high"));
		//display.display();
	}
	//while (int_src = lightning0.AS3935_GetInterruptSrc()>0){}
	lightning0.AS3935_PrintAllRegs(); // for debug...
	lightning0.AS3935_ClearDistance(0x1F);
	lightning0.AS3935_ClearStatistics();
	delay(500);
}

void moveMinutes() {
	for (int i = 0; i < 59; i++) {
		minutes[59 - i] = minutes[58 - i];
	}
	minutes[0] = 0;
	maxMinute = 0;
	for (int i = 0; i <= 59; i++) {
		if (minutes[i] > maxMinute) {
			maxMinute = minutes[i];
		}
	}
	divMinute = maxMinute / 30;
	if (divMinute < 1) {
		divMinute = 1;
	}
	minuteBeeped = 0;
}

void moveHours() {
	for (int i = 0; i < 23; i++) {
		hours[23 - i] = hours[22 - i];
	}
	hours[0] = 0;
	maxHour = 0;
	for (int i = 0; i <= 23; i++) {
		if (hours[i] > maxHour) {
			maxHour = hours[i];
		}
	}
	divHour = maxHour / 30;
	if (divHour < 1) {
		divHour = 1;
	}
}

void moveDays() {
	for (int i = 0; i < 29; i++) {
		days[29 - i] = days[28 - i];
	}
	days[0] = 0;
	maxDay = 0;
	for (int i = 0; i <= 29; i++) {
		if (days[i] > maxDay) {
			maxDay = days[i];
		}
	}
	divDay = maxDay / 30;
	if (divDay < 1) {
		divDay = 1;
	}
}

void showTime() {
	switch (dayOfWeek) {
	case 1:
		Serial.print(F("Sunday"));
		break;
	case 2:
		Serial.print(F("Monday"));
		break;
	case 3:
		Serial.print(F("Tuesday"));
		break;
	case 4:
		Serial.print(F("Wednesday"));
		break;
	case 5:
		Serial.print(F("Thursday"));
		break;
	case 6:
		Serial.print(F("Friday"));
		break;
	case 7:
		Serial.print(F("Saturday"));
		break;
	}
	Serial.print(' ');
	if (hour < 10)
	{
		Serial.print('0');
	}
	Serial.print(hour, DEC);
	Serial.print(':');
	if (minute < 10)
	{
		Serial.print('0');
	}
	Serial.print(minute, DEC);
	Serial.print(':');
	if (second < 10)
	{
		Serial.print('0');
	}
	Serial.print(second, DEC);
	Serial.print(' ');
}

void dispTime(byte line, byte dw, byte hr, byte mn, byte sc) {
	display.setCursor(0, line*lineHeight);
	switch (dw) {
	case 1:
		display.print(F("Sun"));
		break;
	case 2:
		display.print(F("Mon"));
		break;
	case 3:
		display.print(F("Tue"));
		break;
	case 4:
		display.print(F("Wed"));
		break;
	case 5:
		display.print(F("Thu"));
		break;
	case 6:
		display.print(F("Fri"));
		break;
	case 7:
		display.print(F("Sat"));
		break;
	default:
		display.print(F("***"));
		break;
	}

	display.setCursor(21, line*lineHeight);
	if (hr < 10)
	{
		display.print('0');
	}
	display.print(hr, DEC);
	display.print(':');
	if (mn < 10)
	{
		display.print('0');
	}
	display.print(mn, DEC);
	if (sc < 99) {
		display.print(':');
		if (sc < 10)
		{
			display.print('0');
		}
		display.print(sc, DEC);
	}
}

void setup()
{
	MCUSR = 0;
	pinMode(LED, OUTPUT);
	pinMode(BEEPER, OUTPUT);
	pinMode(IRQ_PIN, INPUT);

#ifdef isColor
	pinMode(RST, OUTPUT);
	pinMode(CE, OUTPUT);
	pinMode(DC, OUTPUT);
#else
	pinMode(DIN, OUTPUT);
	pinMode(CLK, OUTPUT);
	pinMode(RST, OUTPUT);
	pinMode(CE, OUTPUT);
	pinMode(DC, OUTPUT);
#endif

	digitalWrite(BEEPER,beepOff);
	digitalWrite(LED, bckLightOn);
	//if (storage.beeperCnt>0) SingleBeep(2);

	Serial.begin(115200);
	Serial.print(F("Playing With Fusion: AS3935 Lightning Sensor, SEN-39001-R01  v"));
	Serial.print(majorVersion);
	Serial.print(F("."));
	Serial.println(minorVersion);
	Serial.println(F("beginning boot procedure...."));
	Serial.println(F("Start display"));

	if (EEPROM.read(offsetEEPROM) != storage.chkDigit || analogRead(BUTTON)<500){
		Serial.println(F("Writing defaults...."));
		saveConfig();
	}

	loadConfig();
	printConfig();

	display.begin(84, 48, storage.contrast);
	delay(200);
	printLogo();
	delay(1000);

	esp8266.begin(9600); // Start de software monitor op 9600 baud.

	Serial.println(F("Type GS to enter setup:"));
	delay(5000);
	if (Serial.available()) {
		Serial.println(F("Check for setup"));
		if (Serial.find(chkGS)) {
			Serial.println(F("Setup entered..."));
			setSettings(1);
			delay(2000);
		}
	}

	// setup for the the I2C library: (enable pullups, set speed to 400kHz)
	I2c.begin();
	I2c.pullup(true);
	I2c.setSpeed(1);
	delay(2);

	lightning0.AS3935_DefInit();   // set registers to default
	lightning0.AS3935_ManualCal(storage.AS3935_Capacitance, storage.doorMode, storage.distMode);
	lightning0.AS3935_SetNoiseFloorLvl(storage.AS3935_NoiseFloorLvl);
	lightning0.AS3935_SetSpikeRejection(storage.AS3935_SpikeRejection);
	delay(10);

	cli(); // disable all interrupts
	configure_wdt();     // enable the watchdog
	configure_timer();
	sei(); // enable all interrupts

	lightning0.AS3935_PrintAllRegs();
	AS3935_ISR_Trig = 0;           // clear trigger
	delay(10);

	printLogo();
	delay(1000);

	Serial.print(F("Clear array...."));
	for (int i = 0; i < 10; i++) {
		lastData[i].dw = 0;
		lastData[i].hr = 0;
		lastData[i].mn = 0;
		lastData[i].sc = 0;
		lastData[i].dt = 0;
		Serial.print(F("."));
	}
	Serial.println();
	Serial.println(F("Array cleared...."));

	display.clear();
	outputCalibrationValues();

	InitWiFiConnection();
	myButton = analogRead(BUTTON);
	display.clear();
	printInfo();
}

void configure_wdt(void)
{
	MCUSR = 0;                       // reset status register flags
	// Put timer in interrupt-only mode:
	WDTCSR |= 0b00011000;            // Set WDCE (5th from left) and WDE (4th from left) to enter config mode,
	// using bitwise OR assignment (leaves other bits unchanged).
	WDTCSR =  0b01000000 | 0b100001; // set WDIE: interrupt enabled
	// clr WDE: reset disabled
	// and set delay interval (right side of bar) to 8 seconds

	// reminder of the definitions for the time before firing
	// delay interval patterns:
	//  16 ms:     0b000000
	//  500 ms:    0b000101
	//  1 second:  0b000110
	//  2 seconds: 0b000111
	//  4 seconds: 0b100000
	//  8 seconds: 0b100001
}

void configure_timer() {
	TCCR1A = 0; // set entire TCCR1A register to 0
	TCCR1B = 0; // same for TCCR1B
	TCNT1 = 34286; // preload timer 65536-16MHz/256/2Hz
	TCCR1B |= (1 << CS12);
	TIMSK1 |= (1 << TOIE1);
}

void InitWiFiConnection() {
	isConnected  = false;
	isAsleep = false;
	Serial.println(F("Hard reset..."));
	if (wifi.hardReset()) {
		Serial.println(F("Soft reset..."));
		if (wifi.softReset()) {
			Serial.println(F("Set TimeOuts..."));
			wifi.setTimeouts(1000,5000,15000);
			Serial.println(F("Connecting to WiFi..."));
			if (wifi.connectToAP(storage.ESP_SSID, storage.ESP_PASS)) {
				Serial.println(F("OK\nChecking IP addr..."));
				if (wifi.checkIP()) {
					isConnected = true;
					if (wifi.setServerMode(0)) {
						Serial.println(F("Servermode set"));
					} else {
						Serial.println(F("Servermode already set"));
					}
					if (wifi.setMultiConnect(0)) Serial.println(F("Set to single connect mode"));
				} else { // IP addr check failed
					Serial.println(F("Failed: No IP number"));
				}
			} else { // WiFi connection failed
				Serial.println(F("Failed: connecting"));
			}
		} else {
			Serial.println(F("Failed: software reset"));
		}
	} else {
		Serial.println(F("Failed: hardware reset"));
	}
	if (isConnected == true) {
		esp8266.println(F("AT+CIFSR"));
		getData(500);
		for (int i = 0; i < 3; i++) if (hour == 0) getNTPData();
	}
}

void sendToSite(byte whichInt, byte dist) {
	if (isAsleep == true) {
		InitWiFiConnection();
	}
	if (isConnected == 1) {
		if (wifi.connectTCP(F("onweer.pi4raz.nl"), 80)) {
			//getData(500);
			int recLen = 85;
			recLen = recLen + sizeof(storage.MyCall);
			if (dist > 9) recLen++;
			Serial.println(recLen);
			if (wifi.sendTCP(recLen)) {
				esp8266.print(F("GET /AddEvent.php?Callsign="));
				esp8266.print(storage.MyCall);
				esp8266.print(F("&IntType=")); esp8266.print(whichInt); esp8266.print(F("&Distance="));
				esp8266.print(dist);
				esp8266.println(F(" HTTP/1.1\r\nHost: onweer.pi4raz.nl"));
				esp8266.println();
				esp8266.println();
				getData(500);
				getData(500);
				getData(500);
			}
			wifi.closeTCP();
		} else {
			wifi.closeTCP();
		}
	}
}

void getNTPData() {
	if (isAsleep == true) {
		InitWiFiConnection();
	}
	if (isConnected == 1) {
		if (wifi.connectTCP(F("onweer.pi4raz.nl"), 80)) {
			getData(500);
			if (wifi.sendTCP(52)) {
				esp8266.println(F("GET /time.php HTTP/1.1\r\nHost: onweer.pi4raz.nl"));
				esp8266.println();
				esp8266.println();
				getData(500);
				getData(500);
				getData(500);
			}
			wifi.closeTCP();
		} else {
			wifi.closeTCP();
		}
	}
}

void getData(int timeout) {
	Serial.println(F("------"));
	receivedString[0] = 0;
	int i = 0;
	long int time = millis();
	while ( (time + timeout) > millis()) {
		while (esp8266.available() && i < 125) {
			receivedString[i] = esp8266.read();
			i++;
		}
	}
	receivedString[i] = 0;

	if (i > 0) {
		if (debug == 1) {
			Serial.println(receivedString);
			Serial.println(F("++++++"));
		}
		CheckStatus();
	}
}

void CheckStatus() {
	if (strstr(receivedString, "Date:") > 0) {
		Serial.print(F("Date&Time found")); Serial.println();
		int j = 0;
		for (int i = 0; i < 128; i++) {
			if (receivedString[i] == ':' && j == 0) {
				j++;
				i++;
			}
			if (receivedString[i] == ':' && j == 1) {
				j++;
				if (receivedString[i + 2] == 'S' && receivedString[i + 3] == 'u' && receivedString[i + 4] == 'n') dayOfWeek = 1;
				if (receivedString[i + 2] == 'M' && receivedString[i + 3] == 'o' && receivedString[i + 4] == 'n') dayOfWeek = 2;
				if (receivedString[i + 2] == 'T' && receivedString[i + 3] == 'u' && receivedString[i + 4] == 'e') dayOfWeek = 3;
				if (receivedString[i + 2] == 'W' && receivedString[i + 3] == 'e' && receivedString[i + 4] == 'd') dayOfWeek = 4;
				if (receivedString[i + 2] == 'T' && receivedString[i + 3] == 'h' && receivedString[i + 4] == 'u') dayOfWeek = 5;
				if (receivedString[i + 2] == 'F' && receivedString[i + 3] == 'r' && receivedString[i + 4] == 'i') dayOfWeek = 6;
				if (receivedString[i + 2] == 'S' && receivedString[i + 3] == 'a' && receivedString[i + 4] == 't') dayOfWeek = 7;
				i++;
			}
			if (receivedString[i] == ':' && j == 2) {
				j++;
				hour = ((receivedString[i - 2] - 48) * 10) + (receivedString[i - 1] - 48);
				minute = ((receivedString[i + 1] - 48) * 10) + (receivedString[i + 2] - 48);
				second = ((receivedString[i + 4] - 48) * 10) + (receivedString[i + 5] - 48);
				i++;
			}
		}
        hour = hour + storage.timeCorrection;
        if (hour > 23) {
          hour = hour - 24;
          dayOfWeek++;
          if (dayOfWeek > 7) dayOfWeek = 1;
        }
	}

	if (strstr(receivedString, "STAIP") > 0) {
		int i = 0;
		int j = 0;
		int k = 0;

		Serial.print(F("Got IP:")); Serial.println(i);
		Serial.println(receivedString);

		while (receivedString[i] != 0 && i < 125) {
			if (receivedString[i] == '\"') {
				if (j == 0 && k == 0) {
					j = i;
				}
				else {
					if (k == 0) {
						k = i;
					}
				}
			}
			i++;
		}
		if (k - j < 16 && k > j) {
			memcpy(IPNo, &receivedString[j + 1], k - (j + 1) );
			IPNo[k - (j + 1)] = 0;
		}
	}
}

#ifdef isColor
void printMinutes() {
	display.clear();
	display.setCursor(startPix, startPix);
	display.setTextColor(ST7735_YELLOW);
	display.setTextSize(2);
	display.print(F("Minutes"));
	display.setTextSize(1);
	display.setCursor(startPix, 20);
	display.print(F("Max/min:"));
	display.print(maxMinute);
	printGraph();
	display.setCursor(startPix, 113);
	display.print(F("M:"));

	if (dispSide == 0) {
		display.setCursor(startPix+5+77, 113);
		display.print('0');
		display.setCursor(startPix+5+47, 113);
		display.print(F("15"));
		display.setCursor(startPix+5+17, 113);
		display.print(F("30"));
		startPos = 0;
	}
	if (dispSide == 1) {
		display.setCursor(startPix+5+71, 113);
		display.print(F("15"));
		display.setCursor(startPix+5+47, 113);
		display.print(F("30"));
		display.setCursor(startPix+5+17, 113);
		display.print(F("45"));
		startPos = 15;
	}
	if (dispSide == 2) {
		display.setCursor(startPix+5+71, 113);
		display.print(F("30"));
		display.setCursor(startPix+5+47, 113);
		display.print(F("45"));
		display.setCursor(startPix+5+17, 113);
		display.print(F("60"));
		startPos = 30;
	}

	for (int i = 0; i < 30; i++) {
		height = minutes[(i + startPos)] / divMinute;
		if (height > 65) {
			height = 65;
			if (i == 0) {
				printArrow();
			}
		}
		display.drawLine(88 - (i * 2),  110 - (height),  88 - (i * 2), 110, GRFCLR);
	}
	display.setTextColor(ST7735_WHITE);
}

void printHours() {
	display.clear();
	display.setCursor(startPix, startPix);
	display.setTextColor(ST7735_GREEN);
	display.setTextSize(2);
	display.print(F("Hours"));
	display.setTextSize(1);
	display.setCursor(startPix, 20);
	display.print(F("Max/hour:"));
	display.print(maxHour);
	printGraph();
	display.setCursor(startPix, 113);
	display.print(F("H:"));

	if (dispSide == 0) {
		display.setCursor(startPix+5+77, 113);
		display.print('0');
		display.setCursor(startPix+5+53, 113);
		display.print(F("12"));
		display.setCursor(startPix+5+29, 113);
		display.print(F("24"));
		startPos = 0;
	}
	if (dispSide == 1) {
		display.setCursor(startPix+5+71, 113);
		display.print(F("12"));
		display.setCursor(startPix+5+53, 113);
		display.print(F("24"));
		display.setCursor(startPix+5+29, 113);
		display.print(F("36"));
		startPos = 12;
	}
	if (dispSide == 2) {
		display.setCursor(startPix+5+71, 113);
		display.print(F("24"));
		display.setCursor(startPix+5+53, 113);
		display.print(F("36"));
		display.setCursor(startPix+5+29, 113);
		display.print(F("48"));
		startPos = 24;
	}
	for (int i = 0; i < 24; i++) {
		height = hours[(i + startPos)] / divHour;
		if (height > 65) {
			height = 65;
			if (i == 0) {
				printArrow();
			}
		}
		display.drawLine(88 - (i * 2),  110 - (height),  88 - (i * 2), 110, GRFCLR);
	}
	display.setTextColor(ST7735_WHITE);
}

void printDays() {
	display.clear();
	display.setCursor(startPix, startPix);
	display.setTextColor(ST7735_BLUE);
	display.setTextSize(2);
	display.print(F("Days"));
	display.setTextSize(1);
	display.setCursor(startPix, 20);
	display.print(F("Max/day:"));
	display.print(maxDay);
	printGraph();
	display.setCursor(startPix, 113);
	display.print(F("D:"));

	display.setCursor(startPix+5+77, 113);
	display.print('0');
	display.setCursor(startPix+5+47, 113);
	display.print(F("15"));
	display.setCursor(startPix+5+17, 113);
	display.print(F("30"));

	for (int i = 0; i < 30; i++) {
		height = days[i] / divDay;
		if (height > 65) {
			height = 65;
			if (i == 0) {
				printArrow();
			}
		}
		display.drawLine(88 - (i * 2),  110 - (height),  88 - (i * 2), 110, GRFCLR);
	}
	display.setTextColor(ST7735_WHITE);
}

void printGraph() {
	display.setCursor(0, 45);
	display.print(100);
	display.setCursor(0, 65);
	display.print(75);
	display.setCursor(0, 85);
	display.print(50);
	display.setCursor(0, 105);
	display.print(25);
	display.drawLine( 20,  44,  25, 44, LINECLR);
	display.drawLine( 20,  64,  25, 64, LINECLR);
	display.drawLine( 20,  84,  25, 84, LINECLR);
	display.drawLine( 20,  104,  25, 104, LINECLR);
	display.drawLine( 25,  110,  89, 110, LINECLR);
	display.drawLine( 25,  111,  89, 111, LINECLR);
	display.drawLine( 25,  44,  25, 110, LINECLR);
	display.drawLine( 24,  44,  24, 111, LINECLR);
	display.setCursor(0, 0);
}

void printArrow() {
	display.drawLine( 86,  42,  88, 44, GRFCLR);
	display.drawLine( 86,  46,  88, 44, GRFCLR);
}
#endif














