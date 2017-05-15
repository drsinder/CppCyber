#ifndef NPU_H
#define NPU_H
/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2011, Tom Hunter
**
**  Name: npu.h
**
**  Description:
**      This file defines NPU constants, types, variables, macros and
**      function prototypes.
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
**  -------------
**  NPU Constants
**  -------------
*/

/*
**  Block offsets.
*/
#define BlkOffDN        0
#define BlkOffSN        1
#define BlkOffCN        2
#define BlkOffBTBSN     3
#define BlkOffData      4
#define BlkOffDbc       4
#define BlkOffPfc       4
#define BlkOffSfc       5
#define BlkOffP3        6
#define BlkOffP4        7

#define BlkShiftBT      0
#define BlkMaskBT       017

#define BlkShiftBSN     4
#define BlkMaskBSN      7

#define BlkShiftPRIO    7
#define BlkMaskPRIO     1
                   
/*
**  Block types
*/
#define BtHTBLK         0x1     // Block type                
#define BtHTMSG         0x2     // Message type              
#define BtHTBACK        0x3     // Back type                 
#define BtHTCMD         0x4     // Command type              
#define BtHTBREAK       0x5     // Break type                
#define BtHTQBLK        0x6     // Qualified block type      
#define BtHTQMSG        0x7     // Qualified message type    
#define BtHTRESET       0x8     // Reset type                
#define BtHTRINIT       0x9     // Request initialize type   
#define BtHTNINIT       0xA     // Initialize response type  
#define BtHTTERM        0xB     // Terminate type            
#define BtHTICMD        0xC     // Interrupt command         
#define BtHTICMR        0xD     // Interrupt command response

/*
**  Secondary function flag bits.
*/
#define SfcReq          (0 << 6)    // Request
#define SfcResp         (1 << 6)    // Normal response
#define SfcErr          (2 << 6)    // Abnormal response
                        
/*                      
**  TIP types           
*/                      
#define TtASYNC         1       // ASYNC TIP
#define TtMODE4         2       // MODE 4 TIP
#define TtHASP          3       // HASP TIP

/*
**  Device types
*/
#define DtCONSOLE       0       // Normal terminal
#define DtCR            1       // Card reader
#define DtLP            2       // Line printer
#define DtCP            3       // Card punch
#define DtPLOTTER       4       // Plotter
                        
/*                      
**  Line speed codes    
*/                      
#define LsNA            0       //  N/A (i.e. synchronous)
#define Ls110           1       //  110
#define Ls134           2       //  134.5
#define Ls150           3       //  150
#define Ls300           4       //  300
#define Ls600           5       //  600
#define Ls1200          6       //  1200
#define Ls2400          7       //  2400
#define Ls4800          8       //  4800
#define Ls9600          9       //  9600
#define Ls19200         10      //  19200
#define Ls38400         11      //  38400
                        
/*                      
**  Code set codes      
*/                      
#define CsBCD           1       //  BCD - MODE 4A BCD
#define CsASCII         2       //  ASCII - ASCII for ASYNC or MODE 4A ASCII
#define CsMODE4C        3       //  MODE 4C
#define CsTYPPAPL       3       //  Typewriter-paired APL-ASCII
#define CsBITPAPL       4       //  Bit-paired APL-ASCII
#define CsEBCDAPL       5       //  EBCD
#define CsEAPLAPL       6       //  EBCD APL
#define CsCORR          7       //  Correspondence
#define CsCORAPL        8       //  Correspondence APL
#define CsEBCDIC        9       //  EBCDIC

