/*
casparapi - All the interaction between the Javascript and the node.  A method accessible
to the Node needs to be in namespace caspar below.


*/
#include <node.h>  // /usr/include/node  Add to C/Cpp Edit Configurations for file c_cpp_properties.json.  Ctl-Sh-P type C/C++ select JSON configs.
#include <unistd.h>
#include "caspar.h"
#include <wiringPi.h>
#include <nan.h>   // CASPAR/node_modules/nan/nan.h
#include <iostream>
#include <sys/stat.h> // For the mkdir() function in Linux.
#include <vector>

namespace caspar
{

    using namespace Nan;
    using namespace v8;
    using v8::FunctionCallbackInfo;
    using v8::Isolate;
    using v8::Local;
    using v8::Number;
    using v8::Object;
    using v8::String;
    using v8::Value;
    int iterate = 0;
    int threadIter = 0; // For sampler thread below print statements.

    // Setup a thread to read the temperatures thermocouples,
    // and the ADC0 voltage if slow reads.  Other commented thread
    // for fast reading of Qiagen...as Nick originally planned.
    // Copying the pollSample and retrieveSample below.
    void pollTemperatureNADC()
    {
        while (runflag == true)
        {
            retrieveTemperatures();
            delay(temper_readeveryms);  // in ms
        }
    }
    PI_THREAD(readTemperatures)
    {
        pollTemperatureNADC();
        return 0;
    }


    // Setup a thread for reading the Qiagen LDNA channel using the
    // ADC.  Very fast but pretty much unused now.  See also 
    // piThreadCreate(sampler); in CASPARCycler::Execute .
    void pollSample()
    {
        while (runflag == true)
        {
            retrieveSample();
            delay(10);
        }
    }

    PI_THREAD(sampler)
    {
        pollSample();
        return 0;
    }

    class CASPARCycler : public AsyncWorker
    {
    public:
        CASPARCycler(Callback *callback) : AsyncWorker(callback) {}
        ~CASPARCycler(){};

        void Execute()
        {
            cycles = 0;
            clearactivedata();
            data.clear();
            // setupADC();  // Moved to initializePCR L145, weg, 20240223
            runflag = true;
            sens1->LED_off(1);
            sens1->LED_off(2);
            sens2->LED_off(1);
            sens2->LED_off(2);
            if (RTflag)
            {
                if (gainCalibration)
                {
                    if(LTP[0] == 1)
                    {
                        sens1->calibrateGain(FluorCalibPremelt,LTP[1]);
                    }
                    else
                    {
                        sens2->calibrateGain(FluorCalibPremelt,LTP[1]);
                    }
                    if(HTP[0] == 1)
                    {
                        sens1->calibrateGain(FluorCalibPremelt,HTP[1]);
                    }
                    else
                    {
                        sens2->calibrateGain(FluorCalibPremelt,HTP[1]);
                    }
                }                                
            }

            changeQiagen(HTP);
            piThreadCreate(sampler);
            piThreadCreate(readTemperatures);  // weg 20230410 adding in ADCs.
            delay(100);
            recordflag = true;
            if (RTflag)
            {
                premelt();
                reconstitute();
                premelt();
                runRT();
            }
            recordflag = false;
            delay(100);
            // After the RT step if there is one.
            // This is an AsyncWorker/thread, so check runflag just in case someone else changed it to false.
            sens1->LED_off(1);
            sens1->LED_off(2);
            sens2->LED_off(1);
            sens2->LED_off(2);
            // This logic below seems inefficient. weg 20240403
            // Calibrate every channel as regular PCR channel, then calibrate the HTP and LTP correctly.
            // This should be temporary until the PCR Chan <-> Qiagen and Method map implemented.  WEG 20240305
            if (runflag && gainCalibration) sens1->calibrateGain(FluorCalib, 1); // E1D1 520ex 570em, HEX
            if (runflag && gainCalibration) sens2->calibrateGain(FluorCalib, 1); // E1D1 470ex 520em, FAM
            if (runflag && gainCalibration) sens1->calibrateGain(FluorCalib, 3); // E2D2 625ex 680em, CY5
            if (runflag && gainCalibration) sens2->calibrateGain(FluorCalib, 3); // E2D2 590ex 640em, TXRED
            // Handle the LDNA and possible HTP and LTP channels and qiagens.  Will re-calib some of above channels.
            qiagen *sensHTP, *sensLTP;
            if (HTP==LTP)  // Single channel LDNA or "two hump".  Uses HTP vars from the config files.
            {
                sensHTP = (HTP[0]==1)? sens1 : sens2;                
                if (runflag && gainCalibration) sensHTP->calibrateGain(FluorCalibLDNAHTP, HTP[1]); 
            } else  // Two channel LDNA or "single hump", common for laser work and fast PCR cycling.
            {
                sensHTP = (HTP[0]==1)? sens1 : sens2;
                if (runflag && gainCalibration) sensHTP->calibrateGain(FluorCalibLDNAHTP, HTP[1]);

                sensLTP = (LTP[0]==1)? sens1 : sens2;               
                if (runflag && gainCalibration) sensLTP->calibrateGain(FluorCalibLDNALTP, LTP[1]);
            }

            changeQiagen(HTP);
            recordflag = true;
            delay(100);
            runerror = cycle();
        }

