#ifndef INCLUDE_CONFIGURATION_H_
#define INCLUDE_CONFIGURATION_H_

#include <user_config.h>
#include <SmingCore/SmingCore.h>

// If you want, you can define WiFi settings globally in Eclipse Environment Variables
#ifndef WIFI_SSID
	#define WIFI_SSID "FRITZ!Box 7490" // Put you SSID and Password here
	#define WIFI_PWD "75151423810134506770"
#endif

// Pin for communication with DS18B20 sensor
#define WORK_PIN1 12 // GPIO12
#define WORK_PIN2 5 // GPIO5

// Pin for trigger control output
//#define CONTROL_PIN 3 // UART0 RX pin
//#define CONTROL_PIN 15

#define METEO_CONFIG_FILE ".meteo.conf" // leading point for security reasons :)

enum TriggerType
{
	eTT_None = 0,
	eTT_Temperature1,
	eTT_Temperature2
};

struct MeteoConfig
{
	MeteoConfig()
	{
		AddT1 = 0;
		AddT2 = 0;
		AddTZ = 0;
		Trigger = eTT_Temperature1;
		RangeMin = 18;
		RangeMax = 29;
///////////////////////////////////////////
// evtl. noch Trigger f�r die zweite Temperatur einfuegen
//////////////////////////////////////////
	}

	String NetworkSSID;
	String NetworkPassword;

	float AddT1; // Temperature adjustment
	float AddT2; // Humidity adjustment
	float AddTZ; // TimeZone - local time offset

	TriggerType Trigger; // Sensor trigger type
	float RangeMin;
	float RangeMax;
};

MeteoConfig loadConfig();
void saveConfig(MeteoConfig& cfg);
extern void startWebClock();

extern MeteoConfig ActiveConfig;

#endif /* INCLUDE_CONFIGURATION_H_ */
