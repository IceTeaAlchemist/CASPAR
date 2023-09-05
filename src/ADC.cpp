#include "ADC.h"
// #include "caspar.h"  // For devicesIni .
#include <wiringPiI2C.h>
#include <wiringPi.h>
#include <byteswap.h>
#include <iostream>

#define CONVERSION 0x00
#define CONFIG 0x01
#define LOWTHRESH 0x02
#define HITHRESH 0x03
// inline unsigned long CONVERSION = stoul( devicesIni.get_value("ADC", "CONVERSION") );
// inline unsigned long CONFIG = stoul( devicesIni.get_value("ADC", "CONFIG") );
// inline unsigned long LOWTHRESH = stoul( devicesIni.get_value("ADC", "LOWTHRESH") );
// inline unsigned long HITHRESH = stoul( devicesIni.get_value("ADC", "HITHRESH") );

using namespace std;

/*
 * Default constructor for the ADC. It must be declared with an address.
 * int addr: I2C Address where the ADS1115 is communicating.
 * 
 */

adc::adc(int addr)
{
	address = addr;
	highthresh = 0x7FFF;
	lowthresh = 0x8000;
	config = {0,0,0,0,0,1,0,1,1,0,0,0,0,0,1,1};
	// config is the integer repr of the 15 bit config reg.
	// congfig[0] is bit 15, 1, 14, etc.  See Fig. 36 docs.
	gainvoltage = 2.048;
	maxbitcounts = 32768; //2**15;  // The number of effective bits for the ADS1115, 15 bits because signed.
	// Intended to be used in getvoltage() or similar.
	adcsetup();
}

/*
 * Constructor that sets the high threshold and low threshold.
 * int addr: I2C address where the ADS 1115 is communicating.
 * int ht: High threshold of the comparator.
 * int lt: Low threshold of the comparator.
 */

adc::adc(int addr, int ht, int lt)
{
	address = addr;
	highthresh = ht;
	lowthresh = lt;
	config = {0,0,0,0,0,1,0,1,1,0,0,0,0,0,1,1};
	gainvoltage = 2.048;
	maxbitcounts = 32768; // 2**15 
	adcsetup();
}

// Default constructor, weg, 20230202
adc::adc()
{

}

void adc::adcsetup()
{
	pihandle = wiringPiI2CSetup(address);
	if (pihandle == -1)
	{
		cout << "adc::adcsetup: Warning: Failed to initiate I2C communication." << endl;
	}
	wiringPiI2CWriteReg16(pihandle, HITHRESH, __bswap_16(highthresh));
	wiringPiI2CWriteReg16(pihandle, LOWTHRESH, __bswap_16(lowthresh));
	writeconfig();
	// checking
	cout << "adc::adcsetup: At the end of this method.  Address " << hex << address << endl; 
}

void adc::writeconfig()
{
	int reg = 0;
	int place = 32768;
	for(int i = 0; i < 16; i++)
	{
		reg += config[i] * place;
		place = place/2;
	}
	wiringPiI2CWriteReg16(pihandle, CONFIG, __bswap_16(reg));
}

void adc::StartConversion()
{
	config[0] = 1;
	writeconfig();
}

void adc::SetMultiplex(int up)
{
	SetMultiplex(up, -1);
}

void adc::SetMultiplex(int up, int down)
{
	if(up == 0 && down == 1)  // A0 - A1
	{
		config[1] = 0;
		config[2] = 0;
		config[3] = 0;
		writeconfig();
	} 
	else if(up == 0 && down == 3)  // A0 - A3
	{
		config[1] = 0;
		config[2] = 0;
		config[3] = 1;
		writeconfig();
	}
	else if(up == 1 && down == 3)
	{
		config[1] = 0;
		config[2] = 1;
		config[3] = 0;
		writeconfig();
	}
	else if(up == 2 && down == 3)
	{
		config[1] = 0;
		config[2] = 1;
		config[3] = 1;
		writeconfig();
	}
	else if(up == 0 && down == -1)  // A0 - GND
	{
		config[1] = 1;
		config[2] = 0;
		config[3] = 0;
		writeconfig();
	}
	else if(up == 1 && down == -1)  // A1 - GND
	{
		config[1] = 1;
		config[2] = 0;
		config[3] = 1;
		writeconfig();
	}
	else if(up == 2 && down == -1)
	{
		config[1] = 1;
		config[2] = 1;
		config[3] = 0;
		writeconfig();
	}
	else if(up == 3 && down == -1)
	{
		config[1] = 1;
		config[2] = 1;
		config[3] = 1;
		writeconfig();
	}
	else
	{
		cout << "adc::SetMultiplex: Invalid multiplex configuration. Valid combos are 0 and 1, any value and 3 or ground." << endl;
	}
}

void adc::SetGain(int gain)
{
	switch (gain)
	{
	case 0:
		config[4] = 0;
		config[5] = 0;
		config[6] = 0;
		gainvoltage = 6.144;
		writeconfig();
		break;
	case 1:
		config[4] = 0;
		config[5] = 0;
		config[6] = 1;
		gainvoltage = 4.096;
		writeconfig();
		break;
	case 2:
		config[4] = 0;
		config[5] = 1;
		config[6] = 0;
		gainvoltage = 2.048;
		writeconfig();
		break;
	case 3:
		config[4] = 0;
		config[5] = 1;
		config[6] = 1;
		gainvoltage = 1.024;
		writeconfig();
		break;
	case 4:
		config[4] = 1;
		config[5] = 0;
		config[6] = 0;
		gainvoltage = 0.512;
		writeconfig();
		break;
	case 5:
		config[4] = 1;
		config[5] = 0;
		config[6] = 1;
		gainvoltage = 0.256;
		writeconfig();
		break;
	case 6:
		config[4] = 1;
		config[5] = 1;
		config[6] = 0;
		gainvoltage = 0.256;
		writeconfig();
		break;
	case 7:
		config[4] = 1;
		config[5] = 1;
		config[6] = 1;
		gainvoltage = 0.256;
		writeconfig();
		break;
	default:
		cout << "adc::SetGain: Invalid gain selected. Please use 0 - 7. Write cancelled." << endl;
	}
}

