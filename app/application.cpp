#include <user_config.h>
#include <SmingCore/SmingCore.h>
#include <Libraries/OneWire/OneWire.h> //DS18B20
#include "../include/configuration.h" // application configuration
#include "special_chars.h"
#include "webserver.h"

OneWire ds1(WORK_PIN1); // Sensor 1
OneWire ds2(WORK_PIN2); // Sensor 2

Timer procTimer;

bool state = true;
// Sensors string values
String StrT1, StrT2, StrTime;

void process();
void connectOk();
void connectFail();

void init()
{
	Serial.begin(SERIAL_BAUD_RATE); // 115200 by default
	Serial.systemDebugOutput(true); // Debug output to serial

	ActiveConfig = loadConfig();

	// Select control line
	//pinMode(CONTROL_PIN, OUTPUT);

	ds1.begin(); // It's required for one-wire initialization!
	ds2.begin();

	WifiStation.config(ActiveConfig.NetworkSSID, ActiveConfig.NetworkPassword);
	WifiStation.enable(true);
	WifiAccessPoint.enable(false);

	WifiStation.waitConnection(connectOk, 20, connectFail); // We recommend 20+ seconds for connection timeout at start

	procTimer.initializeMs(5000, process).start();
	process();
}

void process()
{
	byte i;
	byte j;
	byte present1 = 0;
	byte present2 = 0;
	byte type_s1;
	byte type_s2;
	byte data1[12];
	byte data2[12];
	byte addr1[8];
	byte addr2[8];
	float temp1;
	float temp2;
	
	ds1.reset_search();
	ds2.reset_search();
	
	if (!ds1.search(addr1))
	{
		Serial.println("No addresses found.");
		Serial.println();
		ds1.reset_search();
		delay(250);
		return;
	}
	
	if (!ds2.search(addr2))
	{
		Serial.println("No addresses found.");
		Serial.println();
		ds2.reset_search();
		delay(250);
		return;
	}

	ds1.reset();
	ds2.reset();
	ds1.select(addr1);
	ds2.select(addr2); 
	ds1.write(0x44, 1);     // start conversion, with parasite power on at the end
	ds2.write(0x44, 1);   // start conversion, with parasite power on at the end

	delay(1000);     // maybe 750ms is enough, maybe not
	// we might do a ds.depower() here, but the reset will take care of it.

	present1 = ds1.reset();
	present2 = ds2.reset();
	ds1.select(addr1);
	ds2.select(addr2);
	ds1.write(0xBE);         // Read Scratchpad
	ds2.write(0xBE); 	// Read Scratchpad

	for ( i = 0; i < 9; i++)
	{
		// we need 9 bytes
		data1[i] = ds1.read();
	}
	
	for ( j = 0; j < 9; j++)
	{
		// we need 9 bytes
		data2[j] = ds2.read();
	}
	
	int16_t raw1 = (data1[1] << 8) | data1[0];	
	int16_t raw2 = (data2[1] << 8) | data2[0];

	if (type_s1)
	{
		raw1 = raw1 << 3; // 9 bit resolution default
		if (data1[7] == 0x10)
		{
		  // "count remain" gives full 12 bit resolution
		  raw1 = (raw1 & 0xFFF0) + 12 - data1[6];
		}
	} else {
		byte cfg1 = (data1[4] & 0x60);
		// at lower res, the low bits are undefined, so let's zero them
		if (cfg1 == 0x00) raw1 = raw1 & ~7;  // 9 bit resolution, 93.75 ms
		else if (cfg1 == 0x20) raw1 = raw1 & ~3; // 10 bit res, 187.5 ms
		else if (cfg1 == 0x40) raw1 = raw1 & ~1; // 11 bit res, 375 ms
		//// default is 12 bit resolution, 750 ms conversion time
	}
	
	if (type_s2)
	{
		raw2 = raw2 << 3; // 9 bit resolution default
		if (data2[7] == 0x10)
		{
		  // "count remain" gives full 12 bit resolution
		  raw2 = (raw2 & 0xFFF0) + 12 - data2[6];
		}
	} else {
		byte cfg2 = (data2[4] & 0x60);
		// at lower res, the low bits are undefined, so let's zero them
		if (cfg2 == 0x00) raw2 = raw2 & ~7;  // 9 bit resolution, 93.75 ms
		else if (cfg2 == 0x20) raw2 = raw2 & ~3; // 10 bit res, 187.5 ms
		else if (cfg2 == 0x40) raw2 = raw2 & ~1; // 11 bit res, 375 ms
		//// default is 12 bit resolution, 750 ms conversion time
	}

	temp1 = (float)raw1 / 16.0;
	Serial.print("  Temperature1 = ");
	Serial.print(temp1);
	Serial.print(" Celsius, ");
		
	temp2 = (float)raw2 / 16.0;
	Serial.print("  Temperature2 = ");
	Serial.print(temp2);	
	Serial.print(" Celsius2, ");
	Serial.println();

	float t1 = temp1 + ActiveConfig.AddT1;
	float t2 = temp2 + ActiveConfig.AddT2;

	if (ActiveConfig.Trigger == eTT_Temperature1)
		state = t1 < ActiveConfig.RangeMin || t1 > ActiveConfig.RangeMax;
	else if (ActiveConfig.Trigger == eTT_Temperature2)
		state = t2 < ActiveConfig.RangeMin || t2 > ActiveConfig.RangeMax;

//	digitalWrite(CONTROL_PIN, state);
	StrT1 = String(t1, 0);
	StrT2 = String(t2, 0);

}

