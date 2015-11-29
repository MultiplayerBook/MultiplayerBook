"use strict";

var express = require('express');
var request = require( 'request' );
var uuid = require('node-uuid');
var async = require('async');
var childProcess = require('child_process');
var path = require( 'path' );
var router = express.Router();

var gVMs = {};
var gAvailableVMs = {};

var eMachineState =
{
	empty: "empty",
	partial: "partial",
	full: "full",
	pending: "pending",
	shuttingDown: "shuttingDown",
	recentLaunchUnknown: "recentLaunchUnknown"
};

var gMaxRunningHeartbeatAge = 20;
var gHeartbeatCheckPeriodMS = 30000;



function getUTCSecondsSince1970()
{
	return Math.floor( ( new Date() ).valueOf() / 1000 );
}

function getFirstAvailableVM()
{
	for( var vmuuid in gAvailableVMs )
	{
		return gAvailableVMs[ vmuuid ];
	}
	return null;
}

function updateVMState( vm, newState )
{
	var oldState = vm.machineState;

	if( oldState !== newState )
	{
		if( oldState === eMachineState.partial )
		{
			delete gAvailableVMs[ vm.uuid ];
		}

		vm.machineState = newState;

		if( newState === eMachineState.partial )
		{
			gAvailableVMs[ vm.uuid ] = vm;
		}
	}
}

var gSimulatedCloudProvider = {};

function askCloudProviderForVM( vmUUID, callback )
{
	//for testing, we're going to locally spin up an lspm
	//we gotta get to the process, and pass ( assuming we're on port 3000 )
	// --vmuuid vmUUID --vmurl http://127.0.0.1:3000
	//and the lspm will tell us what port we're on so we can pass that back to the listener.
	//in your own program, you would ask cloud provider to do this and send back relevant information

	//also keep the children mapper privately so we can kill them when we need to
	var cloudProviderId = uuid.v1();

	var scriptPath = path.join( __dirname, "..", "..", "localServerProcessManager", "bin", "www");

	//console.log( "attempting to boot " + scriptPath );

	var child = childProcess.spawn( "node", [ scriptPath, "--vmuuid", vmUUID, "--vmurl", "http://127.0.0.1:3000" ] );

	gSimulatedCloudProvider[ cloudProviderId ] = child;

	var isInitted = false;
	child.stdout.on('data', function (data) 
	{
	  console.log("stdout of " + vmUUID + ": " + data);
	  if( !isInitted )
	  {
	  	var configData = JSON.parse( data );
	  	isInitted = true;
		var toRet =
		{
			url: "http://127.0.0.1:" + configData.port,
			id: cloudProviderId
		};
	  	callback( null, toRet );
	  }
	});

	child.on( 'error', function( err )
	{
		console.log( "Error for child " + cloudProviderId +": " + err );
		if( !isInitted )
		{
			delete gSimulatedCloudProvider[ cloudProviderId ] ;
			callback( err );
		}
	});

	child.stderr.on('data', function (data) {
	  console.log("stderr of " + vmUUID + ": " + data);
	});
}

function askCloudProviderToKillVM( cloudProviderId, callback )
{
	var child = gSimulatedCloudProvider[ cloudProviderId ];
	if( child )
	{
		//console.log( "askCloudProviderToKillVM going to kill child");
		child.kill();
		delete gSimulatedCloudProvider[ cloudProviderId ];
		callback( null );
	}
	else
	{
		//console.log( "askCloudProviderToKillVM can't find child");
		callback( new Error( "child for id " + cloudProviderId + " not found" ) );
	}
}


//curl -H "Content-Type: application/json" -X POST -d '{"params":{"maxPlayers":4}}' http://127.0.0.1:3000/api/processes
router.post('/processes/', function(req, res)
{
	//somebody wants a new game server
	var params = req.body.params;
	//do we have aany available servers?
	var vm = getFirstAvailableVM();

	//begin async activities...
	async.series(
	[
		function( callback )
		{
			//if no vm, we have to spin one up...
			if( !vm )
			{
				var vmUUID = uuid.v1();
				askCloudProviderForVM( vmUUID, function( err, cloudProviderResponse )
				{
					if( err )
					{
						callback( err );
					}
					else
					{
						vm =
						{
							lastSequenceIndex: 0,
							machineState: eMachineState.pending,
							uuid: vmUUID,
							url: cloudProviderResponse.url,
							cloudProviderId: cloudProviderResponse.id,
							lastHeartbeat: getUTCSecondsSince1970() 
						};
						gVMs[ vm.uuid ] = vm;
						callback( null );
					}
				} );
			}
			else
			{
				updateVMState( vm, eMachineState.pending );
				callback( null );
			}
		},

		//now we have a valid vm in the pending state so nobody else can touch it
		function( callback )
		{
			//now let's ask it to start a serverprocess
			var options = 
			{
				url: vm.url + "/api/processes/",
				method: 'POST',
				json: { params: params }
			};

			request( options, function( error, response, body ) 
			{
			 	if( !error && response.statusCode === 200 ) 
			 	{
			 		if( body.sequenceIndex > vm.lastSequenceIndex )
			 		{			 		
			 			vm.lastSequenceIndex = body.sequenceIndex;
			 			if( body.msg === 'OK' )
				 		{
					 		updateVMState( vm, body.machineState );
					 		callback( null );
				 		}
				 		else
				 		{
				 			//something weird- we were probably already full
					 		callback( body.msg );
						}
				 	}
				 	else
				 	{
				 		callback( "sequence response out of order, so can't trust state" );
				 	}
			  	}
			  	else
			  	{
				 	callback( "error from lspm: " + error );
			  	}
			} );
		}
	],
	function( err )
	{
		if( err )
		{
			//if we do have a vm, make sure it's not stuck in the pending state
			if( vm )
			{
				updateVMState( vm, eMachineState.recentLaunchUnknown );
			}
			res.send( { msg: "Error starting server process: " + err } );
		}
		else
		{
			res.send( { msg: 'OK' } );
		}
	});
});

