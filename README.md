# CppCyber
Desktop Cyber with Dual Mainframe/Dual CPU support - based on work by Tom Hunter

This project was undertaken to accomplish 2 primary goals:

1) Enable DUAL CPU support
2) Enable DUAL MAINFRAME support.

All code was placed in .cpp files.

Several C++ Classes were added to aid in this work:

1) MCpu - Instances represent a single Cyber CPU.
2) Mpp	- Instances represent a single Cyber Peripheral processor.
3) MMainFrame - Instances represent a single mainframe which can support
	up to two CPUs, 20 Peripheral processors, and Central Memory.
4) MSystem - The single instance represents the system as a whole:
	a) Up to two mainframes
	b) Extended Memory
	MSystem also reads the system configuration file "cyber.ini".

A new main entry point is provided in CppCyber.cpp.
These files were removed from the original work as their tasks
were moved elsewhere:

main.c
init.c

A number of other features were added - cyber.ini 
[cyber] section:

1) option to auto remove paper from the 3000 class 
	line printer when a file completes printing: 
	autoRemovePaper=1
2) option to invoke an external program to process
	removed paper.  One might for example email it
	to someone:
	printApp=D:\Applications\CybisRelease1\Mail2.exe
	(A Mail2 program is included for Windows)
3) Option to specify a sub folder in which to place 
	prints:
	printDir=Prints/
4) Option to specify number of CPU words to execute
	per pp instruction executed.  default is 4:
	cpuratio=5
5) Set Windows process priority (default is normal):
	priority=above_normal
	priority=below_normal
	priority=high
6)  Set date and time automatically (year 1998)
	autodate=enter date
7)  Set year with autodateyear - override 1998. eg: 99

If the program has been compiled with 2 mainframe support an
additional parameter is required for each line in the
cyber.ini [equipment] section:  The mainframe to which to
attach the equipment.  The parameter, 0 or 1, goes after 
the channel number.

Example cyber.ini:

;------------------------------------------------------------------------
;
;   Copyright (c) 2016, Tom Hunter (see license.txt)
;   Dual CPU and Dual Mainframe support by Dale Sinder 2017
;
;
;   Name: cyber.ini
;
;   Description:
;       Define emulation and deadstart parameters.
;
;   Supported OSs:
;       NOS 2.8.7 PSR 871 with CYBIS
;
;------------------------------------------------------------------------

;
; NOS 2.8.7 PSR 871	(CYBIS)
;
[cyber]
model=CYBER865
deadstart=deadstart.cybis
equipment=equipment.cybis2mf
npuConnections=npu.cybis
clock=0
memory=4000000
esmbanks=16
pps=24
persistDir=PersistStore
printDir=Prints/
printApp=D:\Applications\CybisRelease1\Mail2.exe
autoRemovePaper=1
cpuratio=4
autodate=enter date
;  trace=017777777777    

[npu.cybis]
; tcp-port,num-conns,type
6610,32,raw
8005,30,pterm
6620,2,rs232

[equipment.cybis]
; single mainframe compile section
; type,eqNo,unitNo,channel,path
DD885,0,0,01,Disks/DM01_SYSTFA
DD885,0,0,02,Disks/DM02_SYSTFA
DD885,0,0,03,Disks/DM03_CYBDEV 
DD885,0,0,04,Disks/DM04_BINARY 
DD885,0,1,01,Disks/DM11_DEV0   
DD885,0,1,02,Disks/DM12_CYB0   
DD885,0,1,03,Disks/DM13_CYB1   
DD885,0,1,04,Disks/DM14_CYB2   
DD885,0,2,01,Disks/DM21_UOL    
DD885,0,2,02,Disks/DM22_PUB0   
DD885,0,2,03,Disks/DM23_PUB1   
DD885,0,2,04,Disks/DM24_CYB3
; DD885,0,3,01,Disks/DM31_DRS
CO6612,0,0,10
LP512,7,0,07,3555
CR3447,7,0,11
CP3446,6,0,11
MT679,0,0,13,Deadstart/CYBIS_15DS.tap 
MT679,0,1,13
MT679,0,2,13
MT679,0,3,13
TPM,0,0,15
NPU,7,0,05
;  

[equipment.cybis2mf]
; dual mainframe compile section
; type,eqNo,unitNo,channel,mainframe,path
DD885,0,0,01,0,Disks/DM01_SYSTFA
DD885,0,0,02,0,Disks/DM02_SYSTFA
DD885,0,0,03,0,Disks/DM03_CYBDEV 
DD885,0,0,04,0,Disks/DM04_BINARY 
DD885,0,1,01,0,Disks/DM11_DEV0   
DD885,0,1,02,0,Disks/DM12_CYB0   
DD885,0,1,03,0,Disks/DM13_CYB1   
DD885,0,1,04,0,Disks/DM14_CYB2   
DD885,0,2,01,0,Disks/DM21_UOL    
DD885,0,2,02,0,Disks/DM22_PUB0   
DD885,0,2,03,0,Disks/DM23_PUB1   
DD885,0,2,04,0,Disks/DM24_CYB3
; DD885,0,3,01,0,Disks/DM31_DRS
CO6612,0,0,10,0
LP512,7,0,07,0,3555
CR3447,7,0,11,0
CP3446,6,0,11,0
MT679,0,0,13,0,Deadstart/CYBIS_15DS.tap 
MT679,0,1,13,0
MT679,0,2,13,0
MT679,0,3,13,0
TPM,0,0,15,0
NPU,7,0,05,0
;  
DD885,0,0,01,1,Disks/DM01_SYSTFA
DD885,0,0,01,1,Disks/DM01_SYSTFA
DD885,0,0,02,1,Disks/DM02_SYSTFA
DD885,0,0,03,1,Disks/DM03_CYBDEV
DD885,0,0,04,1,Disks/DM04_BINARY
DD885,0,1,01,1,Disks/DM11_DEV0
DD885,0,1,02,1,Disks/DM12_CYB0
DD885,0,1,03,1,Disks/DM13_CYB1
DD885,0,1,04,1,Disks/DM14_CYB2
DD885,0,2,01,1,Disks/DM21_UOL
DD885,0,2,02,1,Disks/DM22_PUB0
DD885,0,2,03,1,Disks/DM23_PUB1
DD885,0,2,04,1,Disks/DM24_CYB3
CO6612,0,1,10,1
MT679,0,0,13,1,Deadstart/CYBIS_15DS-1.tap
MT679,0,1,13,1
MT679,0,2,13,1
MT679,0,3,13,1
LP512,7,0,07,1,3555
CR3447,7,0,11,1
CP3446,6,0,11,1
TPM,0,0,15,1
NPU,7,0,05,1

[deadstart.cybis]
0000
0000
0000
7553 DCN 13
7713 FCN 13, 
0120        0120 for ATS
7413 ACN 13
7113 IAM 13,
7301        7301
0000
1001 wxyy w=level, x=display, yy=cmrdeck
0000

---------------------------------------------------------------------

Channels, Devices, and PPUs have been made "mainframe aware" by placing 
both a mainfram ID and a mainframe object pointer in the Channel Slot 
and Device Slot as well as the Mpp objects structures.

The single MSystem object is called BigIron and is globally accessible.

While Extended Memory is in the MSytem object, a pointer to it is
placed in each MCpu object at start up.  This provides for quick
access and minimized code changes needed.

Likewise MMainFrame objects hold CM, but a pointer to it is also
copied to each MCpu object at startup.  This provides for quick
access and minimized code changes needed.

