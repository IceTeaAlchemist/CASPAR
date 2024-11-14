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
#include "setup.h"

/* Sets up the Pi's GPIO pins and initiates wiringPi's library for GPIO communications.
 */

double phasestart;
double phaseend;
void setupPi(void)
{
    string progName = "setupPi";
    // Read the defaults for the devices.ini/Hardware and for the default.ini/Recipe.
    doHardwareConfig();  // Does NOT inlcude setupPWMLaser.  See below.

    // Print out the hardware config/ini file.
    // cout << devicesIni->print_file() << endl;
    wiringPiSetup();
    // Set pins for Alert, heating and cooling.
    pinMode(ALERT_PIN, INPUT);
    pinMode(HEATER_PIN, OUTPUT);
    pinMode(FAN_PIN, OUTPUT);
    pinMode(BOX_FAN, OUTPUT);
    
    // cout << progName << ": Info, pwm_enable is " << pwm_enable << endl;
    doRecipeConfig(); // Includes setupPWMLaser.

    digitalWrite(BOX_FAN, HIGH);
    digitalWrite(HEATER_PIN, LOW);
    // Open the runtime log file for appending and print the initialization time to it.
    runtime_out.open(runlogDir + runlog, std::ios_base::app | std::ios_base::in);
    runtime_out << progName << ":  PCR Session initialized at " << timestamp() << endl;
}

/* Sets up the Qiagen using default settings. Note that the qiagen itself is initialized externally. In
   the future this function will factor UI/recipe details.
 */
void setupQiagen(void)
{
    // Set Qiagen 1 sample protocol--Maximum samples with minimum delay between them. This is so we can use
    // it for cycling.
    sens1->writeqiagen(0, {255, 255}); // Cycles to read, 65535.
    sens1->writeqiagen(1, {00, 00});   // Cycle time 0s.
    sens1->writeqiagen(32, {01, 244}); // ADC Sampling 500 Hz.
    sens1->writeqiagen(5, {01, 00});   // Average 1 sample, no average.
    // Set Qiagen 2 sample protocol-- 200 samples with minimum delay between them.
    sens2->writeqiagen(0, {255, 255}); // Cycles to read, 65535.
    sens2->writeqiagen(1, {00, 00});  // Cycle time 0s.
    sens2->writeqiagen(32, {01, 244}); // ADC Sampling 500 Hz.
    sens2->writeqiagen(5, {01, 00});   // Average 1 sample, no average.
    // Note: The number of samples is basically irrelevant, just make sure it's more than 3 (Hz of
    // sample rate) * delay after the LED turns on in readPCR().

    // Set the LED power for the PCR wavelengths. Cycling sensing (sens1 LED 2) is set by the calibrator.
    //sens1->LED_current(1, 50);
    //sens2->LED_current(2, 120);
    //sens2->LED_current(1, 40);

    // Make sure all the LEDs besides the cycling one are off.
    sens2->LED_off(2);
    sens2->LED_off(1);
    sens1->LED_off(1);
    sens1->LED_off(2);

    // Set mode (beginning sampling routine after the LEDs are turned on) and method (which detector will be read).
    sens1->setMode(0);
    sens1->setMethod(3);
    sens2->setMode(0);
    sens2->setMethod(1);

    sens1->setPIDNos(PIDNos);
    sens2->setPIDNos(PIDNos);
}

/* Sets the ADC to run using the interrupt pin as well as the samples per second and mode. 
   ADC0 is for laser IMON or other generic, formerly D2 as the Qiagen detector (CASPA01 only) and
   TEMP is the temperature detectors, two thermocouples.
 */
void setupADC(void)
{
    string progName="setupADC";
    cout << progName << ": before ADC0->SetGain(adc0gain), with adc0gain = " << adc0gain << " ." << endl;
    // Setup the qiagen tapped ADC. See the class for documentation.
    ADC0->SetGain(adc0gain);
    ADC0->SetMode(adc0mode);
    ADC0->SetSPS(adc0sps);
    ADC0->SetCompPol(adc0comppol);
    ADC0->SetCompQueue(adc0compqueue);
    ADC0->SetMultiplex(adc0multiplex[0], adc0multiplex[1]);
    ADC0->SetCalibVoffset(imon_calibVoffset);
    ADC0->SetCalibSlope(imon_calibSlope);
    int chkconfig = ADC0->getconfig();
    cout << progName << ": ADC0->getconfig " << chkconfig;
    cout << " and hex " << hex << chkconfig << dec << endl;

    cout << progName << ": before TEMP->SetGain(adc1gain), with adc1gain = " << adc1gain << " ." << endl;
    // Setup the ADC for the temperature. See the ADC class for documentation.
    TEMP->SetGain(adc1gain); // For range +/- 4.096V .  adafruit-4-chan PDF page 13.
    TEMP->SetMode(adc1mode);
    TEMP->SetSPS(adc1sps);
    TEMP->SetCompPol(adc1comppol);
    TEMP->SetCompQueue(adc1compqueue);
    TEMP->SetMultiplex(adc1multiplex[0], adc1multiplex[1]);

    chkconfig = TEMP->getconfig();
    cout << progName << ": TEMP->getconfig " << chkconfig;
    cout << " and hex " << hex << chkconfig << dec << endl;
}

