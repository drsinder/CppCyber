/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2011, Tom Hunter
**  C++ adaptation by Dale Sinder 2017
**
**  Name: Mpp.h
**
**  Description:
**      Perform emulation of CDC 6600 or CYBER class CPU.
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

#include "stdafx.h"


// ReSharper disable once CppPossiblyUninitializedMember
MMainFrame::MMainFrame()
{
}

MMainFrame::~MMainFrame()
{
}


void MMainFrame::Init(u8 id, long memory)
{
	mainFrameID = id;

#if MaxMainFrames == 2 || MaxCpus == 2
	INIT_MUTEX(&PpuMutex, 0x0400000);
	INIT_MUTEX(&DummyMutex, 0x01);
	INIT_MUTEX(&XJMutex, 0x040000);
	INIT_MUTEX(&XJWaitMutex, 0x04000);

	INIT_COND_VAR(&XJDone);
#endif

	// allocate CM here

	cpMem = (CpWord*)calloc(memory, sizeof(CpWord));
	if (cpMem == NULL)
	{
		fprintf(stderr, "Failed to allocate CPU memory\n");
		exit(1);
	}

	cpuMaxMemory = memory;

	if (*persistDir != '\0')
	{
		char fileName[256];

		/*
		**  Try to open existing CM file.
		*/
		char mfnum[10];
		sprintf(mfnum, "%d", mainFrameID);
		strcpy(fileName, persistDir);
		strcat(fileName, "/cmStore");
		strcat(fileName, mfnum);
		cmHandle = fopen(fileName, "r+b");
		if (cmHandle != NULL)
		{
			/*
			**  Read CM contents.
			*/
			if (fread(cpMem, sizeof(CpWord), cpuMaxMemory, cmHandle) != cpuMaxMemory)
			{
				printf("Unexpected length of CM backing file, clearing CM\n");
				memset(cpMem, 0, cpuMaxMemory);
			}
		}
		else
		{
			/*
			**  Create a new file.
			*/
			cmHandle = fopen(fileName, "w+b");
			if (cmHandle == NULL)
			{
				fprintf(stderr, "Failed to create CM backing file\n");
				exit(1);
			}
		}
	}

	/*
	**  Allocate ppus.
	*/
	u8 pp;
	u8 ppuCount = (u8)BigIron->pps;

	for (pp = 0; pp < ppuCount; pp++)
	{
		ppBarrel[pp] = new Mpp(pp, mainFrameID);
		ppBarrel[pp]->ppu.id = pp;
		ppBarrel[pp]->mfrID = mainFrameID;
		ppBarrel[pp]->mfr = this;
	}

	/*
	**  Print a friendly message.
	*/
	printf("PPs initialised (number of PPUs %o) on mainframe %d\n", ppuCount, mainFrameID);

	u8 chCount;
	if (ppuCount == 012)
	{
		chCount = 020;
	}
	else
	{
		chCount = 040;
	}

	channel = channelInit((u8)chCount, this);
	channelCount = chCount;

	rtcInit((u8)BigIron->clockIncrement, BigIron->setMHz, mainFrameID);

	/*
	**  Initialise optional Interlock Register on channel 15.
	*/
	if ((features & HasInterlockReg) != 0)
	{
		if (ppuCount == 012)
		{
			ilrInit(64, mainFrameID);
		}
		else
		{
			ilrInit(128, mainFrameID);
		}
	}

	/*
	**  Initialise optional Status/Control Register on channel 16.
	*/
	if ((features & HasStatusAndControlReg) != 0)
	{
		scrInit(ChStatusAndControl, mainFrameID);
		if (ppuCount == 024)
		{
			scrInit(ChStatusAndControl + 020, mainFrameID);
		}
	}

	// create and Init CPUs

	monitorCpu = -1;

	for (int i = 0; i < BigIron->initCpus; i++)
	{
		Acpu[i] = new MCpu(i, mainFrameID);
		Acpu[i]->Init(BigIron->model, this);
	}

	if (mainFrameID == BigIron->initMainFrames - 1)
		BigIron->InitEquipment();

}





/*---------------------------  End Of File  ------------------------------*/


