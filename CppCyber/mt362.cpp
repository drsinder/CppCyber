/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2011, Tom Hunter
**  C++ adaptation by Dale Sinder 2017
**
**  Name: mt362x.cpp
**
**  Description:
**      Perform emulation of CDC 607 7-track tape drives attached to a
**      362x magnetic tape controller. Also supports 9-track tape images
**      by using a fictious 609 9-track drive.
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
#include "dcc6681.h"

/*
**  -----------------
**  Private Constants
**  -----------------
*/

/*
**  CDC 362x tape function codes.
*/
#define Fc362xRelease			00000
#define Fc362xSelectBinary		00001
#define Fc362xSelectCoded		00002
#define Fc362xSelect556Bpi		00003
#define Fc362xSelect200Bpi		00004
#define Fc362xClear				00005
#define Fc362xSelect800Bpi		00006
#define Fc362xRewind			00010
#define Fc362xRewindUnload		00011
#define Fc362xBackspace			00012
#define Fc362xSearchFwdFileMark	00013
#define Fc362xSearchBckFileMark	00014
#define Fc362xWriteFileMark		00015
#define Fc362xSkipBadSpot		00016
#define Fc362xSelectIntReady	00020
#define Fc362xReleaseIntReady	00021
#define Fc362xSelectIntEndOfOp	00022
#define Fc362xReleaseIntEndOfOp	00023
#define	Fc362xSelectIntError	00024
#define Fc362xReleaseIntError	00025
#define Fc362xClearReverseRead	00040
#define Fc362xSetReverseRead	00041

/*
**  CDC 362x tape status bits.
*/
#define St362xReady             00001
#define St362xBusy				00002
#define St362xWriteEnable		00004
#define St362xFileMark			00010
#define St362xLoadPoint			00020
#define St362xEndOfTape			00040
#define St362xDensity200Bpi		00000
#define St362xDensity556Bpi		00100
#define St362xDensity800Bpi		00200
#define St362xLostData			00400
#define St362xEndOfOperation	01000
#define St362xParityError		02000
#define St362xUnitReserved		04000

#define Int362xReady			00001
#define Int362xEndOfOp			00002
#define Int362xError			00004

#define St362xReadyMask			(St362xReady | St362xBusy)
#define St362xWriteMask			00007	// Also includes Busy, Ready
#define St362xWriteReady		00005
#define St362xNonDensityMask	07475
#define St362xConnectClr		03367
#define St362xClearMask			01765	// Clears Parity, File Mark, Busy
#define St362xMstrClrMask		01365
#define St362xTpMotionClr		03305
#define St362xDensityParity		03300
#define St362xRWclear			01305
#define St362xClearBusy			07775

/*
**  Misc constants.
*/
#define MaxPpBuf                40000
#define MaxByteBuf              60000
#define MaxTapeSize             1250000000   // this may need adjusting for shorter real tapes

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
**  362x tape unit.
*/
typedef struct tapeParam
{
	/*
	**  Info for show_tape operator command.
	*/
	struct tapeParam * nextTape;
	u8          channelNo;
	u8          eqNo;
	u8          unitNo;
	char        fileName[_MAX_PATH + 1];

	/*
	**  Format parameters.
	*/
	u8          tracks;

	/*
	**  Tape status variables.
	*/
	PpWord		intMask;
	PpWord		intStatus;
	PpWord		status;

	bool		bcdMode;
	bool		reverseRead;
	bool        writing;

	bool        unitReady;
	bool        busy;
	bool        ringIn;
	bool        fileMark;
	u32         blockNo;
	bool        endOfTape;
	u16         density;
	bool        lostData;
	bool        endOfOperation;
	bool        parityError;
	bool        reserved;

	bool        rewinding;
	u32         rewindStart;

	/*
	**  I/O buffer.
	*/
	PpWord      recordLength;
	PpWord      ioBuffer[MaxPpBuf];
	PpWord      *bp;
} TapeParam;

/*
**  ---------------------------
**  Private Function Prototypes
**  ---------------------------
*/
static void mt362xInit(u8 mfrID, u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName, u8 tracks);
static void mt362xInitStatus(TapeParam *tp);
static void mt362xResetStatus(TapeParam *tp);
static void mt362xSetupStatus(TapeParam *tp, u8 mfrId);
static FcStatus mt362xFunc(PpWord funcCode, u8 mfrId);
static void mt362xIo(u8 mfrId);
static void mt362xActivate(u8 mfrId);
static void mt362xDisconnect(u8 mfrId);
static void mt362xFuncRead(u8 mfrId);
static void mt362xFuncReadBkw(u8 mfrId);
static void mt362xFuncForespace(u8 mfrId);
static void mt362xFuncBackspace(u8 mfrId);
static void mt362xPackAndConvert(u32 recLen, u8 mfrId);
static void mt362xUnload(TapeParam *tp, u8 mfrId);
static char *mt362xFunc2String(PpWord funcCode);

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
static TapeParam *firstTape = nullptr;
static TapeParam *lastTape = nullptr;
static u8 rawBuffer[MaxByteBuf];

#if DEBUG
static FILE *mt362xLog = nullptr;

#define OctalColumn(x) (5 * (x) + 1 + 5)
#define AsciiColumn(x) (OctalColumn(5) + 2 + (2 * x))
#define LogLineLength (AsciiColumn(5))

static void mt362xLogFlush(void);
static void mt362xLogByte(int b);
static char mt362xLogBuf[LogLineLength + 1];
static int mt362xLogCol = 0;

/*--------------------------------------------------------------------------
**  Purpose:        Flush incomplete numeric/ascii data line
**
**  Parameters:     Name        Description.
**
**  Returns:        nothing
**
**------------------------------------------------------------------------*/
static void mt362xLogFlush(void)
{
	if (mt362xLogCol != 0)
	{
		fputs(mt362xLogBuf, mt362xLog);
	}

	mt362xLogCol = 0;
	memset(mt362xLogBuf, ' ', LogLineLength);
	mt362xLogBuf[0] = '\n';
	mt362xLogBuf[LogLineLength] = '\0';
}

/*--------------------------------------------------------------------------
**  Purpose:        Log a byte in octal/ascii form
**
**  Parameters:     Name        Description.
**
**  Returns:        nothing
**
**------------------------------------------------------------------------*/
static void mt362xLogByte(int b)
{
	char octal[10];
	int col;

	col = OctalColumn(mt362xLogCol);
	sprintf(octal, "%04o ", b);
	memcpy(mt362xLogBuf + col, octal, 5);

	col = AsciiColumn(mt362xLogCol);
	mt362xLogBuf[col + 0] = cdcToAscii[(b >> 6) & Mask6];
	mt362xLogBuf[col + 1] = cdcToAscii[(b >> 0) & Mask6];
	if (++mt362xLogCol == 5)
	{
		mt362xLogFlush();
	}
}
#endif

