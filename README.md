# MySoundDriver
This is my second software kerne driver project. a WDM non-PnP kernel driver with a user-mode application that used to play a sound when a device was plagged-in or out to the USB-port, You choose the audio file to be played. 

## The internal structure
In this project I faced a lot of new things. starting first with the driver it self:
- first I signed for PnP notifications with the USB port GUID to make the PnP manager send a notification when there's a device plugged in or out to that port
- I used also the **Inverted Call** model to make the driver can notify the user-mode application when there's an event. So I turned all the incoming Irps from user-mode app into pending state then queued it using the **Cancel-Safe framework** with **LIST_ENTRY** data structure. and when the call-back routine of the PnP notification runs it dequeue one of the queued Irps and complete it. 
- Inside the User-mode application, I made it multi-threaded application that the main thread creates the **IOCP** (I/O Completion Port), control our driver using our **IOCTL code**, and runs the rest of the program. while other thread waits for that IOCP attached to our device so when it completes and I/O operation it dequeues it and runs the specified sound file then repeat.

I'm going to write some articles on my [portfolio](https://omarshehata11.github.io/) that describe those new topics I have learned.

## How to run it
First, you need to install the driver as a service using the *sc.exe* tool. Then start it and also start the usermode application. it will print to you the help page to choose :
- 1 -> to send an Irp to the driver
- 2 -> to exit 
### How to change the sound
To change the sound file. just go the user-mode app and in the line **93** put your full path to the file. It's recommended to make the file name with no spaces within it. **And don't forget to put the escape characters when you write the path**.

***NOTE***: You can monitor the output from the driver to see how many Pending Irps and which function is running now..etc. see it from any tool that prints Debugging output like **Dbgview** from *sysinternals*. 

> if you have any question or anything related don't wait to message me :). ***SEE YA***

### Some future changes needed
 - make the user-mode app send Irps automatically.
 - add PeekContext to Irps to distinguish between the different events (plugged-in or out). 
 - signing the driver