/*
**  Terminal class
*/
#define TcNA            0       //  N/A
#define TcM33           1       //  ASYNC  - M33,M35,M37,M38         
#define Tc713           2       //         - CDC 713, 751-1, 752, 756
#define Tc721           3       //         - CDC 721                 
#define Tc2741          4       //         - IBM 2741                
#define TcM40           5       //         - M40                     
#define TcH2000         6       //         - HAZELTINE 2000          
#define TcX364          7       //         - X3.64 type terminals (ANSI e.g. VT100)   
#define TcT4014         8       //         - TEXTRONIX 4014          
#define TcHASP          9       //  HASP   - POST                    
#define Tc200UT         10      //  MODE 4 - 200UT, 214              
#define Tc71430         11      //         - 714-30                  
#define Tc711           12      //         - 711-10                  
#define Tc714           13      //         - 714                     
#define TcHPRE          14      //  HASP   - PRE                     
#define Tc734           15      //         - 734                     
#define Tc2780          16      //  BSC 2780                         
#define Tc3780          17      //  BSC 3780                         
#define Tc327E          18      //  3270 EBCDIC                      
#define TcTCOUPLER      19      //  Coupler                          
#define TcTCONSOLE      20      //  Console                          
#define TcTHDLC         21      //  HDLC LIP                         
#define TcTDIAG         22      //  On-line diagnostics              
#define TcSYNAUTO       23      //  SYNC AUTO                        
#define TcUTC1          28      //  User terminal class 1            
#define TcUTC2          29      //  User terminal class 2            
#define TcUTC3          30      //  User terminal class 3            
#define TcUTC4          31      //  User terminal class 4            

/*
**  Data Block Clarifier.
*/
#define DbcNoCursorPos  0x10    // Cursor pos off next input
#define DbcNoFe         0x08    // Disable Format Effectors - display FE byte as data
#define DbcTransparent  0x04    // Transparent data
#define DbcEchoplex     0x02    // Echoplex off next input
#define DbcCancel       0x02    // Cancel upline message

/*
**  Source and Destination node addresses.
**  (For now these are hardcoded).
**
**  "Host" address is the Coupler Node address given in the NDL file.
**  It is also the ND= parameter of the equipment entry in EQPDECK
**  for the NPU.
**
**  "Npu" address is the NPU Node address given in the NDL file.
*/
#define AddrHost        1
#define AddrNpu         2

/*
**  NPU connection types.
*/
#define ConnTypeRaw     0
#define ConnTypePterm   1
#define ConnTypeRs232   2
#define MaxConnTypes    3

/*
**  npuNetRegister() return codes
*/
#define NpuNetRegOk     0
#define NpuNetRegOvfl   1
#define NpuNetRegDupl   2

/*
**  Miscellaneous constants.
*/
#define MaxBuffer       2048

/*
**  Character definitions.
*/
#define ChrNUL          0x00    // Null
#define ChrSTX          0x02    // Start text
#define ChrEOT          0x04    // End of transmission
#define ChrBEL          0x07    // Bell
#define ChrBS           0x08    // Backspace
#define ChrTAB          0x09    // Tab
#define ChrLF           0x0A    // Line feed
#define ChrFF           0x0C    // Form feed
#define ChrCR           0x0D    // Carriage return
#define ChrDC1          0x11    // DC1 (X-ON)
#define ChrDC3          0x13    // DC3 (X-OFF)
#define ChrESC          0x1B    // Escape
#define ChrUS           0x1f    // End of record marker
#define ChrDEL          0x7F    // Del

/*
**  NPU buffer flags
*/
#define NpuBufNeedsAck  0x01

/*
**  -------------------
**  NPU Macro Functions
**  -------------------
*/

/*
**  --------------------
**  NPU Type Definitions
**  --------------------
*/

/*
**  NPU buffers.
*/
typedef struct npuBuffer
    {
    struct npuBuffer    *next;
    u16                 offset;
    u16                 numBytes;
    u8                  blockSeqNo;
    u8                  data[MaxBuffer];
    } NpuBuffer;

/*
**  NPU buffer queue.
*/
typedef struct npuQueue
    {
    struct npuBuffer    *first;
    struct npuBuffer    *last;
    } NpuQueue;