/*
**--------------------------------------------------------------------------
**
**  Public Functions
**
**--------------------------------------------------------------------------
*/
/*--------------------------------------------------------------------------
**  Purpose:        Initialise tape drive types on 362x controller(7 or 9 track)
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
void mt362xInit_7(u8 mfrId, u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName)
{
	mt362xInit(mfrId, eqNo, unitNo, channelNo, deviceName, 7);
}

void mt362xInit_9(u8 mfrID, u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName)
{
	mt362xInit(mfrID, eqNo, unitNo, channelNo, deviceName, 9);
}

/*--------------------------------------------------------------------------
**  Purpose:        Initialise 362x connected tape drives.
**
**  Parameters:     Name        Description.
**                  eqNo        equipment number
**                  unitNo      unit number
**                  channelNo   channel number the device is attached to
**                  deviceName  optional device file name
**                  track       number of tracks
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void mt362xInit(u8 mfrID, u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName, u8 tracks)
{
#if DEBUG
	if (mt362xLog == nullptr)
	{
		mt362xLog = fopen("mt362xlog.txt", "wt");
	}
#endif

	/*
	** Attach 362x controller to converter (create if necessary).
	*/
	DevSlot *dp = dcc6681Attach(channelNo, eqNo, unitNo, DtMt362x, mfrID);
	dp->activate = mt362xActivate;
	dp->disconnect = mt362xDisconnect;
	dp->func = mt362xFunc;
	dp->io = mt362xIo;

	/*
	**  Check if unit has already been configured.
	*/
	if (unitNo >= MaxUnits2 || dp->context[unitNo] != nullptr)
	{
		fprintf(stderr, "Invalid or duplicate MT372x unit number\n");
		exit(1);
	}

	/*
	**  Setup tape unit parameter block.
	*/
	TapeParam *tp = static_cast<TapeParam*>(calloc(1, sizeof(TapeParam)));
	if (tp == nullptr)
	{
		fprintf(stderr, "Failed to allocate MT362x tape unit context block\n");
		exit(1);
	}

	dp->context[unitNo] = tp;
	tp->tracks = tracks;

	/*
	**  Link into list of tape units.
	*/
	if (lastTape == nullptr)
	{
		firstTape = tp;
	}
	else
	{
		lastTape->nextTape = tp;
	}

	lastTape = tp;

	/*
	**  Open TAP container if file name was specified.
	*/
	if (deviceName != nullptr)
	{
		strncpy(tp->fileName, deviceName, _MAX_PATH);
		FILE *fcb = fopen(deviceName, "rb");
		if (fcb == nullptr)
		{
			fprintf(stderr, "Failed to open %s\n", deviceName);
			exit(1);
		}

		dp->fcb[unitNo] = fcb;

		tp->blockNo = 0;
		tp->unitReady = true;
		tp->status = St362xReady | St362xLoadPoint;
	}
	else
	{
		dp->fcb[unitNo] = nullptr;
		tp->unitReady = false;
		tp->status = 0;
	}

	/*
	**  Setup show_tape values.
	*/
	tp->channelNo = channelNo;
	tp->eqNo = eqNo;
	tp->unitNo = unitNo;

	/*
	**  All initially mounted tapes are read only.
	*/
	tp->ringIn = false;

	/*
	**  Print a friendly message.
	*/
	printf("MT362x initialized on channel %o equipment %o unit %o\n", channelNo, eqNo, unitNo);
}

/*--------------------------------------------------------------------------
**  Purpose:        Load a new tape (operator interface).
**
**  Parameters:     Name        Description.
**                  params      parameters
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void mt362xLoadTape(char *params)
{
	static char str[200];
	int channelNo;
	int equipmentNo;
	int unitNo;
	int mfrID;
	FILE *fcb;
	u8 unitMode;

	/*
	**  Operator inserted a new tape.
	*/
	int numParam = sscanf(params, "%o,%o,%o,%o,%c,%s", &mfrID, &channelNo, &equipmentNo, &unitNo, &unitMode, str);

	/*
	**  Check parameters.
	*/
	if (numParam != 6)
	{
		printf("Not enough or invalid parameters\n");
		return;
	}

	if (channelNo < 0 || channelNo >= MaxChannels)
	{
		printf("Invalid channel no\n");
		return;
	}

	if (unitNo < 0 || unitNo >= MaxUnits2)
	{
		printf("Invalid unit no\n");
		return;
	}

	if (unitMode != 'w' && unitMode != 'r')
	{
		printf("Invalid ring mode (r/w)\n");
		return;
	}

	if (str[0] == 0)
	{
		printf("Invalid file name\n");
		return;
	}

	/*
	**  Locate the device control block.
	*/
	DevSlot *dp = dcc6681FindDevice(static_cast<u8>(mfrID), static_cast<u8>(channelNo), static_cast<u8>(equipmentNo), DtMt362x);
	if (dp == nullptr)
	{
		return;
	}

	/*
	**  Check if the unit is even configured.
	*/
	TapeParam *tp = static_cast<TapeParam *>(dp->context[unitNo]);
	if (tp == nullptr)
	{
		printf("Unit %d not allocated\n", unitNo);
		return;
	}

	/*
	**  Check if the unit has been unloaded.
	*/
	if (dp->fcb[unitNo] != nullptr)
	{
		printf("Unit %d not unloaded\n", unitNo);
		return;
	}

	/*
	**  Open the file in the requested mode.
	*/
	if (unitMode == 'w')
	{
		fcb = fopen(str, "r+b");
		if (fcb == nullptr)
		{
			fcb = fopen(str, "w+b");
		}
	}
	else
	{
		fcb = fopen(str, "rb");
	}

	dp->fcb[unitNo] = fcb;

	/*
	**  Check if the open succeeded.
	*/
	if (fcb == nullptr)
	{
		printf("Failed to open %s\n", str);
		return;
	}

	/*
	**  Setup show_tape path name.
	*/
	strncpy(tp->fileName, str, _MAX_PATH);

	/*
	**  Setup status.
	*/
	mt362xInitStatus(tp);
	tp->unitReady = true;
	tp->ringIn = unitMode == 'w';

	printf("Successfully loaded %s\n", str);
}

/*--------------------------------------------------------------------------
**  Purpose:        Unload a mounted tape (operator interface).
**
**  Parameters:     Name        Description.
**                  params      parameters
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void mt362xUnloadTape(char *params)
{
	int channelNo;
	int equipmentNo;
	int unitNo;
	int mfrID;

	/*
	**  Operator inserted a new tape.
	*/
	int numParam = sscanf(params, "%o,%o,%o,%o", &mfrID, &channelNo, &equipmentNo, &unitNo);

	/*
	**  Check parameters.
	*/
	if (numParam != 4)
	{
		printf("Not enough or invalid parameters\n");
		return;
	}

	if (channelNo < 0 || channelNo >= MaxChannels)
	{
		printf("Invalid channel no\n");
		return;
	}

	if (unitNo < 0 || unitNo >= MaxUnits2)
	{
		printf("Invalid unit no\n");
		return;
	}

	/*
	**  Locate the device control block.
	*/
	DevSlot *dp = dcc6681FindDevice(static_cast<u8>(mfrID), static_cast<u8>(channelNo), static_cast<u8>(equipmentNo), DtMt362x);
	if (dp == nullptr)
	{
		return;
	}

	/*
	**  Check if the unit is even configured.
	*/
	TapeParam *tp = static_cast<TapeParam *>(dp->context[unitNo]);
	if (tp == nullptr)
	{
		printf("Unit %d not allocated\n", unitNo);
		return;
	}

	/*
	**  Check if the unit has been unloaded.
	*/
	if (dp->fcb[unitNo] == nullptr)
	{
		printf("Unit %d not loaded\n", unitNo);
		return;
	}

	/*
	**  Close the file.
	*/
	fclose(dp->fcb[unitNo]);
	dp->fcb[unitNo] = nullptr;

	/*
	**  Clear show_tape path name.
	*/
	memset(tp->fileName, '0', _MAX_PATH);

	/*
	**  Setup status.
	*/
	mt362xInitStatus(tp);

	printf("Successfully unloaded MT362x on channel %o equipment %o unit %o\n", channelNo, equipmentNo, unitNo);
}

