/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2011, Tom Hunter, Paul Koning
**  C++ adaptation by Dale Sinder 2017
**
**  Name: npu_hip.cpp
**
**  Description:
**      Perform emulation of the Host Interface Protocol in an NPU
**      consisting of a CDC 2550 HCP running CCP.
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
**  Include Files
**  -------------
*/
#include "stdafx.h"
#define DEBUG 0

/*
**  -----------------
**  Private Constants
**  -----------------
*/

/*
**  Function codes.
*/
#define FcNpuInData             00003
#define FcNpuInNpuStatus        00004
#define FcNpuInCouplerStatus    00005
#define FcNpuInNpuOrder         00006
#define FcNpuInProgram          00007

#define FcNpuOutMemAddr0        00010
#define FcNpuOutMemAddr1        00011
#define FcNpuOutData            00014
#define FcNpuOutProgram         00015
#define FcNpuOutNpuOrder        00016

#define FcNpuStartNpu           00040
#define FcNpuHaltNpu            00100
#define FcNpuClearNpu           00200
#define FcNpuClearCoupler       00400

#define FcNpuEqMask             07000

/*
**  Coupler status bits (read by PP).
*/
#define StCplrStatusLoaded      (1 << 2)
#define StCplrAddrLoaded        (1 << 3)
#define StCplrTransferCompleted (1 << 5)
#define StCplrHostTransferTerm  (1 << 7)    // PP has disconnected during the transfer
#define StCplrOrderLoaded       (1 << 8)
#define StCplrNpuStatusRead     (1 << 9)
#define StCplrTimeout           (1 << 10)

/*
**  NPU status values (read by PP when StCplrStatusLoaded is set).
*/
#define StNpuIgnore             00000
#define StNpuIdle               00001
#define StNpuReadyOutput        00002
#define StNpuNotReadyOutput     00003
#define StNpuInputAvailLe256    00004
#define StNpuInputAvailGt256    00005
#define StNpuInputAvailPru      00006
#define StNpuInitRequest        00007
#define StNpuInitCompleted      00010

/*
**  NPU order word codes (written by PP which results in
**  StCplrOrderLoaded being set). The LSB contains the
**  block length or the new regulation level.
*/
#define OrdOutServiceMsg        0x100
#define OrdOutPriorHigh         0x200
#define OrdOutPriorLow          0x300
#define OrdNotReadyForInput     0x400
#define OrdRegulationLvlChange  0x500
#define OrdInitRequestAck       0x600

#define OrdMaskType             0xF00
#define OrdMaskValue            0x0FF

/*
**  Misc constants.
*/
#define CyclesOneSecond         100000
//#define ReportInitCount         4

#if DEBUG
#define HexColumn(x) (4 * (x) + 1 + 4)
#define AsciiColumn(x) (HexColumn(16) + 2 + (x))
#define LogLineLength (AsciiColumn(16))
#endif

/*
**  -----------------------
**  Private Macro Functions
**  -----------------------
*/

/*
**  -----------------------------------------
**  Private Typedef and Structure Definitions
**  -----------------------------------------
*/

/*
**  ---------------------------
**  Private Function Prototypes
**  ---------------------------
*/
static void npuReset(u8 mfrId);
static FcStatus npuHipFunc(PpWord funcCode, u8 mfrId);
static void npuHipIo(u8 mfrId);
static void npuHipActivate(u8 mfrId);
static void npuHipDisconnect(u8 mfrId);
static void npuHipWriteNpuStatus(PpWord status, u8 mfrId);
static PpWord npuHipReadNpuStatus(u8 mfrId);
static char *npuHipFunc2String(PpWord funcCode);
#if DEBUG
static void npuLogFlush();
static void npuLogByte(int b);
#endif

/*
**  ----------------
**  Public Variables
**  ----------------
*/

/*
**  -----------------
**  Private Variables
**  -----------------
*/


#if DEBUG
static FILE *npuLog = nullptr;
static char npuLogBuf[LogLineLength + 1];
static int npuLogCol = 0;
#endif