/*
**  TIP parameters.
*/
typedef struct tipParams
    {
    u8                  fvAbortBlock;     // Abort block character
    u8                  fvBlockFactor;    // Blocking factor; multiple of 100 chars (upline block)
    bool                fvBreakAsUser;    // Break as user break 1; yes (1), no (0)
    u8                  fvBS;             // Backspace character
    u8                  fvUserBreak1;     // User break 1 character
    u8                  fvUserBreak2;     // User break 2 character
    bool                fvEnaXUserBreak;  // Enable transparent user break commands; yes (1), no (0)
    u8                  fvCI;             // Carriage return idle count
    bool                fvCIAuto;         // Carriage return idle count - TIP calculates suitable number
    u8                  fvCN;             // Cancel character
    bool                fvCursorPos;      // Cursor positioning; yes (1), no (0)
    u8                  fvCT;             // Network control character
    bool                fvXCharFlag;      // Transparent input delimiter character specified flag; yes (1), no (0)
    u16                 fvXCnt;           // Transparent input delimiter count
    u8                  fvXChar;          // Transparent input delimiter character
    bool                fvXTimeout;       // Transparent input delimiter timeout; yes (1), no (0)
    bool                fvXModeMultiple;  // Transparent input mode; multiple (1), singe (0)
    u8                  fvEOB;            // End of block character
    u8                  fvEOBterm;        // End of block terminator; EOL (1), EOB (2)
    u8                  fvEOBCursorPos;   // EOB cursor pos; no (0), CR (1), LF (2), CR & LF (3)
    u8                  fvEOL;            // End of line character
    u8                  fvEOLTerm;        // End of line terminator; EOL (1), EOB (2)
    u8                  fvEOLCursorPos;   // EOL cursor pos; no (0), CR (1), LF (2), CR & LF (3)
    bool                fvEchoplex;       // Echoplex mode
    bool                fvFullASCII;      // Full ASCII input; yes (1), no (0)
    bool                fvInFlowControl;  // Input flow control; yes (1), no (0)
    bool                fvXInput;         // Transparent input; yes (1), no (0)
    u8                  fvInputDevice;    // Keyboard (0), paper tape (1), block mode (2)
    u8                  fvLI;             // Line feed idle count
    bool                fvLIAuto;         // Line feed idle count - TIP calculates suitable number
    bool                fvLockKeyboard;   // Lockout unsolicited input from keyboard
    bool                fvOutFlowControl; // Output flow control; yes (1), no (0)
    u8                  fvOutputDevice;   // Printer (0), display (1), paper tape (2)
    u8                  fvParity;         // Zero (0), odd (1), even (2), none (3), ignore (4)
    bool                fvPG;             // Page waiting; yes (1), no (0)
    u8                  fvPL;             // Page length in lines; 0, 8 - FF
    u8                  fvPW;             // Page width in columns; 0, 20 - FF
    bool                fvSpecialEdit;    // Special editing mode; yes (1), no (0)
    u8                  fvTC;             // Terminal class
    bool                fvXStickyTimeout; // Sticky transparent input forward on timeout; yes (1), no (0)
    u8                  fvXModeDelimiter; // Transparent input mode delimiter character
    bool                fvDuplex;         // full (1), half (0)
    u16                 fvTermTransBs;    // Terminal transmission block size
    bool                fvSolicitInput;   // yes (1), no (0)
    u8                  fvCIDelay;        // Carriage return idle delay in 4 ms increments
    u8                  fvLIDelay;        // Line feed idle delay in 4 ms increments

    u8                  fvHostNode;       // Selected host node
    bool                fvAutoConnect;    // yes (1), no (0)
    u8                  fvPriority;       // Terminal priority
    u8                  fvUBL;            // Upline block count limit
    u8                  fvABL;            // Application block count limit
    u8                  fvDBL;            // Downline block count limit
    u16                 fvDBSize;         // Downline block size
    u8                  fvRestrictedRBF;  // Restriced capacity RBF

    } TipParams;

/*
**  Terminal connection state.
*/
typedef enum
    {
    StTermIdle = 0,
    StTermNetConnected,
    StTermRequestConfig,
    StTermRequestConnection,
    StTermHostConnected,
    StTermNpuDisconnect,
    StTermHostDisconnect,
    } TermConnState;


