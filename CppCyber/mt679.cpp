/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2011, Tom Hunter
**  C++ adaptation by Dale Sinder 2017
**
**  Name: mt679.c
**
**  Description:
**      Perform emulation of CDC 6600 679 tape drives attached to a
**      7021-31 magnetic tape controller.
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
**  ATS tape function codes:
*/
#define Fc679ClearUnit          00000
#define Fc679Release            00001
#define Fc679FormatUnit         00004
#define Fc679OppositeParity     00005
#define Fc679OppositeDensity    00105
#define Fc679SetReadClipNorm    00006
#define Fc679SetReadClipHigh    00106
#define Fc679SetReadClipLow     00206
#define Fc679SetReadClipHyper   00306
#define Fc679Rewind             00010
#define Fc679RewindUnload       00110
#define Fc679StopMotion         00011
#define Fc679GeneralStatus      00012
#define Fc679DetailedStatus     00112
#define Fc679UnitStatus         00212
#define Fc679Forespace          00013
#define Fc679Backspace          00113
#define Fc679CtrledBackspace    00114
#define Fc679SearchTapeMarkF    00015
#define Fc679SearchTapeMarkB    00115
#define Fc679Connect            00020
#define Fc679WarmstartHighDens  00120
#define Fc679WarmstartLowDens   00320
#define Fc679ReadFwd            00040
#define Fc679ReadBkw            00140
#define Fc679CopyReadConv       00047
#define Fc679CopyWriteConv      00247
#define Fc679Write              00050
#define Fc679WriteShort         00250
#define Fc679WriteTapeMark      00051
#define Fc679Erase              00052
#define Fc679EraseDataSecurity  00252
#define Fc679LoadReadConv       00057
#define Fc679LoadWriteConv      00257
#define Fc679RewindOnEOT        00060
#define Fc679WaitForStop        00061
#define Fc679TestVelocityVect   00071
#define Fc679MeasureGapSizeFwd  00072
#define Fc679MeasureGapSizeBkw  00172
#define Fc679MeasureStartTFwd   00073
#define Fc679SetTransferCheckCh 00074
#define Fc679SetLoopWTRTcu      00075
#define Fc679SetLoopWTR1TU      00175
#define Fc679SetLoopWTR2TU      00275
#define Fc679SetEvenWrParity    00076
#define Fc679SetEvenChParity    00176
#define Fc679ForceDataErrors    00077
#define Fc679MasterClear        00414

/*
**  General status reply:
*/
#define St679Alert              04000
#define St679NoUnit             01000
#define St679WriteEnabled       00200
#define St679NineTrack          00100
#define St679CharacterFill      00040
#define St679TapeMark           00020
#define St679EOT                00010
#define St679BOT                00004
#define St679Busy               00002
#define St679Ready              00001

/*
**  Detailed status error codes.
*/
#define EcMissingRing           006       
#define EcBlankTape             010       
#define EcBackPastLoadpoint     030
#define EcIllegalUnit           031
#define EcIllegalFunction       050       
#define EcNoTapeUnitConnected   051
#define EcNoFuncParams          052
#define EcDiagnosticError       070

/*
**  Misc constants.
*/
#define MaxPpBuf                40000
#define MaxByteBuf              60000
#define MaxPackedConvBuf        (((256 * 8) + 11) / 12)
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
**  ATS controller.
*/
typedef struct ctrlParam
{
	FILE        *convFileHandle;
	u8          readConv[4][256];
	u8          writeConv[4][256];
	PpWord      packedConv[MaxPackedConvBuf];

	u8          selectedConversion;
	bool        packedMode;
	u8          density;
	u8          minBlockLength;
	bool        lwrMode;
	bool        writing;
	bool        oddFrameCount;

	PpWord      controllerStatus[17];   // first element not used
} CtrlParam;

/*
**  ATS tape unit.
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
	**  Dynamic state.
	*/
	bool        alert;
	bool        endOfTape;
	bool        fileMark;
	bool        unitReady;
	bool        ringIn;
	bool        characterFill;
	bool        flagBitDetected;
	bool        rewinding;
	bool        suppressBot;
	u32         rewindStart;
	u16         blockCrc;
	u8          errorCode;

	u32         blockNo;
	PpWord      recordLength;
	PpWord      deviceStatus[17];   // first element not used
	PpWord      ioBuffer[MaxPpBuf];
	PpWord      *bp;
} TapeParam;

/*
**  ---------------------------
**  Private Function Prototypes
**  ---------------------------
*/
static void mt679ResetStatus(TapeParam *tp);
static void mt679SetupStatus(TapeParam *tp, u8 mfrId);
static void mt679PackConversionTable(u8 *convTable, u8 mfrId);
static void mt679UnpackConversionTable(u8 *convTable, u8 mfrId);
static void mt679Unpack6BitTable(u8 *convTable, u8 mfrId);
static FcStatus mt679Func(PpWord funcCode, u8 mfrId);
static void mt679Io(u8 mfrId);
static void mt679Activate(u8 mfrId);
static void mt679Disconnect(u8 mfrId);
static void mt679FlushWrite(u8 mfrId);
static void mt679PackAndConvert(u32 recLen, u8 mfrId);
static void mt679FuncRead(u8 mfrId);
static void mt679FuncForespace(u8 mfrId);
static void mt679FuncBackspace(u8 mfrId);
static void mt679FuncReadBkw(u8 mfrId);
static char *mt679Func2String(PpWord funcCode);

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
static TapeParam *firstTape = NULL;
static TapeParam *lastTape = NULL;
static u8 rawBuffer[MaxByteBuf];

#if DEBUG
static FILE *mt679Log = NULL;
#endif

