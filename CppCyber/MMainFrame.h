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

#pragma once
#include "stdafx.h"



class MMainFrame
{
public:
	MMainFrame();

	~MMainFrame();

	void Init(u8 id, long memory);

	CpWord *cpMem;
	u32 cpuMaxMemory;
	int monitorCpu = -1;
	char ppKeyIn;
	u32 traceMask = 0;
	u32 traceSequenceNo = 0;

	PpSlot *activePpu;

	//u16 mux6676TelnetPort;
	//u16 mux6676TelnetConns;
	//u16 npuNetTelnetPort;
	//u16 npuNetTcpConns;
	u32 cycles = 0;

	int cpuCnt = 0;		// count of active cpus

	ChSlot *channel;
	u8 channelCount;

	//PpSlot *activePpu;
	//ChSlot *activeChannel;
	//DevSlot *activeDevice;
	//DevSlot *active3000Device;

#if MaxMainFrames == 2 || MaxCpus == 2
	CRITICAL_SECTION PpuMutex;
	CRITICAL_SECTION DummyMutex;
	CRITICAL_SECTION XJMutex;
	CRITICAL_SECTION XJWaitMutex;

	CONDITION_VARIABLE XJDone;
	CONDITION_VARIABLE CpuRun;
#endif

	FILE *cmHandle;

	u8 mainFrameID;
	
	class Mpp *ppBarrel[MaxPpu];
	class MCpu *Acpu[MaxCpus];

};


/*---------------------------  End Of File  ------------------------------*/


