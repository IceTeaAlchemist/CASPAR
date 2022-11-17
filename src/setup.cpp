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
config devicesIni("configs/devices.ini");
config recipeDef("configs/recipes/default.ini");

// Set up the qiagens on relevant USB ports.
qiagen sens1( devicesIni.get_value("Qiagen", "Q1SerialPort") );  // "/dev/ttyUSB0"
qiagen sens2( devicesIni.get_value("Qiagen", "Q2SerialPort") );  // "/dev/ttyUSB1"
// Check that it matches devices.ini file for Q1 and Q2.  Adjust which is Q1 (front one) and which Q2 (back one).
// put in control.cpp??  checkRenameQiagens(sens1, sens2, devicesIni);

// Set up the ADCs to use interrupts as well as declare them.
int DEVICE_ID = stoul(devicesIni.get_value("ADC", "ADC0DevID"));
int TEMP_ID = stoul(devicesIni.get_value("ADC", "ADC1DevID")); 
adc D2( DEVICE_ID, 0x8000, 0x7FFF);  // Was define DEVICE_ID, 0x48 .
adc TEMP( TEMP_ID, 0x8000, 0x7FFF);  // Was define TEMP_ID, 0x49 .

bool pwm_enable = (devicesIni.get_value("PWM", "Enable") == "true"); // Turn off PWM avoid sudo node caspar.js .
const double pwm_high_ratio = stof( devicesIni.get_value("PWM", "HighRatio") ); //0.85;
const int pwm_high = 1024.0*pwm_high_ratio;
const double pwm_low_ratio = stof( devicesIni.get_value("PWM", "LowRatio") );  //0.0;
const int pwm_low = 1024.0*pwm_low_ratio;

// Some GPIO extern values from the devices.ini file.
int HEATER_PIN = stoi( devicesIni.get_value("GPIO", "HEATER_PIN") ); // Typ 7.
int FAN_PIN = stoi( devicesIni.get_value("GPIO", "FAN_PIN") ); // Typ 4.
int BOX_FAN = stoi( devicesIni.get_value("GPIO", "BOX_FAN") ); // Typ 5.
int PWM_PIN = stoi( devicesIni.get_value("GPIO", "PWM_PIN") ); // Typ 23.
int ALERT_PIN = stoi( devicesIni.get_value("GPIO", "ALERT_PIN") ); // Typ 23.

// Miscellaneous Fit and RT values.  Some of these should be in Assay Recipes too.
int SMOOTHING = stoi( devicesIni.get_value("MISC", "SMOOTHING") );  // 25
int CONVERGENCE_THRESHOLD = stoi( devicesIni.get_value("MISC", "CONVERGENCE_THRESHOLD") );  // 1
int RT_LENGTH = stoi( devicesIni.get_value("MISC", "RT_LENGTH") );  // 600
int RT_TEMP = stoi( devicesIni.get_value("MISC", "RT_TEMP") );  // 60
int RT_WAITTOTEMP = stoi( devicesIni.get_value("MISC", "RT_WAITTOTEMP") );  // 55
int RECON_LENGTH = stoi( devicesIni.get_value("MISC", "RECON_LENGTH") );  // 600
int RECON_TEMP = stoi( devicesIni.get_value("MISC", "RECON_TEMP") );  // 55
int RECON_WAITTOTEMP = stoi( devicesIni.get_value("MISC", "RECON_WAITTOTEMP") );  // 55
// int CALIBRATION_MIN = stoi( devicesIni.get_value("MISC", "CALIBRATION_MIN") );  // 150
// Some fitting params in control.cpp L345-ish.
int ITER_MAX = stoi( devicesIni.get_value("MISC", "ITER_MAX") ); // 24
double AMPL_MIN = stod( devicesIni.get_value("MISC", "AMPL_MIN") ); // 10
double CTR_MIN = stod( devicesIni.get_value("MISC", "CTR_MIN") ); // 2
int ITER_MAX_PREMELT = stoi( devicesIni.get_value("MISC", "ITER_MAX_PREMELT") );
double AMPL_MIN_PREMELT = stod( devicesIni.get_value("MISC", "AMPL_MIN_PREMELT") );
double CTR_MIN_PREMELT = stod( devicesIni.get_value("MISC", "CTR_MIN_PREMELT") );
// Thresholds for switching, see control.cpp Lxxx.  Lower case variable names show up a lot in code.
double THRESH = stod( devicesIni.get_value("MISC", "THRESH") ); // 0.05
double THRESHCOOL = stod( devicesIni.get_value("MISC", "THRESHCOOL") ); // 0.135
double DTHRESHHEAT = stod( devicesIni.get_value("MISC", "DTHRESHHEAT") );  // 0.25
double DTHRESHCOOL = stod( devicesIni.get_value("MISC", "DTHRESHCOOL") );
// casparapi L65-102
double FluorCalibPremelt = stod( devicesIni.get_value("MISC", "FluorCalibPremelt") );  // 150
double FluorCalib = stod( devicesIni.get_value("MISC", "FluorCalib") ); // 300
double FluorCalibLDNA = stod( devicesIni.get_value("MISC", "FluorCalibLDNA") ); // 200

