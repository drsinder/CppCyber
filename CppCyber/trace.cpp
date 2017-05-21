/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2011, Tom Hunter
**  C++ adaptation by Dale Sinder 2017
**
**  Name: trace.cpp
**
**  Description:
**      Trace execution.
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
**  PPU command adressing modes.
*/
#define AN              1
#define Amd             2
#define Ar              3
#define Ad              4
#define Adm             5

/*
**  CPU command adressing modes.
*/
#define CN              1
#define CK              2
#define Ci              3
#define Cij             4
#define CiK             5
#define CjK             6
#define Cijk            7
#define Cik             8
#define Cikj            9
#define CijK            10
#define Cjk             11
#define Cj              12
#define CLINK           100

/*
**  CPU register set markers.
*/
#define R               1
#define RAA             2
#define RAAB            3
#define RAB             4
#define RABB            5
#define RAX             6
#define RAXB            7
#define RBA             8
#define RBAB            9
#define RBB             10
#define RBBB            11
#define RBX             12
#define RBXB            13
#define RX              14
#define RXA             15
#define RXAB            16
#define RXB             17
#define RXBB            18
#define RXBX            19
#define RXX             20
#define RXXB            21
#define RXXX            22
#define RZB             23
#define RZX             24
#define RXNX            25
#define RNXX            26
#define RNXN            27

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
typedef struct decPpControl
{
	u8          mode;
	char        *mnemonic;
} DecPpControl;

typedef struct decCpControl
{
	u8          mode;
	char        *mnemonic;
	u8          regSet;
} DecCpControl;

/*
**  ---------------------------
**  Private Function Prototypes
**  ---------------------------
*/

/*
**  ----------------
**  Public Variables
**  ----------------
*/

FILE *devF2;

/*
**  -----------------
**  Private Variables
**  -----------------
*/

static FILE *cpuFAt[4];
static FILE **ppuF;

static DecPpControl ppDecode[] =
{
	AN,           "PSN",    // 00
	Amd,          "LJM",    // 01
	Amd,          "RJM",    // 02
	Ar,           "UJN",    // 03
	Ar,           "ZJN",    // 04
	Ar,           "NJN",    // 05
	Ar,           "PJN",    // 06
	Ar,           "MJN",    // 07

	Ar,           "SHN",    // 10
	Ad,           "LMN",    // 11
	Ad,           "LPN",    // 12
	Ad,           "SCN",    // 13
	Ad,           "LDN",    // 14
	Ad,           "LCN",    // 15
	Ad,           "ADN",    // 16
	Ad,           "SBN",    // 17

	Adm,          "LDC",    // 20
	Adm,          "ADC",    // 21
	Adm,          "LPC",    // 22
	Adm,          "LMC",    // 23
	AN,           "PSN",    // 24
	AN,           "PSN",    // 25
	Ad,           "EXN",    // 26
	Ad,           "RPN",    // 27

	Ad,           "LDD",    // 30
	Ad,           "ADD",    // 31
	Ad,           "SBD",    // 32
	Ad,           "LMD",    // 33
	Ad,           "STD",    // 34
	Ad,           "RAD",    // 35
	Ad,           "AOD",    // 36
	Ad,           "SOD",    // 37

	Ad,           "LDI",    // 40
	Ad,           "ADI",    // 41
	Ad,           "SBI",    // 42
	Ad,           "LMI",    // 43
	Ad,           "STI",    // 44
	Ad,           "RAI",    // 45
	Ad,           "AOI",    // 46
	Ad,           "SOI",    // 47

	Amd,          "LDM",    // 50
	Amd,          "ADM",    // 51
	Amd,          "SBM",    // 52
	Amd,          "LMM",    // 53
	Amd,          "STM",    // 54
	Amd,          "RAM",    // 55
	Amd,          "AOM",    // 56
	Amd,          "SOM",    // 57

	Ad,           "CRD",    // 60
	Amd,          "CRM",    // 61
	Ad,           "CWD",    // 62
	Amd,          "CWM",    // 63
	Amd,          "AJM",    // 64
	Amd,          "IJM",    // 65
	Amd,          "FJM",    // 66
	Amd,          "EJM",    // 67

	Ad,           "IAN",    // 70
	Amd,          "IAM",    // 71
	Ad,           "OAN",    // 72
	Amd,          "OAM",    // 73
	Ad,           "ACN",    // 74
	Ad,           "DCN",    // 75
	Ad,           "FAN",    // 76
	Amd,          "FNC"     // 77
};

static DecCpControl rjDecode[010] =
{
	CK,         "RJ    %6.6o",          R,      // 0
	CjK,        "REC   B%o+%6.6o",      RZB,    // 1
	CjK,        "WEC   B%o+%6.6o",      RZB,    // 2
	CK,         "XJ    %6.6o",          R,      // 3
	Cjk,        "RX    X%o,X%o",        RNXX,   // 4
	Cjk,        "WX    X%o,X%o",        RNXX,   // 5
	Cj,         "RC    X%o",            RNXN,   // 6
	CN,         "Illegal",              R,      // 7
};

