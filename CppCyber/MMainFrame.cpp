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
	traceMask = traceMaskx;
	traceSequenceNo = 0;
	mux6676TelnetPort = BigIron->mux6676TelnetPortx;
	mux6676TelnetConns = BigIron->mux6676TelnetConnsx;

#if MaxMainFrames > 1 || MaxCpus == 2
	INIT_MUTEX(&PpuMutex, 0x0400000);
	INIT_MUTEX(&DummyMutex, 0x01);
	INIT_MUTEX(&XJMutex, 0x040000);
	INIT_MUTEX(&XJWaitMutex, 0x04000);

	INIT_COND_VAR(&XJDone);
#endif

	// allocate CM here

	cpMem = static_cast<CpWord*>(calloc(memory, sizeof(CpWord)));
	if (cpMem == nullptr)
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
		if (cmHandle != nullptr)
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
			if (cmHandle == nullptr)
			{
				fprintf(stderr, "Failed to create CM backing file\n");
				exit(1);
			}
		}
	}

	u8 ppuCount = static_cast<u8>(BigIron->pps);

	for (u8 pp = 0; pp < ppuCount; pp++)
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

	channel = channelInit(static_cast<u8>(chCount), this);
	channelCount = chCount;

	rtcInit(static_cast<u8>(BigIron->clockIncrement), BigIron->setMHz, mainFrameID);

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

	BigIron->InitDeadstart(mainFrameID);
	BigIron->InitNpuConnections(mainFrameID);
	BigIron->InitEquipment(mainFrameID);

	if (mainFrameID == BigIron->initMainFrames - 1)
	{
		BigIron->FinishInitFile();
	}

}

/*
**  Rules:  1) Don't let CPUs and PPUs run at same time.
**			2) Best to tell cpu 1 when cpu 0 is about to start
**				steps so cpu 1 thread does not hog the ppuMutex.
*/

/*---------------------------------------------------------
**	CPU 0 Thread
**	Input:		Pointer to a CPU instance
**	Returns:	Nothing
**
**	In addition to stepping CPU 0 this thread steps the pps
**	channels, counts cycles, keeps time and does operator
**	interaction..
**--------------------------------------------------------*/
void MMainFrame::CPUThread(LPVOID pCpu)
{
	MCpu *ncpu = static_cast<MCpu*>(pCpu);
	ncpu->mfr->cycles = 0;

	while (BigIron->emulationActive)
	{
#if CcCycleTime
		rtcStartTimer();
#endif

		/*
		**  Count major cycles.
		*/
		ncpu->mfr->cycles++;

		/*
		**  Deal with operator interface requests.
		*/
		if (opActive)
		{
			opRequest();
		}

		/*
		**  Execute PP, CPU and RTC.
		*/
#if MaxMainFrames > 1
		if (BigIron->initMainFrames > 1)
		{
			RESERVE1(&BigIron->SysPpMutex);
		}
#endif
#if MaxMainFrames > 1 || MaxCpus == 2
		if (BigIron->initCpus > 1 || BigIron->initMainFrames > 1)
		{
			RESERVE1(&ncpu->mfr->PpuMutex);
		}
#endif
		Mpp::StepAll(ncpu->mfr->mainFrameID);
#if MaxMainFrames > 1 || MaxCpus == 2
		if (BigIron->initCpus > 1 || BigIron->initMainFrames > 1)
		{
			RELEASE1(&ncpu->mfr->PpuMutex);
		}
#endif
#if MaxMainFrames > 1
		if (BigIron->initMainFrames > 1)
		{
			RELEASE1(&BigIron->SysPpMutex);
		}
#endif
		// step CPU
#if MaxCpus == 2
		if (BigIron->initCpus > 1)	// tell cpu 1 thread it can run now too
			WakeConditionVariable(&ncpu->mfr->CpuRun);
#endif
		for (int i = 0; i < BigIron->cpuRatio; i++)
		{
			if (ncpu->Step())	// Step returns true if CPU stopped - no need to step more
				break;
		}
#if MaxMainFrames > 1
		if (BigIron->initMainFrames > 1)
		{
			RESERVE1(&BigIron->SysPpMutex);
		}
#endif
#if MaxCpus == 2
		RESERVE1(&ncpu->mfr->PpuMutex);		// not sure if this is needed
#endif
		channelStep(ncpu->mfr->mainFrameID);
#if MaxCpus == 2
		RELEASE1(&ncpu->mfr->PpuMutex);
#endif
#if MaxMainFrames > 1
		if (BigIron->initMainFrames > 1)
		{
			RELEASE1(&BigIron->SysPpMutex);
		}
#endif
		rtcTick();

#if CcCycleTime
		cycleTime = rtcStopTimer();
#endif
	}
}

#if MaxCpus == 2
/*----------------------------------------------------------------
**	CPU 1 Thread
**	Input:		Pointer to a CPU instance
**	Returns:	Nothing
**
**	This thread just waits for an opportunity to step CPU 1.
**  Thread 0 signals when it starts so we can run then too.
**---------------------------------------------------------------*/
void MMainFrame::CPUThread1(LPVOID pCpu)
{
	MCpu *ncpu = static_cast<MCpu*>(pCpu);

	while (BigIron->emulationActive)
	{
		// step CPU
		// wait for cpu 0 thread to tell us to run
#if MaxCpus == 2
		RESERVE1(&ncpu->mfr->DummyMutex);
		SleepConditionVariableCS(&ncpu->mfr->CpuRun, &ncpu->mfr->DummyMutex, 1);
		RELEASE1(&ncpu->mfr->DummyMutex);
		// make sure we don't step on ppu time.
		RESERVE1(&ncpu->mfr->PpuMutex);
#endif
		for (int i = 0; i < (BigIron->cpuRatio); i++)
		{
			if (ncpu->Step())	// Step returns true if CPU stopped
				break;
		}
#if MaxCpus == 2
		RELEASE1(&ncpu->mfr->PpuMutex);
#endif
	}
}
#endif

/*---------------------------  End Of File  ------------------------------*/


