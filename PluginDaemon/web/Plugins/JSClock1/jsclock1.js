var JSClock1 = function() {


	var instance = this;
	this.updatePeriod = 5; //update clock ever x seconds
	this.loaded = false;
	this.interval = null;
	this.timeInterval = null;

	this.minObj = null;
	this.hourObj = null;
	this.secObj = null;

	this.setPeriod = function(newPeriod) {
		newPeriod = parseInt(newPeriod);

		if (instance.timeInterval)
			window.clearInterval(instance.timeInterval)

		instance.updatePeriod = parseInt(newPeriod * 1000);

		instance.timeInterval = setInterval(function() {
	    	instance.updateTime();
	    }, instance.updatePeriod);

	}

	this.getPeriod = function(nothing) {
		return parseInt(instance.updatePeriod/1000);
	}

	this.updateTime = function() {
		var angle = 360/60,
            date = new Date(),
            hour = date.getHours() % 12,
            minute = date.getMinutes(),
            second = date.getSeconds(),
            hourAngle = (360/12) * hour + (360/(12*60)) * minute;


        instance.minObj.style['transform'] = 'rotate('+angle * minute+'deg)';
        instance.hourObj.style['transform'] = 'rotate('+hourAngle+'deg)';
        instance.secObj.style['transform'] = 'rotate('+angle * second+'deg)';
	}

	//constructor must be called init for the web frontend to initialize
	//when this plugin is loaded
	this.init = function() {

		console.log("JSClock1 constructor called");

	    instance.minObj = document.getElementById('minute');
	    instance.hourObj = document.getElementById('hour');
	    instance.secObj = document.getElementById('second');

	    instance.updateTime();
	    instance.setPeriod(instance.updatePeriod)
	}

	//destructor must be named 'destroy' for web frontend to cleanup
	//when unloading this plugin
	this.destroy = function() {
		console.log("JSClock1 destructor called");

		if (instance.interval)
			window.clearInterval(instance.interval);
		if (instance.timeInterval)
			window.clearInterval(instance.timeInterval);
	}

	//wait for html data to load before initializing
	this.interval = setInterval(function() {
		var clockObj = document.getElementById('clock');
		loaded = (clockObj !== undefined && clockObj !== null);

		if (loaded) {
			window.clearInterval(instance.interval);
			instance.interval = null;
			instance.init();
		}

	}, 50);
};
