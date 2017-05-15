#ifndef PROTO_H
#define PROTO_H
/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2011, Tom Hunter
**  C++ adaptation by Dale Sinder 2017
**
**  Name: proto.h
**
**  Description:
**      This file defines external function prototypes and variables.
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
**  -------------------------------------------------
**  Macros for use with Mutex and Condition Variables
**  -------------------------------------------------
**	These should map easily to the unix versions also
**	with some conditional compiles.
*/


#if MaxCpus == 2 || MaxMainFrames  == 2
#define RESERVE(x) if (BigIron->initCpus > 1 || BigIron->initMainFrames > 1)	\
					{															\
					EnterCriticalSection(x);									\
					}

#define RELEASE(x) if (BigIron->initCpus > 1 || BigIron->initMainFrames > 1)	\
					{															\
						LeaveCriticalSection(x);								\
					}

#define RESERVE1(x)			EnterCriticalSection(x)
#define RELEASE1(x)			LeaveCriticalSection(x)
#define INIT_MUTEX(x, y)	InitializeCriticalSectionAndSpinCount(x, y)
#define INIT_COND_VAR(x)	InitializeConditionVariable(x)
#else
#define RESERVE(x)	
#define RELEASE(x)	
#define RESERVE1(x)	
#define RELEASE1(x)
#define INIT_MUTEX(x, y)
#define INIT_COND_VAR(x)
#endif

/*
**  --------------------
**  Function Prototypes.
**  --------------------
*/

/*
**  deadstart.c
*/
void deadStart(u8 k);

/*
**  rtc.c
*/
void rtcInit(u8 increment, u32 setMHz, u8 mfrID);
void rtcTick(void);
void rtcStartTimer(void);
double rtcStopTimer(void);
void rtcReadUsCounter(void);

/*
**  channel.c
*/
ChSlot *channelInit(u8 count, MMainFrame *mfr);
void channelTerminate(u8 mfrID);
DevSlot *channelFindDevice(u8 channelNo, u8 devType, u8 mfrID);
DevSlot *channelAttach(u8 channelNo, u8 eqNo, u8 devType, u8 mfrID);
void channelFunction(PpWord funcCode);
void channelActivate(void);
void channelDisconnect(void);
void channelIo(void);
void channelCheckIfActive(void);
void channelCheckIfFull(void);
void channelOut(void);
void channelIn(void);
void channelSetFull(void);
void channelSetEmpty(void);
void channelStep(u8 mfrID);

/*
**  mt362x.c
*/
void mt362xInit_7(u8 mfrID, u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName);
void mt362xInit_9(u8 mfrID, u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName);
void mt362xLoadTape(char *params);
void mt362xUnloadTape(char *params);
void mt362xShowTapeStatus(void);

/*
**  mt607.c
*/
void mt607Init(u8 mfrID, u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName);

/*
**  mt669.c
*/
void mt669Init(u8 mfrID, u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName);
void mt669Terminate(DevSlot *dp);
void mt669LoadTape(char *params);
void mt669UnloadTape(char *params);
void mt669ShowTapeStatus(void);

/*
**  mt679.c
*/
void mt679Init(u8 mfrID, u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName);
void mt679Terminate(DevSlot *dp);
void mt679LoadTape(char *params);
void mt679UnloadTape(char *params);
void mt679ShowTapeStatus(void);

/*
**  cr405.c
*/
void cr405Init(u8 mfrID, u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName);
void cr405LoadCards(char *params);

/*
**  cp3446.c
*/
void cp3446Init(u8 mfrID, u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName);
void cp3446RemoveCards(char *params);

/*
**  cr3447.c
*/
void cr3447Init(u8 mfrID, u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName);
void cr3447LoadCards(char *params);

/*
**  lp1612.c
*/
void lp1612Init(u8 mfrID, u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName);
void lp1612RemovePaper(char *params);

/*
**  lp3000.c
*/
void lp501Init(u8 mfrID, u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName);
void lp512Init(u8 mfrID, u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName);
void lp3000RemovePaper(char *params);

/*
**  console.c
*/
void consoleInit(u8 mfrID, u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName);

/*
**  dd6603.c
*/
void dd6603Init(u8 mfrID, u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName);

/*
**  dd8xx.c
*/
void dd844Init_2(u8 mfrID, u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName);
void dd844Init_4(u8 mfrID, u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName);
void dd885Init_1(u8 mfrID, u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName);
void dd885Dump(char *cmdParams);

#if CcDumpDisk == 1
void dd8xxDumpDisk(char *params);		// DRS
#endif
/*
**  dcc6681.c
*/
void dcc6681Terminate(DevSlot *dp);
void dcc6681Interrupt(bool status);

/*
**  ddp.c
*/
void ddpInit(u8 mfrID, u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName);

/*
**  mux6676.c
*/
void mux6676Init(u8 mfrID, u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName);

