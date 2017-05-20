/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2011, Tom Hunter
**  C++ adaptation by Dale Sinder 2017
**
**  Name: console.c
**
**  Description:
**      Perform emulation of CDC 6612 or CC545 console.
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
**  CDC 6612 console functions and status codes.
*/
#define Fc6612Sel64CharLeft     07000
#define Fc6612Sel32CharLeft     07001
#define Fc6612Sel16CharLeft     07002

#define Fc6612Sel512DotsLeft    07010
#define Fc6612Sel512DotsRight   07110
#define Fc6612SelKeyIn          07020

#define Fc6612Sel64CharRight    07100
#define Fc6612Sel32CharRight    07101
#define Fc6612Sel16CharRight    07102


#define KeyBufSize              50      /* Input buffer size */



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
static FcStatus consoleFunc(PpWord funcCode, u8 mfrId);
static void consoleIo(u8 mfrId);
static void consoleActivate(u8 mfrId);
static void consoleDisconnect(u8 mfrId);

#if MaxMainFrames == 2
static FcStatus consoleFunc1(PpWord funcCode, u8 mfrId);
static void consoleIo1(u8 mfrId);
static void consoleActivate1(u8 mfrId);
static void consoleDisconnect1(u8 mfrId);
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
static u8 currentFont;
static u16 currentOffset;
static bool emptyDrop = FALSE;


/* Ring buffer for keyboard input */
static u8 keyRing[KeyBufSize];
static u32 keyIn, keyOut;

static char ts[40];
time_t t;
struct tm tmbuf;
static int autoPos;
u8 *p;

#if MaxMainFrames == 2
static u8 currentFont1;
static u16 currentOffset1;
static bool emptyDrop1 = FALSE;

static u8 keyRing1[KeyBufSize];
static u32 keyIn1, keyOut1;

static char ts1[40];
time_t t1;
struct tm tmbuf1;
static int autoPos1;
u8 *p1;
#endif
/*
**--------------------------------------------------------------------------
**
**  Public Functions
**
**--------------------------------------------------------------------------
*/

/*--------------------------------------------------------------------------
**  Purpose:        Initialise 6612 console.
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
void consoleInit(u8 mfrID, u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName)
{
	DevSlot *dp;

	(void)eqNo;
	(void)unitNo;
	(void)deviceName;

	keyIn = keyOut = 0;

	dp = channelAttach(channelNo, eqNo, DtConsole, mfrID);

	if (mfrID == 0)
	{
		dp->activate = consoleActivate;
		dp->disconnect = consoleDisconnect;
		dp->selectedUnit = unitNo;
		dp->func = consoleFunc;
		dp->io = consoleIo;
	}
#if MaxMainFrames == 2
	else
	{
		dp->activate = consoleActivate1;
		dp->disconnect = consoleDisconnect1;
		dp->selectedUnit = unitNo;
		dp->func = consoleFunc1;
		dp->io = consoleIo1;
	}
#endif
	/*
	**  Initialise (X)Windows environment.
	*/
	windowInit(mfrID);

	/*
	**  Print a friendly message.
	*/
	printf("Console initialised on channel %o for mainframe %o\n", channelNo, mfrID);
}

