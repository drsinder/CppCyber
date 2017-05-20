/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2011, Tom Hunter
**  C++ adaptation by Dale Sinder 2017
**
**  Name: mt607.c
**
**  Description:
**      Perform emulation of CDC 6600 607 tape drives.
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
**  CDC 607 tape function and status codes.
**
**  200x    Select
**  201x    Write Binary
**  202x    Read Binary
**  203x    Backspace
**  206x    Rewind
**  207x    Rewind/Unload
**  2100    Status request
**  221x    Write BCD
**  222x    Read BCD
**  261x    Write File Mark
**
**  (x = unit = 0 - 7)
*/
#define Fc607UnitMask           07770

#define Fc607SelUnitCode        02000
#define Fc607WrBinary           02010
#define Fc607RdBinary           02020
#define Fc607Backspace          02030
#define Fc607Rewind             02060
#define Fc607RewindUnload       02070
#define Fc607StatusReq          02100
#define Fc607WrBCD              02210
#define Fc607RdBCD              02220
#define Fc607WrFileMark         02610

/*
**
**  Status Reply:
**
**  0x00 = Ready
**  0x01 = Not Ready
**  0x02 = Parity Error
**  0x04 = Load Point
**  0x10 = End of Tape
**  0x20 = File Mark
**  0x40 = Write Lockout
**
**  x = 0: 800 bpi
**  x = 1: 556 bpi
**  x = 2: 200 bpi
**
*/
#define St607DensityMask        0700

#define St607Ready              0
#define St607NotReadyMask       001
#define St607ParityErrorMask    002
#define St607LoadPoint          004
#define St607EOT                010
#define St607FileMark           020
#define St607WriteLockout       040
/*
**  Misc constants.
*/
#define MaxPpBuf                010000
#define MaxByteBuf              014000

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
typedef struct tapeBuf
{
	PpWord      ioBuffer[MaxPpBuf];
	PpWord      *bp;
} TapeBuf;

/*
**  ---------------------------
**  Private Function Prototypes
**  ---------------------------
*/
static FcStatus mt607Func(PpWord funcCode, u8 mfrId);
static void mt607Io(u8 mfrId);
static void mt607Activate(u8 mfrId);
static void mt607Disconnect(u8 mfrId);
static char *mt607Func2String(PpWord funcCode);

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
static u8 rawBuffer[MaxByteBuf];

#if DEBUG
static FILE *mt607Log = NULL;
#endif

/*
**--------------------------------------------------------------------------
**
**  Public Functions
**
**--------------------------------------------------------------------------
*/
/*--------------------------------------------------------------------------
**  Purpose:        Initialise 607 tape drives.
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
void mt607Init(u8 mfrID, u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName)
{
	DevSlot *dp;
	char fname[80];

	(void)eqNo;

#if DEBUG
	if (mt607Log == NULL)
	{
		mt607Log = fopen("mt607log.txt", "wt");
	}
#endif

	/*
	**  Attach device to channel.
	*/
	dp = channelAttach(channelNo, eqNo, DtMt607, mfrID);

	/*
	**  Setup channel functions.
	*/
	dp->activate = mt607Activate;
	dp->disconnect = mt607Disconnect;
	dp->func = mt607Func;
	dp->io = mt607Io;
	dp->selectedUnit = unitNo;

	/*
	**  Setup controller context.
	*/
	dp->context[unitNo] = calloc(1, sizeof(TapeBuf));
	if (dp->context[unitNo] == NULL)
	{
		fprintf(stderr, "Failed to allocate MT607 context block\n");
		exit(1);
	}

	/*
	**  Open the device file.
	*/
	if (deviceName == NULL)
	{
		sprintf(fname, "MT607_C%02o_U%o.tap", channelNo, unitNo);
	}
	else
	{
		strcpy(fname, deviceName);
	}

	dp->fcb[unitNo] = fopen(fname, "rb");
	if (dp->fcb[unitNo] == NULL)
	{
		fprintf(stderr, "Failed to open %s\n", fname);
		exit(1);
	}

	/*
	**  Print a friendly message.
	*/
	printf("MT607 initialised on channel %o unit %o)\n", channelNo, unitNo);
}

