/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2011, Tom Hunter
**  C++ adaptation by Dale Sinder 2017
**
**  Name: MCpu.h
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

#include "MMainFrame.h"

/*
**  Class for representing a CPU
*/

class MCpu
{

public:

	MCpu();					// don't use!!
	MCpu(u8 id, u8 mfrID);	// USE ME!
	~MCpu();

	/*
	**  ----------------
	**  Public Functions
	**  ----------------
	*/

	// member function pointer
	typedef void (MCpu::*MCpuMbrFn)(void);

	void Init(char *model, MMainFrame *mainfr);
	void Terminate(void);
	u32  GetP(void);
	void PpReadMem(u32 address, CpWord *data);
	void PpWriteMem(u32 address, CpWord data);
	bool ExchangeJump(u32 addr, int monitorx, char *xjSource);
	bool Step(void);
	bool EcsFlagRegister(u32 ecsAddress);

	/*
	**  ----------------
	**  Public Variables
	**  ----------------
	*/

	CpuContext cpu;

	MMainFrame *mfr;	// mainframe I belong to.
	u8 mainFrameID;

	// Copies of mainframe held values for quick access.
	CpWord *cpMem;
	CpWord *extMem;
	u32 cpuMaxMemory;
	u32 extMaxMemory;

private:
	/*
	**  ---------------------------
	**  Private Function Prototypes
	**  ---------------------------
	*/
	void OpIllegal(char *from);
	bool CheckOpAddress(u32 address, u32 *location);
	void FetchOpWord(u32 address, CpWord *data);
	void VoidIwStack(u32 branchAddr);
	bool ReadMem(u32 address, CpWord *data);
	bool WriteMem(u32 address, CpWord *data);
	void RegASemantics(void);
	u32 AddRa(u32 op);
	u32 Add18(u32 op1, u32 op2);
	u32 Add24(u32 op1, u32 op2);
	u32 Subtract18(u32 op1, u32 op2);
	void UemWord(bool writeToUem);
	void EcsWord(bool writeToEcs);
	void UemTransfer(bool writeToUem);
	void EcsTransfer(bool writeToEcs);
	bool CmuGetByte(u32 address, u32 pos, u8 *byte);
	bool CmuPutByte(u32 address, u32 pos, u8 byte);
	void CmuMoveIndirect(void);
	void CmuMoveDirect(void);
	void CmuCompareCollated(void);
	void CmuCompareUncollated(void);
	void FloatCheck(CpWord value);
	void FloatExceptionHandler(void);

	void Op00(void);
	void Op01(void);
	void Op02(void);
	void Op03(void);
	void Op04(void);
	void Op05(void);
	void Op06(void);
	void Op07(void);
	void Op10(void);
	void Op11(void);
	void Op12(void);
	void Op13(void);
	void Op14(void);
	void Op15(void);
	void Op16(void);
	void Op17(void);
	void Op20(void);
	void Op21(void);
	void Op22(void);
	void Op23(void);
	void Op24(void);
	void Op25(void);
	void Op26(void);
	void Op27(void);
	void Op30(void);
	void Op31(void);
	void Op32(void);
	void Op33(void);
	void Op34(void);
	void Op35(void);
	void Op36(void);
	void Op37(void);
	void Op40(void);
	void Op41(void);
	void Op42(void);
	void Op43(void);
	void Op44(void);
	void Op45(void);
	void Op46(void);
	void Op47(void);
	void Op50(void);
	void Op51(void);
	void Op52(void);
	void Op53(void);
	void Op54(void);
	void Op55(void);
	void Op56(void);
	void Op57(void);
	void Op60(void);
	void Op61(void);
	void Op62(void);
	void Op63(void);
	void Op64(void);
	void Op65(void);
	void Op66(void);
	void Op67(void);
	void Op70(void);
	void Op71(void);
	void Op72(void);
	void Op73(void);
	void Op74(void);
	void Op75(void);
	void Op76(void);
	void Op77(void);

	typedef struct decodeElement
	{
		MCpuMbrFn execute;
		u32 length;
	} DecodeElement;

	decodeElement decodeCpuOpcode[64] =
	{
    { &MCpu::Op00, 15 },
    { &MCpu::Op01, 0 },
    { &MCpu::Op02, 30 },
    { &MCpu::Op03, 30 },
    { &MCpu::Op04, 30 },
    { &MCpu::Op05, 30 },
    { &MCpu::Op06, 30 },
    { &MCpu::Op07, 30 },
    { &MCpu::Op10, 15 },
    { &MCpu::Op11, 15 },
    { &MCpu::Op12, 15 },
    { &MCpu::Op13, 15 },
    { &MCpu::Op14, 15 },
    { &MCpu::Op15, 15 },
    { &MCpu::Op16, 15 },
    { &MCpu::Op17, 15 },
    { &MCpu::Op20, 15 },
    { &MCpu::Op21, 15 },
    { &MCpu::Op22, 15 },
    { &MCpu::Op23, 15 },
    { &MCpu::Op24, 15 },
    { &MCpu::Op25, 15 },
    { &MCpu::Op26, 15 },
    { &MCpu::Op27, 15 },
    { &MCpu::Op30, 15 },
    { &MCpu::Op31, 15 },
    { &MCpu::Op32, 15 },
    { &MCpu::Op33, 15 },
    { &MCpu::Op34, 15 },
    { &MCpu::Op35, 15 },
    { &MCpu::Op36, 15 },
    { &MCpu::Op37, 15 },
    { &MCpu::Op40, 15 },
    { &MCpu::Op41, 15 },
    { &MCpu::Op42, 15 },
    { &MCpu::Op43, 15 },
    { &MCpu::Op44, 15 },
    { &MCpu::Op45, 15 },
    { &MCpu::Op46, 15 },
    { &MCpu::Op47, 15 },
    { &MCpu::Op50, 30 },
    { &MCpu::Op51, 30 },
    { &MCpu::Op52, 30 },
    { &MCpu::Op53, 15 },
    { &MCpu::Op54, 15 },
    { &MCpu::Op55, 15 },
    { &MCpu::Op56, 15 },
    { &MCpu::Op57, 15 },
    { &MCpu::Op60, 30 },
    { &MCpu::Op61, 30 },
    { &MCpu::Op62, 30 },
    { &MCpu::Op63, 15 },
    { &MCpu::Op64, 15 },
    { &MCpu::Op65, 15 },
    { &MCpu::Op66, 15 },
    { &MCpu::Op67, 15 },
    { &MCpu::Op70, 30 },
    { &MCpu::Op71, 30 },
    { &MCpu::Op72, 30 },
    { &MCpu::Op73, 15 },
    { &MCpu::Op74, 15 },
    { &MCpu::Op75, 15 },
    { &MCpu::Op76, 15 },
    { &MCpu::Op77, 15 },	
	};

	u8 cpOp01Length[8] = { 30, 30, 30, 30, 15, 15, 15, 15 };

	/*
	**  -----------------
	**  Private Variables
	**  -----------------
	*/

	//FILE *cmHandle;
	//FILE *ecsHandle;
	
	u8 opOffset;
	CpWord opWord;
	u8 opFm;
	u8 opI;
	u8 opJ;
	u8 opK;
	u8 opLength;
	u32 opAddress;
	u32 oldRegP;
	CpWord acc60;
	u32 acc18;
	u32 acc21;
	u32 acc24;
	bool floatException = FALSE;

	int debugCount = 0;

#if CcSMM_EJT
	int skipStep = 0;
#endif

};


/*---------------------------  End Of File  ------------------------------*/


