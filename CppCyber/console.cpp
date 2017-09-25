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

#if MaxMainFrames > 1
static FcStatus consoleFunc1(PpWord funcCode, u8 mfrId);
static void consoleIo1(u8 mfrId);
static void consoleActivate1(u8 mfrId);
static void consoleDisconnect1(u8 mfrId);
#endif

#if MaxMainFrames > 2
static FcStatus consoleFunc2(PpWord funcCode, u8 mfrId);
static void consoleIo2(u8 mfrId);
static void consoleActivate2(u8 mfrId);
static void consoleDisconnect2(u8 mfrId);
#endif

#if MaxMainFrames > 3
static FcStatus consoleFunc3(PpWord funcCode, u8 mfrId);
static void consoleIo3(u8 mfrId);
static void consoleActivate3(u8 mfrId);
static void consoleDisconnect3(u8 mfrId);
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
static bool emptyDrop = false;


/* Ring buffer for keyboard input */
static u8 keyRing[KeyBufSize];
static u32 keyIn, keyOut;

static char ts[40];
time_t t;
struct tm tmbuf;
static int autoPos;
u8 *p;

#if MaxMainFrames > 1
static u8 currentFont1;
static u16 currentOffset1;
static bool emptyDrop1 = false;

static u8 keyRing1[KeyBufSize];
static u32 keyIn1, keyOut1;

static char ts1[40];
time_t t1;
struct tm tmbuf1;
static int autoPos1;
u8 *p1;
#endif

#if MaxMainFrames > 2
static u8 currentFont2;
static u16 currentOffset2;
static bool emptyDrop2 = false;

static u8 keyRing2[KeyBufSize];
static u32 keyIn2, keyOut2;

static char ts2[40];
time_t t2;
struct tm tmbuf2;
static int autoPos2;
u8 *p2;
#endif

#if MaxMainFrames > 3
static u8 currentFont3;
static u16 currentOffset3;
static bool emptyDrop3 = false;

static u8 keyRing3[KeyBufSize];
static u32 keyIn3, keyOut3;

static char ts3[40];
time_t t3;
struct tm tmbuf3;
static int autoPos3;
u8 *p3;
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
	(void)eqNo;
	(void)unitNo;
	(void)deviceName;

	keyIn = keyOut = 0;

	DevSlot *dp = channelAttach(channelNo, eqNo, DtConsole, mfrID);

	if (mfrID == 0)
	{
		dp->activate = consoleActivate;
		dp->disconnect = consoleDisconnect;
		dp->selectedUnit = unitNo;
		dp->func = consoleFunc;
		dp->io = consoleIo;
	}
#if MaxMainFrames > 1
	if (mfrID == 1)
	{
		dp->activate = consoleActivate1;
		dp->disconnect = consoleDisconnect1;
		dp->selectedUnit = unitNo;
		dp->func = consoleFunc1;
		dp->io = consoleIo1;
	}
#endif
#if MaxMainFrames > 2
	if (mfrID == 2)
	{
		dp->activate = consoleActivate2;
		dp->disconnect = consoleDisconnect2;
		dp->selectedUnit = unitNo;
		dp->func = consoleFunc2;
		dp->io = consoleIo2;
	}