void adc::SetMode(int mode)
{
	if(mode == 0)
	{
		config[7] = 0;
		writeconfig();
	}
	else if(mode == 1)
	{
		config[7] = 1;
		writeconfig();
	}
	else
	{
		cout << "adc::SetMode: Invalid mode selected. Canceled without writing." << endl;
	}
}

void adc::SetSPS(int sps)
{
	switch (sps)
	{
	case 0:
		config[8] = 0;
		config[9] = 0;
		config[10] = 0;
		writeconfig();
		break;
	case 1:
		config[8] = 0;
		config[9] = 0;
		config[10] = 1;
		writeconfig();
		break;
	case 2:
		config[8] = 0;
		config[9] = 1;
		config[10] = 0;
		writeconfig();
		break;
	case 3:
		config[8] = 0;
		config[9] = 1;
		config[10] = 1;
		writeconfig();
		break;
	case 4:
		config[8] = 1;
		config[9] = 0;
		config[10] = 0;
		writeconfig();
		break;
	case 5:
		config[8] = 1;
		config[9] = 0;
		config[10] = 1;
		writeconfig();
		break;
	case 6:
		config[8] = 1;
		config[9] = 1;
		config[10] = 0;
		writeconfig();
		break;
	case 7:
		config[8] = 1;
		config[9] = 1;
		config[10] = 1;
		writeconfig();
		break;
	default:
		cout << "adc::SetSPS: Invalid samples per second selected. Please use 0 - 7. Write cancelled." << endl;
	}
}

void adc::SetCompMode(int mode)
{
	if (mode == 0)
	{
		config[11] = 0;
		writeconfig();
	}
	else if (mode == 1)
	{
		config[11] = 1;
		writeconfig();
	}
	else
	{
		cout << "adc::SetCompMode: Invalid comparator mode selected. Canceled without writing." << endl;
	}
}

void adc::SetCompPol(int pol)
{
	if (pol == 0)
	{
		config[12] = 0;
		writeconfig();
	}
	else if (pol == 1)
	{
		config[12] = 1;
		writeconfig();
	}
	else
	{
		cout << "adc::SetCompPol: Invalid comparator polarity selected. Canceled without writing." << endl;
	}
}

void adc::SetLatch(int latch)
{
	if (latch == 0)
	{
		config[13] = 0;
		writeconfig();
	}
	else if (latch == 1)
	{
		config[13] = 1;
		writeconfig();
	}
	else
	{
		cout << "adc::SetLatch: Invalid latch selected. Canceled without writing." << endl;
	}
}

void adc::SetCompQueue(int que)
{
	switch (que)
	{
	case 0:
		config[14] = 0;
		config[15] = 0;
		writeconfig();
		break;
	case 1:
		config[14] = 0;
		config[15] = 1;
		writeconfig();
		break;
	case 2:
		config[14] = 1;
		config[15] = 0;
		writeconfig();
		break;
	case 3:
		config[14] = 1;
		config[15] = 1;
		writeconfig();
		break;
	default:
		cout << "adc::SetCompQueue: Invalid comparator queue. Write canceled." << endl;
	}
}

int adc::getreading()
{
	int reading = __bswap_16(wiringPiI2CReadReg16(pihandle, CONVERSION));
	return reading;
}

double adc::getvoltage()
{
	int reading = getreading();
	int adjustedreading;
	if(reading >= maxbitcounts) // maxbitcounts is 32768
	{
		adjustedreading = reading - maxbitcounts;
		return adjustedreading/(maxbitcounts-1.0)*(-1.0)*gainvoltage;  // negative voltages
	}
	else
	{
		return reading/(maxbitcounts-1.0)*gainvoltage;
	}
}

int adc::gethighthresh()
{
	return highthresh;
}

int adc::getlowthresh()
{
	return lowthresh;
}

int adc::getconfig()
{
	int readconfig = __bswap_16(wiringPiI2CReadReg16(pihandle, CONFIG));
	int reg = 0;
	int place = 32768;
	for(int i = 0; i < 16; i++)
	{
		reg += place * config[i];
		place = place/2;
	}
	if(reg != readconfig)
	{
		cout << "adc::getconfig: Register and class mismatch." << endl;
		return -1;
	}
	else
	{
		return readconfig;
	}
}

// Conversion for AD8495 board.  (volts-1.25)/0.005 for volts in Volts and output deg C.
double adc::convertToDegC(double volts)
{
	return (volts-1.25)*200.;
} 

adc::~adc()
{
	int resetd = wiringPiI2CSetup(0x00); // Writes to the default "bus" to ALL ADS-ADC's all listen on 0x00.
	wiringPiI2CWrite(resetd, 0b00000110);
	cout << "adc::~adc: ADC reset and turned off.  Address " << hex << address << endl;
}
