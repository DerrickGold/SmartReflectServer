var PluginClient = function(inputSrc, outDiv, comDir, ipAddr) {
	var instance = this;

	this.socketObj = null;
	this.doLogging = true;

	this.splitToken = '\n';
	this.isReading = false;
	this.outDivName = outDiv;
	this.outDiv = null;
	this.inputSrc = inputSrc;
	this.worker = null;
	this.sockPort = 5000;
	this.cssPath = [];
	this.jsPath = [];
	this.jsClass = null;
	this.ipAddr = ipAddr;

	this.scriptLoadCount = 0;
	this.cssLoadCount = 0;
	this.instantiateTimer = null;
	this.doneLoadTimer = null;


	this.getDiv = function() {
		return this.outDiv;
	}

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


	this.availableActions = {
		close: function(data) {
			instance.close();
		},
	    write: function(data) {

	        var div = instance.getDiv();
	      	data = data.replace(/\n/g, "<br>");
	      	div.innerHTML += data;
	    },
	    innerdiv: function(data) {
	    	var div = instance.getDiv();
	    	div.innerHTML = data;
	    },
	    clear: function(data) {

	        var div = instance.getDiv();
	        div.innerHTML = "";
	    },
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
					var str = "new " + data["main"] + "();";

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
	    		/*instance.worker.postMessage({
	    			type: "send",
	    			data: "PluginClient Loaded"
	    		});*/
	    		instance.socketObj.send("PluginClient Loaded");
	    		window.clearInterval(instance.doneLoadTimer);
	    		instance.doneLoadTimer = null;
	    	}, 10);

	    },
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

	       	/*instance.worker.postMessage({
	       		type: "send",
	       		data: "CSS Applied"
	       	});*/
	       	instance.socketObj.send("CSS Applied");
	    },
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

		    /*instance.worker.postMessage({
				type: "send",
				data: results
			});*/
			instance.socketObj.send(results);

	    },
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


	this.parseAction = function(input) {

	    var action = this.availableActions;

	    if (input.constructor === Array) {
      		input.forEach(function(obj) {
	        	action[obj.command](obj.data);
	      	});
	    }
	    else {
	      	action[input.command](input.data);
	    }
	};

	this.socketReceive = function(data) {
		console.log(data.data);
		instance.parseAction(JSON.parse(data.data));
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

			instance.socketObj.send(name);
		};
	}

	this.init = function() {

		//check if div already exists
		instance.outDiv = document.getElementById(instance.outDivName);
		if (!instance.outDiv) {
			//create output div if it doesn't exist
			instance.outDiv = document.createElement('div');
			instance.outDiv.setAttribute('id', instance.outDivName);
			document.body.appendChild(instance.outDiv);
		}

		instance.serverConnect({
			sockName: instance.inputSrc,
			portNum: instance.sockPort,
			ip: instance.ipAddr
		});
	}

	this.close = function() {
		instance.socketObj.close();
		instance.availableActions["unload"](null);
	}


  	this.init();
};