#endif
#if MaxMainFrames > 3
	if (mfrID == 3)
	{
		dp->activate = consoleActivate3;
		dp->disconnect = consoleDisconnect3;
		dp->selectedUnit = unitNo;
		dp->func = consoleFunc3;
		dp->io = consoleIo3;
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

	mfr->activeChannel->full = false;

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
#if MaxMainFrames > 1
static FcStatus consoleFunc1(PpWord funcCode, u8 mfrId)
{
	MMainFrame *mfr = BigIron->chasis[mfrId];

	if (mfr->activeChannel->mfrID !=1)
		printf("consoleFunc1 mfrID = %d\n", mfr->activeChannel->mfrID);

	mfr->activeChannel->full = false;

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

#if MaxMainFrames > 2
static FcStatus consoleFunc2(PpWord funcCode, u8 mfrId)
{
	MMainFrame *mfr = BigIron->chasis[mfrId];

	if (mfr->activeChannel->mfrID != 2)
		printf("consoleFunc2 mfrID = %d\n", mfr->activeChannel->mfrID);

	mfr->activeChannel->full = false;

	switch (funcCode)
	{
	default:
		return(FcDeclined);

	case Fc6612Sel512DotsLeft:
		currentFont2 = FontDot;
		currentOffset2 = OffLeftScreen;
		windowSetFont2(currentFont2);
		break;

	case Fc6612Sel512DotsRight:
		currentFont2 = FontDot;
		currentOffset2 = OffRightScreen;
		windowSetFont2(currentFont2);
		break;

	case Fc6612Sel64CharLeft:
		currentFont2 = FontSmall;
		currentOffset2 = OffLeftScreen;
		windowSetFont2(currentFont2);
		break;

	case Fc6612Sel32CharLeft:
		currentFont2 = FontMedium;
		currentOffset2 = OffLeftScreen;
		windowSetFont2(currentFont2);
		break;

	case Fc6612Sel16CharLeft:
		currentFont2 = FontLarge;
		currentOffset2 = OffLeftScreen;
		windowSetFont2(currentFont2);
		break;

	case Fc6612Sel64CharRight:
		currentFont2 = FontSmall;
		currentOffset2 = OffRightScreen;
		windowSetFont2(currentFont2);
		break;

	case Fc6612Sel32CharRight:
		currentFont2 = FontMedium;
		currentOffset2 = OffRightScreen;
		windowSetFont2(currentFont2);
		break;

	case Fc6612Sel16CharRight:
		currentFont2 = FontLarge;
		currentOffset2 = OffRightScreen;
		windowSetFont2(currentFont2);
		break;

	case Fc6612SelKeyIn:
		break;
	}

	mfr->activeDevice->fcode = funcCode;

	return(FcAccepted);
}
#endif

///
#if MaxMainFrames > 3
static FcStatus consoleFunc3(PpWord funcCode, u8 mfrId)
{
	MMainFrame *mfr = BigIron->chasis[mfrId];

	if (mfr->activeChannel->mfrID != 3)
		printf("consoleFunc3 mfrID = %d\n", mfr->activeChannel->mfrID);

	mfr->activeChannel->full = false;

	switch (funcCode)
	{
	default:
		return(FcDeclined);

	case Fc6612Sel512DotsLeft:
		currentFont3 = FontDot;
		currentOffset3 = OffLeftScreen;
		windowSetFont3(currentFont3);
		break;

	case Fc6612Sel512DotsRight:
		currentFont3 = FontDot;
		currentOffset3 = OffRightScreen;
		windowSetFont3(currentFont3);
		break;

	case Fc6612Sel64CharLeft:
		currentFont3 = FontSmall;
		currentOffset3 = OffLeftScreen;
		windowSetFont3(currentFont3);
		break;

	case Fc6612Sel32CharLeft:
		currentFont3 = FontMedium;
		currentOffset3 = OffLeftScreen;
		windowSetFont3(currentFont3);
		break;

	case Fc6612Sel16CharLeft:
		currentFont3 = FontLarge;
		currentOffset3 = OffLeftScreen;
		windowSetFont3(currentFont3);
		break;

	case Fc6612Sel64CharRight:
		currentFont3 = FontSmall;
		currentOffset3 = OffRightScreen;
		windowSetFont3(currentFont3);
		break;

	case Fc6612Sel32CharRight:
		currentFont3 = FontMedium;
		currentOffset3 = OffRightScreen;
		windowSetFont3(currentFont3);
		break;

	case Fc6612Sel16CharRight:
		currentFont3 = FontLarge;
		currentOffset3 = OffRightScreen;
		windowSetFont3(currentFont3);
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
bool autoDate2 = false;		// enter date/time automatically - year 98 MF2
bool autoDate3 = false;		// enter date/time automatically - year 98 MF3

void consoleQueueKey(char ch)
{
	int nextin = keyIn + 1;
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
char consoleGetKey()
{
	if (keyIn == keyOut)
		return 0;
	if ((++keyloops % 3L) != 1)
		return 0;
	int nextout = keyOut + 1;
	if (nextout == KeyBufSize)
	{
		nextout = 0;
	}
	char key = keyRing[keyOut];
	keyOut = nextout;
	//printf("keyout %c\n", consoleToAscii[key]);
	return key;
}

#if MaxMainFrames > 1
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
	int nextin = keyIn1 + 1;
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
char consoleGetKey1()
{
	if (keyIn1 == keyOut1)
		return 0;
	if ((++keyloops1 % 3L) != 1)
		return 0;
	int nextout = keyOut1 + 1;
	if (nextout == KeyBufSize)
	{
		nextout = 0;
	}
	char key = keyRing1[keyOut1];
	keyOut1 = nextout;
	//printf("keyout %c\n", consoleToAscii[key]);
	return key;
}

#endif


#if MaxMainFrames > 2
/*--------------------------------------------------------------------------
**  Purpose:        Queue keyboard input.
**
**  Parameters:     Name        Description.
**                  ch          character to be queued (display code)
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/

void consoleQueueKey2(char ch)
{
	int nextin = keyIn2 + 1;
	if (nextin == KeyBufSize)
	{
		nextin = 0;
	}
	if (nextin != keyOut2)
	{
		keyRing2[keyIn2] = ch;
		keyIn2 = nextin;
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
static u64 keyloops2 = 0;
char consoleGetKey2()
{
	if (keyIn2 == keyOut2)
		return 0;
	if ((++keyloops2 % 3L) != 1)
		return 0;
	int nextout = keyOut2 + 1;
	if (nextout == KeyBufSize)
	{
		nextout = 0;
	}
	char key = keyRing2[keyOut2];
	keyOut2 = nextout;
	//printf("keyout %c\n", consoleToAscii[key]);
	return key;
}

#endif

#if MaxMainFrames > 3
/*--------------------------------------------------------------------------
**  Purpose:        Queue keyboard input.
**
**  Parameters:     Name        Description.
**                  ch          character to be queued (display code)
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/

void consoleQueueKey3(char ch)
{
	int nextin = keyIn3 + 1;
	if (nextin == KeyBufSize)
	{
		nextin = 0;
	}
	if (nextin != keyOut3)
	{
		keyRing3[keyIn3] = ch;
		keyIn3 = nextin;
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
static u64 keyloops3 = 0;
char consoleGetKey3()
{
	if (keyIn3 == keyOut3)
		return 0;
	if ((++keyloops3 % 3L) != 1)
		return 0;
	int nextout = keyOut3 + 1;
	if (nextout == KeyBufSize)
	{
		nextout = 0;
	}
	char key = keyRing3[keyOut3];
	keyOut3 = nextout;
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
			emptyDrop = false;

			ch = static_cast<u8>((mfr->activeChannel->data >> 6) & Mask6);

			if (ch >= 060)
			{
				if (ch >= 070)
				{
					/*
					**  Vertical coordinate.
					*/
					windowSetY(static_cast<u16>(mfr->activeChannel->data & Mask9));
				}
				else
				{
					/*
					**  Horizontal coordinate.
					*/
					windowSetX(static_cast<u16>((mfr->activeChannel->data & Mask9) + currentOffset));
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
					((mfr->activeChannel->data >> 6) & Mask6) == asciiToCdc[static_cast<u8>(autoDateString[autoPos])] &&
					(mfr->activeChannel->data & Mask6) == asciiToCdc[static_cast<u8>(autoDateString[autoPos + 1])])
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
						autoDate = false;
						if (keyOut == keyIn ) // && !keyboardTrue)
						{
							time(&t);
							/* Note that DSD supplies punctuation */
							strftime(ts, sizeof(ts) - 1,
								"%y%m%d\n%H%M%S\n",
								localtime(&t));
							*ts = autoDateYear[0]; *(ts+1) = autoDateYear[1];
							for (p = reinterpret_cast<u8 *>(ts); *p; p++)
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
		mfr->activeChannel->full = false;
		break;

	case Fc6612Sel512DotsLeft:
	case Fc6612Sel512DotsRight:
		if (mfr->activeChannel->full)
		{
			emptyDrop = false;

			ch = static_cast<u8>((mfr->activeChannel->data >> 6) & Mask6);

			if (ch >= 060)
			{
				if (ch >= 070)
				{
					/*
					**  Vertical coordinate.
					*/
					windowSetY(static_cast<u16>(mfr->activeChannel->data & Mask9));
					windowQueue('.');
				}
				else
				{
					/*
					**  Horizontal coordinate.
					*/
					windowSetX(static_cast<u16>((mfr->activeChannel->data & Mask9) + currentOffset));
				}
			}

			mfr->activeChannel->full = false;
		}
		break;

	case Fc6612SelKeyIn:
		windowGetChar();
		mfr->activeChannel->data = asciiToConsole[mfr->activeChannel->mfr->ppKeyIn];
		if (mfr->activeChannel->data == 0)
		{
			mfr->activeChannel->data = consoleGetKey();
		}
		mfr->activeChannel->full = true;
		mfr->activeChannel->status = 0;
		mfr->activeDevice->fcode = 0;
		mfr->activeChannel->mfr->ppKeyIn = 0;
		break;
	}

}

#if MaxMainFrames > 1
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
			emptyDrop1 = false;

			ch = static_cast<u8>((mfr->activeChannel->data >> 6) & Mask6);

			if (ch >= 060)
			{
				if (ch >= 070)
				{
					/*
					**  Vertical coordinate.
					*/
					windowSetY1(static_cast<u16>(mfr->activeChannel->data & Mask9));
				}
				else
				{
					/*
					**  Horizontal coordinate.
					*/
					windowSetX1(static_cast<u16>((mfr->activeChannel->data & Mask9) + currentOffset1));
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
					((mfr->activeChannel->data >> 6) & Mask6) == asciiToCdc[static_cast<u8>(autoDateString[autoPos1])] &&
					(mfr->activeChannel->data & Mask6) == asciiToCdc[static_cast<u8>(autoDateString[autoPos1 + 1])])
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
						autoDate1 = false;
						if (keyOut1 == keyIn1) // && !keyboardTrue)
						{
							time(&t1);
							/* Note that DSD supplies punctuation */
							strftime(ts1, sizeof(ts1) - 1,
								"%y%m%d\n%H%M%S\n",
								localtime(&t1));
							*ts1 = autoDateYear[0]; *(ts1 + 1) = autoDateYear[1];
							for (p1 = reinterpret_cast<u8 *>(ts1); *p1; p1++)
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

			mfr->activeChannel->full = false;
		}
		break;

	case Fc6612Sel512DotsLeft:
	case Fc6612Sel512DotsRight:
		if (mfr->activeChannel->full)
		{
			emptyDrop1 = false;

			ch = static_cast<u8>((mfr->activeChannel->data >> 6) & Mask6);

			if (ch >= 060)
			{
				if (ch >= 070)
				{
					/*
					**  Vertical coordinate.
					*/
					windowSetY1(static_cast<u16>(mfr->activeChannel->data & Mask9));
					windowQueue1('.');
				}
				else
				{
					/*
					**  Horizontal coordinate.
					*/
					windowSetX1(static_cast<u16>((mfr->activeChannel->data & Mask9) + currentOffset1));
				}
			}

			mfr->activeChannel->full = false;
		}
		break;

	case Fc6612SelKeyIn:
		windowGetChar1();
		mfr->activeChannel->data = asciiToConsole[mfr->activeChannel->mfr->ppKeyIn];
		if (mfr->activeChannel->data == 0)
		{
			mfr->activeChannel->data = consoleGetKey1();
		}
		mfr->activeChannel->full = true;
		mfr->activeChannel->status = 0;
		mfr->activeDevice->fcode = 0;
		mfr->activeChannel->mfr->ppKeyIn = 0;
		break;
	}
}
#endif

#if MaxMainFrames > 2
static void consoleIo2(u8 mfrId)
{
	u8 ch;

	MMainFrame *mfr = BigIron->chasis[mfrId];

	if (mfr->activeDevice->mfrID == 0)
		printf("consoleIo2 mfrID = %d\n", mfr->activeDevice->mfrID);

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
			emptyDrop2 = false;

			ch = static_cast<u8>((mfr->activeChannel->data >> 6) & Mask6);

			if (ch >= 060)
			{
				if (ch >= 070)
				{
					/*
					**  Vertical coordinate.
					*/
					windowSetY2(static_cast<u16>(mfr->activeChannel->data & Mask9));
				}
				else
				{
					/*
					**  Horizontal coordinate.
					*/
					windowSetX2(static_cast<u16>((mfr->activeChannel->data & Mask9) + currentOffset2));
				}
			}
			else
			{
				windowQueue2(consoleToAscii[(mfr->activeChannel->data >> 6) & Mask6]);
				windowQueue2(consoleToAscii[(mfr->activeChannel->data >> 0) & Mask6]);
			}

			/*
			**  Check for auto date entry.
			*/
			if (autoDate2)
			{
				/*
				**  See if medium char size, and text matches
				**  next word of "enter date" message.
				*/
				if ((mfr->activeDevice->fcode == Fc6612Sel32CharLeft ||
					mfr->activeDevice->fcode == Fc6612Sel32CharRight) &&
					((mfr->activeChannel->data >> 6) & Mask6) == asciiToCdc[static_cast<u8>(autoDateString[autoPos2])] &&
					(mfr->activeChannel->data & Mask6) == asciiToCdc[static_cast<u8>(autoDateString[autoPos2 + 1])])
				{
					/*
					**  It matches so far.  Let's see if we're done.
					*/
					if (autoDateString[autoPos2 + 1] == 0 ||
						autoDateString[autoPos2 + 2] == 0)
					{
						/*
						**  Entire pattern matched, supply
						**  auto date and time, provided that
						**  there is no typeahead, and keyboard
						**  is in "easy" mode.
						*/
						autoDate2 = false;
						if (keyOut2 == keyIn2) // && !keyboardTrue)
						{
							time(&t2);
							/* Note that DSD supplies punctuation */
							strftime(ts2, sizeof(ts2) - 1,
								"%y%m%d\n%H%M%S\n",
								localtime(&t2));
							*ts2 = autoDateYear[0]; *(ts2 + 1) = autoDateYear[1];
							for (p2 = reinterpret_cast<u8 *>(ts2); *p2; p2++)
							{
								consoleQueueKey2(asciiToConsole[*p2]);
							}
						}
					}
					else
					{
						/*
						**  Partial match; advance the string pointer
						*/
						autoPos2 += 2;
					}
				}
				else
				{
					/*
					**  No match, reset match position to start.
					*/
					autoPos2 = 0;
				}
			}

			mfr->activeChannel->full = false;
		}
		break;

	case Fc6612Sel512DotsLeft:
	case Fc6612Sel512DotsRight:
		if (mfr->activeChannel->full)
		{
			emptyDrop2 = false;

			ch = static_cast<u8>((mfr->activeChannel->data >> 6) & Mask6);

			if (ch >= 060)
			{
				if (ch >= 070)
				{
					/*
					**  Vertical coordinate.
					*/
					windowSetY2(static_cast<u16>(mfr->activeChannel->data & Mask9));
					windowQueue2('.');
				}
				else
				{
					/*
					**  Horizontal coordinate.
					*/
					windowSetX2(static_cast<u16>((mfr->activeChannel->data & Mask9) + currentOffset2));
				}
			}

			mfr->activeChannel->full = false;
		}
		break;

	case Fc6612SelKeyIn:
		windowGetChar2();
		mfr->activeChannel->data = asciiToConsole[mfr->activeChannel->mfr->ppKeyIn];
		if (mfr->activeChannel->data == 0)
		{
			mfr->activeChannel->data = consoleGetKey2();
		}
		mfr->activeChannel->full = true;
		mfr->activeChannel->status = 0;
		mfr->activeDevice->fcode = 0;
		mfr->activeChannel->mfr->ppKeyIn = 0;
		break;
	}
}
#endif
////
#if MaxMainFrames > 3
static void consoleIo3(u8 mfrId)
{
	u8 ch;

	MMainFrame *mfr = BigIron->chasis[mfrId];

	if (mfr->activeDevice->mfrID == 0)
		printf("consoleIo3 mfrID = %d\n", mfr->activeDevice->mfrID);

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
			emptyDrop3 = false;

			ch = static_cast<u8>((mfr->activeChannel->data >> 6) & Mask6);

			if (ch >= 060)
			{
				if (ch >= 070)
				{
					/*
					**  Vertical coordinate.
					*/
					windowSetY3(static_cast<u16>(mfr->activeChannel->data & Mask9));
				}
				else
				{
					/*
					**  Horizontal coordinate.
					*/
					windowSetX3(static_cast<u16>((mfr->activeChannel->data & Mask9) + currentOffset3));
				}
			}
			else
			{
				windowQueue3(consoleToAscii[(mfr->activeChannel->data >> 6) & Mask6]);
				windowQueue3(consoleToAscii[(mfr->activeChannel->data >> 0) & Mask6]);
			}

			/*
			**  Check for auto date entry.
			*/
			if (autoDate3)
			{
				/*
				**  See if medium char size, and text matches
				**  next word of "enter date" message.
				*/
				if ((mfr->activeDevice->fcode == Fc6612Sel32CharLeft ||
					mfr->activeDevice->fcode == Fc6612Sel32CharRight) &&
					((mfr->activeChannel->data >> 6) & Mask6) == asciiToCdc[static_cast<u8>(autoDateString[autoPos3])] &&
					(mfr->activeChannel->data & Mask6) == asciiToCdc[static_cast<u8>(autoDateString[autoPos3 + 1])])
				{
					/*
					**  It matches so far.  Let's see if we're done.
					*/
					if (autoDateString[autoPos3 + 1] == 0 ||
						autoDateString[autoPos3 + 2] == 0)
					{
						/*
						**  Entire pattern matched, supply
						**  auto date and time, provided that
						**  there is no typeahead, and keyboard
						**  is in "easy" mode.
						*/
						autoDate3 = false;
						if (keyOut3 == keyIn3) // && !keyboardTrue)
						{
							time(&t3);
							/* Note that DSD supplies punctuation */
							strftime(ts3, sizeof(ts3) - 1,
								"%y%m%d\n%H%M%S\n",
								localtime(&t3));
							*ts3 = autoDateYear[0]; *(ts3 + 1) = autoDateYear[1];
							for (p3 = reinterpret_cast<u8 *>(ts3); *p3; p3++)
							{
								consoleQueueKey3(asciiToConsole[*p3]);
							}
						}
					}
					else
					{
						/*
						**  Partial match; advance the string pointer
						*/
						autoPos3 += 2;
					}
				}
				else
				{
					/*
					**  No match, reset match position to start.
					*/
					autoPos3 = 0;
				}
			}

			mfr->activeChannel->full = false;
		}
		break;

	case Fc6612Sel512DotsLeft:
	case Fc6612Sel512DotsRight:
		if (mfr->activeChannel->full)
		{
			emptyDrop1 = false;

			ch = static_cast<u8>((mfr->activeChannel->data >> 6) & Mask6);

			if (ch >= 060)
			{
				if (ch >= 070)
				{
					/*
					**  Vertical coordinate.
					*/
					windowSetY3(static_cast<u16>(mfr->activeChannel->data & Mask9));
					windowQueue3('.');
				}
				else
				{
					/*
					**  Horizontal coordinate.
					*/
					windowSetX3(static_cast<u16>((mfr->activeChannel->data & Mask9) + currentOffset3));
				}
			}

			mfr->activeChannel->full = false;
		}
		break;

	case Fc6612SelKeyIn:
		windowGetChar3();
		mfr->activeChannel->data = asciiToConsole[mfr->activeChannel->mfr->ppKeyIn];
		if (mfr->activeChannel->data == 0)
		{
			mfr->activeChannel->data = consoleGetKey3();
		}
		mfr->activeChannel->full = true;
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
	emptyDrop = true;
}
#if MaxMainFrames > 1
static void consoleActivate1(u8 mfrId)
{
	emptyDrop1 = true;
}
#endif

#if MaxMainFrames > 2
static void consoleActivate2(u8 mfrId)
{
	emptyDrop2 = true;
}
#endif
#if MaxMainFrames > 3
static void consoleActivate3(u8 mfrId)
{
	emptyDrop3 = true;
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
		emptyDrop = false;
	}
}
#if MaxMainFrames > 1
static void consoleDisconnect1(u8 mfrId)
{
	if (emptyDrop1)
	{
		windowUpdate1();
		emptyDrop1 = false;
	}
}
#endif

#if MaxMainFrames > 2
static void consoleDisconnect2(u8 mfrId)
{
	if (emptyDrop2)
	{
		windowUpdate2();
		emptyDrop2 = false;
	}
}
#endif

#if MaxMainFrames > 3
static void consoleDisconnect3(u8 mfrId)
{
	if (emptyDrop3)
	{
		windowUpdate3();
		emptyDrop3 = false;
	}
}
#endif
/*---------------------------  End Of File  ------------------------------*/