static DecCpControl cjDecode[010] =
{
	CjK,        "ZR    X%o,%6.6o",      RZX,    // 0
	CjK,        "NZ    X%o,%6.6o",      RZX,    // 1
	CjK,        "PL    X%o,%6.6o",      RZX,    // 2
	CjK,        "NG    X%o,%6.6o",      RZX,    // 3
	CjK,        "IR    X%o,%6.6o",      RZX,    // 4
	CjK,        "OR    X%o,%6.6o",      RZX,    // 5
	CjK,        "DF    X%o,%6.6o",      RZX,    // 6
	CjK,        "ID    X%o,%6.6o",      RZX,    // 7
};

static DecCpControl cpDecode[0100] =
{
	CN,         "PS",                   R,      // 00
	CLINK, reinterpret_cast<char *>(rjDecode),            R,      // 01
	CiK,        "JP    %6.6o",          R,      // 02
	CLINK, reinterpret_cast<char *>(cjDecode),            R,      // 03
	CijK,       "EQ    B%o,B%o,%6.6o",  RBB,    // 04
	CijK,       "NE    B%o,B%o,%6.6o",  RBB,    // 05
	CijK,       "GE    B%o,B%o,%6.6o",  RBB,    // 06
	CijK,       "LT    B%o,B%o,%6.6o",  RBB,    // 07

	Cij,        "BX%o   X%o",           RXX,    // 10
	Cijk,       "BX%o   X%o*X%o",       RXXX,   // 11
	Cijk,       "BX%o   X%o+X%o",       RXXX,   // 12
	Cijk,       "BX%o   X%o-X%o",       RXXX,   // 13
	Cik,        "BX%o   -X%o",          RXXX,   // 14
	Cikj,       "BX%o   -X%o*X%o",      RXXX,   // 15
	Cikj,       "BX%o   -X%o+X%o",      RXXX,   // 16
	Cikj,       "BX%o   -X%o-X%o",      RXXX,   // 17

	Cijk,       "LX%o   %o%o",          RX,     // 20
	Cijk,       "AX%o   %o%o",          RX,     // 21
	Cijk,       "LX%o   B%o,X%o",       RXBX,   // 22
	Cijk,       "AX%o   B%o,X%o",       RXBX,   // 23
	Cijk,       "NX%o   B%o,X%o",       RXBX,   // 24
	Cijk,       "ZX%o   B%o,X%o",       RXBX,   // 25
	Cijk,       "UX%o   B%o,X%o",       RXBX,   // 26
	Cijk,       "PX%o   B%o,X%o",       RXBX,   // 27

	Cijk,       "FX%o   X%o+X%o",       RXXX,   // 30
	Cijk,       "FX%o   X%o-X%o",       RXXX,   // 31
	Cijk,       "DX%o   X%o+X%o",       RXXX,   // 32
	Cijk,       "DX%o   X%o-X%o",       RXXX,   // 33
	Cijk,       "RX%o   X%o+X%o",       RXXX,   // 34
	Cijk,       "RX%o   X%o-X%o",       RXXX,   // 35
	Cijk,       "IX%o   X%o+X%o",       RXXX,   // 36
	Cijk,       "IX%o   X%o-X%o",       RXXX,   // 37

	Cijk,       "FX%o   X%o*X%o",       RXXX,   // 40
	Cijk,       "RX%o   X%o*X%o",       RXXX,   // 41
	Cijk,       "DX%o   X%o*X%o",       RXXX,   // 42
	Cijk,       "MX%o   %o%o",          RX,     // 43
	Cijk,       "FX%o   X%o/X%o",       RXXX,   // 44
	Cijk,       "RX%o   X%o/X%o",       RXXX,   // 45
	CN,         "NO",                   R,      // 46
	Cik,        "CX%o   X%o",           RXNX,   // 47

	CijK,       "SA%o   A%o+%6.6o",     RAA,    // 50
	CijK,       "SA%o   B%o+%6.6o",     RAB,    // 51
	CijK,       "SA%o   X%o+%6.6o",     RAX,    // 52
	Cijk,       "SA%o   X%o+B%o",       RAXB,   // 53
	Cijk,       "SA%o   A%o+B%o",       RAAB,   // 54
	Cijk,       "SA%o   A%o-B%o",       RAAB,   // 55
	Cijk,       "SA%o   B%o+B%o",       RABB,   // 56
	Cijk,       "SA%o   B%o-B%o",       RABB,   // 57

	CijK,       "SB%o   A%o+%6.6o",     RBA,    // 60
	CijK,       "SB%o   B%o+%6.6o",     RBB,    // 61
	CijK,       "SB%o   X%o+%6.6o",     RBX,    // 62
	Cijk,       "SB%o   X%o+B%o",       RBXB,   // 63
	Cijk,       "SB%o   A%o+B%o",       RBAB,   // 64
	Cijk,       "SB%o   A%o-B%o",       RBAB,   // 65
	Cijk,       "SB%o   B%o+B%o",       RBBB,   // 66
	Cijk,       "SB%o   B%o-B%o",       RBBB,   // 67

	CijK,       "SX%o   A%o+%6.6o",     RXA,    // 70
	CijK,       "SX%o   B%o+%6.6o",     RXB,    // 71
	CijK,       "SX%o   X%o+%6.6o",     RXX,    // 72
	Cijk,       "SX%o   X%o+B%o",       RXXB,   // 73
	Cijk,       "SX%o   A%o+B%o",       RXAB,   // 74
	Cijk,       "SX%o   A%o-B%o",       RXAB,   // 75
	Cijk,       "SX%o   B%o+B%o",       RXBB,   // 76
	Cijk,       "SX%o   B%o-B%o",       RXBB,   // 77
};

