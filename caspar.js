const engine = require('./build/Release/casparengine');
const WebSocket = require('ws');
const event = require('events');
const { Server } = require('http');

const wss = new WebSocket.Server({ port: 7071});

let DataIntervId;

var isRunning = false;

wss.on('connection', function connection(ws) {
    ws.on('message', function message(data){
        var msg = JSON.parse(data);
        switch(msg.id){
            case "start/stop":
                if(isRunning === false)
                {
                    engine.start(function (){});
                    isRunning = true;
                    console.log('Ignition started.');
                    DataIntervId = setInterval(function sendit(){ws.send(engine.readdata());}, 100);
                }
                else
                {
                    engine.stop();
                    isRunning = false;
                    console.log('Cycling shutdown.');
                    clearInterval(DataIntervId);
                }
                break;
            case "shutdown": 
            engine.stop();
            isRunning = false;
            console.log("Server disconnected.");
            process.exit();
        }
        console.log('received: %s', data);
        console.log(isRunning);
    });
    ws.send('something');
})



engine.initializePCR();

//engine.start();

//setInterval(readout, 500);

/*function readout()
{
    wss.send(JSON.stringify(engine.readdata()));
}*/

console.log(engine.hello());

console.log(engine.readdata())

console.log(engine.sum_up())