/*
**--------------------------------------------------------------------------
**
**  Public Functions
**
**--------------------------------------------------------------------------
*/
/*--------------------------------------------------------------------------
**  Purpose:        Initialise NPU.
**
**  Parameters:     Name        Description.
**                  eqNo        equipment number
**                  unitNo      unit number
**                  channelNo   channel number the device is attached to
**                  deviceName  optional device file name
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void npuInit(u8 mfrID, u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName)
{
	MMainFrame *mfr = BigIron->chasis[mfrID];

	(void)unitNo;
	(void)deviceName;

#if DEBUG
	if (npuLog == nullptr)
	{
		npuLog = fopen("npulog.txt", "wt");
	}
#endif

	/*
	**  Attach device to channel and initialise device control block.
	*/
	DevSlot *dp = channelAttach(channelNo, eqNo, DtNpu, mfrID);
	dp->activate = npuHipActivate;
	dp->disconnect = npuHipDisconnect;
	dp->func = npuHipFunc;
	dp->io = npuHipIo;
	dp->selectedUnit = unitNo;
	mfr->activeDevice = dp;

	/*
	**  Allocate and initialise NPU parameters.
	*/
	mfr->npu = static_cast<NpuParam*>(calloc(1, sizeof(NpuParam)));
	if (mfr->npu == nullptr)
	{
		fprintf(stderr, "Failed to allocate npu context block\n");
		exit(1);
	}

	dp->controllerContext = mfr->npu;
	mfr->npu->regCouplerStatus = 0;
	mfr->hipState = StHipInit;

	/*
	**  Initialise BIP, SVC and TIP.
	*/
	//if (mfrID == 0)
	{
		npuBipInit(mfrID);
		npuSvmInit(mfrID);
	}
	npuTipInit(mfrID);

	/*
	**  Print a friendly message.
	*/
	printf("NPU initialised on channel %o equipment %o mainframe %o\n", channelNo, eqNo, mfrID);
}

/*--------------------------------------------------------------------------
**  Purpose:        Request sending of upline block.
**
**  Parameters:     Name        Description.
**                  bp          pointer to first upline buffer.
**
**  Returns:        TRUE if buffer can be accepted, FALSE otherwise.
**
**------------------------------------------------------------------------*/
bool npuHipUplineBlock(NpuBuffer *bp, u8 mfrId)
{
	MMainFrame *mfr = BigIron->chasis[mfrId];

	if (mfr->hipState != StHipIdle)
	{
		return(false);
	}

	if (bp->numBytes <= 256)
	{
		npuHipWriteNpuStatus(StNpuInputAvailLe256, mfrId);
	}
	else
	{
		npuHipWriteNpuStatus(StNpuInputAvailGt256, mfrId);
	}

	mfr->npu->buffer = bp;
	mfr->hipState = StHipUpline;
	return(true);
}

/*--------------------------------------------------------------------------
**  Purpose:        Request reception of downline block.
**
**  Parameters:     Name        Description.
**                  bp          pointer to first downline buffer.
**
**  Returns:        TRUE if buffer can be accepted, FALSE otherwise.
**
**------------------------------------------------------------------------*/
bool npuHipDownlineBlock(NpuBuffer *bp, u8 mfrId)
{
	MMainFrame *mfr = BigIron->chasis[mfrId];

	if (mfr->hipState != StHipIdle)
	{
		return(false);
	}

	if (bp == nullptr)
	{
		npuHipWriteNpuStatus(StNpuNotReadyOutput, mfrId);
		return(false);
	}

	npuHipWriteNpuStatus(StNpuReadyOutput, mfrId);
	mfr->npu->buffer = bp;
	mfr->hipState = StHipDownline;
	return(true);
}

/*--------------------------------------------------------------------------
**  Purpose:        Write to the error log
**
**  Parameters:     Name        Description.
**                  format      format specifier (as in printf)
**                  ...         variable argument list
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void npuLogMessage(char *format, ...)
{
#if DEBUG
	va_list args;

	npuLogFlush();
	fprintf(npuLog, "\n\n");
	va_start(args, format);
	vfprintf(npuLog, format, args);
	va_end(args);
	fprintf(npuLog, "\n");
#endif
}

/*
**--------------------------------------------------------------------------
**
**  Private Functions
**
**--------------------------------------------------------------------------
*/

/*--------------------------------------------------------------------------
**  Purpose:        Reset NPU.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void npuReset(u8 mfrId)
{
	MMainFrame *mfr = BigIron->chasis[mfrId];

	/*
	**  Reset all subsystems - order matters!
	*/
	npuNetReset(mfrId);
	npuTipReset(mfrId);
	npuSvmReset(mfrId);
	npuBipReset(mfrId);

	/*
	**  Reset HIP state.
	*/
	memset(mfr->npu, 0, sizeof(NpuParam));
	mfr->initCount = ReportInitCount;
	mfr->hipState = StHipInit;
}