        void HandleOKCallback()
        {
            Nan::HandleScope scope;
            v8::Local<v8::Array> results = New<v8::Array>(1);
            Nan::Set(results, 0, New<v8::Number>(runerror));
            Local<Value> argv[] = {Null(), results};
            callback->Call(2, argv);
        }
    };// end CASPARCycler

    void initializePCR(const FunctionCallbackInfo<Value> &args)
    {
        cout << "casparapi.cpp: initializePCR(): At the start of routine." << endl;
        setupPi();
        setupQiagen();
        setupADC(); // weg, moving from Execute function??
        // if (dataDir != "./data/")// If DataDir not correctly defined from Start Run do not do this.  Maybe never do this. weg
        // {
        //     openFiles();
        // }
        cout << "casparapi.cpp: initializePCR(): At the end of routine." << endl;
    }

    void turnOffBoxFan(const FunctionCallbackInfo<Value> &args)
    {
        digitalWrite(BOX_FAN, LOW);
    }

    void RTon(const FunctionCallbackInfo<Value> &args)
    {
        RTflag = true;
    }

    void RToff(const FunctionCallbackInfo<Value> &args)
    {
        RTflag = false;
    }


    // Package the arrays for the fluor and deriv for the engine / caspar.js .  Called readdata() there.
    void readoutData(const FunctionCallbackInfo<Value> &args)
    {
        Isolate *isolate = args.GetIsolate();
        v8::Local<v8::Array> datahandoff = New<v8::Array>(2);
        double jsoutput;
        double derivoutput = 0;
        if (runflag == true && recordflag == true && y.size() > 0)
        {
            jsoutput = y[y.size() - 1];
        }
        else
        {
            jsoutput = 0;
        }
        if (derivs.size() > 5 && runflag == true && recordflag == true)
        {
            derivoutput = derivs[derivs.size() - 1];
        }
        else
        {
            derivoutput = 0;
        }
        // Local<Number> num = Number::New(isolate, jsoutput);
        Nan::Set(datahandoff, 0, New<v8::Number>(jsoutput));
        Nan::Set(datahandoff, 1, New<v8::Number>(derivoutput));
        args.GetReturnValue().Set(datahandoff);
    }

    void readoutTemp(const FunctionCallbackInfo<Value> &args)
    {
        Isolate *isolate = args.GetIsolate();
        double jsoutput;
        if (runflag == true && recordflag == true)
        {
            // float voltage = (TEMP->getreading() * 4.096) / 32767.0;
            // float temperature = (voltage - 1.25) / 0.005;
            float voltage = (TEMP->getreading() * temper_vmax) / temper_pow2effbits;
            float temperature = (voltage - temper_calibVoffset) / temper_calibSlope;
            jsoutput = temperature;
        }
        else
        {
            jsoutput = 0;
        }
        Local<Number> num = Number::New(isolate, jsoutput);
        args.GetReturnValue().Set(num);
    }