/* Runs the calibration algorithm for the L-DNA. CALIBRATION_MIN can be set in devices.ini .
 */
// NOT USED ANYMORE, see casparapi.cpp and Qiagen sensN.calibrateGain() calls.
// void calibrategain()
// {
//     string progName = "calibrategain";
//     ostringstream bstream;
//     sens1->LED_off(1);
//     sens1->LED_off(2);
//     sens1->setMethod(3); // Hard coded!  e2d2 here, so LED 2.
//     sens1->writeqiagen(0, {255,255});
//     // Start at our minimum gain for this qiagen.
//     // int basegain = 80;
//     int gainmin = sens1->getLED_min(2);
//     int gainmax = sens1->getLED_max(2);
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
//         sens1->LED_off(2);
//         sens1->LED_current(2,basegain);
//         sens1->LED_on(2);
//         sens1->startMethod();
//         delay(400);
//         // Measure what gain we receive.
//         readinval = sens1->measure();
//         // Log what reading we receive to console.
//         cout << progName << ": Reading: " << readinval << endl;
//         // Increase the gain.
//         basegain += 10;
//         sens1->stopMethod();
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
    // If dataDir is not properly defined then skip all of this.

    // Open coefficient and PCR storage files from provided strings. These names are generated
    // from the timestamp the instrument was started or user input + timestamp.
    coeff_out.open(dataDir + coeffstorage, std::ios_base::out);
    pcr_out.open(dataDir + pcrstorage, std::ios_base::out);
    notes_out.open(dataDir + notesstorage, std::ios_base::out);
    temper_out.open(dataDir + temperstorage, std::ios_base::out);
    // Dump column headings into the PCR file. Commas are so Excel will see this as a CSV.
    pcr_out << "Time,"
            << "FAM,"
            << "TEX,"
            << "HEX,"
            << "Cy5," << endl;
    // Check that both fstreams are open, log any failures.
    if (!coeff_out.is_open())
    {
        bstream << progName << ": **Failed to open " << dataDir + coeffstorage << endl;
        cout << bstream.str();
        runtime_out << bstream.str();
    }
    if (!pcr_out.is_open())
    {
        bstream << progName << ": **Failed to open " << dataDir + pcrstorage << endl;
        cout << bstream.str();
        runtime_out << bstream.str();
    }
    if (!notes_out.is_open())
    {
        bstream << progName << ": **Failed to open " << dataDir + notesstorage << endl;
        cout << bstream.str();
        runtime_out << bstream.str();
    }
    if (!temper_out.is_open())
    {
        bstream << progName << ": **Failed to open " << dataDir + temperstorage << endl;
        cout << bstream.str();
        runtime_out << bstream.str();
    }

    // Try to open the binary output file, log if it fails.
    output = fopen((dataDir + rawstorage).c_str(), "wb");
    if (output == NULL)
    {
        bstream << progName << ": **Failed to open " << dataDir + rawstorage << endl;
        cout << bstream.str();
        runtime_out << bstream.str();
    }
}

/* Closes all cycling output files.
 */
void closeFiles()
{
    bool oneOpen = (coeff_out.is_open()) || (pcr_out.is_open());
    if (coeff_out.is_open())
        coeff_out.close();
    if (pcr_out.is_open())
        pcr_out.close();
    if (notes_out.is_open())
        notes_out.close();
    if (temper_out.is_open())
        temper_out.close();
    if (oneOpen)
        fclose(output); // Kludge, likely but not guaranteed that if coeff_out OR pcr_out are open so is output.
}

