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

	typedef void (MMainFrame::*MMainFrameMbrFn)();

	void CPUThread(LPVOID pCpu);
	void CPUThread1(LPVOID pCpu);

	void Init(u8 id, long memory);

	CpWord *cpMem;
	u32 cpuMaxMemory;
	int monitorCpu = -1;
	char ppKeyIn;
	u32 traceMask = 0;
	u32 traceSequenceNo = 0;

	u32 ecsFlagRegister;

	PpSlot *activePpu;
	ChSlot *activeChannel;
	DevSlot *activeDevice;
	DevSlot *active3000Device;

	u16 mux6676TelnetPort;
	u16 mux6676TelnetConns;

	u16 deadstartPanel[MaxDeadStart];
	u8 deadstartCount;


	Tcb *npuTcbs;
	int npuTcbCount;

	/*
	**  -----------------
	**  Private Variables (npu_svm)
	**  -----------------
	*/

	u8 linkRegulation[14] =
	{
		AddrHost,           // DN
		AddrNpu,            // SN
		0,                  // CN
		4,                  // BT=CMD
		PfcREG,             // PFC
		SfcLL,              // SFC
		0x0F,               // NS=1, CS=1, Regulation level=3
		0,0,0,0,            // not used
		0,0,0              // not used
	};

	u8 requestSupervision[21] =
	{
		AddrHost,           // DN
		AddrNpu,            // SN
		0,                  // CN
		4,                  // BT=CMD
		PfcSUP,             // PFC
		SfcIN,              // SFC
		0,                  // PS
		0,                  // PL
		0,                  // RI
		0,0,0,              // not used
		3,                  // CCP version
		1,                  // ...
		0,                  // CCP level
		0,                  // ...
		0,                  // CCP cycle or variant
		0,                  // ...
		0,                  // not used
		0,0                 // NCF version in NDL file (ignored)
	};

	u8 responseNpuStatus[6] =
	{
		AddrHost,           // DN
		AddrNpu,            // SN
		0,                  // CN
		4,                  // BT=CMD
		PfcNPS,             // PFC
		SfcNP | SfcResp,    // SFC
	};

	u8 responseTerminateConnection[7] =
	{
		AddrHost,           // DN
		AddrNpu,            // SN
		0,                  // CN
		4,                  // BT=CMD
		PfcTCN,             // PFC
		SfcTA | SfcResp,    // SFC
		0,                  // CN
	};

	u8 requestTerminateConnection[7] =
	{
		AddrHost,           // DN
		AddrNpu,            // SN
		0,                  // CN
		4,                  // BT=CMD
		PfcTCN,             // PFC
		SfcTA,              // SFC
		0,                  // CN
	};


	typedef enum
	{
		StIdle,
		StWaitSupervision,
		StReady,
	} SvmState;

	SvmState svmState = StIdle;

	u8 oldRegLevel = 0;

	/////

	Tcb *npuTp;

	u8 echoBuffer[1000];
	u8 *echoPtr;
	int echoLen;

	///

	NpuBuffer *bufPool = nullptr;
	int bufCount = 0;

	NpuBuffer *bipUplineBuffer = nullptr;
	NpuQueue *bipUplineQueue;

	NpuBuffer *bipDownlineBuffer = nullptr;

	typedef enum
	{
		BipIdle,
		BipDownSvm,
		BipDownDataLow,
		BipDownDataHigh,
	} BipState;

	BipState bipState = BipIdle;

	///
#define ReportInitCount         4

	int initCount = ReportInitCount;
	NpuParam *npu;

	HipState hipState = StHipInit;

	////

	u16 npuNetTcpConns = 0;
	NpuConnType connTypes[MaxConnTypes];
	int numConnTypes = 0;

	int pollIndex = 0;

	///

	//u16 mux6676TelnetPort;
	//u16 mux6676TelnetConns;

	///

	u32 cycles = 0;

	int cpuCnt = 0;		// count of active cpus

	ChSlot *channel;
	u8 channelCount;

#if MaxMainFrames > 1 || MaxCpus == 2
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


