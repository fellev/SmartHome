Sync Mode
----------

Using SYNC mode (synchronize buttons on two SwitchMotes):

* Press the sync button on Mote_switch you want to be controlled (the slave), LEDs for that button will start flashing rapidly. 
* Select the button mode (ON or OFF) by pressing the controlled button. Mode is ON when LED Duty Cycle is above 80% and OFF if 20%
* walk to the Mote_switch from which you want to control and press sync button and then the button you want to be the controller (the master). At this point the master will broadcast a SYNC? token and the slave will respond with a SYNCx:y token where x=button placed in SYNC, y=mode of button placed in SYNC