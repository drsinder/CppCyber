/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2011, Tom Hunter
**  C++ adaptation by Dale Sinder 2017
**
**  Name: CppCyber.cpp
**
**  Description:
**      Perform emulation of CDC 6600 mainframe system.
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

// CppCyber.cpp : Defines the entry point for the console application.
//
/*
**  -------------
**  Include Files
**  -------------
*/
#include "stdafx.h"


#if defined(_WIN32)
#include <windows.h>
#include <winsock.h>
#else
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
static void waitTerminationMessage();
static void tracePpuCalls();
void CreateThreads();
static void CreateCPUThread(MCpu *c);
static void CPUThread(LPVOID p);

#if MaxCpus == 2
static void CreateCPUThread1(MCpu *c);
static void CPUThread1(LPVOID p);
#endif

#if MaxMainFrames > 1
static void CreateCPUThreadX(MCpu *c);
static void CPUThreadX(LPVOID p);
static void CreateCPUThread1X(MCpu *c);
static void CPUThread1X(LPVOID p);
#endif
/*
**  ----------------
**  Public Variables
**  ----------------
*/


/*
** Global values related to entire system
*/

/*
** Tried to put System wide things in the one and only MSystem instance:
** MSystem.h
** Mainframe specific things in MMainFrame.h
*/
MSystem *BigIron;

// leave these below global

char persistDir[256];
char printDir[256];	
char printApp[256];	

u32 traceMaskx = 0;

volatile bool opActive = false;

#if CcCycleTime
double cycleTime;
#endif

/*
**  -----------------
**  Private Variables
**  -----------------
*/

/*
**--------------------------------------------------------------------------
**
**  Public Functions
**
**--------------------------------------------------------------------------
*/

/*--------------------------------------------------------------------------
**  Purpose:        System initialisation and main program loop.
**
**  Parameters:     Name        Description.
**                  argc        Argument count.
**                  argv        Array of argument strings.
**
**  Returns:        Zero.
**
**------------------------------------------------------------------------*/

#pragma comment(lib, "ws2_32.lib")

int main(int argc, char **argv)
{
#if defined(_WIN32)
	WSADATA wsaData;

	int wsockv1 = 2;
	int wsockv1b = 2;

	if (argc > 3)	// change winsock version - params 2 and 3
	{
		wsockv1 = atoi(argv[2]);
		wsockv1b = atoi(argv[3]);
	}

	WORD versionRequested = MAKEWORD(wsockv1, wsockv1b);

	int err = WSAStartup(versionRequested, &wsaData);
	if (err != 0)
	{
		fprintf(stderr, "\r\nError in WSAStartup: %d\r\n", err);
		exit(1);
	}
#endif

	(void)argc;
	(void)argv;

	/*
	**  Setup exit handling.
	*/
	atexit(waitTerminationMessage);

	/*
	**  Setup error logging.
	*/
	logInit();

	BigIron = new MSystem();

	if (argc > 1)
	{
		BigIron->InitStartup(argv[1]);
	}
	else
	{
		BigIron->InitStartup("cyber");
	}

	BigIron->CreateMainFrames();

	/*
	**  Setup debug support.
	*/
#if CcDebug == 1
	traceInit();
#endif
	/*
	**  Setup operator interface.
	*/
	opInit();

	/*
	**  Initiate deadstart sequence.
	*/
	CreateThreads();


	while (BigIron->emulationActive)
	{
#if defined(_WIN32)
		Sleep(1000);
#else
		Sleep(1);
#endif
	}

#if CcDebug == 1
	/*
	**  Example post-mortem dumps.
	*/
	dumpInit();
	dumpAll();
#endif

	/*
	**  Shut down debug support.
	*/
#if CcDebug == 1
	traceTerminate();
	dumpTerminate();
#endif

	/*
	**  Shut down emulation.
	*/
	windowTerminate();
#if MaxMainFrames > 1
	if (BigIron->initMainFrames > 1)
		windowTerminate1();
#endif
	BigIron->Terminate();

	exit(0);
}

void CreateThreads()
{
	deadStart(0);
	CreateCPUThread(BigIron->chasis[0]->Acpu[0]);
#if MaxCpus == 2
	if ( BigIron->initCpus > 1)
	{
		CreateCPUThread1(BigIron->chasis[0]->Acpu[1]);
	}
#endif
#if MaxMainFrames > 1
	if (BigIron->initMainFrames > 1)
	{
		deadStart(1);
		CreateCPUThreadX(BigIron->chasis[1]->Acpu[0]);
#if MaxCpus == 2
		if (BigIron->initCpus > 1)
		{
			CreateCPUThread1X(BigIron->chasis[1]->Acpu[1]);
		}
#endif
	}
#endif
}


