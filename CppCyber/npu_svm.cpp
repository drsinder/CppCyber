/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2011, Tom Hunter
**  C++ adaptation by Dale Sinder 2017
**
**  Name: npu_svm.cpp
**
**  Description:
**      Perform emulation of the Service Message (SVM) subsystem in an NPU
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
//#include "npu.h"

///*
//**  -----------------
//**  Private Constants
//**  -----------------
//*/
//

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
static bool npuSvmRequestTerminalConfig(Tcb *tp, u8 mfrId);
static bool npuSvmProcessTerminalConfig(Tcb *tp, NpuBuffer *bp);
static bool npuSvmRequestTerminalConnection(Tcb *tp, u8 mfrId);

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

/*
**--------------------------------------------------------------------------
**
**  Public Functions
**
**--------------------------------------------------------------------------
*/

/*--------------------------------------------------------------------------
**  Purpose:        Initialize SVM.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void npuSvmInit(u8 mfrId)
{
	MMainFrame *mfr = BigIron->chasis[mfrId];

	/*
	**  Set initial state.
	*/
	mfr->svmState = mfr->StIdle;
}

/*--------------------------------------------------------------------------
**  Purpose:        Reset SVM.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void npuSvmReset(u8 mfrId)
{
	MMainFrame *mfr = BigIron->chasis[mfrId];

	/*
	**  Set initial state.
	*/
	mfr->svmState = mfr->StIdle;
	mfr->oldRegLevel = 0;
}

/*--------------------------------------------------------------------------
**  Purpose:        Process regulation order word.
**
**  Parameters:     Name        Description.
**                  regLevel    regulation level
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void npuSvmNotifyHostRegulation(u8 regLevel, u8 mfrId)
{
	MMainFrame *mfr = BigIron->chasis[mfrId];
	if (mfr->svmState == mfr->StIdle || regLevel != mfr->oldRegLevel)
	{
		mfr->oldRegLevel = regLevel;
		mfr->linkRegulation[BlkOffP3] = regLevel;
		npuBipRequestUplineCanned(mfr->linkRegulation, sizeof(mfr->linkRegulation), mfrId);
	}

	if (mfr->svmState == mfr->StIdle && (regLevel & RegLvlCsAvailable) != 0)
	{
		npuBipRequestUplineCanned(mfr->requestSupervision, sizeof(mfr->requestSupervision), mfrId);
		mfr->svmState = mfr->StWaitSupervision;
	}
}

/*--------------------------------------------------------------------------
**  Purpose:        Start host connection sequence.
**
**  Parameters:     Name        Description.
**                  tp          TCB pointer
**
**  Returns:        TRUE if sequence started, FALSE otherwise.
**
**------------------------------------------------------------------------*/
bool npuSvmConnectTerminal(Tcb *tp, u8 mfrId)
{
	if (npuSvmRequestTerminalConfig(tp, mfrId))
	{
		tp->state = StTermRequestConfig;
		return(true);
	}

	return(false);
}

