#include <wiringPiI2C.h>
#include <wiringPi.h>
#include <byteswap.h>
#include <iostream>

#define CONFIG 0x01
#define TEMP_ID 0x49

using namespace std;

int main()
{
	int pihandle = wiringPiI2CSetup(TEMP_ID);
	int configreadout = __bswap_16(wiringPiI2CReadReg16(pihandle, CONFIG));
	cout << "Configuration register: " << configreadout << endl;
	return 0;
}
