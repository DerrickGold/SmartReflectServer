# Smart Reflect Server
A modular Smart Mirror software solution with API's for external hardware interaction, and support for plugins written in any programming language.


##About:
The biggest flaw of the typical Smart Mirror setup is the utilization of the web browser as the mirror display’s form of information presentation. Web browsers create a sandboxed environment for the code that runs within it which limits hardware access and is typically driven through user events generated on the displayed web page. These limitations pose a few problems: 
  1. user events can not be generated naturally in a browser when one interacts with the browser as one would a mirror; 
  2. the “sandboxed” environment limits the use of external hardware to generate new events based on typical mirror interaction; 
  3. so far, only JavaScript runs natively in web browsers. 

As a result, current smart mirror software solutions are limited in that they: 
  1. are not truly modular, plugin systems exist but require some JavaScript knowledge to enable, disable, or configure plugins 
  2. utilize complicated server side solutions that are typically geared for web sites and REST API’s. This limitation is further amplified by the fact that users typically have no easy way of generating events to obtain data through an already established design principle
  3. not inclusive for all programmers and programming methodologies. Only JavaScript is supported which in the web browser is geared for event driven programming. So far, no solutions through the browser display system  exist for supporting other programming languages with their vast libraries of features or user base, thus, fragmenting the potential pool of developers for extending smart mirror features.

That being said, there are still some obvious benefits for using a web browser for displaying information:
  1. it is scriptable with JavaScript and customizable with CSS
  2. it has built in support for multiple media formats such as text, images, and video 
  3. hyperlinking and web connectivity allows for borrowing and sharing of resources.

Smart Reflect Server solves these browser limitations by creating and managing a real time communication API for plugin communications. This allows native programs and scripts to send draw calls and retreive information from the web browser display with all the scripting and styling benefits of using a web browser for presenting plugin information.

###Features:
* Support for plugins written in any programming language! 
* An API for any program to update their own web display and web configuration
* An API for managing plugins with built in web interface (enabling/disabling plugins + re-arranging plugins) 
* Built in scheduler for running short lived scripts/programs and redirecting their output onto the mirror display
* Easy to add/remove plugins, no coding necessary; just drag and drop them into the necessary folder!

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

The web GUI for enabling, disabline, and moving plugins, or accessing plugin specific settings.

![Moving Plugin] (https://raw.githubusercontent.com/DerrickGold/SmartReflectServer/master/ScreenShots/MovePlugin.png)

Rearrange your plugins using your phone touch screen via the web GUI!

##Wiki
Please refer to the Wiki for further details on setup, operation, and plugin development.
[The Wiki] (../../wiki)

##Project History
This project started as a capstone project for our (Derrick and David) Computer Science degree. Our semester was only 3 months long, so with other classes going on, not a lot of time could be put into the project; it's more or less a proof of concept system. This repository is a second repository with all our capstone specific information and code that is not necessary removed for the sake of keeping our original repository intact for marking and historic purposes; hence the suddenly populated repository.

Our prototype mirror utilizes a Raspberry Pi 2B with a 3.5" touch screen on it for interactive demos as seen below.
![Prototype Hardware](https://raw.githubusercontent.com/DerrickGold/SmartReflectServer/master/ScreenShots/Prototype.jpg)

[Video with the software running](https://www.youtube.com/watch?v=vvyk46WU3A4)
