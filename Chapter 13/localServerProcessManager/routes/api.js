"use strict";

var express = require('express');
var request = require( 'request' );
var childProcess = require('child_process');
var uuid = require('node-uuid');
var router = express.Router();

var gProcesses = {};
var gProcessCount = 0;
var gProcessPath = "/Users/josh/Documents/Multiplayer Game Programming/TestProcess/TestProcess/build/Debug/TestProcess";
var gMaxProcessCount = 4;

var gMaxStartingHeartbeatAge = 20;
var gMaxRunningHeartbeatAge = 10;
var gHeartbeatCheckPeriodMS = 5000;

var gListenPort = 3000;

var eMachineState =
{
	empty: 'empty',
	partial: 'partial',
	full: 'full',
	shuttingDown: 'shuttingDown'
};
var gMachineState = eMachineState.empty;

var gSequenceIndex = 0;

var gVMUUID = process.env.VMUUID;
var gVMMURL = process.env.VMMURL;
var gHeartbeatSendPeriodMS = 20000;
var gSendHeartbeatInterval;

function getUTCSecondsSince1970()
{
	return Math.floor( ( new Date() ).valueOf() / 1000 );
}

function makeReturnableProcess( process )
{
	var time = getUTCSecondsSince1970();
	return { 
		processUUID: process.processUUID, 
		params: process.params, 
		state: process.state, 
		heartbeatAge: time - process.lastHeartbeat 
	};
}

//curl -X GET http://127.0.0.1:3000/api/processes
router.get('/processes/', function(req, res) 
{
	//send all processes currently running
	var toRet = [];
	var processUUID;
	for( processUUID in gProcesses )
	{
		toRet.push( makeReturnableProcess( gProcesses[ processUUID ] ) );
	}
	res.send( toRet );
});

router.get('/processes/:processUUID', function( req, res )
{
	var processUUID = req.params.processUUID;
	var process = gProcesses[ processUUID ];
	if( process )
	{
		res.send( makeReturnableProcess( process ) );
	}
	else
	{
		res.sendStatus( 404 );
	}
});


//tell our vmm how we're doing
function sendHeartbeat()
{
	var options = 
	{
		url: gVMMURL + "/api/vms/" + gVMUUID + "/heartbeat",
		method: 'POST',
		json: { machineState: gMachineState, sequenceIndex: ++gSequenceIndex }
	};

	request( options, function( err ) 
	{
		if( err )
		{
			console.log( "Error sending heartbeat: " + err );
		}
	} );
}


//curl -H "Content-Type: application/json" -X POST -d '{"params":{"maxPlayers":4}}' http://127.0.0.1:3000/api/processes
//create a new process
router.post('/processes/', function( req, res )
{
	if( gMachineState === eMachineState.full )
	{
		res.send(
		{ 
			msg: 'Already Full', 
			machineState: gMachineState, 
			sequenceIndex: ++gSequenceIndex
		} );
	}
	else if( gMachineState === eMachineState.shuttingDown )
	{
		res.send(
		{ 
			msg: 'Already Shutting Down', 
			machineState: gMachineState, 
			sequenceIndex: ++gSequenceIndex
		} );		
	}
	else
	{
		var processUUID = uuid.v1();
		var params = req.body.params;
		var child = childProcess.spawn( gProcessPath, 
		[ 
			'--processUUID', processUUID, 
			'--lspmURL', "http://127.0.0.1:" + gListenPort,
			'--json', JSON.stringify( params ) 
		] );

		gProcesses[ processUUID ] = 
		{ 
			child: child,
			params: params,
			state: 'starting',
			lastHeartbeat: getUTCSecondsSince1970()
		};
		
		++gProcessCount;
		gMachineState = gProcessCount === gMaxProcessCount ? eMachineState.full : eMachineState.partial;

		child.stdout.on('data', function (data) {
		  console.log('stdout: ' + data);
		});

		child.stderr.on('data', function (data) {
		  console.log('stderr: ' + data);
		});

		child.on('close', function (code, signal) 
		{
			console.log('child terminated by signal '+ signal);
			//we're you at max process count?
			var oldMachineState = gMachineState;
			--gProcessCount;
			gMachineState = gProcessCount > 0 ? eMachineState.partial : eMachineState.empty;
			delete gProcesses[ processUUID ];
			if( oldMachineState !== gMachineState )
			{
				console.log( "Machine state changed to " + gMachineState );
				sendHeartbeat();
			}
		});

		res.send( 
		{ 
			msg: 'OK', 
			processUUID: processUUID, 
			machineState: gMachineState,
			sequenceIndex: ++gSequenceIndex
		} );
	}
} );