/*--------------------------------------------------------------------------
**  Purpose:        Show tape status (operator interface).
**
**  Parameters:     Name        Description.
**
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void mt362xShowTapeStatus()
{
	TapeParam *tp = firstTape;

	while (tp)
	{
		printf("MT362x-%d on %o,%o,%o", tp->tracks, tp->channelNo, tp->eqNo, tp->unitNo);
		if (tp->unitReady)
		{
			printf(",%c,%s\n", tp->ringIn ? 'w' : 'r', tp->fileName);
		}
		else
		{
			printf("  (idle)\n");
		}

		tp = tp->nextTape;
	}
}

/*--------------------------------------------------------------------------
**  Purpose:        Reset device status at start of new function.
**
**  Parameters:     Name        Description.
**                  tp          pointer to tape parameters
**
**  Returns:        Nothing
**
**------------------------------------------------------------------------*/
static void mt362xInitStatus(TapeParam *tp)
{
	tp->bcdMode = false;
	tp->reverseRead = false;
	tp->writing = false;

	tp->unitReady = false;
	tp->busy = false;
	tp->ringIn = false;
	tp->fileMark = false;
	tp->blockNo = 0;
	tp->endOfTape = false;
	tp->density = 800;
	tp->lostData = false;
	tp->endOfOperation = false;
	tp->parityError = false;
	tp->reserved = false;

	tp->rewinding = false;
	tp->rewindStart = false;
}

/*--------------------------------------------------------------------------
**  Purpose:        Reset device status at start of new function.
**
**  Parameters:     Name        Description.
**                  tp          pointer to tape parameters
**
**  Returns:        Nothing
**
**------------------------------------------------------------------------*/
static void mt362xResetStatus(TapeParam *tp)
{
	tp->busy = false;
	tp->fileMark = false;
	tp->endOfTape = false;
	tp->lostData = false;
	tp->endOfOperation = false;
	tp->parityError = false;
	tp->reserved = false;
}

/*--------------------------------------------------------------------------
**  Purpose:        Setup general status based on current tape parameters.
**
**  Parameters:     Name        Description.
**                  tp          pointer to tape parameters
**
**  Returns:        Nothing
**
**------------------------------------------------------------------------*/
static void mt362xSetupStatus(TapeParam *tp, u8 mfrId)
{
	MMainFrame *mfr = BigIron->chasis[mfrId];
	tp->status = 0;

	if (tp->rewinding)
	{
		if (labs(mfr->active3000Device->mfr->cycles - tp->rewindStart) > 1000)
		{
			tp->rewinding = false;
			tp->blockNo = 0;
			tp->endOfOperation = true;
			tp->intStatus |= Int362xEndOfOp;
		}
		else
		{
			tp->busy = true;
		}
	}
	else
	{
		if (mfr->active3000Device->selectedUnit != -1)
		{
			if (tp->unitReady)
			{
				if (ftell(mfr->active3000Device->fcb[mfr->active3000Device->selectedUnit]) > MaxTapeSize)
				{
					tp->endOfTape = true;
				}
			}
		}
	}

	if (tp->unitReady)
	{
		tp->status |= St362xReady;
	}

	if (tp->busy)
	{
		tp->status |= St362xBusy;
	}

	if (tp->ringIn)
	{
		tp->status |= St362xWriteEnable;
	}

	if (tp->fileMark)
	{
		tp->status |= St362xFileMark;
	}

	if (tp->blockNo == 0)
	{
		tp->status |= St362xLoadPoint;
	}

	if (tp->endOfTape)
	{
		tp->status |= St362xEndOfTape;
	}

	switch (tp->density)
	{
	case 200:
		tp->status |= St362xDensity200Bpi;
		break;

	case 556:
		tp->status |= St362xDensity556Bpi;
		break;

	case 800:
	default:
		tp->status |= St362xDensity800Bpi;
		break;
	}

	if (tp->lostData)
	{
		tp->status |= St362xLostData;
	}

	if (tp->endOfOperation)
	{
		tp->status |= St362xEndOfOperation;
	}

	if (tp->parityError)
	{
		tp->status |= St362xParityError;
	}

	if (tp->reserved)
	{
		tp->status |= St362xUnitReserved;
	}
}