/*
**  Create Thread for CPU 0 MainFrame 0
*/
void CreateCPUThread(MCpu *cpu)
{
#if defined(_WIN32)
	DWORD dwThreadId;

	/*
	**  Create operator thread.
	*/
	HANDLE hThread = CreateThread(
		nullptr,                                       // no security attribute 
		0,                                          // default stack size 
		reinterpret_cast<LPTHREAD_START_ROUTINE>(CPUThread),
		static_cast<LPVOID>(cpu),                               // thread parameter = MCpu pointer 
		0,                                          // not suspended 
		&dwThreadId);                               // returns thread ID 

	if (hThread == nullptr)
	{
		fprintf(stderr, "Failed to create CPU %d thread\n", cpu->cpu.CpuID);
		exit(1);
	}
#else
	int rc;
	pthread_t thread;
	pthread_attr_t attr;

	/*
	**  Create POSIX thread with default attributes.
	*/
	pthread_attr_init(&attr);
	rc = pthread_create(&thread, &attr, CPU0Thread, (void*)cpu);
	if (rc < 0)
	{
		fprintf(stderr, "Failed to create CPU %d thread\n", cpu->cpu.CpuID);
		exit(1);
	}
#endif
}

#if MaxCpus == 2
/*
**  Create Thread for CPU 1 Mainrame 0
*/
void CreateCPUThread1(MCpu *cpu)
{
#if defined(_WIN32)
	DWORD dwThreadId;

	INIT_COND_VAR(&cpu->mfr->CpuRun);

	/*
	**  Create operator thread.
	*/
	HANDLE hThread = CreateThread(
		nullptr,                                       // no security attribute 
		0,                                          // default stack size 
		reinterpret_cast<LPTHREAD_START_ROUTINE>(CPUThread1),
		static_cast<LPVOID>(cpu),                               // thread parameter = MCpu pointer
		0,                                          // not suspended 
		&dwThreadId);                               // returns thread ID 

	if (hThread == nullptr)
	{
		fprintf(stderr, "Failed to create CPU %d thread\n", cpu->cpu.CpuID);
		exit(1);
	}

#else
	int rc;
	pthread_t thread;
	pthread_attr_t attr;

	/*
	**  Create POSIX thread with default attributes.
	*/
	pthread_attr_init(&attr);
	rc = pthread_create(&thread, &attr, CPU0Thread1, (void*)cpu);
	if (rc < 0)
	{
		fprintf(stderr, "Failed to create CPU %d thread\n", cpu->cpu.CpuID);
		exit(1);
	}
#endif
}
#endif
#if MaxMainFrames > 1
/*
**  Create Thread for CPU 0 MainFrame 1
*/
void CreateCPUThreadX(MCpu *cpu)
{
#if defined(_WIN32)
	DWORD dwThreadId;

	/*
	**  Create operator thread.
	*/
	HANDLE hThread = CreateThread(
		nullptr,                                       // no security attribute 
		0,                                          // default stack size 
		reinterpret_cast<LPTHREAD_START_ROUTINE>(CPUThreadX),
		static_cast<LPVOID>(cpu),                               // thread parameter = MCpu pointer 
		0,                                          // not suspended 
		&dwThreadId);                               // returns thread ID 

	if (hThread == nullptr)
	{
		fprintf(stderr, "Failed to create CPU %d thread\n", cpu->cpu.CpuID);
		exit(1);
	}
#else
	int rc;
	pthread_t thread;
	pthread_attr_t attr;

	/*
	**  Create POSIX thread with default attributes.
	*/
	pthread_attr_init(&attr);
	rc = pthread_create(&thread, &attr, CPU0Thread, (void*)cpu);
	if (rc < 0)
	{
		fprintf(stderr, "Failed to create CPU %d thread\n", cpu->cpu.CpuID);
		exit(1);
	}
#endif
}
#if MaxCpus == 2
/*
**  Create Thread for CPU 1 MainFrame 1
*/
void CreateCPUThread1X(MCpu *cpu)
{
#if defined(_WIN32)
	DWORD dwThreadId;

	INIT_COND_VAR(&cpu->mfr->CpuRun);

	/*
	**  Create operator thread.
	*/
	HANDLE hThread = CreateThread(
		nullptr,                                       // no security attribute 
		0,                                          // default stack size 
		reinterpret_cast<LPTHREAD_START_ROUTINE>(CPUThread1X),
		static_cast<LPVOID>(cpu),                               // thread parameter = MCpu pointer
		0,                                          // not suspended 
		&dwThreadId);                               // returns thread ID 

	if (hThread == nullptr)
	{
		fprintf(stderr, "Failed to create CPU %d thread\n", cpu->cpu.CpuID);
		exit(1);
	}

#else
	int rc;
	pthread_t thread;
	pthread_attr_t attr;

	/*
	**  Create POSIX thread with default attributes.
	*/
	pthread_attr_init(&attr);
	rc = pthread_create(&thread, &attr, CPU0Thread1, (void*)cpu);
	if (rc < 0)
	{
		fprintf(stderr, "Failed to create CPU %d thread\n", cpu->cpu.CpuID);
		exit(1);
	}
#endif
}
#endif
#endif
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
void CPUThread(LPVOID pCpu)
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
void CPUThread1(LPVOID pCpu)
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

