/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2011, Tom Hunter, Paul Koning
**  C++ adaptation by Dale Sinder 2017
**
**  Name: rtc.cpp
**
**  Description:
**      Perform emulation of CDC 6600 real-time clock.
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
#include <math.h>
#if defined(_WIN32)
#include <windows.h>
#elif defined(__GNUC__) || defined(__SunOS)
#include <sys/time.h>
#include <unistd.h>
#endif



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
static FcStatus rtcFunc(PpWord funcCode, u8 mfrId);
static void rtcIo(u8 mfrId);
static void rtcActivate(u8 mfrId);
static void rtcDisconnect(u8 mfrId);
static bool rtcInitTick();
static u64 rtcGetTick();

/*
**  ----------------
**  Public Variables
**  ----------------
*/
u32 rtcClock = 0;

//double clockx = 1.0;

/*
**  -----------------
**  Private Variables
**  -----------------
*/
static u8 rtcIncrement;
static bool rtcFull;
static u64 Hz;
static double MHz;
#if CcCycleTime
static u64 startTime;
#endif


/*
**--------------------------------------------------------------------------
**
**  Public Functions
**
**--------------------------------------------------------------------------
*/

/*--------------------------------------------------------------------------
**  Purpose:        Initialise RTC.
**
**  Parameters:     Name        Description.
**                  increment   clock increment per iteration.
**                  setMHz      cycle counter frequency in MHz.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void rtcInit(u8 increment, u32 setMHz, u8 mfrID)
{
	(void)setMHz;
	MMainFrame *mfr = BigIron->chasis[mfrID];

	DevSlot *dp = channelAttach(ChClock, 0, DtRtc, mfrID);

	dp->activate = rtcActivate;
	dp->disconnect = rtcDisconnect;
	dp->func = rtcFunc;
	dp->io = rtcIo;
	dp->selectedUnit = 0;

	mfr->activeChannel->ioDevice = dp;
	mfr->activeChannel->hardwired = true;

	mfr->activeChannel->mfrID = mfrID;

	if (increment == 0)
	{
		if (!rtcInitTick())
		{
			printf("Invalid clock increment 0, defaulting to 1\n");
			increment = 1;
		}
	}

	rtcIncrement = increment;

	/*
	**  RTC channel may be active or inactive and empty or full
	**  depending on model.
	*/
	rtcFull = (features & HasFullRTC) != 0;
	mfr->activeChannel->full = rtcFull;
	mfr->activeChannel->active = (features & HasFullRTC) != 0;
}

/*--------------------------------------------------------------------------
**  Purpose:        Do a clock tick
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void rtcTick()
{
	rtcClock += rtcIncrement;
}

/*--------------------------------------------------------------------------
**  Purpose:        Start timing measurement.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
#if CcCycleTime
void rtcStartTimer()
{
	if (rtcIncrement == 0)
	{
		startTime = rtcGetTick();
	}
}
#endif

/*--------------------------------------------------------------------------
**  Purpose:        Complete timing measurement.
**
**  Parameters:     Name        Description.
**
**  Returns:        Time in microseconds.
**
**------------------------------------------------------------------------*/
#if CcCycleTime
double rtcStopTimer()
{
	u64 endTime;
	if (rtcIncrement == 0)
	{
		endTime = rtcGetTick();
		return((double)(int)(endTime - startTime) / ((double)(i64)Hz / 1000000.0L));
	}
	else
	{
		return(0.0);
	}
}
#endif

/*--------------------------------------------------------------------------
**  Purpose:        Read current 32-bit microsecond counter and store in
**                  global variable rtcClock.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing
**
**------------------------------------------------------------------------*/

#define MaxMicroseconds 400.0L

void rtcReadUsCounter()
{
	static bool first = true;
	static u64 old = 0;
	static double fraction = 0.0L;
	static double delayedMicroseconds = 0.0L;

	if (rtcIncrement != 0)
	{
		return;
	}

	if (first)
	{
		first = false;
		old = rtcGetTick();
	}

	u64 newt = rtcGetTick();

	//newt = static_cast<u64>(newt * clockx);

	if (static_cast<i64>(newt) < static_cast<i64>(old))
	{
		/* Ignore ticks if they go backward */
		//printf("Ignored clock tick\n");
		old = newt;
		return;
	}

	u64 difference = newt - old;
	old = newt;

	double microseconds = static_cast<double>(static_cast<i64>(difference)) / MHz;
	microseconds += fraction + delayedMicroseconds;
	delayedMicroseconds = 0.0;

	if (microseconds > MaxMicroseconds)
	{
		//printf("microseconds > MaxMicroseconds\n");
		delayedMicroseconds = microseconds - MaxMicroseconds;
		microseconds = MaxMicroseconds;
	}

	double result = floor(microseconds);
	fraction = microseconds - result;

	rtcClock += static_cast<u32>(result);
}

/*
**--------------------------------------------------------------------------
**
**  Private Functions
**
**--------------------------------------------------------------------------
*/

/*--------------------------------------------------------------------------
**  Purpose:        Execute function code on RTC pseudo device.
**
**  Parameters:     Name        Description.
**                  funcCode    function code
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static FcStatus rtcFunc(PpWord funcCode, u8 mfrId)
{
	(void)funcCode;

	return(FcAccepted);
}

/*--------------------------------------------------------------------------
**  Purpose:        Perform I/O.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void rtcIo(u8 mfrId)
{
	MMainFrame *mfr = BigIron->chasis[mfrId];

	rtcReadUsCounter();
	mfr->activeChannel->full = rtcFull;
	mfr->activeChannel->data = static_cast<PpWord>(rtcClock) & Mask12;
}

/*--------------------------------------------------------------------------
**  Purpose:        Handle channel activation.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void rtcActivate(u8 mfrId)
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
static void rtcDisconnect(u8 mfrId)
{
}

#if defined(_WIN32)

/*--------------------------------------------------------------------------
**  Purpose:        Low-level microsecond tick functions.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static bool rtcInitTick()
{
	LARGE_INTEGER lhz;

	if (!QueryPerformanceFrequency(&lhz))
	{
		printf("No high resolution hardware clock, using emulation cycle counter\n");
		return(false);
	}

	Hz = lhz.QuadPart;
	MHz = static_cast<double>(static_cast<i64>(Hz)) / 1000000.0;
	printf("Using QueryPerformanceCounter() clock at %f MHz\n", MHz);
	return(true);
}

static u64 rtcGetTick()
{
	LARGE_INTEGER ctr;

	QueryPerformanceCounter(&ctr);
	return(ctr.QuadPart);
}

#elif defined(__GNUC__) && (defined(__linux__) || defined(__SunOS) || defined (__FreeBSD__) || defined (__APPLE__))

/*--------------------------------------------------------------------------
**  Purpose:        Low-level microsecond tick functions.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static bool rtcInitTick(void)
{
	Hz = 1000000;
	MHz = 1.0;
	printf("Using gettimeofday() clock at %f MHz\n", MHz);
	return(TRUE);
}

static u64 rtcGetTick(void)
{
	struct timeval tv;

	gettimeofday(&tv, NULL);
	return((u64)tv.tv_sec * (u64)1000000 + (u64)tv.tv_usec);
}

#else

/*--------------------------------------------------------------------------
**  Purpose:        Low-level microsecond tick functions.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static bool rtcInitTick(void)
{
	printf("No high resolution hardware clock, using emulation cycle counter\n");
	return(FALSE);
}

#endif

/*---------------------------  End Of File  ------------------------------*/