/*
**  Terminal control block.
*/
typedef struct tcb
    {
    /*
    **  Connection state.
    */
    TermConnState       state;
    u8                  portNumber;
    bool                active;
    bool                hostDisconnect;
    bool                breakPending;
    int                 connFd;
    u8                  connType;

    /*
    **  Configuration.
    */
    bool                enabled;
    char                termName[7];
    u8                  tipType;
    u8                  subTip;
    u8                  deviceType;
    u8                  codeSet;

    /*
    **  Active TIP parameters.
    */
    TipParams           params;

    /*
    **  Input state.
    */
    u8                  uplineBsn;

    u8                  inputData[100];
    int                 inputCount;

    u8                  inBuf[MaxBuffer];
    u8                  *inBufPtr;
    u8                  *inBufStart;

    bool                xInputTimerRunning;
    u32                 xStartCycle;

    /*
    **  Output state.
    */
    NpuQueue            outputQ;
    bool                xoff;
    bool                dbcNoEchoplex;
    bool                dbcNoCursorPos;
    bool                lastOpWasInput;
    } Tcb;

/*
**  -----------------------
**  NPU Function Prototypes
**  -----------------------
*/

/*
**  npu_hip.c
*/
bool npuHipUplineBlock(NpuBuffer *bp);
bool npuHipDownlineBlock(NpuBuffer *bp);
void npuLogMessage(char *format, ...);

/*
**  npu_bip.c
*/
void npuBipInit(void);
void npuBipReset(void);
NpuBuffer *npuBipBufGet(void);
void npuBipBufRelease(NpuBuffer *bp);
void npuBipQueueAppend(NpuBuffer *bp, NpuQueue *queue);
void npuBipQueuePrepend(NpuBuffer *bp, NpuQueue *queue);
NpuBuffer *npuBipQueueExtract(NpuQueue *queue);
NpuBuffer *npuBipQueueGetLast(NpuQueue *queue);
bool npuBipQueueNotEmpty(NpuQueue *queue);
void npuBipNotifyServiceMessage(void);
void npuBipNotifyData(int priority);
void npuBipRetryInput(void);
void npuBipNotifyDownlineReceived(void);
void npuBipAbortDownlineReceived(void);
void npuBipRequestUplineTransfer(NpuBuffer *bp);
void npuBipRequestUplineCanned(u8 *msg, int msgSize);
void npuBipNotifyUplineSent(void);

/*
**  npu_svm.c
*/
void npuSvmInit(void);
void npuSvmReset(void);
void npuSvmNotifyHostRegulation(u8 regLevel);
void npuSvmProcessBuffer(NpuBuffer *bp);
bool npuSvmConnectTerminal(Tcb *tp);
void npuSvmDiscRequestTerminal(Tcb *tp);
void npuSvmDiscReplyTerminal(Tcb *tp);
bool npuSvmIsReady(void);

/*
**  npu_tip.c
*/
void npuTipInit(void);
void npuTipReset(void);
void npuTipProcessBuffer(NpuBuffer *bp, int priority);
void npuTipTerminateConnection(Tcb *tp);
void npuTipSetupTerminalClass(Tcb *tp, u8 tc);
bool npuTipParseFnFv(u8 *mp, int len, Tcb *tp);
void npuTipInputReset(Tcb *tp);
void npuTipSendUserBreak(Tcb *tp, u8 bt);
void npuTipDiscardOutputQ(Tcb *tp);
void npuTipNotifySent(Tcb *tp, u8 blockSeqNo);

/*
**  npu_net.c
*/
int npuNetRegister(int tcpPort, int numConns, int connType);
void npuNetInit(bool startup);
void npuNetReset(void);
void npuNetConnected(Tcb *tp);
void npuNetDisconnected(Tcb *tp);
void npuNetSend(Tcb *tp, u8 *data, int len);
void npuNetQueueAck(Tcb *tp, u8 blockSeqNo);
void npuNetCheckStatus(void);

/*
**  npu_async.c
*/
void npuAsyncProcessDownlineData(u8 cn, NpuBuffer *bp, bool last);
void npuAsyncProcessUplineData(Tcb *tp);
void npuAsyncFlushUplineTransparent(Tcb *tp);

/*
**  --------------------
**  Global NPU variables
**  --------------------
*/
extern Tcb *npuTcbs;
extern int npuTcbCount;

#endif /* NPU_H */
/*---------------------------  End Of File  ------------------------------*/

