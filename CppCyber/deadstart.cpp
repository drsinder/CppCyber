/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2011, Tom Hunter
**  C++ adaptation by Dale Sinder 2017
**
**  Name: deadstart.cpp
**
**  Description:
**      Perform emulation of CDC 6600 deadstart.
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

/*
**  -----------------
**  Private Constants
**  -----------------
*/

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
static FcStatus deadFunc(PpWord funcCode, u8 mfrId);
static void deadIo(u8 mfrId);
static void deadActivate(u8 mfrId);
static void deadDisconnect(u8 mfrId);

#if MaxMainFrames > 1
static void deadIo1(u8 mfrId);
#endif

#if MaxMainFrames > 2
static void deadIo2(u8 mfrId);
#endif
#if MaxMainFrames > 3
static void deadIo3(u8 mfrId);
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
static u8 dsSequence = 0;       /* deadstart sequencer */
static u8 dsSequence1 = 0;       /* deadstart sequencer */
static u8 dsSequence2 = 0;       /* deadstart sequencer */
static u8 dsSequence3 = 0;       /* deadstart sequencer */

/*
**--------------------------------------------------------------------------
**
**  Public Functions
**
**--------------------------------------------------------------------------
*/
/*--------------------------------------------------------------------------
**  Purpose:        Execute deadstart function.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void deadStart(u8 k)
{
	MMainFrame *mfr = BigIron->chasis[k];

	DevSlot *dp = channelAttach(0, 0, DtDeadStartPanel, k);

	dp->activate = deadActivate;
	dp->disconnect = deadDisconnect;
	dp->func = deadFunc;
#if MaxMainFrames > 1

	if (k == 0)
		dp->io = deadIo;
	if (k == 1)
		dp->io = deadIo1;
#if MaxMainFrames > 2
	if (k == 2)
		dp->io = deadIo2;
#endif
#if MaxMainFrames > 3
	if (k == 3)
		dp->io = deadIo3;
#endif
#else
	dp->io = deadIo;
#endif
	dp->selectedUnit = 0;

	/*
	**  Set all normal channels to active and empty.
	*/
	for (u8 ch = 0; ch < mfr->channelCount; ch++)
	{
		if (ch <= 013 || (ch >= 020 && ch <= 033))
		{
			mfr->channel[ch].active = true;
		}
	}

	/*
	**  Set special channels appropriately.
	*/
	mfr->channel[ChInterlock].active = (features & HasInterlockReg) != 0;
	mfr->channel[ChMaintenance].active = false;

	/*
	**  Reset deadstart sequencer.
	*/
	dsSequence = 0;

	for (u8 pp = 0; pp < BigIron->pps; pp++)
	{
		/*
		**  Assign PPs to the corresponding channels.
		*/
		if (pp < 012)
		{
			mfr->ppBarrel[pp]->ppu.opD = pp;
			mfr->channel[pp].active = true;
		}
		else
		{
			mfr->ppBarrel[pp]->ppu.opD = pp - 012 + 020;
			mfr->channel[pp - 012 + 020].active = true;
		}

		/*
		**  Set all PPs to INPUT (71) instruction.
		*/
		mfr->ppBarrel[pp]->ppu.opF = 071;
		mfr->ppBarrel[pp]->ppu.busy = true;

		/*
		**  Clear P registers and location zero of each PP.
		*/
		mfr->ppBarrel[pp]->ppu.regP = 0;
		mfr->ppBarrel[pp]->ppu.mem[0] = 0;

		/*
		**  Set all A registers to an input word count of 10000.
		*/
		mfr->ppBarrel[pp]->ppu.regA = 010000;
	}
	/*
	**  Start load of PPU0.
	*/
	mfr->channel[0].ioDevice = dp;
	mfr->channel[0].active = true;
	mfr->channel[0].full = true;
	mfr->channel[0].data = 0;
}

/*--------------------------------------------------------------------------
**  Purpose:        Execute function code on deadstart pseudo device.
**
**  Parameters:     Name        Description.
**                  funcCode    function code
**
**  Returns:        FcStatus
**
**------------------------------------------------------------------------*/
static FcStatus deadFunc(PpWord funcCode, u8 mfrId)
{
	(void)funcCode;

	return(FcDeclined);
}

/*--------------------------------------------------------------------------
**  Purpose:        Perform I/O on deadstart panel.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void deadIo(u8 mfrId)
{
	MMainFrame *mfr = BigIron->chasis[mfrId];

	if (!mfr->activeChannel->full)
	{
		if (dsSequence == mfr->deadstartCount)
		{
			mfr->activeChannel->active = false;
		}
		else
		{
			mfr->activeChannel->data = mfr->deadstartPanel[dsSequence++] & Mask12;
			mfr->activeChannel->full = true;
			//printf("\ndeadIo on mfr %d data %4o # %d", activeChannel->mfrID, activeChannel->data, dsSequence-1);
		}
	}
}

#if MaxMainFrames > 1
static void deadIo1(u8 mfrId)
{
	MMainFrame *mfr = BigIron->chasis[mfrId];

	//printf("deadIo on mfr %d\n", activeChannel->mfrID);
	if (!mfr->activeChannel->full)
	{
		if (dsSequence1 == mfr->deadstartCount)
		{
			mfr->activeChannel->active = false;
		}
		else
		{
			mfr->activeChannel->data = mfr->deadstartPanel[dsSequence1++] & Mask12;
			mfr->activeChannel->full = true;
			//printf("\ndeadIo1 on mfr %d data %4o # %d", activeChannel->mfrID, activeChannel->data, dsSequence1-1);
		}
	}
}
#endif

#if MaxMainFrames > 2
static void deadIo2(u8 mfrId)
{
	MMainFrame *mfr = BigIron->chasis[mfrId];

	//printf("deadIo on mfr %d\n", activeChannel->mfrID);
	if (!mfr->activeChannel->full)
	{
		if (dsSequence2 == mfr->deadstartCount)
		{
			mfr->activeChannel->active = false;
		}
		else
		{
			mfr->activeChannel->data = mfr->deadstartPanel[dsSequence2++] & Mask12;
			mfr->activeChannel->full = true;
			//printf("\ndeadIo1 on mfr %d data %4o # %d", activeChannel->mfrID, activeChannel->data, dsSequence1-1);
		}
	}
}
#endif
#if MaxMainFrames > 3
static void deadIo3(u8 mfrId)
{
	MMainFrame *mfr = BigIron->chasis[mfrId];

	//printf("deadIo on mfr %d\n", activeChannel->mfrID);
	if (!mfr->activeChannel->full)
	{
		if (dsSequence3 == mfr->deadstartCount)
		{
			mfr->activeChannel->active = false;
		}
		else
		{
			mfr->activeChannel->data = mfr->deadstartPanel[dsSequence3++] & Mask12;
			mfr->activeChannel->full = true;
			//printf("\ndeadIo1 on mfr %d data %4o # %d", activeChannel->mfrID, activeChannel->data, dsSequence1-1);
		}
	}
}
#endif

/*--------------------------------------------------------------------------
**  Purpose:        Handle channel activation.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void deadActivate(u8 mfrId)
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
static void deadDisconnect(u8 mfrId)
{
}

/*---------------------------  End Of File  ------------------------------*/
