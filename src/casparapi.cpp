/*
casparapi - All the interaction between the Javascript and the node.  A method accessible
to the Node needs to be in namespace caspar below.


*/
#include <node.h>  // /usr/include/node  Add to C/Cpp Edit Configurations for file c_cpp_properties.json.
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
            setupADC();
            runflag = true;
            sens1.LED_off(1);
            sens1.LED_off(2);
            sens2.LED_off(1);
            sens2.LED_off(2);
            if(LTP[0] == 1)
            {
                sens1.calibrateGain(FluorCalibPremelt,LTP[1]);
            }
            else
            {
                sens2.calibrateGain(FluorCalibPremelt,LTP[1]);
            }
            if(HTP[0] == 1)
            {
                sens1.calibrateGain(FluorCalibPremelt,HTP[1]);
            }
            else
            {
                sens2.calibrateGain(FluorCalibPremelt,HTP[1]);
            }
            changeQiagen(HTP);
            piThreadCreate(sampler);
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
            sens1.LED_off(1);
            sens1.LED_off(2);
            sens2.LED_off(1);
            sens2.LED_off(2);
            if (runflag) sens1.calibrateGain(FluorCalib, 1); // E1D1 470ex 520em, FAM
            if (runflag) sens2.calibrateGain(FluorCalib, 1); // E1D1 520ex 570em, HEX
            if (runflag) sens2.calibrateGain(FluorCalib, 3); // E2D2 625ex 680em, CY5
            if (runflag) sens1.calibrateGain(FluorCalibLDNA, 3); // LDNA, i.e.Tex Red, the "back qiagen". E2D2 590ex 640em
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
        openFiles();
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
            float voltage = (TEMP.getreading() * 4.096) / 32767.0;
            float temperature = (voltage - 1.25) / 0.005;
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
     */
    void readoutFilenames(const FunctionCallbackInfo<Value> &args)
    {
        Isolate *isolate = args.GetIsolate();
        v8::Local<v8::Array> filehandoff = New<v8::Array>(5);
        Nan::Set(filehandoff, 0, New<v8::String>((runlogDir+runlog).c_str()).ToLocalChecked());
        Nan::Set(filehandoff, 1, New<v8::String>((dataDir+coeffstorage).c_str()).ToLocalChecked());
        Nan::Set(filehandoff, 2, New<v8::String>((dataDir+pcrstorage).c_str()).ToLocalChecked());
        Nan::Set(filehandoff, 3, New<v8::String>((dataDir+rawstorage).c_str()).ToLocalChecked());
        Nan::Set(filehandoff, 4, New<v8::String>((dataDir+notesstorage).c_str()).ToLocalChecked());
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
        sens1.LED_off(1);
        sens1.LED_off(2);
        sens2.LED_off(1);
        sens2.LED_off(2);



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
        rawstorage = "binaryoutput_" + ts + ".bin";
        cout << "reopenFiles:  new directory dataDir " << dataDir << endl;
        cout << "reopenFiles:  newfilename " << newfilename << " projname " << projname << " exptname " << exptname << endl;
        cout << "reopenFiles:  coeffstorage " << coeffstorage << " pcrstorage " << pcrstorage << " rawstorage " << rawstorage;
        cout << " notesstorage " << notesstorage << endl;
        closeFiles();
        openFiles();
    }

    void setCutoff(const FunctionCallbackInfo<Value>& args)
    {
        Isolate* isolate = args.GetIsolate();
        int cyclefromserver = args[0].As<Number>()->Value();
        cyclecutoff = cyclefromserver;
        cout << "setCutoff: Cycles set: " << cyclecutoff << endl;
    }

    void checkInit(const FunctionCallbackInfo<Value> &args)
    {
        // This function is a placeholder for later.
    }

    NAN_METHOD(Ignition)
    {
        Callback *callback = new Callback(info[0].As<Function>());
        AsyncQueueWorker(new CASPARCycler(callback));
    }

    void Initialize(Local<Object> exports)
    {
        Nan::Set(exports, New<String>("start").ToLocalChecked(), GetFunction(New<FunctionTemplate>(Ignition)).ToLocalChecked());
        NODE_SET_METHOD(exports, "initializePCR", initializePCR);
        NODE_SET_METHOD(exports, "stop", stopRun);
        NODE_SET_METHOD(exports, "readdata", readoutData);
        NODE_SET_METHOD(exports, "readtemp", readoutTemp);
        NODE_SET_METHOD(exports, "readPCR", readoutPCR);
        NODE_SET_METHOD(exports, "changefiles", reopenFiles);
        NODE_SET_METHOD(exports, "getfiles", readoutFilenames);
        NODE_SET_METHOD(exports, "boxfanoff", turnOffBoxFan);
        NODE_SET_METHOD(exports, "setCutoff", setCutoff);
        NODE_SET_METHOD(exports, "RToff", RToff);
        NODE_SET_METHOD(exports, "RTon", RTon);
        NODE_SET_METHOD(exports, "putComments", writeComments); 
        // WEG, make these functions, putComments in caspar.js and writeComments in casparapi.cpp  or setup.cpp ??
    }

    NODE_MODULE(casparengine, Initialize)

}// end namespace caspar
