# Smart Reflect Server
A modular Smart Mirror software solution with API's for external hardware interaction.


##About:
Most smart mirror software solutions utilize the web browser for managing the mirror's display. However, the web browser is a sandboxed environment limiting the type plugins that can be made to only run within the browser--preventing hardware interaction with external devices and generating persistent data through interacting with the mirror. Smart Reflect Server solves this conundrum by creating and managing a real time communication protocol for plugin communications. This allows native programs and scripts to send draw calls and retreive information from the web browser display with all the scripting and styling benefits of using a web browser for theming plugin displays.

Features include:
* Support for plugins written in any programming language! 
* An API for any program to update their own web display and web configuration
* An API for managing plugins with built in web interface (enabling/disabling plugins + re-arranging plugins) 
* Built in scheduler for running short lived scripts/programs and redirecting their output onto the mirror display
* Easy to add/remove plugins, no coding necessary; just drag and drop them into the necessary folder!

##Screenshots:
![Mirror display](https://raw.githubusercontent.com/DerrickGold/SmartReflectServer/master/ScreenShots/Display.png)

Fortune piped into Cowsay bash script on display. 

![Web GUI] (https://raw.githubusercontent.com/DerrickGold/SmartReflectServer/master/ScreenShots/WebManager.png)

The web GUI for enabling, disabline, and moving plugins, or accessing plugin specific settings.

![Moving Plugin] (https://raw.githubusercontent.com/DerrickGold/SmartReflectServer/master/ScreenShots/MovePlugin.png)

Rearrange your plugins using your phone touch screen via the web GUI!

##Wiki
Please refer to the Wiki for further details on setup, operation, and plugin development.
[The Wiki] (../../wiki)
