/*
 Name:		Sketch1.ino
 Created:	13/09/2016 23:32:06
 Author:	MOLLO Alain
*/

// the setup function runs once when you press reset or power the board
#include <Wire.h>
#include <LowPower.h>
#include <Sim800LX.h>
#include <LiquidCrystal.h>

#define HARD_SERIAL_OK

#define DS1307_ADDRESS 0x68

uint8_t stepper;
uint32_t counter;
bool smsReceived = false;

// Setup LCD library
LiquidCrystal lcd(12, 11, 10, 9, 8, 7);

//Setup Sim800L library
Sim800LX Sim800;

void smsTreatCommand(Sim800LX::smsReader * smsCommand);

// Init method
void setup() {
	counter = 0;
	stepper = 0;

	Wire.begin();

	setUpLcd();

#ifdef HARD_SERIAL_OK
	Serial.begin(115200);
	Serial.println(F("Arduino start..."));
#endif

	lcd.clear();
	lcd.setCursor(0, 0);
	lcd.print(F("Arduino start ..."));

	// Hardware reset Sim800L module
	Sim800.reset();

	lcd.clear();
	lcd.setCursor(0, 0);
	lcd.print(F("Sim800L is ready"));

	// Turn on/off Led flashing
#ifdef DEBUG_MODE_SET
	Sim800.setOnLedFlash();
#else
	Sim800.setOffLedFlash();
#endif

#ifdef HARD_SERIAL_OK
	// Set sim800 serial interface baud rate
	Sim800.sendAtPlusCommand(F("IPR=9600"));
	Sim800.waitOK();
#endif

	// Save sim800 eeprom settings
	Sim800.saveAllSettings();

	// Wait for signal recepetion level > 0
	uint8_t sig = Sim800.waitSignal();

#ifdef HARD_SERIAL_OK
	Serial.print(F("Signal quality = "));
	Serial.println(String(sig, DEC));
#endif

	lcd.setCursor(0, 1);
	lcd.print(F("Signal level="));
	lcd.print(String(sig, DEC));

	// Attach ring hardware interruption
	attachInterrupt(digitalPinToInterrupt(2), ringinterrupt, CHANGE);
}

// Sim800L hardwware ring signal interruption
void ringinterrupt(void)
{
	smsReceived = true;
}

// the loop function runs over and over again until power down or reset
void loop()
{
#ifdef HARD_SERIAL_OK
	// if data is in rx buffer
	if (Serial.available())
	{
		// read the buffer
		String message = Serial.readString();

		// if string start with AT => translate command directly to the Sim800L module
		if (message.startsWith(F("AT")))
			Sim800.print(message);
		else
		{
			// Else this is a compute command
			Sim800LX::smsReader * smsCommand = new Sim800LX::smsReader();
			smsCommand->Message = message;
			smsTreatCommand(smsCommand);
		}
		delay(100);
	}
#endif

	// Just for read serial communication from Sim800L and send on debug output
	if (Sim800.available())
	{
		String sim800data = Sim800.readString();

#ifdef HARD_SERIAL_OK
		Serial.print(sim800data);
#endif

		if (sim800data.indexOf(F("+CMTI:")) != -1 || smsReceived)
		{
			// An sms was received ...
			// We shoulf launch a reset and let start up check SMS

			lcd.setCursor(0, 1);
			lcd.print(F("SMS detection   "));

			smsReceived = false;

#ifdef HARD_SERIAL_OK
			Serial.println(F("SMS was detected ..."));
#endif

			delay(1000);

			// Check if SMS is present
			CheckSMS();
		}
		delay(100);
	}

	// An alive signal on debug output wich permit to detect sleep mode ! or alive
	if (counter++ > 300000)
	{
		if (stepper++ > 10)
		{
			stepper = 0;
			CheckSMS();
		}

		lcd.clear();
		lcd.setCursor(0, 0);
		for (int i = 0; i < stepper; i++)
		{
			lcd.print(F("*"));
		}

		counter = 0;

#ifdef HARD_SERIAL_OK
		Serial.println("alive ...");
		printDate();
#endif
	}
}

void setUpLcd(void)
{
	lcd.begin(16, 2);
	lcd.clear();
}

// Check if SMS was arrived
void CheckSMS(void)
{
#ifdef HARD_SERIAL_OK
	Serial.println(F("CheckSMS launch..."));
#endif

	lcd.clear();
	lcd.setCursor(0, 0);
	lcd.println(F("CheckSMS launch."));

	// Check Sim800L not sleeping !
	Sim800.sendAtCommand(F(""));
	if (!Sim800.waitOK())
	{
		// Reset arduino and make setup cycle to refresh Sim800L
		Sim800.reset();
	}

	int i = 1;
	Sim800LX::smsReader * SMS;
	do
	{
		SMS = Sim800.readSms(i);
		if (SMS != nullptr)
		{
#ifdef HARD_SERIAL_OK
			Serial.println(F("SMS reading =>"));
			Serial.println("who  : " + SMS->WhoSend);
			Serial.println("when : " + SMS->WhenSend);
			Serial.println("what : " + SMS->Message);
#endif

			Sim800.delSms(i++);

			if (SMS->Message.indexOf(F("AT")) != -1)
			{
				Sim800.print(SMS->Message);
				String buffer = Sim800.readString();
				Sim800.sendSms("+336xxxxxxxx", buffer);
			}
			else
				smsTreatCommand(SMS);
		}
		else
		{
#ifdef HARD_SERIAL_OK
			Serial.println(F("No message..."));
#endif
		}
	} while (SMS != nullptr);
}

