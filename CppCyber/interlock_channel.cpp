/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2011, Tom Hunter
**  C++ adaptation by Dale Sinder 2017
**
**  Name: interlock_channel.cpp
**
**  Description:
**      Perform emulation of interlock register on channel 15.
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
#define InterlockWords          11

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
static FcStatus ilrFunc(PpWord funcCode, u8 mfrId);
static void ilrIo(u8 mfrId);
static void ilrActivate(u8 mfrId);
static void ilrDisconnect(u8 mfrId);
static void ilrExecute(PpWord func, u8 mfrId);

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
static u8 ilrBits;
static u8 ilrWords;

#if DEBUG
static FILE *ilrLog = NULL;
#endif

/*
**--------------------------------------------------------------------------
**
**  Public Functions
**
**--------------------------------------------------------------------------
*/
/*--------------------------------------------------------------------------
**  Purpose:        Initialise interlock register channel.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void ilrInit(u8 registerSize, u8 mfrID)
{
#if DEBUG
	if (ilrLog == NULL)
	{
		ilrLog = fopen("ilrlog.txt", "wt");
	}
#endif

	DevSlot *dp = channelAttach(ChInterlock, 0, DtInterlockRegister, mfrID);
	dp->activate = ilrActivate;
	dp->disconnect = ilrDisconnect;
	dp->func = ilrFunc;
	dp->io = ilrIo;

	dp->mfr->channel[ChInterlock].active = true;
	dp->mfr->channel[ChInterlock].ioDevice = dp;
	dp->mfr->channel[ChInterlock].hardwired = true;

	ilrBits = registerSize;
	ilrWords = (ilrBits + 11) / 12;

	/*
	**  Print a friendly message.
	*/
	printf("Interlock Register initialised on channel %o\n", ChInterlock);
}

/*--------------------------------------------------------------------------
**  Purpose:        Execute function code on interlock register channel.
**
**  Parameters:     Name        Description.
**                  funcCode    function code
**
**  Returns:        FcStatus
**
**------------------------------------------------------------------------*/
static FcStatus ilrFunc(PpWord funcCode, u8 mfrId)
{
	return(FcAccepted);
}

/*--------------------------------------------------------------------------
**  Purpose:        Perform I/O on interlock register channel.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void ilrIo(u8 mfrId)
{
	/*
	**  This function relies on pp.c only calling it when doing the OAN. The
	**  IAN will not block as the response to the Interlock function request
	**  is made available immediately (i.e. the channel is full).
	*/
	MMainFrame *mfr = BigIron->chasis[mfrId];

	if (!mfr->activeChannel->inputPending && mfr->activeChannel->full)
	{
		mfr->activeChannel->inputPending = true;
		ilrExecute(mfr->activeChannel->data, mfrId);
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
static void ilrActivate(u8 mfrId)
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
static void ilrDisconnect(u8 mfrId)
{
}

/*--------------------------------------------------------------------------
**  Purpose:        Execute interlock register request.
**
**  Parameters:     Name        Description.
**                  func        function word
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void ilrExecute(PpWord func, u8 mfrId)
{
	static PpWord interlockRegister[InterlockWords] = { 0 };
	u8 word;
	u8 bit;
	MMainFrame *mfr = BigIron->chasis[mfrId];

	u8 code = (func >> 9) & 7;
	u8 designator = func & 0177;

	// ReSharper disable once CppDefaultCaseNotHandledInSwitchStatement
	switch (code)
	{
	case 0:
		/*
		**  Read word.
		*/
		if (designator < ilrWords)
		{
			mfr->activeChannel->data = interlockRegister[designator] & Mask12;
		}
		else
		{
			mfr->activeChannel->data = 0;
		}

		break;

	case 1:
		/*
		**  Test bit.
		*/
		if (designator < ilrBits)
		{
			word = designator / 12;
			bit = designator % 12;

			mfr->activeChannel->data = (interlockRegister[word] & (1 << bit)) != 0 ? 1 : 0;
		}
		else
		{
			mfr->activeChannel->data = 0;
		}

		break;

	case 2:
		/*
		**  Clear bit.
		*/
		if (designator < ilrBits)
		{
			word = designator / 12;
			bit = designator % 12;

			interlockRegister[word] &= ~(1 << bit);
		}

		mfr->activeChannel->data = 0;
		break;

	case 3:
		/*
		**  Test bit and leave clear.
		*/
		if (designator < ilrBits)
		{
			word = designator / 12;
			bit = designator % 12;

			mfr->activeChannel->data = (interlockRegister[word] & (1 << bit)) != 0 ? 1 : 0;
			interlockRegister[word] &= ~(1 << bit);
		}
		else
		{
			mfr->activeChannel->data = 0;
		}

		break;

	case 4:
		/*
		**  Set bit.
		*/
		if (designator < ilrBits)
		{
			word = designator / 12;
			bit = designator % 12;

			interlockRegister[word] |= (1 << bit);
		}

		mfr->activeChannel->data = 0;
		break;

	case 5:
		/*
		**  Test bit and leave set.
		*/
		if (designator < ilrBits)
		{
			word = designator / 12;
			bit = designator % 12;

			mfr->activeChannel->data = (interlockRegister[word] & (1 << bit)) != 0 ? 1 : 0;
			interlockRegister[word] |= (1 << bit);
		}
		else
		{
			mfr->activeChannel->data = 0;
		}

		break;

	case 6:
		/*
		**  Clear all bits.
		*/
		for (word = 0; word < ilrWords; word++)
		{
			interlockRegister[word] = 0;
		}

		mfr->activeChannel->data = 0;
		break;

	case 7:
		/*
		**  Test all bits and return one if any set.
		*/
		mfr->activeChannel->data = 0;
		for (word = 0; word < ilrWords; word++)
		{
			if (interlockRegister[word] != 0)
			{
				mfr->activeChannel->data = 1;
				break;
			}
		}

		break;
	}

	mfr->activeChannel->full = true;

#if DEBUG
	{
		static char *codeString[] =
		{
			"read word",
			"test bit",
			"clear bit",
			"test & clear bit",
			"set bit",
			"test & set bit",
			"clear all",
			"test all"
		};

		fprintf(ilrLog, "%06d Interlock Reg: bit %03o %s result: %04o\n", traceSequenceNo, designator, codeString[code], activeChannel->data);
	}
#endif
}

/*---------------------------  End Of File  ------------------------------*/