/*--------------------------------------------------------------------------
**  Purpose:        Execute function code on NPU.
**
**  Parameters:     Name        Description.
**                  funcCode    function code
**
**  Returns:        FcStatus
**
**------------------------------------------------------------------------*/
static FcStatus npuHipFunc(PpWord funcCode, u8 mfrId)
{
	NpuBuffer *bp;
	MMainFrame *mfr = BigIron->chasis[mfrId];

	funcCode &= ~FcNpuEqMask;

#if DEBUG
	npuLogFlush();
	if (funcCode != FcNpuInCouplerStatus)
	{
		fprintf(npuLog, "\n%06d PP:%02o CH:%02o f:%04o T:%-25s  >   ",
			traceSequenceNo,
			activePpu->id,
			activeChannel->id,
			funcCode,
			npuHipFunc2String(funcCode));
	}
#endif

	switch (funcCode)
	{
	default:
#if DEBUG
		fprintf(npuLog, " FUNC not implemented & declined!");
#endif
		return(FcDeclined);

	case FcNpuInCouplerStatus:
		switch (mfr->hipState)
		{
		case StHipInit:
			if (mfr->initCount > 0)
			{
				/*
				**  Tell PIP a few times that the NPU has initialized.
				*/
				mfr->initCount -= 1;
				npuHipWriteNpuStatus(StNpuInitCompleted, mfrId);
			}
			else
			{
				mfr->hipState = StHipIdle;
				npuHipWriteNpuStatus(StNpuIdle, mfrId);
			}

			break;

		case StHipIdle:
			/*
			**  Poll network status.
			*/
			npuNetCheckStatus(mfrId);

			/*
			**  If no upline data pending.
			*/
			if (mfr->hipState == StHipIdle)
			{
				/*
				**  Announce idle state to PIP at intervals of less then one second,
				**  otherwise PIP will assume that the NPU is dead.
				*/
				if (labs(mfr->activeChannel->mfr->cycles - mfr->npu->lastCommandTime) > CyclesOneSecond)
				{
					npuHipWriteNpuStatus(StNpuIdle, mfrId);
				}
			}
			break;

		default:
			break;
		}

		break;

	case FcNpuInData:
		bp = mfr->npu->buffer;
		if (bp == nullptr)
		{
			/*
			**  Unexpected input request by host.
			*/
			mfr->hipState = StHipIdle;
			mfr->npu->npuData = nullptr;
			mfr->activeDevice->recordLength = 0;
			mfr->activeDevice->fcode = 0;
			return(FcDeclined);
		}

		mfr->npu->npuData = bp->data;
		mfr->activeDevice->recordLength = bp->numBytes;
		break;

	case FcNpuOutData:
		bp = mfr->npu->buffer;
		if (bp == nullptr)
		{
			/*
			**  Unexpected output request by host.
			*/
			mfr->hipState = StHipIdle;
			mfr->npu->npuData = nullptr;
			mfr->activeDevice->recordLength = 0;
			mfr->activeDevice->fcode = 0;
			return(FcDeclined);
		}

		mfr->npu->npuData = bp->data;
		mfr->activeDevice->recordLength = 0;
		break;

	case FcNpuInNpuStatus:
	case FcNpuInNpuOrder:
		break;

	case FcNpuOutNpuOrder:
		mfr->hipState = StHipIdle;
		npuHipWriteNpuStatus(StNpuIdle, mfrId);
		break;

	case FcNpuClearNpu:
		npuReset(mfrId);
		break;

		/*
		**  The functions below are not supported and are implemented as dummies.
		*/
	case FcNpuInProgram:
	case FcNpuOutMemAddr0:
	case FcNpuOutMemAddr1:
	case FcNpuOutProgram:
		break;

	case FcNpuStartNpu:
	case FcNpuHaltNpu:
	case FcNpuClearCoupler:
		return(FcProcessed);
	}

	mfr->activeDevice->fcode = funcCode;
	return(FcAccepted);
}

