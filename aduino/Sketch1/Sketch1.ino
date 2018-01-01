#define DEBUG

#include <Timer.h>
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
#include <OneButton.h>
//#include <radio.h>
//#include <TEA5767.h>
#include <TEA5767N.h>
#include <RotaryEncoder.h>
#include <EEPROM.h>


/*
		|		푸시 스위치 1			|			푸시 스위치 2
--------+-----------+---------------+-------+-------------------
Playing |클릭		|다음 채널		|		|		
--------+-----------+---------------+-------+------------------
Playing |더블클릭		|다음 주파수 검색	|		|		
--------+-----------+---------------+-------+------------------
Playing |롱클릭		|채널 저장모드	|		|		
--------+-----------+---------------+-------+------------------
Channel |	채널선택					|				돌아가기


			로터리엔코더 1			|			로터리엔코더 1
--------+---------------------------+--------------------------
Playing |	FREQ (+/-)				|			VOL (+/-)
--------+---------------------------+--------------------------
Channel |	다른채널					|			None	


*/


/*
	화면 1 (라디오 재생) - Playing

0| FM Radio Freq 81.9 Mhz [S]|[M] [CH1]|[?] 
1| ----------------------------------------
2| SIGNAL [9] <Meter> 
3| 
*/


/*
	화면 2 (채널 저장 모드) - Channel

0| FM Radio Freq 81.9 Mhz [S]|[M] [CH1]|[?]
1| ----------------------------------------
2| -> [CH 0] : 89.1 Mhz 	
3|	SAVE					Back
*/


// https://github.com/mroger/TEA5767
// https://www.electronicsblog.net/arduino-fm-receiver-with-tea5767/
// http://mr0ger-arduino.blogspot.kr/2014/08/tea5767n-fm-philips-library-for-arduino.html


////////////////////////////////////////////////////////////////////////////////////////////////////////////////

