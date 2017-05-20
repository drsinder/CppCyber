/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2011, Tom Hunter
**  C++ adaptation by Dale Sinder 2017
**
**  Name: mt669.cpp
**
**  Description:
**      Perform emulation of CDC 6600 669 tape drives attached to a
**      7021-21 magnetic tape controller.
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
#ifndef _WIN32
#include <unistd.h>
#endif


/*
**  -----------------
**  Private Constants
**  -----------------
*/

/*
**  MTS tape function codes:
**  ========================
*/

/*
**  Setup functions.
*/
#define Fc669FormatUnit             00030
#define Fc669LoadConversion1        00131
#define Fc669LoadConversion2        00231
#define Fc669LoadConversion3        00331

/*
**  Unit reserve functions.
*/
#define Fc669Connect                00020
#define Fc669Release                00001
#define Fc669ClearReserve           00002
#define Fc669ClearOppositeReserve   00003

/*
**  Unit manipulation functions.
*/
#define Fc669Rewind                 00010
#define Fc669RewindUnload           00110
#define Fc669SearchTapeMarkF        00015
#define Fc669SearchTapeMarkB        00115
#define Fc669CtrlForespaceFindGap   00214
#define Fc669CtrlBackspaceFindGap   00314
#define Fc669Forespace              00013
#define Fc669Backspace              00113
#define Fc669WriteTapeMark          00051
#define Fc669EraseToEOT             00152
#define Fc669CtrledForespace        00014
#define Fc669CtrledBackspace        00114
#define Fc669StopMotion             00011

/*
**  Read functions.
*/
#define Fc669ReadFwd                00040
#define Fc669ReadBkw                00140

/*
**  Write functions.
*/
#define Fc669Write                  00050
#define Fc669WriteOdd12             00150
#define Fc669WriteOdd               00250

/*
**  Status functions.
*/
#define Fc669GeneralStatus          00012
#define Fc669DetailedStatus         00112
#define Fc669CumulativeStatus       00212
#define Fc669UnitReadyStatus        00312

/*
**  Non-motion read recovery functions.
*/
#define Fc669SetReadClipNorm        00006
#define Fc669SetReadClipHigh        00106
#define Fc669SetReadClipLow         00206
#define Fc669SetReadClipHyper       00306
#define Fc669ReadSprktDlyNorm       00007
#define Fc669ReadSprktDlyIncr       00107
#define Fc669ReadSprktDlyDecr       00207
#define Fc669OppParity              00005
#define Fc669OppDensity             00105

/*
**  Read error recovery functions.
*/
#define Fc669LongForespace          00213
#define Fc669LongBackspace          00313
#define Fc669RereadFwd              00041
#define Fc669RereadBkw              00141
#define Fc669ReadBkwOddLenParity    00340
#define Fc669RereadBkwOddLenParity  00341
#define Fc669RepeatRead             00042

/*
**  Write error recovery functions.
*/
#define Fc669Erase                  00052
#define Fc669WriteRepos             00017
#define Fc669WriteEraseRepos        00117
#define Fc669WriteReposiCtrl        00217
#define Fc669WriteEraseReposCtrl    00317
#define Fc669EraseRepos             00016
#define Fc669EraseEraseRepos        00116

/*
**  Diagnostic functions.
*/
#define Fc669LoadReadRam            00132
#define Fc669LoadWriteRam           00232
#define Fc669LoadReadWriteRam       00332
#define Fc669CopyReadRam            00133
#define Fc669CopyWriteRam           00233
#define Fc669FormatTcuUnitStatus    00034
#define Fc669CopyTcuStatus          00035
#define Fc669SendTcuCmd             00036
#define Fc669SetQuartReadSprktDly   00037

/*
**  Undocumented functions.
*/
#define Fc669ConnectRewindRead      00260
#define Fc669MasterClear            00414
#define Fc669ClearUnit              00000


/*
**  General status reply:
**  =====================
*/
#define St669Alert                  04000
#define St669NoUnit                 01000
#define St669WriteEnabled           00200
#define St669NineTrack              00100
#define St669OddCount               00040
#define St669TapeMark               00020
#define St669EOT                    00010
#define St669BOT                    00004
#define St669Busy                   00002
#define St669Ready                  00001

/*
**  Detailed status error codes:
**  ============================
*/
#define EcIllegalUnit               001
#define EcUnitNotReady              004
#define EcMissingRing               006       
#define EcBlankTape                 010       
#define EcStopMotion                011     // alert bit not set
#define EcBackPastLoadpoint         030
#define EcIllegalFunction           050       
#define EcNoFuncParams              052
#define EcMiscUnitError             047

/*
**  Misc constants.
*/
#define MaxPpBuf                    40000
#define MaxByteBuf                  60000
#define MaxPackedConvBuf            (((256 * 8) + 11) / 12)
#define MaxTapeSize                 1250000000   // this may need adjusting for shorter real tapes


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
**  MTS controller.
*/
typedef struct ctrlParam
{
	FILE        *convFileHandle;
	u8          readConv[3][256];
	u8          writeConv[3][256];
	PpWord      deviceStatus[9];   // first element not used
	PpWord      excludedUnits;
	bool        writing;
} CtrlParam;

/*
**  MTS tape unit.
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
	u8          selectedConversion;
	bool        packedMode;
	u8          assemblyMode;
	u8          density;
	u8          minBlockLength;

	/*
	**  Tape status variables.
	*/
	bool        alert;
	bool        endOfTape;
	bool        fileMark;
	bool        unitReady;
	bool        ringIn;
	bool        oddCount;
	bool        flagBitDetected;
	bool        rewinding;
	bool        suppressBot;
	u32         rewindStart;
	u16         blockCrc;
	u8          errorCode;
	u32         blockNo;

	/*
	**  I/O buffer.
	*/
	PpWord      frameCount;
	PpWord      recordLength;
	PpWord      ioBuffer[MaxPpBuf];
	PpWord      *bp;
} TapeParam;

/*
**  ---------------------------
**  Private Function Prototypes
**  ---------------------------
*/
static void mt669ResetStatus(TapeParam *tp);
static void mt669SetupGeneralStatus(TapeParam *tp, u8 mfId);
static void mt669SetupDetailedStatus(TapeParam *tp, u8 mfId);
static void mt669SetupCumulativeStatus(TapeParam *tp, u8 mfId);
static void mt669SetupUnitReadyStatus(u8 mfId);
static FcStatus mt669Func(PpWord funcCode, u8 mfrId);
static void mt669Io(u8 mfrId);
static void mt669Activate(u8 mfrId);
static void mt669Disconnect(u8 mfrId);
static void mt669PackAndConvert(u32 recLen, u8 mfId);
static void mt669FuncRead(u8 mfId);
static void mt669FuncForespace(u8 mfId);
static void mt669FuncBackspace(u8 mfId);
static void mt669FuncReadBkw(u8 mfId);
static char *mt669Func2String(PpWord funcCode);

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
static FILE *mt669Log = nullptr;
#endif

/*
**--------------------------------------------------------------------------
**
**  Public Functions
**
**--------------------------------------------------------------------------
*/
/*--------------------------------------------------------------------------
**  Purpose:        Initialise 669 tape drives.
**
**  Parameters:     Name        Description.
**                  eqNo        equipment number
**                  unitNo      number of the unit to initialise
**                  channelNo   channel number the device is attached to
**                  deviceName  optional device file name
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void mt669Init(u8 mfrID, u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName)
{
	(void)eqNo;

#if DEBUG
	if (mt669Log == nullptr)
	{
		mt669Log = fopen("mt669log.txt", "wt");
	}
#endif

	/*
	**  Attach device to channel.
	*/
	DevSlot *dp = channelAttach(channelNo, eqNo, DtMt669, mfrID);

	/*
	**  Setup channel functions.
	*/
	dp->activate = mt669Activate;
	dp->disconnect = mt669Disconnect;
	dp->func = mt669Func;
	dp->io = mt669Io;
	dp->selectedUnit = -1;

	/*
	**  Setup controller context.
	*/
	if (dp->controllerContext == nullptr)
	{
		dp->controllerContext = calloc(1, sizeof(CtrlParam));

		/*
		**  Optionally read in persistent conversion tables.
		*/
		if (*persistDir != '\0')
		{
			CtrlParam *cp = static_cast<CtrlParam*>(dp->controllerContext);
			char fileName[256];

			/*
			**  Try to open existing backing file.
			*/
			sprintf(fileName, "%s/mt669StoreC%02oE%02o", persistDir, channelNo, eqNo);
			cp->convFileHandle = fopen(fileName, "r+b");
			if (cp->convFileHandle != nullptr)
			{
				/*
				**  Read conversion table contents.
				*/
				if (fread(cp->writeConv, 1, sizeof(cp->writeConv), cp->convFileHandle) != sizeof(cp->writeConv)
					|| fread(cp->readConv, 1, sizeof(cp->readConv), cp->convFileHandle) != sizeof(cp->readConv))
				{
					printf("Unexpected length of MT669 backing file, clearing tables\n");
					memset(cp->writeConv, 0, sizeof(cp->writeConv));
					memset(cp->readConv, 0, sizeof(cp->readConv));
				}
			}
			else
			{
				/*
				**  Create a new file.
				*/
				cp->convFileHandle = fopen(fileName, "w+b");
				if (cp->convFileHandle == nullptr)
				{
					fprintf(stderr, "Failed to create MT669 backing file\n");
					exit(1);
				}
			}
		}
	}

	/*
	**  Setup tape unit parameter block.
	*/
	TapeParam *tp = static_cast<TapeParam*>(calloc(1, sizeof(TapeParam)));
	if (tp == nullptr)
	{
		fprintf(stderr, "Failed to allocate MT669 context block\n");
		exit(1);
	}

	dp->context[unitNo] = tp;

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
	}
	else
	{
		dp->fcb[unitNo] = nullptr;
		tp->unitReady = false;
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
	printf("MT669 initialised on channel %o equipment %o unit %o\n", channelNo, eqNo, unitNo);
}