/*--------------------------------------------------------------------------
**  Purpose:        Execute function code on 6612 console.
**
**  Parameters:     Name        Description.
**                  funcCode    function code
**
**  Returns:        FcStatus
**
**------------------------------------------------------------------------*/
static FcStatus consoleFunc(PpWord funcCode, u8 mfrId)
{
	MMainFrame *mfr = BigIron->chasis[mfrId];

	if (mfr->activeChannel->mfrID != 0)
		printf("consoleFunc mfrID = %d\n", mfr->activeChannel->mfrID);

	mfr->activeChannel->full = FALSE;

	switch (funcCode)
	{
	default:
		return(FcDeclined);

	case Fc6612Sel512DotsLeft:
		currentFont = FontDot;
		currentOffset = OffLeftScreen;
		windowSetFont(currentFont);
		break;

	case Fc6612Sel512DotsRight:
		currentFont = FontDot;
		currentOffset = OffRightScreen;
		windowSetFont(currentFont);
		break;

	case Fc6612Sel64CharLeft:
		currentFont = FontSmall;
		currentOffset = OffLeftScreen;
		windowSetFont(currentFont);
		break;

	case Fc6612Sel32CharLeft:
		currentFont = FontMedium;
		currentOffset = OffLeftScreen;
		windowSetFont(currentFont);
		break;

	case Fc6612Sel16CharLeft:
		currentFont = FontLarge;
		currentOffset = OffLeftScreen;
		windowSetFont(currentFont);
		break;

	case Fc6612Sel64CharRight:
		currentFont = FontSmall;
		currentOffset = OffRightScreen;
		windowSetFont(currentFont);
		break;

	case Fc6612Sel32CharRight:
		currentFont = FontMedium;
		currentOffset = OffRightScreen;
		windowSetFont(currentFont);
		break;

	case Fc6612Sel16CharRight:
		currentFont = FontLarge;
		currentOffset = OffRightScreen;
		windowSetFont(currentFont);
		break;

	case Fc6612SelKeyIn:
		break;
	}

	mfr->activeDevice->fcode = funcCode;

	return(FcAccepted);
}
#if MaxMainFrames == 2
static FcStatus consoleFunc1(PpWord funcCode, u8 mfrId)
{
	MMainFrame *mfr = BigIron->chasis[mfrId];

	if (mfr->activeChannel->mfrID !=1)
		printf("consoleFunc1 mfrID = %d\n", mfr->activeChannel->mfrID);

	mfr->activeChannel->full = FALSE;

	switch (funcCode)
	{
	default:
		return(FcDeclined);

	case Fc6612Sel512DotsLeft:
		currentFont1 = FontDot;
		currentOffset1 = OffLeftScreen;
		windowSetFont1(currentFont1);
		break;

	case Fc6612Sel512DotsRight:
		currentFont1 = FontDot;
		currentOffset1 = OffRightScreen;
		windowSetFont1(currentFont1);
		break;

	case Fc6612Sel64CharLeft:
		currentFont1 = FontSmall;
		currentOffset1 = OffLeftScreen;
		windowSetFont1(currentFont1);
		break;

	case Fc6612Sel32CharLeft:
		currentFont1 = FontMedium;
		currentOffset1 = OffLeftScreen;
		windowSetFont1(currentFont1);
		break;

	case Fc6612Sel16CharLeft:
		currentFont1 = FontLarge;
		currentOffset1 = OffLeftScreen;
		windowSetFont1(currentFont1);
		break;

	case Fc6612Sel64CharRight:
		currentFont1 = FontSmall;
		currentOffset1 = OffRightScreen;
		windowSetFont1(currentFont1);
		break;

	case Fc6612Sel32CharRight:
		currentFont1 = FontMedium;
		currentOffset1 = OffRightScreen;
		windowSetFont1(currentFont1);
		break;

	case Fc6612Sel16CharRight:
		currentFont1 = FontLarge;
		currentOffset1 = OffRightScreen;
		windowSetFont1(currentFont1);
		break;

	case Fc6612SelKeyIn:
		break;
	}

	mfr->activeDevice->fcode = funcCode;

	return(FcAccepted);
}
#endif
/*--------------------------------------------------------------------------
**  Purpose:        Perform I/O on 6612 console.
**
**  Parameters:     Name        Description.
**                  device      Device control block
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/

char autoDateString[40];
char autoDateYear[40] = "98";
/*--------------------------------------------------------------------------
**  Purpose:        Queue keyboard input.
**
**  Parameters:     Name        Description.
**                  ch          character to be queued (display code)
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/

bool autoDate = false;		// enter date/time automatically - year 98
bool autoDate1 = false;		// enter date/time automatically - year 98 MF1

void consoleQueueKey(char ch)
{
	int nextin;

	nextin = keyIn + 1;
	if (nextin == KeyBufSize)
	{
		nextin = 0;
	}
	if (nextin != keyOut)
	{
		keyRing[keyIn] = ch;
		keyIn = nextin;
	}
}

/*--------------------------------------------------------------------------
**  Purpose:        Get next keycode from buffer
**
**  Parameters:     Name        Description.
**
**  Returns:        keycode or 0 if nothing pending.
**                  keycode has 0200 bit set for key-up
**
**------------------------------------------------------------------------*/
static u64 keyloops = 0;
char consoleGetKey(void)
{
	int nextout;
	char key;

	if (keyIn == keyOut)
		return 0;
	if ((++keyloops % 3L) != 1)
		return 0;
	nextout = keyOut + 1;
	if (nextout == KeyBufSize)
	{
		nextout = 0;
	}
	key = keyRing[keyOut];
	keyOut = nextout;
	//printf("keyout %c\n", consoleToAscii[key]);
	return key;
}