// Treat Sms Command incomming
void smsTreatCommand(Sim800LX::smsReader * smsCommand)
{
	String message = smsCommand->Message;

	// Clean all SMS
	if (message.indexOf(F("CLEAN")) != -1)
	{
		if (Sim800.delAllSms())
			ResponseOut(F("Cleaning sms"));
		else
			ResponseOut(F("Error cleaning"));
	}

	// Read SMS
	if (message.indexOf(F("READ")) != -1)
	{
		ResponseOut(F("Reading sms"));
		CheckSMS();
	}

	// Reset the Arduino
	if (message.indexOf(F("RESET")) != -1)
	{
		ResponseOut(F("Reset system"));
		delay(1000);

		// Reset arduino and make setup cycle
		software_Reset();
	}

	// Show DateTime
	if (message.indexOf(F("CLOCK")) != -1)
	{
		Sim800LX::dateTime * dte = new Sim800LX::dateTime();
		String dateTime = Sim800.RTCtime(dte);

#ifdef HARD_SERIAL_OK
		Serial.print(String(dte->day, DEC));
		Serial.print(F("/"));
		Serial.print(String(dte->month, DEC));
		Serial.print(F("/"));
		Serial.print(String(dte->year, DEC));
		Serial.println(F(""));
#endif

		ResponseOut(dateTime);
	}

	// Put Sim800 and Arduino in deep sleep mode wich can be wake up by ring signal or rx signal (SMS)
	if (message.indexOf(F("SLEEP")) != -1)
	{
		lcd.setCursor(0, 1);
		lcd.print(F("Set sleep mode."));

#ifdef HARD_SERIAL_OK
		Serial.println(F("Set sleep mode."));
#endif

		// Sim800L go to deep sleep mode (able to receive SMS)
		Sim800.sleepMode();

		// Put arduino in deep sleep mode for lower power consumption
		delay(1000);
		LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF);

		// After back from sleep, reset the Sim800 module
		Sim800.reset();
	}

	if (message.indexOf(F("PDOWN")) != -1)
	{
		lcd.setCursor(0, 1);
		lcd.print(F("Set PDown mode."));

#ifdef HARD_SERIAL_OK
		Serial.println(F("Set PDown mode."));
#endif

		// Sim800L go to deep sleep mode (able to receive SMS)
		Sim800.powerDownMode();

		// Wait for pdown mode ok
		Sim800.waitResponse(F("NORMAL POWER DOWN"));

		// Put arduino in deep sleep mode for lower power consumption
		delay(1000);
		LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF);

		// After back from sleep, reset the Sim800 module
		Sim800.reset();
	}
}

void ResponseOut(const __FlashStringHelper * response)
{
	lcd.setCursor(0, 1);
	lcd.print(response);

#ifdef HARD_SERIAL_OK
	Serial.println(response);
#endif

	Sim800.sendSms("+336xxxxxxxx", response);
}

void ResponseOut(String response)
{
	lcd.setCursor(0, 1);
	lcd.print(response);

#ifdef HARD_SERIAL_OK
	Serial.println(response);
#endif

	Sim800.sendSms("+336xxxxxxxx", response);
}

// Restarts program from beginning but does not reset the peripherals and registers
void software_Reset(void)
{
	asm volatile ("  jmp 0");
}


byte bcdToDec(byte val) {
	// Convert binary coded decimal to normal decimal numbers
	return ((val / 16 * 10) + (val % 16));
}

void printDate() {

	// Reset the register pointer
	Wire.beginTransmission(DS1307_ADDRESS);

	byte zero = 0x00;
	Wire.write(zero);
	Wire.endTransmission();

	Wire.requestFrom(DS1307_ADDRESS, 7);

	int second = bcdToDec(Wire.read());
	int minute = bcdToDec(Wire.read());
	int hour = bcdToDec(Wire.read() & 0b111111); //24 hour time
	int weekDay = bcdToDec(Wire.read()); //0-6 -> sunday - Saturday
	int monthDay = bcdToDec(Wire.read());
	int month = bcdToDec(Wire.read());
	int year = bcdToDec(Wire.read());

	//print the date EG   3/1/11 23:59:59
	Serial.print(month);
	Serial.print("/");
	Serial.print(monthDay);
	Serial.print("/");
	Serial.print(year);
	Serial.print(" ");
	Serial.print(hour);
	Serial.print(":");
	Serial.print(minute);
	Serial.print(":");
	Serial.println(second);

}
