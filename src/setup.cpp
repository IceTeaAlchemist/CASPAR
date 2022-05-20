#include "ADC.h"
#include "caspar.h"
#include "qiagen.h"
#include <wiringPiI2C.h>
#include <wiringPi.h>
#include <iostream>
#include <fstream>
#include <vector>

qiagen sens1("/dev/ttyUSB0");
qiagen sens2("/dev/ttyUSB1");

adc D2(DEVICE_ID, 0x8000, 0x7FFF);
adc TEMP(TEMP_ID, 0x8000, 0x7FFF);
vector<reading> data;
vector<double> tempkey;
vector<double> fluorkey;
vector<double> x;
vector<double> y;
vector<double> xderivs;
vector<double> derivs;
deque<double> yaverage(SMOOTHING,0);
deque<double> derivaverage(SMOOTHING,0);
fstream coeff_out;
fstream pcr_out;
int cycles = 0;

FILE *output;
int datapoints = 0;
double run_start = 0;

bool RTdone = false;
bool recordflag = false;

string ts = timestamp();

string coeffstorage = "./data/coeff" + ts + ".txt";
string pcrstorage = "./data/pcr" + ts + ".txt";
string rawstorage = "./data/binaryoutput" + ts + ".bin";

/* Sets up the Pi's GPIO pins and initiates wiringPi's library for GPIO communications.
 */
void setupPi(void)
{
    wiringPiSetup();
	pinMode(2,INPUT);
	pinMode(3,OUTPUT);
    pinMode(HEATER_PIN, OUTPUT);
    pinMode(FAN_PIN, OUTPUT);
    wiringPiISR(2,INT_EDGE_RISING,sampletriggered);
}

/* Sets up the Qiagen using default settings. Note that the qiagen itself is initialized externally. In the future this function will factor UI/recipe details.
 */
void setupQiagen(void)
{
    sens1.writeqiagen(0, {00,04});
    sens1.writeqiagen(1, {00,00});
    sens2.writeqiagen(0, {00,200});
    sens2.writeqiagen(1, {00,00});   
    sens1.LED_power(1,50);
    sens2.LED_power(2,95);
    sens2.LED_power(1,40);
    sens2.LED_off(2);
    sens2.LED_off(1);
    sens1.LED_off(1);
    sens1.setMode(0);
	sens1.setMethod(1);
    sens2.setMode(0);
    sens2.setMethod(1);
}

/* Sets the ADC to run using the interrupt pin as well as the samples per second and mode. D2 is the Qiagen detector, TEMP is the temperature detector.
 */
void setupADC(void)
{
    D2.SetGain(1);
	D2.SetMode(0);
	D2.SetSPS(5);
	D2.SetCompPol(1);
	D2.SetCompQueue(0);
    D2.SetMultiplex(0,1);
    TEMP.SetGain(1);
	TEMP.SetMode(0);
	TEMP.SetSPS(5);
	TEMP.SetCompPol(1);
	TEMP.SetCompQueue(0);
    TEMP.SetMultiplex(0,-1);
}


/* Runs the calibration algorithm for the L-DNA. CALIBRATION_MIN can be set in caspar.h.
 */
void calibrategain()
{
    int basegain = 80;
    double readinval;
    cout << "Calibrating gain." << endl;
    do
    {
        cout << "Testing at gain of: " << basegain << "." << endl;
        sens1.LED_off(2);
        sens1.LED_power(2,basegain);
        sens1.LED_on(2);
        delay(200);
        readinval = (D2.getreading()/32768.0)*4096.0;
        cout << "Reading: " << readinval << endl;
        basegain += 10;
        if(basegain > 224)
        {
            cout << "Couldn't find a suitable gain. Sample not present or illprepared." << endl;
            break;
        }
    } while (readinval < CALIBRATION_MIN);
}

/* Opens files for fit information, raw binary data, and PCR. In the future these strings will need to be set by the UI, as well as headings influenced by recipe.
 */
void openFiles()
{
    coeff_out.open(coeffstorage,std::ios_base::out);
    pcr_out.open(pcrstorage,std::ios_base::out);
    pcr_out << "Time    " << "FAM   " << "Cy5   " << endl;
    if(!coeff_out.is_open())
    {
        cout << "Failed to open " << coeffstorage << endl;
    }
    if(!pcr_out.is_open())
    {
        cout << "Failed to open " << pcrstorage << endl;
    }

    output = fopen(rawstorage.c_str(),"wb");
	if(output == NULL)
	{
		cout << "File failed to open." << endl;
	} 
}

void closeFiles()
{
    coeff_out.close();
    pcr_out.close();
    fclose(output);
}