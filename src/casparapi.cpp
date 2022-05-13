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
        setupADC();
        calibrategain();
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
        if(runflag == true)
        {
            jsoutput = y[y.size()-1];
        }
        else
        {
            jsoutput = -1;
        }
        Local<Number> num = Number::New(isolate, jsoutput);
        args.GetReturnValue().Set(num);
    }

    void readoutPCR(const FunctionCallbackInfo<Value>& args)
    {
        // This is another placeholder. readPCR needs to be updated.
    }



    void startRun(const FunctionCallbackInfo<Value>& args)
    {
        runflag = true;
        cycle();
    }

    void stopRun(const FunctionCallbackInfo<Value>& args)
    {
        runflag = false;
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
    }

    NODE_MODULE(casparengine, Initialize)

}