#if MaxMainFrames == 2
/*--------------------------------------------------------------------------
**  Purpose:        Queue keyboard input.
**
**  Parameters:     Name        Description.
**                  ch          character to be queued (display code)
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/

void consoleQueueKey1(char ch)
{
	int nextin;

	nextin = keyIn1 + 1;
	if (nextin == KeyBufSize)
	{
		nextin = 0;
	}
	if (nextin != keyOut1)
	{
		keyRing1[keyIn1] = ch;
		keyIn1 = nextin;
	}
}

/*--------------------------------------------------------------------------
**  Purpose:        Get next keycode from buffer
**
**  Parameters:     Name        Description.
**
**  Returns:        keycode or 0 if nothing pending.
**                  keycode has 0200 bit set for key-up
**
**------------------------------------------------------------------------*/
static u64 keyloops1 = 0;
char consoleGetKey1(void)
{
	int nextout;
	char key;

	if (keyIn1 == keyOut1)
		return 0;
	if ((++keyloops1 % 3L) != 1)
		return 0;
	nextout = keyOut1 + 1;
	if (nextout == KeyBufSize)
	{
		nextout = 0;
	}
	key = keyRing1[keyOut1];
	keyOut1 = nextout;
	//printf("keyout %c\n", consoleToAscii[key]);
	return key;
}