/*--------------------------------------------------------------------------
**  Purpose:        Execute function code on 607 tape drives.
**
**  Parameters:     Name        Description.
**                  funcCode    function code
**
**  Returns:        FcStatus
**
**------------------------------------------------------------------------*/
static FcStatus mt607Func(PpWord funcCode, u8 mfrId)
{
	u32 len;
	u32 recLen0;
	u32 recLen1;
	u32 recLen2;
	u32 i;
	u16 c1, c2, c3;
	u16 *op;
	u8 *rp;
	TapeBuf *tp;

	MMainFrame *mfr = BigIron->chasis[mfrId];

#if DEBUG
	fprintf(mt607Log, "\n%06d PP:%02o CH:%02o u:%d f:%04o T:%-25s  >   ",
		traceSequenceNo,
		activePpu->id,
		activeDevice->channel->id,
		activeDevice->selectedUnit,
		funcCode,
		mt607Func2String(funcCode));
#endif

	switch (funcCode & Fc607UnitMask)
	{
	default:
#if DEBUG
		fprintf(mt607Log, " FUNC not implemented & declined!");
#endif
		return(FcDeclined);

	case Fc607WrBinary:
	case Fc607Backspace:
	case Fc607RewindUnload:
	case Fc607WrBCD:
	case Fc607RdBCD:
	case Fc607WrFileMark:
		mfr->activeDevice->fcode = 0;
		logError(LogErrorLocation, "channel %02o - unsupported function code: %04o", mfr->activeChannel->id, (u32)funcCode);
		break;

	case Fc607Rewind:
		mfr->activeDevice->fcode = 0;
		fseek(mfr->activeDevice->fcb[mfr->activeDevice->selectedUnit], 0, SEEK_SET);
		break;

	case Fc607StatusReq:
		mfr->activeDevice->fcode = funcCode;
		break;

	case Fc607SelUnitCode:
		mfr->activeDevice->fcode = 0;
		mfr->activeDevice->selectedUnit = funcCode & 07;
		if (mfr->activeDevice->fcb[mfr->activeDevice->selectedUnit] == NULL)
		{
			logError(LogErrorLocation, "channel %02o - invalid select: %04o", mfr->activeChannel->id, (u32)funcCode);
		}
		break;

	case Fc607RdBinary:
		mfr->activeDevice->fcode = funcCode;
		if (mfr->activeDevice->recordLength > 0)
		{
			mfr->activeChannel->status = St607Ready;
			break;
		}

		mfr->activeChannel->status = St607Ready;

		/*
		**  Reset tape buffer pointer.
		*/
		tp = (TapeBuf *)mfr->activeDevice->context[mfr->activeDevice->selectedUnit];
		tp->bp = tp->ioBuffer;

		/*
		**  Read and verify TAP record length header.
		*/
		len = (u32)fread(&recLen0, sizeof(recLen0), 1, mfr->activeDevice->fcb[mfr->activeDevice->selectedUnit]);

		if (len != 1)
		{
			mfr->activeChannel->status = St607EOT;
			mfr->activeDevice->recordLength = 0;
			break;
		}

		/*
		**  The TAP record length is little endian - convert if necessary.
		*/
		if (BigIron->bigEndian)
		{
			recLen1 = MSystem::ConvertEndian(recLen0);
		}
		else
		{
			recLen1 = recLen0;
		}

		if (recLen1 == 0)
		{
			mfr->activeDevice->recordLength = 0;
#if DEBUG
			fprintf(mt607Log, "Tape mark\n");
#endif
			break;
		}

		if (recLen1 > MaxByteBuf)
		{
			logError(LogErrorLocation, "channel %02o - tape record too long: %d", mfr->activeChannel->id, recLen1);
			mfr->activeChannel->status = St607NotReadyMask;
			mfr->activeDevice->recordLength = 0;
			break;
		}

		/*
		**  Read and verify the actual raw data.
		*/
		len = (u32)fread(rawBuffer, 1, recLen1, mfr->activeDevice->fcb[mfr->activeDevice->selectedUnit]);

		if (recLen1 != (u32)len)
		{
			logError(LogErrorLocation, "channel %02o - short tape record read: %d", mfr->activeChannel->id, len);
			mfr->activeChannel->status = St607NotReadyMask;
			mfr->activeDevice->recordLength = 0;
			break;
		}

		/*
		**  Read and verify the TAP record length trailer.
		*/
		len = (u32)fread(&recLen2, sizeof(recLen2), 1, mfr->activeDevice->fcb[mfr->activeDevice->selectedUnit]);

		if (len != 1 || recLen0 != recLen2)
		{
			logError(LogErrorLocation, "channel %02o - invalid tape record trailer: %08x", mfr->activeChannel->id, recLen2);
			mfr->activeChannel->status = St607NotReadyMask;
			mfr->activeDevice->recordLength = 0;
			break;
		}

		/*
		**  Convert the raw data into PP words suitable for a channel.
		*/
		op = tp->ioBuffer;
		rp = rawBuffer;

		for (i = 0; i < recLen1; i += 3)
		{
			c1 = *rp++;
			c2 = *rp++;
			c3 = *rp++;

			*op++ = ((c1 << 4) | (c2 >> 4)) & Mask12;
			*op++ = ((c2 << 8) | (c3 >> 0)) & Mask12;
		}

		mfr->activeDevice->recordLength = (PpWord)(op - tp->ioBuffer);
		mfr->activeChannel->status = St607Ready;

#if DEBUG
		fprintf(mt607Log, "Read fwd %d PP words (%d 8-bit bytes)\n", activeDevice->recordLength, recLen1);
#endif
		break;
	}

	return(FcAccepted);
}

