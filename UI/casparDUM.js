// Import the C++ addon, file reading, as well as websocket and mail handler libraries.
const engine = require('./build/Release/casparengine');
const WebSocket = require('ws');
const { clearInterval } = require('timers');
var nodemailer = require('nodemailer');
const fs = require('fs');
const httpserver = require('http');

// Initialize the PCR engine and print some brief informational messages to the console.
engine.initializePCR();
console.log("Welcome to CASPAR--C-based Analysis of Samples by PCR with Adaptive Routine.");
console.log("----------Runtime Status----------");
console.log("Server running.");

const hostname = '192.168.0.10';
var serverforip = httpserver.createServer();


// Initialize the websocket server. Port doesn't matter as long as it matches on the website, 7071 is usually unoccupied.
serverforip.listen(7071,hostname, () => {
    console.log(`Server running at http://${hostname}:7071`)
});
const wss = new WebSocket.Server({ server: serverforip });

// These are for handling the SetInterval functions.
let DataIntervId;
let PCRIntervId;

// Tracks whether or not the PCR is currently running a sample and should be updated accordingly.
var isRunning = false;

//New variables to store data from client
var savedStartTime = "";
var savedProjName = "";
var savedOperator = "";
var savedExperimentName = "";
var savedConfig = "";
var savedComments = "";
var savedFileName = "";
var savedFinishTime = "";
var savedDefProj = "";
var savedDefOp = "";
var savedDefEm = "";
var savedFAM = "";
var savedCY5 = "";
var savedHEX = "";
var savedRT = "";
var savedTotCycles = "";
var saveddeftotcycles = "";
var saveddefrt = "";

// Vectors for storing PCR data server side.
var timestamprecord = [];
var FAMrecord = [];
var HEXrecord = [];
var cy5record = [];

//Every Time the server is launched. A new configuration "none" is made that takes up the first X lines. It isnt added if it already exists
let alldata = fs.readFileSync('./configurations/configs.txt', 'utf8');
let allarray = alldata.toString().split("\n");
var newarray;
var add = true;
for (var j = 0; j<allarray.length; j++) {
    if (allarray[j].trim() == "cname: None"){
        add = false;
    }
}
let noneConfig = "cname: None" + "\n" + "oname: " + "\n" + "ename: " + "\n" + "pname: " + "\n" + "fam: " + "\n" + "cy5: " + "\n" + "hex: " + "\n" + "rtval: OFF" + "\n" + "totcycles:" + "\n \n";
if(add)
{
    var fd = fs.openSync('./configurations/configs.txt', 'w+');
    var buffer = Buffer.from(noneConfig);

    fs.writeSync(fd, buffer, 0, buffer.length, 0); //write new data
    fs.appendFile('./configurations/configs.txt', alldata, (err) => { //adds to the file
        if (err) {
            console.error(err);
            return;
        }
    });
    fs.close(fd);
}

