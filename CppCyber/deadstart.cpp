/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2011, Tom Hunter
**  C++ adaptation by Dale Sinder 2017
**
**  Name: deadstart.c
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
static FcStatus deadFunc(PpWord funcCode);
static void deadIo(void);
static void deadActivate(void);
static void deadDisconnect(void);

#if MaxMainFrames == 2
static void deadIo1(void);
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
	DevSlot *dp;
	u8 pp;
	u8 ch;

	MMainFrame *mfr = BigIron->chasis[k];

	dp = channelAttach(0, 0, DtDeadStartPanel, k);

	dp->activate = deadActivate;
	dp->disconnect = deadDisconnect;
	dp->func = deadFunc;
#if MaxMainFrames == 2
	dp->io = k == 0 ? deadIo : deadIo1;
#else
	dp->io = deadIo;
#endif
	dp->selectedUnit = 0;

	/*
	**  Set all normal channels to active and empty.
	*/
	for (ch = 0; ch < mfr->channelCount; ch++)
	{
		if (ch <= 013 || (ch >= 020 && ch <= 033))
		{
			mfr->channel[ch].active = TRUE;
		}
	}

	/*
	**  Set special channels appropriately.
	*/
	mfr->channel[ChInterlock].active = (features & HasInterlockReg) != 0;
	mfr->channel[ChMaintenance].active = FALSE;

	/*
	**  Reset deadstart sequencer.
	*/
	dsSequence = 0;

	for (pp = 0; pp < BigIron->pps; pp++)
	{
		/*
		**  Assign PPs to the corresponding channels.
		*/
		if (pp < 012)
		{
			mfr->ppBarrel[pp]->ppu.opD = pp;
			mfr->channel[pp].active = TRUE;
		}
		else
		{
			mfr->ppBarrel[pp]->ppu.opD = pp - 012 + 020;
			mfr->channel[pp - 012 + 020].active = TRUE;
		}

		/*
		**  Set all PPs to INPUT (71) instruction.
		*/
		mfr->ppBarrel[pp]->ppu.opF = 071;
		mfr->ppBarrel[pp]->ppu.busy = TRUE;

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
	mfr->channel[0].active = TRUE;
	mfr->channel[0].full = TRUE;
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
static FcStatus deadFunc(PpWord funcCode)
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
static void deadIo(void)
{
	
	if (!activeChannel->full)
	{
		if (dsSequence == BigIron->deadstartCount)
		{
			activeChannel->active = FALSE;
		}
		else
		{
			activeChannel->data = BigIron->deadstartPanel[dsSequence++] & Mask12;
			activeChannel->full = TRUE;
			//printf("\ndeadIo on mfr %d data %4o # %d", activeChannel->mfrID, activeChannel->data, dsSequence-1);
		}
	}
}

#if MaxMainFrames == 2
static void deadIo1(void)
{
	//printf("deadIo on mfr %d\n", activeChannel->mfrID);
	if (!activeChannel->full)
	{
		if (dsSequence1 == BigIron->deadstartCount)
		{
			activeChannel->active = FALSE;
		}
		else
		{
			activeChannel->data = BigIron->deadstartPanel[dsSequence1++] & Mask12;
			activeChannel->full = TRUE;
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
static void deadActivate(void)
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
static void deadDisconnect(void)
{
}

/*---------------------------  End Of File  ------------------------------*/
