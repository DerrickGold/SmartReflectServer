var MirrorAPI = function(addr) {

	var instance = this;
	this.doLogging = false;
	this.stdinSocket = null;
	this.socketBusy = false;


	/* User callback functions */
	this.apiResponseCallback = {
		list: 0,
		getstate: 0,
		disable: 0,
		enable: 0,
		setcss: 0,
		getcss: 0,
		mirrorsize: 0,
		savecss: 0,
		display: 0,

	};

	//set a callback function for a particular api response
	this.onAPIResponse = function(apiCall, cb) {
		instance.apiResponseCallback[apiCall] = function(status, plugin, payload) {
			cb(status, plugin, payload);
		};
	}

	//when a call back is received, this will process the returned data
	//into a format that can be better manipulated in the users callback
	this.transformApiResponsePayload = {

		list: function(payload) {
			//create an array of all plugins from the returned list
			var allPlugins = payload.split('\n').filter(function(o){
    			return o.length > 0;
    		});

			return allPlugins;
		},
		getstate: function(payload) { return payload; },
		disable: function(payload) { return payload; },
		enable: function(payload) { return payload; },
		setcss: function(payload) { return payload; },
		getcss: function(payload) {
			if (instance.doLogging)
				console.log(payload);
			var pluginCSS = {};

			var cssResults = payload.split('\n');
            cssResults.forEach(function(c) {
                var parts = c.split('=');
                pluginCSS[parts[0]] = parts[1];
            });

            return pluginCSS;
		},
		mirrorsize: function(payload) {
			var dimensions = payload.split('x');
			return {
				width: dimensions[0],
				height: dimensions[1]
			};
		},
		savecss: function(payload) { return payload; },
		display: function(payload) { return payload; },
		getopt: function(payload) {

			if (instance.doLogging)
				console.log(payload);

			var parts = payload.split(':', 2);
			return {
				setting: parts[0],
				value: parts[1]
			};
		},
		setopt: function(payload) { return payload; },
		getdir: function(payload) { return payload; },
		jscmd: function(payload) {

			var parts = payload.split(':');
			var res=[]
  			res.push(parts.slice(1).join(':'))

			return {
				fn: parts[0],
				value: res[0]
			};
		}
	}

	this.apiCall = {
        list: function(plugin, data) {
            instance.stdinSocket.send("list\n");
        },
        status: function(plugin, data) {
            instance.stdinSocket.send("getstate\n"+plugin);
        },
        enable: function(plugin, data) {
            instance.stdinSocket.send("enable\n"+plugin);
        },
        disable: function(plugin, data) {
            instance.stdinSocket.send("disable\n"+plugin);
        },
        setcss: function(plugin, cssObj) {

        	var cmdStr = "setcss\n" + plugin + "\n";
        	for (var key in cssObj) {

        		if (!cssObj.hasOwnProperty(key) || key == undefined ||
        			cssObj[key] == undefined)
        			continue;
	        	cmdStr = cmdStr.concat(key + '=' + cssObj[key] + ";");
        	}
        	//remove trailing ';'
            cmdStr = cmdStr.replace(/;+$/, "");
            if (instance.doLogging)
            	console.log(cmdStr);
            instance.stdinSocket.send(cmdStr);
        },
        getcss: function(plugin, cssvals) {

            var cmdStr = "getcss\n"+plugin+"\n";

            cssvals.forEach(function(css) {
                cmdStr = cmdStr.concat(css + ",");
            });

            //remove trailing comma
            cmdStr = cmdStr.replace(/,+$/, "");
            instance.stdinSocket.send(cmdStr);
        },
        dumpcss: function(plugin, data) {

            var cmdStr ="savecss\n"+plugin+"\n";
            instance.stdinSocket.send(cmdStr);
        },
        mirrorsize: function(plugin, data) {
        	instance.stdinSocket.send("mirrorsize\n");
        },
        display: function(plugin, data) {
        	instance.stdinSocket.send("display\n");
        },
        getopt: function(plugin, data) {
        	instance.stdinSocket.send("getopt\n" + plugin + "\n" + data);
        },
        setopt: function(plugin, data) {
        	instance.stdinSocket.send("setopt\n" + plugin + "\n" + data);
        },
        getdir: function(plugin, data) {
        	instance.stdinSocket.send("getdir\n" + plugin + "\n");
        },
        jscmd: function(plugin, data) {
        	var jsonStr = JSON.stringify(data);
        	instance.stdinSocket.send("jscmd\n" + plugin + "\n" + jsonStr);
        }
    };

	this.apiResponse = function(data) {
        var response = data.split(':');
  		var res = response.slice(0, 3);
  		res.push(response.slice(3).join(':'))

  		if (instance.doLogging)
        	console.log(res);

        var completedAction = res[0],
            status = res[1],
            plugin = res[2],
            payload = instance.transformApiResponsePayload[completedAction](res[3]);

        //call user defined callback
		instance.apiResponseCallback[completedAction](status, plugin, payload);
	};

	this.onConnection = function(cb) {
		setTimeout(function() {
			if (instance.stdinSocket.readyState === 1) {
				if (cb) cb();
				return;
			}
			instance.onConnection(cb);
		}, 50);
	};

	this.doAPICall = function(call, plugin, data, noBuffer) {

		instance.apiCall[call](plugin, data);
	};


	this.init = function() {
		instance.stdinSocket = new WebSocket("ws://" + window.location.host, "STDIN");

		instance.stdinSocket.onmessage = function(e) {
			console.log(e.data);
			instance.apiResponse(e.data);
		};
	};

	this.close = function() {
		instance.stdinSocket.close();
	};

	this.init();
}