/*
**  npu.c
*/
void npuInit(u8 mfrID, u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName);
int npuBipBufCount(void);

/*
**  pci_channel_{win32,linux}.c
*/
void pciInit(u8 mfrID, u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName);

/*
**  pci_console_linux.c
*/
void pciConsoleInit(u8 mfrID, u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName);

/*
**  tpmux.c
*/
void tpMuxInit(u8 mfrID, u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName);

/*
**  maintenance_channel.c
*/
void mchInit(u8 mfrID, u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName);

/*
**  scr_channel.c
*/
void scrInit(u8 channelNo, u8 mfrID);

/*
**  interlock_channel.c
*/
void ilrInit(u8 registerSize, u8 mfrID);

/*
**  trace.c
*/
void traceInit(void);
void traceTerminate(void);
void traceSequence(u8 mfrID);
void traceRegisters(u8 mfrID);
void traceOpcode(u8 mfrID);
u8 traceDisassembleOpcode(char *str, PpWord *pm);
void traceChannelFunction(PpWord funcCode);
void tracePrint(char *str, u8 mfrID);
void traceCpuPrint(MCpu *cc, char *str);
void traceChannel(u8 ch, u8 mfrID);
void traceEnd(u8 mfrID);
void traceCpu(MCpu *cc, u32 p, u8 opFm, u8 opI, u8 opJ, u8 opK, u32 opAddress);
void traceExchange(MCpu *cc, u32 addr, char *title, char *xjSource);

/*
**  dump.c
*/
void dumpInit(void);
void dumpTerminate(void);
void dumpAll(void);
void dumpCpu(u8 mfrID);
void dumpPpu(u8 pp, u8 mfrID);
void dumpDisassemblePpu(u8 pp);
void dumpRunningPpu(u8 pp);
void dumpRunningCpu(u8 mfrID);

/*
**  float.c
*/
CpWord floatAdd(CpWord v1, CpWord v2, bool doRound, bool doDouble);
CpWord floatMultiply(CpWord v1, CpWord v2, bool doRound, bool doDouble);
CpWord floatDivide(CpWord v1, CpWord v2, bool doRound);

/*
**  shift.c
*/
CpWord shiftLeftCircular(CpWord data, u32 count);
CpWord shiftRightArithmetic(CpWord data, u32 count);
CpWord shiftPack(CpWord coeff, u32 expo);
CpWord shiftUnpack(CpWord number, u32 *expo);
CpWord shiftNormalize(CpWord number, u32 *shift, bool round);
CpWord shiftMask(u8 count);

/*
**  window_{win32,x11}.c
*/
void windowInit(u8 mfrID);
void windowSetFont(u8 font);
void windowSetX(u16 x);
void windowSetY(u16 y);
void windowQueue(u8 ch);
void windowUpdate(void);
void windowGetChar(void);
void windowTerminate(void);

void windowSetFont1(u8 font);
void windowSetX1(u16 x);
void windowSetY1(u16 y);
void windowQueue1(u8 ch);
void windowUpdate1(void);
void windowGetChar1(void);
void windowTerminate1(void);

/*
**  operator.c
*/
void opInit(void);
void opRequest(void);

/*
**  log.c
*/
void logInit(void);
void logError(char *file, int line, char *fmt, ...);



/*
**  -----------------
**  Global variables.
**  -----------------
*/

/*
** Tried to put most things in the one and only MSystem instance.
*/
extern MSystem *BigIron;

/*
** The rest of these were harder and left for now.
*/

extern PpSlot *activePpu;
extern ChSlot *activeChannel;
extern DevSlot *activeDevice;
extern DevSlot *active3000Device;

extern u32 traceMask;
extern u32 traceSequenceNo;

extern DevDesc deviceDesc[];
extern u8 deviceCount;

extern volatile bool opActive;
extern u16 mux6676TelnetPort;
extern u16 mux6676TelnetConns;
extern u16 npuNetTcpConns;

extern u32 features;
extern u32 rtcClock;
extern double clockx;

extern bool autoDate;		// enter date/time automatically - year 98
extern bool autoDate1;		// enter date/time automatically - year 98
extern char autoDateString[40];

extern char persistDir[];
extern char printDir[];
extern char printApp[];

/*
** Charset translation maps  - charset.cpp
*/
extern const u8 asciiToCdc[256];
extern const char cdcToAscii[64];
extern const u8 asciiToConsole[256];
extern const char consoleToAscii[64];
extern const u16 asciiTo026[256];
extern const u16 asciiTo029[256];
extern const u8  asciiToBcd[256];
extern const char bcdToAscii[64];
extern const char extBcdToAscii[64];
extern const i8 asciiToPlato[128];
extern const i8 altKeyToPlato[128];



#endif /* PROTO_H */
/*---------------------------  End Of File  ------------------------------*/