/*--------------------------------------------------------------------------
**  Purpose:        Execute function code on 362x tape controller.
**
**  Parameters:     Name        Description.
**                  funcCode    function code
**
**  Returns:        FcStatus
**
**------------------------------------------------------------------------*/
static FcStatus mt362xFunc(PpWord funcCode, u8 mfrId)
{
	TapeParam *tp;
	FcStatus st;

	MMainFrame *mfr = BigIron->chasis[mfrId];

	i8 unitNo = mfr->active3000Device->selectedUnit;
	if (unitNo != -1 && unitNo < MaxUnits2)
	{
		tp = static_cast<TapeParam *>(mfr->active3000Device->context[unitNo]);
	}
	else
	{
		tp = nullptr;
	}

	if (tp == nullptr)
	{
		return(FcDeclined);
	}

#if DEBUG
	mt362xLogFlush();
	fprintf(mt362xLog, "\n%06d PP:%02o CH:%02o Unit:%02o f:%04o T:%-25s  >   ",
		traceSequenceNo,
		activePpu->id,
		activeDevice->channel->id,
		unitNo,
		funcCode,
		mt362xFunc2String(funcCode));
#endif


	switch (funcCode)
	{
	case Fc362xRelease:
		mfr->active3000Device->selectedUnit = -1;
		st = FcProcessed;
		break;

	case Fc362xSelectBinary:
		tp->bcdMode = false;
		st = FcProcessed;
		break;

	case Fc362xSelectCoded:
		tp->bcdMode = true;
		st = FcProcessed;
		break;

	case Fc362xSelect200Bpi:
		tp->density = 200;
		st = FcProcessed;
		break;

	case Fc362xSelect556Bpi:
		tp->density = 556;
		st = FcProcessed;
		break;

	case Fc362xSelect800Bpi:
		tp->density = 800;
		st = FcProcessed;
		break;

	case Fc362xClear:
		mfr->active3000Device->selectedUnit = -1;
		st = FcProcessed;
		break;

	case Fc362xRewind:
		if (tp->unitReady)
		{
			mt362xResetStatus(tp);
			fseek(mfr->active3000Device->fcb[unitNo], 0, SEEK_SET);
			if (tp->blockNo != 0)
			{
				if (!tp->rewinding)
				{
					tp->rewinding = true;
					tp->rewindStart = mfr->active3000Device->mfr->cycles;
				}
			}

			tp->busy = true;
		}

		st = FcProcessed;
		break;

	case Fc362xRewindUnload:
		if (tp->unitReady)
		{
			mt362xResetStatus(tp);
			tp->blockNo = 0;
			tp->unitReady = false;
			tp->ringIn = false;
			fclose(mfr->active3000Device->fcb[unitNo]);
			mfr->active3000Device->fcb[unitNo] = nullptr;
			tp->endOfOperation = true;
			tp->intStatus |= Int362xEndOfOp;
		}

		st = FcProcessed;
		break;

	case Fc362xBackspace:
		if (tp->unitReady)
		{
			if (tp->reverseRead)
				mt362xFuncForespace(mfrId);
			else
				mt362xFuncBackspace(mfrId);

			tp->endOfOperation = true;
			tp->intStatus |= Int362xEndOfOp;
		}

		st = FcProcessed;
		break;

	case Fc362xSearchFwdFileMark:
		if (tp->unitReady)
		{
			mt362xResetStatus(tp);
			do
			{
				mt362xFuncForespace(mfrId);
			} while (!tp->fileMark && !tp->endOfTape && !tp->parityError);

			tp->endOfOperation = true;
			tp->intStatus |= Int362xEndOfOp;
		}

		st = FcProcessed;
		break;

	case Fc362xSearchBckFileMark:
		if (tp->unitReady)
		{
			mt362xResetStatus(tp);
			do
			{
				mt362xFuncBackspace(mfrId);
			} while (!tp->fileMark && tp->blockNo != 0 && !tp->parityError);

			if (tp->blockNo == 0)
			{
				mt362xUnload(tp, mfrId);
			}

			tp->endOfOperation = true;
			tp->intStatus |= Int362xEndOfOp;
		}

		st = FcProcessed;
		break;

	case Fc362xWriteFileMark:
		if (tp->unitReady && tp->ringIn)
		{
			mt362xResetStatus(tp);
			tp->blockNo += 1;

			/*
			**  The following fseek makes fwrite behave as desired after an fread.
			*/
			fseek(mfr->active3000Device->fcb[unitNo], 0, SEEK_CUR);

			/*
			**  Write a TAP tape mark.
			*/
			u32 recLen1 = 0;
			fwrite(&recLen1, sizeof(recLen1), 1, mfr->active3000Device->fcb[unitNo]);
			tp->fileMark = true;

			/*
			**  The following fseek prepares for any subsequent fread.
			*/
			fseek(mfr->active3000Device->fcb[unitNo], 0, SEEK_CUR);

			tp->endOfOperation = true;
			tp->intStatus |= Int362xEndOfOp;
		}

		st = FcProcessed;
		break;

	case Fc362xSkipBadSpot:
		if (tp->unitReady && tp->ringIn)
		{
			mt362xResetStatus(tp);
			tp->endOfOperation = true;
			tp->intStatus |= Int362xEndOfOp;
		}

		st = FcProcessed;
		break;

	case Fc362xSelectIntReady:
		tp->intMask |= Int362xReady;
		tp->intStatus &= ~Int362xReady;
		st = FcProcessed;
		break;

	case Fc362xReleaseIntReady:
		tp->intMask &= ~Int362xReady;
		tp->intStatus &= ~Int362xReady;
		st = FcProcessed;
		break;

	case Fc362xSelectIntEndOfOp:
		tp->intMask |= Int362xEndOfOp;
		tp->intStatus &= ~Int362xEndOfOp;
		st = FcProcessed;
		break;

	case Fc362xReleaseIntEndOfOp:
		tp->intMask &= ~Int362xEndOfOp;
		tp->intStatus &= ~Int362xEndOfOp;
		st = FcProcessed;
		break;

	case Fc362xSelectIntError:
		tp->intMask |= Int362xError;
		tp->intStatus &= ~Int362xError;
		st = FcProcessed;
		break;

	case Fc362xReleaseIntError:
		tp->intMask &= ~Int362xError;
		tp->intStatus &= ~Int362xError;
		st = FcProcessed;
		break;

	case Fc362xClearReverseRead:
		tp->reverseRead = false;
		st = FcProcessed;
		break;

	case Fc362xSetReverseRead:
		tp->reverseRead = true;
		st = FcProcessed;
		break;

	case Fc6681DevStatusReq:
		tp->busy = true;
		st = FcAccepted;
		break;

	case Fc6681InputToEor:
	case Fc6681Input:
		if (tp->unitReady && (tp->intStatus & Int362xError) == 0)
		{
			mt362xResetStatus(tp);
			if (tp->reverseRead)
			{
				mt362xFuncReadBkw(mfrId);
			}
			else
			{
				mt362xFuncRead(mfrId);
			}

			tp->busy = true;
			st = FcAccepted;
		}
		else
			/*  Tape unit was already busy when read was requested */
			st = FcDeclined;
		break;

	case Fc6681Output:
		if (tp->unitReady && tp->ringIn)
		{
			mt362xResetStatus(tp);
			tp->bp = tp->ioBuffer;
			mfr->active3000Device->recordLength = 0;
			tp->writing = true;
			tp->blockNo += 1;
			tp->busy = true;
			st = FcAccepted;
		}
		else
			st = FcDeclined;
		break;

	case Fc6681MasterClear:
		mfr->active3000Device->selectedUnit = -1;
		tp->bcdMode = false;
		tp->intMask = 0;
		tp->intStatus = 0;
		for (unitNo = 0; unitNo < 16; unitNo++)
		{
			tp = static_cast<TapeParam *>(mfr->active3000Device->context[unitNo]);
			if (tp != nullptr)
			{
				mt362xResetStatus(tp);
			}
		}

		st = FcProcessed;
		break;

	default:
		st = FcDeclined;
		break;
	}

	/*
	**  Remember function code for subsequent I/O.
	*/
	if (st == FcAccepted)
	{
		mfr->active3000Device->fcode = funcCode;
	}

	/*
	**  Signal interrupts.
	*/
	mt362xSetupStatus(tp, mfrId);
	// setup any pending interrupts based on status
	dcc6681Interrupt((tp->intMask & tp->intStatus) != 0, mfrId);

	return(st);
}

