var PluginClient = function(clientSettings) {

	var instance = this;

	this.clientInfo = clientSettings;

	this.socketObj = null;
	this.doLogging = false;

	this.splitToken = '\n';
	this.isReading = false;
	this.outDiv = null;
	this.cssPath = [];
	this.jsPath = [];
	this.jsClass = null;

	this.scriptLoadCount = 0;
	this.cssLoadCount = 0;
	this.instantiateTimer = null;
	this.doneLoadTimer = null;


	this.getDiv = function() {
		return this.outDiv;
	}

	//break file caching done by browsers to allow for proper reloading of resources
	this.breakCache = function(filename) {
		return filename + "?" + Math.floor((Math.random() * 1000000) + 1);
	}

	//source http://www.javascriptkit.com/javatutors/loadjavascriptcss2.shtml
	this.removeJsCssFile = function(filename, filetype) {
		if (filename === null) return;

		//determine element type to create nodelist from
	    var targetelement=(filetype=="js")? "script" : (filetype=="css")? "link" : "none"
	    //determine corresponding attribute to test for
	    var targetattr=(filetype=="js")? "src" : (filetype=="css")? "href" : "none"
	    var allsuspects=document.getElementsByTagName(targetelement)

	    //search backwards within nodelist for matching elements to remove
	    for (var i=allsuspects.length; i>=0; i--) {
		    if (allsuspects[i] && allsuspects[i].getAttribute(targetattr)!=null &&
		    	allsuspects[i].getAttribute(targetattr).indexOf(filename)!=-1)
		    {
		    	//remove element by calling parentNode.removeChild()
		        allsuspects[i].parentNode.removeChild(allsuspects[i])
		    }

	    }
	}

	//source http://www.javascriptkit.com/javatutors/loadjavascriptcss.shtml
	this.loadJsCssFile = function(filename, filetype, cb) {
		if (filename === null) return;

		var fileref = "undefined";
	    if (filetype=="js"){ //if filename is a external JavaScript file
	        fileref=document.createElement('script');
	        fileref.setAttribute("type","text/javascript");
	        fileref.setAttribute("src", filename);

	       	fileref.addEventListener('load', function() {
	    		instance.scriptLoadCount++;
	    	});
	    }
	    else if (filetype=="css"){ //if filename is an external CSS file
	        fileref=document.createElement("link")
	        fileref.setAttribute("rel", "stylesheet")
	        fileref.setAttribute("type", "text/css")
	        fileref.setAttribute("href", filename)

	        fileref.addEventListener('load', function() {
	        	instance.cssLoadCount++;
	        });
	    }


	    document.getElementsByTagName("head")[0].appendChild(fileref);
	}

	/*
	 * These actions are available for an instantiated JavaScript class object
	 * to call for retreiving data from the server.
	 */
	this.supplementalActions = {
		apiResponse: function(data) {
	        var response = data.split(':');
	  		var res = response.slice(0, 4);
	  		res.push(response.slice(4).join(':'))


	        var idToken = res[0],
	        	completedAction = res[1],
	            status = res[2],
	            plugin = res[3],
	            payload = res[4];

	        console.log(res);

	        //call user defined callback
	        if (status !== "fail")
				instance.supplementalActions.callbacks[completedAction](payload);
		},

		callbacks: {
			getopt: function(response) {
				var parts = response.split(':', 2);

				if (instance.supplementalActions.pluginConf.onGet)
					instance.supplementalActions.pluginConf.onGet({ setting: parts[0], value: parts[1]});
			},

			setcfg: function(response) {
				if (instance.supplementalActions.pluginConf.onSet)
					instance.supplementalActions.pluginConf.onSet(response);
			}
		},

		/*
		 * Plugin's JavaScript object instance only gets access to this portion
		 * of the API.
		 */
		pluginConf: {
			//functions to call when the API response is returned
			onGet: null,
			onSet: null,

			/*
			 * API calls require the [API] header prepended. The API runs on its own
			 * socket protocol that plugins do not connect with. This header informs the
			 * plugin protocol to perform an API call with the data sent to it.
			 */

			/*
			 * Get a setting from the "plugin.conf" file
			 */
			get: function(data) {
				instance.socketObj.send("[API]\ngetopt\n" + instance.clientInfo.containerID +
										"\n" + data);
			},

			/*
			 * Add or overwrite a setting in the "plugin.conf" file
			 */
			set: function(data) {
				instance.socketObj.send("[API]\nsetopt\n" + instance.clientInfo.containerID  +
										"\n" + data);
			}
		}
	};

	/*
	 * These actions define functions that are available to be called from
	 * the server. This allows external programs or scripts to modify the
	 * plugin client DOM, and the server to load and unload plugin data.
	 * Plugins powered by JavaScript do not need these particular functions.
	 */
	this.availableActions = {
		//completely disable this plugin
		close: function(data) {
			instance.close();
		},

		//write data to plugin's div element text
	    write: function(data) {
	        var div = instance.getDiv();
	      	data = data.replace(/\n/g, "<br>");
	      	div.innerHTML += data;
	    },

	    innerdiv: function(data) {
	    	var div = instance.getDiv();
	    	div.innerHTML = data;
	    },

	    //clear contents of a plugin's div element
	    clear: function(data) {
	        var div = instance.getDiv();
	        div.innerHTML = "";
	    },

	    //helper function for unloading this plugin
	    unload: function(data) {
	    	//unload the contents of the main javascript class
	    	if (instance.jsClass != null && instance.jsClass.destroy)
	    		instance.jsClass.destroy();

	    	//then clear the html content for this plugin
	    	var div = instance.getDiv();
	    	div.innerHTML = "";

	    	//remove all css files
	    	while (instance.cssPath.length > 0) {
	    		var file = instance.cssPath.pop();
	    		instance.removeJsCssFile(file, "css");
	    	}

	    	//remove all js file inclusions
	    	while (instance.jsPath.length > 0) {
	    		var file = instance.jsPath.pop();
	    		instance.removeJsCssFile(file, "js");
	    	}

	    	//clear if script is in the middle of loading
	    	instance.scriptLoadCount = 0;
	    	if (instance.instantiateTimer)
	    		window.clearInterval(instance.instantiateTimer);
			instance.instantiateTimer = null;

			if (instance.doneLoadTimer)
				window.clearInterval(instance.doneLoadTimer);
	    	instance.doneLoadTimer = null;


			//remove the div created by this script
			var div = instance.getDiv();
			if (div.parentNode)
				div.parentNode.removeChild(div);
	    },

	    //load this plugin
	    load: function(data) {
	    	if (instance.doLogging)
	    		console.log("Loading: ");

	    	data = JSON.parse(data);

	    	if (instance.doLogging)
	    		console.log(data);

	    	//load new css file if given
	    	if (data["css"] != null && data["css"].length > 0){
	    		data["css"].forEach(function(cssFile) {
	    			if (cssFile.length <= 0) return;

	    			//make it so the browser can't cache the file name
		    		var file = instance.breakCache(cssFile)
		    		//add css file to list of css files
		    		instance.cssPath.push(file);
		    		//finally load the css file
		    		instance.loadJsCssFile(file, "css");
	    		});
	    	} else
	    		instance.cssPath = [];

	    	//load new javascript file if given
	    	if (data["js"] != null && data["js"].length > 0){
	    		data["js"].forEach(function(jsFile) {

	    			if (jsFile.length <= 0) return;

	    			var file = instance.breakCache(data["js"]);
		    		instance.jsPath.push(file);
	    			instance.loadJsCssFile(file, "js");
	    		});
	    	} else
	    		instance.jsPath = [];

	    	//before instantiating the plugins class object
	    	//make sure all scripts are loaded
	    	instance.instantiateTimer = setInterval(function() {

	    		if (instance.scriptLoadCount < data["js"].length)
	    			return;

				 //if a main class has been specified, we need to instantiate it
				if (data["main"] != null && data["main"].length > 0) {
					var str = "new " + data["main"] + "(instance.supplementalActions.pluginConf);";

					if (instance.doLogging)
						console.log("Invoking: " + str);

					instance.jsClass = eval(str);
				}

				//once loaded, we don't need to keep polling
				window.clearInterval(instance.instantiateTimer);
				instance.instantiateTimer = null;

	    	}, 10);


	    	//set an interval to wait check for all browser files loaded
	    	instance.doneLoadTimer = setInterval(function() {

	    		//not done loading everything
	    		if (instance.cssLoadCount < data["css"].length ||
	    			instance.scriptLoadCount < data["js"].length)
	    			return;

	    		//when all js and css files are loaded, send message back
	    		//to server indicating a plugin client has completely loaded
	    		instance.socketObj.send("PluginClient Loaded");
	    		window.clearInterval(instance.doneLoadTimer);
	    		instance.doneLoadTimer = null;
	    	}, 10);

	    },

	    //set or modify a css style attribute for the plugin div.
	    setcss: function(data) {
	       	var style = instance.getDiv().style;

	       	var rules = data.split(';');
	       	rules.forEach(function(css) {
	       		var results=css.split("=", 2);
	       		if (results.length > 1) {
	       			//if the value is defined and not NULL, set the style
	       			if (results[1] !== undefined && results[1] != "NULL" && results[1] != null)
	       				style[results[0]] = results[1];
	       			//otherwise, remove the style
	       			else
	       				style[results[0]] = "";
	       		}
	       	});

	       	instance.socketObj.send("CSS Applied");
	    },

	    //return a css style attribute from this plugin div back to caller
	    getcss: function(data) {
	    	var style = window.getComputedStyle(instance.getDiv(), '');
	    	var properties = data.split(',').filter(function(s) { return s.length > 0; });
	    	var results = "";

	    	properties.forEach(function(p) {
	    		var prop = style.getPropertyValue(p);

	    		//since multiple parameters can be requested
	    		//preserve order for parameters that don't exist
	    		if (!prop)
	    			prop = "NULL";

	    		results = results.concat( p + '=' + prop + "\n");
	    	});

			instance.socketObj.send(results);
	    },

	    //execute a function from the instantiated JavaScript class created
	    //for this plugin
	    jsPluginCmd: function(data) {
	    	var preJson = data;

	    	if (!instance.jsClass) {
	    		instance.socketObj.send("No Javascript Class Instance Defined");
	    		return;
	    	}

	    	try {
	    		if (typeof data == 'string')
		    		data = JSON.parse(data);

	    		if (instance.doLogging)
	    			console.log(data);

	    		var fnName = data.fn;
	    		var args = data.args;
	    		var response = fnName + ":" + instance.jsClass[fnName](args);
	    		instance.socketObj.send(response);
	    	} catch (err) {
	    		if (instance.doLogging) {
	    			console.log(preJson);
	    			console.log(err.message);
	    		}

	    		instance.socketObj.send(fnName + ":" + err.message);
	    	}
	    }

	};

	//parse in-comming API call from server and call the appropriate function
	this.parseAction = function(input) {
	    var action = this.availableActions;

	    if (input.constructor === Array) {
      		input.forEach(function(obj) {
	        	action[obj.command](obj.data);
	      	});
	    }
	    else if (typeof input === 'object') {
	      	action[input.command](input.data);
	    }
	    else {
	    	instance.supplementalActions.apiResponse(input);
	    }
	};


	this.socketReceive = function(data) {
		if (instance.doLogging)
			console.log(data.data);
		try {
			instance.parseAction(JSON.parse(data.data));
		} catch (err) {
			instance.parseAction(data.data);
		}

	}


	this.serverConnect = function(server) {
		var name = server.sockName;
		var portNum = server.portNum;


		if (instance.doLogging)
			console.log("Starting websocket: " + name);

		instance.socketObj = new WebSocket("ws://" + server.ip + ":" + portNum, name);
		instance.socketObj.onmessage = instance.socketReceive;
		instance.socketObj.onopen = function(e) {
			if (instance.doLogging)
				console.log("starting input");

			//send message back to server indicating connection opened
			instance.socketObj.send(name);
		};
	}

	this.init = function() {

		//check if div already exists
		instance.outDiv = document.getElementById(instance.clientInfo.containerID);
		if (!instance.outDiv) {
			//create output div if it doesn't exist
			instance.outDiv = document.createElement('div');
			instance.outDiv.setAttribute('id', instance.clientInfo.containerID);
			document.body.appendChild(instance.outDiv);
		}

		instance.serverConnect({
			sockName: instance.clientInfo.protocol,
			portNum: instance.clientInfo.port,
			ip: instance.clientInfo.ip
		});
	}

	this.close = function() {
		instance.socketObj.close();
		instance.availableActions["unload"](null);
	}

  	this.init();
};