/*
**--------------------------------------------------------------------------
**
**  Public Functions
**
**--------------------------------------------------------------------------
*/

long long mstart;

#define melapsed		(double)(milliseconds_now() - mstart)

long long milliseconds_now() {
	static LARGE_INTEGER s_frequency;
	static BOOL s_use_qpc = QueryPerformanceFrequency(&s_frequency);
	if (s_use_qpc) {
		LARGE_INTEGER now;
		QueryPerformanceCounter(&now);
		return (1000LL * now.QuadPart) / s_frequency.QuadPart;
	}
	else {
		return GetTickCount();
	}
}

/*--------------------------------------------------------------------------
**  Purpose:        Initialise execution trace.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void traceInit()
{
	char ppTraceName[20];

	devF2 = fopen("device.trcx", "wt");
	if (devF2 == nullptr)
	{
		fprintf(stderr, "can't open device.trcx - aborting\n");
		exit(1);
	}

	for (u8 k = 0; k < BigIron->initCpus * BigIron->initMainFrames; k++)
	{
		char stuff[40];
		sprintf(stuff, "cpu%d.trcx", k);
		cpuFAt[k] = fopen(stuff, "wt");
		if (cpuFAt[k] == nullptr)
		{
			fprintf(stderr, "can't open cpu%d.trcx - aborting\n", k);
			exit(1);
		}
	}


	ppuF = static_cast<FILE**>(calloc(BigIron->pps * BigIron->initMainFrames, sizeof(FILE *)));
	if (ppuF == nullptr)
	{
		fprintf(stderr, "Failed to allocate PP trace FILE pointers - aborting\n");
		exit(1);
	}

	for (u8 k = 0; k < BigIron->initMainFrames; k++)
	{
		for (u8 pp = 0; pp < BigIron->pps; pp++)
		{
			sprintf(ppTraceName, "ppu-%d-%02o.trcx", k, pp);
			ppuF[pp + k*024] = fopen(ppTraceName, "wt");
			if (ppuF[pp] == nullptr)
			{
				fprintf(stderr, "Can't open ppu[%02o] trace (%s) - aborting\n", pp, ppTraceName);
				exit(1);
			}
		}
	}

	u8 mfrs = BigIron->initMainFrames;
	BigIron->chasis[0]->traceSequenceNo = 0;
	if (mfrs == 2)
		BigIron->chasis[1]->traceSequenceNo = 0;

	mstart = milliseconds_now();
}

/*--------------------------------------------------------------------------
**  Purpose:        Terminate all traces.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void traceTerminate()
{
	if (devF2 != nullptr)
	{
		fclose(devF2);
	}

	for (u8 k = 0; k < BigIron->initMainFrames; k++)
	{
		for (u8 j = 0; j < BigIron->initCpus; j++)
		{
			u8 kk = j + 2 * k;
			if (cpuFAt[kk] != nullptr)
			{
				fclose(cpuFAt[kk]);
			}
		}

		for (u8 pp = 0; pp < BigIron->pps; pp++)
		{
			if (ppuF[pp] != nullptr)
			{
				fclose(ppuF[pp]);
			}
		}
	}

	free(ppuF);
}


/*--------------------------------------------------------------------------
**  Purpose:        Output CPU opcode.
**
**  Parameters:     Name        Description.
**                  opFm        Opcode
**                  opI         i
**                  opJ         j
**                  opK         k
**                  opAddress   jk
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void traceCpu(MCpu *cpux, u32 p, u8 opFm, u8 opI, u8 opJ, u8 opK, u32 opAddress)
{
	bool link = true;
	//static bool oneIdle = TRUE;
	DecCpControl *decode = cpDecode;
	static char str[80];

	/*
	**  Bail out if no trace of the CPU is requested.
	*/


	if (cpux->cpu.CpuID == 0)
	{
		if ((cpux->mfr->traceMask & TraceCpu) == 0)
		{
			return;
		}
	}

	if (cpux->cpu.CpuID == 1)
	{
		if ((cpux->mfr->traceMask & TraceCpu1) == 0)
		{
			return;
		}
	}

	RESERVE(&BigIron->TraceMutex);

	MCpu cpuo = *cpux;

	FILE *cpuF = cpuFAt[cpuo.cpu.CpuID  + 2* cpuo.mainFrameID];
	CpuContext cpu = cpux->cpu;