#endif
static void consoleIo(u8 mfrId)
{
	u8 ch;

	MMainFrame *mfr = BigIron->chasis[mfrId];

	if (mfr->activeDevice->mfrID == 1)
		printf("consoleIo mfrID = %d\n", mfr->activeDevice->mfrID);

	switch (mfr->activeDevice->fcode)
	{
	default:
		break;

	case Fc6612Sel64CharLeft:
	case Fc6612Sel32CharLeft:
	case Fc6612Sel16CharLeft:
	case Fc6612Sel64CharRight:
	case Fc6612Sel32CharRight:
	case Fc6612Sel16CharRight:
		if (mfr->activeChannel->full)
		{
			emptyDrop = FALSE;

			ch = (u8)((mfr->activeChannel->data >> 6) & Mask6);

			if (ch >= 060)
			{
				if (ch >= 070)
				{
					/*
					**  Vertical coordinate.
					*/
					windowSetY((u16)(mfr->activeChannel->data & Mask9));
				}
				else
				{
					/*
					**  Horizontal coordinate.
					*/
					windowSetX((u16)((mfr->activeChannel->data & Mask9) + currentOffset));
				}
			}
			else
			{
				windowQueue(consoleToAscii[(mfr->activeChannel->data >> 6) & Mask6]);
				windowQueue(consoleToAscii[(mfr->activeChannel->data >> 0) & Mask6]);
			}

			/*
			**  Check for auto date entry.
			*/
			if (autoDate)
			{
				/*
				**  See if medium char size, and text matches
				**  next word of "enter date" message.
				*/
				if ((mfr->activeDevice->fcode == Fc6612Sel32CharLeft ||
					mfr->activeDevice->fcode == Fc6612Sel32CharRight) &&
					((mfr->activeChannel->data >> 6) & Mask6) == asciiToCdc[(u8)autoDateString[autoPos]] &&
					(mfr->activeChannel->data & Mask6) == asciiToCdc[(u8)autoDateString[autoPos + 1]])
				{
					/*
					**  It matches so far.  Let's see if we're done.
					*/
					if (autoDateString[autoPos + 1] == 0 ||
						autoDateString[autoPos + 2] == 0)
					{
						/*
						**  Entire pattern matched, supply
						**  auto date and time, provided that
						**  there is no typeahead, and keyboard
						**  is in "easy" mode.
						*/
						autoDate = FALSE;
						if (keyOut == keyIn ) // && !keyboardTrue)
						{
							time(&t);
							/* Note that DSD supplies punctuation */
							strftime(ts, sizeof(ts) - 1,
								"%y%m%d\n%H%M%S\n",
								localtime(&t));
							*ts = autoDateYear[0]; *(ts+1) = autoDateYear[1];
							for (p = (u8 *)ts; *p; p++)
							{
								consoleQueueKey(asciiToConsole[*p]);
							}
						}
					}
					else
					{
						/*
						**  Partial match; advance the string pointer
						*/
						autoPos += 2;
					}
				}
				else
				{
					/*
					**  No match, reset match position to start.
					*/
					autoPos = 0;
				}
			}
		}
		mfr->activeChannel->full = FALSE;
		break;

	case Fc6612Sel512DotsLeft:
	case Fc6612Sel512DotsRight:
		if (mfr->activeChannel->full)
		{
			emptyDrop = FALSE;

			ch = (u8)((mfr->activeChannel->data >> 6) & Mask6);

			if (ch >= 060)
			{
				if (ch >= 070)
				{
					/*
					**  Vertical coordinate.
					*/
					windowSetY((u16)(mfr->activeChannel->data & Mask9));
					windowQueue('.');
				}
				else
				{
					/*
					**  Horizontal coordinate.
					*/
					windowSetX((u16)((mfr->activeChannel->data & Mask9) + currentOffset));
				}
			}

			mfr->activeChannel->full = FALSE;
		}
		break;

	case Fc6612SelKeyIn:
		windowGetChar();
		mfr->activeChannel->data = asciiToConsole[mfr->activeChannel->mfr->ppKeyIn];
		if (mfr->activeChannel->data == 0)
		{
			mfr->activeChannel->data = consoleGetKey();
		}
		mfr->activeChannel->full = TRUE;
		mfr->activeChannel->status = 0;
		mfr->activeDevice->fcode = 0;
		mfr->activeChannel->mfr->ppKeyIn = 0;
		break;
	}

}