function shutdownVM( vm )
{
	//console.log( "trying to shut down vm " + vm.uuid );
	//alright, remember we're shutting down...
	updateVMState( vm, eMachineState.shuttingDown );
	askCloudProviderToKillVM( vm.cloudProviderId, function( err )
	{
		if( err )
		{
			console.log( "Error closing vm " + vm.uuid );
			//we'll try again when heartbeat is missed
		}
		else
		{
			//it worked! remove from everywhere...
			delete gVMs[ vm.uuid ];
			delete gAvailableVMs[ vm.uuid ];
		}
	} );
}

//end point to list all vms for debugging
router.get( '/vms/', function( req, res )
{
	var vms = [];
	var vmuuid;
	for( vmuuid in gVMs )
	{
		vms.push( gVMs[ vmuuid ] );
	}

	res.send( JSON.stringify( vms, null, '  ' ) );
} );

//end point for machine shutting down a process and changing state...
router.post('/vms/:vmUUID/heartbeat', function(req, res)
{
	var vmUUID = req.params.vmUUID;
	var sequenceIndex = req.body.sequenceIndex;
	var newState = req.body.machineState;

	var vm = gVMs[ vmUUID ];
	if( vm )
	{
		var oldState = vm.machineState;
		//if the vm is in the pending or shutting down state, a heartbeat does not affect it
		//or if the heartbeat is old, ignore it
		//potentially verify incoming ip address to make sure it matches expected ip address of lspm
		res.sendStatus( 200 );

		if( oldState !== eMachineState.pending && 
			oldState !== eMachineState.shuttingDown && 
			sequenceIndex > vm.lastSequenceIndex )
		{
	
			vm.lastHeartbeat = getUTCSecondsSince1970();
			vm.lastSequenceIndex = sequenceIndex;
			if( newState === eMachineState.empty )
			{
				//request shut down of lspm
				var options = 
				{
					url: vm.url + "/api/shutdown",
					method: 'POST',
				};

				request( options, function( error, response, body ) 
				{
					body = JSON.parse( body );
					//console.log( "VMM got response from trying to shutdown " + vm.uuid + ": " + body );
					//then request shut down of vm
				 	if( !error && response.statusCode === 200 ) 
				 	{
						//console.log( "updating state of vm to " + body.machineState );
			 			updateVMState( vm, body.machineState );
			 			//does lspm still think it's okay to shut down?
				 		if( body.machineState === eMachineState.shuttingDown )
				 		{
				 			shutdownVM( vm );
				 		}
				  	}
				} );
			}
			else
			{
				updateVMState( vm, newState );
			}
		}
	}
	else
	{
		res.sendStatus( 404 );
	}
} );

function checkHeartbeats()
{
	console.log( "VMM checking for LSPM heartbeats..." );
	//we probably don't want to remove things from this hash while iterating so...
	var vmsToKill = [];
	var vmUUID, vm, heartbeatAge;
	var time = getUTCSecondsSince1970();
	for( vmUUID in gVMs )
	{
		vm = gVMs[ vmUUID ];
		heartbeatAge = time - vm.lastHeartbeat;
		if( heartbeatAge > gMaxRunningHeartbeatAge && 
			vm.machineState !== eMachineState.pending )
		{
			//uhoh, no heartbeat in too long!
			console.log( "vm " + vmUUID + " exceeded heartbeat timeout!" );
			vmsToKill.push( vm );
		}
	}

	vmsToKill.forEach( shutdownVM );
}

//also start checking for heartbeats!
setInterval( checkHeartbeats, gHeartbeatCheckPeriodMS );

//heartbeat check ( important to send state, so that we don't leak vms )



module.exports = router;
