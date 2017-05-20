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

/*
**  Class for representing a PP
*/

class Mpp
{
public:
	Mpp();
	Mpp(u8 id, u8 mfrID);
	~Mpp();

	static void Terminate(u8 mfrID);
	static void StepAll(u8 mfrID);

	PpSlot ppu;

	MMainFrame *mfr;
	u8 mfrID;

private:

	PpByte opF;
	PpByte opD;

	FILE *ppHandle;
	PpWord location;
	u32 acc18;
	bool noHang;

	void Step();

	u32 Add18(u32 op1, u32 op2);
	u32 Subtract18(u32 op1, u32 op2);

	void OpPSN();    // 00
	void OpLJM();    // 01
	void OpRJM();    // 02
	void OpUJN();    // 03
	void OpZJN();    // 04
	void OpNJN();    // 05
	void OpPJN();    // 06
	void OpMJN();    // 07
	void OpSHN();    // 10
	void OpLMN();    // 11
	void OpLPN();    // 12
	void OpSCN();    // 13
	void OpLDN();    // 14
	void OpLCN();    // 15
	void OpADN();    // 16
	void OpSBN();    // 17
	void OpLDC();    // 20
	void OpADC();    // 21
	void OpLPC();    // 22
	void OpLMC();    // 23
	void OpPSN24();  // 24
	void OpPSN25();  // 25
	void OpEXN();    // 26
	void OpRPN();    // 27
	void OpLDD();    // 30
	void OpADD();    // 31
	void OpSBD();    // 32
	void OpLMD();    // 33
	void OpSTD();    // 34
	void OpRAD();    // 35
	void OpAOD();    // 36
	void OpSOD();    // 37
	void OpLDI();    // 40
	void OpADI();    // 41
	void OpSBI();    // 42
	void OpLMI();    // 43
	void OpSTI();    // 44
	void OpRAI();    // 45
	void OpAOI();    // 46
	void OpSOI();    // 47
	void OpLDM();    // 50
	void OpADM();    // 51
	void OpSBM();    // 52
	void OpLMM();    // 53
	void OpSTM();    // 54
	void OpRAM();    // 55
	void OpAOM();    // 56
	void OpSOM();    // 57
	void OpCRD();    // 60
	void OpCRM();    // 61
	void OpCWD();    // 62
	void OpCWM();    // 63
	void OpAJM();    // 64
	void OpIJM();    // 65
	void OpFJM();    // 66
	void OpEJM();    // 67
	void OpIAN();    // 70
	void OpIAM();    // 71
	void OpOAN();    // 72
	void OpOAM();    // 73
	void OpACN();    // 74
	void OpDCN();    // 75
	void OpFAN();    // 76
	void OpFNC();    // 77


	// member function pointer
	typedef void (Mpp::*MppMbrFn)();


	MppMbrFn decodePpuOpcode[64] =
	{
		&Mpp::OpPSN,    // 00
		&Mpp::OpLJM,    // 01
		&Mpp::OpRJM,    // 02
		&Mpp::OpUJN,    // 03
		&Mpp::OpZJN,    // 04
		&Mpp::OpNJN,    // 05
		&Mpp::OpPJN,    // 06
		&Mpp::OpMJN,    // 07
		&Mpp::OpSHN,    // 10
		&Mpp::OpLMN,    // 11
		&Mpp::OpLPN,    // 12
		&Mpp::OpSCN,    // 13
		&Mpp::OpLDN,    // 14
		&Mpp::OpLCN,    // 15
		&Mpp::OpADN,    // 16
		&Mpp::OpSBN,    // 17
		&Mpp::OpLDC,    // 20
		&Mpp::OpADC,    // 21
		&Mpp::OpLPC,    // 22
		&Mpp::OpLMC,    // 23
		&Mpp::OpPSN24,  // 24
		&Mpp::OpPSN25,  // 25
		&Mpp::OpEXN,    // 26
		&Mpp::OpRPN,    // 27
		&Mpp::OpLDD,    // 30
		&Mpp::OpADD,    // 31
		&Mpp::OpSBD,    // 32
		&Mpp::OpLMD,    // 33
		&Mpp::OpSTD,    // 34
		&Mpp::OpRAD,    // 35
		&Mpp::OpAOD,    // 36
		&Mpp::OpSOD,    // 37
		&Mpp::OpLDI,    // 40
		&Mpp::OpADI,    // 41
		&Mpp::OpSBI,    // 42
		&Mpp::OpLMI,    // 43
		&Mpp::OpSTI,    // 44
		&Mpp::OpRAI,    // 45
		&Mpp::OpAOI,    // 46
		&Mpp::OpSOI,    // 47
		&Mpp::OpLDM,    // 50
		&Mpp::OpADM,    // 51
		&Mpp::OpSBM,    // 52
		&Mpp::OpLMM,    // 53
		&Mpp::OpSTM,    // 54
		&Mpp::OpRAM,    // 55
		&Mpp::OpAOM,    // 56
		&Mpp::OpSOM,    // 57
		&Mpp::OpCRD,    // 60
		&Mpp::OpCRM,    // 61
		&Mpp::OpCWD,    // 62
		&Mpp::OpCWM,    // 63
		&Mpp::OpAJM,    // 64
		&Mpp::OpIJM,    // 65
		&Mpp::OpFJM,    // 66
		&Mpp::OpEJM,    // 67
		&Mpp::OpIAN,    // 70
		&Mpp::OpIAM,    // 71
		&Mpp::OpOAN,    // 72
		&Mpp::OpOAM,    // 73
		&Mpp::OpACN,    // 74
		&Mpp::OpDCN,    // 75
		&Mpp::OpFAN,    // 76
		&Mpp::OpFNC     // 77
	};
};


/*---------------------------  End Of File  ------------------------------*/