/*--------------------------------------------------------------------------
**  Purpose:        Process service message from host.
**
**  Parameters:     Name        Description.
**                  bp          buffer with service message.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void npuSvmProcessBuffer(NpuBuffer *bp, u8 mfrId)
{
	MMainFrame *mfr = BigIron->chasis[mfrId];

	u8 *block = bp->data;
	Tcb *tp;

	/*
	**  Ensure there is at least a minimal service message.
	*/
	if (bp->numBytes < BlkOffSfc + 1)
	{
		if (bp->numBytes == BlkOffBTBSN + 1 && block[BlkOffCN] != 0)
		{
			/*
			**  Exception to minimal service message:
			**  For some strange reason NAM sends an input acknowledgment
			**  as a SVM - forward it to the TIP which is better equipped
			**  to deal with this.
			*/
			npuTipProcessBuffer(bp, 0, mfrId);
			return;
		}

		/*
		**  Service message must be at least DN/SN/0/BSN/PFC/SFC.
		*/
		npuLogMessage("Short SVM message in state %d", mfr->svmState);

		/*
		**  Release downline buffer and return.
		*/
		npuBipBufRelease(bp, mfrId);
		return;
	}

	/*
	**  Connection number for all service messages must be zero.
	*/
	u8 cn = block[BlkOffCN];
	if (cn != 0)
	{
		/*
		**  Connection number out of range.
		*/
		npuLogMessage("Connection number is %u but must be zero in SVM messages %02X/%02X", cn, block[BlkOffPfc], block[BlkOffSfc]);

		/*
		**  Release downline buffer and return.
		*/
		npuBipBufRelease(bp, mfrId);
		return;
	}

	/*
	**  Extract the true connection number for those messages which carry it in P3.
	*/
	// ReSharper disable once CppDefaultCaseNotHandledInSwitchStatement
	switch (block[BlkOffPfc])
	{
	case PfcCNF:
	case PfcICN:
	case PfcTCN:
		if (bp->numBytes < BlkOffP3 + 1)
		{
			/*
			**  Message too short.
			*/
			npuLogMessage("SVM message %02X/%02X is too short and has no required P3", block[BlkOffPfc], block[BlkOffSfc]);
			npuBipBufRelease(bp, mfrId);
			return;
		}

		cn = block[BlkOffP3];
		if (cn == 0 || cn > mfr->npuTcbCount)
		{
			/*
			**  Port number out of range.
			*/
			npuLogMessage("Unexpected port number %u in SVM message %02X/%02X", cn, block[BlkOffPfc], block[BlkOffSfc]);
			npuBipBufRelease(bp, mfrId);
			return;
		}

		tp = mfr->npuTcbs + cn - 1;
		break;
	}

	/*
	**  Process message.
	*/
	// ReSharper disable once CppDefaultCaseNotHandledInSwitchStatement
	switch (block[BlkOffPfc])
	{
	case PfcSUP:
		if (block[BlkOffSfc] == (SfcIN | SfcResp))
		{
			if (mfr->svmState != mfr->StWaitSupervision)
			{
				npuLogMessage("Unexpected Supervision Reply in state %d", mfr->svmState);
				break;
			}

			/*
			**  Host (CS) has agreed to supervise us, we are now ready to handle network
			**  connection attempts.
			*/
			mfr->svmState = mfr->StReady;
		}
		else
		{
			npuLogMessage("Unexpected SVM message %02X/%02X in state %d", block[BlkOffPfc], block[BlkOffSfc], mfr->svmState);
		}

		break;

	case PfcNPS:
		if (block[BlkOffSfc] == SfcNP)
		{
			npuBipRequestUplineCanned(mfr->responseNpuStatus, sizeof(mfr->responseNpuStatus), mfrId);
		}
		else
		{
			npuLogMessage("Unexpected SVM message %02X/%02X in state %d", block[BlkOffPfc], block[BlkOffSfc], mfr->svmState);
		}

		break;

	case PfcCNF:
		// ReSharper disable once CppDeclaratorMightNotBeInitialized
		if (tp->state != StTermRequestConfig)
		{
			// ReSharper disable once CppDeclaratorMightNotBeInitialized
			npuLogMessage("Unexpected Terminal Configuration Reply in state %d", tp->state);
			break;
		}

		if (block[BlkOffSfc] == (SfcTE | SfcResp))
		{
			/*
			**  Process configuration reply and if all is well, issue
			**  terminal connection request.
			*/
			// ReSharper disable once CppDeclaratorMightNotBeInitialized
			if (npuSvmProcessTerminalConfig(tp, bp)
				// ReSharper disable once CppDeclaratorMightNotBeInitialized
				&& npuSvmRequestTerminalConnection(tp, mfrId))
			{
				// ReSharper disable once CppDeclaratorMightNotBeInitialized
				tp->state = StTermRequestConnection;
			}
			else
			{
				// ReSharper disable once CppDeclaratorMightNotBeInitialized
				npuNetDisconnected(tp);
			}
		}
		else if (block[BlkOffSfc] == (SfcTE | SfcErr))
		{
			/*
			**  This port appears to be unknown to the host.
			*/
			npuLogMessage("Terminal on port %d not configured", cn);
			// ReSharper disable once CppDeclaratorMightNotBeInitialized
			npuNetDisconnected(tp);
		}
		else
		{
			npuLogMessage("Unexpected SVM message %02X/%02X with CN %d", block[BlkOffPfc], block[BlkOffSfc], cn);
			// ReSharper disable once CppDeclaratorMightNotBeInitialized
			npuNetDisconnected(tp);
		}

		break;

	case PfcICN:
		// ReSharper disable once CppDeclaratorMightNotBeInitialized
		if (tp->state != StTermRequestConnection)
		{
			// ReSharper disable once CppDeclaratorMightNotBeInitialized
			npuLogMessage("Unexpected Terminal Connection Reply in state %d", tp->state);
			break;
		}

		if (block[BlkOffSfc] == (SfcTE | SfcResp))
		{
			// ReSharper disable once CppDeclaratorMightNotBeInitialized
			npuNetConnected(tp);
		}
		else if (block[BlkOffSfc] == (SfcTE | SfcErr))
		{
			npuLogMessage("Terminal Connection Rejected - reason 0x%02X", block[BlkOffP4]);
			// ReSharper disable once CppDeclaratorMightNotBeInitialized
			npuNetDisconnected(tp);
		}
		else
		{
			npuLogMessage("Unexpected SVM message %02X/%02X with CN %d", block[BlkOffPfc], block[BlkOffSfc], cn);
			// ReSharper disable once CppDeclaratorMightNotBeInitialized
			npuNetDisconnected(tp);
		}

		break;

	case PfcTCN:
		if (block[BlkOffSfc] == SfcTA)
		{
			/*
			**  Terminate connection from host.
			*/
			// ReSharper disable once CppDeclaratorMightNotBeInitialized
			npuTipTerminateConnection(tp, mfrId);
		}
		else if (block[BlkOffSfc] == (SfcTA | SfcResp))
		{
			// ReSharper disable once CppDeclaratorMightNotBeInitialized
			if (tp->state == StTermNpuDisconnect)
			{
				/*
				**  Reset connection state.
				*/
				// ReSharper disable once CppDeclaratorMightNotBeInitialized
				tp->state = StTermIdle;
			}
		}
		else
		{
			npuLogMessage("Unexpected SVM message %02X/%02X with CN %d", block[BlkOffPfc], block[BlkOffSfc], cn);
		}

		break;
	}

	/*
	**  Release downline buffer.
	*/
	npuBipBufRelease(bp, mfrId);
}