#if 0
	/*
	**  Don't trace Scope 3.1 idle loop.
	*/
	if (cpu.regRaCm == 02020 && cpu.regP == 2)
	{
		if (!oneIdle)
		{
			return;
		}
		else
		{
			oneIdle = FALSE;
		}
	}
	else
	{
		oneIdle = TRUE;
	}
#endif

#if 0
	for (i = 0; i < 8; i++)
	{
		data = cpu.regX[i];
		fprintf(cpuF, "        A%d %06.6o  X%d %04.4o %04.4o %04.4o %04.4o %04.4o   B%d %06.6o\n",
			i, cpu.regA[i], i,
			(PpWord)((data >> 48) & Mask12),
			(PpWord)((data >> 36) & Mask12),
			(PpWord)((data >> 24) & Mask12),
			(PpWord)((data >> 12) & Mask12),
			(PpWord)((data)& Mask12),
			i, cpu.regB[i]);
	}
#endif

	/*
	**  Print sequence no.
	*/
	cpux->mfr->traceSequenceNo += 1;

	//if (p == 0 || p == 2 || p == 076)	// too much output EQ B0,B0
	//{
	//	RELEASE(BigIron->TraceMutex);
	//	return;
	//}



	fprintf(cpuF, "%06d ", cpux->mfr->traceSequenceNo & Mask31);

	/*
	**  Print program counter and opcode.
	*/
	fprintf(cpuF, "%6.6o  ", p);

	fprintf(cpuF, "%02o %o %o %o   ", opFm, opI, opJ, opK);        // << not quite correct, but still nice for debugging
																   /*
																   **  Print opcode mnemonic and operands.
																   */
	u8 addrMode = decode[opFm].mode;

	if (opFm == 066 && opI == 0)
	{
		sprintf(str, "CRX%o  X%o", opJ, opK);
		fprintf(cpuF, "%-30s", str);
		fprintf(cpuF, "X%d=" FMT60_020o "   ", opJ, cpu.regX[opJ]);
		fprintf(cpuF, "X%d=" FMT60_020o "   ", opK, cpu.regX[opK]);
		fprintf(cpuF, "\n");

		RELEASE(&BigIron->TraceMutex);

		return;
	}

	if (opFm == 067 && opI == 0)
	{
		sprintf(str, "CWX%o  X%o", opJ, opK);
		fprintf(cpuF, "%-30s", str);
		fprintf(cpuF, "X%d=" FMT60_020o "   ", opJ, cpu.regX[opJ]);
		fprintf(cpuF, "X%d=" FMT60_020o "   ", opK, cpu.regX[opK]);
		fprintf(cpuF, "\n");

		RELEASE(&BigIron->TraceMutex);
		return;
	}

	while (link)
	{
		link = false;

		switch (addrMode)
		{
		case CN:
			sprintf(str, decode[opFm].mnemonic);
			break;

		case CK:
			sprintf(str, decode[opFm].mnemonic, opAddress);
			break;

		case Ci:
			sprintf(str, decode[opFm].mnemonic, opI);
			break;

		case Cij:
			sprintf(str, decode[opFm].mnemonic, opI, opJ);
			break;

		case CiK:
			sprintf(str, decode[opFm].mnemonic, cpu.regB[opI] + opAddress);
			break;

		case CjK:
			sprintf(str, decode[opFm].mnemonic, opJ, opAddress);
			break;

		case Cijk:
			sprintf(str, decode[opFm].mnemonic, opI, opJ, opK);
			break;

		case Cik:
			sprintf(str, decode[opFm].mnemonic, opI, opK);
			break;

		case Cikj:
			sprintf(str, decode[opFm].mnemonic, opI, opK, opJ);
			break;

		case CijK:
			sprintf(str, decode[opFm].mnemonic, opI, opJ, opAddress);
			break;

		case Cjk:
			sprintf(str, decode[opFm].mnemonic, opJ, opK);
			break;

		case Cj:
			sprintf(str, decode[opFm].mnemonic, opJ);
			break;

		case CLINK:
			decode = reinterpret_cast<DecCpControl *>(decode[opFm].mnemonic);
			opFm = opI;
			addrMode = decode[opFm].mode;
			link = true;
			break;

		default:
			sprintf(str, "unsupported mode %02o", opFm);
			break;
		}
	}

	fprintf(cpuF, "%-30s", str);

	/*
	**  Dump relevant register set.
	*/
	switch (decode[opFm].regSet)
	{
	case R:
		break;

	case RAA:
		fprintf(cpuF, "A%d=%06o    ", opI, cpu.regA[opI]);
		fprintf(cpuF, "A%d=%06o    ", opJ, cpu.regA[opJ]);
		fprintf(cpuF, "X%d=" FMT60_020o "   ", opI, cpu.regX[opI]);
		break;

	case RAAB:
		fprintf(cpuF, "A%d=%06o    ", opI, cpu.regA[opI]);
		fprintf(cpuF, "A%d=%06o    ", opJ, cpu.regA[opJ]);
		fprintf(cpuF, "B%d=%06o    ", opK, cpu.regB[opK]);
		fprintf(cpuF, "X%d=" FMT60_020o "   ", opI, cpu.regX[opI]);
		break;

	case RAB:
		fprintf(cpuF, "A%d=%06o    ", opI, cpu.regA[opI]);
		fprintf(cpuF, "B%d=%06o    ", opJ, cpu.regB[opJ]);
		fprintf(cpuF, "X%d=" FMT60_020o "   ", opI, cpu.regX[opI]);
		break;

	case RABB:
		fprintf(cpuF, "A%d=%06o    ", opI, cpu.regA[opI]);
		fprintf(cpuF, "B%d=%06o    ", opJ, cpu.regB[opJ]);
		fprintf(cpuF, "B%d=%06o    ", opK, cpu.regB[opK]);
		fprintf(cpuF, "X%d=" FMT60_020o "   ", opI, cpu.regX[opI]);
		break;

	case RAX:
		fprintf(cpuF, "A%d=%06o    ", opI, cpu.regA[opI]);
		fprintf(cpuF, "X%d=" FMT60_020o "   ", opJ, cpu.regX[opJ]);
		fprintf(cpuF, "X%d=" FMT60_020o "   ", opI, cpu.regX[opI]);
		break;

	case RAXB:
		fprintf(cpuF, "A%d=%06o    ", opI, cpu.regA[opI]);
		fprintf(cpuF, "X%d=" FMT60_020o "   ", opJ, cpu.regX[opJ]);
		fprintf(cpuF, "B%d=%06o    ", opK, cpu.regB[opK]);
		fprintf(cpuF, "X%d=" FMT60_020o "   ", opI, cpu.regX[opI]);
		break;

	case RBA:
		fprintf(cpuF, "B%d=%06o    ", opI, cpu.regB[opI]);
		fprintf(cpuF, "A%d=%06o    ", opJ, cpu.regA[opJ]);
		break;

	case RBAB:
		fprintf(cpuF, "B%d=%06o    ", opI, cpu.regB[opI]);
		fprintf(cpuF, "A%d=%06o    ", opJ, cpu.regA[opJ]);
		fprintf(cpuF, "B%d=%06o    ", opK, cpu.regB[opK]);
		break;

	case RBB:
		fprintf(cpuF, "B%d=%06o    ", opI, cpu.regB[opI]);
		fprintf(cpuF, "B%d=%06o    ", opJ, cpu.regB[opJ]);
		break;

	case RBBB:
		fprintf(cpuF, "B%d=%06o    ", opI, cpu.regB[opI]);
		fprintf(cpuF, "B%d=%06o    ", opJ, cpu.regB[opJ]);
		fprintf(cpuF, "B%d=%06o    ", opK, cpu.regB[opK]);
		break;

	case RBX:
		fprintf(cpuF, "B%d=%06o    ", opI, cpu.regB[opI]);
		fprintf(cpuF, "X%d=" FMT60_020o "   ", opJ, cpu.regX[opJ]);
		break;

	case RBXB:
		fprintf(cpuF, "B%d=%06o    ", opI, cpu.regB[opI]);
		fprintf(cpuF, "X%d=" FMT60_020o "   ", opJ, cpu.regX[opJ]);
		fprintf(cpuF, "B%d=%06o    ", opK, cpu.regB[opK]);
		break;

	case RX:
		fprintf(cpuF, "X%d=" FMT60_020o "   ", opI, cpu.regX[opI]);
		break;

	case RXA:
		fprintf(cpuF, "X%d=" FMT60_020o "   ", opI, cpu.regX[opI]);
		fprintf(cpuF, "A%d=%06o    ", opJ, cpu.regA[opJ]);
		break;

	case RXAB:
		fprintf(cpuF, "X%d=" FMT60_020o "   ", opI, cpu.regX[opI]);
		fprintf(cpuF, "A%d=%06o    ", opJ, cpu.regA[opJ]);
		fprintf(cpuF, "B%d=%06o    ", opK, cpu.regB[opK]);
		break;

	case RXB:
		fprintf(cpuF, "X%d=" FMT60_020o "   ", opI, cpu.regX[opI]);
		fprintf(cpuF, "B%d=%06o    ", opJ, cpu.regB[opJ]);
		break;

	case RXBB:
		fprintf(cpuF, "X%d=" FMT60_020o "   ", opI, cpu.regX[opI]);
		fprintf(cpuF, "B%d=%06o    ", opJ, cpu.regB[opJ]);
		fprintf(cpuF, "B%d=%06o    ", opK, cpu.regB[opK]);
		break;

	case RXBX:
		fprintf(cpuF, "X%d=" FMT60_020o "   ", opI, cpu.regX[opI]);
		fprintf(cpuF, "B%d=%06o    ", opJ, cpu.regB[opJ]);
		fprintf(cpuF, "X%d=" FMT60_020o "   ", opK, cpu.regX[opK]);
		break;

	case RXX:
		fprintf(cpuF, "X%d=" FMT60_020o "   ", opI, cpu.regX[opI]);
		fprintf(cpuF, "X%d=" FMT60_020o "   ", opJ, cpu.regX[opJ]);
		break;

	case RXXB:
		fprintf(cpuF, "X%d=" FMT60_020o "   ", opI, cpu.regX[opI]);
		fprintf(cpuF, "X%d=" FMT60_020o "   ", opJ, cpu.regX[opJ]);
		fprintf(cpuF, "B%d=%06o    ", opK, cpu.regB[opK]);
		break;

	case RXXX:
		fprintf(cpuF, "X%d=" FMT60_020o "   ", opI, cpu.regX[opI]);
		fprintf(cpuF, "X%d=" FMT60_020o "   ", opJ, cpu.regX[opJ]);
		fprintf(cpuF, "X%d=" FMT60_020o "   ", opK, cpu.regX[opK]);
		break;

	case RZB:
		fprintf(cpuF, "B%d=%06o    ", opJ, cpu.regB[opJ]);
		break;

	case RZX:
		fprintf(cpuF, "X%d=" FMT60_020o "   ", opJ, cpu.regX[opJ]);
		break;

	case RXNX:
		fprintf(cpuF, "X%d=" FMT60_020o "   ", opI, cpu.regX[opI]);
		fprintf(cpuF, "X%d=" FMT60_020o "   ", opK, cpu.regX[opK]);
		break;

	case RNXX:
		fprintf(cpuF, "X%d=" FMT60_020o "   ", opJ, cpu.regX[opJ]);
		fprintf(cpuF, "X%d=" FMT60_020o "   ", opK, cpu.regX[opK]);
		break;

	case RNXN:
		fprintf(cpuF, "X%d=" FMT60_020o "   ", opJ, cpu.regX[opJ]);
		break;

	default:
		fprintf(cpuF, "unsupported register set %d", decode[opFm].regSet);
		break;
	}

	fprintf(cpuF, "\n");

	RELEASE(&BigIron->TraceMutex);

}