#if MaxMainFrames > 1
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
void CPUThreadX(LPVOID pCpu)
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

		//if ((ncpu->mfr->cycles % 10000) == 0)
		//	printf("cycles: %d\n", ncpu->mfr->cycles++);

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
#if MaxCpus == 2
		if (BigIron->initCpus > 1)
		{
			RESERVE1(&ncpu->mfr->PpuMutex);
		}
#endif
		Mpp::StepAll(ncpu->mfr->mainFrameID);
#if MaxCpus == 2
		if (BigIron->initCpus > 1)
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
		if (BigIron->initCpus > 1)
		{
			RESERVE1(&ncpu->mfr->PpuMutex);		// not sure if this is needed
		}
		channelStep(ncpu->mfr->mainFrameID);
		if (BigIron->initCpus > 1)
		{
			RELEASE1(&ncpu->mfr->PpuMutex);
		}
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
void CPUThread1X(LPVOID pCpu)
{
	MCpu *ncpu = static_cast<MCpu*>(pCpu);

	while (BigIron->emulationActive)
	{
		//if (opActive)
		//{
		//	opRequest();
		//}
		// step CPU
		// wait for cpu 0 thread to tell us to run
		if (BigIron->initCpus > 1)
		{
			RESERVE1(&ncpu->mfr->DummyMutex);
			SleepConditionVariableCS(&ncpu->mfr->CpuRun, &ncpu->mfr->DummyMutex, 1);
			RELEASE1(&ncpu->mfr->DummyMutex);
		}
		// make sure we don't step on ppu time.
		RESERVE1(&ncpu->mfr->PpuMutex);
		for (int i = 0; i < (BigIron->cpuRatio); i++)
		{
			if (ncpu->Step())	// Step returns true if CPU stopped
				break;
		}
		RELEASE1(&ncpu->mfr->PpuMutex);
	}
}
#endif
#endif
/*
**--------------------------------------------------------------------------
**
**  Private Functions
**
**--------------------------------------------------------------------------
*/

/*--------------------------------------------------------------------------
**  Purpose:        Trace SCOPE 3.1 PPU calls (debug only).
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void tracePpuCalls()
{
	static u64 ppIrStatus[10] = { 0 };
	static u64 l;
	static u64 r;
	static FILE *f = nullptr;

	if (f == nullptr)
	{
		errno_t err = fopen_s(&f, "ppcalls.txt", "w");
		if (err != 0 || f == nullptr)
		{
			return;
		}
	}
	for (int pp = 1; pp < 10; pp++)
	{
		l = BigIron->chasis[0]->cpMem[050 + (pp * 010)] & (static_cast<CpWord>(Mask18) << (59 - 18));
		r = ppIrStatus[pp] & (static_cast<CpWord>(Mask18) << (59 - 18));
		if (l != r)
		{
			ppIrStatus[pp] = l;
			if (l != 0)
			{
				l >>= (59 - 17);
				fprintf(f, "%c", cdcToAscii[(l >> 12) & Mask6]);
				fprintf(f, "%c", cdcToAscii[(l >> 6) & Mask6]);
				fprintf(f, "%c", cdcToAscii[(l >> 0) & Mask6]);
				fprintf(f, "\n");
			}
		}
	}
}


/*--------------------------------------------------------------------------
**  Purpose:        Wait to display shutdown message.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void waitTerminationMessage()
{
	fflush(stdout);
#if defined(_WIN32)
	Sleep(5000);
#else
	sleep(5);
#endif
}

/*---------------------------  End Of File  ------------------------------*/