/*--------------------------------------------------------------------------
**  Purpose:        Optionally persist conversion tables.
**
**  Parameters:     Name        Description.
**                  dp          Device pointer.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void mt669Terminate(DevSlot *dp)
{
	CtrlParam *cp = static_cast<CtrlParam*>(dp->controllerContext);

	/*
	**  Optionally save conversion tables.
	*/
	if (cp->convFileHandle != nullptr)
	{
		fseek(cp->convFileHandle, 0, SEEK_SET);
		if (fwrite(cp->writeConv, 1, sizeof(cp->writeConv), cp->convFileHandle) != sizeof(cp->writeConv)
			|| fwrite(cp->readConv, 1, sizeof(cp->readConv), cp->convFileHandle) != sizeof(cp->readConv))
		{
			fprintf(stderr, "Error writing MT669 backing file\n");
		}

		fclose(cp->convFileHandle);
		cp->convFileHandle = nullptr;
	}
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
void mt669LoadTape(char *params)
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
	if (numParam != 5)
	{
		printf("Not enough or invalid parameters\n");
		return;
	}

	if (channelNo < 0 || channelNo >= MaxChannels)
	{
		printf("Invalid channel no\n");
		return;
	}

	if (unitNo < 0 || unitNo >= MaxUnits)
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
	DevSlot *dp = channelFindDevice(static_cast<u8>(channelNo), DtMt669, mfrID);
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
	mt669ResetStatus(tp);
	tp->ringIn = unitMode == 'w';
	tp->blockNo = 0;
	tp->unitReady = true;

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
void mt669UnloadTape(char *params)
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
	if (numParam != 3)
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
	DevSlot *dp = channelFindDevice(static_cast<u8>(channelNo), DtMt669, mfrID);
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
	mt669ResetStatus(tp);
	tp->unitReady = false;
	tp->ringIn = false;
	tp->rewinding = false;
	tp->rewindStart = 0;
	tp->blockCrc = 0;
	tp->blockNo = 0;

	printf("Successfully unloaded MT669 on channel %o equipment %o unit %o\n", channelNo, equipmentNo, unitNo);
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
void mt669ShowTapeStatus()
{
	TapeParam *tp = firstTape;

	while (tp)
	{
		printf("MT669 on %o,%o,%o", tp->channelNo, tp->eqNo, tp->unitNo);
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

/*
**--------------------------------------------------------------------------
**
**  Private Functions
**
**--------------------------------------------------------------------------
*/
/*--------------------------------------------------------------------------
**  Purpose:        Reset device status at start of new function.
**
**  Parameters:     Name        Description.
**                  tp          pointer to tape parameters
**
**  Returns:        Nothing
**
**------------------------------------------------------------------------*/
static void mt669ResetStatus(TapeParam *tp)
{
	if (tp != nullptr)
	{
		tp->alert = false;
		tp->endOfTape = false;
		tp->fileMark = false;
		tp->oddCount = false;
		tp->flagBitDetected = false;
		tp->suppressBot = false;
		tp->errorCode = 0;
	}
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
static void mt669SetupGeneralStatus(TapeParam *tp, u8 mfrId)
{
	MMainFrame *mfr = BigIron->chasis[mfrId];
	CtrlParam *cp = static_cast<CtrlParam*>(mfr->activeDevice->controllerContext);

	if (tp == nullptr)
	{
		cp->deviceStatus[1] = St669NineTrack;
		cp->deviceStatus[2] = 0;
		return;
	}

	cp->deviceStatus[1] = St669NineTrack;

	if (tp->alert)
	{
		cp->deviceStatus[1] |= St669Alert;
	}

	if (tp->ringIn)
	{
		cp->deviceStatus[1] |= St669WriteEnabled;
	}

	if (tp->oddCount)
	{
		cp->deviceStatus[1] |= St669OddCount;
	}

	if (tp->fileMark)
	{
		cp->deviceStatus[1] |= St669TapeMark;
	}

	if (tp->endOfTape)
	{
		cp->deviceStatus[1] |= St669EOT;
	}

	if (tp->rewinding)
	{
		cp->deviceStatus[1] |= St669Busy;
		if (labs(mfr->activeDevice->mfr->cycles - tp->rewindStart) > 1000)
		{
			tp->rewinding = false;
			tp->blockNo = 0;
		}
	}
	else
	{
		if (tp->blockNo == 0 && !tp->suppressBot)
		{
			cp->deviceStatus[1] |= St669BOT;
		}

		if (tp->unitReady)
		{
			cp->deviceStatus[1] |= St669Ready;
			if (ftell(mfr->activeDevice->fcb[mfr->activeDevice->selectedUnit]) > MaxTapeSize)
			{
				cp->deviceStatus[1] |= St669EOT;
			}
		}
	}

	cp->deviceStatus[2] = (tp->blockCrc & Mask9) << 3;
}

/*--------------------------------------------------------------------------
**  Purpose:        Setup detailed status based on current tape parameters.
**
**  Parameters:     Name        Description.
**                  tp          pointer to tape parameters
**
**  Returns:        Nothing
**
**------------------------------------------------------------------------*/
static void mt669SetupDetailedStatus(TapeParam *tp, u8 mfrId)
{
	MMainFrame *mfr = BigIron->chasis[mfrId];

	CtrlParam *cp = static_cast<CtrlParam*>(mfr->activeDevice->controllerContext);

	if (tp == nullptr)
	{
		cp->deviceStatus[1] = 0;
		cp->deviceStatus[2] = 0;
		cp->deviceStatus[3] = 0;
		cp->deviceStatus[4] = 0;
		cp->deviceStatus[5] = 0;
		cp->deviceStatus[6] = 0;
		cp->deviceStatus[7] = 0;
		cp->deviceStatus[8] = 0;
		return;
	}

	cp->deviceStatus[1] = tp->errorCode;
	cp->deviceStatus[2] = 0;
	cp->deviceStatus[3] = 0;

	if (tp->flagBitDetected)
	{
		cp->deviceStatus[3] |= 1 << 5;
	}

	if (tp->oddCount)
	{
		cp->deviceStatus[3] |= 1 << 10;
	}

	cp->deviceStatus[4] = 0;

	/*
	**  Report: forward tape motion, speed=100 ips, density=1600 cpi
	**  and configured unit number.
	*/
	cp->deviceStatus[5] = 00600 + mfr->activeDevice->selectedUnit;

	cp->deviceStatus[6] = 0;

	/*
	**  24 bit last read frame count or zero if last operation was a
	**  successful write.
	*/
	cp->deviceStatus[7] = (tp->frameCount >> 12) & Mask12;
	cp->deviceStatus[8] = (tp->frameCount >> 0) & Mask12;
}

/*--------------------------------------------------------------------------
**  Purpose:        Setup cumulative status based on current tape parameters.
**
**  Parameters:     Name        Description.
**                  tp          pointer to tape parameters
**
**  Returns:        Nothing
**
**------------------------------------------------------------------------*/
static void mt669SetupCumulativeStatus(TapeParam *tp, u8  mfrId)
{
	MMainFrame *mfr = BigIron->chasis[mfrId];

	CtrlParam *cp = static_cast<CtrlParam*>(mfr->activeDevice->controllerContext);

	if (tp == nullptr)
	{
		cp->deviceStatus[1] = 0;
		cp->deviceStatus[2] = 0;
		cp->deviceStatus[3] = 0;
		cp->deviceStatus[4] = 0;
		cp->deviceStatus[5] = 0;
		cp->deviceStatus[6] = 0;
		cp->deviceStatus[7] = 0;
		cp->deviceStatus[8] = 0;
		return;
	}

	/*
	**  Report: forward tape motion, speed=100 ips, density=1600 cpi
	**  and configured unit number.
	*/
	cp->deviceStatus[1] = 00600 + mfr->activeDevice->selectedUnit;
	cp->deviceStatus[2] = mfr->activeDevice->selectedUnit << 8;
	cp->deviceStatus[3] = 0;
	cp->deviceStatus[4] = 0;
	cp->deviceStatus[5] = 0;
	cp->deviceStatus[6] = 0;
	cp->deviceStatus[7] = 0;
	cp->deviceStatus[8] = 0;
}

/*--------------------------------------------------------------------------
**  Purpose:        Setup all tape unit's ready status.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing
**
**------------------------------------------------------------------------*/
static void mt669SetupUnitReadyStatus(u8 mfrId)
{
	MMainFrame *mfr = BigIron->chasis[mfrId];

	CtrlParam *cp = static_cast<CtrlParam*>(mfr->activeDevice->controllerContext);
	PpWord s = 0;

	for (u8 unitNo = 0; unitNo < 8; unitNo++)
	{
		TapeParam *tp = static_cast<TapeParam *>(mfr->activeDevice->context[unitNo]);
		if (tp != nullptr && tp->unitReady)
		{
			if (tp->rewinding)
			{
				/*
				**  Unit is not ready while rewinding.
				*/
				if (labs(mfr->activeDevice->mfr->cycles - tp->rewindStart) > 1000)
				{
					tp->rewinding = false;
					tp->blockNo = 0;
				}
			}
			else
			{
				s |= 1 << unitNo;
			}
		}
	}

	cp->deviceStatus[1] = 0;
	cp->deviceStatus[2] = s & cp->excludedUnits;
}

/*--------------------------------------------------------------------------
**  Purpose:        Execute function code on 669 tape drives.
**
**  Parameters:     Name        Description.
**                  funcCode    function code
**
**  Returns:        FcStatus
**
**------------------------------------------------------------------------*/
static FcStatus mt669Func(PpWord funcCode, u8 mfrId)
{
	MMainFrame *mfr = BigIron->chasis[mfrId];

	TapeParam *tp;
	CtrlParam *cp = static_cast<CtrlParam*>(mfr->activeDevice->controllerContext);

	i8 unitNo = mfr->activeDevice->selectedUnit;
	if (unitNo != -1)
	{
		tp = static_cast<TapeParam *>(mfr->activeDevice->context[unitNo]);
	}
	else
	{
		tp = nullptr;
	}

#if DEBUG
	fprintf(mt669Log, "\n%06d PP:%02o CH:%02o u:%d f:%04o T:%-25s  >   ",
		traceSequenceNo,
		activePpu->id,
		activeDevice->channel->id,
		unitNo,
		funcCode,
		mt669Func2String(funcCode));
#endif

	/*
	**  Reset function code.
	*/
	mfr->activeDevice->fcode = 0;
	mfr->activeChannel->full = false;

	/*
	**  Controller is hard-wired to equipment number 0 requiring top three bits to be zero.
	*/
	if (((funcCode >> 9) & Mask3) != 0)
	{
		/*
		**  Not for us.
		*/
		return(FcDeclined);
	}

	/*
	**  Process tape function.
	*/
	switch (funcCode)
	{
	default:
#if DEBUG
		fprintf(mt669Log, " FUNC not implemented & declined!");
#endif
		if (unitNo != -1)
		{
			tp->errorCode = EcIllegalFunction;
			tp->alert = true;
		}

		return(FcDeclined);

		/*
		**  Setup functions.
		*/
	case Fc669FormatUnit:
		mfr->activeDevice->fcode = funcCode;
		mfr->activeDevice->recordLength = 2;
		mt669ResetStatus(tp);
		break;

	case Fc669LoadConversion1:
	case Fc669LoadConversion2:
	case Fc669LoadConversion3:
		mfr->activeDevice->fcode = funcCode;
		mfr->activeDevice->recordLength = 0;
		break;

		/*
		**  Unit reserve functions.
		*/
	case Fc669Connect + 0:
	case Fc669Connect + 1:
	case Fc669Connect + 2:
	case Fc669Connect + 3:
	case Fc669Connect + 4:
	case Fc669Connect + 5:
	case Fc669Connect + 6:
	case Fc669Connect + 7:
		unitNo = funcCode & Mask3;
		tp = static_cast<TapeParam *>(mfr->activeDevice->context[unitNo]);
		if (tp == nullptr)
		{
			mfr->activeDevice->selectedUnit = -1;
			logError(LogErrorLocation, "channel %02o - invalid select: %04o", mfr->activeChannel->id, static_cast<u32>(funcCode));
			return(FcDeclined);
		}

		mfr->activeDevice->selectedUnit = unitNo;
		return(FcProcessed);

	case Fc669Release:
	case Fc669ClearReserve:
	case Fc669ClearOppositeReserve:
		mfr->activeDevice->selectedUnit = -1;
		return(FcProcessed);

		/*
		**  Unit manipulation functions.
		*/
	case Fc669Rewind:
		if (unitNo != -1 && tp->unitReady)
		{
			mt669ResetStatus(tp);
			fseek(mfr->activeDevice->fcb[unitNo], 0, SEEK_SET);
			if (tp->blockNo != 0)
			{
				if (!tp->rewinding)
				{
					tp->rewinding = true;
					tp->rewindStart = mfr->activeDevice->mfr->cycles;
				}
			}
		}

		return(FcProcessed);

	case Fc669RewindUnload:
		if (unitNo != -1 && tp->unitReady)
		{
			mt669ResetStatus(tp);
			tp->blockNo = 0;
			tp->unitReady = false;
			tp->ringIn = false;
			fclose(mfr->activeDevice->fcb[unitNo]);
			mfr->activeDevice->fcb[unitNo] = nullptr;
		}

		return(FcProcessed);

	case Fc669SearchTapeMarkF:
		if (unitNo != -1 && tp->unitReady)
		{
			mt669ResetStatus(tp);

			do
			{
				mt669FuncForespace(mfrId);
			} while (!tp->fileMark && !tp->endOfTape && !tp->alert);
		}
		return(FcProcessed);

	case Fc669SearchTapeMarkB:
		if (unitNo != -1 && tp->unitReady)
		{
			mt669ResetStatus(tp);

			do
			{
				mt669FuncBackspace(mfrId);
			} while (!tp->fileMark && tp->blockNo != 0 && !tp->alert);
		}

		if (tp->blockNo == 0)
		{
			/*
			**  A "catastrophic" error has occured - we reached load point.
			**  (see manual pages 2-7 and A-2)
			**  <<<<<<<<<<<<<<<<<<< this probably should move into mt679FuncBackspace >>>>>>>>>>>>>>>>>>
			**  <<<<<<<<<<<<<<<<<<< we also need to do this in mt679FuncForespace     >>>>>>>>>>>>>>>>>>
			*/
			tp->alert = true;
			tp->errorCode = EcBackPastLoadpoint;
		}

		tp->fileMark = false;

		return(FcProcessed);

	case Fc669CtrlForespaceFindGap:
	case Fc669CtrlBackspaceFindGap:
		logError(LogErrorLocation, "channel %02o - unsupported function: %04o", mfr->activeChannel->id, static_cast<u32>(funcCode));
		return(FcProcessed);

	case Fc669Forespace:
		if (unitNo != -1 && tp->unitReady)
		{
			mt669ResetStatus(tp);
			mt669FuncForespace(mfrId);
		}

		return(FcProcessed);

	case Fc669Backspace:
		if (unitNo != -1 && tp->unitReady)
		{
			mt669ResetStatus(tp);
			mt669FuncBackspace(mfrId);
		}

		return(FcProcessed);

	case Fc669WriteTapeMark:
		if (unitNo != -1 && tp->unitReady && tp->ringIn)
		{
			mt669ResetStatus(tp);
			tp->bp = tp->ioBuffer;
			// ReSharper disable once CppEntityNeverUsed
			i32 position = ftell(mfr->activeDevice->fcb[unitNo]);
			tp->blockNo += 1;

			/*
			**  The following fseek makes fwrite behave as desired after an fread.
			*/
			fseek(mfr->activeDevice->fcb[unitNo], 0, SEEK_CUR);

			/*
			**  Write a TAP tape mark.
			*/
			u32 recLen1 = 0;
			fwrite(&recLen1, sizeof(recLen1), 1, mfr->activeDevice->fcb[unitNo]);
			tp->fileMark = true;

			/*
			**  The following fseek prepares for any subsequent fread.
			*/
			fseek(mfr->activeDevice->fcb[unitNo], 0, SEEK_CUR);
		}

		return(FcProcessed);

	case Fc669EraseToEOT:
		if (unitNo != -1 && tp->unitReady && tp->ringIn)
		{
			// ? would be nice to truncate somehow
			logError(LogErrorLocation, "channel %02o - unsupported function: %04o", mfr->activeChannel->id, static_cast<u32>(funcCode));
		}

		return(FcProcessed);

	case Fc669CtrledForespace:
	case Fc669CtrledBackspace:
		logError(LogErrorLocation, "channel %02o - unsupported function: %04o", mfr->activeChannel->id, static_cast<u32>(funcCode));
		return(FcProcessed);

	case Fc669StopMotion:
		mt669ResetStatus(tp);
		return(FcProcessed);

		/*
		**  Read functions.
		*/
	case Fc669ReadFwd:
		if (unitNo != -1 && tp->unitReady)
		{
			mfr->activeDevice->fcode = funcCode;
			mt669ResetStatus(tp);
			mt669FuncRead(mfrId);
			break;
		}

		return(FcProcessed);

	case Fc669ReadBkw:
		if (unitNo != -1 && tp->unitReady)
		{
			mfr->activeDevice->fcode = funcCode;
			mt669ResetStatus(tp);
			mt669FuncReadBkw(mfrId);
			break;
		}

		return(FcProcessed);

		/*
		**  Write functions.
		*/
	case Fc669WriteOdd12:
		funcCode = Fc669WriteOdd;
		/* fall through */
	case Fc669Write:
	case Fc669WriteOdd:
		if (unitNo != -1 && tp->unitReady && tp->ringIn)
		{
			mfr->activeDevice->fcode = funcCode;
			mt669ResetStatus(tp);
			tp->bp = tp->ioBuffer;
			mfr->activeDevice->recordLength = 0;
			cp->writing = true;
			tp->blockNo += 1;
			break;
		}

		return(FcProcessed);

		/*
		**  Status functions.
		*/
	case Fc669GeneralStatus:
		mfr->activeDevice->fcode = funcCode;
		mfr->activeDevice->recordLength = 2;
		mt669SetupGeneralStatus(tp, mfrId);
		break;

	case Fc669DetailedStatus:
		mfr->activeDevice->fcode = funcCode;
		mfr->activeDevice->recordLength = 8;
		mt669SetupDetailedStatus(tp, mfrId);
		break;

	case Fc669CumulativeStatus:
		mfr->activeDevice->fcode = funcCode;
		mfr->activeDevice->recordLength = 8;
		mt669SetupCumulativeStatus(tp, mfrId);
		break;

	case Fc669UnitReadyStatus:
		mfr->activeDevice->fcode = funcCode;
		mfr->activeDevice->recordLength = 2;
		mt669SetupUnitReadyStatus(mfrId);
		break;

		/*
		**  Non-motion read recovery functions.
		*/
	case Fc669SetReadClipNorm:
	case Fc669SetReadClipHigh:
	case Fc669SetReadClipLow:
	case Fc669SetReadClipHyper:
	case Fc669ReadSprktDlyNorm:
	case Fc669ReadSprktDlyIncr:
	case Fc669ReadSprktDlyDecr:
	case Fc669OppParity:
	case Fc669OppDensity:
		mt669ResetStatus(tp);
		logError(LogErrorLocation, "channel %02o - unsupported function: %04o", mfr->activeChannel->id, static_cast<u32>(funcCode));
		return(FcProcessed);

		/*
		**  Read error recovery functions.
		*/
	case Fc669LongForespace:
	case Fc669LongBackspace:
	case Fc669RereadFwd:
	case Fc669RereadBkw:
	case Fc669ReadBkwOddLenParity:
	case Fc669RereadBkwOddLenParity:
	case Fc669RepeatRead:
		mt669ResetStatus(tp);
		logError(LogErrorLocation, "channel %02o - unsupported function: %04o", mfr->activeChannel->id, static_cast<u32>(funcCode));
		return(FcProcessed);

		/*
		**  Write error recovery functions.
		*/
	case Fc669Erase:
	case Fc669WriteRepos:
	case Fc669WriteEraseRepos:
	case Fc669WriteReposiCtrl:
	case Fc669WriteEraseReposCtrl:
	case Fc669EraseRepos:
	case Fc669EraseEraseRepos:
		mt669ResetStatus(tp);
		logError(LogErrorLocation, "channel %02o - unsupported function: %04o", mfr->activeChannel->id, static_cast<u32>(funcCode));
		return(FcProcessed);

		/*
		**  Diagnostic functions.
		*/
	case Fc669LoadReadRam:
	case Fc669LoadWriteRam:
	case Fc669LoadReadWriteRam:
	case Fc669CopyReadRam:
	case Fc669CopyWriteRam:
		mt669ResetStatus(tp);
		logError(LogErrorLocation, "channel %02o - unsupported function: %04o", mfr->activeChannel->id, static_cast<u32>(funcCode));
		return(FcProcessed);

	case Fc669FormatTcuUnitStatus:
		mfr->activeDevice->fcode = funcCode;
		mfr->activeDevice->recordLength = 1;
		break;

	case Fc669CopyTcuStatus:
	case Fc669SendTcuCmd:
	case Fc669SetQuartReadSprktDly:
		mt669ResetStatus(tp);
		logError(LogErrorLocation, "channel %02o - unsupported function: %04o", mfr->activeChannel->id, static_cast<u32>(funcCode));
		return(FcProcessed);

		/*
		**  Undocumented functions.
		*/
	case Fc669ConnectRewindRead + 0:
	case Fc669ConnectRewindRead + 1:
	case Fc669ConnectRewindRead + 2:
	case Fc669ConnectRewindRead + 3:
	case Fc669ConnectRewindRead + 4:
	case Fc669ConnectRewindRead + 5:
	case Fc669ConnectRewindRead + 6:
	case Fc669ConnectRewindRead + 7:
		unitNo = funcCode & Mask3;
		tp = static_cast<TapeParam *>(mfr->activeDevice->context[unitNo]);
		if (tp == nullptr || !tp->unitReady)
		{
			mfr->activeDevice->selectedUnit = -1;
			logError(LogErrorLocation, "channel %02o - invalid select: %04o", mfr->activeChannel->id, static_cast<u32>(funcCode));
			return(FcDeclined);
		}

		mt669ResetStatus(tp);
		mfr->activeDevice->selectedUnit = unitNo;
		fseek(mfr->activeDevice->fcb[unitNo], 0, SEEK_SET);
		tp->selectedConversion = 0;
		tp->packedMode = true;
		tp->blockNo = 0;
		mfr->activeDevice->fcode = Fc669ReadFwd;
		mt669ResetStatus(tp);
		mt669FuncRead(mfrId);
		break;

	case Fc669MasterClear:
		mfr->activeDevice->fcode = funcCode;
		mfr->activeDevice->selectedUnit = -1;
		mt669ResetStatus(nullptr);
		break;

	case Fc669ClearUnit:
		if (unitNo != -1)
		{
			mfr->activeDevice->recordLength = 0;
			tp->recordLength = 0;
			tp->errorCode = 0;
			mt669ResetStatus(tp);
		}
		return(FcProcessed);
	}

	return(FcAccepted);
}

/*--------------------------------------------------------------------------
**  Purpose:        Perform I/O on MT669.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void mt669Io(u8 mfrId)
{
	MMainFrame *mfr = BigIron->chasis[mfrId];

	CtrlParam *cp = static_cast<CtrlParam*>(mfr->activeDevice->controllerContext);
	TapeParam *tp;
	int wordNumber;
	PpWord param;

	/*
	**  The following avoids too rapid changes of the full/empty status
	**  when probed via FJM and EJM PP opcodes. This allows a second PP
	**  to monitor the progress of a transfer (used by 1MT and 1LT to
	**  coordinate the transfer of a large tape record).
	*/
	if (mfr->activeChannel->delayStatus != 0)
	{
		return;
	}

	mfr->activeChannel->delayStatus = 3;

	/*
	**  Setup selected unit context.
	*/
	i8 unitNo = mfr->activeDevice->selectedUnit;
	if (unitNo != -1)
	{
		tp = static_cast<TapeParam *>(mfr->activeDevice->context[unitNo]);
	}
	else
	{
		tp = nullptr;
	}

	/*
	**  Perform actual tape I/O according to function issued.
	*/
	switch (mfr->activeDevice->fcode)
	{
	default:
		logError(LogErrorLocation, "channel %02o - unsupported function code: %04o",
			mfr->activeChannel->id, mfr->activeDevice->fcode);
		break;

	case 0:
		/*
		**  Previous function has terminated.
		*/
		break;

	case Fc669FormatUnit:
		if (mfr->activeDevice->recordLength > 0)
		{
			if (mfr->activeChannel->full)
			{
				wordNumber = 3 - mfr->activeDevice->recordLength;

#if DEBUG
				fprintf(mt669Log, " %04o", activeChannel->data);
#endif

				if (wordNumber == 1)
				{
					/*
					**  Process parameter word 1.
					*/
					param = mfr->activeChannel->data;

					if (((param >> 4) & 1) != 0)
					{
						unitNo = param & Mask4;
						mfr->activeDevice->selectedUnit = unitNo;
						tp = static_cast<TapeParam *>(mfr->activeDevice->context[unitNo]);
					}

					if (tp != nullptr && ((param >> 11) & 1) != 0)
					{
						tp->selectedConversion = (param >> 8) & Mask3;
						if (tp->selectedConversion > 3)
						{
							tp->selectedConversion = 0;
						}
					}

					if (tp != nullptr && ((param >> 7) & 1) != 0)
					{
						tp->assemblyMode = (param >> 5) & Mask2;
						tp->packedMode = tp->assemblyMode == 1;
					}
				}

				if (wordNumber == 2)
				{
					/*
					**  Process parameter word 2.
					*/
					param = mfr->activeChannel->data;

					if (tp != nullptr && ((param >> 8) & 1) != 0)
					{
						tp->density = (param >> 6) & Mask2;
					}

					if (tp != nullptr && ((param >> 5) & 1) != 0)
					{
						tp->minBlockLength = param & Mask5;
					}

					/*
					**  Last parameter word deactivates function.
					*/
					mfr->activeDevice->fcode = 0;
				}

				mfr->activeDevice->recordLength -= 1;
			}

			mfr->activeChannel->full = false;
		}

		break;

	case Fc669LoadConversion1:
		if (mfr->activeChannel->full)
		{
			mfr->activeChannel->full = false;
			cp->readConv[0][mfr->activeDevice->recordLength] = mfr->activeChannel->data & 077;
			if (mfr->activeChannel->data & 01000)
			{
				cp->writeConv[0][mfr->activeChannel->data & 077] = static_cast<u8>(mfr->activeDevice->recordLength);
			}

			mfr->activeDevice->recordLength += 1;
		}

		break;

	case Fc669LoadConversion2:
		if (mfr->activeChannel->full)
		{
			mfr->activeChannel->full = false;
			cp->readConv[1][mfr->activeDevice->recordLength] = mfr->activeChannel->data & 077;
			if (mfr->activeChannel->data & 01000)
			{
				cp->writeConv[1][mfr->activeChannel->data & 077] = static_cast<u8>(mfr->activeDevice->recordLength);
			}

			mfr->activeDevice->recordLength += 1;
		}

		break;

	case Fc669LoadConversion3:
		if (mfr->activeChannel->full)
		{
			mfr->activeChannel->full = false;
			cp->readConv[2][mfr->activeDevice->recordLength] = mfr->activeChannel->data & 077;
			cp->writeConv[2][mfr->activeChannel->data & 077] = static_cast<u8>(mfr->activeDevice->recordLength);
			mfr->activeDevice->recordLength += 1;
		}

		break;

	case Fc669ReadFwd:
		if (mfr->activeChannel->full)
		{
			break;
		}

		if (tp->recordLength == 0)
		{
			mfr->activeChannel->active = false;
		}

		if (tp->recordLength > 0)
		{
			mfr->activeChannel->data = *tp->bp++;
			mfr->activeChannel->full = true;
			tp->recordLength -= 1;
			if (tp->recordLength == 0)
			{
				/*
				**  Last word deactivates function.
				*/
				mfr->activeDevice->fcode = 0;
				mfr->activeChannel->discAfterInput = true;
			}
		}

		break;

	case Fc669ReadBkw:
		if (mfr->activeChannel->full)
		{
			break;
		}

		if (tp->recordLength == 0)
		{
			mfr->activeChannel->active = false;
		}

		if (tp->recordLength > 0)
		{
			mfr->activeChannel->data = *tp->bp--;
			mfr->activeChannel->full = true;
			tp->recordLength -= 1;
			if (tp->recordLength == 0)
			{
				/*
				**  Last word deactivates function.
				*/
				mfr->activeDevice->fcode = 0;
				mfr->activeChannel->discAfterInput = true;
			}
		}

		break;

	case Fc669Write:
	case Fc669WriteOdd:
		if (mfr->activeChannel->full && mfr->activeDevice->recordLength < MaxPpBuf)
		{
			mfr->activeChannel->full = false;
			mfr->activeDevice->recordLength += 1;
			*tp->bp++ = mfr->activeChannel->data;
		}

		break;

	case Fc669GeneralStatus:
		if (!mfr->activeChannel->full)
		{
			if (mfr->activeDevice->recordLength > 0)
			{
				wordNumber = 3 - mfr->activeDevice->recordLength;
				mfr->activeChannel->data = cp->deviceStatus[wordNumber];
				mfr->activeChannel->full = true;
				mfr->activeDevice->recordLength -= 1;
#if DEBUG
				fprintf(mt669Log, " %04o", activeChannel->data);
#endif
				if (mfr->activeDevice->recordLength == 0)
				{
					/*
					**  Last word deactivates function. In case this was triggered by EJM or FJM
					**  and the status is not picked up by an IAN we disconnect after too many cycles.
					*/
					mfr->activeDevice->fcode = 0;
					mfr->activeChannel->discAfterInput = true;
					mfr->activeChannel->delayDisconnect = 50;
				}
				else
				{
					/*
					**  Force a disconnect if the PP didn't read the status for too many cycles.
					**  This is needed for SMM/KRONOS which expect only one status word.
					*/
					mfr->activeChannel->delayDisconnect = 50;
				}
			}
		}

		break;

	case Fc669UnitReadyStatus:
		if (!mfr->activeChannel->full)
		{
			if (mfr->activeDevice->recordLength > 0)
			{
				wordNumber = 3 - mfr->activeDevice->recordLength;
				mfr->activeChannel->data = cp->deviceStatus[wordNumber];
				mfr->activeChannel->full = true;
				mfr->activeDevice->recordLength -= 1;
#if DEBUG
				fprintf(mt669Log, " %04o", activeChannel->data);
#endif
				if (mfr->activeDevice->recordLength == 0)
				{
					/*
					**  Last word deactivates function.
					*/
					mfr->activeDevice->fcode = 0;
					mfr->activeChannel->discAfterInput = true;
				}
			}
		}

		break;

	case Fc669DetailedStatus:
	case Fc669CumulativeStatus:
		if (!mfr->activeChannel->full)
		{
			if (mfr->activeDevice->recordLength > 0)
			{
				wordNumber = 9 - mfr->activeDevice->recordLength;
				mfr->activeChannel->data = cp->deviceStatus[wordNumber];
				mfr->activeDevice->recordLength -= 1;
				if (wordNumber == 8)
				{
					/*
					**  Last word deactivates function.
					*/
					mfr->activeDevice->fcode = 0;
					mfr->activeChannel->discAfterInput = true;
				}

				mfr->activeChannel->full = true;
#if DEBUG
				fprintf(mt669Log, " %04o", activeChannel->data);
#endif
			}
		}

		break;

	case Fc669FormatTcuUnitStatus:
		if (mfr->activeDevice->recordLength > 0)
		{
			if (mfr->activeChannel->full)
			{
#if DEBUG
				fprintf(mt669Log, " %04o", activeChannel->data);
#endif
				/*
				**  Ignore the possibility of the alternate meaning when bit 8
				**  is clear as it is never used.
				*/
				cp->excludedUnits = (~mfr->activeChannel->data) & Mask8;
				mfr->activeDevice->recordLength -= 1;
			}

			mfr->activeChannel->full = false;
		}

		break;

	case Fc669MasterClear:
		if (mfr->activeChannel->full)
		{
			mfr->activeChannel->full = false;
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
static void mt669Activate(u8 mfrId)
{
	BigIron->chasis[mfrId]->activeChannel->delayStatus = 5;
}

/*--------------------------------------------------------------------------
**  Purpose:        Handle disconnecting of channel.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void mt669Disconnect(u8 mfrId)
{
	MMainFrame *mfr = BigIron->chasis[mfrId];
	CtrlParam *cp = static_cast<CtrlParam*>(mfr->activeDevice->controllerContext);
	u32 i;
	u64 recLen1;

	/*
	**  Abort pending device disconnects - the PP is doing the disconnect.
	*/
	mfr->activeChannel->delayDisconnect = 0;
	mfr->activeChannel->discAfterInput = false;

	/*
	**  Nothing more to do unless we are writing.
	*/
	if (!cp->writing)
	{
		return;
	}

	/*
	**  Flush written TAP record to disk.
	*/
	i8 unitNo = mfr->activeDevice->selectedUnit;
	TapeParam *tp = static_cast<TapeParam *>(mfr->activeDevice->context[unitNo]);

	if (unitNo == -1 || !tp->unitReady)
	{
		return;
	}

	FILE *fcb = mfr->activeDevice->fcb[unitNo];
	tp->bp = tp->ioBuffer;
	u64 recLen0 = 0;
	u64 recLen2 = mfr->activeDevice->recordLength;
	PpWord *ip = tp->ioBuffer;
	u8 *rp = rawBuffer;
	bool oddFrameCount = mfr->activeDevice->fcode == Fc669WriteOdd;

	// ReSharper disable once CppDefaultCaseNotHandledInSwitchStatement
	switch (tp->selectedConversion)
	{
	case 0:
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
		**  Now implement the Mode 1 Write table on page B-6 of the
		**  7021-1/2 manual (60403900E).
		*/
		recLen0 = (recLen2 / 4) * 6;

		// ReSharper disable once CppDefaultCaseNotHandledInSwitchStatement
		switch (recLen2 % 4)
		{
		case 1:
			recLen0 += oddFrameCount ? 1 : 0;
			break;

		case 2:
			recLen0 += oddFrameCount ? 3 : 2;
			break;

		case 3:
			recLen0 += oddFrameCount ? 5 : 4;
			break;

		case 0:
			if (recLen0 > 0 && oddFrameCount)
			{
				recLen0 -= 1;
			}
			break;
		}

		break;

	case 1:
	case 2:
	case 3:
	case 4:
		/*
		**  Convert the channel data to appropriate character set.
		*/
		u8 *writeConv = cp->writeConv[tp->selectedConversion - 1];

		for (i = 0; i < recLen2; i++)
		{
			*rp++ = writeConv[(*ip >> 6) & 077];
			*rp++ = writeConv[(*ip >> 0) & 077];
			ip += 1;
		}

		recLen0 = rp - rawBuffer;
		if (oddFrameCount)
		{
			recLen0 -= 1;
		}
		break;
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
	cp->writing = false;

	/*
	**  Indicate successful write in detailed status.
	*/
	tp->frameCount = 0;
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
static void mt669PackAndConvert(u32 recLen, u8 mfrId)
{
	MMainFrame *mfr = BigIron->chasis[mfrId];

	i8 unitNo = mfr->activeDevice->selectedUnit;
	TapeParam *tp = static_cast<TapeParam*>(mfr->activeDevice->context[unitNo]);
	CtrlParam *cp = static_cast<CtrlParam*>(mfr->activeDevice->controllerContext);
	u32 i;
	u16 c1;

	/*
	**  Determine odd count setting.
	*/
	tp->oddCount = (recLen & 1) != 0;

	/*
	**  Convert the raw data into PP words suitable for a channel.
	*/
	u16 *op = tp->ioBuffer;
	u8 *rp = rawBuffer;

	switch (tp->selectedConversion)
	{
	default:
		break;

	case 0:
		/*
		**  Tape controller unit works in units of 16 bits, so we have to
		**  round up to multiples of 16 bits. See table on page B-5 of the
		**  7021-1/2 manual (60403900E). The fill byte is all 1's (see page
		**  B-2).
		*/
		if (tp->oddCount)
		{
			rawBuffer[recLen] = 0xFF;
			recLen += 1;
		}

		/*
		**  Convert the raw data into PP Word data.
		*/
		for (i = 0; i < recLen; i += 3)
		{
			c1 = *rp++;
			u16 c2 = *rp++;
			u16 c3 = *rp++;

			*op++ = ((c1 << 4) | (c2 >> 4)) & Mask12;
			*op++ = ((c2 << 8) | (c3 >> 0)) & Mask12;
		}

		/*
		**  Now calculate the number of PP words taking into account the
		**  16 bit TCU words. This seems strange at first, but the table
		**  referenced above illustrates it clearly.
		*/
		recLen *= 8;
		mfr->activeDevice->recordLength = static_cast<PpWord>(recLen / 12);
		if (recLen % 12 != 0)
		{
			mfr->activeDevice->recordLength += 1;
		}

		break;

	case 1:
	case 2:
	case 3:
		/*
		**  Convert the Raw data to appropriate character set.
		*/
		u8 *readConv = cp->readConv[tp->selectedConversion - 1];
		for (i = 0; i < recLen; i++)
		{
			c1 = readConv[*rp++];
			if ((c1 & (1 << 6)) != 0)
			{
				/*
				**  Indicate illegal character.
				*/
				tp->alert = true;
				tp->flagBitDetected = true;
			}

			if ((i & 1) == 0)
			{
				*op = (c1 & Mask6) << 6;
			}
			else
			{
				*op++ |= c1 & Mask6;
			}
		}

		mfr->activeDevice->recordLength = static_cast<PpWord>(op - tp->ioBuffer);

		if (tp->oddCount)
		{
			mfr->activeDevice->recordLength += 1;
		}
		break;
	}
}

/*--------------------------------------------------------------------------
**  Purpose:        Process read function.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void mt669FuncRead(u8 mfrId)
{
	u32 recLen0;
	u32 recLen1;
	u32 recLen2;

	MMainFrame *mfr = BigIron->chasis[mfrId];
	i8 unitNo = mfr->activeDevice->selectedUnit;
	TapeParam *tp = static_cast<TapeParam *>(mfr->activeDevice->context[unitNo]);

	mfr->activeDevice->recordLength = 0;
	tp->recordLength = 0;

	/*
	**  Determine if the tape is at the load point.
	*/
	i32 position = ftell(mfr->activeDevice->fcb[unitNo]);

	/*
	**  Read and verify TAP record length header.
	*/
	u32 len = static_cast<u32>(fread(&recLen0, sizeof(recLen0), 1, mfr->activeDevice->fcb[unitNo]));

	if (len != 1)
	{
		if (position == 0)
		{
			tp->errorCode = EcBlankTape;
		}
		else
		{
			//            tp->endOfTape = true;
			tp->fileMark = true;
#if DEBUG
			fprintf(mt669Log, "TAP is at EOF (simulate tape mark)\n");
#endif
		}

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
		tp->alert = true;
		tp->errorCode = EcMiscUnitError;
		return;
	}

	if (recLen1 == 0)
	{
		/*
		**  Report a tape mark and return.
		*/
		tp->fileMark = true;
		tp->blockNo += 1;

#if DEBUG
		fprintf(mt669Log, "Tape mark\n");
#endif
		return;
	}

	/*
	**  Read and verify the actual raw data.
	*/
	len = static_cast<u32>(fread(rawBuffer, 1, recLen1, mfr->activeDevice->fcb[unitNo]));

	if (recLen1 != static_cast<u32>(len))
	{
		logError(LogErrorLocation, "channel %02o - short tape record read: %d", mfr->activeChannel->id, len);
		tp->alert = true;
		tp->errorCode = EcMiscUnitError;
		return;
	}

	/*
	**  Read and verify the TAP record length trailer.
	*/
	len = static_cast<u32>(fread(&recLen2, sizeof(recLen2), 1, mfr->activeDevice->fcb[unitNo]));

	if (len != 1)
	{
		logError(LogErrorLocation, "channel %02o - missing tape record trailer", mfr->activeChannel->id);
		tp->alert = true;
		tp->errorCode = EcMiscUnitError;
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
			fseek(mfr->activeDevice->fcb[unitNo], 1, SEEK_CUR);
		}
		else
		{
			logError(LogErrorLocation, "channel %02o - invalid tape record trailer: %d", mfr->activeChannel->id, recLen2);
			tp->alert = true;
			tp->errorCode = EcMiscUnitError;
			return;
		}
	}

	/*
	**  Convert the raw data into PP words suitable for a channel.
	*/
	mt669PackAndConvert(recLen1, mfrId);

	/*
	**  Setup length, buffer pointer and block number.
	*/
#if DEBUG
	fprintf(mt669Log, "Read fwd %d PP words (%d 8-bit bytes)\n", activeDevice->recordLength, recLen1);
#endif

	tp->frameCount = static_cast<PpWord>(recLen1);
	tp->recordLength = mfr->activeDevice->recordLength;
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
static void mt669FuncReadBkw(u8 mfrId)
{
	u32 recLen0;
	u32 recLen1;
	u32 recLen2;
	MMainFrame *mfr = BigIron->chasis[mfrId];
	i8 unitNo = mfr->activeDevice->selectedUnit;
	TapeParam *tp = static_cast<TapeParam *>(mfr->activeDevice->context[unitNo]);

	mfr->activeDevice->recordLength = 0;
	tp->recordLength = 0;

	/*
	**  Check if we are already at the beginning of the tape.
	*/
	i32 position = ftell(mfr->activeDevice->fcb[unitNo]);
	if (position == 0)
	{
		tp->suppressBot = false;
		tp->blockNo = 0;
		return;
	}

	/*
	**  Position to the previous record's trailer and read the length
	**  of the record (leaving the file position ahead of the just read
	**  record trailer).
	*/
	fseek(mfr->activeDevice->fcb[unitNo], -4, SEEK_CUR);
	u32 len = static_cast<u32>(fread(&recLen0, sizeof(recLen0), 1, mfr->activeDevice->fcb[unitNo]));
	fseek(mfr->activeDevice->fcb[unitNo], -4, SEEK_CUR);

	if (len != 1)
	{
		logError(LogErrorLocation, "channel %02o - missing tape record trailer", mfr->activeChannel->id);
		tp->alert = true;
		tp->errorCode = EcMiscUnitError;
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
		tp->alert = true;
		tp->errorCode = EcMiscUnitError;
		return;
	}

	position -= 4;
	if (recLen1 != 0)
	{
		/*
		**  Skip backward over the TAP record body and header.
		*/
		position -= 4 + recLen1;
		fseek(mfr->activeDevice->fcb[unitNo], position, SEEK_SET);

		/*
		**  Read and verify the TAP record header.
		*/
		len = static_cast<u32>(fread(&recLen2, sizeof(recLen2), 1, mfr->activeDevice->fcb[unitNo]));

		if (len != 1)
		{
			logError(LogErrorLocation, "channel %02o - missing TAP record header", mfr->activeChannel->id);
			tp->alert = true;
			tp->errorCode = EcMiscUnitError;
			return;
		}

		if (recLen0 != recLen2)
		{
			/*
			**  This is more weird shit to deal with "padded" TAP records.
			*/
			position -= 1;
			fseek(mfr->activeDevice->fcb[unitNo], position, SEEK_SET);
			len = static_cast<u32>(fread(&recLen2, sizeof(recLen2), 1, mfr->activeDevice->fcb[unitNo]));

			if (len != 1 || recLen0 != recLen2)
			{
				logError(LogErrorLocation, "channel %02o - invalid record length2: %d %08X != %08X", mfr->activeChannel->id, len, recLen0, recLen2);
				tp->alert = true;
				tp->errorCode = EcMiscUnitError;
				return;
			}
		}

		/*
		**  Read and verify the actual raw data.
		*/
		len = static_cast<u32>(fread(rawBuffer, 1, recLen1, mfr->activeDevice->fcb[unitNo]));

		if (recLen1 != static_cast<u32>(len))
		{
			logError(LogErrorLocation, "channel %02o - short tape record read: %d", mfr->activeChannel->id, len);
			tp->alert = true;
			tp->errorCode = EcMiscUnitError;
			return;
		}

		/*
		**  Position to the TAP record header.
		*/
		fseek(mfr->activeDevice->fcb[unitNo], position, SEEK_SET);

		/*
		**  Convert the raw data into PP words suitable for a channel.
		*/
		mt669PackAndConvert(recLen1, mfrId);

		/*
		**  Setup length and buffer pointer.
		*/
#if DEBUG
		fprintf(mt669Log, "Read bkwd %d PP words (%d 8-bit bytes)\n", activeDevice->recordLength, recLen1);
#endif

		tp->frameCount = static_cast<PpWord>(recLen1);
		tp->recordLength = mfr->activeDevice->recordLength;
		tp->bp = tp->ioBuffer + tp->recordLength - 1;
	}
	else
	{
		/*
		**  A tape mark consists of only a single TAP record header of zero.
		*/
		tp->fileMark = true;

#if DEBUG
		fprintf(mt669Log, "Tape mark\n");
#endif
	}

	/*
	**  Set block number.
	*/
	if (position == 0)
	{
		tp->suppressBot = true;
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
static void mt669FuncForespace(u8 mfrId)
{
	u32 recLen0;
	u32 recLen1;
	u32 recLen2;
	MMainFrame *mfr = BigIron->chasis[mfrId];

	i8 unitNo = mfr->activeDevice->selectedUnit;
	TapeParam *tp = static_cast<TapeParam *>(mfr->activeDevice->context[unitNo]);

	/*
	**  Determine if the tape is at the load point.
	*/
	i32 position = ftell(mfr->activeDevice->fcb[unitNo]);

	/*
	**  Read and verify TAP record length header.
	*/
	u32 len = static_cast<u32>(fread(&recLen0, sizeof(recLen0), 1, mfr->activeDevice->fcb[unitNo]));

	if (len != 1)
	{
		if (position == 0)
		{
			tp->errorCode = EcBlankTape;
		}
		else
		{
			//            tp->endOfTape = true;
			tp->fileMark = true;
#if DEBUG
			fprintf(mt669Log, "TAP is at EOF (simulate tape mark)\n");
#endif
		}

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
		tp->alert = true;
		tp->errorCode = EcMiscUnitError;
		return;
	}

	if (recLen1 == 0)
	{
		/*
		**  Report a tape mark and return.
		*/
		tp->fileMark = true;
		tp->blockNo += 1;

#if DEBUG
		fprintf(mt669Log, "Tape mark\n");
#endif
		return;
	}

	/*
	**  Skip the actual raw data.
	*/
	if (fseek(mfr->activeDevice->fcb[unitNo], recLen1, SEEK_CUR) != 0)
	{
		logError(LogErrorLocation, "channel %02o - short tape record read: %d", mfr->activeChannel->id, len);
		tp->alert = true;
		tp->errorCode = EcMiscUnitError;
		return;
	}

	/*
	**  Read and verify the TAP record length trailer.
	*/
	len = static_cast<u32>(fread(&recLen2, sizeof(recLen2), 1, mfr->activeDevice->fcb[unitNo]));

	if (len != 1)
	{
		logError(LogErrorLocation, "channel %02o - missing tape record trailer", mfr->activeChannel->id);
		tp->alert = true;
		tp->errorCode = EcMiscUnitError;
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
			fseek(mfr->activeDevice->fcb[unitNo], 1, SEEK_CUR);
		}
		else
		{
			logError(LogErrorLocation, "channel %02o - invalid tape record trailer: %d", mfr->activeChannel->id, recLen2);
			tp->alert = true;
			tp->errorCode = EcMiscUnitError;
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
static void mt669FuncBackspace(u8 mfrId)
{
	u32 recLen0;
	u32 recLen1;
	u32 recLen2;
	MMainFrame *mfr = BigIron->chasis[mfrId];

	i8 unitNo = mfr->activeDevice->selectedUnit;
	TapeParam *tp = static_cast<TapeParam *>(mfr->activeDevice->context[unitNo]);

	/*
	**  Check if we are already at the beginning of the tape.
	*/
	i32 position = ftell(mfr->activeDevice->fcb[unitNo]);
	if (position == 0)
	{
		tp->blockNo = 0;
		return;
	}

	/*
	**  Position to the previous record's trailer and read the length
	**  of the record (leaving the file position ahead of the just read
	**  record trailer).
	*/
	fseek(mfr->activeDevice->fcb[unitNo], -4, SEEK_CUR);
	u32 len = static_cast<u32>(fread(&recLen0, sizeof(recLen0), 1, mfr->activeDevice->fcb[unitNo]));
	fseek(mfr->activeDevice->fcb[unitNo], -4, SEEK_CUR);

	if (len != 1)
	{
		logError(LogErrorLocation, "channel %02o - missing tape record trailer", mfr->activeChannel->id);
		tp->alert = true;
		tp->errorCode = EcMiscUnitError;
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
		tp->alert = true;
		tp->errorCode = EcMiscUnitError;
		return;
	}

	position -= 4;
	if (recLen1 != 0)
	{
		/*
		**  Skip backward over the TAP record body and header.
		*/
		position -= 4 + recLen1;
		fseek(mfr->activeDevice->fcb[unitNo], position, SEEK_SET);

		/*
		**  Read and verify the TAP record header.
		*/
		len = static_cast<u32>(fread(&recLen2, sizeof(recLen2), 1, mfr->activeDevice->fcb[unitNo]));

		if (len != 1)
		{
			logError(LogErrorLocation, "channel %02o - missing TAP record header", mfr->activeChannel->id);
			tp->alert = true;
			tp->errorCode = EcMiscUnitError;
			return;
		}

		if (recLen0 != recLen2)
		{
			/*
			**  This is more weird shit to deal with "padded" TAP records.
			*/
			position -= 1;
			fseek(mfr->activeDevice->fcb[unitNo], position, SEEK_SET);
			len = static_cast<u32>(fread(&recLen2, sizeof(recLen2), 1, mfr->activeDevice->fcb[unitNo]));

			if (len != 1 || recLen0 != recLen2)
			{
				logError(LogErrorLocation, "channel %02o - invalid record length2: %d %08X != %08X", mfr->activeChannel->id, len, recLen0, recLen2);
				tp->alert = true;
				tp->errorCode = EcMiscUnitError;
				return;
			}
		}

		/*
		**  Position to the TAP record header.
		*/
		fseek(mfr->activeDevice->fcb[unitNo], position, SEEK_SET);
	}
	else
	{
		/*
		**  A tape mark consists of only a single TAP record header of zero.
		*/
		tp->fileMark = true;

#if DEBUG
		fprintf(mt669Log, "Tape mark\n");
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
**  Purpose:        Convert function code to string.
**
**  Parameters:     Name        Description.
**                  funcCode    function code
**
**  Returns:        String equivalent of function code.
**
**------------------------------------------------------------------------*/
static char *mt669Func2String(PpWord funcCode)
{
	static char buf[30];
#if DEBUG
	switch (funcCode)
	{
	case Fc669FormatUnit: return "Fc669FormatUnit";
	case Fc669LoadConversion1: return "Fc669LoadConversion1";
	case Fc669LoadConversion2: return "Fc669LoadConversion2";
	case Fc669LoadConversion3: return "Fc669LoadConversion3";
	case Fc669Connect + 0: return "Fc669Connect + 0";
	case Fc669Connect + 1: return "Fc669Connect + 1";
	case Fc669Connect + 2: return "Fc669Connect + 2";
	case Fc669Connect + 3: return "Fc669Connect + 3";
	case Fc669Connect + 4: return "Fc669Connect + 4";
	case Fc669Connect + 5: return "Fc669Connect + 5";
	case Fc669Connect + 6: return "Fc669Connect + 6";
	case Fc669Connect + 7: return "Fc669Connect + 7";
	case Fc669Release: return "Fc669Release";
	case Fc669ClearReserve: return "Fc669ClearReserve";
	case Fc669ClearOppositeReserve: return "Fc669ClearOppositeReserve";
	case Fc669Rewind: return "Fc669Rewind";
	case Fc669RewindUnload: return "Fc669RewindUnload";
	case Fc669SearchTapeMarkF: return "Fc669SearchTapeMarkF";
	case Fc669SearchTapeMarkB: return "Fc669SearchTapeMarkB";
	case Fc669CtrlForespaceFindGap: return "Fc669CtrlForespaceFindGap";
	case Fc669CtrlBackspaceFindGap: return "Fc669CtrlBackspaceFindGap";
	case Fc669Forespace: return "Fc669Forespace";
	case Fc669Backspace: return "Fc669Backspace";
	case Fc669WriteTapeMark: return "Fc669WriteTapeMark";
	case Fc669EraseToEOT: return "Fc669EraseToEOT";
	case Fc669CtrledForespace: return "Fc669CtrledForespace";
	case Fc669CtrledBackspace: return "Fc669CtrledBackspace";
	case Fc669StopMotion: return "Fc669StopMotion";
	case Fc669ReadFwd: return "Fc669ReadFwd";
	case Fc669ReadBkw: return "Fc669ReadBkw";
	case Fc669Write: return "Fc669Write";
	case Fc669WriteOdd12: return "Fc669WriteOdd12";
	case Fc669WriteOdd: return "Fc669WriteOdd";
	case Fc669GeneralStatus: return "Fc669GeneralStatus";
	case Fc669DetailedStatus: return "Fc669DetailedStatus";
	case Fc669CumulativeStatus: return "Fc669CumulativeStatus";
	case Fc669UnitReadyStatus: return "Fc669UnitReadyStatus";
	case Fc669SetReadClipNorm: return "Fc669SetReadClipNorm";
	case Fc669SetReadClipHigh: return "Fc669SetReadClipHigh";
	case Fc669SetReadClipLow: return "Fc669SetReadClipLow";
	case Fc669SetReadClipHyper: return "Fc669SetReadClipHyper";
	case Fc669ReadSprktDlyNorm: return "Fc669ReadSprktDlyNorm";
	case Fc669ReadSprktDlyIncr: return "Fc669ReadSprktDlyIncr";
	case Fc669ReadSprktDlyDecr: return "Fc669ReadSprktDlyDecr";
	case Fc669OppParity: return "Fc669OppParity";
	case Fc669OppDensity: return "Fc669OppDensity";
	case Fc669LongForespace: return "Fc669LongForespace";
	case Fc669LongBackspace: return "Fc669LongBackspace";
	case Fc669RereadFwd: return "Fc669RereadFwd";
	case Fc669RereadBkw: return "Fc669RereadBkw";
	case Fc669ReadBkwOddLenParity: return "Fc669ReadBkwOddLenParity";
	case Fc669RereadBkwOddLenParity: return "Fc669RereadBkwOddLenParity";
	case Fc669RepeatRead: return "Fc669RepeatRead";
	case Fc669Erase: return "Fc669Erase";
	case Fc669WriteRepos: return "Fc669WriteRepos";
	case Fc669WriteEraseRepos: return "Fc669WriteEraseRepos";
	case Fc669WriteReposiCtrl: return "Fc669WriteReposiCtrl";
	case Fc669WriteEraseReposCtrl: return "Fc669WriteEraseReposCtrl";
	case Fc669EraseRepos: return "Fc669EraseRepos";
	case Fc669EraseEraseRepos: return "Fc669EraseEraseRepos";
	case Fc669LoadReadRam: return "Fc669LoadReadRam";
	case Fc669LoadWriteRam: return "Fc669LoadWriteRam";
	case Fc669LoadReadWriteRam: return "Fc669LoadReadWriteRam";
	case Fc669CopyReadRam: return "Fc669CopyReadRam";
	case Fc669CopyWriteRam: return "Fc669CopyWriteRam";
	case Fc669FormatTcuUnitStatus: return "Fc669FormatTcuUnitStatus";
	case Fc669CopyTcuStatus: return "Fc669CopyTcuStatus";
	case Fc669SendTcuCmd: return "Fc669SendTcuCmd";
	case Fc669SetQuartReadSprktDly: return "Fc669SetQuartReadSprktDly";
	case Fc669ConnectRewindRead + 0: return "Fc669ConnectRewindRead + 0";
	case Fc669ConnectRewindRead + 1: return "Fc669ConnectRewindRead + 1";
	case Fc669ConnectRewindRead + 2: return "Fc669ConnectRewindRead + 2";
	case Fc669ConnectRewindRead + 3: return "Fc669ConnectRewindRead + 3";
	case Fc669ConnectRewindRead + 4: return "Fc669ConnectRewindRead + 4";
	case Fc669ConnectRewindRead + 5: return "Fc669ConnectRewindRead + 5";
	case Fc669ConnectRewindRead + 6: return "Fc669ConnectRewindRead + 6";
	case Fc669ConnectRewindRead + 7: return "Fc669ConnectRewindRead + 7";
	case Fc669MasterClear: return "Fc669MasterClear";
	case Fc669ClearUnit: return "Fc669ClearUnit";
	}
#endif
	sprintf(buf, "UNKNOWN: %04o", funcCode);
	return(buf);
}

/*---------------------------  End Of File  ------------------------------*/