router.post('/processes/:processUUID/kill', function( req, res )
{
	var processUUID = req.params.processUUID;
	console.log( "Attempting to kill process: " + processUUID );
	var process = gProcesses[ processUUID ];
	if( process )
	{
		//killing will trigger the close event and remove from the process list
		process.child.kill();
		res.sendStatus(200);
	}
	else
	{
		res.sendStatus(404);
	}
});

router.post('/processes/:processUUID/heartbeat', function( req, res )
{
	var processUUID = req.params.processUUID;
	console.log( "heartbeat received for: " + processUUID );
	var process = gProcesses[ processUUID ];
	if( process )
	{
		process.lastHeartbeat = getUTCSecondsSince1970();
		process.state = "running";
		res.sendStatus(200);
	}
	else
	{
		res.sendStatus(404);
	}
});

router.post('/shutdown', function( req, res )
{
	//are you not already shutting down?
	console.log( "LSPM going to shut down.  current state is " + gMachineState );
	if( gMachineState === eMachineState.empty || gMachineState === eMachineState.shuttingDown )
	{
		//okay to shut down!
		gMachineState = eMachineState.shuttingDown;
		//kill the heartbeat interval
		if( gSendHeartbeatInterval )
		{
			console.log( "LSPM stopping sending heartbeats to vmm" );
			clearInterval( gSendHeartbeatInterval );
			gSendHeartbeatInterval = undefined;
		}
		console.log( "LSPM sending response that our state is now " + gMachineState );
		res.send(
		{
			msg: "OK",
			machineState: gMachineState
		});			
	}
	else
	{
		//not okay! we're still running at least a process! respond with correct state...
		res.send(
		{
			msg: "Not Empty",
			machineState: gMachineState
		});
	}
});

function checkHeartbeats()
{
	console.log( "LSPM checking for process heartbeats..." );
	//we probably don't want to remove things from this hash while iterating so...
	var processesToKill = [];
	var processUUID, process, heartbeatAge;
	var time = getUTCSecondsSince1970();
	for( processUUID in gProcesses )
	{
		process = gProcesses[ processUUID ];
		heartbeatAge = time - process.lastHeartbeat;
		if( heartbeatAge > gMaxStartingHeartbeatAge || 
			( heartbeatAge > gMaxRunningHeartbeatAge 
				&& process.state !== 'starting' ) )
		{
			//uhoh, no heartbeat in too long!
			console.log( "Process " + processUUID + " timeout!" );
			processesToKill.push( process.child );
		}
	}

	processesToKill.forEach( function( toKill )
	{
		toKill.kill();
	} );

}

//also start checking for heartbeats!
setInterval( checkHeartbeats, gHeartbeatCheckPeriodMS );

gSendHeartbeatInterval = setInterval( sendHeartbeat, gHeartbeatSendPeriodMS );

//overload fro setting some details- in a larger project, break this out
router.setVMMURL = function( url )
{
	gVMMURL = url;
};

router.setVMUUID = function( vmuuid )
{
	gVMUUID = vmuuid;
};

router.setListenPort = function( listenPort )
{
	gListenPort = listenPort;
 	console.log( JSON.stringify( 
	{ 
		"port": listenPort,
		"vmuuid": gVMUUID,
		"vmurl": gVMMURL
	} ) );
};

module.exports = router;
