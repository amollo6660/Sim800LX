/*
 Name:		Sketch1.ino
 Created:	13/09/2016 23:32:06
 Author:	MOLLO Alain
*/

// the setup function runs once when you press reset or power the board
#include <LowPower.h>
#include <Sim800LX.h>
#include <LiquidCrystal.h>

uint32_t counter;

// Setup LCD library
LiquidCrystal lcd(12, 11, 10, 9, 8, 7);

//Setup Sim800L library
Sim800LX Sim800;

// Init method
void setup() {
	counter = 0;

	setUpLcd();
	Serial.begin(115200);

	Serial.println(F("Arduino start ..."));

	Sim800.reset();
	delay(5000);

	// Wait for end of initialization
	Sim800.waitResponse(F("+CIEV"));
		
	// Set the RX interrupt method
	Sim800.setInterrupt(rxterrupt);

	// Turn on Led flashing
	Sim800.setOffLedFlash();
	Sim800.waitOK();

	// Set sim800 serial interface baud rate
	Sim800.sendAtPlusCommand(F("IPR=9600"));
	Sim800.waitOK();

	// Save sim800 eeprom settings
	Sim800.saveAllSettings();

	// Wait for signal recepetion level > 0
	waitSignal();
}

void waitSignal(void)
{
	uint8_t sig;
	while ((sig = Sim800.signalQuality()) == 0)
	{
		Serial.println(F("Wait for signal ..."));
		delay(1000);
		if (counter++ > 30)
		{
			Sim800.reset();
			delay(5000);
			Sim800.waitOK();
			counter = 0;
		}
	}
	Serial.print(F("Signal quality = "));
	Serial.println(String(sig, DEC));
}

void rxterrupt(void)
{
}

void ringinterrupt(void)
{
}

// the loop function runs over and over again until power down or reset
void loop()
{
	if (Serial.available())
	{
		String message = Serial.readString();
		if (message.indexOf(F("AT")) != -1)
			Sim800.print(message);
		else
		{
			if (message.indexOf(F("SMS")) != -1)
			{
				Serial.println(F("Sending sms"));
				Sim800.sendSms("+33614490515", "Hello World !");
			}
			if (message.indexOf(F("RESET")) != -1)
			{
				Serial.println(F("Reset system"));
				Sim800.reset();
				delay(5000);
				software_Reset();
			}
			if (message.indexOf(F("SLEEP")) != -1)
			{
				Serial.println(F("Put in sleep mode."));

				Sim800.sleepMode();

				// Attach interrupt method to Sim800 module interruption
				Sim800.startInterrupt();
				attachInterrupt(digitalPinToInterrupt(2), ringinterrupt, CHANGE);

				delay(5000);
				LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF);
			}
		}
		delay(10);
	}

	if (Sim800.available())
	{
		Serial.print(Sim800.readString());
		delay(10);
	}

	if (counter++ > 300000)
	{
		counter = 0;
		Serial.println("alive ...");
	}
}

void setUpLcd(void)
{
	lcd.begin(16, 2);
	lcd.clear();
}

// Restarts program from beginning but does not reset the peripherals and registers
void software_Reset(void)
{
	asm volatile ("  jmp 0");
}