// WebSocket initialization upon client connection and declaration of events for handling WS messages from the client.
wss.on('connection', function connection(ws) {

    //Reading configuration file and then sending that data to the client
    var configstrings = [];

    let currentdata = fs.readFileSync('./configurations/configs.txt', 'utf8');
    let array = currentdata.toString().split("\n");

    for (var i = 0; i<array.length; i++) {
        if (array[i].includes("cname: ")){
            configstrings.push((array[i].substring(array[i].lastIndexOf("cname: ") + 7)).trim()); //sends all names in one string with a seperator
        }
    }

    // Send a message to the client to confirm connection.
    var connect = {
        id: "connect",
        status: "Server connected.",
        //information to send to client
        running: isRunning,
        starttime: savedStartTime,
        projectname: savedProjName,
        operatorname: savedOperator,
        experimentname: savedExperimentName,
        configname: savedConfig,
        configs: configstrings,
        comments: savedComments,
        filename: savedFileName,
        defproj: savedDefProj,
        defop: savedDefOp,
        defem: savedDefEm,
        deftotcycles: saveddeftotcycles,
        defrt: saveddefrt,
        totcycles: savedTotCycles,
        fam: savedFAM,
        cy5: savedCY5,
        hex: savedHEX,
        rt: savedRT,
        FAMdata: FAMrecord,
        HEXdata: HEXrecord,
        cy5data: cy5record,
    };
    ws.send(JSON.stringify(connect));

    console.log(connect);

    // Log that a client connected to the console.
    console.log("Connected to client.");

    // Set up message handling.
    ws.on('message', function message(data){
        var msg = JSON.parse(data);
        // Switch off the ID blank of the message.
        switch(msg.id){
            //new case "config request" that gets a request to load a configuration and then reads the file to load it
            case "configrequest":
                console.log(msg);
                    var requestdata = [];
                    let alldata = fs.readFileSync('./configurations/configs.txt', 'utf8'); //read file
                    var dataarray = alldata.toString().split("\n");
                    for (i = 0; i<dataarray.length; i++) {
                        console.log(dataarray[i]);
                        if (dataarray[i].trim() == ("cname: " + msg.config.trim())){ // if a match is found
                            let j = i+1;
                            while(j < dataarray.length)
                            {
                                if(dataarray[j].includes("cname: ") == false) // keep reading until another configuration starts
                                {
                                    console.log(dataarray[j]);
                                    requestdata.push(dataarray[j].substring(dataarray[j].indexOf(":")+1).trim()); // push all data into array
                                }
                                else
                                {
                                    break;
                                }
                                j++;
                            }
                        }
                    }
                    //send the data
                    var configloader = {
                        id: "loadconfig",
                        data: requestdata,
                    };
                    console.log(configloader);
                    ws.send(JSON.stringify(configloader)); // data sent back to client
                lastRequest = msg.config;
                break;
            case "start/stop":
                var send = false; //Controls if an RT On message is sent
                //engine.changeCycle(savedTotCycles.trim());
                 if(savedRT === "true")
                {
                    engine.RTon();
                    send = true;
                }
                else
                {
                    engine.RToff();
                }
                if(isRunning === false)
                {
                    // Start the PCR cycling if it's not.
                    engine.start(function (err, errorvalue){
                        console.log(errorvalue);
                        clearInterval(DataIntervId);
                        clearInterval(PCRIntervId);
                        sendPCR();
                        engine.stop();
                        isRunning = false;
                        ws.send(JSON.stringify({id: "isRunning", value: isRunning}));
                        console.log('Cycling shutdown.');
                        if(errorvalue[0] > 0) // error valyes reported to UI if errors exist
                        {
                            var errorreport = {
                                id: "errorreport",
                                value: errorvalue[0]
                            }
                            ws.send(JSON.stringify(errorreport));
                        }
                        ws.send(JSON.stringify({id: "runcomplete"}));
                        if(msg.email != "false"){ // If an email exists in the UI when start stop is pressed. Have a sample finished message be sent to the persons email
                            var mailOptions = {
                                from: 'caspar@casparvu.com',
                                to: msg.email,
                                subject: 'CASPAR run finished.',
                                text: 'Sample finished.',
                            };
                            // Send the email and log info.
                            transporter.sendMail(mailOptions, function(error, info){
                                if (error) {
                                    console.log(error);
                                } else {
                                    console.log('Email sent: ' + info.response);
                                }

                            });
                        }
                    });
                    isRunning = true;
                    ws.send(JSON.stringify({id: "isRunning", value: isRunning})); // send if its running
                    if(send){
                        ws.send(JSON.stringify({id: "RTStatus", bool: "true"})); // if RT is running, send it
                        send = false;
                    }
                    console.log('Ignition started.');
                    // Start the periodic cycling data send and PCR data check.
                    DataIntervId = setInterval(sendit, 300);
                    PCRIntervId = setInterval(sendPCR, 5000);
                }
                else
                {
                    // Stop the cycling if it's running.
                    // Turn off the data sends.
                    clearInterval(DataIntervId);
                    clearInterval(PCRIntervId);
                    // Tell the engine to turn off.
                    engine.stop();
                    FAMrecord = [];
                    HEXrecord = [];
                    timestamprecord = [];
                    cy5record = [];
                    isRunning = false;
                    ws.send(JSON.stringify({id: "isRunning", value: isRunning}));
                    console.log('Cycling shutdown.');
                }
                break;
            case "ping":
                var pongmsg = {
                    id:"pong",
                };
                ws.send(JSON.stringify(pongmsg));
                break;
            case "startSave": // all the vars to save on the server in case the client disconnects.
                savedStartTime = msg.startTime;
                savedProjName = msg.projectName;
                savedOperator = msg.operator;
                savedExperimentName = msg.experimentName;
                savedConfig = msg.config;
                savedComments = msg.comments;
                savedFileName = msg.filename;
                savedFinishTime = "";
                savedDefProj = msg.defproj;
                savedDefOp = msg.defop;
                savedDefEm = msg.defem;
                savedFAM = msg.fam;
                savedCY5 = msg.cy5;
                savedHEX = msg.hex;
                savedRT = msg.rt;
                savedTotCycles = msg.totcycles;
                saveddeftotcycles = msg.deftotcycles;
                saveddefrt = msg.defrt;
                break;
            case "filename": //change filebane!
                // Change the filename.
                if(isRunning === false)
                {
                    // If the system's off, go ahead and change the filename.
                    engine.changefiles(msg.newname);
                    ws.send(JSON.stringify({id: "filestatus", status: "File changed."}));
                }
                else
                {
                    // If it's running, reject the request with a message.
                    ws.send(JSON.stringify({id: "filestatus", status: "Currently running PCR. Filename rejected."}));
                }
                break;
                //new case "save finish" that saves comments and finish time when the run ends
            case "savefinish":
                savedComments = msg.comments;
                savedFinishTime = msg.finishtime;
                break;
                //new case "saveconfiguration" saves the new configuration the client wants in the configs document
            case "saveconfiguration":
                let newConfig = "cname: " + msg.name.trim() + "\n" + "oname: " + msg.defaultoperator + "\n" + "ename: " + msg.defaultemail + "\n" + "pname: " + msg.defaultproject + "\n" + "fam: " + msg.fam + "\n" + "cy5: " + msg.cy5 + "\n" + "hex: " + msg.hex + "\n" + "rtval:" + msg.rt + "\n" + "totcycles:" + msg.totalcycles + "\n \n";
                fs.appendFile('./configurations/configs.txt', newConfig, (err) => { //adds to the file
                    if (err) {
                        console.error(err);
                        return;
                    }
                });
                break;
            case "deleteConfig": //deletes the requested configuration
                var allthedata = fs.readFileSync('./configurations/configs.txt', 'utf8');
                var allarray = allthedata.toString().split("\n");
                var newarray = [];
                var add = true;
                for (var j = 0; j<allarray.length; j++) {
                    if(add == false && allarray[j].includes("cname:")){
                        add = true;
                        console.log(allarray[j]);
                    }

                    if (allarray[j].trim() == ("cname: " + msg.name.trim())){ // once a match is found, stop adding until next 'cname:'
                        add = false;
                    }

                    if(add){
                        newarray.push(allarray[j]);
                    }
                }
                var newdata = newarray.join('\n') + "\n"

                var fd = fs.openSync('./configurations/configs.txt', 'w+');

                fs.writeFileSync('./configurations/configs.txt', newdata, (err) => { //adds to the new file
                    if (err) {
                        console.error(err);
                        return;
                    }
                });

                fs.close(fd);
                break;
            case "sendemail":
                // Retrieve filenames from engine, and send them to an email address.
                var filenames = engine.getfiles();
                var mailOptions = {
                    from: 'caspar@casparvu.com',
                    to: msg.address,
                    subject: 'CASPAR files',
                    text: 'See attached for requested data.',
                    attachments: [
                        {
                            filename: 'runtimelog.txt',
                            path: filenames[0] // stream this file
                        },
                        {
                            filename: 'cyclelog.txt',
                            path: filenames[1] // stream this file
                        },
                        {
                            filename: 'pcrlog.txt',
                            path: filenames[2] // stream this file
                        },
                        {
                            filename: 'rawdata.bin',
                            path: filenames[3] // stream this file
                        },
                    ]
                };
                // Send the email and log info.
                transporter.sendMail(mailOptions, function(error, info){
                    if (error) {
                        console.log(error);
                    } else {
                        console.log('Email sent: ' + info.response);
                    }
                });
                break;
            case "shutdown":
                // Turn off the engine if it's running.
                clearInterval(DataIntervId);
                clearInterval(PCRIntervId);
                isRunning = false;
                ws.send(JSON.stringify({id: "isRunning", value: isRunning}));
                wss.close();
                wss.clients.forEach(function closeconnection(servclient) {servclient.close();});
                serverforip.close();
                engine.stop();
                // Log that the server is turning off and tell the client as much.
                console.log("Server disconnected.");
                engine.boxfanoff();
                console.log("Delaying to allow threads to wrap up. Closing in ten seconds.");
                // Hard shutdown. There should be a better way.
                setTimeout(killserver,10000);
        }
        // Log request received.
        console.log('received: %s', data);
    });
})

