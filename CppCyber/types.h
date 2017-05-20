#ifndef TYPES_H
#define TYPES_H
/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2011, Tom Hunter
**  C++ adaptation by Dale Sinder 2017
**
**  Name: types.h
**
**  Description:
**      This file defines global types.
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
**  -----------------------
**  Public Type Definitions
**  -----------------------
*/

/*
**  Basic types used in emulation.
*/
#if defined(_WIN32)
    /*
    **  MS Win32 systems
    */
    typedef signed char  i8;
    typedef signed short i16;
    typedef signed long  i32;
    typedef signed __int64 i64;
    typedef unsigned char  u8;
    typedef unsigned short u16;
    typedef unsigned long  u32;
    typedef unsigned __int64 u64;
    #define FMT60_020o "%020I64o"
#elif defined (__GNUC__) || defined(__SunOS)
    #if defined(__amd64) || defined(__amd64__) || defined(__alpha__) || defined(__powerpc64__) || defined(__ppc64__) \
        || (defined(__sparc64__) && defined(__arch64__))
        /*
        **  64 bit systems
        */
        typedef signed char i8;
        typedef signed short i16;
        typedef signed int i32;
        typedef unsigned long int i64;
        typedef unsigned char u8;
        typedef unsigned short u16;
        typedef unsigned int u32;
        typedef unsigned long int u64;
        #define FMT60_020o "%020lo"
    #elif defined(__i386) || defined(__i386__) || defined(__powerpc__) || defined(__ppc__) \
        || defined(__sparc__) || defined(__hppa__) || defined(__APPLE__)
        /*
        **  32 bit systems
        */
        typedef signed char i8;
        typedef signed short i16;
        typedef signed int i32;
        typedef unsigned long long int i64;
        typedef unsigned char u8;
        typedef unsigned short u16;
        typedef unsigned int u32;
        typedef unsigned long long int u64;
        #define FMT60_020o "%020llo"
    #else
        #error "Unable to determine size of basic data types"
    #endif
#else
    #error "Unable to determine size of basic data types"
#endif

#if (!defined(__cplusplus) && !defined(bool) && !defined(CURSES) && !defined(CURSES_H) && !defined(_CURSES_H))
    typedef int bool;
#endif

#if defined(__APPLE__)
    #include <stdbool.h>
#endif

typedef u16 PpWord;                     /* 12 bit PP word */
typedef u8 PpByte;                      /* 6 bit PP word */
typedef u64 CpWord;                     /* 60 bit CPU word */

/*
**  Function code processing status.
*/
typedef enum {FcDeclined, FcAccepted, FcProcessed} FcStatus;

/*
**	for calling C++ member functions from pointers
*/
#define CALL_MEMBER_FN(object,ptrToMember)  ((object).*(ptrToMember))

/*
**  Device descriptor.
*/                                        
typedef struct
    {
    char            id[10];              /* device id */
    void            (*init)(u8 mfrID, u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName);
    } DevDesc;

/*
**  Device control block.
*/                                        
typedef struct devSlot                  
    {                                   
    struct devSlot  *next;              /* next device attached to this channel or converter */
    struct chSlot   *channel;           /* channel this device is attached to */
    FILE            *fcb[MaxUnits2];    /* unit data file control block */
    void            (*activate)(u8);  /* channel activation function */        
    void            (*disconnect)(u8);/* channel deactivation function */
    FcStatus        (*func)(PpWord, u8);    /* function request handler */
    void            (*io)(u8);        /* I/O request handler */
    PpWord          (*in)();        /* PCI channel input request */
    void            (*out)(PpWord);     /* PCI channel output request */
    void            (*full)();      /* PCI channel full request */
    void            (*empty)();     /* PCI channel empty request */
    u16             (*flags)();     /* PCI channel flags request */
    void            *context[MaxUnits2];/* device specific context data */
    void            *controllerContext; /* controller specific context data */
    PpWord          status;             /* device status */
    PpWord          fcode;              /* device function code */
    PpWord          recordLength;       /* length of read record */
    u8              devType;            /* attached device type */
    u8              eqNo;               /* equipment number */
    i8              selectedUnit;       /* selected unit */
	u8				mfrID;				/* mainframe ID*/
	class MMainFrame		*mfr;		/* MainFrame */		
} DevSlot;
                                        
