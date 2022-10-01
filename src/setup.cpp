#include "ADC.h"
#include "caspar.h"
#include "qiagen.h"
#include "configINI.h"
#include <wiringPiI2C.h>
#include <wiringPi.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>

/*
 * These declarations are all for the externally linked variables found in caspar.h 
 * and used throughout the code. Please try to keep those declarations IN setup.cpp 
 * for consistency.
 */

// Read the config file with sections [Qiagen] and [ADC], etc.
config hardCfg("configs/devices.ini");
config recipeDef("configs/recipes/default.ini");

// Set up the qiagens on relevant USB ports.
qiagen sens1( hardCfg.get_value("Qiagen", "Q0SerialPort") );  // "/dev/ttyUSB0"
qiagen sens2( hardCfg.get_value("Qiagen", "Q1SerialPort") ); // "/dev/ttyUSB1"

// Set up the ADCs to use interrupts as well as declare them.
adc D2(DEVICE_ID, 0x8000, 0x7FFF);
adc TEMP(TEMP_ID, 0x8000, 0x7FFF);

bool pwm_enable = (hardCfg.get_value("PWM", "Enable") == "true"); // Turn off PWM avoid sudo node caspar.js .
const double pwm_high_ratio = stof( hardCfg.get_value("PWM", "HighRatio") ); //0.90;
const int pwm_high = 1024.0*pwm_high_ratio;
const double pwm_low_ratio = stof( hardCfg.get_value("PWM", "LowRatio") );  //0.001;
const int pwm_low = 1024.0*pwm_low_ratio;

// Declare vectors for tracking the thermal/fluor correspondence.
vector<double> tempkey;
vector<double> fluorkey;

// Declare all the vectors for storing data by the interrupt thread
vector<reading> data;
vector<double> x;
vector<double> y;
vector<double> xderivs;
vector<double> derivs;

// Declare deques for the moving average filter. SMOOTHING is defined in caspar.h.
deque<double> yaverage(SMOOTHING,0);
deque<double> derivaverage(SMOOTHING,0);

// Declare fstream objects globally.
fstream coeff_out;
fstream pcr_out;
fstream runtime_out;

// Declare cycle number variable.
int cycles = 0;
int cyclecutoff = 40;

// Declare the file operator and a few check variables for the thread.
FILE *output;
int datapoints = 0;
double run_start = 0;

// Set up flags for threads.
bool RTdone = false;
bool recordflag = false;
int runerror;
int temperrors;
bool RTflag = false;

// Get initialization time, format YYYYMMDD_HHMMSS .
string ts = timestamp();

// Declare initial file names for saving the data. 
// For Project and Experiment (night be none) names, use the
// directory ./data/ProjName/Expt/TimeStamp/<filename.csv> .
string dataDir = "./data/";
string coeffstorage = "coeff_" + ts + ".csv";
string pcrstorage = "pcr_" + ts + ".csv";
string rawstorage = "binaryoutput_" + ts + ".bin";
string notesstorage = "notes_" + ts + ".txt";
string runlogDir = "./";
string runlog = "runtimelog.txt";

/* Sets up the Pi's GPIO pins and initiates wiringPi's library for GPIO communications.
 */
void setupPi(void)
{
    string progName = "setupPi";
    // Print out the hardware config/ini file.
    // cout << hardCfg.print_file() << endl;
    wiringPiSetup();
    // Set pins for Alert, heating and cooling.
	pinMode(3,INPUT);
    pinMode(HEATER_PIN, OUTPUT);
    pinMode(FAN_PIN, OUTPUT);
    pinMode(BOX_FAN, OUTPUT);
    // cout << progName << ": Info, pwm_enable is " << pwm_enable << endl;
    if (pwm_enable){
        pinMode(PWM_PIN, PWM_OUTPUT);
        pwmSetMode(PWM_MODE_MS);
        pwmSetClock(19.53); // See wiringPi.h, takes an int.   
        // Clock should be 19.2e6 divided by the desired Hz (1000) and then the Range (1024), 
        // usually like 19.2 Mhz / 1024 / 1000 = 19 (an int).
        pwmSetRange(1024);
        // Set up the interrupt for sample reading. Note that these CANNOT be turned off once started,
        // so wrapping them in a boolean with a flag is a good idea.
        // wiringPiISR(3,INT_EDGE_RISING,sampletriggered);
        // Open the runtime log file for appending and print the initialization time to it.
        delay(1000);
        cout << progName << ":  pwm_enable is " << pwm_enable << " pwm_low is " << pwm_low << endl;
        pwmWrite(PWM_PIN, pwm_low);
    }
    digitalWrite(BOX_FAN, HIGH);
    digitalWrite(HEATER_PIN, LOW);
    // Open the runtime log file for appending and print the initialization time to it.
    runtime_out.open(runlogDir+runlog, std::ios_base::app | std::ios_base::in);
    runtime_out << progName << ":  PCR Session initialized at " << timestamp() << endl;
}

/* Sets up the Qiagen using default settings. Note that the qiagen itself is initialized externally. In 
   the future this function will factor UI/recipe details.
 */
