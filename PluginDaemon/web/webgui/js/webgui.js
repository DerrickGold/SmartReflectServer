var WebGui = function(mirrorAPI) {
	var instance = this;

    this.movemenu = null;
    this.initialMove = false;
    this.curSent = -1;

    var selectedPlugin = {
        name: null,
        css: {},
    };

    //each plugin on the page
    var panels = {};

    var mirrorH = 1920,
        mirrorW = 1080,
        haveSize = false;

    var cssProperties = ["left", "top", "position", "transform", "margin",
                	"marginLeft", "marginRight"];

    var panel = "<div class=\"panel panel-default\">" +
    			"<div class=\"panel-heading\" role=\"tab\">" +
      			"<h4 class=\"panel-title\">" +
        		"<a role=\"button\" data-toggle=\"collapse\" data-parent=\"#accordion\"  aria-expanded=\"false\">" +
        		"</a>" + "</h4>" + "</div>" +
    			"<div class=\"panel-collapse collapse\" role=\"tabpanel\">" + "<div class=\"panel-body\">" + "</div></div></div>"

    var button = "<button type=\"button\" class=\"btn btn-primary\" status=0></button>"

    var statusIcon = "<span class=\"label\">Not Available</span>"

    function showInstallScreen(on) {
        if (on)
            $('#installScreen').show();
        else
            $('#installScreen').hide();
    }


    function statusClass(pluginName) {
    	return pluginName + "Status";
    }

   	function moveClass(pluginName) {
   		return pluginName + "Move";
   	}

   	function guiClass(pluginName) {
   		return pluginName + "Gui";
   	}


    //updates all status indicators for a plugin
    function setStatus(pane, plugin, newStatus) {

		if (newStatus === "success")
			newStatus = 1;
		else
			newStatus = 0;

		$(pane).find("." + statusClass(plugin)).each(function(el){
			var element = $(this);
			console.log(element);

			//update label to reflect the plugin status
			if (element.hasClass("label")) {

				element.removeClass("label-success");
				element.removeClass("label-default");

				if (newStatus) {
					element.addClass("label-success");
					element.text("Enabled");
				} else {
					element.addClass("label-default");
					element.text("Off");
				}
			}
			//update toggle buttons
			else if (element.hasClass("btn")) {
				if (newStatus)
					element.text("Disable");
				else
					element.text("Enable");

				element.attr("status", newStatus);
			}
		});

    	var movBtn = $(pane).find("." + moveClass(plugin)).first();
    	if (newStatus)
    		movBtn.prop('disabled', false);
    	else
    		movBtn.prop('disabled', true);
	}


	function makeInnerPanel(plugin) {

		//var div = $("div").addClass(plugin);
		//div.append(button);
		var div = document.createElement("div");
		var newButton = $(button).addClass(statusClass(plugin));


		$(newButton).click(function() {
			var btn = $(this);

			var result = btn.attr("status");
			console.log("Button Status: " + result);
			if (result == 1)
				//if plugin is enabled, disable it
				mirrorAPI.doAPICall("disable", plugin);
			else
				//otherwise, if plugin is disabled, enable it
				mirrorAPI.doAPICall("enable", plugin);
		});

        var movButton = $(button).addClass(moveClass(plugin)).text("Move");

        $(movButton).click(function() {
            mirrorAPI.doAPICall("getcss", plugin, cssProperties);

            //hide plugins when in move menu
            $('#accordion').hide();
            instance.movemenu.showMirror();
        });

        var webguiButton = $(button).addClass(guiClass(plugin)).text("Plugin Gui");
        $(webguiButton).hide();

		$(div).append(newButton).append(movButton).append(webguiButton);
		return div;
	}



	function makePanel(plugName, number, innerdata, status) {
		var newPanel;
		newPanel = $(panel);

		var heading = "heading" + number;
		var collapse = "collapse" + number;


		//set heading name
		$(newPanel).children(".panel-heading:first-child").attr('id', heading);
		//set button url
		var panelTitleBar = $(newPanel).find("h4.panel-title").first();
		$(newPanel).find("a").first().attr("href", "#" + collapse)
				.attr("aria-controls",collapse)
				.text(plugName);

		var statIcon = $(statusIcon).addClass(statusClass(plugName));
		panelTitleBar.prepend(statIcon);

		//set collapse content
		$(newPanel).find(".panel-collapse.collapse").first()
				.attr("aria-labelledby", heading)
				.attr("id", collapse);
		$(newPanel).find(".panel-body").first().append(innerdata);

		//$("#accordion").append(newPanel);
		return newPanel;
	}

    this.initMoveMenu = function(mirrorWidth, mirrorHeight) {
        instance.movemenu = new MoveMenu(mirrorWidth, mirrorHeight, function(left, top) {
                //move callback, update plugin position temporarily based on movemenu position

                if (panels[selectedPlugin.name].csssent == instance.curSent)
                    return;

                instance.curSent = panels[selectedPlugin.name].csssent;

                if (!instance.initialMove) {
                    mirrorAPI.doAPICall("setcss", selectedPlugin.name, {
                        "position":"absolute",
                        "transform": "0",
                        "margin": "0",
                        "left": left,
                        "top": top
                        });

                    instance.initialMove = true;
                } else {
                    mirrorAPI.doAPICall("setcss", selectedPlugin.name, {
                        "left": left,
                        "top": top
                        }, true);
                }
            },

            //cancel callback, exits move menu without saving new position
            function() {
                //show plugins when exiting move menu
                $('#accordion').show();

                //revert plugin on mirror back to its original location
                //copy all plugin settings and set them to NULL to be removed
                var styles = {};
                for (var prop in selectedPlugin["css"]) {
                    if (selectedPlugin["css"].hasOwnProperty(prop)) {

                        var property = selectedPlugin["css"][prop];
                        if (property == null || property == undefined || property == "NULL")
                            property = "";

                        styles[prop] = property;
                    }
                }
                mirrorAPI.doAPICall("setcss", selectedPlugin.name, selectedPlugin["css"]);
                instance.initialMove = false;
            },
            //save cb, exits menu and saves position
            function() {
                //show plugins when exiting move menu
                $('#accordion').show();

                mirrorAPI.doAPICall("dumpcss", selectedPlugin.name, null);
                instance.initialMove = false;
            });
    }


	this.init = function() {
		//initialize mirror api callbacks
        mirrorAPI.onAPIResponse("list", function(status, plugin, allPlugins) {

            //clear out options first before repopulating
            $('#accordion').empty();
            panels = {};

            allPlugins.forEach(function(plugin, index) {
                var newPane = makePanel(plugin, index, makeInnerPanel(plugin));
                panels[plugin] = newPane;
                $("#accordion").append(newPane);
                mirrorAPI.doAPICall("status", plugin);
                mirrorAPI.doAPICall("getopt", plugin, "webgui-html");
                mirrorAPI.doAPICall("getdir", plugin);
            });
        });

        mirrorAPI.onAPIResponse("getstate", function(status, plugin, payload) {
            //update status for the specific plugin
            var pane = panels[plugin];
            setStatus(pane, plugin, status);
        });

        mirrorAPI.onAPIResponse("disable", function(status, plugin, payload){
            var pane = panels[plugin];

            //plugin disabled successfully
            if (status == "success")
                //so set status to disabled
                status = 0;

            setStatus(pane, plugin, status);
        });

        mirrorAPI.onAPIResponse("enable", function(status, plugin, payload) {
            var pane = panels[plugin];
            setStatus(pane, plugin, status);
        });

        mirrorAPI.onAPIResponse("setcss", function(status, plugin, payload) {
            panels[plugin].csssent++;
        });

        mirrorAPI.onAPIResponse("getcss", function(status, plugin, css) {
            panels[plugin].csssent = 0;
            selectedPlugin.name = plugin;
            selectedPlugin["css"] = css;
            $("#posMarker").css("left",
                instance.movemenu.scalePos(parseInt(selectedPlugin["css"].left), mirrorW));
            $("#posMarker").css("top",
                instance.movemenu.scalePos(parseInt(selectedPlugin["css"].top), mirrorH));

        });

        mirrorAPI.onAPIResponse("mirrorsize", function(status, plugin, size) {
            mirrorW = size.width;
            mirrorH = size.height;
            instance.haveSize = true;
            instance.initMoveMenu(mirrorW, mirrorH);
        });

        mirrorAPI.onAPIResponse("savecss", function(status, plugin, payload) {

        });

        mirrorAPI.onAPIResponse("display", function(status, plugin, payload) {
            if (status == "success") {
                $('.Connected.alert.alert-warning').hide();
                if (!instance.haveSize)
                    mirrorAPI.doAPICall("mirrorsize");
            } else
                $('.Connected.alert.alert-warning').show();

        });

        mirrorAPI.onAPIResponse("getopt", function(status, plugin, payload) {
        	console.log("Got Option: ");
        	console.log(payload);
        	if (status != "success")
        		return;

        	if (payload.setting == 'webgui-html') {
        		var configButton = '.' + guiClass(plugin);
	        	var btn = $(configButton).first();
	        	btn.show();
	        	btn.click(function(e) {
                    mirrorAPI.close();
	        		window.location = "../" + panels[plugin].pluginDir + "/" + payload.value;
	        	});



	        }
        });

        mirrorAPI.onAPIResponse("getdir", function(status, plugin, payload) {
        	panels[plugin].pluginDir = payload;
        });

        mirrorAPI.onAPIResponse("install", function(status, plugin, payload) {
            showInstallScreen(false);
        });

        mirrorAPI.onAPIResponse("reboot", function(status, plugin, payload) {
            alert("Mirror is rebooting!");
        });

        mirrorAPI.doAPICall("list");
        mirrorAPI.doAPICall("display");
	};

    $('#addPluginBtn').prop('disabled', true);

    $('#pluginUrlBox').on('input change', function(e) {
        //console.log(e.target.value);
        var repoUrl = e.target.value;
        if (repoUrl.length > 0)
            $('#addPluginBtn').prop('disabled', false);
        else
            $('#addPluginBtn').prop('disabled', true);
    });

    $('#addPluginBtn').click(function() {
        var repourl = $('#pluginUrlBox').val();
        console.log("installing... " + repourl);
        mirrorAPI.doAPICall("install", null, repourl);
        $('#loadCenter p').text("Installing Plugin");
        showInstallScreen(true);
    });

	this.init();
};
