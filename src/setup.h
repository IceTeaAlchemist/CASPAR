#ifndef SETUP_H
#define SETUP_H

// To take care of the "red" in VSCode.
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
config *devicesIni;
config *recipeIni;
string BoxName; 
string recipeVersion;
string recipeDate;
string recipeDescShort;
string recipeDescLong;

// Set up the qiagens on relevant USB ports.
qiagen *sens1;  // "/dev/ttyUSB0"
qiagen *sens2;  // "/dev/ttyUSB1"
// Check that it matches devices.ini file for Q1 and Q2.  Adjust which is Q1 (front one) and which 
// Q2 (back one).
qiagenMap myQiagenMap;  // Mapping of HTP, LTP, etc to Qiagen number and method/channel.
bool gainCalibration; // If true, do the gain calibration...usually always do the gain calibration.

// Set up the ADCs to use interrupts as well as declare them.
int DEVICE_ID;
int TEMP_ID; 
adc *ADC0;  // Was define DEVICE_ID, 0x48 .  Generic for laser IMON or other.
adc *TEMP;  // Was define TEMP_ID, 0x49 .
int DACHandle;
int DACChannel;
int DACAddress;

bool pwm_enable; // Turn off PWM avoid sudo node caspar.js .
double pwm_high_ratio; //0.85;
int pwm_high;
double pwm_low_ratio;  //0.0;
int pwm_low;
float pwm_clock;  //19.53;
int pwm_range;  //1024;
int pwm_mode;  //0;

bool temper_enable;
int adc1gain;
int adc1mode;
int adc1sps;
int adc1comppol;
int adc1compqueue;
vector<int> adc1multiplex;
float temper_vmax;
float temper_pow2effbits;
float temper_calibVoffset;
float temper_calibSlope;
float temper_readeveryms;

bool imon_enable;
int adc0gain;
int adc0mode;
int adc0sps;
int adc0comppol;
int adc0compqueue;
vector<int> adc0multiplex;
float imon_vmax;
float imon_pow2effbits;
float imon_calibVoffset;
float imon_calibSlope;
float imon_readeveryms;
// Some GPIO extern values from the devices.ini file.
int HEATER_PIN; // Typ 7
int FAN_PIN; // Typ 4
int BOX_FAN; // Typ 5
int PWM_PIN; // Typ 23
int ALERT_PIN; // Typ 23

// Miscellaneous Fit and RT values.  Some of these should be in Assay Recipes too.
int SMOOTHING; // 25
int CONVERGENCE_THRESHOLD; // 1
bool RT_Enable; 
int RT_LENGTH; // 600
int RT_TEMP; // 60
int RT_WAITTOTEMP; // 55
bool RECON_Enable;
int RECON_LENGTH;  // 600
int RECON_TEMP;  // 55
int RECON_WAITTOTEMP; // 55
// int CALIBRATION_MIN = stoi( devicesIni.get_value("MISC", "CALIBRATION_MIN") );  // 150
// Some fitting params in control.cpp L345-ish.
int ITER_MAX; // 24
double AMPL_MIN_HTP; // 10
double CTR_MIN_HTP; // 2
double AMPL_MIN_LTP; // 10
double CTR_MIN_LTP; // 2
int ITER_MAX_PREMELT;
double AMPL_MIN_PREMELT;
double CTR_MIN_PREMELT;
// Thresholds for switching, see control.cpp Lxxx.  Lower case variable names show up a lot in code.
double THRESH; // 0.05
double THRESHCOOL; // 0.135
double DTHRESHHEAT;  // 0.25
double DTHRESHCOOL;
// casparapi L65-102
double FluorCalibPremelt;  // 150
double FluorCalib; // 300
double FluorCalibLDNAHTP; // 200
double FluorCalibLDNALTP; // 200

// Was in control.cpp L14.
double heattoolongfirst; // secs, 75 typically, first heating cycle.
double heattoolong;  // secs, 40 typ., second and up heating cycle check
double cooltoolong;  // secs, 40 typ., first and up coolin gcycle check.
int allowed_temp_errors;  // 3-ish

// Declare vectors for tracking the thermal/fluor correspondence.
// vector<double> tempkey;  // moved to test of setup.h
// vector<double> fluorkey;

// Declare all the vectors for storing data by the interrupt thread
vector<reading> data;
vector<error> errorArray;
vector<double> x;
vector<double> y;
double meltout = 0;
double meltderivout = 0;
vector<double> xderivs;
vector<double> derivs;

// Declare the vectors for the Temperature (and other ADC) data, in thread,
// see data.cpp and casparapi.cpp.
vector<reading> data0_tempers;
vector<reading> data1_tempers;
vector<reading> data0_adc0;
vector<double> x0_tempers;
vector<double> y0_tempers;
vector<double> x1_tempers;
vector<double> y1_tempers;
vector<double> x0_adc0;
vector<double> y0_adc0;
vector<double> meltvals;

// Declare deques for the moving average filter. SMOOTHING is NOT defined in caspar.h.  Read from config file.
deque<double> yaverage;
deque<double> derivaverage;
deque<double> meltaverage;

// Declare fstream objects globally.
fstream coeff_out;
fstream pcr_out;
fstream runtime_out;
fstream notes_out;
fstream temper_out;
fstream melt_out;

// Declare cycle number variable.  ADD THESE TO RECIPE.
int cycles = 0;
int cyclecutoff = 40;
int cyclesfit;
int cyclesaverage;
vector<int> channelflags{1,1,1,1}; //Set up which channels to read. By default, all are on.

// Set qiagen properties for fitting-- format is {QIAGEN, METHOD}
vector<int> LTP;
vector<int> HTP;
vector<int> MELTP;
vector<int> RT = {2,1};
vector<float> PIDNos;
bool single_hump = false;  // If true then single hump running, if false double.
int fittingqiagen;

double RTfluor;


// Declare the file operator and a few check variables for the thread.
FILE *output;
int datapoints = 0;
double run_start = 0;

// Set up flags for threads.
bool RTdone;
bool recordflag;
int runerror;
int temperrors;
bool RTflag;
bool meltflag;
bool overdriveflag;
bool HOLD_ON;
double HOLD_LENGTH;
double LASER_POWER;
double hold_power = 1.00;
double cool_power = 0.0;
bool active_cooling = true;
int overdrive_start;
double overdrive_power = 1.95;
double power_setting = 1.95;
bool autopilotmelt = true;


// Get initialization time, format YYYYMMDD_HHMMSS .
string ts;

// Declare initial file names for saving the data. 
// For Project and Experiment names (might be none), use the
// directory ./data/ProjName/Expt/TimeStamp/<filename.csv> .
string dataDir;
string coeffstorage;  // these are not really used with openFiles() in setup gone.
string pcrstorage;
string temperstorage;
string notesstorage;
string rawstorage;
string runlogDir;
string runlog;

// Declare vectors for tracking the thermal/fluor correspondence.
vector<double> tempkey;
vector<double> fluorkey;

void checkRenameQiagens(qiagen*&, qiagen*&, config*);

string devicesFile = "devices.ini";  // default file name, see setup.cpp
string devicesDir = "./configs/";   // default
string recipeFile = "default.ini";  // default file name
string recipeDir = "./configs/recipes/";  // default

void setupPWMLaser();
void readQiagenMappings(config* recipeIni);

#endif
