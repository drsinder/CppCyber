/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2011, Tom Hunter, Paul Koning
**  C++ adaptation by Dale Sinder 2017
**
**  Name: npu_async.cpp
**
**  Description:
**      Perform emulation of the ASYNC TIP in an NPU consisting of a
**      CDC 2550 HCP running CCP.
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

/*
**  -----------------
**  Private Constants
**  -----------------
*/
#define MaxIvtData          100

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
static void npuAsyncDoFeBefore(u8 fe, u8 mfrId);
static void npuAsyncDoFeAfter(u8 fe, u8 mfrId);
static void npuAsyncProcessUplineTransparent(Tcb *tp, u8 mfrId);
static void npuAsyncProcessUplineAscii(Tcb *tp, u8 mfrId);
static void npuAsyncProcessUplineSpecial(Tcb *tp, u8 mfrId);
static void npuAsyncProcessUplineNormal(Tcb *tp, u8 mfrId);

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
static Tcb *npuTp;

static u8 fcSingleSpace[] = "\r\n";
static u8 fcDoubleSpace[] = "\r\n\n";
static u8 fcTripleSpace[] = "\r\n\n\n";
static u8 fcBol[] = "\r";
static u8 fcTofAnsi[] = "\r\n\033[H";
static u8 fcTof[] = "\f";
static u8 fcClearHomeAnsi[] = "\r\n\033[H\033[J";

static u8 netBEL[] = { ChrBEL };
static u8 netLF[] = { ChrLF };
static u8 netCR[] = { ChrCR };
static u8 netCRLF[] = { ChrCR, ChrLF };
//static u8 echoBuffer[1000];
//static u8 *echoPtr;
//static int echoLen;

/*
**--------------------------------------------------------------------------
**
**  Public Functions
**
**--------------------------------------------------------------------------
*/

/*--------------------------------------------------------------------------
**  Purpose:        Process downline data from host.
**
**  Parameters:     Name        Description.
**                  cn          connection number
**                  bp          buffer with downline data message.
**                  last        last buffer.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void npuAsyncProcessDownlineData(u8 cn, NpuBuffer *bp, bool last, u8 mfrId)
{
	MMainFrame *mfr = BigIron->chasis[mfrId];

	u8 *blk = bp->data + BlkOffData;
	int len = bp->numBytes - BlkOffData;
	u8 fe;

	/*
	**  Locate TCB dealing with this connection.
	*/
	if (cn == 0 || cn > mfr->npuTcbCount)
	{
		npuLogMessage("ASYNC: unexpected CN %d - message ignored", cn);
		return;
	}

	npuTp = mfr->npuTcbs + cn - 1;

	/*
	**  Extract Data Block Clarifier settings.
	*/
	u8 dbc = *blk++;
	len -= 1;
	npuTp->dbcNoEchoplex = (dbc & DbcEchoplex) != 0;
	npuTp->dbcNoCursorPos = (dbc & DbcNoCursorPos) != 0;

	if ((dbc & DbcTransparent) != 0)
	{
		npuNetSend(npuTp, blk, len, mfrId);
		npuNetQueueAck(npuTp, static_cast<u8>(bp->data[BlkOffBTBSN] & (BlkMaskBSN << BlkShiftBSN)), mfrId);
		return;
	}

	/*
	**  Process data.
	*/
	while (len > 0)
	{
		if ((dbc & DbcNoFe) != 0)
		{
			/*
			**  Format effector is supressed - output is single-spaced.
			*/
			fe = ' ';
		}
		else
		{
			fe = *blk++;
			len -= 1;
		}

		/*
		**  Process leading format effector.
		*/
		npuAsyncDoFeBefore(fe, mfrId);

		if (len == 0)
		{
			/*
			**  End-of-data.
			*/
			break;
		}

		/*
		**  Locate the US byte which defines the end-of-line.
		*/
		u8 *ptrUS = static_cast<u8*>(memchr(blk, ChrUS, len));
		if (ptrUS == nullptr)
		{
			/*
			**  No US byte in the rest of the buffer, so send the entire
			**  rest to the terminal.
			*/
			npuNetSend(npuTp, blk, len, mfrId);
			break;
		}

		/*
		**  Send the line.
		*/
		int textlen = static_cast<int>(ptrUS - blk);
		npuNetSend(npuTp, blk, textlen, mfrId);

		/*
		**  Process trailing format effector.
		*/
		if ((dbc & DbcNoCursorPos) == 0)
		{
			npuAsyncDoFeAfter(fe, mfrId);
		}

		/*
		**  Advance pointer past the US byte and adjust the length.
		*/
		blk += textlen + 1;
		len -= textlen + 1;
	}

	npuNetQueueAck(npuTp, static_cast<u8>(bp->data[BlkOffBTBSN] & (BlkMaskBSN << BlkShiftBSN)), mfrId);
}

