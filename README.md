# MySoundDriver
This is my second software kerne driver project. a WDM non-PnP kernel driver with a user-mode application that used to play a sound when a device was plagged-in or out with the USB-port, you choose by your self the sound file that would be played. 

## The internal structure
In this project I faced a lot of new things. starting first with the driver it self:
- first I signed for PnP notifications with the USB port GUID to make the PnP manager send a notification when there's a device plugged in or out to that port
- I used also the **Inverted Call** model to make the driver can notify the user-mode application when there's an event. So I turned all the incoming Irps from user-mode app into pending state then queued it using the **Cancel-Safe framework** with **LIST_ENTRY** data structure. and when the call-back routine of the PnP notification runs it dequeue one of the queued Irps and complete it. 
- Inside the User-mode application, I made it multi-threaded application that the main thread creates the **IOCP** (I/O Completion Port), control our driver using our **IOCTL code**, and runs the rest of the program. while other thread waits for that IOCP attached to our device so when it completes and I/O operation it dequeues it and runs the specified sound file then repeat.

I'm going to write some articles on my [portfolio](https://omarshehata11.github.io/) that describe those new topics I have learned. 
| if you have any question or anything related don't wait to message me :). ***SEE YA***