/*--------------------------------------------------------------------------
**  Purpose:        Perform I/O on the 362x Tape Controller.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void mt362xIo(u8 mfrId)
{
	TapeParam *tp;

	MMainFrame *mfr = BigIron->chasis[mfrId];

	/*
	**  The following avoids too rapid changes of the full/empty status
	**  when probed via FJM and EJM PP opcodes. This allows a second PP
	**  to monitor the progress of a transfer.
	*/
	if (mfr->activeChannel->delayStatus != 0)
	{
		return;
	}

	mfr->activeChannel->delayStatus = 0;

	/*
	**  Setup selected unit context.
	*/
	i8 unitNo = mfr->active3000Device->selectedUnit;
	if (unitNo != -1 && unitNo < MaxUnits2)
	{
		tp = static_cast<TapeParam *>(mfr->active3000Device->context[unitNo]);
	}
	else
	{
		tp = nullptr;
	}

	if (tp == nullptr)
	{
		return;
	}

	switch (mfr->active3000Device->fcode)
	{
	case Fc6681DevStatusReq:
		if (!mfr->activeChannel->full)
		{
			tp->status &= St362xClearBusy;
			mfr->activeChannel->data = tp->status;
			mfr->activeChannel->full = true;
			tp->endOfOperation = true;
			tp->intStatus |= Int362xEndOfOp;
#if DEBUG
			fprintf(mt362xLog, " %04o", activeChannel->data);
#endif
		}
		break;

	case Fc6681Input:
	case Fc6681InputToEor:
		if (mfr->activeChannel->full)
		{
			break;
		}

		if (tp->recordLength == 0)
		{
			mfr->activeChannel->active = false;
			tp->busy = false;
			tp->intStatus |= Int362xEndOfOp;
			break;
		}

		if (tp->recordLength > 0)
		{
			if (tp->reverseRead)
			{
				mfr->activeChannel->data = *tp->bp--;
			}
			else
			{
				mfr->activeChannel->data = *tp->bp++;
			}

#if DEBUG
			mt362xLogByte(activeChannel->data);
#endif

			mfr->activeChannel->full = true;
			tp->recordLength -= 1;
			if (tp->recordLength == 0)
			{
				/*
				**  Last word deactivates function.
				*/
				mfr->activeDevice->fcode = 0;
				mfr->activeChannel->discAfterInput = true;
				tp->busy = false;
				tp->intStatus |= Int362xEndOfOp;
			}
		}

		break;

	case Fc6681Output:
		if (mfr->activeChannel->full && mfr->active3000Device->recordLength < MaxPpBuf)
		{
			*tp->bp++ = mfr->activeChannel->data;
			mfr->activeChannel->full = false;
			mfr->active3000Device->recordLength += 1;
#if DEBUG
			mt362xLogByte(activeChannel->data);
#endif
		}

		break;

	default:	// No Action
		break;
	}

	/*
	**  Signal interrupts.
	*/
	mt362xSetupStatus(tp, mfrId);
	// setup any pending interrupts based on status
	dcc6681Interrupt((tp->intMask & tp->intStatus) != 0, mfrId);
}

/*--------------------------------------------------------------------------
**  Purpose:        Handle channel activation.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void mt362xActivate(u8 mfrId)
{
	MMainFrame *mfr = BigIron->chasis[mfrId];

	mfr->activeChannel->delayStatus = 5;
}

/*--------------------------------------------------------------------------
**  Purpose:        Handle disconnecting of channel.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void mt362xDisconnect(u8 mfrId)
{
	TapeParam *tp;
	u32 i;
	u64 recLen1;

	MMainFrame *mfr = BigIron->chasis[mfrId];

	i8 unitNo = mfr->active3000Device->selectedUnit;
	if (unitNo != -1 && unitNo < MaxUnits2)
	{
		tp = static_cast<TapeParam *>(mfr->active3000Device->context[unitNo]);
	}
	else
	{
		tp = nullptr;
	}

	/*
	**  Abort pending device disconnects - the PP is doing the disconnect.
	*/
	mfr->activeChannel->delayDisconnect = 0;
	mfr->activeChannel->discAfterInput = false;

	/*
	**  Nothing more to do unless we are writing.
	*/
	if (tp != nullptr && tp->writing)
	{
		/*
		**  Flush written TAP record to disk.
		*/
		unitNo = mfr->active3000Device->selectedUnit;
		tp = static_cast<TapeParam *>(mfr->active3000Device->context[unitNo]);

		if (unitNo == -1 || !tp->unitReady)
		{
			return;
		}

		FILE *fcb = mfr->active3000Device->fcb[unitNo];
		tp->bp = tp->ioBuffer;
		// ReSharper disable once CppInitializedValueIsAlwaysRewritten
		u64 recLen0 = 0;
		u64 recLen2 = mfr->active3000Device->recordLength;
		PpWord *ip = tp->ioBuffer;
		u8 *rp = rawBuffer;

		if (tp->tracks == 9)
		{
			/* 9 track */
			if (tp->bcdMode)
			{
				/*
				**  Make BCD readable as ASCII.
				*/
				for (i = 0; i < recLen2; i++)
				{
					*rp++ = bcdToAscii[(*ip >> 6) & Mask6];
					*rp++ = bcdToAscii[(*ip >> 0) & Mask6];
					ip += 1;
				}

				recLen0 = rp - rawBuffer;
			}
			else
			{
				/*
				**  No conversion, just unpack.
				*/
				for (i = 0; i < recLen2; i += 2)
				{
					*rp++ = ((ip[0] >> 4) & 0xFF);
					*rp++ = ((ip[0] << 4) & 0xF0) | ((ip[1] >> 8) & 0x0F);
					*rp++ = ((ip[1] >> 0) & 0xFF);
					ip += 2;
				}

				/*
				**  Calculate the actual length.
				*/
				recLen1 = (recLen2 * 12);
				recLen0 = recLen1 / 8;
				if ((recLen1 % 8) != 0)
				{
					recLen0 += 1;
				}
			}
		}
		else
		{
			/* 7 track */
			if (tp->bcdMode)
			{
				/*
				**  Make BCD readable as ASCII.
				*/
				for (i = 0; i < recLen2; i++)
				{
					*rp++ = bcdToAscii[(*ip >> 6) & Mask6];
					*rp++ = bcdToAscii[(*ip >> 0) & Mask6];
					ip += 1;
				}

			}
			else
			{
				/*
				**  No conversion, just unpack.
				*/
				for (i = 0; i < recLen2; i++)
				{
					*rp++ = ((*ip >> 6) & Mask6);
					*rp++ = ((*ip >> 0) & Mask6);
					ip += 1;
				}
			}

			recLen0 = rp - rawBuffer;
		}

		/*
		** The TAP record length is little endian - convert if necessary.
		*/
		if (BigIron->bigEndian)
		{
			recLen1 = MSystem::ConvertEndian(static_cast<u32>(recLen0));
		}
		else
		{
			recLen1 = recLen0;
		}

		/*
		**  The following fseek makes fwrite behave as desired after an fread.
		*/
		fseek(fcb, 0, SEEK_CUR);

		/*
		**  Write the TAP record.
		*/
		fwrite(&recLen1, sizeof(recLen1), 1, fcb);
		fwrite(&rawBuffer, 1, recLen0, fcb);
		fwrite(&recLen1, sizeof(recLen1), 1, fcb);

		/*
		**  The following fseek prepares for any subsequent fread.
		*/
		fseek(fcb, 0, SEEK_CUR);

		/*
		**  Writing completed.
		*/
		tp->writing = false;
	}

	tp->busy = false;
	tp->endOfOperation = true;
	tp->intStatus |= Int362xEndOfOp;

	mt362xSetupStatus(tp, mfrId);
	// setup any pending interrupts based on status
	dcc6681Interrupt((tp->intMask & tp->intStatus) != 0, mfrId);
}