byte meter[8][8] = 
{
	{ 0b00000,0b00000,0b00000,0b00000,0b00000,0b00000,0b00000,0b11111 },
	{ 0b00000,0b00000,0b00000,0b00000,0b00000,0b00000,0b11111,0b11111 },
	{ 0b00000,0b00000,0b00000,0b00000,0b00000,0b11111,0b11111,0b11111 },
	{ 0b00000,0b00000,0b00000,0b00000,0b11111,0b11111,0b11111,0b11111 },
	{ 0b00000,0b00000,0b00000,0b11111,0b11111,0b11111,0b11111,0b11111 },
	{ 0b00000,0b00000,0b11111,0b11111,0b11111,0b11111,0b11111,0b11111 },
	{ 0b00000,0b11111,0b11111,0b11111,0b11111,0b11111,0b11111,0b11111 },
	{ 0b11111,0b11111,0b11111,0b11111,0b11111,0b11111,0b11111,0b11111 }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////

LiquidCrystal_I2C lcd(0x3F, 16, 4);

//I2C device found at address 0x3F  !
//I2C device found at address 0x60  !

//TEA5767 radio;

TEA5767N radio = TEA5767N();

RotaryEncoder encoder0(A0, A1);
RotaryEncoder encoder1(A2, A3);

Timer ts;
int samplingT = 1000;		// 1sec

OneButton button1(11, true);
OneButton button2(12, true);

#define MIN_FM_FREQ		88.0
#define MAX_FM_FREQ		108.0

#define MAX_CHANNEL_NUM			10

#define	MODE_PLAYING			0
#define MODE_SELECT_CHANNEL		1
#define MODE_SCAN_CHANNEL		2

int current_mode = MODE_PLAYING;

byte current_channel = 0;
float current_freq = 0;
int prev_signal = 0;

#define BACKLIGHT_TIMEOUT	20	// 20 sec
bool isbackligtOn = true;
int backlightTime = 0;

/*
0: KBS2 FM: 89.1
1: MBC FM4U : 91.9
2: KBS1 FM : 93.1
3: CBS FM : 93.9
4: 교통방송 : 95.1
5: MBC : 95.9
6: KBS1 : 97.3
7: CBS 98.1
8: SBS LOVE FM : 103.5
9: KBS2 : 106.1
10: SBS POWER FM : 107.7
*/
struct BandInfo
{
	float freq;
	String name;
};

BandInfo radiobandlist[10] = {
	{ 89.10, "KBS2 FM"}, 
	{ 91.90, "MBC FM4U" },
	{ 93.10, "KBS1 FM" },
	{ 93.90, "CBS FM" },
	{ 95.90, "MBC" },
	{ 97.30, "KBS1" },
	{ 98.10, "CBS" },
	{ 103.50, "SBS LOVE FM" },
	{ 106.10, "KBS2" },
	{ 107.70, "SBS POWER FM" }
};

float channellist[MAX_CHANNEL_NUM] = { 0, };

////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void loadLastChannel()
{
	uint8_t r = EEPROM.read(0);
	if(r != 0xffff)
		current_channel = r;
}

void saveLastChannel() 
{
	EEPROM.write(0, current_channel);
}

void initRadio()
{
	// TODO. current_channel 로드해야함
	// channellist 로드해야함

	// 임시로 넣어둠
	for (int i = 0; i < MAX_CHANNEL_NUM; i++)
		channellist[i] = radiobandlist[i].freq;

	//radio.setStereoNoiseCancellingOn();
	//radio.setStereoNoiseCancellingOff();

	radio.setStereoReception();
	//radio.setMonoReception();
	radio.turnTheSoundBackOn();

	current_freq = channellist[current_channel];
	radio.selectFrequency(current_freq);

	printRadioInfo(0);
}

void initTimer()
{
	ts.every(samplingT, doTimer);
}

void initLCD()
{
	lcd.begin();
	lcd.clear();

	for (int k = 0; k < 8; k++)
		lcd.createChar(k, meter[k]);
}


void setup()
{
#ifdef DEBUG
	Serial.begin(9600);
#endif
	Wire.begin();

	loadLastChannel();

	initTimer();
	initLCD();
	initRadio();

	button1.attachClick(click1);
	button2.attachClick(click2);
	button1.attachDoubleClick(doubleclick1);
	button2.attachDoubleClick(doubleclick2);
	button1.attachDuringLongPress(longPress1);
	button2.attachDuringLongPress(longPress2);
	button1.attachLongPressStart(longPressStart1);
	button2.attachLongPressStart(longPressStart2);

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void printLCD(int line, String str)
{
	lcd.setCursor(0, line);
	lcd.print("                    ");
	lcd.setCursor(0, line);
	lcd.print(str);
}

void customclear()
{
	// ensures all custom character slots are clear before new custom
	// characters can be defined. 
	byte blank[8] =
	{
		B00000, B00000, B00000, B00000, B00000, B00000, B00000, B00000
	};
	for (int i = 0; i < 8; i++)
	{
		lcd.createChar(i, blank);
	}
}

// 주파수로 이름 얻기
String seachChannel(float freq)
{
	for (int i = 0; i < MAX_CHANNEL_NUM; i++)
	{
		if (radiobandlist[i].freq == freq)
			return radiobandlist[i].name;
	}
	return String("????");
}

void printRadioInfo(int line)
{
	//FM Radio 81.9 Mhz [S] | [M]
	byte isStereo = radio.isStereo();
	String stereoStr = isStereo == 1 ? "[S]" : "[M]";
	String str = "FM " + String(current_freq) + " Mhz " + stereoStr;
	lcd.setCursor(0, line);
	lcd.print("                    ");
	lcd.setCursor(0, line);
	lcd.print(str);

#ifdef DEBUG
	String debug = "---> " + String(isStereo);
	Serial.println(debug);
#endif

	lcd.setCursor(0, line + 1);
	lcd.print("                    ");
	String channelStr = "[CH" + String(current_channel) + "]";
	String channelName = seachChannel(current_freq);
	if (channelName == "????")
		channelStr = "[????]";

	String str2 = channelStr + "[" + channelName + "]";
	lcd.setCursor(0, line+1);
	lcd.print(str2);

	lcd.setCursor(0, line + 2);
	lcd.print("--------------------");
}

void printSignalMeterLCD(int line, int value)
{
	value = value > 10 ? 10 : value;

	lcd.setCursor(0, line);
	String str = "TUNE [" + String(value) + "] ";
	lcd.print(str);
	int pos = str.length();

	for (int j = 0; j < 20-pos; j++)
	{
		lcd.setCursor(pos+j, line);
		lcd.print(" ");
	}

	for (int i = 0; i < value; i++)
	{
		lcd.setCursor(pos+i, line);
		byte c = i > 7 ? 7 : (byte)i;
		lcd.write(c);
	}
}

void changeRadioChannel(int ch)
{
	current_channel = ch;
	current_freq = channellist[current_channel];
	radio.selectFrequency(current_freq);
	printRadioInfo(0);

#ifdef DEBUG
	Serial.println(ch);
	Serial.println(current_freq);
#endif


	turnonBackLight();
	saveLastChannel();
}

void changeRadioFreq(float freq)
{
	current_freq = freq;
	radio.selectFrequency(current_freq);
	printRadioInfo(0);

#ifdef DEBUG
	String str = "FREQ : " + String(current_freq);
	Serial.println(str);
#endif

	turnonBackLight();
}

int encoder0lastpos = 0;
int encoder1lastpos = 0;

void updateEncoder()
{
	encoder0.tick();
	encoder1.tick();

	int new0 = encoder0.getPosition();
	int new1 = encoder1.getPosition();

	if (encoder0lastpos != new0)
	{
		if (new0 > encoder0lastpos)
		{
			// +
			int ch = current_channel + 1 > MAX_CHANNEL_NUM-1 ? 0 : current_channel + 1;
			changeRadioChannel(ch);
		}
		else
		{
			// -
			int ch = current_channel - 1 < 0 ? MAX_CHANNEL_NUM-1 : current_channel - 1;
			changeRadioChannel(ch);
		}
		encoder0lastpos = new0;
	}

	if (encoder1lastpos != new1)
	{
		float freq = current_freq;

		if (new1 > encoder1lastpos)
		{
			// +
			freq += 0.10;
			changeRadioFreq(freq);
		}
		else
		{
			// -
			freq -= 0.10;
			changeRadioFreq(freq);
		}
		encoder1lastpos = new1;
	}
}

void loop() 
{
	updateEncoder();
	ts.update();

	button1.tick();
	button2.tick();

	/*
	if (LOW == digitalRead(11))
	{
		Serial.println("Button 1 click.");
	}

	if (LOW == digitalRead(12))
	{
		Serial.println("Button 2 click.");
	}
	*/
}

void checkSignal()
{
	int signal = radio.getSignalLevel();
	if (prev_signal != signal)
	{
//		String str = "Signal : " + String(signal);
//		Serial.println(str);
		printSignalMeterLCD(3, signal);
		prev_signal = signal;
	}
}

void turnonBackLight()
{
	backlightTime = 0;
	if (isbackligtOn == false)
	{
		lcd.backlight();
		isbackligtOn = true;
	}

}

void checkBackLight()
{
	if (isbackligtOn == true)
	{
		backlightTime++;
		if (backlightTime > BACKLIGHT_TIMEOUT)
		{
			lcd.noBacklight();
			isbackligtOn = false;
		}
	}
}

void doTimer() 
{
	checkSignal();
	checkBackLight();
}

void scanRadio()
{
	byte isBandLimitReached = radio.startsSearchMutingFromBeginning();

	float freq = radio.readFrequencyInMHz();
	Serial.print("Station found: ");
	Serial.print(freq, 1);
	Serial.println(" MHz");
	delay(2000);

	while (!isBandLimitReached) 
	{
		Serial.println("Search Up in progress...");
		//If you want listen to station search, use radio.searchNext() instead
		isBandLimitReached = radio.searchNextMuting();

		Serial.print("Band limit reached? ");
		Serial.println(isBandLimitReached ? "Yes" : "No");
		delay(1000);

		freq = radio.readFrequencyInMHz();
		Serial.print("Station found: ");
		Serial.print(freq, 1);
		Serial.println(" MHz");

		int signal = radio.getSignalLevel();
		Serial.println("Signal");
		Serial.println(signal);

		delay(1000);
	}

}

void scanNextRadio()
{
	printLCD(0, "Scan Start !");

	float freq = radio.readFrequencyInMHz();

	freq += 0.10;
	freq = freq < MIN_FM_FREQ ? MIN_FM_FREQ : freq;
	freq = freq > MAX_FM_FREQ ? MIN_FM_FREQ : freq;
	byte isBandLimitReached = radio.startsSearchFrom(freq);

#ifdef DEBUG
	Serial.print("Station found: ");
	Serial.print(freq, 1);
	Serial.println(" MHz");
	delay(2000);
#endif

	isBandLimitReached = radio.searchNextMuting();
	freq = radio.readFrequencyInMHz();
	changeRadioFreq(freq);

#ifdef DEBUG
	Serial.print("Station found: ");
	Serial.print(freq, 1);
	Serial.println(" MHz");

	int signal = radio.getSignalLevel();
	Serial.println("Signal");
	Serial.println(signal);
#endif

	changeMode(MODE_PLAYING);
}

////////////////////////////////////////////////////////////////////////////////////////

void changeMode(int mode)
{
	if (current_mode == mode)
		return;

	current_mode = mode;

	switch (mode)
	{
		case MODE_PLAYING :
			Serial.println("---> MODE_PLAYING");
			break;

		case MODE_SELECT_CHANNEL:
			Serial.println("---> MODE_SELECT_CHANNEL");
			break;

		case MODE_SCAN_CHANNEL:
			Serial.println("---> MODE_SCAN_CHANNEL");
			scanNextRadio();
			break;
	}
}

////////////////////////////////////////////////////////////////////////////////////////


// This function will be called when the button1 was pressed 1 time (and no 2. button press followed).
void click1() {
	Serial.println("Button 1 click.");
} 


  // This function will be called when the button1 was pressed 2 times in a short timeframe.
void doubleclick1() {
	Serial.println("Button 1 doubleclick.");
} // doubleclick1


  // This function will be called once, when the button1 is pressed for a long time.
void longPressStart1() {
	Serial.println("Button 1 longPress start");
	changeMode(MODE_SCAN_CHANNEL);
} // longPressStart1


  // This function will be called often, while the button1 is pressed for a long time.
void longPress1() {
	Serial.println("Button 1 longPress...");
} // longPress1


  // This function will be called once, when the button1 is released after beeing pressed for a long time.
void longPressStop1() {
	Serial.println("Button 1 longPress stop");
} // longPressStop1


  // ... and the same for button 2:

void click2() {
	Serial.println("Button 2 click.");
	changeMode(MODE_PLAYING);
} 


void doubleclick2() {
	Serial.println("Button 2 doubleclick.");
} // doubleclick2


void longPressStart2() {
	Serial.println("Button 2 longPress start");
} // longPressStart2


void longPress2() {
	Serial.println("Button 2 longPress...");
} // longPress2

void longPressStop2() {
	Serial.println("Button 2 longPress stop");
} // longPressStop2