/*--------------------------------------------------------------------------
**  Purpose:        Trace a exchange jump.
**
**  Parameters:     Name        Description.
**                  cc          CPU context pointer
**                  addr        Address of exchange package
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void traceExchange(MCpu *cpux, u32 addr, char *title, char *xjSource)
{
	RESERVE(&BigIron->TraceMutex);
	CpuContext *cc = &cpux->cpu;
	MCpu cpuo = *cpux;
	FILE *cpuF = cpuFAt[cpuo.cpu.CpuID + 2 * cpuo.mainFrameID];

	/*
	**  Bail out if no trace of exchange jumps is requested.
	*/
	if ((cpux->mfr->traceMask & TraceExchange) == 0)
	{
		RELEASE(&BigIron->TraceMutex);
		return;
	}


	time_t mytime;
	time(&mytime);
	struct tm * timeinfo = localtime(&mytime);
	
	fprintf(cpuF, "\nAt: %02d:%02d:%02d", timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
	fprintf(cpuF, "\n%06d CPU%d Exchange jump with package address %06o (%s) - Source: %s\n\n", cpux->mfr->traceSequenceNo & Mask31, cpuo.cpu.CpuID, addr, title, xjSource);
	fprintf(cpuF, "P       %06o  ", cc->regP);
	fprintf(cpuF, "A%d %06o  ", 0, cc->regA[0]);
	fprintf(cpuF, "B%d %06o", 0, cc->regB[0]);
	fprintf(cpuF, "\n");

	fprintf(cpuF, "RA      %06o  ", cc->regRaCm);
	fprintf(cpuF, "A%d %06o  ", 1, cc->regA[1]);
	fprintf(cpuF, "B%d %06o", 1, cc->regB[1]);
	fprintf(cpuF, "\n");

	fprintf(cpuF, "FL      %06o  ", cc->regFlCm);
	fprintf(cpuF, "A%d %06o  ", 2, cc->regA[2]);
	fprintf(cpuF, "B%d %06o", 2, cc->regB[2]);
	fprintf(cpuF, "\n");

	fprintf(cpuF, "RAE   %08o  ", cc->regRaEcs);
	fprintf(cpuF, "A%d %06o  ", 3, cc->regA[3]);
	fprintf(cpuF, "B%d %06o", 3, cc->regB[3]);
	fprintf(cpuF, "\n");

	fprintf(cpuF, "FLE   %08o  ", cc->regFlEcs);
	fprintf(cpuF, "A%d %06o  ", 4, cc->regA[4]);
	fprintf(cpuF, "B%d %06o", 4, cc->regB[4]);
	fprintf(cpuF, "\n");

	fprintf(cpuF, "EM/FL %08o  ", cc->exitMode);
	fprintf(cpuF, "A%d %06o  ", 5, cc->regA[5]);
	fprintf(cpuF, "B%d %06o", 5, cc->regB[5]);
	fprintf(cpuF, "\n");

	fprintf(cpuF, "MA      %06.6o  ", cc->regMa);
	fprintf(cpuF, "A%d %06o  ", 6, cc->regA[6]);
	fprintf(cpuF, "B%d %06o", 6, cc->regB[6]);
	fprintf(cpuF, "\n");

	fprintf(cpuF, "STOP         %d  ", BigIron->chasis[cpuo.mainFrameID]->Acpu[cpuo.cpu.CpuID]->cpu.cpuStopped ? 1 : 0);
	fprintf(cpuF, "A%d %06o  ", 7, cc->regA[7]);
	fprintf(cpuF, "B%d %06o  ", 7, cc->regB[7]);
	fprintf(cpuF, "\n");
	fprintf(cpuF, "ECOND       %02o  ", cc->exitCondition);
	fprintf(cpuF, "\n");
	fprintf(cpuF, "MonitorCPU %d", BigIron->chasis[cpuo.mainFrameID]->monitorCpu );
	fprintf(cpuF, "\n");
	fprintf(cpuF, "\n");

	for (u8 i = 0; i < 8; i++)
	{
		fprintf(cpuF, "X%d ", i);
		CpWord data = cc->regX[i];
		fprintf(cpuF, "%04o %04o %04o %04o %04o   ",
			static_cast<PpWord>((data >> 48) & Mask12),
			static_cast<PpWord>((data >> 36) & Mask12),
			static_cast<PpWord>((data >> 24) & Mask12),
			static_cast<PpWord>((data >> 12) & Mask12),
			static_cast<PpWord>((data) & Mask12));
		fprintf(cpuF, "\n");
	}

	fprintf(cpuF, "\n\n");

	RELEASE(&BigIron->TraceMutex);

}

