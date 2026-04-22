This repository was created after about a year ago I decided to reclaim an old skill of C Programming. It has been 40 some  years since
I thought about doing this. A friend of mine and I decided to buy Arduino UNO R4 WiFI kits and follow Paul McWhorter on YouTube
since he has a program for this board where you basically start from scratch. It was difficult going through this but by the time
we got to around lesson 70 we got wind that Visual Studio was free to experimenters like us. Got the 2022 version and began to play with it, and noticing CoPilot
and AI assisted programming. I tossed in a broken lesson from Paul and said Fix My Code to CoPilot. It returned a perfectly working file so decided to embrace this
technology.

So what should we do with AI? We decided to use a club CW trainer curriculum another ham buddy found for us online. Plugging bits of that into Claude
in Virtual Studio we slowly developed the CW Trainer and Memory Keyer project you see here. I being an avid CW guy did most of the testing and final
code cleanup. It was just easier for me than for Dan so no glory hogging here. Dan did plenty. As is here the program works great. There is a schematic
that will help you wire a prototype using a perfboard to mount the ESP-32 and all the parts. Thise are available on Amazon cheap.
THe prototype is not a complicated thing. I used a quad DIP switch to enable the options shown on the schematic. Out of the box
into a bare ESP-32 board (I used the 30 pin version Devkit) with no external wiring, it will fire up in the Access Point mode. See the documentation
in the Docs directory for details connecting. It should compile using the Arduino IDE though I have not tested that yet at the time of writing this as I
am using Visual Studio 2022. In the AP mode you can bring up the trainer dashboard on your smartphone.

I can send a file that you can install onto the board using ESPConnect from Espressif. There is a browser version that gives all the details you
need on your ESP-32 boards. Give it a try and good luck!