/*--------------------------------------------------------------------------
**  Purpose:        Process upline data from terminal.
**
**  Parameters:     Name        Description.
**                  tp          TCB pointer
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void npuAsyncProcessUplineData(Tcb *tp, u8 mfrId)
{
	MMainFrame *mfr = BigIron->chasis[mfrId];

	mfr->echoPtr = mfr->echoBuffer;

	if (tp->params.fvXInput)
	{
		npuAsyncProcessUplineTransparent(tp, mfrId);
	}
	else if (tp->params.fvFullASCII)
	{
		npuAsyncProcessUplineAscii(tp, mfrId);
	}
	else if (tp->params.fvSpecialEdit)
	{
		npuAsyncProcessUplineSpecial(tp, mfrId);
	}
	else
	{
		npuAsyncProcessUplineNormal(tp, mfrId);
	}

	/*
	**  Optionally echo characters.
	*/
	if (!tp->dbcNoEchoplex)
	{
		mfr->echoLen = static_cast<int>(mfr->echoPtr - mfr->echoBuffer);
		if (mfr->echoLen)
		{
			npuNetSend(tp, mfr->echoBuffer, mfr->echoLen, mfrId);
		}
	}
}

/*--------------------------------------------------------------------------
**  Purpose:        Flush transparent upline data from terminal.
**
**  Parameters:     Name        Description.
**                  tp          TCB pointer
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void npuAsyncFlushUplineTransparent(Tcb *tp, u8 mfrId)
{
	if (!tp->params.fvXStickyTimeout)
	{
		/*
		**  Terminate transparent mode unless sticky timeout has been selected.
		*/
		tp->params.fvXInput = false;
	}

	/*
	**  Send the upline data.
	*/
	tp->inBuf[BlkOffDbc] = DbcTransparent;
	npuBipRequestUplineCanned(tp->inBuf, static_cast<int>(tp->inBufPtr - tp->inBuf), mfrId);
	npuTipInputReset(tp);
	tp->xInputTimerRunning = false;
}

/*
**--------------------------------------------------------------------------
**
**  Private Functions
**
**--------------------------------------------------------------------------
*/

/*--------------------------------------------------------------------------
**  Purpose:        Process format effector at start of line
**
**  Parameters:     Name        Description.
**                            format effector
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void npuAsyncDoFeBefore(u8 fe, u8 mfrId)
{
	// ReSharper disable once CppDefaultCaseNotHandledInSwitchStatement
	switch (fe)
	{
	case ' ':
		if (npuTp->lastOpWasInput)
		{
			npuNetSend(npuTp, fcBol, sizeof(fcBol) - 1, mfrId);
		}
		else
		{
			npuNetSend(npuTp, fcSingleSpace, sizeof(fcSingleSpace) - 1, mfrId);
		}
		break;

	case '0':
		if (npuTp->lastOpWasInput)
		{
			npuNetSend(npuTp, fcSingleSpace, sizeof(fcSingleSpace) - 1, mfrId);
		}
		else
		{
			npuNetSend(npuTp, fcDoubleSpace, sizeof(fcDoubleSpace) - 1, mfrId);
		}
		break;

	case '-':
		if (npuTp->lastOpWasInput)
		{
			npuNetSend(npuTp, fcDoubleSpace, sizeof(fcDoubleSpace) - 1, mfrId);
		}
		else
		{
			npuNetSend(npuTp, fcTripleSpace, sizeof(fcTripleSpace) - 1, mfrId);
		}
		break;

	case '+':
		npuNetSend(npuTp, fcBol, sizeof(fcBol) - 1, mfrId);
		break;

	case '*':
		if (npuTp->params.fvTC == TcX364)
		{
			/*
			**  Cursor Home (using ANSI/VT100 control sequences) for VT100.
			*/
			npuNetSend(npuTp, fcTofAnsi, sizeof(fcTofAnsi) - 1, mfrId);
		}
		else
		{
			/*
			**  Formfeed for any other terminal.
			*/
			npuNetSend(npuTp, fcTof, sizeof(fcTof) - 1, mfrId);
		}

		break;

	case '1':
		if (npuTp->params.fvTC == TcX364)
		{
			/*
			**  Cursor Home and Clear (using ANSI/VT100 control sequences) for VT100.
			*/
			npuNetSend(npuTp, fcClearHomeAnsi, sizeof(fcClearHomeAnsi) - 1, mfrId);
		}
		else
		{
			/*
			**  Formfeed for any other terminal.
			*/
			npuNetSend(npuTp, fcTof, sizeof(fcTof) - 1, mfrId);
		}

		break;

	case ',':
		/*
		**  Do not change position.
		*/
		break;
	}

	npuTp->lastOpWasInput = false;
}