function killserver()
{
    process.exit();
}

function sendit()
{
    // Create a struct of thermal and fluoresence data and send them to the client(s).
    var cyclingdata = engine.readdata();
    var datastruct = {
        id: "cycledata",
        fluor: cyclingdata[0],
        temp: cyclingdata[1]
    };
    wss.clients.forEach(function dataupdate(ws) {ws.send(JSON.stringify(datastruct));});

}

function sendPCR()
{
    // Check if PCR values are available.
    var PCRinfo = engine.readPCR();
    // If there aren't any, readPCR() returns negative 1, thus:
    if(PCRinfo[0] > 0)
    {
        // If there are, we log we're sending PCR values to the client, and do so.
        var cycletime = 60;
        if(timestamprecord.length > 0)
        {
            cycletime = (PCRinfo[1] - timestamprecord[timestamprecord.length-1])/1000.0;
        }
        console.log("Sending PCR.");
        var datastruct = {
            id: "PCRdata",
            cycle: PCRinfo[0],
            timestamp: PCRinfo[1],
            fam: PCRinfo[2],
            hex: PCRinfo[3],
            cy5: PCRinfo[4],
            secs: cycletime
        };
        console.log(datastruct);
        timestamprecord.push(PCRinfo[1]);
        FAMrecord.push(PCRinfo[2]);
        HEXrecord.push(PCRinfo[3]);
        cy5record.push(PCRinfo[4]);
        wss.clients.forEach(function pcrupdate(ws) {ws.send(JSON.stringify(datastruct));});
    }
    else
    {
        // Do nothing
    }
}

// Initialize the transporter for email use.
var transporter = nodemailer.createTransport({
  service: 'Outlook365',
  auth: {
    user: 'caspar@casparvu.com',
    pass: 'thefriendlyghostinthemachine'
  }
});
