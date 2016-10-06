/*
  Sim800LX.cpp

  Copyright (c) 2015, Arduino LLC
  Original code (pre-library): Copyright (c) 2016, Alain Mollo

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "Sim800LX.h"
#include "Arduino.h"

//================================================================================

// Base constructor
Sim800LX::Sim800LX(void)
	: SoftwareSerial(SIM800_RX_PIN_DEFAULT, SIM800_TX_PIN_DEFAULT)
{
	SetUp(SIM800_BAUD_RATE_DEFAULT, SIM800_RX_PIN_DEFAULT);
}

// Constructor with capabilities to set rx and tx pin's
Sim800LX::Sim800LX(uint8_t receivePin, uint8_t transmitPin)
	: SoftwareSerial(receivePin, transmitPin)
{
	SetUp(SIM800_BAUD_RATE_DEFAULT, receivePin);
}

// Constructor with capabilities to set rx and tx pin's and also baud rate
Sim800LX::Sim800LX(uint8_t receivePin, uint8_t transmitPin, long baudrate)
	: SoftwareSerial(receivePin, transmitPin)
{
	SetUp(baudrate, receivePin);
}

// Private factoring method SetUp
void Sim800LX::SetUp(long baudrate, uint8_t receivePin)
{
	_ReceivePin = receivePin;

	this->begin(baudrate);

	// reserve memory to prevent intern fragmention
	_buffer.reserve(255);

	// set reset pin as output and low level
	pinMode(RESET_PIN, OUTPUT);
	digitalWrite(RESET_PIN, 0);
}

// Send command to module (optimized)
void Sim800LX::sendCommand(String command, bool cr = true)
{
	#ifdef DEBUG_MODE_SET
		Serial.print(F("sendCommand : "));
		Serial.println(command);
	#endif

	this->print(command);
	carriageReturn(cr);
}

// Send command to module (optimized)
void Sim800LX::sendCommand(char command, bool cr = true)
{
	String Command(command);
	sendCommand(Command, cr);
}

// Send command to module (optimized)
void Sim800LX::sendCommand(char * command, bool cr = true)
{
	String Command(command);
	sendCommand(Command, cr);
}

// Send command to module (optimized)
void Sim800LX::sendCommand(const __FlashStringHelper * command, bool cr = true)
{
	String Command(command);
	sendCommand(Command, cr);
}

// Send carriage return on demande
void Sim800LX::carriageReturn(bool cr)
{
	if (cr)
		this->print(F("\r\n"));
}

// Send AT command to module (optimized)
void Sim800LX::sendAtCommand(const __FlashStringHelper * command, bool cr = true)
{
	this->print(F("AT"));
	sendCommand(command, cr);
}

// Send AT+ command to module (optimized)
void Sim800LX::sendAtPlusCommand(const __FlashStringHelper * command, bool cr = true)
{
	this->print(F("AT+"));
	sendCommand(command, cr);
}

// Private method for reading serial data incomming from Sim800L after a command
String Sim800LX::_readSerial(void)
{
	#ifdef DEBUG_MODE_SET
		Serial.println(F("_readSerial process..."));
	#endif

	int timeout = 0;
	while (!this->available() && timeout++ < MAX_TIME_OUT)
	{
		delay(10);
	}
	if (this->available())
	{
		_buffer = this->readString();

		#ifdef DEBUG_MODE_SET
			Serial.print(F("result = "));
			Serial.println(_buffer);
		#endif

		return _buffer;
	}
	else
	{
		#ifdef DEBUG_MODE_SET
			Serial.println("reading timeout !");
		#endif

		_buffer = F("");
		return _buffer;
	}
}

// Wait OK response
bool Sim800LX::waitOK(void)
{
	return waitResponse(F("OK"));
}

// Wait xxx response
bool Sim800LX::waitResponse(const __FlashStringHelper * response)
{
	#ifdef DEBUG_MODE_SET
		Serial.print(F("Waiting for response = "));
		Serial.println(response);
	#endif

	int counter = 0;
	while (_readSerial().indexOf(response) == -1 && counter++ < RESPONSE_COUNT_OUT) 
	{
		delay(10);
		if (_buffer.length() > 0)
			counter = 0;

		#ifdef DEBUG_MODE_SET
			Serial.print(F("loop wait response : "));
			Serial.println(String(counter, DEC));
		#endif
	}

	if (_buffer.indexOf(response) != -1)
	{
		#ifdef DEBUG_MODE_SET
			Serial.println("Response is OK");
		#endif

		return true;
	}
	else
		{
			#ifdef DEBUG_MODE_SET
				Serial.println("Response time out");
			#endif

			return false;
		}
}

// Waiting for good quality signal received
uint8_t Sim800LX::waitSignal(void)
{
	uint8_t counter;
	uint8_t sig;
	while ((sig = this->signalQuality()) == 0)
	{
		#ifdef DEBUG_MODE_SET
			Serial.println(F("Wait for signal ..."));
		#endif

		delay(1000);
		if (counter++ > 30)
		{
			this->reset();
			counter = 0;
		}
	}
	return sig;
}

// Public initialize method
void Sim800LX::reset(void)
{
	digitalWrite(RESET_PIN, 1);
	delay(1000);
	digitalWrite(RESET_PIN, 0);
	delay(1000);

	// wait for ready answer ...
	// waitResponse(F("+CPIN"));
	waitResponse(F("+CIEV"));
}

// Put module into sleep mode
void Sim800LX::sleepMode(void)
{
	sendAtPlusCommand(F("CSCLK=1"));
	waitOK();
}

// Put module into power down mode
void Sim800LX::powerDownMode(void)
{
	sendAtPlusCommand(F("CPOWD=1"));
	waitOK();
}

// Put device in phone functionality mode
bool Sim800LX::setPhoneFunctionality(uint8_t mode)
{
	sendAtPlusCommand(F("CFUN="), false);
	sendCommand(String(mode, DEC));
	waitOK();
}

// Check signal quality
uint8_t Sim800LX::signalQuality(void)
{
	sendAtPlusCommand(F("CSQ"));

	String signal = _readSerial();
	return signal.substring(_buffer.indexOf(F("+CSQ: ")) + 6, _buffer.indexOf(F("+CSQ: ")) + 8).toInt();
}

// Send a Sms method
bool Sim800LX::sendSms(char * number, char * text)
{
	String Number(number);
	String Text(text);

	this->sendSms(Number, Text);
}

// Send a Sms method
bool Sim800LX::sendSms(String number, String text)
{
	sendAtPlusCommand(F("CMGF=1"));
	_readSerial();
	sendAtPlusCommand(F("CMGS=\""), false);
	sendCommand(number, false);
	sendCommand(F("\""));

	_readSerial();
	sendCommand(text);

	delay(100);
	sendCommand((char)26, false);

	return waitResponse(F("CMGS"));
}

// Private method for jump in buffer posititon
void Sim800LX::nextBuffer(void)
{
	_buffer = _buffer.substring(_buffer.indexOf(F("\",\"")) + 2);
}

// Get an indexed Sms
Sim800LX::smsReader * Sim800LX::readSms(uint8_t index)
{
	// Put module in SMS text mode
	sendAtPlusCommand(F("CMGF=1"));

	// If no error ...
	if ((_readSerial().indexOf(F("ER"))) == -1)
	{
		// Read the indexed SMS
		sendAtPlusCommand(F("CMGR="), false);
		sendCommand(String(index, DEC));

		// Read module response
		_readSerial();

		// If there were an sms
		if (_buffer.indexOf(F("CMGR:")) != -1)
		{
			nextBuffer();
			String whoSend = _buffer.substring(1, _buffer.indexOf(F("\",\"")));
			nextBuffer();
			nextBuffer();
			nextBuffer();
			String whenSend = _buffer.substring(0, _buffer.indexOf(F("\"")));
			nextBuffer();
			if (_buffer.length() > 10) //avoid empty sms
			{
				_buffer = _buffer.substring(_buffer.indexOf(F("\"")) + 3);
				smsReader * result = new smsReader();
				result->WhoSend = whoSend;
				result->WhenSend = whenSend;
				result->Message = _buffer;

				// Return the sms
				#ifdef DEBUG_MODE_SET
					Serial.println("who = " + whoSend);
					Serial.println("when = " + whenSend);
					Serial.print(F("sms reading = "));
					Serial.println(_buffer);
				#endif

				return result;
			}
			else
				return nullptr;
		}
		else
			return nullptr;
	}
	else
		return nullptr;
}

// Delete Indexed Sms method
bool Sim800LX::delSms(uint8_t index)
{
	sendAtPlusCommand(F("CMGD="), false);
	sendCommand(String(index, DEC), false);
	sendCommand(F(",0"));
	return waitOK();
}

// Delete all Sms method
bool Sim800LX::delAllSms(void)
{
	sendAtPlusCommand(F("CMGDA=\"DEL ALL\""));
	return waitOK();
}

// Get Rtc internal Timer in decimal and string format
String Sim800LX::RTCtime(dateTime * result = nullptr)
{
	sendAtPlusCommand(F("CCLK?"));
	_readSerial();

	if ((_buffer.indexOf(F("ERR"))) == -1)
	{
		_buffer = _buffer.substring(_buffer.indexOf(F("\"")) + 1, _buffer.lastIndexOf(F("\"")) - 1);

		uint8_t year = _buffer.substring(0, 2).toInt();
		uint8_t month = _buffer.substring(3, 5).toInt();
		uint8_t day = _buffer.substring(6, 8).toInt();
		uint8_t hour = _buffer.substring(9, 11).toInt();
		uint8_t minute = _buffer.substring(12, 14).toInt();
		uint8_t second = _buffer.substring(15, 17).toInt();

		if (result != nullptr)
		{
			result->year = year;
			result->month = month;
			result->day = day;
			result->hour = hour;
			result->minute = minute;
			result->second = second;
		}

		return String(day, DEC) + "/" + String(month, DEC) + "/" + String(year, DEC) + "," +
			String(hour, DEC) + ":" + String(minute, DEC) + ":" + String(second, DEC);
	}
}

// Setup Sim800L to automatic rtc setup from cellular network
bool Sim800LX::setAutoCellRTC(void)
{
	return AutoCellRTC(F("+CLTS: 0"), F("CLTS=1"), F("+CLTS: 1"));
}

// Setup Sim800L to automatic rtc setup from cellular network
bool Sim800LX::resetAutoCellRTC(void)
{
	return AutoCellRTC(F("+CLTS: 1"), F("CLTS=0"), F("+CLTS: 0"));
}

// Set or Reset Sim800L to automatic rtc setup from cellular network
bool Sim800LX::AutoCellRTC(const __FlashStringHelper * command1, const __FlashStringHelper * command2, const __FlashStringHelper * command3)
{
	sendAtPlusCommand(F("CLTS?"));

	_readSerial();
	if (_buffer.indexOf(command1) != -1)
	{
		sendAtPlusCommand(command2);
		_readSerial();

		sendAtPlusCommand(F("CLTS?"));
		_readSerial();

		if (_buffer.indexOf(command3) != -1)
		{
			saveAllSettings();
			reset();
			return true;
		}
		else
			return false;
	}
	else
		return true;
}

// Save All settings in memory
bool Sim800LX::saveAllSettings(void)
{
	sendAtCommand(F("&W"));
	waitOK();
}

// Change state of module led flash
void Sim800LX::setOnLedFlash(void)
{
	sendAtPlusCommand(F("CNETLIGHT=1"));
	waitOK();
}

// Change state of module led flash
void Sim800LX::setOffLedFlash(void)
{
	sendAtPlusCommand(F("CNETLIGHT=0"));
	waitOK();
}

uint8_t Sim800LX::getReceivePin(void)
{
	return _ReceivePin;
}