void setupQiagen(void)
{
    // Set Qiagen 1 sample protocol--Maximum samples with minimum delay between them. This is so we can use 
    // it for cycling.
    sens1.writeqiagen(0, {255,255});  // Cycles to read, 65535.
    sens1.writeqiagen(1, {00,00});   // Cycle time 0s.
    sens1.writeqiagen(32,{01,244});   // ADC Sampling 500 Hz.
    sens1.writeqiagen(5,{01,00});   // Average 1 sample, no average.
    // Set Qiagen 2 sample protocol-- 200 samples with minimum delay between them.
    sens2.writeqiagen(0, {00,200});  // Cycles to read, 200.
    sens2.writeqiagen(1, {00,00});   // Cycle time 0s.
    // Note: The number of samples is basically irrelevant, just make sure it's more than 3 (Hz of 
    // sample rate) * delay after the LED turns on in readPCR().

    // Set the LED power for the PCR wavelengths. Cycling sensing (sens1 LED 2) is set by the calibrator.
    sens1.LED_current(1,50);
    sens2.LED_current(2,120);
    sens2.LED_current(1,40);

    // Make sure all the LEDs besides the cycling one are off.
    sens2.LED_off(2);
    sens2.LED_off(1);
    sens1.LED_off(1);

    // Set mode (beginning sampling routine after the LEDs are turned on) and method (which detector will be read).
    sens1.setMode(0);
	sens1.setMethod(3);
    sens2.setMode(0);
    sens2.setMethod(1);
}

/* Sets the ADC to run using the interrupt pin as well as the samples per second and mode. D2 is the Qiagen detector, TEMP is the temperature detector.
 */
void setupADC(void)
{
    // Setup the qiagen tapped ADC. See the class for documentation.
    D2.SetGain(1);
	D2.SetMode(0);
	D2.SetSPS(5);
	D2.SetCompPol(1);
	D2.SetCompQueue(0);
    D2.SetMultiplex(1,3);
    // Setup the ADC for the temperature. See the ADC class for documentation.
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
    string progName = "calibrategain";
    ostringstream bstream;
    sens1.LED_off(1);
    sens1.LED_off(2);
    sens1.setMethod(3);
    sens1.writeqiagen(0, {255,255});
    // Start at our minimum gain for this qiagen.
    int basegain = 80;
    double readinval;
    // Log to console and file that we're entering calibration.
    bstream << progName << ": Calibrating gain." << endl; 
    cout << bstream.str();
    runtime_out << bstream.str();
    do
    {
        if(basegain > 224) // If our gain outstrips the limits of the qiagen, exit the loop 
        // and log that we're pushing the limits of our optics.
        {
            bstream << progName << ": Couldn't find a suitable gain. Sample not present or illprepared." << endl;
            cout << bstream.str();
            runtime_out << bstream.str();
            break;
        }
        // Log to console what gain is being tested.
        cout << progName << ": Testing at gain of: " << basegain << "." << endl;
        // Turn the LED, adjust the current, and turn it back on.
        sens1.LED_off(2);
        sens1.LED_current(2,basegain);
        sens1.LED_on(2);
        sens1.startMethod();
        delay(400);
        // Measure what gain we receive.
        readinval = sens1.measure();
        // Log what reading we receive to console.
        cout << progName << ": Reading: " << readinval << endl;
        // Increase the gain.
        basegain += 10;
        sens1.stopMethod();
    } while (readinval < CALIBRATION_MIN && runflag == true); // Continue while we haven't reached our 
    // requested minimum calibration fluoresence. This is defined in the header.
}



/* Opens files for fit information, raw binary data, and PCR. These will need to be able to rewrite the headings influenced by recipe.
 */
void openFiles()
{
    string progName = "openFiles";
    ostringstream bstream;
    //Open coefficient and PCR storage files from provided strings. These names are generated from the timestamp the instrument was started or user input + timestamp.
    coeff_out.open(dataDir + coeffstorage,std::ios_base::out);
    pcr_out.open(dataDir + pcrstorage,std::ios_base::out);
    //Dump column headings into the PCR file. Commas are so Excel will see this as a CSV.
    pcr_out << "Time," << "FAM," << "HEX," << "Cy5," << endl;
    // Check that both fstreams are open, log any failures.
    if(!coeff_out.is_open())
    {
        bstream << progName << ": **Failed to open " << dataDir + coeffstorage << endl;
        cout << bstream.str();
        runtime_out << bstream.str();
    }
    if(!pcr_out.is_open())
    {
        bstream << progName << ": **Failed to open " << dataDir + pcrstorage << endl;
        cout << bstream.str();
        runtime_out << bstream.str();
    }

    // Try to open the binary output file, log if it fails.
    output = fopen((dataDir+rawstorage).c_str(),"wb");
	if(output == NULL)
	{
        bstream << "openFiles: **Failed to open " << dataDir+rawstorage << endl;
		cout << bstream.str();
        runtime_out << bstream.str();
	} 
}

/* Closes all cycling output files.
*/
void closeFiles()
{
    bool oneOpen = ( coeff_out.is_open() ) || ( pcr_out.is_open() );
    if ( coeff_out.is_open() ) coeff_out.close();
    if ( pcr_out.is_open() ) pcr_out.close();
    if ( oneOpen ) fclose(output);  // Kludge, likely but not guaranteed that if coeff_out and pcr_out are open so is output.
}
