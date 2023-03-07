# Documentation

Contains the Standard Operation Procedures for the CASPAR PCR machine.<br>

# Design of the Code

There are three main parts to the code:<br>
* UI (user interface), UI/CASPAR-UI.html
* Javascript run by node, caspar.js
* Engine of c++ routines doing all the heavy work, src/*

## UI/User Interface

  This is the html page that the user is aimed at.  It provides the controls for the user.  It should do minimal programming, that is it can have helper functions for what it needs to do, but should not set timestamps, or anything other than some basics...like the total cycles, use reverse transcription, etc.  Has minimal editing mostly for the configs.txt file, a "meta" configuration that aims at the recipes file, in configs/recipes/XXXX.ini .

  This code contains HTML and in the \<script\> section helper code for its display.  It communicates to the Javascript using WebSocket.  This amounts to sending a JSON packet with an "id" and then pairs of "name" : "data" .<br>

  It receives data over the WebSocket (WS)<br>
```Javascript
ws.onmessage = function(event){
    var msg = JSON.parse(event.data);
    ...
```
followed by a giant switch case to handle the msg.id setting.

```Javascript
         case "connect": //when connected to server
            console.log(msg);
            ...
```
The HTML setup, style is very verbose.  Want to get to a more modular function, updating all of div "tab1c" say, etc.


## Main Javascript, caspar.js

The main javascript code which is invoked by
```shell
node caspar.js
```
It accesses the C++11 code as an "engine" that is build with 
```shell
node-gyp build     build only what has changed
node-gyp rebuils   rebuild all components
```
This code may need to do more than the HTML, but really want all the functions to be in the C++11 side.

## Engine, collection of C++11 code in src/* 

Collection of C++11 code that runs the hardware and takes data from the caspar.js (often originating in the UI).  The key here is that functions can be mapped using V8 for node (??), this looks like in casparapi.cpp (L374)
```C++
void Initialize(Local<Object> exports)
    {
        Nan::Set(exports, New<String>("start").ToLocalChecked(), GetFunction(New<FunctionTemplate>(Ignition)).ToLocalChecked());
        NODE_SET_METHOD(exports, "initializePCR", initializePCR);
        NODE_SET_METHOD(exports, "stop", stopRun);
        ...
        NODE_SET_METHOD(exports, "RECONon", RECONon);    // ditto
        NODE_SET_METHOD(exports, "RToff", RToff);
        NODE_SET_METHOD(exports, "RTon", RTon);
        NODE_SET_METHOD(exports, "putComments", writeComments);
        NODE_SET_METHOD(exports, "setRecipeFilename", setRecipeFilename);  // Set the last pulled up recipe filename.  
        // WEG, make these functions, putComments in caspar.js and writeComments in casparapi.cpp  or setup.cpp ??
    }
```
and those mappings are setup by casparapi.cpp (L395)<br>
```C++
    NODE_MODULE(casparengine, Initialize)  // The main deal here.
```





