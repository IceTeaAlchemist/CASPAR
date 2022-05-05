#include <node.h>
#include <unistd.h>
#include "caspar.h"

namespace demo 
{
    
    using v8::FunctionCallbackInfo;
    using v8::Isolate;
    using v8::Local;
    using v8::Object;
    using v8::String;
    using v8::Number;
    using v8::Value;
    int iterate = 0;

    void Method(const FunctionCallbackInfo<Value>& args)
    {
        Isolate* isolate = args.GetIsolate();
        args.GetReturnValue().Set(String::NewFromUtf8(isolate, "world").ToLocalChecked());
    }

    void startPCR(const FunctionCallbackInfo<Value>& args)
    {
        setupPi();
        setupQiagen();
        openFiles();
        setupADC();
        calibrategain();
        cycle();
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

    int citeration(int num)
    {
        for(int i = 0; i < 5; i++)
        {
            num++;
        }
        return num;
    }

    void Initialize(Local<Object> exports)
    {
        NODE_SET_METHOD(exports, "hello", Method);
        NODE_SET_METHOD(exports, "sum_up", Iteration);
        NODE_SET_METHOD(exports, "runPCR", startPCR);
        iterate = 5;
        iterate = citeration(iterate);
    }

    NODE_MODULE(addon, Initialize)

}