void connectOk()
{
	debugf("connected");
	WifiAccessPoint.enable(false);
	procTimer.restart();

	startWebClock();
	// At first run we will download web server content
	if (!fileExist("index.html") || !fileExist("config.html") || !fileExist("api.html") || !fileExist("bootstrap.css.gz") || !fileExist("jquery.js.gz"))
		downloadContentFiles();
	else
		startWebServer();
}

void connectFail()
{
	debugf("connection FAILED");
	WifiAccessPoint.config("MeteoConfig", "", AUTH_OPEN);
	WifiAccessPoint.enable(true);
	// Stop main screen output
	procTimer.stop();

	startWebServer();
	WifiStation.waitConnection(connectOk); // Wait connection
}

////// WEB Clock //////
Timer clockRefresher;
HttpClient clockWebClient;
uint32_t lastClockUpdate = 0;
DateTime clockValue;
const int clockUpdateIntervalMs = 10 * 60 * 1000; // Update web clock every 10 minutes

void onClockUpdating(HttpClient& client, bool successful)
{
	if (!successful)
	{
		debugf("CLOCK UPDATE FAILED %d (code: %d)", successful, client.getResponseCode());
		lastClockUpdate = 0;
		return;
	}

	// Extract date header from response
	clockValue = client.getServerDate();
	if (clockValue.isNull()) clockValue = client.getLastModifiedDate();
	if (!clockValue.isNull())
		clockValue.addMilliseconds(ActiveConfig.AddTZ * 1000 * 60 * 60);
}

void refreshClockTime()
{
	uint32_t nowClock = millis();
	if (nowClock < lastClockUpdate) lastClockUpdate = 0; // Prevent overflow, restart
	if ((lastClockUpdate == 0 || nowClock - lastClockUpdate > clockUpdateIntervalMs) && !clockWebClient.isProcessing())
	{
		clockWebClient.downloadString("google.com", onClockUpdating);
		lastClockUpdate = nowClock;
	}
	else if (!clockValue.isNull())
		clockValue.addMilliseconds(clockRefresher.getIntervalMs());

	if (!clockValue.isNull())
	{
		StrTime = clockValue.toShortDateString() + " " + clockValue.toShortTimeString(false);

		if ((millis() % 2000) > 1000)
			StrTime.setCharAt(13, ' ');
		else
			StrTime.setCharAt(13, ':');
	}
}

void startWebClock()
{
	lastClockUpdate = 0;
	clockRefresher.stop();
	clockRefresher.initializeMs(500, refreshClockTime).start();
}
