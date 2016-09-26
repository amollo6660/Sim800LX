/*
 Name:		Sketch1.ino
 Created:	13/09/2016 23:32:06
 Author:	MOLLO Alain
*/

// the setup function runs once when you press reset or power the board
#include <LowPower.h>
#include <Sim800LX.h>
#include <LiquidCrystal.h>

uint8_t stepper;
uint32_t counter;

// Setup LCD library
LiquidCrystal lcd(12, 11, 10, 9, 8, 7);

//Setup Sim800L library
Sim800LX Sim800;

void smsTreatCommand(Sim800LX::smsReader * smsCommand);

// Init method
void setup() {
	counter = 0;
	stepper = 0;

	setUpLcd();
	Serial.begin(115200);

	Serial.println(F("Arduino start..."));

	lcd.clear();
	lcd.setCursor(0, 0);
	lcd.print(F("Arduino start ..."));

	// Hardware reset Sim800L module
	Sim800.reset();

	lcd.clear();
	lcd.setCursor(0, 0);
	lcd.print(F("Sim800L is ready"));

	// Set the RX interrupt method
	Sim800.setInterrupt(rxterrupt);

	// Turn on/off Led flashing
	Sim800.setOnLedFlash();
	// Sim800.setOffLedFlash();

	// Set sim800 serial interface baud rate
	Sim800.sendAtPlusCommand(F("IPR=9600"));
	Sim800.waitOK();

	// Save sim800 eeprom settings
	Sim800.saveAllSettings();

	// Wait for signal recepetion level > 0
	uint8_t sig = Sim800.waitSignal();

	Serial.print(F("Signal quality = "));
	Serial.println(String(sig, DEC));
	
	lcd.setCursor(0, 1);
	lcd.print(F("Signal level="));
	lcd.print(String(sig, DEC));

	CheckSMS();
}

// Software serial data communication interruption
void rxterrupt(void)
{
}

// Sim800L hardwware ring signal interruption
void ringinterrupt(void)
{
}

// the loop function runs over and over again until power down or reset
void loop()
{
	// if data is in rx buffer
	if (Serial.available())
	{
		// read the buffer
		String message = Serial.readString();

		// if string start with AT => translate command directly to the Sim800L module
		if (message.indexOf(F("AT")) != -1)
			Sim800.print(message);
		else
		{
			// Else this is a compute command
			Sim800LX::smsReader * smsCommand = new Sim800LX::smsReader();
			smsCommand->Message = message;
			smsTreatCommand(smsCommand);
		}
		delay(10);
	}

	// Just for read serial communication from Sim800L and send on debug output
	if (Sim800.available())
	{
		String sim800data = Sim800.readString();
		Serial.print(sim800data);
		if (sim800data.indexOf(F("+CMTI:")) != -1)
		{
			// An sms was received ...
			// We shoulf launch a reset and let start up check SMS

			lcd.setCursor(0, 1);
			lcd.print(F("SMS detection   "));

			Serial.println(F("SMS was detected ..."));

			Sim800.sendAtCommand(F(""));
			if (!Sim800.waitOK())
			{
				delay(1000);

				// Reset arduino and make setup cycle
				software_Reset();
			}
			else
			{
				CheckSMS();
			}
		}
		delay(10);
	}

	// An alive signal on debug output wich permit to detect sleep mode ! or alive
	if (counter++ > 300000)
	{
		if (stepper++ > 15) 
			stepper = 0;

		lcd.clear();
		lcd.setCursor(0, 0);
		for (int i = 0; i < stepper; i++)
		{
			lcd.print(F("*"));
		}

		counter = 0;
		Serial.println("alive ...");
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
	int i = 1;
	Sim800LX::smsReader * SMS;
	do
	{
		SMS = Sim800.readSms(i++);
		if (SMS != nullptr)
		{
			Serial.println("who  : " + SMS->WhoSend);
			Serial.println("when : " + SMS->WhenSend);
			Serial.println("what : " + SMS->Message);

			smsTreatCommand(SMS);
		}
		else
		{
			Serial.println(F("No message..."));
		}
	} while (SMS != nullptr);
	Sim800.delAllSms();
}

// Treat Sms Command incomming
void smsTreatCommand(Sim800LX::smsReader * smsCommand)
{
	String message = smsCommand->Message;

	// Clean all SMS
	if (message.indexOf(F("CLEAN")) != -1)
	{
		lcd.setCursor(0, 1);
		lcd.print(F("Cleaning sms    "));

		Serial.println(F("Cleaning sms"));
		Sim800.delAllSms();
	}

	// Send an SMS
	if (message.indexOf(F("SMS")) != -1)
	{
		lcd.setCursor(0, 1);
		lcd.print(F("Sending sms     "));

		Serial.println(F("Sending sms"));
		Sim800.sendSms("+336XXXXXXXX", "Hello World !");
	}

	// Read SMS
	if (message.indexOf(F("READ")) != -1)
	{
		lcd.setCursor(0, 1);
		lcd.print(F("Reading sms     "));

		Serial.println(F("Reading sms"));

		CheckSMS();
	}

	// Reset the Arduino
	if (message.indexOf(F("RESET")) != -1)
	{
		lcd.setCursor(0, 1);
		lcd.print(F("Reset system    "));

		Serial.println(F("Reset system"));
		delay(1000);

		// Reset arduino and make setup cycle
		software_Reset();
	}

	// Put Sim800 and Arduino in deep sleep mode wich can be wake up by ring signal or rx signal (SMS)
	if (message.indexOf(F("SLEEP")) != -1)
	{
		Serial.println(F("Put in sleep mode."));

		lcd.setCursor(0, 1);
		lcd.print(F("Enter sleep mode"));

		// Sim800L go to deep sleep mode (able to receive SMS)
		Sim800.sleepMode();

		// Attach rx interrupt method to Sim800L module interruption
		Sim800.startInterrupt();

		// Attach ring hardware interruption
		attachInterrupt(digitalPinToInterrupt(2), ringinterrupt, CHANGE);

		// Put arduino in deep sleep mode for lower power consumption
		delay(1000);
		LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF);
	}
}

// Restarts program from beginning but does not reset the peripherals and registers
void software_Reset(void)
{
	asm volatile ("  jmp 0");
}