#if MaxMainFrames == 2
static void consoleIo1(u8 mfrId)
{
	u8 ch;

	MMainFrame *mfr = BigIron->chasis[mfrId];

	if (mfr->activeDevice->mfrID == 0)
		printf("consoleIo1 mfrID = %d\n", mfr->activeDevice->mfrID);

	switch (mfr->activeDevice->fcode)
	{
	default:
		break;

	case Fc6612Sel64CharLeft:
	case Fc6612Sel32CharLeft:
	case Fc6612Sel16CharLeft:
	case Fc6612Sel64CharRight:
	case Fc6612Sel32CharRight:
	case Fc6612Sel16CharRight:
		if (mfr->activeChannel->full)
		{
			emptyDrop1 = FALSE;

			ch = (u8)((mfr->activeChannel->data >> 6) & Mask6);

			if (ch >= 060)
			{
				if (ch >= 070)
				{
					/*
					**  Vertical coordinate.
					*/
					windowSetY1((u16)(mfr->activeChannel->data & Mask9));
				}
				else
				{
					/*
					**  Horizontal coordinate.
					*/
					windowSetX1((u16)((mfr->activeChannel->data & Mask9) + currentOffset1));
				}
			}
			else
			{
				windowQueue1(consoleToAscii[(mfr->activeChannel->data >> 6) & Mask6]);
				windowQueue1(consoleToAscii[(mfr->activeChannel->data >> 0) & Mask6]);
			}

			/*
			**  Check for auto date entry.
			*/
			if (autoDate1)
			{
				/*
				**  See if medium char size, and text matches
				**  next word of "enter date" message.
				*/
				if ((mfr->activeDevice->fcode == Fc6612Sel32CharLeft ||
					mfr->activeDevice->fcode == Fc6612Sel32CharRight) &&
					((mfr->activeChannel->data >> 6) & Mask6) == asciiToCdc[(u8)autoDateString[autoPos1]] &&
					(mfr->activeChannel->data & Mask6) == asciiToCdc[(u8)autoDateString[autoPos1 + 1]])
				{
					/*
					**  It matches so far.  Let's see if we're done.
					*/
					if (autoDateString[autoPos1 + 1] == 0 ||
						autoDateString[autoPos1 + 2] == 0)
					{
						/*
						**  Entire pattern matched, supply
						**  auto date and time, provided that
						**  there is no typeahead, and keyboard
						**  is in "easy" mode.
						*/
						autoDate1 = FALSE;
						if (keyOut1 == keyIn1) // && !keyboardTrue)
						{
							time(&t1);
							/* Note that DSD supplies punctuation */
							strftime(ts1, sizeof(ts1) - 1,
								"%y%m%d\n%H%M%S\n",
								localtime(&t1));
							*ts1 = autoDateYear[0]; *(ts1 + 1) = autoDateYear[1];
							for (p1 = (u8 *)ts1; *p1; p1++)
							{
								consoleQueueKey1(asciiToConsole[*p1]);
							}
						}
					}
					else
					{
						/*
						**  Partial match; advance the string pointer
						*/
						autoPos1 += 2;
					}
				}
				else
				{
					/*
					**  No match, reset match position to start.
					*/
					autoPos1 = 0;
				}
			}

			mfr->activeChannel->full = FALSE;
		}
		break;

	case Fc6612Sel512DotsLeft:
	case Fc6612Sel512DotsRight:
		if (mfr->activeChannel->full)
		{
			emptyDrop1 = FALSE;

			ch = (u8)((mfr->activeChannel->data >> 6) & Mask6);

			if (ch >= 060)
			{
				if (ch >= 070)
				{
					/*
					**  Vertical coordinate.
					*/
					windowSetY1((u16)(mfr->activeChannel->data & Mask9));
					windowQueue1('.');
				}
				else
				{
					/*
					**  Horizontal coordinate.
					*/
					windowSetX1((u16)((mfr->activeChannel->data & Mask9) + currentOffset1));
				}
			}

			mfr->activeChannel->full = FALSE;
		}
		break;

	case Fc6612SelKeyIn:
		windowGetChar1();
		mfr->activeChannel->data = asciiToConsole[mfr->activeChannel->mfr->ppKeyIn];
		if (mfr->activeChannel->data == 0)
		{
			mfr->activeChannel->data = consoleGetKey1();
		}
		mfr->activeChannel->full = TRUE;
		mfr->activeChannel->status = 0;
		mfr->activeDevice->fcode = 0;
		mfr->activeChannel->mfr->ppKeyIn = 0;
		break;
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
static void consoleActivate(u8 mfrId)
{
	emptyDrop = TRUE;
}
#if MaxMainFrames == 2
static void consoleActivate1(u8 mfrId)
{
	emptyDrop1 = TRUE;
}
#endif
/*--------------------------------------------------------------------------
**  Purpose:        Handle disconnecting of channel.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void consoleDisconnect(u8 mfrId)
{
	if (emptyDrop)
	{
		windowUpdate();
		emptyDrop = FALSE;
	}
}
#if MaxMainFrames == 2
static void consoleDisconnect1(u8 mfrId)
{
	if (emptyDrop1)
	{
		windowUpdate1();
		emptyDrop1 = FALSE;
	}
}
#endif
/*---------------------------  End Of File  ------------------------------*/
