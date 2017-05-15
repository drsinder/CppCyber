/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2011, Tom Hunter
**  C++ adaptation by Dale Sinder 2017
**
**  Name: dump.c
**
**  Description:
**      Perform dump of PP and CPU memory as well as post-mortem
**      disassembly of PP memory.
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
static FILE *cpuF[2];
static FILE *ppuF[024 * 2];

/*
**--------------------------------------------------------------------------
**
**  Public Functions
**
**--------------------------------------------------------------------------
*/

/*--------------------------------------------------------------------------
**  Purpose:        Initialize dumping.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void dumpInit(void)
{
	u8 pp;
	char ppDumpName[20];

	cpuF[0] = fopen("mainframe0.dmp", "wt");
	if (cpuF[0] == NULL)
	{
		logError(LogErrorLocation, "can't open cpu0 dump");
	}
	if (BigIron->initMainFrames == 2)
	{
		cpuF[1] = fopen("mainframe1.dmp", "wt");
		if (cpuF[1] == NULL)
		{
			logError(LogErrorLocation, "can't open mainframe1 dump");
		}
	}

	for (int k = 0; k < BigIron->initMainFrames; k++)
	{
		for (pp = 0; pp < BigIron->pps; pp++)
		{
			sprintf(ppDumpName, "ppu%02o-%o.dmp", pp, k);
			ppuF[pp+k*024] = fopen(ppDumpName, "wt");
			if (ppuF[pp] == NULL)
			{
				logError(LogErrorLocation, "can't open ppu[%02o-%o] dump", pp, k);
			}
		}
	}
}

/*--------------------------------------------------------------------------
**  Purpose:        Termiante dumping.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void dumpTerminate(void)
{
	u8 pp;

	for (u8 k = 0; k < BigIron->initMainFrames; k++)
	{
		if (cpuF[k] != NULL)
		{
			fclose(cpuF[k]);
		}

		for (pp = 0; pp < BigIron->pps; pp++)
		{
			if (ppuF[pp+ k*024] != NULL)
			{
				fclose(ppuF[pp]);
			}
		}
	}
}

/*--------------------------------------------------------------------------
**  Purpose:        Dump all PPs and CPU.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void dumpAll(void)
{
	u8 pp;

	fprintf(stderr, "dumping core...");
	fflush(stderr);

	for (u8 k = 0; k < BigIron->initMainFrames; k++)
	{
		dumpCpu(k);
		for (pp = 0; pp < BigIron->pps; pp++)
		{
			dumpPpu(pp, k);
		}
	}
}

/*--------------------------------------------------------------------------
**  Purpose:        Dump CPU.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void dumpCpu(u8 mfrID)
{
	u32 addr;
	CpWord data;
	CpWord lastData;
	bool duplicateLine;
	u8 ch;
	u8 i;
	u8 shiftCount;

	for (int cpunum = 0; cpunum < BigIron->initCpus; cpunum++)
	{
		CpuContext cpu = BigIron->chasis[mfrID]->Acpu[cpunum]->cpu;

		fprintf(cpuF[mfrID], "CPU%d\n", cpunum);

		fprintf(cpuF[mfrID], "P       %06o  ", cpu.regP);
		fprintf(cpuF[mfrID], "A%d %06o  ", 0, cpu.regA[0]);
		fprintf(cpuF[mfrID], "B%d %06o", 0, cpu.regB[0]);
		fprintf(cpuF[mfrID], "\n");

		fprintf(cpuF[mfrID], "RA      %06o  ", cpu.regRaCm);
		fprintf(cpuF[mfrID], "A%d %06o  ", 1, cpu.regA[1]);
		fprintf(cpuF[mfrID], "B%d %06o", 1, cpu.regB[1]);
		fprintf(cpuF[mfrID], "\n");

		fprintf(cpuF[mfrID], "FL      %06o  ", cpu.regFlCm);
		fprintf(cpuF[mfrID], "A%d %06o  ", 2, cpu.regA[2]);
		fprintf(cpuF[mfrID], "B%d %06o", 2, cpu.regB[2]);
		fprintf(cpuF[mfrID], "\n");

		fprintf(cpuF[mfrID], "RAE   %08o  ", cpu.regRaEcs);
		fprintf(cpuF[mfrID], "A%d %06o  ", 3, cpu.regA[3]);
		fprintf(cpuF[mfrID], "B%d %06o", 3, cpu.regB[3]);
		fprintf(cpuF[mfrID], "\n");

		fprintf(cpuF[mfrID], "FLE   %08o  ", cpu.regFlEcs);
		fprintf(cpuF[mfrID], "A%d %06o  ", 4, cpu.regA[4]);
		fprintf(cpuF[mfrID], "B%d %06o", 4, cpu.regB[4]);
		fprintf(cpuF[mfrID], "\n");

		fprintf(cpuF[mfrID], "EM/FL %08o  ", cpu.exitMode);
		fprintf(cpuF[mfrID], "A%d %06o  ", 5, cpu.regA[5]);
		fprintf(cpuF[mfrID], "B%d %06o", 5, cpu.regB[5]);
		fprintf(cpuF[mfrID], "\n");

		fprintf(cpuF[mfrID], "MA      %06o  ", cpu.regMa);
		fprintf(cpuF[mfrID], "A%d %06o  ", 6, cpu.regA[6]);
		fprintf(cpuF[mfrID], "B%d %06o", 6, cpu.regB[6]);
		fprintf(cpuF[mfrID], "\n");

		fprintf(cpuF[mfrID], "ECOND       %02o  ", cpu.exitCondition);
		fprintf(cpuF[mfrID], "A%d %06o  ", 7, cpu.regA[7]);
		fprintf(cpuF[mfrID], "B%d %06o  ", 7, cpu.regB[7]);
		fprintf(cpuF[mfrID], "\n");
		fprintf(cpuF[mfrID], "STOP         %d  ", cpu.cpuStopped ? 1 : 0);
		fprintf(cpuF[mfrID], "\n");
		fprintf(cpuF[mfrID], "\n");

		for (i = 0; i < 8; i++)
		{
			fprintf(cpuF[mfrID], "X%d ", i);
			data = cpu.regX[i];
			fprintf(cpuF[mfrID], "%04o %04o %04o %04o %04o   ",
				(PpWord)((data >> 48) & Mask12),
				(PpWord)((data >> 36) & Mask12),
				(PpWord)((data >> 24) & Mask12),
				(PpWord)((data >> 12) & Mask12),
				(PpWord)((data)& Mask12));
			fprintf(cpuF[mfrID], "\n");
		}

		fprintf(cpuF[mfrID], "\n");
	}

	lastData = ~BigIron->chasis[mfrID]->cpMem[0];
	duplicateLine = FALSE;
	for (addr = 0; addr < BigIron->chasis[mfrID]->cpuMaxMemory; addr++)
	{
		data = BigIron->chasis[mfrID]->cpMem[addr];

		if (data == lastData)
		{
			if (!duplicateLine)
			{
				fprintf(cpuF[mfrID], "     DUPLICATED LINES.\n");
				duplicateLine = TRUE;
			}
		}
		else
		{
			duplicateLine = FALSE;
			lastData = data;
			fprintf(cpuF[mfrID], "%07o   ", addr & Mask21);
			fprintf(cpuF[mfrID], "%04o %04o %04o %04o %04o   ",
				(PpWord)((data >> 48) & Mask12),
				(PpWord)((data >> 36) & Mask12),
				(PpWord)((data >> 24) & Mask12),
				(PpWord)((data >> 12) & Mask12),
				(PpWord)((data)& Mask12));

			shiftCount = 60;
			for (i = 0; i < 10; i++)
			{
				shiftCount -= 6;
				ch = (u8)((data >> shiftCount) & Mask6);
				fprintf(cpuF[mfrID], "%c", cdcToAscii[ch]);
			}
		}

		if (!duplicateLine)
		{
			fprintf(cpuF[mfrID], "\n");
		}
	}

	if (duplicateLine)
	{
		fprintf(cpuF[mfrID], "LAST ADDRESS:%07o\n", addr & Mask21);
	}
}

/*--------------------------------------------------------------------------
**  Purpose:        Dump PPU.
**
**  Parameters:     Name        Description.
**                  pp          PPU number.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void dumpPpu(u8 pp, u8 mfrID)
{
	u32 addr;
	PpWord *pm = BigIron->chasis[mfrID]->ppBarrel[pp]->ppu.mem;
	FILE *pf = ppuF[pp+mfrID*024];
	u8 i;
	PpWord pw;

	fprintf(pf, "P   %04o\n", BigIron->chasis[mfrID]->ppBarrel[pp]->ppu.regP);
	fprintf(pf, "A %06o\n", BigIron->chasis[mfrID]->ppBarrel[pp]->ppu.regA);
	fprintf(pf, "R %08o\n", BigIron->chasis[mfrID]->ppBarrel[pp]->ppu.regR);
	fprintf(pf, "\n");

	for (addr = 0; addr < PpMemSize; addr += 8)
	{
		fprintf(pf, "%04o   ", addr & Mask12);
		fprintf(pf, "%04o %04o %04o %04o %04o %04o %04o %04o  ",
			pm[addr + 0] & Mask12,
			pm[addr + 1] & Mask12,
			pm[addr + 2] & Mask12,
			pm[addr + 3] & Mask12,
			pm[addr + 4] & Mask12,
			pm[addr + 5] & Mask12,
			pm[addr + 6] & Mask12,
			pm[addr + 7] & Mask12);

		for (i = 0; i < 8; i++)
		{
			pw = pm[addr + i] & Mask12;
			fprintf(pf, "%c%c", cdcToAscii[(pw >> 6) & Mask6], cdcToAscii[pw & Mask6]);
		}

		fprintf(pf, "\n");
	}
}

/*--------------------------------------------------------------------------
**  Purpose:        Disassemble PPU memory.
**
**  Parameters:     Name        Description.
**                  pp          PPU number.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void dumpDisassemblePpu(u8 pp)  // DRS??!! both mainframes   // not called in prod.
{
	u32 addr;
	PpWord *pm = BigIron->chasis[0]->ppBarrel[pp]->ppu.mem;
	char ppDisName[20];
	FILE *pf;
	char str[80];
	u8 cnt;
	PpWord pw0, pw1;

	sprintf(ppDisName, "ppu%02o.dis", pp);
	pf = fopen(ppDisName, "wt");
	if (pf == NULL)
	{
		logError(LogErrorLocation, "can't open %s", ppDisName);
		return;
	}

	fprintf(pf, "P   %04o\n", BigIron->chasis[0]->ppBarrel[pp]->ppu.regP);
	fprintf(pf, "A %06o\n", BigIron->chasis[0]->ppBarrel[pp]->ppu.regA);
	fprintf(pf, "\n");

	for (addr = 0100; addr < PpMemSize; addr += cnt)
	{
		fprintf(pf, "%04o  ", addr & Mask12);

		cnt = traceDisassembleOpcode(str, pm + addr);
		fputs(str, pf);

		pw0 = pm[addr] & Mask12;
		pw1 = pm[addr + 1] & Mask12;
		fprintf(pf, "   %04o ", pw0);
		if (cnt == 2)
		{
			fprintf(pf, "%04o  ", pw1);
		}
		else
		{
			fprintf(pf, "      ");
		}

		fprintf(pf, "  %c%c", cdcToAscii[(pw0 >> 6) & Mask6], cdcToAscii[pw0 & Mask6]);

		if (cnt == 2)
		{
			fprintf(pf, "%c%c", cdcToAscii[(pw1 >> 6) & Mask6], cdcToAscii[pw1 & Mask6]);
		}

		fprintf(pf, "\n");
	}
}

/*--------------------------------------------------------------------------
**  Purpose:        Dump running PPU.
**
**  Parameters:     Name        Description.
**                  pp          PPU number.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void dumpRunningPpu(u8 pp)
{
	FILE *pf;
	char ppDumpName[20];

	sprintf(ppDumpName, "ppu%02o_run.dmp", pp);
	pf = fopen(ppDumpName, "wt");
	if (pf == NULL)
	{
		logError(LogErrorLocation, "can't open %s", ppDumpName);
		return;
	}

	ppuF[pp] = pf;

	dumpPpu(pp, 0);
	fclose(pf);

	ppuF[pp] = NULL;
}

/*--------------------------------------------------------------------------
**  Purpose:        Dump running CPU.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void dumpRunningCpu(u8 mfrID)
{
	cpuF[mfrID] = fopen("cpu_run.dmp", "wt");
	if (cpuF[mfrID] == NULL)
	{
		logError(LogErrorLocation, "can't open cpu_run.dmp");
		return;
	}

	dumpCpu(mfrID);
	fclose(cpuF[mfrID]);

	cpuF[mfrID] = NULL;
}

/*---------------------------  End Of File  ------------------------------*/