    void readoutPCR(const FunctionCallbackInfo<Value> &args)
    {
        Isolate *isolate = args.GetIsolate();
        v8::Local<v8::Array> pcrhandoff;
        if (pcrReady == true)
        {
            pcrhandoff = New<v8::Array>(pcrValues.size());
            for (int i = 0; i < pcrValues.size(); i++)
            {
                Nan::Set(pcrhandoff, i, New<v8::Number>(pcrValues[i]));
            }
            pcrValues.clear();
            pcrReady = false;
        }
        else
        {
            pcrhandoff = New<v8::Array>(pcrValues.size());
            for (int i = 0; i < pcrValues.size(); i++)
            {
                Nan::Set(pcrhandoff, i, New<v8::Number>(-1));
            }
        }
        args.GetReturnValue().Set(pcrhandoff);
    }

    /*
     * Accessible method to retrieve the current filenames and return them to the server.
     *
     * Args:
     * None
     *
     * Returns:
     * return[0]: Error log, Run log
     * return[1]: Coefficient log
     * return[2]: PCR data storage
     * return[3]: Raw data storage
     * return[4]: Notes storage
     * return[5]: Temperatures
     */
    void readoutFilenames(const FunctionCallbackInfo<Value> &args)
    {
        Isolate *isolate = args.GetIsolate();
        v8::Local<v8::Array> filehandoff = New<v8::Array>(6);
        Nan::Set(filehandoff, 0, New<v8::String>((runlogDir+runlog).c_str()).ToLocalChecked());
        Nan::Set(filehandoff, 1, New<v8::String>((dataDir+coeffstorage).c_str()).ToLocalChecked());
        Nan::Set(filehandoff, 2, New<v8::String>((dataDir+pcrstorage).c_str()).ToLocalChecked());
        Nan::Set(filehandoff, 3, New<v8::String>((dataDir+rawstorage).c_str()).ToLocalChecked());
        Nan::Set(filehandoff, 4, New<v8::String>((dataDir+notesstorage).c_str()).ToLocalChecked());
        Nan::Set(filehandoff, 5, New<v8::String>((dataDir+temperstorage).c_str()).ToLocalChecked());
        args.GetReturnValue().Set(filehandoff);
    }

    /* Accessible method to turn off the LEDs and recording algorithms.
     */
    void stopRun(const FunctionCallbackInfo<Value> &args)
    {
        runflag = false;
        recordflag = false;
        digitalWrite(HEATER_PIN, LOW);
        digitalWrite(FAN_PIN, LOW);
        cout << "stopRun:  pwm_enable is " << pwm_enable << endl;
        if (pwm_enable) pwmWrite(PWM_PIN, pwm_low);
        sens1->LED_off(1);
        sens1->LED_off(2);
        sens2->LED_off(1);
        sens2->LED_off(2);



    }
    /* Accessible method to shut down the device and delete dynamically allocated memory.
    */
    void stopEngine(const FunctionCallbackInfo<Value> &args)
    {
        delete sens1;
        delete sens2;
        delete ADC0;
        delete TEMP;
    }

    void writeComments(const FunctionCallbackInfo<Value> &args)
    {
        // savedComments, savedStartDate, savedStartTime, savedFinishTime, savedProjName, savedOperator, savedExperimentName
        Isolate *isolate = args.GetIsolate();
        String::Utf8Value strSavedComments(isolate, args[0]);
        String::Utf8Value strSavedStartDate(isolate, args[1]);
        String::Utf8Value strSavedStartTime(isolate, args[2]);
        String::Utf8Value strSavedFinishTime(isolate, args[3]);
        String::Utf8Value strSavedProjName(isolate, args[4]);
        String::Utf8Value strSavedOperator(isolate, args[5]);
        String::Utf8Value strSavedExperimentName(isolate, args[6]);
        string savedComments(*strSavedComments);
        string savedStartDate(*strSavedStartDate);
        string savedStartTime(*strSavedStartTime);
        string savedFinishTime(*strSavedFinishTime);
        string savedProjName(*strSavedProjName);
        string savedOperator(*strSavedOperator);
        string savedExperimentName(*strSavedExperimentName);

        cout << "Comments: " << endl << savedComments << endl << savedStartDate << "   " << savedStartTime << "   " << 
            savedFinishTime << endl;
        cout << savedProjName << endl << savedOperator << endl << savedExperimentName << endl;

        doWriteComments(savedComments, savedStartDate, savedStartTime, savedFinishTime,
            savedProjName, savedOperator, savedExperimentName); // fstream notes_out only known in setup.cpp!!

    }