/*--------------------------------------------------------------------------
**  Purpose:        Output sequence number.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void traceSequence(u8 mfrID)
{
	MMainFrame *mfr = BigIron->chasis[mfrID];
	/*
	**  Increment sequence number here.
	*/
	mfr->traceSequenceNo += 1;


	/*
	**  Bail out if no trace of this PPU is requested.
	*/
	if ((mfr->traceMask & (1 << mfr->activePpu->id)) == 0)
	{
		return;
	}

	/*
	**  Print sequence no and PPU number.
	*/
	fprintf(ppuF[mfr->activePpu->id + mfrID*024 ], "%06d [%2o]    ", mfr->traceSequenceNo & Mask31, mfr->activePpu->id);

}

/*--------------------------------------------------------------------------
**  Purpose:        Output registers.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void traceRegisters(u8 mfrID)
{
	MMainFrame *mfr = BigIron->chasis[mfrID];

	/*
	**  Bail out if no trace of this PPU is requested.
	*/
	if ((mfr->traceMask & (1 << mfr->activePpu->id)) == 0)
	{
		return;
	}

	/*
	**  Print registers.
	*/
	fprintf(ppuF[mfr->activePpu->id + mfrID * 024], "P:%04o  ", mfr->activePpu->regP);
	fprintf(ppuF[mfr->activePpu->id + mfrID * 024], "A:%06o", mfr->activePpu->regA);
	fprintf(ppuF[mfr->activePpu->id + mfrID * 024], "    ");
}

