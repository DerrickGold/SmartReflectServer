var Display = function(socket, port, comDir) {
	var instance = this;

	this.doLogging = false;
	this.comDir = comDir;
	this.socket = null;
	this.pluginList = {};
	this.ipAddr = window.location.hostname;


	this.onmessage = function(data) {
		data = JSON.parse(data.data);

		if (instance.doLogging) {
			console.log("Receieved:");
			console.log(data);
		}
		setTimeout(function(){
			instance.actions[data.cmd](data);
		}, 50);
	}

	this.transform = function(name) {

		return name + "Socket";
	}


	this.actions = {

		load: function(data) {
			var plugName = instance.transform(data.pName);
			if (instance.doLogging) {
				console.log("Initializing: " + plugName);
				console.log(data);
			}
			instance.pluginList[plugName] = new PluginClient(data.pName, data.pDiv, instance.comDir, instance.ipAddr);

		},
		unload: function(data) {
			var plugName = instance.transform(data.pName);
			if (instance.pluginList[plugName] !== undefined) {
				instance.pluginList[plugName].close();
				delete instance.pluginList[plugName];
			}
		},
		getsize: function(data) {
			var width = window.innerWidth,
				height = window.innerHeight;

			if (instance.doLogging)
				console.log("Sending size!");
			instance.socket.send(window.innerWidth + "x" + window.innerHeight);
		}

	}


	this.init = function() {
		if (instance.ipAddr.length <= 0 || instance.ipAddr == "")
			instance.ipAddr = "127.0.0.1";

		instance.socket = new WebSocket("ws://" + instance.ipAddr + ":" + port, socket);
		if (instance.doLogging)
			console.log("CONNECTING TO: " + instance.ipAddr);
		instance.socket.onmessage = instance.onmessage;
		instance.socket.onopen = function(e) {
			instance.socket.send("ready");
		};
	}

	this.init();
}