    /*
     * Method for the server to change filenames. Takes strings from the node server and reopens the data output files with those names.
     *
     * Args:
     * args[0]: The new filename key.
     * args[1]: The project name for the directory.
     * args[2]: The experiment name, also for the directory.   ./data/projName/exptName (or none)/timeStamp/
     *
     */
    void reopenFiles(const FunctionCallbackInfo<Value> &args)
    {
        string progName = "reopenFiles";
        Isolate *isolate = args.GetIsolate();
        String::Utf8Value strName(isolate, args[0]);
        String::Utf8Value strProjName(isolate, args[1]);
        String::Utf8Value strExptName(isolate, args[2]);

        string newfilename(*strName);
        string projname(*strProjName);
        string exptname(*strExptName);
        newfilename = (newfilename == "" ? "none" : newfilename);  // Handle the null case.
        projname = (projname == "" ? "none" : projname);
        exptname = (exptname == "" ? "none" : exptname);
        string ts = timestamp();
        dataDir = "./data/" + projname + "/" + exptname + "/" + ts + "/";  // This is an extern, will be known to setup.cpp .
        doMakeDirs(dataDir);  // Create the directory structure, the lower mkdir() calls give error is directory already exists but still makes subdir.

        coeffstorage = "coeff_" + ts + ".csv";  // If new directory structure, do not need to prepend ProjName_ to files (?). weg
        pcrstorage = "pcr_" + ts + ".csv";
        notesstorage = "notes_" + ts + ".txt";
        temperstorage = "temper_" + ts + ".csv";
        rawstorage = "binaryoutput_" + ts + ".bin";
        cout << progName << ":  new directory dataDir " << dataDir << endl;
        cout << progName << ":  newfilename " << newfilename << " projname " << projname << " exptname " << exptname << endl;
        cout << progName << ":  coeffstorage " << coeffstorage << " pcrstorage " << pcrstorage << endl;
        cout << progName << ": rawstorage " << rawstorage << " notesstorage " << notesstorage << endl;
        cout << progName << ": temperstorage " << temperstorage << endl;
        closeFiles();
        openFiles();
    }

    void setCutoff(const FunctionCallbackInfo<Value>& args)
    {
        Isolate* isolate = args.GetIsolate();
        int cyclefromserver = args[0].As<Number>()->Value();
        cyclecutoff = cyclefromserver;
        cout << "setCutoff: Total cycles set: " << cyclecutoff << endl;
    }

    void setTotCycles(const FunctionCallbackInfo<Value>& args)
    {
        Isolate* isolate = args.GetIsolate();
        int cyclefromserver = args[0].As<Number>()->Value();
        cyclecutoff = cyclefromserver;
        cout << "setTotCycles: Total cycles set: " << cyclecutoff << endl;
    }

    void checkInit(const FunctionCallbackInfo<Value> &args)
    {
        // This function is a placeholder for later.
    }

    // setRecipeFilename - Should take as input the recipe directory,
    // and the selected recipe filename.  Possibility that the recipe file
    // does not exist if an old config called for it, i.e. it was not selected
    // by the dropdown.
    void setRecipeFilename(const FunctionCallbackInfo<Value> &args)
    {
        // savedComments, savedStartDate, savedStartTime, savedFinishTime, savedProjName, savedOperator, savedExperimentName
        Isolate *isolate = args.GetIsolate();
        String::Utf8Value strRecDir(isolate, args[0]);
        String::Utf8Value strSelectedRecipeFilename(isolate, args[1]);

        //string selectedRecipeFilename(*strSelectedRecipeFilename);
        recipeDir = string(*strRecDir);
        recipeFile = string(*strSelectedRecipeFilename);
        cout << "setRecipeFilename: using recipe filename " << recipeFile << " ," << endl;
        cout << "\twith recipe directory " << recipeDir << endl;
        cout << "\tRunning doRecipeConfig()." << endl;
        doRecipeConfig(recipeDir + recipeFile);  // Be sure to check for nonexistent recipeFile.
    }