/*
**  Channel control block.
*/                                        
typedef struct chSlot                          
    {                                   
    DevSlot         *firstDevice;       /* linked list of devices attached to this channel */
    DevSlot         *ioDevice;          /* device which deals with current function */
    PpWord          data;               /* channel data */
    PpWord          status;             /* channel status */
    bool            active;             /* channel active flag */
    bool            full;               /* channel full flag */
    bool            discAfterInput;     /* disconnect channel after input flag */
    bool            flag;               /* optional channel flag */
    bool            inputPending;       /* input pending flag */
    bool            hardwired;          /* hardwired devices */
    u8              id;                 /* channel number */
    u8              delayStatus;        /* time to delay change of empty/full status */
    u8              delayDisconnect;    /* time to delay disconnect */
	u8				mfrID;				/* mainframe ID*/
	class MMainFrame		*mfr;		/* MainFrame */
} ChSlot;
                                        
/*
**  PPU control block.
*/                                        
typedef struct                          
    {                                   
    u32             regA;               /* register A (18 bit) */
    u32             regR;               /* register R (28 bit) */
    PpWord          regP;               /* program counter (12 bit) */
    PpWord          regQ;               /* register Q (12 bit) */
    PpWord          mem[PpMemSize];     /* PP memory */
    bool            busy;               /* instruction execution state */
    u8              id;                 /* PP number */
    PpByte          opF;                /* current opcode */
    PpByte          opD;                /* current opcode */

    } PpSlot;                           

/*
**  CPU control block.
*/                                        
typedef struct                          
    {                                   
    CpWord          regX[010];          /* data registers (60 bit) */
    u32             regA[010];          /* address registers (18 bit) */
    u32             regB[010];          /* index registers (18 bit) */
    u32             regP;               /* program counter */
    u32             regRaCm;            /* reference address CM */
    u32             regFlCm;            /* field length CM */
    u32             regRaEcs;           /* reference address ECS */
    u32             regFlEcs;           /* field length ECS */
    u32             regMa;              /* monitor address */
    u32             regSpare;           /* reserved */
    u32             exitMode;           /* CPU exit mode (24 bit) */
    u8              exitCondition;      /* recorded exit conditions since XJ */
	
	bool			cpuStopped = true;

    /*
    **  Instruction word stack.
    */
    CpWord          iwStack[MaxIwStack];
    u32             iwAddress[MaxIwStack];
    bool            iwValid[MaxIwStack];
    u8              iwRank;

	u8				CpuID;				/* CPU ID for DUAL CPU systems 0 and 1 */

    } CpuContext;

/*
**  Model specific feature set.
*/
typedef enum
    {
    HasInterlockReg         = 0x00000001,
    HasStatusAndControlReg  = 0x00000002,
    HasMaintenanceChannel   = 0x00000004,
    HasTwoPortMux           = 0x00000008,
    HasChannelFlag          = 0x00000010,
    HasErrorFlag            = 0x00000020,
    HasRelocationRegShort   = 0x00000040,
    HasRelocationRegLong    = 0x00000080,
    HasRelocationReg        = 0x000000C0,
    HasMicrosecondClock     = 0x00000100,
    HasInstructionStack     = 0x00000200,
    HasIStackPrefetch       = 0x00000400,
    HasCMU                  = 0x00000800,
    HasFullRTC              = 0x00001000,
    HasNoCmWrap             = 0x00002000,
    HasNoCejMej             = 0x00004000,
    Has175Float             = 0x00008000,

    IsSeries6x00            = 0x01000000,
    IsSeries70              = 0x02000000,
    IsSeries170             = 0x04000000,
    IsSeries800             = 0x08000000,
    } ModelFeatures;

typedef enum
    {
    Model6400,
    ModelCyber73,
    ModelCyber173,
    ModelCyber175,
    ModelCyber840A,
    ModelCyber865,
    } ModelType;

typedef enum
    {
    ECS,
    ESM
    } ExtMemory;

#endif /* TYPES_H */
/*---------------------------  End Of File  ------------------------------*/