/*--------------------------------------------------------------------------
**  Purpose:        Process read function.
**
**  Parameters:     Name        Description.
**                  tp          pointer to tape parameters
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void mt362xFuncRead(u8 mfrId)
{
	u32 recLen0;
	u32 recLen1;
	u32 recLen2;

	MMainFrame *mfr = BigIron->chasis[mfrId];

	i8 unitNo = mfr->active3000Device->selectedUnit;
	TapeParam *tp = static_cast<TapeParam *>(mfr->active3000Device->context[unitNo]);

	mfr->active3000Device->recordLength = 0;
	tp->recordLength = 0;

	/*
	**  Determine if the tape is at the load point.
	*/
	// ReSharper disable once CppUseAuto
	// ReSharper disable once CppEntityNeverUsed
	i32 position = ftell(mfr->active3000Device->fcb[unitNo]);

	/*
	**  Read and verify TAP record length header.
	*/
	u32 len = static_cast<u32>(fread(&recLen0, sizeof(recLen0), 1, mfr->active3000Device->fcb[unitNo]));

	if (len != 1)
	{
		tp->intStatus |= Int362xEndOfOp;
		tp->endOfOperation = true;
		tp->fileMark = true;

#if DEBUG
		fprintf(mt362xLog, "TAP is at EOF (simulate tape mark)\n");
#endif

		return;
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

	/*
	**  Check if record length is reasonable.
	*/
	if (recLen1 > MaxByteBuf)
	{
		logError(LogErrorLocation, "channel %02o - tape record too long: %d", mfr->activeChannel->id, recLen1);
		tp->intStatus |= Int362xError | Int362xEndOfOp;
		tp->parityError = true;
		tp->endOfOperation = true;
		return;
	}

	if (recLen1 == 0)
	{
		/*
		**  Report a tape mark and return.
		*/
		tp->intStatus |= Int362xEndOfOp;
		tp->fileMark = true;
		tp->endOfOperation = true;
		tp->blockNo += 1;

#if DEBUG
		fprintf(mt362xLog, "Tape mark\n");
#endif
		return;
	}

	/*
	**  Read and verify the actual raw data.
	*/
	len = static_cast<u32>(fread(rawBuffer, 1, recLen1, mfr->active3000Device->fcb[unitNo]));

	if (recLen1 != static_cast<u32>(len))
	{
		logError(LogErrorLocation, "channel %02o - short tape record read: %d", mfr->activeChannel->id, len);
		tp->intStatus |= Int362xError | Int362xEndOfOp;
		tp->parityError = true;
		tp->endOfOperation = true;
		return;
	}

	/*
	**  Read and verify the TAP record length trailer.
	*/
	len = static_cast<u32>(fread(&recLen2, sizeof(recLen2), 1, mfr->active3000Device->fcb[unitNo]));

	if (len != 1)
	{
		logError(LogErrorLocation, "channel %02o - missing tape record trailer", mfr->activeChannel->id);
		tp->intStatus |= Int362xError | Int362xEndOfOp;
		tp->parityError = true;
		tp->endOfOperation = true;
		return;
	}

	if (recLen0 != recLen2)
	{
		/*
		**  This is some weird shit to deal with "padded" TAP records. My brain refuses to understand
		**  why anyone would have the precise length of a record and then make the reader guess what
		**  the real length is.
		*/
		if (BigIron->bigEndian)
		{
			/*
			**  The TAP record length is little endian - convert if necessary.
			*/
			recLen2 = MSystem::ConvertEndian(recLen2);
		}

		if (recLen1 == ((recLen2 >> 8) & 0xFFFFFF))
		{
			fseek(mfr->active3000Device->fcb[unitNo], 1, SEEK_CUR);
		}
		else
		{
			logError(LogErrorLocation, "channel %02o - invalid tape record trailer: %d", mfr->activeChannel->id, recLen2);
			tp->intStatus |= Int362xError | Int362xEndOfOp;
			tp->parityError = true;
			tp->endOfOperation = true;
			return;
		}
	}

	/*
	**  Convert the raw data into PP words suitable for a channel.
	*/
	mt362xPackAndConvert(recLen1, mfrId);

	/*
	**  Setup length, buffer pointer and block number.
	*/
#if DEBUG
	fprintf(mt362xLog, "Read fwd %d PP words (%d 8-bit bytes)\n", active3000Device->recordLength, recLen1);
#endif

	tp->recordLength = mfr->active3000Device->recordLength;
	tp->bp = tp->ioBuffer;
	tp->blockNo += 1;
}

/*--------------------------------------------------------------------------
**  Purpose:        Process read backward function.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void mt362xFuncReadBkw(u8 mfrId)
{
	u32 recLen0;
	u32 recLen1;
	u32 recLen2;

	MMainFrame *mfr = BigIron->chasis[mfrId];

	i8 unitNo = mfr->active3000Device->selectedUnit;
	TapeParam *tp = static_cast<TapeParam *>(mfr->active3000Device->context[unitNo]);

	mfr->active3000Device->recordLength = 0;
	tp->recordLength = 0;

	/*
	**  Check if we are already at the beginning of the tape.
	*/
	i32 position = ftell(mfr->active3000Device->fcb[unitNo]);
	if (position == 0)
	{
		tp->blockNo = 0;
		tp->intStatus |= Int362xEndOfOp;
		tp->endOfOperation = true;
		return;
	}

	/*
	**  Position to the previous record's trailer and read the length
	**  of the record (leaving the file position ahead of the just read
	**  record trailer).
	*/
	fseek(mfr->active3000Device->fcb[unitNo], -4, SEEK_CUR);
	u32 len = static_cast<u32>(fread(&recLen0, sizeof(recLen0), 1, mfr->active3000Device->fcb[unitNo]));
	fseek(mfr->active3000Device->fcb[unitNo], -4, SEEK_CUR);

	if (len != 1)
	{
		logError(LogErrorLocation, "channel %02o - missing tape record trailer", mfr->activeChannel->id);
		tp->intStatus |= Int362xError | Int362xEndOfOp;
		tp->parityError = true;
		tp->endOfOperation = true;
		return;
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

	/*
	**  Check if record length is reasonable.
	*/
	if (recLen1 > MaxByteBuf)
	{
		logError(LogErrorLocation, "channel %02o - tape record too long: %d", mfr->activeChannel->id, recLen1);
		tp->intStatus |= Int362xError | Int362xEndOfOp;
		tp->parityError = true;
		tp->endOfOperation = true;
		return;
	}

	position -= 4;
	if (recLen1 != 0)
	{
		/*
		**  Skip backward over the TAP record body and header.
		*/
		position -= 4 + recLen1;
		fseek(mfr->active3000Device->fcb[unitNo], position, SEEK_SET);

		/*
		**  Read and verify the TAP record header.
		*/
		len = static_cast<u32>(fread(&recLen2, sizeof(recLen2), 1, mfr->active3000Device->fcb[unitNo]));

		if (len != 1)
		{
			logError(LogErrorLocation, "channel %02o - missing TAP record header", mfr->activeChannel->id);
			tp->intStatus |= Int362xError | Int362xEndOfOp;
			tp->parityError = true;
			tp->endOfOperation = true;
			return;
		}

		if (recLen0 != recLen2)
		{
			/*
			**  This is more weird shit to deal with "padded" TAP records.
			*/
			position -= 1;
			fseek(mfr->active3000Device->fcb[unitNo], position, SEEK_SET);
			len = static_cast<u32>(fread(&recLen2, sizeof(recLen2), 1, mfr->active3000Device->fcb[unitNo]));

			if (len != 1 || recLen0 != recLen2)
			{
				logError(LogErrorLocation, "channel %02o - invalid record length2: %d %08X != %08X", mfr->activeChannel->id, len, recLen0, recLen2);
				tp->intStatus |= Int362xError | Int362xEndOfOp;
				tp->parityError = true;
				tp->endOfOperation = true;
				return;
			}
		}

		/*
		**  Read and verify the actual raw data.
		*/
		len = static_cast<u32>(fread(rawBuffer, 1, recLen1, mfr->active3000Device->fcb[unitNo]));

		if (recLen1 != static_cast<u32>(len))
		{
			logError(LogErrorLocation, "channel %02o - short tape record read: %d", mfr->activeChannel->id, len);
			tp->intStatus |= Int362xError | Int362xEndOfOp;
			tp->parityError = true;
			tp->endOfOperation = true;
			return;
		}

		/*
		**  Position to the TAP record header.
		*/
		fseek(mfr->active3000Device->fcb[unitNo], position, SEEK_SET);

		/*
		**  Convert the raw data into PP words suitable for a channel.
		*/
		mt362xPackAndConvert(recLen1, mfrId);

		/*
		**  Setup length and buffer pointer.
		*/
#if DEBUG
		fprintf(mt362xLog, "Read bkwd %d PP words (%d 8-bit bytes)\n", active3000Device->recordLength, recLen1);
#endif

		tp->recordLength = mfr->active3000Device->recordLength;
		tp->bp = tp->ioBuffer + tp->recordLength - 1;
	}
	else
	{
		/*
		**  A tape mark consists of only a single TAP record header of zero.
		*/
		tp->intStatus |= Int362xEndOfOp;
		tp->fileMark = true;
		tp->endOfOperation = true;

#if DEBUG
		fprintf(mt362xLog, "Tape mark\n");
#endif
	}

	/*
	**  Set block number.
	*/
	if (position == 0)
	{
		tp->blockNo = 0;
	}
	else
	{
		tp->blockNo -= 1;
	}
}