    // sendADcs - Send the last read TC0, TC1, and ADC0 in engineering units.
    // void sendADCs(const FunctionCallbackInfo<Value> &args)
    // {
    //     Isolate *isolate = args.GetIsolate();
    //     String::Utf8Value adctstamp(isolate, args[0]);
    //     String::Utf8Value tc0(isolate, args[1]);
    //     String::Utf8Value tc1(isolate, args[2]);
    //     String::Utf8Value adc0(isolate, args[3]);


    // }

    // Gave up on the AsyncWorker for a symple thread, now called from XXX.
    // TemperatureNADC - like CASPARCycler, a class for reading the ADC for temperatures or other analog in (laser photodiode).
    // This may be overkill, see the PITHREAD(sample) above, created in the CASPARCycler class.
    class TemperatureNADC : public AsyncWorker
    {
        public:
            TemperatureNADC(Callback *atcallback) : AsyncWorker(atcallback) 
            {
                tcallback = atcallback;
            }
            Callback *tcallback; // Temperature callback
            ~TemperatureNADC(){};

        void Execute()
        {
            piThreadCreate(readTemperatures);
            delay(100); 
        }
        
        void HandleOKCallback()
        {
            Nan::HandleScope tscope;
            v8::Local<v8::Array> tresults = New<v8::Array>(1);
            Nan::Set(tresults, 0, New<v8::Number>(runerror));
            Local<Value> argv[] = {Null(), tresults};
            tcallback->Call(2, argv);
        }

    }; // end TemperatureNADC


    NAN_METHOD(Ignition)
    {
        Callback *callback = new Callback(info[0].As<Function>());
        AsyncQueueWorker(new CASPARCycler(callback));
    }

    // Nick suggest NOT doing it this way.  Keep it as a pithread in C++.
    // NAN_METHOD(Temperatures)
    // {
    //     Callback *tcallback = new Callback(info[0].As<Function>());
    //     AsyncQueueWorker(new TemperatureNADC(tcallback));
    // }

    void Initialize(Local<Object> exports)
    {
        Nan::Set(exports, New<String>("start").ToLocalChecked(), GetFunction(New<FunctionTemplate>(Ignition)).ToLocalChecked());
        // weg added 20230410 for the Temperatures asyncworker.  Nick says unnecessary to do this here/nodejs.  Just a c++ thread.
        // Nan::Set(exports, New<String>("start").ToLocalChecked(), GetFunction(New<FunctionTemplate>(Temperatures)).ToLocalChecked());
        NODE_SET_METHOD(exports, "initializePCR", initializePCR);
        NODE_SET_METHOD(exports, "stop", stopRun);
        NODE_SET_METHOD(exports, "kill", stopEngine);
        NODE_SET_METHOD(exports, "readdata", readoutData);
        //NODE_SET_METHOD(exports, "readtemp", readoutTemp);
        NODE_SET_METHOD(exports, "readPCR", readoutPCR);
        NODE_SET_METHOD(exports, "changefiles", reopenFiles);
        NODE_SET_METHOD(exports, "getfiles", readoutFilenames);
        NODE_SET_METHOD(exports, "boxfanoff", turnOffBoxFan);
        NODE_SET_METHOD(exports, "setCutoff", setCutoff);
        NODE_SET_METHOD(exports, "setTotCycles", setTotCycles);
        NODE_SET_METHOD(exports, "RECONoff", RToff);  // WEG, not used yet, will map to RECONoff, RECONon eventually.
        NODE_SET_METHOD(exports, "RECONon", RTon);    // ditto
        NODE_SET_METHOD(exports, "RToff", RToff);
        NODE_SET_METHOD(exports, "RTon", RTon);
        NODE_SET_METHOD(exports, "putComments", writeComments);
        NODE_SET_METHOD(exports, "setRecipeFilename", setRecipeFilename);  // Set the last pulled up recipe filename. 
        //NODE_SET_METHOD(exports, "sendADCs", sendADCs); // Send updated ADC values, pretty much all the time, but slowly.
        // WEG, make these functions, putComments in caspar.js and writeComments in casparapi.cpp  or setup.cpp ??
    }

    NODE_MODULE(casparengine, Initialize)  // The main deal here.

}// end namespace caspar
