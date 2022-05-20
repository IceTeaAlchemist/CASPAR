#include <node.h>
#include <unistd.h>
#include "caspar.h"
#include <nan.h>

namespace demo 
{

    using namespace Nan;
    using namespace v8;
    using v8::FunctionCallbackInfo;
    using v8::Isolate;
    using v8::Local;
    using v8::Object;
    using v8::String;
    using v8::Number;
    using v8::Value;
    int iterate = 0;


    void startRun();

    class CASPARCycler : public AsyncWorker{
        public:
        CASPARCycler(Callback *callback) : AsyncWorker(callback){}
        ~CASPARCycler() {};

        void Execute()
        {
            cycles = 0;
            //openFiles();
            clearactivedata();
            recordflag = true;
            setupADC();
            calibrategain();
            runflag = true;
            cycle();
        }

        void HandleOKCallback()
        {
            Nan::HandleScope scope;
        }
    };



    void Method(const FunctionCallbackInfo<Value>& args)
    {
        Isolate* isolate = args.GetIsolate();
        args.GetReturnValue().Set(String::NewFromUtf8(isolate, "world").ToLocalChecked());
    }

    void initializePCR(const FunctionCallbackInfo<Value>& args)
    {
        setupPi();
        setupQiagen();
        openFiles();
    }

    void Iteration(const FunctionCallbackInfo<Value>& args)
    {
        Isolate* isolate = args.GetIsolate();
        for(int i = 0; i < 5; i++)
        {
            iterate++;
        }
        Local<Number> num = Number::New(isolate, iterate);
        args.GetReturnValue().Set(num);
    }

    void readoutData(const FunctionCallbackInfo<Value>& args)
    {
        Isolate* isolate = args.GetIsolate();
        double jsoutput;
        if(runflag == true && recordflag == true)
        {
            jsoutput = y[y.size()-1];
        }
        else
        {
            jsoutput = 0;
        }
        Local<Number> num = Number::New(isolate, jsoutput);
        args.GetReturnValue().Set(num);
    }

    void readoutTemp(const FunctionCallbackInfo<Value>& args)
    {
        Isolate* isolate = args.GetIsolate();
        double jsoutput;
        if(runflag == true && recordflag == true)
        {
            float voltage = (TEMP.getreading()*4.096)/32767.0;
            float temperature = (voltage - 1.25)/0.005;
            jsoutput = temperature;
        }
        else
        {
            jsoutput = 0;
        }
        Local<Number> num = Number::New(isolate, jsoutput);
        args.GetReturnValue().Set(num);
    }

    void readoutPCR(const FunctionCallbackInfo<Value>& args)
    {
        Isolate* isolate = args.GetIsolate();
        v8::Local<v8::Array> pcrhandoff;
        if(pcrReady == true)
        {
            pcrhandoff = New<v8::Array>(pcrValues.size());
            for(int i = 0; i < pcrValues.size(); i++)
            {
                Nan::Set(pcrhandoff, i, New<v8::Number>(pcrValues[i]));
                //pcrhandoff->Set(i, Number::New(isolate,pcrValues[i]));
            }
            pcrValues.clear();
            pcrReady = false;
        }
        else
        {
            pcrhandoff = New<v8::Array>(pcrValues.size());
            for(int i = 0; i < pcrValues.size(); i++)
            {
                Nan::Set(pcrhandoff, i, New<v8::Number>(-1));
                //pcrhandoff->Set(i, Number::New(isolate,pcrValues[i]));
            }
        }
        args.GetReturnValue().Set(pcrhandoff);
    }



    void startRun(const FunctionCallbackInfo<Value>& args)
    {
        
    }

    void stopRun(const FunctionCallbackInfo<Value>& args)
    {
        runflag = false;
        recordflag = false;
        //D2.resetI2C();
        //closeFiles();
        sens1.LED_off(1);
        sens1.LED_off(2);
        sens2.LED_off(1);
        sens2.LED_off(2);
    }

    void reopenFiles(const FunctionCallbackInfo<Value>& args)
    {
        Isolate* isolate = args.GetIsolate();
        String::Utf8Value str(isolate, args[0]);

        string newfilename(*str);
        string ts = timestamp();

        coeffstorage = "./data/" + newfilename+ "coeff" + ts + ".txt";
        pcrstorage = "./data/" + newfilename + "pcr" + ts + ".txt";
        rawstorage = "./data/" + newfilename + "binaryoutput" + ts + ".bin";
        closeFiles();
        openFiles();
    }

    void checkInit(const FunctionCallbackInfo<Value>& args)
    {
        //This function is a placeholder for later.
    }

    NAN_METHOD(Ignition) {
        Callback *callback = new Callback(info[0].As<Function>());
        AsyncQueueWorker(new CASPARCycler(callback));
    }
    

    void Initialize(Local<Object> exports)
    {
        Nan::Set(exports, New<String>("start").ToLocalChecked(), GetFunction(New<FunctionTemplate>(Ignition)).ToLocalChecked());
        NODE_SET_METHOD(exports, "hello", Method);
        NODE_SET_METHOD(exports, "sum_up", Iteration);
        NODE_SET_METHOD(exports, "initializePCR", initializePCR);
        //NODE_SET_METHOD(exports, "start", startRun);
        NODE_SET_METHOD(exports,"stop", stopRun);
        NODE_SET_METHOD(exports, "readdata", readoutData);
        NODE_SET_METHOD(exports, "readtemp", readoutTemp);
        NODE_SET_METHOD(exports, "readPCR", readoutPCR);
        NODE_SET_METHOD(exports, "changefiles", reopenFiles);
    }

    NODE_MODULE(casparengine, Initialize)

}