/*--------------------------------------------------------------------------
**  Purpose:        Process format effector at end of line
**
**  Parameters:     Name        Description.
**                  fe          format effector
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void npuAsyncDoFeAfter(u8 fe, u8 mfrId)
{
	// ReSharper disable once CppDefaultCaseNotHandledInSwitchStatement
	switch (fe)
	{
	case '.':
		npuNetSend(npuTp, fcSingleSpace, sizeof(fcSingleSpace) - 1, mfrId);
		break;

	case '/':
		npuNetSend(npuTp, fcBol, sizeof(fcBol) - 1, mfrId);
		break;
	}
}

/*--------------------------------------------------------------------------
**  Purpose:        Process upline data from terminal.
**
**  Parameters:     Name        Description.
**                  tp          TCB pointer
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void npuAsyncProcessUplineTransparent(Tcb *tp, u8 mfrId)
{
	MMainFrame *mfr = BigIron->chasis[mfrId];

	u8 *dp = tp->inputData;
	int len = tp->inputCount;

	/*
	**  Cancel transparent input forwarding timeout.
	*/
	tp->xInputTimerRunning = false;

	/*
	**  Process transparent input.
	*/
	while (len--)
	{
		u8 ch = *dp++;

		if (tp->params.fvEchoplex)
		{
			*mfr->echoPtr++ = ch;
		}

		if (tp->params.fvXCharFlag && ch == tp->params.fvXChar)
		{
			if (!tp->params.fvXModeMultiple)
			{
				/*
				**  Terminate single message transparent mode.
				*/
				tp->params.fvXInput = false;
			}

			/*
			**  Send the upline data.
			*/
			tp->inBuf[BlkOffDbc] = DbcTransparent;
			npuBipRequestUplineCanned(tp->inBuf, static_cast<int>(tp->inBufPtr - tp->inBuf), mfrId);
			npuTipInputReset(tp);
		}
		else if (ch == tp->params.fvUserBreak2 && tp->params.fvEnaXUserBreak)
		{
			*tp->inBufPtr++ = ch;
			tp->inBuf[BlkOffDbc] = DbcTransparent;
			npuBipRequestUplineCanned(tp->inBuf, static_cast<int>(tp->inBufPtr - tp->inBuf), mfrId);
			npuTipInputReset(tp);
		}
		else
		{
			*tp->inBufPtr++ = ch;
			if (tp->inBufPtr - tp->inBufStart >= tp->params.fvXCnt
				|| tp->inBufPtr - tp->inBufStart >= MaxBuffer - BlkOffDbc - 2)
			{
				if (!tp->params.fvXModeMultiple)
				{
					/*
					**  Terminate single message transparent mode.
					*/
					tp->params.fvXInput = false;
				}

				/*
				**  Send the upline data.
				*/
				tp->inBuf[BlkOffDbc] = DbcTransparent;
				npuBipRequestUplineCanned(tp->inBuf, static_cast<int>(tp->inBufPtr - tp->inBuf), mfrId);
				npuTipInputReset(tp);
			}
		}
	}

	/*
	**  If data is pending, schedule transparent input forwarding timeout.
	*/
	if (tp->params.fvXTimeout && tp->inBufStart != tp->inBufPtr)
	{
		tp->xStartCycle = mfr->activeChannel->mfr->cycles;  // DRS??!!
		tp->xInputTimerRunning = true;
	}
}