/*--------------------------------------------------------------------------
**  Purpose:        Process forespace function.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void mt362xFuncForespace(u8 mfrId)
{
	u32 recLen0;
	u32 recLen1;
	u32 recLen2;

	MMainFrame *mfr = BigIron->chasis[mfrId];

	i8 unitNo = mfr->active3000Device->selectedUnit;
	TapeParam *tp = static_cast<TapeParam *>(mfr->active3000Device->context[unitNo]);

	/*
	**  Determine if the tape is at the load point.
	*/
	// ReSharper disable once CppEntityNeverUsed
	i32 position = ftell(mfr->active3000Device->fcb[unitNo]);

	/*
	**  Read and verify TAP record length header.
	*/
	u32 len = static_cast<u32>(fread(&recLen0, sizeof(recLen0), 1, mfr->active3000Device->fcb[unitNo]));

	if (len != 1)
	{
		tp->intStatus |= Int362xEndOfOp;
		tp->endOfOperation = true;
		tp->fileMark = true;
#if DEBUG
		fprintf(mt362xLog, "TAP is at EOF (simulate tape mark)\n");
#endif
		return;
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

	/*
	**  Check if record length is reasonable.
	*/
	if (recLen1 > MaxByteBuf)
	{
		logError(LogErrorLocation, "channel %02o - tape record too long: %d", mfr->activeChannel->id, recLen1);
		tp->parityError = true;
		tp->endOfOperation = true;
		return;
	}

	if (recLen1 == 0)
	{
		/*
		**  Report a tape mark and return.
		*/
		tp->fileMark = true;
		tp->blockNo += 1;
		tp->intStatus |= Int362xEndOfOp;
		tp->endOfOperation = true;

#if DEBUG
		fprintf(mt362xLog, "Tape mark\n");
#endif
		return;
	}

	/*
	**  Skip the actual raw data.
	*/
	if (fseek(mfr->active3000Device->fcb[unitNo], recLen1, SEEK_CUR) != 0)
	{
		logError(LogErrorLocation, "channel %02o - short tape record read: %d", mfr->activeChannel->id, len);
		tp->intStatus |= Int362xError | Int362xEndOfOp;
		tp->parityError = true;
		tp->endOfOperation = true;
		return;
	}

	/*
	**  Read and verify the TAP record length trailer.
	*/
	len = static_cast<u32>(fread(&recLen2, sizeof(recLen2), 1, mfr->active3000Device->fcb[unitNo]));

	if (len != 1)
	{
		logError(LogErrorLocation, "channel %02o - missing tape record trailer", mfr->activeChannel->id);
		tp->intStatus |= Int362xError | Int362xEndOfOp;
		tp->parityError = true;
		tp->endOfOperation = true;
		return;
	}

	if (recLen0 != recLen2)
	{
		/*
		**  This is some weird shit to deal with "padded" TAP records. My brain refuses to understand
		**  why anyone would have the precise length of a record and then make the reader guess what
		**  the real length is.
		*/
		if (BigIron->bigEndian)
		{
			/*
			**  The TAP record length is little endian - convert if necessary.
			*/
			recLen2 = MSystem::ConvertEndian(recLen2);
		}

		if (recLen1 == ((recLen2 >> 8) & 0xFFFFFF))
		{
			fseek(mfr->active3000Device->fcb[unitNo], 1, SEEK_CUR);
		}
		else
		{
			logError(LogErrorLocation, "channel %02o - invalid tape record trailer: %d", mfr->activeChannel->id, recLen2);
			tp->intStatus |= Int362xError | Int362xEndOfOp;
			tp->parityError = true;
			tp->endOfOperation = true;
			return;
		}
	}

	tp->blockNo += 1;
}

/*--------------------------------------------------------------------------
**  Purpose:        Process backspace function.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void mt362xFuncBackspace(u8 mfrId)
{
	u32 recLen0;
	u32 recLen1;
	u32 recLen2;

	MMainFrame *mfr = BigIron->chasis[mfrId];

	i8 unitNo = mfr->active3000Device->selectedUnit;
	TapeParam *tp = static_cast<TapeParam *>(mfr->active3000Device->context[unitNo]);

	/*
	**  Check if we are already at the beginning of the tape.
	*/
	i32 position = ftell(mfr->active3000Device->fcb[unitNo]);
	if (position == 0)
	{
		tp->intStatus |= Int362xEndOfOp;
		tp->endOfOperation = true;
		tp->blockNo = 0;
		return;
	}

	/*
	**  Position to the previous record's trailer and read the length
	**  of the record (leaving the file position ahead of the just read
	**  record trailer).
	*/
	fseek(mfr->active3000Device->fcb[unitNo], -4, SEEK_CUR);
	u32 len = static_cast<u32>(fread(&recLen0, sizeof(recLen0), 1, mfr->active3000Device->fcb[unitNo]));
	fseek(mfr->active3000Device->fcb[unitNo], -4, SEEK_CUR);

	if (len != 1)
	{
		logError(LogErrorLocation, "channel %02o - missing tape record trailer", mfr->activeChannel->id);
		tp->intStatus |= Int362xError | Int362xEndOfOp;
		tp->parityError = true;
		tp->endOfOperation = true;
		return;
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

	/*
	**  Check if record length is reasonable.
	*/
	if (recLen1 > MaxByteBuf)
	{
		logError(LogErrorLocation, "channel %02o - tape record too long: %d", mfr->activeChannel->id, recLen1);
		tp->intStatus |= Int362xError | Int362xEndOfOp;
		tp->parityError = true;
		tp->endOfOperation = true;
		return;
	}

	position -= 4;
	if (recLen1 != 0)
	{
		/*
		**  Skip backward over the TAP record body and header.
		*/
		position -= 4 + recLen1;
		fseek(mfr->active3000Device->fcb[unitNo], position, SEEK_SET);

		/*
		**  Read and verify the TAP record header.
		*/
		len = static_cast<u32>(fread(&recLen2, sizeof(recLen2), 1, mfr->active3000Device->fcb[unitNo]));

		if (len != 1)
		{
			logError(LogErrorLocation, "channel %02o - missing TAP record header", mfr->activeChannel->id);
			tp->intStatus |= Int362xError | Int362xEndOfOp;
			tp->parityError = true;
			tp->endOfOperation = true;
			return;
		}

		if (recLen0 != recLen2)
		{
			/*
			**  This is more weird shit to deal with "padded" TAP records.
			*/
			position -= 1;
			fseek(mfr->active3000Device->fcb[unitNo], position, SEEK_SET);
			len = static_cast<u32>(fread(&recLen2, sizeof(recLen2), 1, mfr->active3000Device->fcb[unitNo]));

			if (len != 1 || recLen0 != recLen2)
			{
				logError(LogErrorLocation, "channel %02o - invalid record length2: %d %08X != %08X", mfr->activeChannel->id, len, recLen0, recLen2);
				tp->intStatus |= Int362xError | Int362xEndOfOp;
				tp->parityError = true;
				tp->endOfOperation = true;
				return;
			}
		}

		/*
		**  Position to the TAP record header.
		*/
		fseek(mfr->active3000Device->fcb[unitNo], position, SEEK_SET);
	}
	else
	{
		/*
		**  A tape mark consists of only a single TAP record header of zero.
		*/
		tp->fileMark = true;
		tp->intStatus |= Int362xEndOfOp;
		tp->endOfOperation = true;

#if DEBUG
		fprintf(mt362xLog, "Tape mark\n");
#endif
	}

	/*
	**  Set block number.
	*/
	if (position == 0)
	{
		tp->blockNo = 0;
	}
	else
	{
		tp->blockNo -= 1;
	}
}