// Was in control.cpp L14.
double heattoolong = stod( devicesIni.get_value("MISC", "heattoolong") );  // secs, 75 typically, too long? 20221027 weg
double cooltoolong = stod( devicesIni.get_value("MISC", "cooltoolong") );  // secs, 75 typically

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
fstream notes_out;

// Declare cycle number variable.
int cycles = 0;
int cyclecutoff = 40;

// Set qiagen properties for fitting-- format is {QIAGEN, METHOD}
vector<int> LTP = {1,3};
vector<int> HTP = {1,3};
int fittingqiagen;

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
// For Project and Experiment names (might be none), use the
// directory ./data/ProjName/Expt/TimeStamp/<filename.csv> .
string dataDir = "./data/";
string coeffstorage = "coeff_" + ts + ".csv";
string pcrstorage = "pcr_" + ts + ".csv";
string notesstorage = "notes_" + ts + ".txt";
string rawstorage = "binaryoutput_" + ts + ".bin";
string runlogDir = "./";
string runlog = "runtimelog.txt";

/* Sets up the Pi's GPIO pins and initiates wiringPi's library for GPIO communications.
 */
void setupPi(void)
{
    string progName = "setupPi";
    // Print out the hardware config/ini file.
    // cout << devicesIni.print_file() << endl;
    wiringPiSetup();
    // Set pins for Alert, heating and cooling.
	pinMode(ALERT_PIN,INPUT);
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


/* Runs the calibration algorithm for the L-DNA. CALIBRATION_MIN can be set in devices.ini .
 */
// NOT USED ANYMORE, see casparapi.h and sensN.calibrateGain() calls.
// void calibrategain()
// {
//     string progName = "calibrategain";
//     ostringstream bstream;
//     sens1.LED_off(1);
//     sens1.LED_off(2);
//     sens1.setMethod(3); // Hard coded!  e2d2 here, so LED 2.
//     sens1.writeqiagen(0, {255,255});
//     // Start at our minimum gain for this qiagen.
//     // int basegain = 80;
//     int gainmin = sens1.getLED_min(2);
//     int gainmax = sens1.getLED_max(2);
//     int basegain = gainmin;  // Was 80.
//     double readinval;
//     // Log to console and file that we're entering calibration.
//     bstream << progName << ": Calibrating gain." << endl; 
//     cout << bstream.str();
//     runtime_out << bstream.str();
//     do
//     {
//         if(basegain > gainmax) // Was 244.  If our gain outstrips the limits of the qiagen, exit the loop.
//         // and log that we're pushing the limits of our optics.
//         {
//             bstream << progName << ": Couldn't find a suitable gain. Sample not present or illprepared." << endl;
//             bstream << "\tMin, Max allowed gains " << gainmin << ", " << gainmax << " and basegain at " << basegain << " ." << endl;
//             cout << bstream.str();
//             runtime_out << bstream.str();
//             break;
//         }
//         // Log to console what gain is being tested.
//         cout << progName << ": Testing at gain of: " << basegain << "." << endl;
//         // Turn the LED, adjust the current, and turn it back on.
//         sens1.LED_off(2);
//         sens1.LED_current(2,basegain);
//         sens1.LED_on(2);
//         sens1.startMethod();
//         delay(400);
//         // Measure what gain we receive.
//         readinval = sens1.measure();
//         // Log what reading we receive to console.
//         cout << progName << ": Reading: " << readinval << endl;
//         // Increase the gain.
//         basegain += 10;
//         sens1.stopMethod();
//     } while (readinval < CALIBRATION_MIN && runflag == true); // Continue while we haven't reached our 
//     // requested minimum calibration fluoresence. This is defined in the header.
// }



/* Opens files for fit information, raw binary data, PCR, and Notes. These will need to be able 
   to rewrite the headings influenced by recipe.
   Modified to follow directory structure  ./data/<projname>/<exptname>/ .  20221002 weg
 */
void openFiles()
{
    string progName = "openFiles";
    ostringstream bstream;
    // Open coefficient and PCR storage files from provided strings. These names are generated 
    // from the timestamp the instrument was started or user input + timestamp.
    cout << progName << ": before coeff_out.open(), setup.cpp line 234." << endl;
    coeff_out.open(dataDir + coeffstorage, std::ios_base::out);
    pcr_out.open(dataDir + pcrstorage, std::ios_base::out);
    notes_out.open(dataDir + notesstorage, std::ios_base::out);
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
    if(!notes_out.is_open())
    {
        bstream << progName << ": **Failed to open " << dataDir + notesstorage << endl;
        cout << bstream.str();
        runtime_out << bstream.str();
    }

    // Try to open the binary output file, log if it fails.
    output = fopen( (dataDir+rawstorage).c_str(), "wb" );
	if(output == NULL)
	{
        bstream << progName << ": **Failed to open " << dataDir+rawstorage << endl;
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
    if ( notes_out.is_open() ) notes_out.close();
    if ( oneOpen ) fclose(output);  // Kludge, likely but not guaranteed that if coeff_out OR pcr_out are open so is output.
}

// Must include <sys/stat.h> for the mkdir() function.  Requires a dir name and permissions, usually 0777.
// For subdirs, the function below splits string at Linux / (slashes), and cascades the directory creation.
// Likely the top ones, the first ones already exist, maybe the whole thing.
// Expect longdirname like ./data/myproject/myexperiment/timestamp/  , ignore last /. 
void doMakeDirs(string longdirname)
{
    string progName = "doMakeDirs";
    vector <string> subdirs;
    string delim = "/";
    string subdir, therest;
    int dlen = delim.length();
    // cout << "longdirname " << longdirname << " with delimiter " << delim << endl;

    // Split the longdirname into subdirectories.
    therest = longdirname;
    int pos = 0;
    while (pos < longdirname.length()-1 ) // Iteratively search for delimiter.
    {
        pos = therest.find(delim);
        if ( pos > 0 ) subdirs.push_back( therest.substr(0,pos) );
        therest = therest.substr(pos+dlen);  // Set to the string AFTER the delimiter.
    }

    // Printouts just for debugging.
    // cout << "All positions of delim..." << endl;
    // for ( int iter: posdelims ){
    //     cout << "pos is " << iter << endl;
    // }
    // cout << "All subdirs ... " << endl;
    // for ( string iter: subdirs ){
    //     cout << "subdir is " << iter << endl;
    // }
    // cout << endl;

    // Do the mkdir()'s.
    mode_t perm = 0777;
    string prev = "";  // Previous directory in the hierarchy, top to bottom subdirectory.
    int rc, rctot;
    for (string iter: subdirs)
    {
        if (iter==".") continue;  // Skip the . of ./.
        if (prev == "")  // First time through, top directory only.
        {
        rc = mkdir( iter.c_str(), perm);  // Many of these may already exist.  That is fine.
        // rc = 0 success, rc = -1 if already exists or a problem.
        prev = iter;
        } else  // Already made the top directory, these are all subdirs.
        {
        rc = mkdir( (prev+"/"+iter).c_str(), perm);
        prev = prev + "/" + iter;
        }
        if ( rc == -1 )
        {
            cout << progName << ": Either directory already exists or a problem creating it, like bad path." << endl;
            cout << progName << ": rc " << rc << " iter " << iter << " prev " << prev << endl;
            rctot = -1;
        }
        // All permissions look like 755 and not 777, careful playing with perms easy to make it not allow subdirectories.
    }      
    // return rctot;  // Do not need return code, the upper code will not fix any of this.
}

void doWriteComments(string savedComments, string savedStartTime, string savedFinishTime,
   string savedProjName, string savedOperator, string savedExperimentName)
{
    cout << "Comments: " << endl << savedComments << endl << savedStartTime << "   " << savedFinishTime << endl;
    cout << savedProjName << endl << savedOperator << endl << savedExperimentName << endl;

    string actExperimentName;
    actExperimentName = (savedExperimentName == "" ? "none": savedExperimentName);

    notes_out << "##################################################" << endl;
    notes_out << "Comments: " << endl << savedComments << endl;
    notes_out << "Start Time: " << savedStartTime << "   " << "Finish Time: " << savedFinishTime << endl;
    notes_out << "Project Name: " << savedProjName << endl;
    notes_out << "Operator Name: " << savedOperator << endl;
    notes_out << "Experiment Name (if given): " << actExperimentName << endl;
    notes_out << "##################################################" << endl;
}

// Put some parts or all of this in the Qiagen class when it reads the BoardID and the BoardSerialNumber
// IF it knows its number, that is.
// checkRenameQiagens(&sens1, &sens2, devicesIni);
/* Copmares the actual Qiagens with the devices.ini file and renames them, assigns a reference.
*/
void checkRenameQiagens(qiagen s1, qiagen s2, config devIni)
{
string Q1BoardID = devIni.get_value("Qiagen", "Q1BoardID");  // The expected board IDs.
string Q2BoardID = devIni.get_value("Qiagen", "Q2BoardID");
string Sens1BoardID = s1.getBoardID();  // The actual board IDs.
string Sens2BoardID = s2.getBoardID();
string Q1BoardSerialNumber = devIni.get_value("Qiagen", "Q1BoardSerialNumber");
string Q2BoardSerialNumber = devIni.get_value("Qiagen", "Q2BoardSerialNumber");
string Sens1BoardSerialNumber = s1.getBoardSerialNumber();
string Sens2BoardSerialNumber = s2.getBoardSerialNumber();

array<bool,4> myCheck;  // Check equality with BoardID and then Serial Number.
myCheck = {false, false, false, false};
//      S1ID=Q1ID S1S/N=Q1S/N etc S2 Q2

// Check Q1.
// if ( Sens1BoardID == Q1BoardID ) 
// {
//     myCheck[0] = true;
//     if ( Sens1BoardSerialNumber == Q1BoardSerialNumber )
//     }


}