/*--------------------------------------------------------------------------
**  Purpose:        Perform I/O on MT607.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void mt607Io(u8 mfrId)
{
	TapeBuf *tp;
	MMainFrame *mfr = BigIron->chasis[mfrId];

	switch (mfr->activeDevice->fcode & Fc607UnitMask)
	{
	default:
	case Fc607SelUnitCode:
	case Fc607WrBinary:
	case Fc607Backspace:
	case Fc607Rewind:
	case Fc607RewindUnload:
	case Fc607WrBCD:
	case Fc607RdBCD:
	case Fc607WrFileMark:
		logError(LogErrorLocation, "channel %02o - unsupported function code: %04o", mfr->activeChannel->id, mfr->activeDevice->fcode);
		break;

	case Fc607StatusReq:
		mfr->activeChannel->data = mfr->activeChannel->status;
		mfr->activeChannel->full = TRUE;
#if DEBUG
		fprintf(mt607Log, " %04o", activeChannel->data);
#endif
		break;

	case Fc607RdBinary:
		if (mfr->activeChannel->full)
		{
			break;
		}

		if (mfr->activeDevice->recordLength == 0)
		{
			mfr->activeChannel->active = FALSE;
		}

		tp = (TapeBuf *)mfr->activeDevice->context[mfr->activeDevice->selectedUnit];

		if (mfr->activeDevice->recordLength > 0)
		{
			mfr->activeDevice->recordLength -= 1;
			mfr->activeChannel->data = *tp->bp++;
			mfr->activeChannel->full = TRUE;
			if (mfr->activeDevice->recordLength == 0)
			{
				//                activeChannel->discAfterInput = TRUE;  <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< fixed COS 
			}
		}
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
static void mt607Activate(u8 mfrId)
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
static void mt607Disconnect(u8 mfrId)
{
	/*
	**  Abort pending device disconnects - the PP is doing the disconnect.
	*/
	MMainFrame *mfr = BigIron->chasis[mfrId];

	mfr->activeChannel->discAfterInput = FALSE;
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
static char *mt607Func2String(PpWord funcCode)
{
	static char buf[30];
#if DEBUG
	switch (funcCode)
	{
	case Fc607SelUnitCode: return "Fc607SelUnitCode";
	case Fc607WrBinary: return "Fc607WrBinary";
	case Fc607RdBinary: return "Fc607RdBinary";
	case Fc607Backspace: return "Fc607Backspace";
	case Fc607Rewind: return "Fc607Rewind";
	case Fc607RewindUnload: return "Fc607RewindUnload";
	case Fc607StatusReq: return "Fc607StatusReq";
	case Fc607WrBCD: return "Fc607WrBCD";
	case Fc607RdBCD: return "Fc607RdBCD";
	case Fc607WrFileMark: return "Fc607WrFileMark";
	}
#endif
	sprintf(buf, "UNKNOWN: %04o", funcCode);
	return(buf);
}

/*---------------------------  End Of File  ------------------------------*/