/*--------------------------------------------------------------------------
**  Purpose:        Perform I/O on NPU.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void npuHipIo(u8 mfrId)
{
	MMainFrame *mfr = BigIron->chasis[mfrId];

	// ReSharper disable once CppJoinDeclarationAndAssignment
	PpWord orderType;
	// ReSharper disable once CppJoinDeclarationAndAssignment
	u8 orderValue;

	switch (mfr->activeDevice->fcode)
	{
	default:
		break;

	case FcNpuInNpuStatus:
		mfr->activeChannel->data = npuHipReadNpuStatus(mfrId);
		mfr->activeChannel->full = true;
#if DEBUG
		fprintf(npuLog, " %03X", activeChannel->data);
#endif
		break;

	case FcNpuInCouplerStatus:
		mfr->activeChannel->data = mfr->npu->regCouplerStatus;
		mfr->activeChannel->full = true;
#if DEBUG
		if (npu->regCouplerStatus != 0)
		{
			fprintf(npuLog, "\n%06d PP:%02o CH:%02o f:%04o T:%-25s  >    %03X",
				traceSequenceNo,
				activePpu->id,
				activeChannel->id,
				FcNpuInCouplerStatus,
				npuHipFunc2String(FcNpuInCouplerStatus),
				activeChannel->data);
		}
#endif
		break;

	case FcNpuInNpuOrder:
		mfr->activeChannel->data = mfr->npu->regOrder;
		mfr->activeChannel->full = true;
#if DEBUG
		fprintf(npuLog, " %03X", activeChannel->data);
#endif
		break;

	case FcNpuInData:
		if (mfr->activeChannel->full)
		{
			break;
		}

		if (mfr->activeDevice->recordLength > 0)
		{
			mfr->activeChannel->data = *mfr->npu->npuData++;
			mfr->activeChannel->full = true;

			mfr->activeDevice->recordLength -= 1;
			if (mfr->activeDevice->recordLength == 0)
			{
				/*
				**  Transmission complete.
				*/
				mfr->activeChannel->data |= 04000;
				mfr->activeChannel->discAfterInput = true;
				mfr->activeDevice->fcode = 0;
				mfr->hipState = StHipIdle;
				npuBipNotifyUplineSent(mfrId);
			}
#if DEBUG
			npuLogByte(activeChannel->data);
#endif
		}

		break;

	case FcNpuOutData:
		if (mfr->activeChannel->full)
		{
#if DEBUG
			npuLogByte(activeChannel->data);
#endif
			mfr->activeChannel->full = false;
			if (mfr->activeDevice->recordLength < MaxBuffer)
			{
				*mfr->npu->npuData++ = mfr->activeChannel->data & Mask8;
				mfr->activeDevice->recordLength += 1;
				if ((mfr->activeChannel->data & 04000) != 0)
				{
					/*
					**  Top bit set - process message.
					*/
					mfr->npu->buffer->numBytes = mfr->activeDevice->recordLength;
					mfr->activeDevice->fcode = 0;
					mfr->hipState = StHipIdle;
					npuBipNotifyDownlineReceived(mfrId);
				}
				else if (mfr->activeDevice->recordLength >= MaxBuffer)
				{
					/*
					**  We run out of buffer space before the end of the message.
					*/
					mfr->activeDevice->fcode = 0;
					mfr->hipState = StHipIdle;
					npuBipAbortDownlineReceived(mfrId);
				}
			}
		}

		break;

	case FcNpuOutNpuOrder:
		if (mfr->activeChannel->full)
		{
#if DEBUG
			static char *orderCode[] =
			{
				"",
				"output level one - service messages",
				"output level two - high priority",
				"output level three - low priority",
				"driver not ready for input",
				"regulation level change",
				"initialization request acknowledgment",
				""
			};

			fprintf(npuLog, " Order word %03X - function %02X : %s",
				activeChannel->data, activeChannel->data >> 8, orderCode[(activeChannel->data >> 8) & 7]);
#endif
			mfr->npu->regOrder = mfr->activeChannel->data;
			// ReSharper disable once CppJoinDeclarationAndAssignment
			orderType = mfr->activeChannel->data & OrdMaskType;
			// ReSharper disable once CppJoinDeclarationAndAssignment
			orderValue = static_cast<u8>(mfr->activeChannel->data & OrdMaskValue);
			mfr->activeChannel->full = false;

			// ReSharper disable once CppDefaultCaseNotHandledInSwitchStatement
			switch (orderType)
			{
			case OrdOutServiceMsg:
				npuBipNotifyServiceMessage(mfrId);
				break;

			case OrdOutPriorHigh:
				npuBipNotifyData(1, mfrId);
				break;

			case OrdOutPriorLow:
				npuBipNotifyData(0, mfrId);
				break;

			case OrdNotReadyForInput:
				npuBipRetryInput(mfrId);
				break;

			case OrdRegulationLvlChange:
				npuSvmNotifyHostRegulation(orderValue, mfrId);

				/*
				**  Send any pending upline blocks.
				*/
				npuBipRetryInput(mfrId);
				break;

			case OrdInitRequestAck:
				/*
				**  Ignore because we don't support loading, but send
				**  any pending upline blocks.
				*/
				npuBipRetryInput(mfrId);
				break;
			}
		}

		break;

	case FcNpuInProgram:
		/*
		**  Dummy data because we don't support dumping.
		*/
		mfr->activeChannel->data = 0;
		mfr->activeChannel->full = true;
		break;

	case FcNpuOutMemAddr0:
	case FcNpuOutMemAddr1:
	case FcNpuOutProgram:
		/*
		**  Ignore data because we don't support loading and dumping.
		*/
		mfr->activeChannel->full = false;
		break;

	case FcNpuStartNpu:
	case FcNpuHaltNpu:
	case FcNpuClearNpu:
	case FcNpuClearCoupler:
		/*
		**  Ignore loading and dumping related functions.
		*/
		break;
	}
}

