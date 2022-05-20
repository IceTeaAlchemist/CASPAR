const engine = require('./build/Release/casparengine');
const WebSocket = require('ws');
const event = require('events');
const { Server } = require('http');
const { clearInterval } = require('timers');


console.log("Welcome to CASPAR--C-based Analysis of Samples by PCR with Adaptive Routine.");
console.log("----------Runtime Status----------");

console.log("Server running.");
const wss = new WebSocket.Server({ port: 7071});

let DataIntervId;
let PCRIntervId;

var isRunning = false;

wss.on('connection', function connection(ws) {
    var connect = {
        id: "connect",
        status: "Server connected."
    };
    ws.send(JSON.stringify(connect));
    console.log("Connected to client.");
    ws.on('message', function message(data){
        var msg = JSON.parse(data);
        switch(msg.id){
            case "start/stop":
                if(isRunning === false)
                {
                    engine.start(function (){});
                    isRunning = true;
                    console.log('Ignition started.');
                    DataIntervId = setInterval(sendit, 300);
                    PCRIntervId = setInterval(sendPCR, 5000);
                }
                else
                {
                    clearInterval(DataIntervId);
                    clearInterval(PCRIntervId);
                    engine.stop();
                    isRunning = false;
                    console.log('Cycling shutdown.');
                }
                break;
            case "filename":
                console.log(msg.newname);
                if(isRunning === false)
                {
                    engine.changefiles(msg.newname);
                    ws.send(JSON.stringify({id: "filestatus", status: "File changed."}));
                }
                else
                {
                    ws.send(JSON.stringify({id: "filestatus", status: "Currently running PCR. Filename rejected."}));
                }
                break;
            case "shutdown": 
                if(isRunning === true)
                {
                    engine.stop();
                }
                isRunning = false;
                console.log("Server disconnected.");
                ws.send(JSON.stringify({id: "connect", status: "Server disconnected."}));
                process.exit();
        }
        console.log('received: %s', data);
        console.log(isRunning);
    });
})

function sendit()
{
    var datastruct = {
        id: "cycledata",
        fluor: engine.readdata(),
        temp: 0 // engine.readtemp()
    };
    wss.clients.forEach(function dataupdate(ws) {ws.send(JSON.stringify(datastruct));});
    
}

function sendPCR()
{
    var PCRinfo = engine.readPCR();
    if(PCRinfo[0] > 0)
    {
        console.log("Sending PCR.");
        var datastruct = {
            id: "PCRdata",
            fam: PCRinfo[0],
            cyfive: PCRinfo[1]
        };
        wss.clients.forEach(function pcrupdate(ws) {ws.send(JSON.stringify(datastruct));});
    }
    else
    {
        //do nothing
    }
}



engine.initializePCR();