/*--------------------------------------------------------------------------
**  Purpose:        Output opcode.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void traceOpcode(u8 mfrID)
{
	MMainFrame *mfr = BigIron->chasis[mfrID];

	/*
	**  Bail out if no trace of this PPU is requested.
	*/
	if ((mfr->traceMask & (1 << mfr->activePpu->id)) == 0)
	{
		return;
	}

	FILE *pF = ppuF[mfr->activePpu->id + mfrID*024];

	/*
	**  Print opcode.
	*/
	PpWord opCode = mfr->activePpu->mem[mfr->activePpu->regP];
	u8 opF = opCode >> 6;
	u8 opD = opCode & 077;
	u8 addrMode = ppDecode[opF].mode;

	fprintf(pF, "O:%04o   %3.3s ", opCode, ppDecode[opF].mnemonic);

	// ReSharper disable once CppDefaultCaseNotHandledInSwitchStatement
	switch (addrMode)
	{
	case AN:
		fprintf(pF, "        ");
		break;

	case Amd:
		fprintf(pF, "%04o,%02o ", mfr->activePpu->mem[mfr->activePpu->regP + 1], opD);
		break;

	case Ar:
		if (opD < 040)
		{
			fprintf(pF, "+%02o     ", opD);
		}
		else
		{
			fprintf(pF, "-%02o     ", 077 - opD);
		}
		break;

	case Ad:
		fprintf(pF, "%02o      ", opD);
		break;

	case Adm:
		fprintf(pF, "%02o%04o  ", opD, mfr->activePpu->mem[mfr->activePpu->regP + 1]);
		break;
	}

	fprintf(pF, "    ");

}