/*--------------------------------------------------------------------------
**  Purpose:        Pack and convert 8 bit frames read into channel data.
**
**  Parameters:     Name        Description.
**                  recLen      read tape record length
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void mt362xPackAndConvert(u32 recLen, u8 mfrId)
{
	MMainFrame *mfr = BigIron->chasis[mfrId];
	i8 unitNo = mfr->active3000Device->selectedUnit;
	TapeParam *tp = static_cast<TapeParam*>(mfr->active3000Device->context[unitNo]);
	u32 i;

	/*
	**  Convert the raw data into PP words suitable for a channel.
	*/
	u16 *op = tp->ioBuffer;
	u8 *rp = rawBuffer;

	if (tp->bcdMode)
	{
		/*
		**  Pad.
		*/
		rawBuffer[recLen] = 0;

		for (i = 0; i < recLen; i += 2)
		{
			*op++ = (static_cast<PpWord>(asciiToBcd[rp[0]]) << 6) | (static_cast<PpWord>(asciiToBcd[rp[1]]) << 0);
			rp += 2;
		}

		mfr->active3000Device->recordLength = static_cast<PpWord>(op - tp->ioBuffer);
	}
	else
	{
		if (tp->tracks == 9)
		{
			/*
			**  Pad.
			*/
			rawBuffer[recLen] = 0;

			/*
			**  Convert the raw data into PP Word data.
			*/
			for (i = 0; i < recLen; i += 3)
			{
				u16 c1 = *rp++;
				u16 c2 = *rp++;
				u16 c3 = *rp++;

				*op++ = ((c1 << 4) | (c2 >> 4)) & Mask12;
				*op++ = ((c2 << 8) | (c3 >> 0)) & Mask12;
			}

			/*
			**  Now calculate the number of PP words.
			*/
			recLen *= 8;
			mfr->active3000Device->recordLength = static_cast<PpWord>(recLen / 12);
			if (recLen % 12 != 0)
			{
				mfr->active3000Device->recordLength += 1;
			}
		}
		else
		{
			/*
			**  Pad.
			*/
			rawBuffer[recLen] = 0;

			for (i = 0; i < recLen; i += 2)
			{
				*op++ = (static_cast<PpWord>(rp[0] & Mask6) << 6) | (static_cast<PpWord>(rp[1] & Mask6) << 0);
				rp += 2;
			}

			mfr->active3000Device->recordLength = static_cast<PpWord>(op - tp->ioBuffer);
		}
	}
}

/*--------------------------------------------------------------------------
**  Purpose:        Unload tape unit.
**
**  Parameters:     Name        Description.
**                  tp          pointer to tape parameters
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void mt362xUnload(TapeParam *tp, u8 mfrId)
{
	MMainFrame *mfr = BigIron->chasis[mfrId];

	mt362xResetStatus(tp);
	tp->blockNo = 0;
	tp->unitReady = false;
	tp->ringIn = false;
	tp->endOfOperation = true;
	i8 unitNo = mfr->active3000Device->selectedUnit;
	fclose(mfr->active3000Device->fcb[unitNo]);
	mfr->active3000Device->fcb[unitNo] = nullptr;
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
static char *mt362xFunc2String(PpWord funcCode)
{
	static char buf[30];
#if DEBUG
	switch (funcCode)
	{
	case Fc362xRelease: return "Fc362xRelease";
	case Fc362xSelectBinary: return "Fc362xSelectBinary";
	case Fc362xSelectCoded: return "Fc362xSelectCoded";
	case Fc362xSelect556Bpi: return "Fc362xSelect556Bpi";
	case Fc362xSelect200Bpi: return "Fc362xSelect200Bpi";
	case Fc362xClear: return "Fc362xClear";
	case Fc362xSelect800Bpi: return "Fc362xSelect800Bpi";
	case Fc362xRewind: return "Fc362xRewind";
	case Fc362xRewindUnload: return "Fc362xRewindUnload";
	case Fc362xBackspace: return "Fc362xBackspace";
	case Fc362xSearchFwdFileMark: return "Fc362xSearchFwdFileMark";
	case Fc362xSearchBckFileMark: return "Fc362xSearchBckFileMark";
	case Fc362xWriteFileMark: return "Fc362xWriteFileMark";
	case Fc362xSkipBadSpot: return "Fc362xSkipBadSpot";
	case Fc362xSelectIntReady: return "Fc362xSelectIntReady";
	case Fc362xReleaseIntReady: return "Fc362xReleaseIntReady";
	case Fc362xSelectIntEndOfOp: return "Fc362xSelectIntEndOfOp";
	case Fc362xReleaseIntEndOfOp: return "Fc362xReleaseIntEndOfOp";
	case Fc362xSelectIntError: return "Fc362xSelectIntError";
	case Fc362xReleaseIntError: return "Fc362xReleaseIntError";
	case Fc362xClearReverseRead: return "Fc362xClearReverseRead";
	case Fc362xSetReverseRead: return "Fc362xSetReverseRead";
	case Fc6681DevStatusReq: return "Fc6681DevStatusReq";
	case Fc6681MasterClear: return "Fc6681MasterClear";
	case Fc6681InputToEor: return "Fc6681InputToEor";
	case Fc6681Input: return "Fc6681Input";
	case Fc6681Output: return "Fc6681Output";
	}
#endif
	sprintf(buf, "UNKNOWN: %04o", funcCode);
	return(buf);
}

/*---------------------------  End Of File  ------------------------------*/