/*--------------------------------------------------------------------------
**  Purpose:        Process upline data from terminal.
**
**  Parameters:     Name        Description.
**                  tp          TCB pointer
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void npuAsyncProcessUplineAscii(Tcb *tp, u8 mfrId)
{
	MMainFrame *mfr = BigIron->chasis[mfrId];

	u8 *dp = tp->inputData;
	int len = tp->inputCount;

	/*
	**  Process normalised input.
	*/
	tp->inBuf[BlkOffDbc] = 0;   // non-transparent data

	while (len--)
	{
		u8 ch = *dp++ & Mask7;

		/*
		**  Ignore the following characters when at the begin of a line.
		*/
		if (tp->inBufPtr - tp->inBufStart == 0)
		{
			// ReSharper disable once CppDefaultCaseNotHandledInSwitchStatement
			switch (ch)
			{
			case ChrNUL:
			case ChrLF:
			case ChrDEL:
				continue;
			}
		}

		if ((ch == ChrDC1 || ch == ChrDC3)
			&& tp->params.fvOutFlowControl)
		{
			/*
			**  Flow control characters if enabled.
			*/
			if (ch == ChrDC1)
			{
				/*
				**  XON (turn output on)
				*/
				tp->xoff = false;
			}
			else
			{
				/*
				**  XOFF (turn output off)
				*/
				tp->xoff = true;
			}

			continue;
		}

		if (ch == tp->params.fvCN
			|| ch == tp->params.fvEOL)
		{
			/*
			**  EOL or Cancel entered - send the input upline.
			*/
			*tp->inBufPtr++ = ch;
			npuBipRequestUplineCanned(tp->inBuf, static_cast<int>(tp->inBufPtr - tp->inBuf), mfrId);
			npuTipInputReset(tp);

			/*
			**  Optionally echo characters.
			*/
			if (tp->dbcNoEchoplex)
			{
				/*
				**  DBC prevented echoplex for this line.
				*/
				tp->dbcNoEchoplex = false;
				mfr->echoPtr = mfr->echoBuffer;
			}
			else
			{
				mfr->echoLen = static_cast<int>(mfr->echoPtr - mfr->echoBuffer);
				if (mfr->echoLen)
				{
					npuNetSend(tp, mfr->echoBuffer, mfr->echoLen, mfrId);
					mfr->echoPtr = mfr->echoBuffer;
				}
			}

			/*
			**  Perform cursor positioning.
			*/
			if (tp->dbcNoCursorPos)
			{
				tp->dbcNoCursorPos = false;
			}
			else
			{
				if (tp->params.fvCursorPos)
				{
					// ReSharper disable once CppDefaultCaseNotHandledInSwitchStatement
					switch (tp->params.fvEOLCursorPos)
					{
					case 0:
						break;

					case 1:
						*mfr->echoPtr++ = ChrCR;
						break;

					case 2:
						*mfr->echoPtr++ = ChrLF;
						break;

					case 3:
						*mfr->echoPtr++ = ChrCR;
						*mfr->echoPtr++ = ChrLF;
						break;
					}
				}
			}

			continue;
		}

		if (tp->params.fvEchoplex)
		{
			*mfr->echoPtr++ = ch;
		}

		/*
		**  Store the character for later transmission.
		*/
		*tp->inBufPtr++ = ch;

		if (tp->inBufPtr - tp->inBufStart >= (tp->params.fvBlockFactor * MaxIvtData))
		{
			/*
			**  Send long lines.
			*/
			tp->inBuf[BlkOffBTBSN] = BtHTBLK | (tp->uplineBsn << BlkShiftBSN);
			npuBipRequestUplineCanned(tp->inBuf, static_cast<int>(tp->inBufPtr - tp->inBuf), mfrId);
			npuTipInputReset(tp);
		}
	}
}

