/*
caspar.js -

*/

// Import the C++ addon as well as websocket and mail handler libraries.
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


// Initialize the websocket server. Port doesn't matter as long as it matches on the website, 
// 7071 is usually unoccupied.
serverforip.listen(7071,hostname, () => {
    console.log(`Server running at http://${hostname}:7071`)
});
const wss = new WebSocket.Server({ server: serverforip });

// From main UI about L278.  Needed?? weg
// const configEnum = {"opName":0, "email":1, "projName":2, "recName":3, "FAM":4, "CY5":5, "HEX":6, "RTcheckbox":7, "totCycles":8};
// Object.freeze(configEnum);

// These are for handling the SetInterval functions.
let DataIntervId;
let PCRIntervId;

// Tracks whether or not the PCR is currently running a sample and should be updated accordingly.
var isRunning = false;

//Kunal: New variables to store data from client
var savedStartDate = "";
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
var lastRequest = "";
var lastRecipeRequest = "";
var savedFAM = "";
var savedCY5 = "";
var savedHEX = "";
var savedRT = "";
var savedCycles = 0;

// Vectors for storing PCR data server side. 
var timestamprecord = [];
var FAMrecord = [];
var HEXrecord = [];
var cy5record = [];


// WebSocket initialization upon client connection and declaration of events for handling WS messages from the client.
wss.on('connection', function connection(ws) {

    //Kunal (10): Reading configuration file and then sending that data to the client
    var configstrings = [];
    var recipefiles = [];

    let currentdata = fs.readFileSync('./configs/configs.txt', 'utf8');
    let array = currentdata.toString().split("\n");

    for (var i = 0; i<array.length; i++) {
        if (array[i].includes("cname: ")){
            configstrings.push((array[i].substring(array[i].lastIndexOf("cname: ") + 7)).trim()); 
            //Sends all names for a single cname in one string with a seperator
        }
    }
    // fs.readdir() is the Async version and has a callback, the Sync version returns a list of files.
    recipefiles = fs.readdirSync('./configs/recipes/', 'utf8');
    // Put default.ini first.  https://stackoverflow.com/questions/23921683/javascript-move-an-item-of-an-array-to-the-front 
    recipefiles = recipefiles.filter( function(item){ return item !== "default.ini"; } );  // Return all the elements not equal to default.ini .
    // recipefiles = recipefiles.filter( item => item !== 'default.ini' );
    recipefiles.unshift( 'default.ini' );
    console.log('recipes recipefiles:');
    console.log(recipefiles);

    // Send a message to the client to confirm connection.
    var connect = {
        id: "connect",
        status: "Server connected.",
        //Kunal (12): information to send to client
        running: isRunning,
        startdate: savedStartDate,
        starttime: savedStartTime,
        projectname: savedProjName,
        operatorname: savedOperator,
        experimentname: savedExperimentName,
        configname: savedConfig,
        configs: configstrings,
        recipenames: recipefiles,
        comments: savedComments,
        filename: savedFileName,
        defproj: savedDefProj,
        defop: savedDefOp,
        defem: savedDefEm,
        fam: savedFAM,
        cy5: savedCY5,
        hex: savedHEX,
        rt: savedRT,
        FAMdata: FAMrecord,
        HEXdata: HEXrecord,
        cy5data: cy5record,
        totcycles: savedCycles
    };
    ws.send(JSON.stringify(connect));
    
    // Log that a client connected to the console.
    console.log("Connected to client.");

    // Set up message handling.
    ws.on('message', function message(data){
        var msg = JSON.parse(data);
        // Switch off the ID blank of the message.
        switch(msg.id){
            //Kunal (20): new case "config request" that gets a request to load a configuration and then reads the file to load it
            case "configrequest":
                console.log(msg);
                if(lastRequest != msg.config){ //ensures we dont load it if we just loaded it
                    // var requestdata = "";
                    var requestdata = [];
                    let alldata = fs.readFileSync('./configs/configs.txt', 'utf8'); //read file
                    var dataarray = alldata.toString().split("\n");
                    for (i = 0; i<dataarray.length; i++) { // Loop over the lines in the long configs.txt file.
                        console.log(dataarray[i]);
                        if (dataarray[i].includes("cname: " + msg.config)){  // Find the line for the config you asked for.
                            let j = i+1;
                            while(j < dataarray.length)
                            {
                                if(dataarray[j].includes("cname: ") == false)  // Have not hit the next config list.
                                {
                                    console.log(dataarray[j]);
                                    requestdata.push( dataarray[j].substring( dataarray[j].indexOf(":")+1 ).trim() );  
                                    // .trim() seems not to work.
                                } 
                                else  // Hit the next cname: xxx line.
                                {
                                    break;
                                }
                                j++;
                            }
 //                           requestdata = dataarray[i+1] + ":;:;:" + dataarray[i+2] + ":;:;:" + dataarray[i+3]; //sends all necessary data in one string with a separator
                        }
                    }

                    console.log(requestdata);

                    var configloader = {
                        id: "loadconfig",
                        data: requestdata,
                    };
                    console.log(configloader);
                    ws.send(JSON.stringify(configloader)); // data sent back to client
                }
               // else{}
                lastRequest = msg.config;
                break;
//////////////////////////////////////////////
//  Below patterned on the above configrequest, fills the dropdown with the recipe file names.
//
            case "reciperequest":
                console.log(msg);
                if(lastRecipeRequest != msg.recipe){ // Ensures we dont load it if we just loaded it.
                    var recipefiles = [];
                    // Read the *.ini files that are the recipes.  Use the Sync read.
                    recipefiles = fs.readdirSync('./configs/recipes/', 'utf8');
                } else {  // Do nothing, same file selected.
                    break;
                } 
                console.log("caspar.js: case reciperequest: recipefiles:");
                console.log(recipefiles);

                var recipeloader = {
                    id: "loadrecipes",
                    data: recipefiles,
                };
                console.log(recipeloader);
                ws.send(JSON.stringify(recipeloader)); // data sent back to client

                lastRecipeRequest = msg.recipe;
                break;
/////////////////////////////////////////////////                    
            case "start/stop":
		        savedDefEm = msg.email;
                if(savedRT === "true")
                {
                    engine.RTon();
                }
                else
                {
                    engine.RToff();
                }
                if(isRunning === false)
                {
                    console.log(msg);
                    // Start the PCR cycling if it's not.
                    engine.setCutoff(parseInt(msg.cutoffcycles));
                    savedCycles = parseInt(msg.cutoffcycles);
                    engine.start(function (err, errorvalue)
                    {
                        console.log(errorvalue);
                        clearInterval(DataIntervId);
                        clearInterval(PCRIntervId);
                        sendPCR();
                        engine.stop();
                        isRunning = false;
                        ws.send(JSON.stringify({id: "isRunning", value: isRunning}));
                        console.log('Cycling shutdown.');
                        if(errorvalue[0] > 0)
                        {
                            var errorreport = 
                            {
                                id: "errorreport",
                                value: errorvalue[0]
                            }
                            ws.send(JSON.stringify(errorreport));
                        }
                        ws.send(JSON.stringify({id: "runcomplete"}));
                        var mailOptions = {
                            from: 'caspar@casparvu.com',
                            to: savedDefEm,
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
                    });
                    isRunning = true;
                    ws.send(JSON.stringify({id: "isRunning", value: isRunning}));
                    console.log('Ignition started.');
                    // Start the periodic cycling data send and PCR data check.
                    FAMrecord = [];
                    HEXrecord = [];
                    timestamprecord = [];
                    cy5record = [];
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
                    sendPCR();
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
            case "startSave":
                savedStartDate = msg.startDate;
                savedStartTime = msg.startTime;
                savedProjName = msg.projectName;
                savedOperator = msg.operator;
                savedExperimentName = msg.experimentName;
                savedConfig = msg.config;
                savedRecipe = msg.recipefiles;
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
                break;
            case "filename":
                // Change the filename.
                if(isRunning === false)
                {
                    // If the system's off, go ahead and change the filename.
                    // The below are undefined here!!
                    // console.log("caspar.js L304: msg.newname, msg.projectName, msg.experimentName  " + 
                    // msg.newname +" " + msg.projectName + " " + msg.experimentName);
                    // engine.changefiles(msg.newname, msg.projectName, msg.experimentName);
                    // Below might work if the above startSave case went first.
                    console.log("caspar.js L309: savedFileName, savedProjName, savedExperimentName  " + 
                    msg.newname +" " + msg.projname + " " + msg.exptname); // These are all empty strings.
                    engine.changefiles(msg.newname, msg.projname, msg.exptname);
                    ws.send(JSON.stringify({id: "filestatus", status: "File changed."}));
                }
                else
                {
                    // If it's running, reject the request with a message.
                    ws.send(JSON.stringify({id: "filestatus", status: "Currently running PCR. Filename rejected."}));
                }
                break;
            case "savefinish":
                // Kunal (4): new case "save finish" that saves comments and finish time when the run 
                // ends.  Needs to get sent WEG.
                savedComments = msg.comments;  // Setting globals, should we?
                //savedStartDate = msg.startdate;
                //savedStartTime = msg.starttime;
                savedFinishTime = msg.finishtime;
                // Put the comments to notes_<ts>.txt here.
                // Add project name, operator name, experiment name (or none).
                console.log("In savefinish case:");
                console.log(savedComments, savedStartDate, savedStartTime, savedFinishTime, 
                    savedProjName, savedOperator, savedExperimentName);

                engine.putComments(savedComments, savedStartDate, savedStartTime, savedFinishTime, 
                    savedProjName, savedOperator, savedExperimentName);
                break;
            case "saveconfiguration":
                // Kunal (9): new case "saveconfiguration" saves the new configuration the client 
                // wants in the configs document.
                let newConfig = "cname: " + msg.name.trim() + "\n" + "oname: " + msg.defaultoperator + "\n" + 
                  "ename: " + msg.defaultemail + "\n" + "pname: " + msg.defaultproject + "\n" + "rname: " + msg.defaultrecipe + "\n" +
                  "fam: " + msg.fam + "\n" + "cy5: " + msg.cy5 + "\n" + "hex: " + msg.hex + "\n" + 
                  "rtval:" + msg.rt + "\n" + "cycles:" + msg.totalcycles + "\n \n";
                fs.appendFile('./configs/configs.txt', newConfig, (err) => { //adds to the file
                    if (err) {
                        console.error(err);
                        return;
                    }
                });
                break;
            case "sendemail":
                // Retrieve filenames from engine, and send them to an email address.
                var filenames = engine.getfiles();  // Matched to readoutfiles() in casparapi.cpp .
                var mailOptions = {
                    from: 'caspar@casparvu.com',
                    to: 'kick767@gmail.com',
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
                        {
                            filename: 'tempers.csv',
                            path: filenames[5] // stream this file
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
            case "downloadDataRequest":
                var filenames = engine.getfiles();
                var msg = {
                    id: "downloadresponse",
                    notefile: filenames[4],
                    pcrfile: filenames[2],
                    tempsfile: filenames[5],
                };
                ws.send(JSON.stringify(msg));
                console.log(msg);
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
                console.log("Delaying to allow threads to wrap up. Closing in five seconds.");
                // Hard shutdown. There should be a better way.
                setTimeout(killserver, 5000);
        }
        // Log request received.
        console.log('received: %s', data);
    });
}// end function connection, the callback function.
)// end ws.on

function killserver()
{
    process.exit();// Do we need a more robust exit, a 1 or 2 here?
}

function sendit()
{
    // Create a struct of fluoresence and deriv data and send them to the client(s).
    var cyclingdata = engine.readdata();
    var datastruct = {
        id: "cycledata",
        fluor: cyclingdata[0],
        deriv: cyclingdata[1]
    };
    wss.clients.forEach( function dataupdate(ws) {ws.send(JSON.stringify(datastruct));} );
    
}
//////////////////////
//
//////////////////////
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