/*--------------------------------------------------------------------------
**  Purpose:        Cleanup and send a TCN/TA/R to host.
**
**  Parameters:     Name        Description.
**                  tp          TCB pointer
**
**  Returns:        Nothing
**
**------------------------------------------------------------------------*/
void npuSvmDiscRequestTerminal(Tcb *tp, u8 mfrId)
{
	MMainFrame *mfr = BigIron->chasis[mfrId];

	if (tp->state == StTermHostConnected)
	{
		/*
		**  Clean up flow control state and discard any pending output.
		*/
		tp->xoff = false;
		npuTipDiscardOutputQ(tp, mfrId);
		tp->state = StTermNpuDisconnect;

		/*
		**  Send the TCN/TA/R message.
		*/
		mfr->requestTerminateConnection[BlkOffP3] = tp->portNumber;
		npuBipRequestUplineCanned(mfr->requestTerminateConnection, sizeof(mfr->requestTerminateConnection), mfrId);
	}
	else
	{
		/*
		**  Just reset the state to idle.
		*/
		tp->state = StTermIdle;
	}
}

/*--------------------------------------------------------------------------
**  Purpose:        Send a TCN/TA/N to host.
**
**  Parameters:     Name        Description.
**                  tp          TCB pointer
**
**  Returns:        Nothing
**
**------------------------------------------------------------------------*/
void npuSvmDiscReplyTerminal(Tcb *tp, u8 mfrId)
{
	MMainFrame *mfr = BigIron->chasis[mfrId];

	mfr->responseTerminateConnection[BlkOffP3] = tp->portNumber;
	npuBipRequestUplineCanned(mfr->responseTerminateConnection, sizeof(mfr->responseTerminateConnection), mfrId);
}

/*--------------------------------------------------------------------------
**  Purpose:        Determine if host is ready for connection request
**
**  Parameters:     Name        Description.
**
**  Returns:        TRUE if host is ready to accept connections, FALSE
**                  otherwise.
**
**------------------------------------------------------------------------*/
bool npuSvmIsReady(u8 mfrId)
{
	MMainFrame *mfr = BigIron->chasis[mfrId];

	return(mfr->svmState == mfr->StReady);
}

/*
**--------------------------------------------------------------------------
**
**  Private Functions
**
**--------------------------------------------------------------------------
*/

/*--------------------------------------------------------------------------
**  Purpose:        Send terminal configuration request to host.
**
**  Parameters:     Name        Description.
**                  tp          TCB pointer
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static bool npuSvmRequestTerminalConfig(Tcb *tp, u8 mfrId)
{
	NpuBuffer *bp = npuBipBufGet(mfrId);
	if (bp == nullptr)
	{
		return(false);
	}

	/*
	**  Assemble configure request.
	*/
	u8 *mp = bp->data;

	*mp++ = AddrHost;           // DN
	*mp++ = AddrNpu;            // SN
	*mp++ = 0;                  // CN
	*mp++ = 4;                  // BT=CMD
	*mp++ = PfcCNF;             // PFC
	*mp++ = SfcTE;              // SFC
	*mp++ = tp->portNumber;     // non-zero port number from "PORT=" parameter in NDL source
	*mp++ = 0;                  // sub-port number (always 0 for async ports)
	*mp++ = (0 << 7) | (tp->tipType << 3); // no auto recognition; TIP type; subtype 0

	bp->numBytes = static_cast<u16>(mp - bp->data);

	/*
	**  Send the request.
	*/
	npuBipRequestUplineTransfer(bp, mfrId);

	return(true);
}

