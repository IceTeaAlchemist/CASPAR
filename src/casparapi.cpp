#include <node.h>  // /usr/include/node  Add to C/Cpp Edit Configurations for file c_cpp_properties.json.
#include <unistd.h>
#include "caspar.h"
#include <wiringPi.h>
#include <nan.h>   // CASPAR/node_modules/nan/nan.h
#include <iostream>

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
            calibrategain();
            sens1.startMethod();
            piThreadCreate(sampler);
            delay(100);
            recordflag = true;
            if (RTflag)
            {
                premelt();
                runRT();
            }
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
    };

    void initializePCR(const FunctionCallbackInfo<Value> &args)
    {
        setupPi();
        setupQiagen();
        openFiles();
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
     * return[0]: Error log
     * return[1]: Coefficient log
     * return[2]: PCR data storage
     * return[3]: Raw data storage
     */
    void readoutFilenames(const FunctionCallbackInfo<Value> &args)
    {
        Isolate *isolate = args.GetIsolate();
        v8::Local<v8::Array> filehandoff = New<v8::Array>(4);
        Nan::Set(filehandoff, 0, New<v8::String>(runlog.c_str()).ToLocalChecked());
        Nan::Set(filehandoff, 1, New<v8::String>(coeffstorage.c_str()).ToLocalChecked());
        Nan::Set(filehandoff, 2, New<v8::String>(pcrstorage.c_str()).ToLocalChecked());
        Nan::Set(filehandoff, 3, New<v8::String>(rawstorage.c_str()).ToLocalChecked());
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
        cout << "stopRun:  Info: pwm_enable is " << pwm_enable << endl;
        if (pwm_enable) pwmWrite(PWM_PIN, pwm_low);
        sens1.LED_off(1);
        sens1.LED_off(2);
        sens2.LED_off(1);
        sens2.LED_off(2);
    }

    /*
     * Accessible method for the server to change filenames. Takes a string from the node server and reopens the data output files with that filename.
     *
     * Args:
     * args[0]: The new filename key.
     *
     */
    void reopenFiles(const FunctionCallbackInfo<Value> &args)
    {
        Isolate *isolate = args.GetIsolate();
        String::Utf8Value str(isolate, args[0]);

        string newfilename(*str);
        string ts = timestamp();

        coeffstorage = "./data/" + newfilename + "coeff" + ts + ".txt";
        pcrstorage = "./data/" + newfilename + "pcr" + ts + ".txt";
        rawstorage = "./data/" + newfilename + "binaryoutput" + ts + ".bin";
        closeFiles();
        openFiles();
    }

    void setCutoff(const FunctionCallbackInfo<Value>& args)
    {
        Isolate* isolate = args.GetIsolate();
        int cyclefromserver = args[0].As<Number>()->Value();
        cyclecutoff = cyclefromserver;
        cout << "Cycles set: " << cyclecutoff << endl;
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
    }

    NODE_MODULE(casparengine, Initialize)

}