# Smart Reflect Server
A modular Smart Mirror software solution with API's for external hardware interaction, and support for plugins written in any programming language.

Published Paper here: http://ieeexplore.ieee.org/document/7746277/

##Wiki
Please refer to the Wiki for further details on setup, operation, and plugin development.
[The Wiki] (../../wiki)

###Features:
* Support for plugins written in any programming language! 
* An API for any program to update their own web display and web configuration
* An API for managing plugins with built in web interface (enabling/disabling plugins + re-arranging plugins) 
* Built in scheduler for running short lived scripts/programs and redirecting their output onto the mirror display
* Easy to add/remove plugins, no coding necessary; just drag and drop them into the necessary folder!

##Sample Plugins:
* [YoutubePlaylist] (https://github.com/DerrickGold/YoutubePlaylist): Queue and play youtube videos on your mirror

* [SysInfo] (https://github.com/DerrickGold/SysInfo): Display system information

* [XKCDComic] (https://github.com/DerrickGold/XKCDComic): Display random XKCD Comics


##Screenshots:
![Mirror display](https://raw.githubusercontent.com/DerrickGold/SmartReflectServer/master/ScreenShots/Display.png)

Fortune piped into Cowsay bash script on display. 

![On actual Mirror] (https://raw.githubusercontent.com/DerrickGold/SmartReflectServer/master/ScreenShots/Running.jpg)

The actual mirror running various plugins.

![Shopping List] (https://raw.githubusercontent.com/DerrickGold/SmartReflectServer/master/ScreenShots/ShoppingList.jpg)

A shopping list created using the attached touchscreen.

![Cowsay enabled] (https://raw.githubusercontent.com/DerrickGold/SmartReflectServer/master/ScreenShots/Cowsay.jpg)

Cowsay plugin enabled on the actual mirror.

![Web GUI] (https://raw.githubusercontent.com/DerrickGold/SmartReflectServer/master/ScreenShots/WebManager.png)
![Gui Options] (https://raw.githubusercontent.com/DerrickGold/SmartReflectServer/master/ScreenShots/menu-expanded.png)

The web GUI for installing, enabling, disabling, moving plugins, or accessing plugin specific settings.

![Moving Plugin] (https://raw.githubusercontent.com/DerrickGold/SmartReflectServer/master/ScreenShots/MovePlugin.png)

Rearrange your plugins using your phone touch screen via the web GUI!



##Project History
This project started as a capstone project for our (Derrick and David) Computer Science degree. Our semester was only 3 months long, so with other classes going on, not a lot of time could be put into the project; it's more or less a proof of concept system. This repository is a second repository with all our capstone specific information and code that is not necessary removed for the sake of keeping our original repository intact for marking and historic purposes; hence the suddenly populated repository.

Our prototype mirror utilizes a Raspberry Pi 2B with a 3.5" touch screen on it for interactive demos as seen below.
![Prototype Hardware](https://raw.githubusercontent.com/DerrickGold/SmartReflectServer/master/ScreenShots/Prototype.jpg)

[Video with the software running](https://www.youtube.com/watch?v=vvyk46WU3A4)