/*--------------------------------------------------------------------------
**  Purpose:        Process upline data from terminal.
**
**  Parameters:     Name        Description.
**                  tp          TCB pointer
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void npuAsyncProcessUplineSpecial(Tcb *tp, u8 mfrId)
{
	int i;
	MMainFrame *mfr = BigIron->chasis[mfrId];

	u8 *dp = tp->inputData;
	int len = tp->inputCount;

	/*
	**  Process normalised input.
	*/
	tp->inBuf[BlkOffDbc] = 0;   // non-transparent data

	while (len--)
	{
		u8 ch = *dp++ & Mask7;

		/*
		**  Ignore the following characters.
		*/
		// ReSharper disable once CppDefaultCaseNotHandledInSwitchStatement
		switch (ch)
		{
		case ChrNUL:
		case ChrDEL:
			continue;
		}

		/*
		**  Ignore the following characters when at the begin of a line.
		*/
		if (tp->inBufPtr - tp->inBufStart == 0)
		{
			// ReSharper disable once CppDefaultCaseNotHandledInSwitchStatement
			switch (ch)
			{
			case ChrSTX:
				continue;
			}
		}

		if ((ch == ChrDC1 || ch == ChrDC3)
			&& tp->params.fvOutFlowControl)
		{
			/*
			**  Flow control characters if enabled.
			*/
			if (ch == ChrDC1)
			{
				/*
				**  XON (turn output on)
				*/
				tp->xoff = false;
			}
			else
			{
				/*
				**  XOFF (turn output off)
				*/
				tp->xoff = true;
			}

			continue;
		}

		if (ch == tp->params.fvCN)
		{
			/*
			**  Cancel character entered - erase all characters entered
			**  and indicate it to user via "*DEL*". Use the echobuffer
			**  to build and send the sequence.
			*/
			mfr->echoPtr = mfr->echoBuffer;
			int cnt = static_cast<int>(tp->inBufPtr - tp->inBufStart);
			for (i = cnt; i > 0; i--)
			{
				*mfr->echoPtr++ = ChrBS;
			}

			for (i = cnt; i > 0; i--)
			{
				*mfr->echoPtr++ = ' ';
			}

			for (i = cnt; i > 0; i--)
			{
				*mfr->echoPtr++ = ChrBS;
			}

			*mfr->echoPtr++ = '*';
			*mfr->echoPtr++ = 'D';
			*mfr->echoPtr++ = 'E';
			*mfr->echoPtr++ = 'L';
			*mfr->echoPtr++ = '*';
			*mfr->echoPtr++ = '\r';
			*mfr->echoPtr++ = '\n';
			npuNetSend(tp, mfr->echoBuffer, static_cast<int>(mfr->echoPtr - mfr->echoBuffer), mfrId);

			/*
			**  Send the line, but signal the cancel character.
			*/
			tp->inBuf[BlkOffDbc] = DbcCancel;
			npuBipRequestUplineCanned(tp->inBuf, static_cast<int>(tp->inBufPtr - tp->inBuf), mfrId);

			/*
			**  Reset input and echoplex buffers.
			*/
			npuTipInputReset(tp);
			mfr->echoPtr = mfr->echoBuffer;
			continue;
		}

		if (ch == tp->params.fvUserBreak1)
		{
			/*
			**  User break 1 (typically Ctrl-I).
			*/
			npuTipSendUserBreak(tp, 1, mfrId);
			continue;
		}

		if (ch == tp->params.fvUserBreak2)
		{
			/*
			**  User break 2 (typically Ctrl-T).
			*/
			npuTipSendUserBreak(tp, 2, mfrId);
			continue;
		}

		if (tp->params.fvEchoplex)
		{
			*mfr->echoPtr++ = ch;
		}

		if (ch == tp->params.fvEOL)
		{
			/*
			**  EOL entered - send the input upline.
			*/
			npuBipRequestUplineCanned(tp->inBuf, static_cast<int>(tp->inBufPtr - tp->inBuf), mfrId);
			npuTipInputReset(tp);

			/*
			**  Optionally echo characters.
			*/
			if (tp->dbcNoEchoplex)
			{
				/*
				**  DBC prevented echoplex for this line.
				*/
				tp->dbcNoEchoplex = false;
				mfr->echoPtr = mfr->echoBuffer;
			}
			else
			{
				mfr->echoLen = static_cast<int>(mfr->echoPtr - mfr->echoBuffer);
				if (mfr->echoLen)
				{
					npuNetSend(tp, mfr->echoBuffer, mfr->echoLen, mfrId);
					mfr->echoPtr = mfr->echoBuffer;
				}
			}

			/*
			**  Perform cursor positioning.
			*/
			if (tp->dbcNoCursorPos)
			{
				tp->dbcNoCursorPos = false;
			}
			else
			{
				if (tp->params.fvCursorPos)
				{
					// ReSharper disable once CppDefaultCaseNotHandledInSwitchStatement
					switch (tp->params.fvEOLCursorPos)
					{
					case 0:
						break;

					case 1:
						npuNetSend(tp, netCR, sizeof(netCR), mfrId);
						break;

					case 2:
						npuNetSend(tp, netLF, sizeof(netLF), mfrId);
						break;

					case 3:
						npuNetSend(tp, netCRLF, sizeof(netCRLF), mfrId);
						break;
					}
				}
			}

			continue;
		}

		/*
		**  Store the character for later transmission.
		*/
		*tp->inBufPtr++ = ch;

		if (tp->inBufPtr - tp->inBufStart >= (tp->params.fvBlockFactor * MaxIvtData))
		{
			/*
			**  Send long lines.
			*/
			tp->inBuf[BlkOffBTBSN] = BtHTBLK | (tp->uplineBsn << BlkShiftBSN);
			npuBipRequestUplineCanned(tp->inBuf, static_cast<int>(tp->inBufPtr - tp->inBuf), mfrId);
			npuTipInputReset(tp);
		}
	}
}