// Must include <sys/stat.h> for the mkdir() function.  Requires a dir name and permissions, usually 0777.
// For subdirs, the function below splits string at Linux / (slashes), and cascades the directory creation.
// Likely the top ones, the first ones already exist, maybe the whole thing.
// Expect longdirname like ./data/myproject/myexperiment/timestamp/  , ignore last /.
void doMakeDirs(string longdirname)
{
    string progName = "doMakeDirs";
    vector<string> subdirs;
    string delim = "/";
    string subdir, therest;
    int dlen = delim.length();
    // cout << "longdirname " << longdirname << " with delimiter " << delim << endl;

    // Split the longdirname into subdirectories.
    therest = longdirname;
    int pos = 0;
    while (pos < longdirname.length() - 1) // Iteratively search for delimiter.
    {
        pos = therest.find(delim);
        if (pos > 0)
            subdirs.push_back(therest.substr(0, pos));
        therest = therest.substr(pos + dlen); // Set to the string AFTER the delimiter.
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
    string prev = ""; // Previous directory in the hierarchy, top to bottom subdirectory.
    int rc, rctot;
    for (string iter : subdirs)
    {
        if (iter == ".")
            continue;   // Skip the . of ./.
        if (prev == "") // First time through, top directory only.
        {
            rc = mkdir(iter.c_str(), perm); // Many of these may already exist.  That is fine.
            // rc = 0 success, rc = -1 if already exists or a problem.
            prev = iter;
        }
        else // Already made the top directory, these are all subdirs.
        {
            rc = mkdir((prev + "/" + iter).c_str(), perm);
            prev = prev + "/" + iter;
        }
        if (rc == -1)
        {
            cout << progName << ": Either directory already exists or a problem creating it, like bad path." << endl;
            cout << progName << ": rc " << rc << " iter " << iter << " prev " << prev << endl;
            rctot = -1;
        }
        // All permissions look like 755 and not 777, careful playing with perms easy to make it not allow subdirectories.
    }
    // return rctot;  // Do not need return code, the upper code will not fix any of this.
}

void doWriteComments(string savedComments, string savedStartDate, string savedStartTime, string savedFinishTime,
                     string savedProjName, string savedOperator, string savedExperimentName)
{
    cout << "doWriteComments: Comments: " << endl
         << savedComments << endl
         << savedStartDate << "   " << savedStartTime << "   " << savedFinishTime << endl;
    cout << savedProjName << endl
         << savedOperator << endl
         << savedExperimentName << endl;

    string useExperimentName;
    useExperimentName = (savedExperimentName == "" ? "none" : savedExperimentName);

    notes_out << "##################################################" << endl;
    notes_out << "Comments: " << endl
              << savedComments << endl;
    notes_out << "Start Date: " << savedStartDate << "   "
              << "Start Time: " << savedStartTime << "   "
              << "Finish Time: " << savedFinishTime << endl;
    notes_out << "Project Name: " << savedProjName << endl;
    notes_out << "Operator Name: " << savedOperator << endl;
    notes_out << "Experiment Name (if given): " << useExperimentName << endl;
    notes_out << "##################################################" << endl;
}

// Needed for the config files with strings like vectors "{0, -1}" .
// Returns the vector of ints {0, -1}, in this example.
vector<int> convertstr2vecint(string mystring)
{
    stringstream mystream(mystring.substr(1, mystring.length() - 2));
    vector<int> myvec;
    // cout << "convertstr2vecint: before while loop" << endl;
    while (mystream.good())
    {
        string substr;
        getline(mystream, substr, ',');
        // cout << "convertstr2vecint: substr is " << substr << " and mystring is " << mystring << endl;
        myvec.push_back(stoi(substr));
    }
    // cout << "convertstr2vecint: after while loop" << endl;
    return myvec;
}

// Needed for the config files with strings like vectors "1.0, 15, -15, 0.33, 0.0, 0.0" .
// Returns the vector of floats {1.0, 15.0, -15.0, 0.33, 0.0, 0.0} in this example.
vector<float> convertstr2vecfloats(string mystring)
{
    stringstream mystream(mystring.substr(0, mystring.length() - 1));
    vector<float> myvec;
    //cout << "convertstr2vecfloats: before while loop" << endl;
    while (mystream.good())
    {
        string substr;
        getline(mystream, substr, ',');
        //cout << "convertstr2vecfloats: substr is " << substr << " and mystring is " << mystring << endl;
        myvec.push_back(stof(substr));
    }
    // cout << "convertstr2vecfloats: after while loop, myvec is " << endl;
    // if (true)
    // {
    //     for (auto iter = myvec.begin(); iter != myvec.end(); iter++ )
    //     { 
    //         cout << *iter << " ";
    //     }
    //     cout << endl;
    // }
    return myvec;
}

// doHardwareConfig - reads an INI file for the unchanging hardware configuration of the system.
// 20230202 weg, started
void doHardwareConfig(string filename /*= "configs/devices.ini" */)
{
    ostringstream bstream; // For complicated printouts.
    string progName = "doHardwareConfig";

    /*
     * These declarations are all for the externally linked variables found in caspar.h
     * and used throughout the code. Please try to keep those declarations IN setup.cpp
     * for consistency.
     */

    // Read the config file with sections [Qiagen] and [ADC], etc.
    // Read the defaults for the devices.ini/Hardware and for the default.ini/Recipe.
    delete devicesIni;
    // What to do if filename does not exist?  Abort?
    // if ( ! fileExists(filename) )  // The filename does NOT exist.
    // {
    //     filename = "./configs/recipe/default.ini";
    //     bstream << progName << ": the requested recipe does not exist, " << filename << " ." << endl;
    //     bstream << "\t Setting it to default.ini ." << endl;
    //     cout << bstream.str();
    //     // runtimeout ?? does it exist on the first call?
    // }
    devicesIni = new config(filename); //"configs/devices.ini");

    BoxName = devicesIni->get_value("Box", "BoxName");
    bstream << progName << ": Reading file " << filename << " ." << endl;
    bstream << "\t For box " << BoxName << " ." << endl;
    cout << bstream.str();

    // Set up the qiagens on relevant USB ports.
    delete sens1;
    delete sens2;
    sens1 = new qiagen(devicesIni->get_value("Qiagen", "Q1SerialPort")); // "/dev/ttyUSB0"
    sens2 = new qiagen(devicesIni->get_value("Qiagen", "Q2SerialPort")); // "/dev/ttyUSB1"
    // Check that it matches devices.ini file for Q1 and Q2.  Adjust which is Q1 (front one) and which Q2 (back one).
    cout << progName << ": Before checkRenameQiagens, sens1 " << sens1 << " sens2 " << sens2 << endl;
    checkRenameQiagens(sens1, sens2, devicesIni);
    cout << progName << ": After swap, sens1 " << sens1 << " sens2 " << sens2 << endl;
    // Above calls by reference, have to de-pointer the pointers.
    // Double check that sens1 is Q1, OPPOSITE or Nick's definition.

    // Set up the ADCs to use interrupts as well as declare them.
    DEVICE_ID = stoi(devicesIni->get_value("ADC", "ADC0DevID"), 0, 16);
    TEMP_ID = stoi(devicesIni->get_value("ADC", "ADC1DevID"), 0, 16);
    cout << "We're past the ADC setup stage." << endl;
    // cout << progName << ": DEVICE_ID is (hex) " << hex << DEVICE_ID;
    // cout << " and TEMP_ID is (hex) " << TEMP_ID << dec << endl;
    delete ADC0;
    delete TEMP;
    ADC0 = new adc(DEVICE_ID, 0x8000, 0x7FFF); // Was define DEVICE_ID, 0x48 . // Already declared default constructor.
    TEMP = new adc(TEMP_ID, 0x8000, 0x7FFF); // Was define TEMP_ID, 0x49 .

    cout << "Declaring ADCs." << endl;

    temper_enable = (devicesIni->get_value("Temperature", "Enable") == "true");
    adc1gain = stoi(devicesIni->get_value("Temperature", "ADC1Gain"));
    adc1mode = stoi(devicesIni->get_value("Temperature", "ADC1Mode"));
    adc1sps = stoi(devicesIni->get_value("Temperature", "ADC1SPS"));
    adc1comppol = stoi(devicesIni->get_value("Temperature", "ADC1CompPol"));
    adc1compqueue = stoi(devicesIni->get_value("Temperature", "ADC1CompQueue"));
    adc1multiplex = convertstr2vecint(devicesIni->get_value("Temperature", "ADC1Multiplex"));
    temper_vmax = stof(devicesIni->get_value("Temperature", "Vmax"));
    temper_pow2effbits = stof(devicesIni->get_value("Temperature", "Pow2EffBits"));
    temper_calibVoffset = stof(devicesIni->get_value("Temperature", "CalibVoffset"));
    temper_calibSlope = stof(devicesIni->get_value("Temperature", "CalibSlope"));
    temper_readeveryms = stof(devicesIni->get_value("Temperature", "ReadEveryMS"));

    imon_enable = (devicesIni->get_value("ADC0Setup", "Enable") == "true");
    adc0gain = stoi(devicesIni->get_value("ADC0Setup", "ADC0Gain"));
    adc0mode = stoi(devicesIni->get_value("ADC0Setup", "ADC0Mode"));
    adc0sps = stoi(devicesIni->get_value("ADC0Setup", "ADC0SPS"));
    adc0comppol = stoi(devicesIni->get_value("ADC0Setup", "ADC0CompPol"));
    adc0compqueue = stoi(devicesIni->get_value("ADC0Setup", "ADC0CompQueue"));
    adc0multiplex = convertstr2vecint(devicesIni->get_value("ADC0Setup", "ADC0Multiplex"));
    imon_vmax = stof(devicesIni->get_value("ADC0Setup", "Vmax"));
    imon_pow2effbits = stof(devicesIni->get_value("ADC0Setup", "Pow2EffBits"));
    imon_calibVoffset = stof(devicesIni->get_value("ADC0Setup", "CalibVoffset"));
    imon_calibSlope = stof(devicesIni->get_value("ADC0Setup", "CalibSlope"));
    imon_readeveryms = stof(devicesIni->get_value("ADC0Setup", "ReadEveryMS"));
    
    cout << "ADCs setup." << endl;

    // PWM stuff is repeated in the recipe files too.
    pwm_enable = (devicesIni->get_value("PWM", "Enable") == "true");  // Turn off PWM avoid sudo node caspar.js .
    pwm_high_ratio = stof(devicesIni->get_value("PWM", "HighRatio")); // 0.85;
    pwm_high = 1024.0 * pwm_high_ratio;
    pwm_low_ratio = stof(devicesIni->get_value("PWM", "LowRatio")); // 0.0;
    pwm_low = 1024.0 * pwm_low_ratio;
    pwm_clock = stof(devicesIni->get_value("PWM", "Clock")); // 19.53;
    pwm_range = stoi(devicesIni->get_value("PWM", "Range")); // 1024;
    pwm_mode = stoi(devicesIni->get_value("PWM", "Mode"));   // 0;

    // Some GPIO extern values from the devices.ini file.
    HEATER_PIN = stoi(devicesIni->get_value("GPIO", "HEATER_PIN")); // Typ 7.
    FAN_PIN = stoi(devicesIni->get_value("GPIO", "FAN_PIN"));       // Typ 4.
    BOX_FAN = stoi(devicesIni->get_value("GPIO", "BOX_FAN"));       // Typ 5.
    PWM_PIN = stoi(devicesIni->get_value("GPIO", "PWM_PIN"));       // Typ 23.
    ALERT_PIN = stoi(devicesIni->get_value("GPIO", "ALERT_PIN"));   // Typ 23.

    DACAddress = stoi(devicesIni->get_value("DAC", "DACAddress"),0,16); // Default 0x60
    DACChannel = stoi(devicesIni->get_value("DAC", "DACChannel")); // Default Channel C, or 2.

    DACHandle = wiringPiI2CSetup(DACAddress);
    if(DACHandle < 0)
    {
        cout << "DAC Communication failed. " << endl;
    }
    else
    {
        cout << "Communicating with DAC with handle " << DACHandle << endl;
    }

    // Print at the end of the doHardwareConfig
    bstream << progName << ": At end of subroutine." << endl;
    cout << bstream.str();

} // end doHardwareConfig

// doRecipeConfig - Reads the INI file for the setup to almost every except hardware, so the cycle count, the
// channels to use on the Qiagens and for what, amplitude settings on the derivative check on the LDNA cycling, etc.
// Should be called at the start of every run, when Start is pressed.
// 20230202 weg, started
// 20230305 weg, Do a check for existence of the recipe (called for configs.txt it might no), use default.ini if not.
void doRecipeConfig(string filename /*= "configs/recipes/default.ini" */)
{
    cout << "DACHandle check #3: " << DACHandle << endl;
    ostringstream bstream; // For complicated printouts.
    string progName = "doRecipeConfig";

    /*
     * These declarations are all for the externally linked variables found in caspar.h
     * and used throughout the code. Please try to keep those declarations IN setup.cpp
     * for consistency.
     */

    // Read the config file with sections like [Qiagen] and [ADC], etc.
    delete recipeIni;
    if (!fileExists(filename)) // File does NOT exist.
    {
        filename = "./configs/recipes/default.ini";
        bstream << progName << ": the requested recipe does not exist, " << filename << " ." << endl;
        bstream << "\t Setting it to default.ini ." << endl;
        cout << bstream.str();
        // runtimeout ?? does it exist on the first call?
    }
    recipeIni = new config(filename); //"configs/recipes/default.ini");

    recipeVersion = recipeIni->get_value("Description", "RecipeVersion");
    recipeDate = recipeIni->get_value("Description", "Date");
    recipeDescShort = recipeIni->get_value("Description", "Short");
    recipeDescLong = recipeIni->get_value("Description", "Long");

    bstream << progName << ": Reading recipe named " << filename << " ." << endl;
    bstream << "\t dated " << recipeDate << " and with short description " << endl;
    bstream << "\t" << recipeDescShort << endl;
    cout << bstream.str();

    // Setup the Qiagen mappings, see the [Channel] section of the recipe file.
    // Also sets the boolean gainCalibration from recipe.
    readQiagenMappings(recipeIni);

    // Laser control is in the recipes file.
    pwm_enable = (recipeIni->get_value("PWM", "Enable") == "true");  // Turn off PWM avoid sudo node caspar.js .
    pwm_high_ratio = stof(recipeIni->get_value("PWM", "HighRatio")); // 0.85;
    pwm_high = 1024.0 * pwm_high_ratio;
    pwm_low_ratio = stof(recipeIni->get_value("PWM", "LowRatio")); // 0.0;
    pwm_low = 1024.0 * pwm_low_ratio;
    pwm_clock = stof(recipeIni->get_value("PWM", "Clock")); // 19.53;
    pwm_range = stoi(recipeIni->get_value("PWM", "Range")); // 1024;
    pwm_mode = stoi(recipeIni->get_value("PWM", "Mode"));   // 0;
    // Re-setup the PWM parameters, in case they changed.
    setupPWMLaser();

    // Miscellaneous Fit and RT values.  Some of these should be in Assay Recipes too.
    SMOOTHING = stoi(recipeIni->get_value("Fitting", "SMOOTHING"));                         // 25
    CONVERGENCE_THRESHOLD = stoi(recipeIni->get_value("Fitting", "CONVERGENCE_THRESHOLD")); // 1

    RT_Enable = (recipeIni->get_value("RT", "RTEnable") == "true");
    RT_LENGTH = stoi(recipeIni->get_value("RT", "RT_LENGTH"));         // 600
    RT_TEMP = stoi(recipeIni->get_value("RT", "RT_TEMP"));             // 60
    RT_WAITTOTEMP = stoi(recipeIni->get_value("RT", "RT_WAITTOTEMP")); // 55

    RECON_Enable = (recipeIni->get_value("Recon", "ReconEnable") == "true");
    RECON_LENGTH = stoi(recipeIni->get_value("Recon", "RECON_LENGTH"));         // 600
    RECON_TEMP = stoi(recipeIni->get_value("Recon", "RECON_TEMP"));             // 55
    RECON_WAITTOTEMP = stoi(recipeIni->get_value("Recon", "RECON_WAITTOTEMP")); // 55
    // int CALIBRATION_MIN = stoi( devicesIni->get_value("MISC", "CALIBRATION_MIN") );  // 150

    // Some fitting params in control.cpp L345-ish.
    ITER_MAX = stoi(recipeIni->get_value("Cycling", "ITER_MAX")); // 24
    AMPL_MIN_HTP = stod(recipeIni->get_value("Cycling", "AMPL_MIN_HTP")); // 10
    CTR_MIN_HTP = stod(recipeIni->get_value("Cycling", "CTR_MIN_HTP"));   // 2
    AMPL_MIN_LTP = stod(recipeIni->get_value("Cycling", "AMPL_MIN_LTP")); // 10
    CTR_MIN_LTP = stod(recipeIni->get_value("Cycling", "CTR_MIN_LTP"));   // 2    
    ITER_MAX_PREMELT = stoi(recipeIni->get_value("Recon", "ITER_MAX_PREMELT"));
    AMPL_MIN_PREMELT = stod(recipeIni->get_value("Recon", "AMPL_MIN_PREMELT"));
    CTR_MIN_PREMELT = stod(recipeIni->get_value("Recon", "CTR_MIN_PREMELT"));
    // The Channels used.  Set qiagen properties for fitting-- format is {QIAGEN, METHOD}

    LTP = convertstr2vecint(recipeIni->get_value("Cycling", "LTP"));
    HTP = convertstr2vecint(recipeIni->get_value("Cycling", "HTP"));
    PIDNos = convertstr2vecfloats(recipeIni->get_value("Cycling", "PID"));
    // int fittingqiagen;
    // If LTP == HTP then it is in double hump, if not equal then single hump.
    if (LTP[0]==HTP[0] && LTP[1]==HTP[1]) single_hump = false;
    else single_hump = true;

    // Thresholds for switching, see control.cpp Lxxx.  Lower case variable names show up a lot in code.
    THRESH = stod(recipeIni->get_value("Cycling", "THRESH"));           // 0.05
    THRESHCOOL = stod(recipeIni->get_value("Cycling", "THRESHCOOL"));   // 0.135
    DTHRESHHEAT = stod(recipeIni->get_value("Cycling", "DTHRESHHEAT")); // 0.25
    DTHRESHCOOL = stod(recipeIni->get_value("Cycling", "DTHRESHCOOL"));
    FluorCalib = stod(recipeIni->get_value("Cycling", "FluorCalib"));         // 300
    FluorCalibLDNAHTP = stod(recipeIni->get_value("Cycling", "FluorCalibLDNAHTP")); // 200
    FluorCalibLDNALTP = stod(recipeIni->get_value("Cycling", "FluorCalibLDNALTP")); // 200
    // casparapi L65-102
    FluorCalibPremelt = stod(recipeIni->get_value("RT", "FluorCalibPremelt")); // 150
    // Was in control.cpp L14.
    heattoolongfirst = stod(recipeIni->get_value("Errors", "heattoolongfirst"));             // secs, 75 typically first cycle heating
    heattoolong = stod(recipeIni->get_value("Errors", "heattoolong"));                 // secs, 40 typ, second and other heating cycles
    cooltoolong = stod(recipeIni->get_value("Errors", "cooltoolong"));                 // secs, 40 typ, first and other cooling cycles
    allowed_temp_errors = stoi(recipeIni->get_value("Errors", "allowed_temp_errors")); // 3-ish
    // Declare cycle number variable.
    cycles = 0;

    // Need to send this to the UI, and/or handle the different cyclecutoff that occurs there as input.
    cyclecutoff = stoi(recipeIni->get_value("Cycling", "cyclesMax"));
    cyclesfit = stoi(recipeIni->get_value("Cycling", "cyclesFit"));
    cyclesaverage = stoi(recipeIni->get_value("Cycling", "autopilotAverage"));

    // Declare the file operator and a few check variables for the thread.
    // FILE *output;
    datapoints = 0;
    run_start = 0;

    // Set up flags for threads.
    RTdone = false;
    recordflag = false;
    runerror;
    temperrors;
    RTflag = false;

    // Get initialization time, format YYYYMMDD_HHMMSS .
    string ts = timestamp();

    // Declare initial file names for saving the data.
    // For Project and Experiment names (might be none), use the
    // directory ./data/ProjName/Expt/TimeStamp/<filename.csv> .
    dataDir = "./data/";
    coeffstorage = "coeff_" + ts + ".csv"; // these are not really used with openFiles() in setup gone.
    pcrstorage = "pcr_" + ts + ".csv";
    temperstorage = "temper_" + ts + ".csv";
    notesstorage = "notes_" + ts + ".txt";
    rawstorage = "binaryoutput_" + ts + ".bin";
    runlogDir = "./";
    runlog = "runtimelog.txt";

    bstream << progName << ": At end of the subroutine." << endl;
    cout << bstream.str();

} // end doRecipeConfig

// Put some parts or all of this in the Qiagen class when it reads the BoardID and the BoardSerialNumber
// IF it knows its number, that is.
// checkRenameQiagens(&sens1, &sens2, devicesIni);
// Compares the actual Qiagens with the devices.ini file and renames them, assigns a reference.

// Usually called with aa as sens1 and assigned to Q1SerialPort, but ports can
// change if plugged and unplugged.  Also changing the definition of Q1 and Q2
// from Nick's.
// Found that I need refrences in the below and de-pointer the qiagens
// coming in.

void checkRenameQiagens(qiagen *&s1, qiagen *&s2, config *devIni)
{
    ostringstream bstream;
    string progName = "checkRenameQiagens";

    string s1BoardID, s1SerialNo, s2BoardID, s2SerialNo;
    string Q1BoardID, Q1SerialNo, Q2BoardID, Q2SerialNo;
    // Read from the hardware config, devices.ini .
    Q1BoardID = devIni->get_value("Qiagen", "Q1BoardID");
    Q1SerialNo = devIni->get_value("Qiagen", "Q1SerialNo");
    Q2BoardID = devIni->get_value("Qiagen", "Q2BoardID");
    Q2SerialNo = devIni->get_value("Qiagen", "Q2SerialNo");
    // Grab the actual value filled in when the Qiagens were instantiated.
    s1BoardID = s1->getBoardID().substr(0, 14);           // ESML10-MB-3021??   ? = junk chars
    s1SerialNo = s1->getBoardSerialNumber().substr(0, 4); // 0016????
    s2BoardID = s2->getBoardID().substr(0, 14);
    s2SerialNo = s2->getBoardSerialNumber().substr(0, 4);
    // Because of the junk characters read from the qiagens only check the
    // first 14 chars on BoardID, and 4 chars on SerialNo.
    bstream << progName << ":Q1BoardID (config file) s1BoardID (USB bus)" << endl;
    bstream << "\t" << Q1BoardID << "\t" << s1BoardID << endl;
    bstream << "\t" << "Q1SerialNo s1SerialNo" << endl;
    bstream << "\t" << Q1SerialNo << "\t" << s1SerialNo << endl;
    bstream << "\t" << "Q2BoardID s2BoardID" << endl;
    bstream << "\t" << Q2BoardID << "\t" << s2BoardID << endl;
    bstream << "\t" << "Q2SerialNo s2SerialNo" << endl;
    bstream << "\t" << Q2SerialNo << "\t" << s2SerialNo << endl;
    cout << bstream.str();

    if (s1BoardID == Q1BoardID && s2BoardID == Q2BoardID &&
        s1SerialNo == Q1SerialNo && s2SerialNo == Q2SerialNo)
    {
        cout << progName << ": Qiagens are not swapped, keeping them as they are." << endl;
        return; // Everything is okay.
    }
    else if (s1BoardID == Q2BoardID && s1SerialNo == Q2SerialNo &&
             s2BoardID == Q1BoardID && s2SerialNo == Q1SerialNo)
    {
        bstream << progName << ": Qiagens not matching hardware config order." << endl;
        bstream << "\tLooks like first one matches Q2 and second Q1." << endl;
        bstream << "\tSwapping pointers." << endl;
        cout << bstream.str();

        cout << progName << ": Before swap, s1 " << s1 << " s2 " << s2 << endl;
        std::swap(s1, s2); // Likely the USB ports were swapped.
        cout << progName << ": After swap, s1 " << s1 << " s2 " << s2 << endl;

        return;
    }

    // Some other problem.
    bstream << endl
            << string(40, '=') << endl;
    bstream << progName << ": Qiagens not matching hardware config." << endl;
    bstream << "\t Check the devices.ini-><correct file> in configs folder and restart." << endl;
    bstream << "\t Not changing anything, running swapped or wrong or whatever." << endl;
    bstream << string(40, '=') << endl
            << endl;
    cout << bstream.str();

    return;

} // end checkRenameQiagens

// setupPWMLaser - to set the necessary RPi parameters to run PWM, etc.
// This MUST be called after the wiringPiSetup() function near the top is called, for pinMode, etc.
// 20230329 weg
void setupPWMLaser()
{
    string progName = "setupPWMLaser";

    pinMode(PWM_PIN, PWM_OUTPUT);
    pwmSetMode(pwm_mode);   //(PWM_MODE_MS);
    pwmSetClock(pwm_clock); //(19.53); // See wiringPi.h, takes an int.
    // Clock should be 19.2e6 divided by the desired Hz (1000) and then the Range (1024),
    // usually like 19.2 Mhz / 1024 / 1000 = 19 (an int).

    pwmSetRange(pwm_range); //(1024);
    // Set up the interrupt for sample reading. Note that these CANNOT be turned off once started,
    // so wrapping them in a boolean with a flag is a good idea.
    // wiringPiISR(3,INT_EDGE_RISING,sampletriggered);
    // Open the runtime log file for appending and print the initialization time to it.
    delay(1000);
    cout << progName << ":  pwm_enable is " << pwm_enable << " pwm_low is " << pwm_low << ", pwm_high is " << pwm_high << endl;
    cout << "\t pwm_low_ratio " << pwm_low_ratio << " pwm_high_ratio " << pwm_high_ratio << endl;
    pwmWrite(PWM_PIN, pwm_low);
}

// setup_qiagen_mappings - read from the recipes file lines like 
// [Channels]
// HTP = Q1_E1D1
// HTPQiagen = Q1
// ...
// and write out files that map HTP, LTP, PCRA, PCRB, PCRC, PCRD to qiagens and channels.  Our Qiagen class uses {1,3} for qiagen and method.
// Look at data.cpp:readPCR() etc for how these mappings will be used.
// Find and remove all hardcoded LED_on/offs, etc.  Well, except when turning them all off, maybe.
// 20230911 weg
// ?? put all the following in a class??
void readQiagenMappings(config* recipeIni)
{
    ostringstream bstream; // For complicated printouts.
    string progName = "readQiagenMappings";

    //recipeIni = new config(filename); //"configs/recipes/default.ini");

    //recipeVersion = recipeIni->get_value("Description", "RecipeVersion");
    // Channels are HTP, LTP, PCRA, PCRB, PCRC, PCRD, Recon, RT for the main 
    // channels as of now (20230914).
    // Want there qiagen number as int, and method number as integer.

    // Use the qiagen mappings class for handling all the directions of the mappings.
    // For now (20230911) need the know the HTP, LTP, etc Qiagen numbers (1 or 2) and 
    // their Channels/Methods (1 or 3 as of now).

    qiagenMap myQiagenMap(recipeIni);  // ??Also in setup.h and caspar.h.

    // If true, do the gain calibration...usually always do the gain calibration.
    gainCalibration = ( recipeIni->get_value("Channels", "GainCalibration") == "true" );

}