/*--------------------------------------------------------------------------
**  Purpose:        Handle channel activation.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void npuHipActivate(u8 mfrId)
{
}

/*--------------------------------------------------------------------------
**  Purpose:        Handle disconnecting of channel.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void npuHipDisconnect(u8 mfrId)
{
}

/*--------------------------------------------------------------------------
**  Purpose:        NPU writes NPU status register.
**
**  Parameters:     Name        Description.
**                  status      new status register value.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void npuHipWriteNpuStatus(PpWord status, u8 mfrId)
{
	MMainFrame *mfr = BigIron->chasis[mfrId];

	mfr->npu->lastCommandTime = mfr->cycles;	//DRS??!!
	mfr->npu->regNpuStatus = status;
	mfr->npu->regCouplerStatus |= StCplrStatusLoaded;
}

/*--------------------------------------------------------------------------
**  Purpose:        PP reads NPU status register.
**
**  Parameters:     Name        Description.
**
**  Returns:        NPU status register value.
**
**------------------------------------------------------------------------*/
static PpWord npuHipReadNpuStatus(u8 mfrId)
{
	MMainFrame *mfr = BigIron->chasis[mfrId];

	PpWord value = mfr->npu->regNpuStatus;

	mfr->npu->regCouplerStatus &= ~StCplrStatusLoaded;
	mfr->npu->regNpuStatus = StNpuIgnore;
	return(value);
}

/*--------------------------------------------------------------------------
**  Purpose:        Convert function code to string.
**
**  Parameters:     Name        Description.
**                  funcCode    function code
**
**  Returns:        String equivalent of function code.
**
**------------------------------------------------------------------------*/
static char *npuHipFunc2String(PpWord funcCode)
{
	static char buf[30];
#if DEBUG
	switch (funcCode)
	{
	case FcNpuInData: return "FcNpuInData";
	case FcNpuInNpuStatus: return "FcNpuInNpuStatus";
	case FcNpuInCouplerStatus: return "FcNpuInCouplerStatus";
	case FcNpuInNpuOrder: return "FcNpuInNpuOrder";
	case FcNpuInProgram: return "FcNpuInProgram";
	case FcNpuOutMemAddr0: return "FcNpuOutMemAddr0";
	case FcNpuOutMemAddr1: return "FcNpuOutMemAddr1";
	case FcNpuOutData: return "FcNpuOutData";
	case FcNpuOutProgram: return "FcNpuOutProgram";
	case FcNpuOutNpuOrder: return "FcNpuOutNpuOrder";
	case FcNpuStartNpu: return "FcNpuStartNpu";
	case FcNpuHaltNpu: return "FcNpuHaltNpu";
	case FcNpuClearNpu: return "FcNpuClearNpu";
	case FcNpuClearCoupler: return "FcNpuClearCoupler";
	}
#endif
	sprintf(buf, "UNKNOWN: %04o", funcCode);
	return(buf);
}

#if DEBUG
/*--------------------------------------------------------------------------
**  Purpose:        Flush incomplete numeric/ascii data line
**
**  Parameters:     Name        Description.
**
**  Returns:        nothing
**
**------------------------------------------------------------------------*/
static void npuLogFlush(void)
{
	if (npuLogCol != 0)
	{
		fputs(npuLogBuf, npuLog);
	}

	npuLogCol = 0;
	memset(npuLogBuf, ' ', LogLineLength);
	npuLogBuf[0] = '\n';
	npuLogBuf[LogLineLength] = '\0';
}

/*--------------------------------------------------------------------------
**  Purpose:        Log a byte in hex/ascii form
**
**  Parameters:     Name        Description.
**
**  Returns:        nothing
**
**------------------------------------------------------------------------*/
static void npuLogByte(int b)
{
	char hex[5];
	int col;

	col = HexColumn(npuLogCol);
	sprintf(hex, "%03X ", b);
	memcpy(npuLogBuf + col, hex, 4);

	col = AsciiColumn(npuLogCol);
	b &= 0x7f;
	if (b < 0x20 || b >= 0x7f)
	{
		b = '.';
	}

	npuLogBuf[col] = b;
	if (++npuLogCol == 16)
	{
		npuLogFlush();
	}
}
#endif

/*---------------------------  End Of File  ------------------------------*/

