Sync Mode
----------

Using SYNC mode (synchronize buttons on two SwitchMotes):

* Conect the sync wire to ground on switch you want to be controlled (the slave)
* Select the button mode (ON or OFF) by pressing the controlled button. Press the controlled button for 3 seonds. Now the switch in SYNC mode. Mode is ON when LED Duty Cycle is above 80% and OFF below 20%
* walk to the Mote_switch from which you want to control and ground the sync button and then press 3 seconds the button you want to be the controller (the master). At this point the master will broadcast a SYNC? token and the slave will respond with a SYNCx:y token where x=button placed in SYNC, y=mode of button placed in SYNC