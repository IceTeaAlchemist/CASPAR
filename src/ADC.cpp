#include "ADC.h"
#include <wiringPiI2C.h>
#include <wiringPi.h>
#include <byteswap.h>
#include <iostream>

#define CONVERSION 0x00
#define CONFIG 0x01
#define LOWTHRESH 0x02
#define HITHRESH 0x03

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
	gainvoltage = 2.048;
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
	adcsetup();
}

void adc::adcsetup()
{
	pihandle = wiringPiI2CSetup(address);
	if (pihandle == -1)
	{
		cout << "Warning: Failed to initiate I2C communication." << endl;
	}
	wiringPiI2CWriteReg16(pihandle, HITHRESH, __bswap_16(highthresh));
	wiringPiI2CWriteReg16(pihandle, LOWTHRESH, __bswap_16(lowthresh));
	writeconfig();
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
	wiringPiI2CWriteReg16(pihandle,CONFIG, __bswap_16(reg));
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

void adc::SetMultiplex(int up,int down)
{
	if(up == 0 && down == 1)
	{
		config[1] = 0;
		config[2] = 0;
		config[3] = 0;
		writeconfig();
	} 
	else if(up == 0 && down == 3)
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
	else if(up == 0 && down == -1)
	{
		config[1] = 1;
		config[2] = 0;
		config[3] = 0;
		writeconfig();
	}
	else if(up == 1 && down == -1)
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
		cout << "Invalid multiplex configuration. Valid combos are 0 and 1, any value and 3 or ground." << endl;
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
		cout << "Invalid gain selected. Please use 0 - 7. Write cancelled." << endl;
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
		cout << "Invalid mode selected. Canceled without writing." << endl;
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
		cout << "Invalid samples per second selected. Please use 0 - 7. Write cancelled." << endl;
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
		cout << "Invalid comparator mode selected. Canceled without writing." << endl;
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
		cout << "Invalid comparator polarity selected. Canceled without writing." << endl;
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
		cout << "Invalid latch selected. Canceled without writing." << endl;
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
		cout << "Invalid comparator queue. Write canceled." << endl;
	}
}

int adc::getreading()
{
	int reading = __bswap_16(wiringPiI2CReadReg16(pihandle,CONVERSION));
	return reading;
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
	int readconfig = __bswap_16(wiringPiI2CReadReg16(pihandle,CONFIG));
	int reg = 0;
	int place = 32768;
	for(int i = 0; i < 16; i++)
	{
		reg += place * config[i];
		place = place/2;
	}
	if(reg != readconfig)
	{
		cout << "Register and class mismatch." << endl;
		return -1;
	}
	else
	{
		return readconfig;
	}
}

adc::~adc()
{
	int resetd = wiringPiI2CSetup(0x00);
	wiringPiI2CWrite(resetd, 0b00000110);
	cout << "ADC reset and turned off." << endl;
}