/*--------------------------------------------------------------------------
**  Purpose:        Output opcode.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
u8 traceDisassembleOpcode(char *str, PpWord *pm)
{
	u8 result = 1;

	/*
	**  Print opcode.
	*/
	PpWord opCode = *pm++;
	u8 opF = opCode >> 6;
	u8 opD = opCode & 077;
	u8 addrMode = ppDecode[opF].mode;

	str += sprintf(str, "%3.3s  ", ppDecode[opF].mnemonic);

	// ReSharper disable once CppDefaultCaseNotHandledInSwitchStatement
	switch (addrMode)
	{
	case AN:
		sprintf(str, "        ");
		break;

	case Amd:
		sprintf(str, "%04o,%02o ", *pm, opD);
		result = 2;
		break;

	case Ar:
		if (opD < 040)
		{
			sprintf(str, "+%02o     ", opD);
		}
		else
		{
			sprintf(str, "-%02o     ", 077 - opD);
		}
		break;

	case Ad:
		sprintf(str, "%02o      ", opD);
		break;

	case Adm:
		sprintf(str, "%02o%04o  ", opD, *pm);
		result = 2;
		break;
	}

	return(result);
}

/*--------------------------------------------------------------------------
**  Purpose:        Output channel unclaimed function info.
**
**  Parameters:     Name        Description.
**                  funcCode    Function code.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void traceChannelFunction(PpWord funcCode, u8 mfrId)
{
	MMainFrame *mfr = BigIron->chasis[mfrId];

	fprintf(devF2, "%06d [%02o]    ", mfr->traceSequenceNo & Mask31, BigIron->chasis[0]->activePpu->id);
	fprintf(devF2, "Unclaimed function code %04o on CH%02o\n", funcCode, mfr->activeChannel->id);
}

/*--------------------------------------------------------------------------
**  Purpose:        Output string for PPU.
**
**  Parameters:     Name        Description.
**                  str         String to output.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void tracePrint(char *str, u8 mfrID)
{
	MMainFrame *mfr = BigIron->chasis[mfrID];

	fputs(str, ppuF[mfr->activePpu->id + mfrID*024]);

}

/*--------------------------------------------------------------------------
**  Purpose:        Output string for CPU.
**
**  Parameters:     Name        Description.
**                  str         String to output.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void traceCpuPrint(MCpu *cpux, char *str)
{
	MCpu cpuo = *cpux;
	FILE *cpuF = cpuFAt[cpuo.cpu.CpuID + 2 * cpuo.mainFrameID];

	fputs(str, cpuF);

}

/*--------------------------------------------------------------------------
**  Purpose:        Output status of channel.
**
**  Parameters:     Name        Description.
**                  ch          channel number.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void traceChannel(u8 ch, u8 mfrID)
{
	MMainFrame *mfr = BigIron->chasis[mfrID];

	/*
	**  Bail out if no trace of this PPU is requested.
	*/
	if ((mfr->traceMask & (1 << mfr->activePpu->id)) == 0)
	{
		return;
	}

	fprintf(ppuF[mfr->activePpu->id], "  CH:%c%c%c",
		BigIron->chasis[mfrID]->channel[ch].active ? 'A' : 'D',
		BigIron->chasis[mfrID]->channel[ch].full ? 'F' : 'E',
		BigIron->chasis[mfrID]->channel[ch].ioDevice == nullptr ? 'I' : 'S');
}

/*--------------------------------------------------------------------------
**  Purpose:        Output end-of-line.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void traceEnd(u8 mfrID)
{
	MMainFrame *mfr = BigIron->chasis[mfrID];

	/*
	**  Bail out if no trace of this PPU is requested.
	*/
	if ((mfr->traceMask & (1 << mfr->activePpu->id)) == 0)
	{
		return;
	}

	/*
	**  Print newline.
	*/
	fprintf(ppuF[mfr->activePpu->id + mfrID*024], "\n");
}

/*---------------------------  End Of File  ------------------------------*/