/*--------------------------------------------------------------------------
**  Purpose:        Process upline data from terminal.
**
**  Parameters:     Name        Description.
**                  tp          TCB pointer
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void npuAsyncProcessUplineNormal(Tcb *tp, u8 mfrId)
{
	int i;
	MMainFrame *mfr = BigIron->chasis[mfrId];

	u8 *dp = tp->inputData;
	int len = tp->inputCount;

	/*
	**  Process normalised input.
	*/
	tp->inBuf[BlkOffDbc] = 0;   // non-transparent data

	while (len--)
	{
		u8 ch = *dp++ & Mask7;

		/*
		**  Ignore the following characters.
		*/
		// ReSharper disable once CppDefaultCaseNotHandledInSwitchStatement
		switch (ch)
		{
		case ChrNUL:
		case ChrLF:
		case ChrDEL:
			continue;
		}

		if ((ch == ChrDC1 || ch == ChrDC3)
			&& tp->params.fvOutFlowControl)
		{
			/*
			**  Flow control characters if enabled.
			*/
			if (ch == ChrDC1)
			{
				/*
				**  XON (turn output on)
				*/
				tp->xoff = false;
			}
			else
			{
				/*
				**  XOFF (turn output off)
				*/
				tp->xoff = true;
			}

			continue;
		}

		if (ch == tp->params.fvCN)
		{
			/*
			**  Cancel character entered - erase all characters entered
			**  and indicate it to user via "*DEL*". Use the echobuffer
			**  to build and send the sequence.
			*/
			mfr->echoPtr = mfr->echoBuffer;
			int cnt = static_cast<int>(tp->inBufPtr - tp->inBufStart);
			for (i = cnt; i > 0; i--)
			{
				*mfr->echoPtr++ = ChrBS;
			}

			for (i = cnt; i > 0; i--)
			{
				*mfr->echoPtr++ = ' ';
			}

			for (i = cnt; i > 0; i--)
			{
				*mfr->echoPtr++ = ChrBS;
			}

			*mfr->echoPtr++ = '*';
			*mfr->echoPtr++ = 'D';
			*mfr->echoPtr++ = 'E';
			*mfr->echoPtr++ = 'L';
			*mfr->echoPtr++ = '*';
			*mfr->echoPtr++ = '\r';
			*mfr->echoPtr++ = '\n';
			npuNetSend(tp, mfr->echoBuffer, static_cast<int>(mfr->echoPtr - mfr->echoBuffer), mfrId);

			/*
			**  Send the line, but signal the cancel character.
			*/
			tp->inBuf[BlkOffDbc] = DbcCancel;
			npuBipRequestUplineCanned(tp->inBuf, static_cast<int>(tp->inBufPtr - tp->inBuf), mfrId);

			/*
			**  Reset input and echoplex buffers.
			*/
			npuTipInputReset(tp);
			mfr->echoPtr = mfr->echoBuffer;
			continue;
		}

		if (ch == tp->params.fvUserBreak1)
		{
			/*
			**  User break 1 (typically Ctrl-I).
			*/
			npuTipSendUserBreak(tp, 1, mfrId);
			continue;
		}

		if (ch == tp->params.fvUserBreak2)
		{
			/*
			**  User break 2 (typically Ctrl-T).
			*/
			npuTipSendUserBreak(tp, 2, mfrId);
			continue;
		}

		if (tp->params.fvEchoplex)
		{
			*mfr->echoPtr++ = ch;
		}

		if (ch == tp->params.fvEOL)
		{
			/*
			**  EOL entered - send the input upline.
			*/
			npuBipRequestUplineCanned(tp->inBuf, static_cast<int>(tp->inBufPtr - tp->inBuf), mfrId);
			npuTipInputReset(tp);
			tp->lastOpWasInput = true;

			/*
			**  Optionally echo characters.
			*/
			if (tp->dbcNoEchoplex)
			{
				/*
				**  DBC prevented echoplex for this line.
				*/
				tp->dbcNoEchoplex = false;
				mfr->echoPtr = mfr->echoBuffer;
			}
			else
			{
				mfr->echoLen = static_cast<int>(mfr->echoPtr - mfr->echoBuffer);
				if (mfr->echoLen)
				{
					npuNetSend(tp, mfr->echoBuffer, mfr->echoLen, mfrId);
					mfr->echoPtr = mfr->echoBuffer;
				}
			}

			/*
			**  Perform cursor positioning.
			*/
			if (tp->dbcNoCursorPos)
			{
				tp->dbcNoCursorPos = false;
			}
			else
			{
				if (tp->params.fvCursorPos)
				{
					// ReSharper disable once CppDefaultCaseNotHandledInSwitchStatement
					switch (tp->params.fvEOLCursorPos)
					{
					case 0:
						break;

					case 1:
						npuNetSend(tp, netCR, sizeof(netCR), mfrId);
						break;

					case 2:
						npuNetSend(tp, netLF, sizeof(netLF), mfrId);
						break;

					case 3:
						npuNetSend(tp, netCRLF, sizeof(netCRLF), mfrId);
						break;
					}
				}
			}

			continue;
		}

		if (ch == tp->params.fvBS)
		{
			/*
			**  Process backspace.
			*/
			if (tp->inBufPtr > tp->inBufStart)
			{
				tp->inBufPtr -= 1;
				*mfr->echoPtr++ = ' ';
				*mfr->echoPtr++ = tp->params.fvBS;
			}
			else
			{
				/*
				**  Beep when trying to go past the start of line.
				*/
				npuNetSend(tp, netBEL, 1, mfrId);
			}

			continue;
		}

		/*
		**  Store the character for later transmission.
		*/
		*tp->inBufPtr++ = ch;

		if (tp->inBufPtr - tp->inBufStart >= (tp->params.fvBlockFactor * MaxIvtData))
		{
			/*
			**  Send long lines.
			*/
			tp->inBuf[BlkOffBTBSN] = BtHTBLK | (tp->uplineBsn << BlkShiftBSN);
			npuBipRequestUplineCanned(tp->inBuf, static_cast<int>(tp->inBufPtr - tp->inBuf), mfrId);
			npuTipInputReset(tp);
		}
	}
}

/*---------------------------  End Of File  ------------------------------*/