/*
**--------------------------------------------------------------------------
**
**  Public Functions
**
**--------------------------------------------------------------------------
*/
/*--------------------------------------------------------------------------
**  Purpose:        Initialise 679 tape drives.
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
void mt679Init(u8 mfrID, u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName)
{
	DevSlot *dp;
	FILE *fcb;
	TapeParam *tp;

#if DEBUG
	if (mt679Log == NULL)
	{
		mt679Log = fopen("mt679log.txt", "wt");
	}
#endif

	/*
	**  Attach device to channel.
	*/
	dp = channelAttach(channelNo, eqNo, DtMt679, mfrID);

	/*
	**  Setup channel functions.
	*/
	dp->activate = mt679Activate;
	dp->disconnect = mt679Disconnect;
	dp->func = mt679Func;
	dp->io = mt679Io;
	dp->selectedUnit = -1;

	/*
	**  Setup controller context.
	*/
	if (dp->controllerContext == NULL)
	{
		dp->controllerContext = calloc(1, sizeof(CtrlParam));

		/*
		**  Optionally read in persistent conversion tables.
		*/
		if (*persistDir != '\0')
		{
			CtrlParam *cp = (CtrlParam*)dp->controllerContext;
			char fileName[256];

			/*
			**  Try to open existing backing file.
			*/
			sprintf(fileName, "%s/mt679StoreC%02oE%02o", persistDir, channelNo, eqNo);
			cp->convFileHandle = fopen(fileName, "r+b");
			if (cp->convFileHandle != NULL)
			{
				/*
				**  Read conversion table contents.
				*/
				if (fread(cp->writeConv, 1, sizeof(cp->writeConv), cp->convFileHandle) != sizeof(cp->writeConv)
					|| fread(cp->readConv, 1, sizeof(cp->readConv), cp->convFileHandle) != sizeof(cp->readConv)
					|| fread(cp->packedConv, 1, sizeof(cp->packedConv), cp->convFileHandle) != sizeof(cp->packedConv))
				{
					printf("Unexpected length of MT679 backing file, clearing tables\n");
					memset(cp->writeConv, 0, sizeof(cp->writeConv));
					memset(cp->readConv, 0, sizeof(cp->readConv));
					memset(cp->packedConv, 0, sizeof(cp->packedConv));
				}
			}
			else
			{
				/*
				**  Create a new file.
				*/
				cp->convFileHandle = fopen(fileName, "w+b");
				if (cp->convFileHandle == NULL)
				{
					fprintf(stderr, "Failed to create MT679 backing file\n");
					exit(1);
				}
			}
		}
	}

	/*
	**  Setup tape unit parameter block.
	*/
	tp = (TapeParam*)calloc(1, sizeof(TapeParam));
	if (tp == NULL)
	{
		fprintf(stderr, "Failed to allocate MT679 context block\n");
		exit(1);
	}

	dp->context[unitNo] = tp;

	/*
	**  Link into list of tape units.
	*/
	if (lastTape == NULL)
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
	if (deviceName != NULL)
	{
		strncpy(tp->fileName, deviceName, _MAX_PATH);
		fcb = fopen(deviceName, "rb");
		if (fcb == NULL)
		{
			fprintf(stderr, "Failed to open %s\n", deviceName);
			exit(1);
		}

		dp->fcb[unitNo] = fcb;

		tp->blockNo = 0;
		tp->unitReady = TRUE;
	}
	else
	{
		dp->fcb[unitNo] = NULL;
		tp->unitReady = FALSE;
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
	tp->ringIn = FALSE;

	/*
	**  Print a friendly message.
	*/
	printf("MT679 initialised on channel %o equipment %o unit %o mainframe %o\n", channelNo, eqNo, unitNo, mfrID);
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
void mt679Terminate(DevSlot *dp)
{
	CtrlParam *cp = (CtrlParam*)dp->controllerContext;

	/*
	**  Optionally save conversion tables.
	*/
	if (cp->convFileHandle != NULL)
	{
		fseek(cp->convFileHandle, 0, SEEK_SET);
		if (fwrite(cp->writeConv, 1, sizeof(cp->writeConv), cp->convFileHandle) != sizeof(cp->writeConv)
			|| fwrite(cp->readConv, 1, sizeof(cp->readConv), cp->convFileHandle) != sizeof(cp->readConv)
			|| fwrite(cp->packedConv, 1, sizeof(cp->packedConv), cp->convFileHandle) != sizeof(cp->packedConv))
		{
			fprintf(stderr, "Error writing MT679 backing file\n");
		}

		fclose(cp->convFileHandle);
		cp->convFileHandle = NULL;
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
void mt679LoadTape(char *params)
{
	static char str[200];
	DevSlot *dp;
	int numParam;
	int channelNo;
	int equipmentNo;
	int unitNo;
	int mfrID;
	TapeParam *tp;
	FILE *fcb;
	u8 unitMode;

	/*
	**  Operator inserted a new tape.
	*/
	numParam = sscanf(params, "%o,%o,%o,%o,%c,%s", &mfrID, &channelNo, &equipmentNo, &unitNo, &unitMode, str);

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
	dp = channelFindDevice((u8)channelNo, DtMt679, mfrID);
	if (dp == NULL)
	{
		return;
	}

	/*
	**  Check if the unit is even configured.
	*/
	tp = (TapeParam *)dp->context[unitNo];
	if (tp == NULL)
	{
		printf("Unit %d not allocated\n", unitNo);
		return;
	}

	/*
	**  Check if the unit has been unloaded.
	*/
	if (dp->fcb[unitNo] != NULL)
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
		if (fcb == NULL)
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
	if (fcb == NULL)
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
	mt679ResetStatus(tp);
	tp->ringIn = unitMode == 'w';
	tp->blockNo = 0;
	tp->unitReady = TRUE;

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
void mt679UnloadTape(char *params)
{
	DevSlot *dp;
	int numParam;
	int channelNo;
	int equipmentNo;
	int unitNo;
	int mfrID;
	TapeParam *tp;

	/*
	**  Operator inserted a new tape.
	*/
	numParam = sscanf(params, "%o,%o,%o,%o", &mfrID, &channelNo, &equipmentNo, &unitNo);

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
	dp = channelFindDevice((u8)channelNo, DtMt679, mfrID);
	if (dp == NULL)
	{
		return;
	}

	/*
	**  Check if the unit is even configured.
	*/
	tp = (TapeParam *)dp->context[unitNo];
	if (tp == NULL)
	{
		printf("Unit %d not allocated\n", unitNo);
		return;
	}

	/*
	**  Check if the unit has been unloaded.
	*/
	if (dp->fcb[unitNo] == NULL)
	{
		printf("Unit %d not loaded\n", unitNo);
		return;
	}

	/*
	**  Close the file.
	*/
	fclose(dp->fcb[unitNo]);
	dp->fcb[unitNo] = NULL;

	/*
	**  Clear show_tape path name.
	*/
	memset(tp->fileName, '0', _MAX_PATH);

	/*
	**  Setup status.
	*/
	mt679ResetStatus(tp);
	tp->unitReady = FALSE;
	tp->ringIn = FALSE;
	tp->rewinding = FALSE;
	tp->rewindStart = 0;
	tp->blockCrc = 0;
	tp->blockNo = 0;

	printf("Successfully unloaded MT679 on channel %o equipment %o unit %o\n", channelNo, equipmentNo, unitNo);
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
void mt679ShowTapeStatus(void)
{
	TapeParam *tp = firstTape;

	while (tp)
	{
		printf("MT679 on %o,%o,%o", tp->channelNo, tp->eqNo, tp->unitNo);
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
static void mt679ResetStatus(TapeParam *tp)
{
	if (tp != NULL)
	{
		tp->alert = FALSE;
		tp->endOfTape = FALSE;
		tp->fileMark = FALSE;
		tp->characterFill = FALSE;
		tp->flagBitDetected = FALSE;
		tp->suppressBot = FALSE;
		tp->errorCode = 0;
	}
}

/*--------------------------------------------------------------------------
**  Purpose:        Setup device status based on current tape parameters.
**
**  Parameters:     Name        Description.
**                  tp          pointer to tape parameters
**
**  Returns:        Nothing
**
**------------------------------------------------------------------------*/
static void mt679SetupStatus(TapeParam *tp, u8 mfrId)
{
	MMainFrame *mfr = BigIron->chasis[mfrId];

	CtrlParam *cp = (CtrlParam*)mfr->activeDevice->controllerContext;

	if (tp != NULL)
	{
		tp->deviceStatus[0] = 0;        // unused

										/*
										**  General status.
										*/
		tp->deviceStatus[1] = St679NineTrack;

		if (tp->alert)
		{
			tp->deviceStatus[1] |= St679Alert;
		}

		if (tp->ringIn)
		{
			tp->deviceStatus[1] |= St679WriteEnabled;
		}

		if (tp->characterFill)
		{
			tp->deviceStatus[1] |= St679CharacterFill;
		}

		if (tp->fileMark)
		{
			tp->deviceStatus[1] |= St679TapeMark;
		}

		if (tp->endOfTape)
		{
			tp->deviceStatus[1] |= St679EOT;
		}

		if (tp->rewinding)
		{
			tp->deviceStatus[1] |= St679Busy;
			if (labs(mfr->activeDevice->mfr->cycles - tp->rewindStart) > 1000)
			{
				tp->rewinding = FALSE;
				tp->blockNo = 0;
			}
		}
		else
		{
			if (tp->blockNo == 0 && !tp->suppressBot)
			{
				tp->deviceStatus[1] |= St679BOT;
			}
		}

		if (tp->unitReady)
		{
			tp->deviceStatus[1] |= St679Ready;
			if (ftell(mfr->activeDevice->fcb[mfr->activeDevice->selectedUnit]) > MaxTapeSize)
			{
				tp->deviceStatus[1] |= St679EOT;
			}
		}

		tp->deviceStatus[2] = (tp->blockCrc & Mask9) << 3;

		/*
		**  Detailed status.
		*/
		tp->deviceStatus[3] = tp->errorCode;
		tp->deviceStatus[5] = 0;

		if (tp->flagBitDetected)
		{
			tp->deviceStatus[5] |= 00004;
		}

		tp->deviceStatus[6] = 0;
		tp->deviceStatus[7] = 0;

		tp->deviceStatus[8] = 0;

		if (cp->packedMode)
		{
			tp->deviceStatus[8] |= 01000;
		}

		if (cp->selectedConversion != 0)
		{
			tp->deviceStatus[8] |= 02000;
		}

		tp->deviceStatus[9] = 0;
		tp->deviceStatus[10] = 0500;

		/*
		**  Unit status.
		*/
		tp->deviceStatus[11] = 04072;       // GCR, dual density, 6250 cpi, 100 ips
		tp->deviceStatus[12] = 0;
		tp->deviceStatus[13] = 00043;       // parked + cartridge open and present
		tp->deviceStatus[14] = 00132;       // auto hub activated, tape present & loaded
		tp->deviceStatus[15] = 0;
		tp->deviceStatus[16] = 00040;       // IBG counter
	}
	else
	{
		/*
		**  General status.
		*/
		cp->controllerStatus[0] = 0;
		cp->controllerStatus[1] = St679NoUnit | St679NineTrack;
		cp->controllerStatus[2] = 0;

		/*
		**  Detailed status.
		*/
		cp->controllerStatus[3] = 0;
		cp->controllerStatus[5] = 0;
		cp->controllerStatus[6] = 0;
		cp->controllerStatus[7] = 0;

		cp->controllerStatus[8] = 01000;

		if (cp->selectedConversion != 0)
		{
			cp->controllerStatus[8] |= 02000;
		}

		cp->controllerStatus[9] = 0;
		cp->controllerStatus[10] = 0500;

		/*
		**  Unit status.
		*/
		cp->controllerStatus[11] = 0;
		cp->controllerStatus[12] = 0;
		cp->controllerStatus[13] = 0;
		cp->controllerStatus[14] = 0;
		cp->controllerStatus[15] = 0;
		cp->controllerStatus[16] = 0;
	}
}

/*--------------------------------------------------------------------------
**  Purpose:        Pack a conversion table into PP words.
**
**  Parameters:     Name        Description.
**                  convTable   conversion table
**
**  Returns:        FcStatus
**
**------------------------------------------------------------------------*/
static void mt679PackConversionTable(u8 *convTable, u8 mfrId)
{
	MMainFrame *mfr = BigIron->chasis[mfrId];

	CtrlParam *cp = (CtrlParam*)mfr->activeDevice->controllerContext;
	PpWord *op = cp->packedConv;
	u8 *ip = convTable;
	u16 c1, c2, c3;
	int i;

	for (i = 0; i < 85; i++)
	{
		c1 = *ip++;
		c2 = *ip++;
		c3 = *ip++;

		*op++ = ((c1 << 4) | (c2 >> 4)) & Mask12;
		*op++ = ((c2 << 8) | (c3 >> 0)) & Mask12;
	}

	// ReSharper disable once CppAssignedValueIsNeverUsed
	c1 = *ip++;
	c2 = *convTable;    // wrap

	// ReSharper disable once CppAssignedValueIsNeverUsed
	*op++ = ((c1 << 4) | (c2 >> 4)) & Mask12;
}

/*--------------------------------------------------------------------------
**  Purpose:        Pack a conversion table into PP words.
**
**  Parameters:     Name        Description.
**                  convTable   conversion table
**
**  Returns:        FcStatus
**
**------------------------------------------------------------------------*/
static void mt679Pack6BitTable(u8 *convTable, u8 mfrId)
{
	MMainFrame *mfr = BigIron->chasis[mfrId];

	CtrlParam *cp = (CtrlParam*)mfr->activeDevice->controllerContext;
	PpWord *op = cp->packedConv;
	u8 *ip = convTable;
	int i;

	memset(cp->packedConv, 0, MaxPackedConvBuf * sizeof(PpWord));

	for (i = 0; i < 128; i++)
	{
		*op++ = ((ip[0] << 6) | ip[1]) & Mask12;
		ip += 2;
	}
}

/*--------------------------------------------------------------------------
**  Purpose:        Unpack PP words into a conversion table.
**
**  Parameters:     Name        Description.
**                  convTable   conversion table
**
**  Returns:        FcStatus
**
**------------------------------------------------------------------------*/
static void mt679UnpackConversionTable(u8 *convTable, u8 mfrId)
{
	MMainFrame *mfr = BigIron->chasis[mfrId];

	CtrlParam *cp = (CtrlParam*)mfr->activeDevice->controllerContext;
	PpWord *ip = cp->packedConv;
	u8 *op = convTable;
	int i;

	for (i = 0; i < 85; i++)
	{
		*op++ = ((ip[0] >> 4) & 0xFF);
		*op++ = ((ip[0] << 4) & 0xF0) | ((ip[1] >> 8) & 0x0F);
		*op++ = ((ip[1] >> 0) & 0xFF);
		ip += 2;
	}

	// ReSharper disable once CppAssignedValueIsNeverUsed
	*op++ = ((ip[0] >> 4) & 0xFF);    // discard last 4 bits

#if DEBUG
	{
		int i;
		fprintf(mt679Log, "\nConversion Table %d", cp->selectedConversion);

		for (i = 0; i < 256; i++)
		{
			if (i % 16 == 0)
			{
				fprintf(mt679Log, "\n%02X :", i);
			}

			fprintf(mt679Log, " %02X", convTable[i]);
		}
	}
	fprintf(mt679Log, "\n");
#endif
}

/*--------------------------------------------------------------------------
**  Purpose:        Unpack PP words into a conversion table.
**
**  Parameters:     Name        Description.
**                  convTable   conversion table
**
**  Returns:        FcStatus
**
**------------------------------------------------------------------------*/
static void mt679Unpack6BitTable(u8 *convTable, u8 mfrId)
{
	MMainFrame *mfr = BigIron->chasis[mfrId];

	CtrlParam *cp = (CtrlParam*)mfr->activeDevice->controllerContext;
	PpWord *ip = cp->packedConv;
	u8 *op = convTable;
	int i;

	for (i = 0; i < 128; i++)
	{
		*op++ = (*ip >> 6) & 0x3F;
		*op++ = (*ip >> 0) & 0x3F;
		ip += 1;
	}

#if DEBUG
	{
		int i;
		fprintf(mt679Log, "\nConversion Table %d", cp->selectedConversion);

		for (i = 0; i < 256; i++)
		{
			if (i % 16 == 0)
			{
				fprintf(mt679Log, "\n%02X :", i);
			}

			fprintf(mt679Log, " %02X", convTable[i]);
		}
	}
	fprintf(mt679Log, "\n");
#endif
}

/*--------------------------------------------------------------------------
**  Purpose:        Execute function code on 679 tape drives.
**
**  Parameters:     Name        Description.
**                  funcCode    function code
**
**  Returns:        FcStatus
**
**------------------------------------------------------------------------*/
static FcStatus mt679Func(PpWord funcCode, u8 mfrId)
{
	u32 recLen1;
	i8 unitNo;
	TapeParam *tp;
	MMainFrame *mfr = BigIron->chasis[mfrId];

	CtrlParam *cp = (CtrlParam*)mfr->activeDevice->controllerContext;

	unitNo = mfr->activeDevice->selectedUnit;
	if (unitNo != -1)
	{
		tp = (TapeParam *)mfr->activeDevice->context[unitNo];
	}
	else
	{
		tp = NULL;
	}

#if DEBUG
	fprintf(mt679Log, "\n%06d PP:%02o CH:%02o u:%d f:%04o T:%-25s  >   ",
		traceSequenceNo,
		activePpu->id,
		activeDevice->channel->id,
		unitNo,
		funcCode,
		mt679Func2String(funcCode));
#endif

	/*
	**  Reset function code.
	*/
	mfr->activeDevice->fcode = 0;
	mfr->activeChannel->full = FALSE;

	/*
	**  Controller has a hard-wired equipment number which must match the top
	**  three bits of the function code.
	*/
	if (((funcCode >> 9) & Mask3) != mfr->activeDevice->eqNo)
	{
		/*
		**  Not for us.
		*/
		return(FcDeclined);
	}

	/*
	**  Strip of the equipment number.
	*/
	funcCode &= Mask9;

	/*
	**  Flush write data if necessary.
	*/
	if (cp->writing)
	{
		mt679FlushWrite(mfrId);
	}

	/*
	**  Process tape function.
	*/
	switch (funcCode)
	{
	default:
#if DEBUG
		fprintf(mt679Log, " FUNC not implemented & declined!");
#endif
		if (unitNo != -1)
		{
			tp->errorCode = EcIllegalFunction;
			tp->alert = TRUE;
		}
		return(FcDeclined);

	case Fc679ClearUnit:
		if (unitNo != -1)
		{
			mfr->activeDevice->recordLength = 0;
			tp->recordLength = 0;
			tp->errorCode = 0;
			mt679ResetStatus(tp);
		}
		return(FcProcessed);

	case Fc679Release:
		mfr->activeDevice->selectedUnit = -1;
		return(FcProcessed);

	case Fc679FormatUnit:
		mfr->activeDevice->fcode = funcCode;
		mfr->activeDevice->recordLength = 3;
		mt679ResetStatus(tp);
		break;

	case Fc679OppositeParity:
	case Fc679OppositeDensity:
		mt679ResetStatus(tp);
		return(FcProcessed);

	case Fc679SetReadClipNorm:
	case Fc679SetReadClipHigh:
	case Fc679SetReadClipLow:
	case Fc679SetReadClipHyper:
		mt679ResetStatus(tp);
		return(FcProcessed);

	case Fc679Rewind:
		if (unitNo != -1 && tp->unitReady)
		{
			mt679ResetStatus(tp);
			fseek(mfr->activeDevice->fcb[unitNo], 0, SEEK_SET);
			if (tp->blockNo != 0)
			{
				if (!tp->rewinding)
				{
					tp->rewinding = TRUE;
					tp->rewindStart = mfr->activeDevice->mfr->cycles;
				}
			}
		}
		return(FcProcessed);

	case Fc679RewindUnload:
		if (unitNo != -1 && tp->unitReady)
		{
			mt679ResetStatus(tp);
			tp->blockNo = 0;
			tp->unitReady = FALSE;
			tp->ringIn = FALSE;
			fclose(mfr->activeDevice->fcb[unitNo]);
			mfr->activeDevice->fcb[unitNo] = NULL;
		}
		return(FcProcessed);

	case Fc679StopMotion:
		return(FcProcessed);

	case Fc679GeneralStatus:
		mfr->activeDevice->fcode = funcCode;
		mfr->activeDevice->recordLength = 16;
		mt679SetupStatus(tp, mfrId);
		break;

	case Fc679DetailedStatus:
		mfr->activeDevice->fcode = funcCode;
		mfr->activeDevice->recordLength = 14;
		mt679SetupStatus(tp, mfrId);
		break;

	case Fc679UnitStatus:
		mfr->activeDevice->fcode = funcCode;
		mfr->activeDevice->recordLength = 6;
		mt679SetupStatus(tp, mfrId);
		break;

	case Fc679Forespace:
		if (unitNo != -1 && tp->unitReady)
		{
			mt679ResetStatus(tp);
			mt679FuncForespace(mfrId);
		}
		return(FcProcessed);

	case Fc679Backspace:
		if (unitNo != -1 && tp->unitReady)
		{
			mt679ResetStatus(tp);
			mt679FuncBackspace(mfrId);
		}
		return(FcProcessed);

	case Fc679CtrledBackspace:
		logError(LogErrorLocation, "channel %02o - unsupported function: %04o", mfr->activeChannel->id, (u32)funcCode);
		return(FcProcessed);

	case Fc679SearchTapeMarkF:
		if (unitNo != -1 && tp->unitReady)
		{
			mt679ResetStatus(tp);

			do
			{
				mt679FuncForespace(mfrId);
			} while (!tp->fileMark && !tp->endOfTape && !tp->alert);
		}
		return(FcProcessed);

	case Fc679SearchTapeMarkB:
		if (unitNo != -1 && tp->unitReady)
		{
			mt679ResetStatus(tp);

			do
			{
				mt679FuncBackspace(mfrId);
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
			tp->alert = TRUE;
			tp->errorCode = EcBackPastLoadpoint;
		}

		tp->fileMark = FALSE;

		return(FcProcessed);

	case Fc679Connect + 0:
	case Fc679Connect + 1:
	case Fc679Connect + 2:
	case Fc679Connect + 3:
	case Fc679Connect + 4:
	case Fc679Connect + 5:
	case Fc679Connect + 6:
	case Fc679Connect + 7:
	case Fc679Connect + 010:
	case Fc679Connect + 011:
	case Fc679Connect + 012:
	case Fc679Connect + 013:
	case Fc679Connect + 014:
	case Fc679Connect + 015:
	case Fc679Connect + 016:
	case Fc679Connect + 017:
		unitNo = funcCode & Mask4;
		tp = (TapeParam *)mfr->activeDevice->context[unitNo];
		if (tp == NULL)
		{
			mfr->activeDevice->selectedUnit = -1;
			logError(LogErrorLocation, "channel %02o - invalid select: %04o", mfr->activeChannel->id, (u32)funcCode);
			return(FcDeclined);
		}

		mt679ResetStatus(tp);
		mfr->activeDevice->selectedUnit = unitNo;
		return(FcProcessed);

	case Fc679WarmstartHighDens + 0:
	case Fc679WarmstartHighDens + 1:
	case Fc679WarmstartHighDens + 2:
	case Fc679WarmstartHighDens + 3:
	case Fc679WarmstartHighDens + 4:
	case Fc679WarmstartHighDens + 5:
	case Fc679WarmstartHighDens + 6:
	case Fc679WarmstartHighDens + 7:
	case Fc679WarmstartHighDens + 010:
	case Fc679WarmstartHighDens + 011:
	case Fc679WarmstartHighDens + 012:
	case Fc679WarmstartHighDens + 013:
	case Fc679WarmstartHighDens + 014:
	case Fc679WarmstartHighDens + 015:
	case Fc679WarmstartHighDens + 016:
	case Fc679WarmstartHighDens + 017:
	case Fc679WarmstartLowDens + 0:
	case Fc679WarmstartLowDens + 1:
	case Fc679WarmstartLowDens + 2:
	case Fc679WarmstartLowDens + 3:
	case Fc679WarmstartLowDens + 4:
	case Fc679WarmstartLowDens + 5:
	case Fc679WarmstartLowDens + 6:
	case Fc679WarmstartLowDens + 7:
	case Fc679WarmstartLowDens + 010:
	case Fc679WarmstartLowDens + 011:
	case Fc679WarmstartLowDens + 012:
	case Fc679WarmstartLowDens + 013:
	case Fc679WarmstartLowDens + 014:
	case Fc679WarmstartLowDens + 015:
	case Fc679WarmstartLowDens + 016:
	case Fc679WarmstartLowDens + 017:
		unitNo = funcCode & 017;
		tp = (TapeParam *)mfr->activeDevice->context[unitNo];
		if (tp == NULL || !tp->unitReady)
		{
			mfr->activeDevice->selectedUnit = -1;
			logError(LogErrorLocation, "channel %02o - invalid select: %04o", mfr->activeChannel->id, (u32)funcCode);
			return(FcDeclined);
		}

		mt679ResetStatus(tp);
		mfr->activeDevice->selectedUnit = unitNo;
		fseek(mfr->activeDevice->fcb[unitNo], 0, SEEK_SET);
		cp->selectedConversion = 0;
		cp->packedMode = TRUE;
		tp->blockNo = 0;
		mfr->activeDevice->fcode = Fc679ReadFwd;
		mt679ResetStatus(tp);
		mt679FuncRead(mfrId);
		break;

	case Fc679ReadFwd:
		if (unitNo != -1 && tp->unitReady)
		{
			mfr->activeDevice->fcode = funcCode;
			mt679ResetStatus(tp);
			mt679FuncRead(mfrId);
			break;
		}
		return(FcProcessed);

	case Fc679ReadBkw:
		if (unitNo != -1 && tp->unitReady)
		{
			mfr->activeDevice->fcode = funcCode;
			mt679ResetStatus(tp);
			mt679FuncReadBkw(mfrId);
			break;
		}
		return(FcProcessed);

	case Fc679CopyReadConv:
		if (unitNo == -1 && cp->selectedConversion >= 1 && cp->selectedConversion <= 4)
		{
			mfr->activeDevice->fcode = funcCode;
			mfr->activeDevice->recordLength = 0;
			if (!cp->packedMode)
			{
				mt679Pack6BitTable(cp->readConv[cp->selectedConversion - 1], mfrId);
			}
			else
			{
				mt679PackConversionTable(cp->readConv[cp->selectedConversion - 1], mfrId);
			}
			break;
		}
		return(FcProcessed);

	case Fc679CopyWriteConv:
		if (unitNo == -1 && cp->selectedConversion >= 1 && cp->selectedConversion <= 4)
		{
			mfr->activeDevice->fcode = funcCode;
			mfr->activeDevice->recordLength = 0;
			if (!cp->packedMode)
			{
				mt679Pack6BitTable(cp->writeConv[cp->selectedConversion - 1], mfrId);
			}
			else
			{
				mt679PackConversionTable(cp->writeConv[cp->selectedConversion - 1], mfrId);
			}
			break;
		}
		return(FcProcessed);

	case Fc679Write:
	case Fc679WriteShort:
		if (cp->lwrMode || (unitNo != -1 && tp->unitReady && tp->ringIn))
		{
			mfr->activeDevice->fcode = funcCode;
			mt679ResetStatus(tp);
			tp->bp = tp->ioBuffer;
			mfr->activeDevice->recordLength = 0;
			cp->writing = TRUE;
			cp->oddFrameCount = funcCode == Fc679WriteShort;
			if (!cp->lwrMode)
			{
				tp->blockNo += 1;
			}
			break;
		}
		return(FcProcessed);

	case Fc679WriteTapeMark:
		if (unitNo != -1 && tp->unitReady && tp->ringIn)
		{
			mt679ResetStatus(tp);
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
			recLen1 = 0;
			fwrite(&recLen1, sizeof(recLen1), 1, mfr->activeDevice->fcb[unitNo]);
			tp->fileMark = TRUE;

			/*
			**  The following fseek prepares for any subsequent fread.
			*/
			fseek(mfr->activeDevice->fcb[unitNo], 0, SEEK_CUR);
		}

		return(FcProcessed);

	case Fc679Erase:
		return(FcProcessed);

	case Fc679EraseDataSecurity:
		if (unitNo != -1 && tp->unitReady && tp->ringIn)
		{
			// ? would be nice to truncate somehow
			logError(LogErrorLocation, "channel %02o - unsupported function: %04o", mfr->activeChannel->id, (u32)funcCode);
		}
		return(FcProcessed);

	case Fc679LoadReadConv:
	case Fc679LoadWriteConv:
		if (unitNo == -1)
		{
			mfr->activeDevice->fcode = funcCode;
			mfr->activeDevice->recordLength = 0;
			break;
		}

		return(FcProcessed);

	case Fc679RewindOnEOT:
	case Fc679WaitForStop:
	case Fc679TestVelocityVect:
	case Fc679MeasureGapSizeFwd:
	case Fc679MeasureGapSizeBkw:
	case Fc679MeasureStartTFwd:
	case Fc679SetTransferCheckCh:
	case Fc679SetLoopWTRTcu:
#if DEBUG
		fprintf(mt679Log, "maintenance functions not implemented");
#endif
		return(FcProcessed);

	case Fc679SetLoopWTR1TU:
	case Fc679SetLoopWTR2TU:
		if (unitNo != -1 && tp->unitReady)
		{
			cp->lwrMode = TRUE;
		}
		return(FcProcessed);

	case Fc679SetEvenWrParity:
	case Fc679SetEvenChParity:
	case Fc679ForceDataErrors:
#if DEBUG
		fprintf(mt679Log, "maintenance functions not implemented");
#endif
		return(FcProcessed);

	case Fc679MasterClear:
		mfr->activeDevice->selectedUnit = -1;
		mt679ResetStatus(NULL);
		return(FcProcessed);
	}

	return(FcAccepted);
}

/*--------------------------------------------------------------------------
**  Purpose:        Perform I/O on MT679.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void mt679Io(u8 mfrId)
{
	MMainFrame *mfr = BigIron->chasis[mfrId];

	CtrlParam *cp = (CtrlParam*)mfr->activeDevice->controllerContext;
	i8 unitNo;
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
	unitNo = mfr->activeDevice->selectedUnit;
	if (unitNo != -1)
	{
		tp = (TapeParam *)mfr->activeDevice->context[unitNo];
	}
	else
	{
		tp = NULL;
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

	case Fc679FormatUnit:
		if (mfr->activeDevice->recordLength > 0)
		{
			if (mfr->activeChannel->full)
			{
				wordNumber = 4 - mfr->activeDevice->recordLength;

#if DEBUG
				fprintf(mt679Log, " %04o", activeChannel->data);
#endif

				if (wordNumber == 1)
				{
					/*
					**  Process parameter word 1.
					*/
					param = mfr->activeChannel->data;

					if (((param >> 11) & 1) != 0)
					{
						cp->selectedConversion = (param >> 8) & Mask3;
						if (cp->selectedConversion > 4)
						{
							cp->selectedConversion = 0;
						}
					}

					if (((param >> 7) & 1) != 0)
					{
						cp->packedMode = ((param >> 5) & Mask2) == 1;
					}

					if (((param >> 4) & 1) != 0)
					{
						unitNo = param & Mask4;
						mfr->activeDevice->selectedUnit = unitNo;
						// ReSharper disable once CppAssignedValueIsNeverUsed
						tp = (TapeParam *)mfr->activeDevice->context[unitNo];
					}
				}

				if (wordNumber == 2)
				{
					/*
					**  Process parameter word 2.
					*/
					param = mfr->activeChannel->data;

					if (((param >> 8) & 1) != 0)
					{
						cp->density = (param >> 6) & Mask2;
					}

					if (((param >> 5) & 1) != 0)
					{
						cp->minBlockLength = param & Mask5;
					}
				}

				if (wordNumber == 3)
				{
					/*
					**  Last parameter word deactivates function.
					*/
					mfr->activeDevice->fcode = 0;
				}

				mfr->activeDevice->recordLength -= 1;
			}

			mfr->activeChannel->full = FALSE;
		}
		break;


	case Fc679GeneralStatus:
	case Fc679DetailedStatus:
	case Fc679UnitStatus:
		if (!mfr->activeChannel->full)
		{
			if (mfr->activeDevice->recordLength > 0)
			{
				wordNumber = 17 - mfr->activeDevice->recordLength;
				if (tp == NULL)
				{
					mfr->activeChannel->data = cp->controllerStatus[wordNumber];
				}
				else
				{
					mfr->activeChannel->data = tp->deviceStatus[wordNumber];
				}
				mfr->activeDevice->recordLength -= 1;
				if (wordNumber == 16)
				{
					/*
					**  Last status word deactivates function.
					*/
					mfr->activeDevice->fcode = 0;
					mfr->activeChannel->discAfterInput = TRUE;
				}

				mfr->activeChannel->full = TRUE;
#if DEBUG
				fprintf(mt679Log, " %04o", activeChannel->data);
#endif
			}
		}
		break;

	case Fc679ReadFwd:
		if (mfr->activeChannel->full)
		{
			break;
		}

		if (tp->recordLength == 0)
		{
			mfr->activeChannel->active = FALSE;
			mfr->activeChannel->delayDisconnect = 0;
		}

		if (tp->recordLength > 0)
		{
			mfr->activeChannel->data = *tp->bp++;
			mfr->activeChannel->full = TRUE;
			tp->recordLength -= 1;
			if (tp->recordLength == 0)
			{
				/*
				**  It appears that NOS/BE relies on the disconnect to happen delayed.
				*/
				mfr->activeChannel->delayDisconnect = 10;
			}
		}
		break;

	case Fc679ReadBkw:
		if (mfr->activeChannel->full)
		{
			break;
		}

		if (tp->recordLength == 0)
		{
			mfr->activeChannel->active = FALSE;
		}

		if (tp->recordLength > 0)
		{
			mfr->activeChannel->data = *tp->bp--;
			mfr->activeChannel->full = TRUE;
			tp->recordLength -= 1;
			if (tp->recordLength == 0)
			{
				mfr->activeChannel->discAfterInput = TRUE;
			}
		}
		break;

	case Fc679CopyReadConv:
	case Fc679CopyWriteConv:
		if (mfr->activeChannel->full)
		{
			break;
		}

		if (mfr->activeDevice->recordLength < MaxPackedConvBuf)
		{
#if DEBUG
			if (activeDevice->recordLength % 8 == 0)
			{
				fprintf(mt679Log, "\n");
			}
#endif
			mfr->activeChannel->data = cp->packedConv[mfr->activeDevice->recordLength++];
#if DEBUG
			fprintf(mt679Log, " %04o", activeChannel->data);
#endif
		}
		else
		{
			mfr->activeChannel->data = 0;
		}

		mfr->activeChannel->full = TRUE;
		break;

	case Fc679Write:
	case Fc679WriteShort:
		if (mfr->activeChannel->full && mfr->activeDevice->recordLength < MaxPpBuf)
		{
			mfr->activeChannel->full = FALSE;
			mfr->activeDevice->recordLength += 1;
			*tp->bp++ = mfr->activeChannel->data;
		}
		break;

	case Fc679LoadReadConv:
	case Fc679LoadWriteConv:
		if (!mfr->activeChannel->full)
		{
			break;
		}

		mfr->activeChannel->full = FALSE;

		if (mfr->activeDevice->recordLength < MaxPackedConvBuf)
		{
#if DEBUG
			fprintf(mt679Log, " %04o", activeChannel->data);
			if (activeDevice->recordLength % 8 == 0)
			{
				fprintf(mt679Log, "\n");
			}
#endif
			cp->packedConv[mfr->activeDevice->recordLength++] = mfr->activeChannel->data;   // <<<<<<<<<<<<<<< add wrapping.
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
static void mt679Activate(u8 mfrId)
{
	MMainFrame *mfr = BigIron->chasis[mfrId];
#if DEBUG
	CtrlParam *cp = activeDevice->controllerContext;
	fprintf(mt679Log, "\n%06d PP:%02o CH:%02o Activate",
		traceSequenceNo,
		activePpu->id,
		activeDevice->channel->id);
#endif
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
static void mt679Disconnect(u8 mfrId)
{
	MMainFrame *mfr = BigIron->chasis[mfrId];

	CtrlParam *cp = (CtrlParam*)mfr->activeDevice->controllerContext;

#if DEBUG
	fprintf(mt679Log, "\n%06d PP:%02o CH:%02o Disconnect",
		traceSequenceNo,
		activePpu->id,
		activeDevice->channel->id);
#endif

	/*
	**  Abort pending device disconnects - the PP is doing the disconnect.
	*/
	mfr->activeChannel->delayDisconnect = 0;
	mfr->activeChannel->discAfterInput = FALSE;

	/*
	**  Flush conversion tables.
	*/
	// ReSharper disable once CppDefaultCaseNotHandledInSwitchStatement
	switch (mfr->activeDevice->fcode)
	{
	case Fc679LoadReadConv:
		if (!cp->packedMode)
		{
			if (cp->selectedConversion >= 1 && cp->selectedConversion <= 4)
			{
				mt679Unpack6BitTable(cp->readConv[cp->selectedConversion - 1], mfrId);
			}
		}
		else
		{
			if (cp->selectedConversion >= 1 && cp->selectedConversion <= 4)
			{
				mt679UnpackConversionTable(cp->readConv[cp->selectedConversion - 1], mfrId);
			}
		}

		break;

	case Fc679LoadWriteConv:
		if (!cp->packedMode)
		{
			if (cp->selectedConversion >= 1 && cp->selectedConversion <= 4)
			{
				mt679Unpack6BitTable(cp->writeConv[cp->selectedConversion - 1], mfrId);
			}
		}
		else
		{
			if (cp->selectedConversion >= 1 && cp->selectedConversion <= 4)
			{
				mt679UnpackConversionTable(cp->writeConv[cp->selectedConversion - 1], mfrId);
			}
		}
		break;
	}
}

/*--------------------------------------------------------------------------
**  Purpose:        Flush accumulated write data.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void mt679FlushWrite(u8 mfrId)
{
	MMainFrame *mfr = BigIron->chasis[mfrId];

	CtrlParam *cp = (CtrlParam*)mfr->activeDevice->controllerContext;
	FILE *fcb;
	TapeParam *tp;
	i8 unitNo;
	u32 i;
	u64 recLen0;
	u64 recLen1;
	u64 recLen2;
	PpWord *ip;
	u8 *rp;
	u8 *writeConv;

	unitNo = mfr->activeDevice->selectedUnit;
	tp = (TapeParam *)mfr->activeDevice->context[unitNo];

	if (unitNo == -1 || !tp->unitReady)
	{
		return;
	}

	if (cp->lwrMode)
	{
		cp->lwrMode = FALSE;
		cp->writing = FALSE;
		cp->oddFrameCount = FALSE;
		return;
	}

	fcb = mfr->activeDevice->fcb[unitNo];
	tp->bp = tp->ioBuffer;
	recLen0 = 0;
	recLen2 = mfr->activeDevice->recordLength;
	ip = tp->ioBuffer;
	rp = rawBuffer;

	// ReSharper disable once CppDefaultCaseNotHandledInSwitchStatement
	switch (cp->selectedConversion)
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

		recLen0 = rp - rawBuffer;

		if ((recLen2 & 1) != 0)
		{
			recLen0 -= 2;
		}
		else if (cp->oddFrameCount)
		{
			recLen0 -= 1;
		}
		break;

	case 1:
	case 2:
	case 3:
	case 4:
		/*
		**  Convert the channel data to appropriate character set.
		*/
		writeConv = cp->writeConv[cp->selectedConversion - 1];

		for (i = 0; i < recLen2; i++)
		{
			*rp++ = writeConv[(*ip >> 6) & 077];
			*rp++ = writeConv[(*ip >> 0) & 077];
			ip += 1;
		}

		recLen0 = rp - rawBuffer;
		if (cp->oddFrameCount)
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
		recLen1 = MSystem::ConvertEndian((u32)recLen0);
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
	cp->writing = FALSE;
	cp->oddFrameCount = FALSE;
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
static void mt679PackAndConvert(u32 recLen, u8 mfrId)
{
	MMainFrame *mfr = BigIron->chasis[mfrId];

	i8 unitNo = mfr->activeDevice->selectedUnit;
	TapeParam *tp = (TapeParam*)mfr->activeDevice->context[unitNo];
	CtrlParam *cp = (CtrlParam*)mfr->activeDevice->controllerContext;
	u32 i;
	u16 c1, c2, c3;
	u16 *op;
	u8 *rp;
	u8 *readConv;

	/*
	**  Convert the raw data into PP words suitable for a channel.
	*/
	op = tp->ioBuffer;
	rp = rawBuffer;

	/*
	**  Fill the last few bytes with zeroes.
	*/
	rawBuffer[recLen + 0] = 0;
	rawBuffer[recLen + 1] = 0;

	switch (cp->selectedConversion)
	{
	default:
		break;

	case 0:
		/*
		**  Convert the raw data into PP Word data.
		*/
		for (i = 0; i < recLen; i += 3)
		{
			c1 = *rp++;
			c2 = *rp++;
			c3 = *rp++;

			*op++ = ((c1 << 4) | (c2 >> 4)) & Mask12;
			*op++ = ((c2 << 8) | (c3 >> 0)) & Mask12;
		}

		mfr->activeDevice->recordLength = (PpWord)(op - tp->ioBuffer);

		switch (recLen % 3)
		{
		default:
			break;

		case 1:     /* 2 words + 8 bits */
			mfr->activeDevice->recordLength -= 1;
			break;

		case 2:
			tp->characterFill = TRUE;
			break;
		}
		break;

	case 1:
	case 2:
	case 3:
	case 4:
		/*
		**  Convert the Raw data to appropriate character set.
		*/
		readConv = cp->readConv[cp->selectedConversion - 1];
		for (i = 0; i < recLen; i++)
		{
			c1 = readConv[*rp++];
			if ((c1 & (1 << 6)) != 0)
			{
				/*
				**  Indicate illegal character.
				*/
				tp->alert = TRUE;
				tp->flagBitDetected = TRUE;
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

		mfr->activeDevice->recordLength = (PpWord)(op - tp->ioBuffer);

		if ((recLen % 2) != 0)
		{
			mfr->activeDevice->recordLength += 1;
			tp->characterFill = TRUE;
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
static void mt679FuncRead(u8 mfrId)
{
	u32 len;
	u32 recLen0;
	u32 recLen1;
	u32 recLen2;
	i8 unitNo;
	TapeParam *tp;
	i32 position;
	MMainFrame *mfr = BigIron->chasis[mfrId];

	unitNo = mfr->activeDevice->selectedUnit;
	tp = (TapeParam *)mfr->activeDevice->context[unitNo];

	mfr->activeDevice->recordLength = 0;
	tp->recordLength = 0;

	/*
	**  Determine if the tape is at the load point.
	*/
	position = ftell(mfr->activeDevice->fcb[unitNo]);

	/*
	**  Read and verify TAP record length header.
	*/
	len = (u32)fread(&recLen0, sizeof(recLen0), 1, mfr->activeDevice->fcb[unitNo]);

	if (len != 1)
	{
		if (position == 0)
		{
			tp->errorCode = EcBlankTape;
		}
		else
		{
			//            tp->endOfTape = TRUE;
			tp->fileMark = TRUE;
#if DEBUG
			fprintf(mt679Log, "TAP is at EOF (simulate tape mark)\n");
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
		tp->alert = TRUE;
		tp->errorCode = EcDiagnosticError;
		return;
	}

	if (recLen1 == 0)
	{
		/*
		**  Report a tape mark and return.
		*/
		tp->fileMark = TRUE;
		tp->blockNo += 1;

#if DEBUG
		fprintf(mt679Log, "Tape mark\n");
#endif
		return;
	}

	/*
	**  Read and verify the actual raw data.
	*/
	len = (u32)fread(rawBuffer, 1, recLen1, mfr->activeDevice->fcb[unitNo]);

	if (recLen1 != (u32)len)
	{
		logError(LogErrorLocation, "channel %02o - short tape record read: %d", mfr->activeChannel->id, len);
		tp->alert = TRUE;
		tp->errorCode = EcDiagnosticError;
		return;
	}

	/*
	**  Read and verify the TAP record length trailer.
	*/
	len = (u32)fread(&recLen2, sizeof(recLen2), 1, mfr->activeDevice->fcb[unitNo]);

	if (len != 1)
	{
		logError(LogErrorLocation, "channel %02o - missing tape record trailer", mfr->activeChannel->id);
		tp->alert = TRUE;
		tp->errorCode = EcDiagnosticError;
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
			tp->alert = TRUE;
			tp->errorCode = EcDiagnosticError;
			return;
		}
	}

	/*
	**  Convert the raw data into PP words suitable for a channel.
	*/
	mt679PackAndConvert(recLen1, mfrId);

	/*
	**  Setup length, buffer pointer and block number.
	*/
#if DEBUG
	fprintf(mt679Log, "Read fwd %d PP words (%d 8-bit bytes)\n", activeDevice->recordLength, recLen1);
#endif

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
static void mt679FuncReadBkw(u8 mfrId)
{
	u32 len;
	u32 recLen0;
	u32 recLen1;
	u32 recLen2;
	i8 unitNo;
	TapeParam *tp;
	i32 position;

	MMainFrame *mfr = BigIron->chasis[mfrId];

	unitNo = mfr->activeDevice->selectedUnit;
	tp = (TapeParam *)mfr->activeDevice->context[unitNo];

	mfr->activeDevice->recordLength = 0;
	tp->recordLength = 0;

	/*
	**  Check if we are already at the beginning of the tape.
	*/
	position = ftell(mfr->activeDevice->fcb[unitNo]);
	if (position == 0)
	{
		tp->suppressBot = FALSE;
		tp->blockNo = 0;
		return;
	}

	/*
	**  Position to the previous record's trailer and read the length
	**  of the record (leaving the file position ahead of the just read
	**  record trailer).
	*/
	fseek(mfr->activeDevice->fcb[unitNo], -4, SEEK_CUR);
	len = (u32)fread(&recLen0, sizeof(recLen0), 1, mfr->activeDevice->fcb[unitNo]);
	fseek(mfr->activeDevice->fcb[unitNo], -4, SEEK_CUR);

	if (len != 1)
	{
		logError(LogErrorLocation, "channel %02o - missing tape record trailer", mfr->activeChannel->id);
		tp->alert = TRUE;
		tp->errorCode = EcDiagnosticError;
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
		tp->alert = TRUE;
		tp->errorCode = EcDiagnosticError;
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
		len = (u32)fread(&recLen2, sizeof(recLen2), 1, mfr->activeDevice->fcb[unitNo]);

		if (len != 1)
		{
			logError(LogErrorLocation, "channel %02o - missing TAP record header", mfr->activeChannel->id);
			tp->alert = TRUE;
			tp->errorCode = EcDiagnosticError;
			return;
		}

		if (recLen0 != recLen2)
		{
			/*
			**  This is more weird shit to deal with "padded" TAP records.
			*/
			position -= 1;
			fseek(mfr->activeDevice->fcb[unitNo], position, SEEK_SET);
			len = (u32)fread(&recLen2, sizeof(recLen2), 1, mfr->activeDevice->fcb[unitNo]);

			if (len != 1 || recLen0 != recLen2)
			{
				logError(LogErrorLocation, "channel %02o - invalid record length2: %d %08X != %08X", mfr->activeChannel->id, len, recLen0, recLen2);
				tp->alert = TRUE;
				tp->errorCode = EcDiagnosticError;
				return;
			}
		}

		/*
		**  Read and verify the actual raw data.
		*/
		len = (u32)fread(rawBuffer, 1, recLen1, mfr->activeDevice->fcb[unitNo]);

		if (recLen1 != (u32)len)
		{
			logError(LogErrorLocation, "channel %02o - short tape record read: %d", mfr->activeChannel->id, len);
			tp->alert = TRUE;
			tp->errorCode = EcDiagnosticError;
			return;
		}

		/*
		**  Position to the TAP record header.
		*/
		fseek(mfr->activeDevice->fcb[unitNo], position, SEEK_SET);

		/*
		**  Convert the raw data into PP words suitable for a channel.
		*/
		mt679PackAndConvert(recLen1, mfrId);

		/*
		**  Setup length and buffer pointer.
		*/
#if DEBUG
		fprintf(mt679Log, "Read bkwd %d bytes\n", activeDevice->recordLength);
#endif

		tp->recordLength = mfr->activeDevice->recordLength;
		tp->bp = tp->ioBuffer + tp->recordLength - 1;
	}
	else
	{
		/*
		**  A tape mark consists of only a single TAP record header of zero.
		*/
		tp->fileMark = TRUE;

#if DEBUG
		fprintf(mt679Log, "Tape mark\n");
#endif
	}

	/*
	**  Set block number.
	*/
	if (position == 0)
	{
		tp->suppressBot = TRUE;
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
static void mt679FuncForespace(u8 mfrId)
{
	u32 len;
	u32 recLen0;
	u32 recLen1;
	u32 recLen2;
	i8 unitNo;
	TapeParam *tp;
	i32 position;

	MMainFrame *mfr = BigIron->chasis[mfrId];

	unitNo = mfr->activeDevice->selectedUnit;
	tp = (TapeParam *)mfr->activeDevice->context[unitNo];

	/*
	**  Determine if the tape is at the load point.
	*/
	position = ftell(mfr->activeDevice->fcb[unitNo]);

	/*
	**  Read and verify TAP record length header.
	*/
	len = (u32)fread(&recLen0, sizeof(recLen0), 1, mfr->activeDevice->fcb[unitNo]);

	if (len != 1)
	{
		if (position == 0)
		{
			tp->errorCode = EcBlankTape;
		}
		else
		{
			//            tp->endOfTape = TRUE;
			tp->fileMark = TRUE;
#if DEBUG
			fprintf(mt679Log, "TAP is at EOF (simulate tape mark)\n");
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
		tp->alert = TRUE;
		tp->errorCode = EcDiagnosticError;
		return;
	}

	if (recLen1 == 0)
	{
		/*
		**  Report a tape mark and return.
		*/
		tp->fileMark = TRUE;
		tp->blockNo += 1;

#if DEBUG
		fprintf(mt679Log, "Tape mark\n");
#endif
		return;
	}

	/*
	**  Skip the actual raw data.
	*/
	if (fseek(mfr->activeDevice->fcb[unitNo], recLen1, SEEK_CUR) != 0)
	{
		logError(LogErrorLocation, "channel %02o - short tape record read: %d", mfr->activeChannel->id, len);
		tp->alert = TRUE;
		tp->errorCode = EcDiagnosticError;
		return;
	}

	/*
	**  Read and verify the TAP record length trailer.
	*/
	len = (u32)fread(&recLen2, sizeof(recLen2), 1, mfr->activeDevice->fcb[unitNo]);

	if (len != 1)
	{
		logError(LogErrorLocation, "channel %02o - missing tape record trailer", mfr->activeChannel->id);
		tp->alert = TRUE;
		tp->errorCode = EcDiagnosticError;
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
			tp->alert = TRUE;
			tp->errorCode = EcDiagnosticError;
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
static void mt679FuncBackspace(u8 mfrId)
{
	u32 len;
	u32 recLen0;
	u32 recLen1;
	u32 recLen2;
	i8 unitNo;
	TapeParam *tp;
	i32 position;

	MMainFrame *mfr = BigIron->chasis[mfrId];

	unitNo = mfr->activeDevice->selectedUnit;
	tp = (TapeParam *)mfr->activeDevice->context[unitNo];

	/*
	**  Check if we are already at the beginning of the tape.
	*/
	position = ftell(mfr->activeDevice->fcb[unitNo]);
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
	len = (u32)fread(&recLen0, sizeof(recLen0), 1, mfr->activeDevice->fcb[unitNo]);
	fseek(mfr->activeDevice->fcb[unitNo], -4, SEEK_CUR);

	if (len != 1)
	{
		logError(LogErrorLocation, "channel %02o - missing tape record trailer", mfr->activeChannel->id);
		tp->alert = TRUE;
		tp->errorCode = EcDiagnosticError;
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
		tp->alert = TRUE;
		tp->errorCode = EcDiagnosticError;
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
		len = (u32)fread(&recLen2, sizeof(recLen2), 1, mfr->activeDevice->fcb[unitNo]);

		if (len != 1)
		{
			logError(LogErrorLocation, "channel %02o - missing TAP record header", mfr->activeChannel->id);
			tp->alert = TRUE;
			tp->errorCode = EcDiagnosticError;
			return;
		}

		if (recLen0 != recLen2)
		{
			/*
			**  This is more weird shit to deal with "padded" TAP records.
			*/
			position -= 1;
			fseek(mfr->activeDevice->fcb[unitNo], position, SEEK_SET);
			len = (u32)fread(&recLen2, sizeof(recLen2), 1, mfr->activeDevice->fcb[unitNo]);

			if (len != 1 || recLen0 != recLen2)
			{
				logError(LogErrorLocation, "channel %02o - invalid record length2: %d %08X != %08X", mfr->activeChannel->id, len, recLen0, recLen2);
				tp->alert = TRUE;
				tp->errorCode = EcDiagnosticError;
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
		tp->fileMark = TRUE;

#if DEBUG
		fprintf(mt679Log, "Tape mark\n");
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
static char *mt679Func2String(PpWord funcCode)
{
	static char buf[30];
#if DEBUG
	switch (funcCode)
	{
	case Fc679ClearUnit: return "ClearUnit";
	case Fc679Release: return "Release";
	case Fc679FormatUnit: return "FormatUnit";
	case Fc679OppositeParity: return "OppositeParity";
	case Fc679OppositeDensity: return "OppositeDensity";
	case Fc679SetReadClipNorm: return "SetReadClipNorm";
	case Fc679SetReadClipHigh: return "SetReadClipHigh";
	case Fc679SetReadClipLow: return "SetReadClipLow";
	case Fc679SetReadClipHyper: return "SetReadClipHyper";
	case Fc679Rewind: return "Rewind";
	case Fc679RewindUnload: return "RewindUnload";
	case Fc679StopMotion: return "StopMotion";
	case Fc679GeneralStatus: return "GeneralStatus";
	case Fc679DetailedStatus: return "DetailedStatus";
	case Fc679UnitStatus: return "UnitStatus";
	case Fc679Forespace: return "Forespace";
	case Fc679Backspace: return "Backspace";
	case Fc679CtrledBackspace: return "CtrledBackspace";
	case Fc679SearchTapeMarkF: return "SearchTapeMarkF";
	case Fc679SearchTapeMarkB: return "SearchTapeMarkB";
	case Fc679Connect + 0: return "Connect + 0";
	case Fc679Connect + 1: return "Connect + 1";
	case Fc679Connect + 2: return "Connect + 2";
	case Fc679Connect + 3: return "Connect + 3";
	case Fc679Connect + 4: return "Connect + 4";
	case Fc679Connect + 5: return "Connect + 5";
	case Fc679Connect + 6: return "Connect + 6";
	case Fc679Connect + 7: return "Connect + 7";
	case Fc679Connect + 010: return "Connect + 010";
	case Fc679Connect + 011: return "Connect + 011";
	case Fc679Connect + 012: return "Connect + 012";
	case Fc679Connect + 013: return "Connect + 013";
	case Fc679Connect + 014: return "Connect + 014";
	case Fc679Connect + 015: return "Connect + 015";
	case Fc679Connect + 016: return "Connect + 016";
	case Fc679Connect + 017: return "Connect + 017";
	case Fc679WarmstartHighDens + 0: return "WarmstartHighDens + 0";
	case Fc679WarmstartHighDens + 1: return "WarmstartHighDens + 1";
	case Fc679WarmstartHighDens + 2: return "WarmstartHighDens + 2";
	case Fc679WarmstartHighDens + 3: return "WarmstartHighDens + 3";
	case Fc679WarmstartHighDens + 4: return "WarmstartHighDens + 4";
	case Fc679WarmstartHighDens + 5: return "WarmstartHighDens + 5";
	case Fc679WarmstartHighDens + 6: return "WarmstartHighDens + 6";
	case Fc679WarmstartHighDens + 7: return "WarmstartHighDens + 7";
	case Fc679WarmstartHighDens + 010: return "WarmstartHighDens + 010";
	case Fc679WarmstartHighDens + 011: return "WarmstartHighDens + 011";
	case Fc679WarmstartHighDens + 012: return "WarmstartHighDens + 012";
	case Fc679WarmstartHighDens + 013: return "WarmstartHighDens + 013";
	case Fc679WarmstartHighDens + 014: return "WarmstartHighDens + 014";
	case Fc679WarmstartHighDens + 015: return "WarmstartHighDens + 015";
	case Fc679WarmstartHighDens + 016: return "WarmstartHighDens + 016";
	case Fc679WarmstartHighDens + 017: return "WarmstartHighDens + 017";
	case Fc679WarmstartLowDens + 0: return "WarmstartLowDens + 0";
	case Fc679WarmstartLowDens + 1: return "WarmstartLowDens + 1";
	case Fc679WarmstartLowDens + 2: return "WarmstartLowDens + 2";
	case Fc679WarmstartLowDens + 3: return "WarmstartLowDens + 3";
	case Fc679WarmstartLowDens + 4: return "WarmstartLowDens + 4";
	case Fc679WarmstartLowDens + 5: return "WarmstartLowDens + 5";
	case Fc679WarmstartLowDens + 6: return "WarmstartLowDens + 6";
	case Fc679WarmstartLowDens + 7: return "WarmstartLowDens + 7";
	case Fc679WarmstartLowDens + 010: return "WarmstartLowDens + 010";
	case Fc679WarmstartLowDens + 011: return "WarmstartLowDens + 011";
	case Fc679WarmstartLowDens + 012: return "WarmstartLowDens + 012";
	case Fc679WarmstartLowDens + 013: return "WarmstartLowDens + 013";
	case Fc679WarmstartLowDens + 014: return "WarmstartLowDens + 014";
	case Fc679WarmstartLowDens + 015: return "WarmstartLowDens + 015";
	case Fc679WarmstartLowDens + 016: return "WarmstartLowDens + 016";
	case Fc679WarmstartLowDens + 017: return "WarmstartLowDens + 017";
	case Fc679ReadFwd: return "ReadFwd";
	case Fc679ReadBkw: return "ReadBkw";
	case Fc679CopyReadConv: return "CopyReadConv";
	case Fc679CopyWriteConv: return "CopyWriteConv";
	case Fc679Write: return "Write";
	case Fc679WriteShort: return "WriteShort";
	case Fc679WriteTapeMark: return "WriteTapeMark";
	case Fc679Erase: return "Erase";
	case Fc679EraseDataSecurity: return "EraseDataSecurity";
	case Fc679LoadReadConv: return "LoadReadConv";
	case Fc679LoadWriteConv: return "LoadWriteConv";
	case Fc679RewindOnEOT: return "RewindOnEOT";
	case Fc679WaitForStop: return "WaitForStop";
	case Fc679TestVelocityVect: return "TestVelocityVect";
	case Fc679MeasureGapSizeFwd: return "MeasureGapSizeFwd";
	case Fc679MeasureGapSizeBkw: return "MeasureGapSizeBkw";
	case Fc679MeasureStartTFwd: return "MeasureStartTFwd";
	case Fc679SetTransferCheckCh: return "SetTransferCheckCh";
	case Fc679SetLoopWTRTcu: return "SetLoopWTRTcu";
	case Fc679SetLoopWTR1TU: return "SetLoopWTR1TU";
	case Fc679SetLoopWTR2TU: return "SetLoopWTR2TU";
	case Fc679SetEvenWrParity: return "SetEvenWrParity";
	case Fc679SetEvenChParity: return "SetEvenChParity";
	case Fc679ForceDataErrors: return "ForceDataErrors";
	case Fc679MasterClear: return "MasterClear";
	}
#endif
	sprintf(buf, "UNKNOWN: %04o", funcCode);
	return(buf);
}

/*---------------------------  End Of File  ------------------------------*/

