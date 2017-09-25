#ifndef CONST_H
#define CONST_H
/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2011, Tom Hunter
**  C++ adaptation by Dale Sinder 2017
**
**  Name: const.h
**
**  Description:
**      This file defines public constants and macros
**
**  This program is free software: you can redistribute it and/or modify
**  it under the terms of the GNU General Public License version 3 as
**  published by the Free Software Foundation.
**  
**  This program is distributed in the hope that it will be useful,
**  but WITHOUT ANY WARRANTY; without even the implied warranty of
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**  GNU General Public License version 3 for more details.
**  
**  You should have received a copy of the GNU General Public License
**  version 3 along with this program in file "license-gpl-3.0.txt".
**  If not, see <http://www.gnu.org/licenses/gpl-3.0.txt>.
**
**--------------------------------------------------------------------------
*/
/*
**  ----------------
**  Public Constants
**  ----------------
*/
/*
**--------------------------------------------------------------
**	DUAL CPU /Dual Mainframe support
**
**	Note that some sections of code are duplicated for
**	Dual CPU and Dual Mainframe due to concerns with
**	lack of thread safety in C code sections.  It would
**	be nice to make these classes, however, member
**	functions generally can not be used as function
**	pointers which kills the idea here where function
**	pointers in channels and devices abound.
**
**	You can change MaxCpus to 1 or 2 as appropriate.
**	You can change MaxMainFrames to 1 or 2 as appropriate.
**-----------------------------------------------------------
*/

#define	MaxCpus					2		// CPUs per mainframe
#define	MaxMainFrames			1		// Mainframes

/*=========================================================*/

#define DtCyberVersion          "Desktop CYBER++ 5.5.2"
#define DtCyberCopyright        "Copyright (C) Tom Hunter"
#define DtCyberLicense          "Licensed under the terms of the GNU General Public License version 3"
#define DtCyberLicenseDetails	"For details see included text file 'license-gpl-3.0.txt' or visit\n" \
								"'http://www.gnu.org/licenses'"

#ifndef NULL                    
#define NULL                    ((void *) 0)
#endif                          
                                
#ifndef FALSE
#define FALSE                   0
#endif

#ifndef TRUE
#define TRUE                    (!FALSE)
#endif

/*
**  Conditional compiles:
**  ====================
*/

/*
**  Large screen support.
*/
#define CcLargeWin32Screen      1

/*
**  Debug support
*/
#define CcDebug                 0

/*
**  dump_disk operator support
*/
#define CcDumpDisk              0	// DRS


/*
**  Measure cycle time
*/
#define CcCycleTime             0

/*
**  Device types.
*/
#define DtNone                  0
#define DtDeadStartPanel        1
#define DtMt607                 2
#define DtMt669                 3
#define DtDd6603                4
#define DtDd8xx                 5
#define DtCr405                 6
#define DtLp1612                7
#define DtLp5xx                 8
#define DtRtc                   9
#define DtConsole               10
#define DtMux6676               11
#define DtCp3446                12
#define DtCr3447                13
#define DtDcc6681               14
#define DtTpm                   15
#define DtDdp                   16
#define DtNiu                   17
#define DtMt679                 18
#define DtNpu                   19
#define DtMch                   20
#define DtStatusControlRegister 21
#define DtInterlockRegister     22
#define DtPciChannel            23
#define DtMt362x                24

/*
**  Special channels.
*/
#define ChClock                 014
#define ChInterlock             015
#define ChTwoPortMux            015
#define ChStatusAndControl      016
#define ChMaintenance           017

/*
**  Misc constants.
*/

#define PpMemSize               010000
                                
#define MaxUnits                010
#define MaxUnits2               020
#define MaxEquipment            010
#define MaxDeadStart            020
#define MaxChannels             040
#define MaxPpu					024

#define MaxIwStack              12

#define FontLarge               32
#define FontMedium              16
#define FontSmall               8
#define FontDot                 0

#ifndef _MAX_PATH
#define _MAX_PATH                256
#endif

#if CcLargeWin32Screen == 1
#define OffLeftScreen           010
#define OffRightScreen          01100
#else
#define OffLeftScreen           010
#define OffRightScreen          01040
#endif

/*
**  Bit masks.
*/
#define Mask1                   01
#define Mask2                   03
#define Mask3                   07
#define Mask4                   017
#define Mask5                   037
#define Mask6                   077
#define Mask7                   0177
#define Mask8                   0377
#define Mask9                   0777
#define Mask10                  01777
#define Mask11                  03777
#define Mask12                  07777
#define Mask15                  077777
#define Mask17                  0377777
#define Mask18                  0777777
#define Mask21                  07777777
#define Mask24                  077777777
#define Mask24Ecs               077777700
#define Mask30                  07777777777
#define Mask31                  017777777777
#define Mask30Ecs               07777777700
#define Mask48                  000007777777777777777
#define Mask50                  000037777777777777777
#define Mask60                  077777777777777777777
#define MaskCoeff               000007777777777777777
#define MaskExp                 037770000000000000000
#define MaskNormalize           000004000000000000000

/*
**  Trace masks.
*/
//#define TraceCpu                (1 << 30) 
//#define TraceCpu1               (u32)(1 << 31) 
//
//#define TraceExchange           (1 << 29)

#define TraceCpu                (1 << 29) 
#define TraceCpu1               (u32)(1 << 30) 

#define TraceExchange           (1 << 28)

/*
**  Sign extension and overflow.
*/
#define Overflow12              010000

#define Sign18                  0400000
#define Overflow18              01000000

#define Sign21                  04000000
#define Overflow21              010000000

#define Sign24                  040000000
#define Overflow24              0100000000

#define Sign48                  000004000000000000000

#define Sign60                  040000000000000000000
#define Overflow60              0100000000000000000000

#define SignExtend18To60        077777777777777000000

#define NegativeZero            077777777777777777777

/*
**  CPU exit mode and flag bits.
*/
#define EmNone                  00000000
#define EmAddressOutOfRange     00010000
#define EmOperandOutOfRange     00020000
#define EmIndefiniteOperand     00040000

#define EmFlagStackPurge        00200000
#define EmFlagEnhancedBlockCopy 01000000
#define EmFlagExpandedAddress   02000000
#define EmFlagUemEnable         04000000

/*
**  Channel status masks.
*/
#define MaskActive              0x4000 
#define MaskFull                0x2000

/*
**  ----------------------
**  Public Macro Functions
**  ----------------------
*/
#define LogErrorLocation        __FILE__, __LINE__
#if defined (__GNUC__) || defined(__SunOS)
#define stricmp strcasecmp
#endif


// #define isdigit(c) (c >= '0' && c <= '9')

#endif /* CONST_H */
/*---------------------------  End Of File  ------------------------------*/

