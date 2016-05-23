var MirrorAPI = function(addr) {

	var instance = this;
	this.doLogging = false;
	this.stdinSocket = null;
	this.socketBusy = false;
	this.apiIdentifier = "mirrorAPI" + Math.random().toString(36);


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
            instance.apiSend("list", null, null);
        },
        status: function(plugin, data) {
            instance.apiSend("getstate", plugin, null);
        },
        enable: function(plugin, data) {
            instance.apiSend("enable", plugin, null);
        },
        disable: function(plugin, data) {
            instance.apiSend("disable", plugin, null);
        },
        setcss: function(plugin, cssObj) {

        	var data = "";
        	for (var key in cssObj) {

        		if (!cssObj.hasOwnProperty(key) || key == undefined ||
        			cssObj[key] == undefined)
        			continue;
	        	data = data.concat(key + '=' + cssObj[key] + ";");
        	}
        	//remove trailing ';'
            data = data.replace(/;+$/, "");
            if (instance.doLogging)
            	console.log(data);

            instance.apiSend("setcss", plugin, data);
        },
        getcss: function(plugin, cssvals) {

            var data = "";

            cssvals.forEach(function(css) {
                data = data.concat(css + ",");
            });

            //remove trailing comma
            data = data.replace(/,+$/, "");
            instance.apiSend("getcss", plugin, data);
        },
        dumpcss: function(plugin, data) {
            instance.apiSend("savecss", plugin, null);
        },
        mirrorsize: function(plugin, data) {
        	instance.apiSend("mirrorsize", null, null);
        },
        display: function(plugin, data) {
        	instance.apiSend("display", null, null);
        },
        getopt: function(plugin, data) {
        	instance.apiSend("getopt", plugin, data);
        },
        setopt: function(plugin, data) {
        	instance.apiSend("setopt", plugin, data);
        },
        getdir: function(plugin, data) {
        	instance.apiSend("getdir", plugin, null);
        },
        jscmd: function(plugin, data) {
        	var jsonStr = JSON.stringify(data);
        	instance.apiSend("jscmd", plugin, jsonStr);
        }
    };

    this.apiSend  = function(command, plugin, data) {
    	var str = instance.apiIdentifier + "\n" + command + "\n";
    	if (plugin) str += plugin + "\n";
    	if (data) str += data;

    	if (instance.doLogging)
	    	console.log("API STRING: \n" + str);
    	instance.stdinSocket.send(str);
    }

	this.apiResponse = function(data) {
        var response = data.split(':');
  		var res = response.slice(0, 4);
  		res.push(response.slice(4).join(':'))

  		if (instance.doLogging)
        	console.log(res);

        var apiIdentifier = res[0],
        	completedAction = res[1],
            status = res[2],
            plugin = res[3],
            payload = instance.transformApiResponsePayload[completedAction](res[4]);

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
			if (instance.doLogging)
				console.log(e.data);
			instance.apiResponse(e.data);
		};
	};

	this.close = function() {
		instance.stdinSocket.close();
	};

	this.init();
}