/*--------------------------------------------------------------------------
**  Purpose:        Process terminal configuration reply from host.
**
**  Parameters:     Name        Description.
**                  tp          TCB pointer
**                  bp          buffer with service message.
**
**  Returns:        TRUE if successful, FALSE otherwise.
**
**------------------------------------------------------------------------*/
static bool npuSvmProcessTerminalConfig(Tcb *tp, NpuBuffer *bp)
{
	u8 *mp = bp->data;
	int len = bp->numBytes;
	u8 termName[7];

	/*
	**  Extract configuration.
	*/
	mp += BlkOffP3;
	// ReSharper disable once CppEntityNeverUsed
	u8 port = *mp++;
	// ReSharper disable once CppEntityNeverUsed
	u8 subPort = *mp++;
	// ReSharper disable once CppEntityNeverUsed
	u8 address1 = *mp++;
	// ReSharper disable once CppEntityNeverUsed
	u8 address2 = *mp++;
	u8 deviceType = *mp++;
	u8 subTip = *mp++;

	memcpy(termName, mp, sizeof(termName));
	mp += sizeof(termName);

	u8 termClass = *mp++;
	u8 status = *mp++;
	// ReSharper disable once CppEntityNeverUsed
	u8 lastResp = *mp++;
	u8 codeSet = *mp++;

	/*
	**  Verify minimum length;
	*/
	len -= static_cast<int>(mp - bp->data);
	if (len < 0)
	{
		npuLogMessage("Short Terminal Configuration response with length %d", bp->numBytes);
		return(false);
	}

	/*
	**  Setup default operating parameters for the specified terminal class.
	*/
	npuTipSetupTerminalClass(tp, termClass);

	/*
	**  Setup TCB with supported FN/FV values.
	*/
	npuTipParseFnFv(mp, len, tp);

	/*
	**  Transfer configuration to TCB.
	*/
	tp->enabled = status == 0;
	memcpy(tp->termName, termName, sizeof(termName));
	tp->deviceType = deviceType;
	tp->subTip = subTip;
	tp->codeSet = codeSet;
	tp->params.fvTC = termClass;

	/*
	**  Reset user break 2 status.
	*/
	tp->breakPending = false;

	return(true);
}

/*--------------------------------------------------------------------------
**  Purpose:        Send connect request to host.
**
**  Parameters:     Name        Description.
**                  tp          TCB pointer
**
**  Returns:        TRUE if request sent, FALSE otherwise.
**
**------------------------------------------------------------------------*/
static bool npuSvmRequestTerminalConnection(Tcb *tp, u8 mfrId)
{
	NpuBuffer *bp = npuBipBufGet(mfrId);
	if (bp == nullptr)
	{
		return(false);
	}

	/*
	**  Assemble connect request.
	*/
	u8 *mp = bp->data;

	*mp++ = AddrHost;               // DN
	*mp++ = AddrNpu;                // SN
	*mp++ = 0;                      // CN
	*mp++ = 4;                      // BT=CMD
	*mp++ = PfcICN;                 // PFC
	*mp++ = SfcTE;                  // SFC
	*mp++ = tp->portNumber;         // CN
	*mp++ = tp->params.fvTC;        // TC
	*mp++ = tp->params.fvPL;        // page length
	*mp++ = tp->params.fvPW;        // page width
	*mp++ = tp->deviceType;         // device type
	*mp++ = 3;                      // downline block limit

	memcpy(mp, tp->termName, 7);    // terminal name
	mp += 7;

	*mp++ = 3;                      // application block limit
	*mp++ = 0x07;                   // application block size
	*mp++ = 0x00;                   // ...
	*mp++ = 0;                      // auto login flag
	*mp++ = 0;                      // device ordinal
	*mp++ = 0x07;                   // transmission block size
	*mp++ = 0x00;                   // ...
	*mp++ = 0;                      // sub device type

	memcpy(mp, tp->termName, 7);    // owning console
	mp += 7;

	//    *mp++ = 0;                      // security level
	*mp++ = 7;                      // security level
	*mp++ = tp->params.fvPriority;  // priority
	*mp++ = 1;                      // interactive capability
									//    *mp++ = tp->params.fvEchoplex;  // echoplex
	*mp++ = 1;                      // echoplex
	*mp++ = 100;                    // upline block size
	*mp++ = 1;                      // hardwired flag
	*mp++ = 0;                      // VTP ???
	*mp++ = 0;                      // DTE address length

	bp->numBytes = static_cast<u16>(mp - bp->data);

	/*
	**  Send the request.
	*/
	npuBipRequestUplineTransfer(bp, mfrId);

	return(true);
}

/*---------------------------  End Of File  ------------------------------*/

