/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2011, Tom Hunter
**  C++ adaptation by Dale Sinder 2017
**
**  Name: Mpp.cpp
**
**  Description:
**      Perform emulation of PPUs of a CDC 6600 mainframe system.
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


#define Increment(word) (word) = (((word) + 1) & Mask12)
#define Decrement(word) (word) = (((word) - 1) & Mask12)

///// the program (PPU ?) stops when "from" is 000 or 077 a deadstart is necessary - see 6600 RM page 4-22 (UJN)
#define AddOffset(to, from)													\
    {                                                                       \
    (to) = ((to) - 1) & Mask12;                                             \
    if (from < 040)                                                         \
        (to) = ((to) + (from));                                             \
    else                                                                    \
        (to) = ((to) + (from) - 077);                                       \
    if (((to) & Overflow12) != 0)                                           \
        {                                                                   \
        (to) += 1;                                                          \
        }                                                                   \
    (to) &= Mask12;                                                         \
    }

#define IndexLocation                                                       \
    if (opD != 0)                                                           \
        {                                                                   \
        location = ppu.mem[opD] + ppu.mem[ppu.regP];						\
        }                                                                   \
    else                                                                    \
        {                                                                   \
        location = ppu.mem[ppu.regP];										\
        }                                                                   \
    if ((location & Overflow12) != 0 || (location & Mask12) == 07777)       \
        {                                                                   \
        location += 1;                                                      \
        }                                                                   \
    location &= Mask12;                                                     \
    Increment(ppu.regP);


// Global vars



Mpp::Mpp()
{
	fprintf(stdout, "Must use one ARG Mpp Constructor.\n");
	exit(1);
}

Mpp::Mpp(u8 id, u8 mfrID)
{
	ppu.id = id;	// so I know who I am

	

	/*
	**  Optionally read in persistent CM and ECS contents.
	*/
	if (*persistDir != '\0')
	{
		char fileName[256];

		/*
		**  Try to open existing CM file.
		*/

		char shortfile[80];
		sprintf(shortfile, "/ppStore-%d-%d", id, mfrID);

		strcpy(fileName, persistDir);
		strcat(fileName, shortfile);
		ppHandle = fopen(fileName, "r+b");
		if (ppHandle != NULL)
		{
			/*
			**  Read PPM contents.
			*/
			if (fread(&ppu, sizeof(PpSlot), 1, ppHandle) != 1)
			{
				printf("Unexpected length of PPM backing file, clearing PPM\n");
				memset(&ppu, 0, sizeof(PpSlot));
			}
		}
		else
		{
			/*
			**  Create a new file.
			*/
			ppHandle = fopen(fileName, "w+b");
			if (ppHandle == NULL)
			{
				fprintf(stderr, "Failed to create PPM backing file\n");
				exit(1);
			}
		}
	}
}


Mpp::~Mpp()
{
	if (ppHandle != NULL)
	{
		fseek(ppHandle, 0, SEEK_SET);
		if (fwrite(&ppu, sizeof(PpSlot), 1, ppHandle) != 1)
		{
			fprintf(stderr, "Error writing PPM backing file\n");
		}
		fclose(ppHandle);
	}
}


/*--------------------------------------------------------------------------
**  Purpose:        Terminate PP subsystem.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void Mpp::Terminate(u8 mfrID)
{
	/*
	**  Optionally save PPM.
	*/
	u8 pp;

	for (pp = 0; pp < BigIron->pps ; pp++)
	{
		BigIron->chasis[mfrID]->ppBarrel[pp]->~Mpp();
	}
}

void  Mpp::StepAll(u8 mfrID)
{
	MMainFrame* mfr = BigIron->chasis[mfrID];
	for (u8 pp = 0; pp < BigIron->pps ; pp++)
	{
		mfr->ppBarrel[pp]->Step();
	}
}

/*--------------------------------------------------------------------------
**  Purpose:        Execute one instruction in an active PPU.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void Mpp::Step(void)
{
	PpWord opCode;

	activePpu = &(this->ppu);

	/*
	**  Exercise each PP in the barrel.
	*/

		if (!ppu.busy)
		{
			/*
			**  Extract next PPU instruction.
			*/
			opCode = ppu.mem[ppu.regP];
			opF = (opCode >> 6) & 077;
			opD = opCode & 077;

			/*
			**  Save opF and opD for post-instruction trace.
			*/
			activePpu->opF = opF;
			activePpu->opD = opD;

#if CcDebug == 1


			/*
			**  Trace instructions.
			*/
			traceSequence(mfrID);
			traceRegisters(mfrID);
			traceOpcode(mfrID);
#else
			traceSequenceNo += 1;
#endif

			/*
			**  Increment register P.
			*/
			Increment(ppu.regP);

			/*
			**  Execute PPU instruction.
			*/
			CALL_MEMBER_FN(*this, decodePpuOpcode[ppu.opF])();

		}
		else
		{
			/*
			**  Resume PPU instruction.
			*/
			CALL_MEMBER_FN(*this, decodePpuOpcode[ppu.opF])();
		}

#if CcDebug == 1
		if (!activePpu->busy)
		{
			/*
			**  Trace result.
			*/
			traceRegisters(mfrID);

			/*
			**  Trace new channel status.
			*/
			if (opF >= 064)
			{
				traceChannel((u8)(opD & 037), mfrID);
			}

			traceEnd(mfrID);
		}
#endif

}


/*
**--------------------------------------------------------------------------
**
**  Private Functions
**
**--------------------------------------------------------------------------
*/

/*--------------------------------------------------------------------------
**  Purpose:        18 bit ones-complement addition with subtractive adder
**
**  Parameters:     Name        Description.
**                  op1         18 bit operand1
**                  op2         18 bit operand2
**
**  Returns:        18 bit result.
**
**------------------------------------------------------------------------*/
u32 Mpp::Add18(u32 op1, u32 op2)
{
	acc18 = (op1 & Mask18) - (~op2 & Mask18);
	if ((acc18 & Overflow18) != 0)
	{
		acc18 -= 1;
	}

	return(acc18 & Mask18);
}

/*--------------------------------------------------------------------------
**  Purpose:        18 bit ones-complement subtraction
**
**  Parameters:     Name        Description.
**                  op1         18 bit operand1
**                  op2         18 bit operand2
**
**  Returns:        18 bit result.
**
**------------------------------------------------------------------------*/
u32 Mpp::Subtract18(u32 op1, u32 op2)
{
	acc18 = (op1 & Mask18) - (op2 & Mask18);
	if ((acc18 & Overflow18) != 0)
	{
		acc18 -= 1;
	}

	return(acc18 & Mask18);
}




/*--------------------------------------------------------------------------
**  Purpose:        Functions to implement all opcodes
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void Mpp::OpPSN(void)     // 00
{
	/*
	**  Do nothing.
	*/
}

void Mpp::OpLJM(void)     // 01 LONG JUMP MEMORY
{
	IndexLocation;
	ppu.regP = location;
}

void Mpp::OpRJM(void)     // 02 RETURN JUMP MEMORY
{
	IndexLocation;
	ppu.mem[location] = ppu.regP;
	Increment(location);
	ppu.regP = location;
}

void Mpp::OpUJN(void)     // 03 Unconditional Jump
{
	AddOffset(ppu.regP, opD);
}

void Mpp::OpZJN(void)     // 04 Zero Jump
{
	if (ppu.regA == 0)
	{
		AddOffset(ppu.regP, opD);
	}
}

void Mpp::OpNJN(void)     // 05 Non-Zero Jump
{
	if (ppu.regA != 0)
	{
		AddOffset(ppu.regP, opD);
	}
}

void Mpp::OpPJN(void)     // 06 Positive Jump
{
	if (ppu.regA < 0400000)
	{
		AddOffset(ppu.regP, opD);
	}
}

void Mpp::OpMJN(void)     // 07 Minus (Negative) Jump
{
	if (ppu.regA > 0377777)
	{
		AddOffset(ppu.regP, opD);
	}
}

void Mpp::OpSHN(void)     // 10 SHIFT No Address
{
	u64 acc;

	if (opD < 040)
	{
		opD = opD % 18;
		acc = ppu.regA & Mask18;
		acc <<= opD;
		ppu.regA = (u32)((acc & Mask18) | (acc >> 18));
	}
	else if (opD > 037)
	{
		opD = 077 - opD;
		ppu.regA >>= opD;
	}
}

void Mpp::OpLMN(void)     // 11 LOGICAL MINUS No Address - XOR
{
	ppu.regA ^= opD;
}

void Mpp::OpLPN(void)     // 12  LOGICAL PRODUCT No Address - MASK / AND
{
	ppu.regA &= opD;
}

void Mpp::OpSCN(void)     // 13 SELECTIVE CLEAR No Address
{
	ppu.regA &= ~(opD & 077);
}

void Mpp::OpLDN(void)     // 14 LOAD No Address
{
	ppu.regA = opD;
}

void Mpp::OpLCN(void)     // 15 LOAD COMPLEMENT No Address
{
	ppu.regA = ~opD & Mask18;
}

void Mpp::OpADN(void)     // 16 ADD No Address
{
	ppu.regA = Add18(ppu.regA, opD);
}

void Mpp::OpSBN(void)     // 17 SUBTRACT No Address
{
	ppu.regA = Subtract18(ppu.regA, opD);
}

void Mpp::OpLDC(void)     // 20 LOAD Constant
{
	ppu.regA = (opD << 12) | (ppu.mem[ppu.regP] & Mask12);
	Increment(ppu.regP);
}

void Mpp::OpADC(void)     // 21 ADD Constant
{
	ppu.regA = Add18(ppu.regA, (opD << 12) | (ppu.mem[ppu.regP] & Mask12));
	Increment(ppu.regP);
}

void Mpp::OpLPC(void)     // 22 LOGICAL PRODUCT Constant
{
	ppu.regA &= (opD << 12) | (ppu.mem[ppu.regP] & Mask12);
	Increment(ppu.regP);
}

void Mpp::OpLMC(void)     // 23 LOGICAL MINUS Constant
{
	ppu.regA ^= (opD << 12) | (ppu.mem[ppu.regP] & Mask12);
	Increment(ppu.regP);
}

void Mpp::OpPSN24(void)     // 24
{
	if (opD != 0)
	{
		if ((features & HasRelocationRegShort) != 0)
		{
			/*
			**  LRD.
			*/
			//            ppu.regR  = (u32)(ppu.mem[opD    ] & Mask4 ) << 18; // 875
			ppu.regR = (u32)(ppu.mem[opD] & Mask3) << 18; // 865
			ppu.regR |= (u32)(ppu.mem[opD + 1] & Mask12) << 6;
		}
		else if ((features & HasRelocationRegLong) != 0)
		{
			/*
			**  LRD.
			*/
			ppu.regR = (u32)(ppu.mem[opD] & Mask10) << 18;
			ppu.regR |= (u32)(ppu.mem[opD + 1] & Mask12) << 6;
		}
	}

	/*
	**  Do nothing.
	*/
}

void Mpp::OpPSN25(void)     // 25
{
	if (opD != 0)
	{
		if ((features & HasRelocationRegShort) != 0)
		{
			/*
			**  SRD.
			*/
			//            ppu.mem[opD    ] = (PpWord)(ppu.regR >> 18) & Mask4; // 875
			ppu.mem[opD] = (PpWord)(ppu.regR >> 18) & Mask3; // 865
			ppu.mem[opD + 1] = (PpWord)(ppu.regR >> 6) & Mask12;
		}
		else if ((features & HasRelocationRegLong) != 0)
		{
			/*
			**  SRD.
			*/
			ppu.mem[opD] = (PpWord)(ppu.regR >> 18) & Mask10;
			ppu.mem[opD + 1] = (PpWord)(ppu.regR >> 6) & Mask12;
		}
	}

	/*
	**  Do nothing.
	*/
}

void Mpp::OpEXN(void)     // 26
{
	u32 exchangeAddress;
	int cpnum = opD & 007;
	int monitor = -1;
	int monitorx = 2; // no change to monitor status
	char sub[50];
	sprintf(sub, "EXN or MXN/MAN with CEJ/MEJ disabled");


	if (cpnum > BigIron->initCpus)
	{
		cpnum = 0;
	}

	MCpu *cpu = mfr->Acpu[cpnum]; //DRS

	if ((opD & 070) == 0 || (features & HasNoCejMej) != 0)
	{
		/*
		**  EXN or MXN/MAN with CEJ/MEJ disabled.
		*/
		if ((ppu.regA & Sign18) != 0 && (features & HasRelocationReg) != 0)
		{
			exchangeAddress = ppu.regR + (ppu.regA & Mask17);
			if ((features & HasRelocationRegShort) != 0)
			{
				exchangeAddress &= Mask18;
			}
		}
		else
		{
			exchangeAddress = ppu.regA & Mask18;
		}
	}
	else
	{
		if (mfr->monitorCpu > -1) // a cpu in moonitor
		{
			/*
			**  Pass.
			*/
			return;
		}

		if ((opD & 070) == 010)
		{
			/*
			**  MXN.
			*/
			sprintf(sub, "MXN");

				monitorx = cpu->cpu.CpuID; // this cpu to monitor mode

				if ((ppu.regA & Sign18) != 0 && (features & HasRelocationReg) != 0)
				{
					exchangeAddress = ppu.regR + (ppu.regA & Mask17);
					if ((features & HasRelocationRegShort) != 0)
					{
						exchangeAddress &= Mask18;
					}
				}
				else
				{
					exchangeAddress = ppu.regA & Mask18;
				}
		}
		else if ((opD & 070) == 020)
		{
			/*
			**  MAN.
			*/
			sprintf(sub, "MAN");

				monitorx = cpu->cpu.CpuID;

				exchangeAddress = cpu->cpu.regMa & Mask18;
		}
		else
		{
			/*
			**  Pass.
			*/
			return;
		}
	}

	/*
	**  Perform the exchange, but wait until the last parcel of the
	**  current instruction word has been executed.
	*/

	char xjSource[100];
	sprintf(xjSource, "EXN - %s PP %d", sub, ppu.id);

	while (!cpu->ExchangeJump(exchangeAddress, monitorx, xjSource))
	{
		cpu->Step();
	}
}

void Mpp::OpRPN(void)     // 27
{
	/*
	**  RPN except for series 800 (865 has it though).
	*/
	if ((features & IsSeries800) == 0 || BigIron->modelType == ModelCyber865)
	{

		MCpu *cpu = mfr->Acpu[opD & 07];  // DRS

		ppu.regA = cpu->GetP();
	}
}

void Mpp::OpLDD(void)     // 30 LOAD Direct
{
	ppu.regA = ppu.mem[opD] & Mask12;
	ppu.regA &= Mask18;
}

void Mpp::OpADD(void)     // 31 ADD Direct
{
	ppu.regA = Add18(ppu.regA, ppu.mem[opD] & Mask12);
}

void Mpp::OpSBD(void)     // 32 SUBTRACT Direct
{
	ppu.regA = Subtract18(ppu.regA, ppu.mem[opD] & Mask12);
}

void Mpp::OpLMD(void)     // 33 LOGICAL DIFFERENCE Direct 
{
	ppu.regA ^= ppu.mem[opD] & Mask12;
	ppu.regA &= Mask18;
}

void Mpp::OpSTD(void)     // 34 STORE Direct
{
	ppu.mem[opD] = (PpWord)ppu.regA & Mask12;
}

void Mpp::OpRAD(void)     // 35 REPLACE ADD Direct
{
	ppu.regA = Add18(ppu.regA, ppu.mem[opD] & Mask12);
	ppu.mem[opD] = (PpWord)ppu.regA & Mask12;
}

void Mpp::OpAOD(void)     // 36 REPLACE ADD ONE Direct
{
	ppu.regA = Add18(ppu.mem[opD] & Mask12, 1);
	ppu.mem[opD] = (PpWord)ppu.regA & Mask12;
}

void Mpp::OpSOD(void)     // 37 REPLACE SUBTRACT ONE Direct
{
	ppu.regA = Subtract18(ppu.mem[opD] & Mask12, 1);
	ppu.mem[opD] = (PpWord)ppu.regA & Mask12;
}

void Mpp::OpLDI(void)     // 40 LOAD Indirect
{
	location = ppu.mem[opD] & Mask12;
	ppu.regA = ppu.mem[location] & Mask12;
}

void Mpp::OpADI(void)     // 41 ADD Indirect
{
	location = ppu.mem[opD] & Mask12;
	ppu.regA = Add18(ppu.regA, ppu.mem[location] & Mask12);
}

void Mpp::OpSBI(void)     // 42 SUBTRACT Indirect
{
	location = ppu.mem[opD] & Mask12;
	ppu.regA = Subtract18(ppu.regA, ppu.mem[location] & Mask12);
}

void Mpp::OpLMI(void)     // 43 LOGICAL DIFFERENCE Indirect
{
	location = ppu.mem[opD] & Mask12;
	ppu.regA ^= ppu.mem[location] & Mask12;
	ppu.regA &= Mask18;
}

void Mpp::OpSTI(void)     // 44 STORE Indirect
{
	location = ppu.mem[opD] & Mask12;
	ppu.mem[location] = (PpWord)ppu.regA & Mask12;
}

void Mpp::OpRAI(void)     // 45 REPLACE ADD Indirect
{
	location = ppu.mem[opD] & Mask12;
	ppu.regA = Add18(ppu.regA, ppu.mem[location] & Mask12);
	ppu.mem[location] = (PpWord)ppu.regA & Mask12;
}

void Mpp::OpAOI(void)     // 46 REPLACE ADD ONE Indirect
{
	location = ppu.mem[opD] & Mask12;
	ppu.regA = Add18(ppu.mem[location] & Mask12, 1);
	ppu.mem[location] = (PpWord)ppu.regA & Mask12;
}

void Mpp::OpSOI(void)     // 47 REPLACE SUBTRACT ONE Indirect
{
	location = ppu.mem[opD] & Mask12;
	ppu.regA = Subtract18(ppu.mem[location] & Mask12, 1);
	ppu.mem[location] = (PpWord)ppu.regA & Mask12;
}

void Mpp::OpLDM(void)     // 50 LOAD Memory
{
	IndexLocation;
	ppu.regA = ppu.mem[location] & Mask12;
}

void Mpp::OpADM(void)     // 51 ADD Memory
{
	IndexLocation;
	ppu.regA = Add18(ppu.regA, ppu.mem[location] & Mask12);
}

void Mpp::OpSBM(void)     // 52 SUBTRACT Memory
{
	IndexLocation;
	ppu.regA = Subtract18(ppu.regA, ppu.mem[location] & Mask12);
}

void Mpp::OpLMM(void)     // 53 SUBTRACT Memory
{
	IndexLocation;
	ppu.regA ^= ppu.mem[location] & Mask12;
}

void Mpp::OpSTM(void)     // 54 SUBTRACT Memory
{
	IndexLocation;
	ppu.mem[location] = (PpWord)ppu.regA & Mask12;
}

void Mpp::OpRAM(void)     // 55 REPLACE ADD Memory
{
	IndexLocation;
	ppu.regA = Add18(ppu.regA, ppu.mem[location] & Mask12);
	ppu.mem[location] = (PpWord)ppu.regA & Mask12;
}

void Mpp::OpAOM(void)     // 56 REPLACE ADD ONE Memory
{
	IndexLocation;
	ppu.regA = Add18(ppu.mem[location] & Mask12, 1);
	ppu.mem[location] = (PpWord)ppu.regA & Mask12;
}

void Mpp::OpSOM(void)     // 57 REPLACE SUBTRACT ONE Memory
{
	IndexLocation;
	ppu.regA = Subtract18(ppu.mem[location] & Mask12, 1);
	ppu.mem[location] = (PpWord)ppu.regA & Mask12;
}

void Mpp::OpCRD(void)     // 60 CENTRAL READ DIRECT
{
	CpWord data;

	MCpu *cpu = mfr->Acpu[0];   

	if ((ppu.regA & Sign18) != 0 && (features & HasRelocationReg) != 0)
	{
		cpu->PpReadMem(ppu.regR + (ppu.regA & Mask17), &data);
	}
	else
	{
		cpu->PpReadMem(ppu.regA & Mask18, &data);
	}

	ppu.mem[opD++ & Mask12] = (PpWord)((data >> 48) & Mask12);
	ppu.mem[opD++ & Mask12] = (PpWord)((data >> 36) & Mask12);
	ppu.mem[opD++ & Mask12] = (PpWord)((data >> 24) & Mask12);
	ppu.mem[opD++ & Mask12] = (PpWord)((data >> 12) & Mask12);
	ppu.mem[opD   & Mask12] = (PpWord)((data)& Mask12);
}

void Mpp::OpCRM(void)     // 61 CENTRAL READ MEMORY
{
	CpWord data;
	MCpu *cpu = mfr->Acpu[0];	

	if (!ppu.busy)
	{
		ppu.opF = opF;
		ppu.regQ = ppu.mem[opD] & Mask12;

		ppu.busy = TRUE;

		ppu.mem[0] = ppu.regP;
		ppu.regP = ppu.mem[ppu.regP] & Mask12;
	}

	if (ppu.regQ--)
	{
		if ((ppu.regA & Sign18) != 0 && (features & HasRelocationReg) != 0)
		{
			cpu->PpReadMem(ppu.regR + (ppu.regA & Mask17), &data);
		}
		else
		{
			cpu->PpReadMem(ppu.regA & Mask18, &data);
		}

		ppu.mem[ppu.regP++ & Mask12] = (PpWord)((data >> 48) & Mask12);
		ppu.mem[ppu.regP++ & Mask12] = (PpWord)((data >> 36) & Mask12);
		ppu.mem[ppu.regP++ & Mask12] = (PpWord)((data >> 24) & Mask12);
		ppu.mem[ppu.regP++ & Mask12] = (PpWord)((data >> 12) & Mask12);
		ppu.mem[ppu.regP++ & Mask12] = (PpWord)((data)& Mask12);

		ppu.regA += 1;
		ppu.regA &= Mask18;
	}

	if (ppu.regQ == 0)
	{
		ppu.regP = ppu.mem[0];
		Increment(ppu.regP);
		ppu.busy = FALSE;
	}
}

void Mpp::OpCWD(void)     // 62 CENTRAL WRITE DIRECT
{
	CpWord data;
	MCpu *cpu = mfr->Acpu[0];

	data = ppu.mem[opD++ & Mask12] & Mask12;
	data <<= 12;

	data |= ppu.mem[opD++ & Mask12] & Mask12;
	data <<= 12;

	data |= ppu.mem[opD++ & Mask12] & Mask12;
	data <<= 12;

	data |= ppu.mem[opD++ & Mask12] & Mask12;
	data <<= 12;

	data |= ppu.mem[opD   & Mask12] & Mask12;

	if ((ppu.regA & Sign18) != 0 && (features & HasRelocationReg) != 0)
	{
		cpu->PpWriteMem(ppu.regR + (ppu.regA & Mask17), data);
	}
	else
	{
		cpu->PpWriteMem(ppu.regA & Mask18, data);
	}
}

void Mpp::OpCWM(void)     // 63 CENTRAL WRITE MEMORY
{
	CpWord data;
	MCpu *cpu = mfr->Acpu[0];

	if (!ppu.busy)
	{
		ppu.opF = opF;
		ppu.regQ = ppu.mem[opD] & Mask12;

		ppu.busy = TRUE;

		ppu.mem[0] = ppu.regP;
		ppu.regP = ppu.mem[ppu.regP] & Mask12;
	}

	if (ppu.regQ--)
	{
		data = ppu.mem[ppu.regP++ & Mask12] & Mask12;
		data <<= 12;

		data |= ppu.mem[ppu.regP++ & Mask12] & Mask12;
		data <<= 12;

		data |= ppu.mem[ppu.regP++ & Mask12] & Mask12;
		data <<= 12;

		data |= ppu.mem[ppu.regP++ & Mask12] & Mask12;
		data <<= 12;

		data |= ppu.mem[ppu.regP++ & Mask12] & Mask12;

		if ((ppu.regA & Sign18) != 0 && (features & HasRelocationReg) != 0)
		{
			cpu->PpWriteMem(ppu.regR + (ppu.regA & Mask17), data);
		}
		else
		{
			cpu->PpWriteMem(ppu.regA & Mask18, data);
		}

		ppu.regA += 1;
		ppu.regA &= Mask18;
	}

	if (ppu.regQ == 0)
	{
		ppu.regP = ppu.mem[0];
		Increment(ppu.regP);
		ppu.busy = FALSE;
	}
}

void Mpp::OpAJM(void)     // 64  Jump if Channel ACTIVE
{
	location = ppu.mem[ppu.regP];
	location &= Mask12;
	Increment(ppu.regP);

	if ((opD & 040) != 0
		&& (features & HasChannelFlag) != 0)
	{
		/*
		**  SCF.
		*/
		opD &= 037;
		if (opD < mfr->channelCount)
		{
			if (mfr->channel[opD].flag)
			{
				ppu.regP = location;
			}
			else
			{
				mfr->channel[opD].flag = TRUE;
			}
		}

		return;
	}

	opD &= 037;
	if (opD < mfr->channelCount)
	{
		activeChannel = mfr->channel + opD;
		channelCheckIfActive();
		if (activeChannel->active)
		{
			ppu.regP = location;
		}
	}

}

void Mpp::OpIJM(void)     // 65 Jump if Channel INACTIVE
{
	location = ppu.mem[ppu.regP];
	location &= Mask12;
	Increment(ppu.regP);

	if ((opD & 040) != 0
		&& (features & HasChannelFlag) != 0)
	{
		/*
		**  CCF.
		*/
		opD &= 037;
		if (opD < mfr->channelCount)
		{
			mfr->channel[opD].flag = FALSE;
		}

		return;
	}

	opD &= 037;
	if (opD >= mfr->channelCount)
	{
		ppu.regP = location;
	}
	else
	{
		activeChannel = mfr->channel + opD;
		channelCheckIfActive();
		if (!activeChannel->active)
		{
			ppu.regP = location;
		}
	}
}

void Mpp::OpFJM(void)     // 66 Jump if Channel FULL
{
	location = ppu.mem[ppu.regP];
	location &= Mask12;
	Increment(ppu.regP);

	if ((opD & 040) != 0
		&& (features & HasErrorFlag) != 0)
	{
		/*
		**  SFM - we never have errors, so this is just a pass.
		*/
		return;
	}

	opD &= 037;
	if (opD < mfr->channelCount)
	{
		activeChannel = mfr->channel + opD;
		channelIo();
		channelCheckIfFull();
		if (activeChannel->full)
		{
			ppu.regP = location;
		}
	}
}

void Mpp::OpEJM(void)     // 67 Jump if Channel EMPTY
{
	location = ppu.mem[ppu.regP];
	location &= Mask12;
	Increment(ppu.regP);

	if ((opD & 040) != 0
		&& (features & HasErrorFlag) != 0)
	{
		/*
		**  CFM - we never have errors, so we always jump.
		*/
		opD &= 037;
		if (opD < mfr->channelCount)
		{
			ppu.regP = location;
		}

		return;
	}

	opD &= 037;
	if (opD >= mfr->channelCount)
	{
		ppu.regP = location;
	}
	else
	{
		activeChannel = mfr->channel + opD;
		channelIo();
		channelCheckIfFull();
		if (!activeChannel->full)
		{
			ppu.regP = location;
		}
	}
}

void Mpp::OpIAN(void)     // 70 Input one word to A
{
	if (!ppu.busy)
	{
		ppu.opF = opF;
		ppu.opD = opD;
		activeChannel->delayStatus = 0;
	}

	noHang = (ppu.opD & 040) != 0;
	activeChannel = mfr->channel + (ppu.opD & 037);
	ppu.busy = TRUE;

	channelCheckIfActive();
	if (!activeChannel->active && activeChannel->id != ChClock)
	{
		if (noHang)
		{
			ppu.regA = 0;
			ppu.busy = FALSE;
		}

		return;
	}

	channelCheckIfFull();
	if (!activeChannel->full)
	{
		/*
		**  Handle possible input.
		*/
		channelIo();
	}

	if (activeChannel->full || activeChannel->id == ChClock)
	{
		/*
		**  Handle input (note that the clock channel has always data pending,
		**  but appears full on some models, empty on others).
		*/
		channelIn();
		channelSetEmpty();
		ppu.regA = activeChannel->data & Mask12;
		activeChannel->inputPending = FALSE;
		if (activeChannel->discAfterInput)
		{
			activeChannel->discAfterInput = FALSE;
			activeChannel->delayDisconnect = 0;
			activeChannel->active = FALSE;
			activeChannel->ioDevice = NULL;
		}

		ppu.busy = FALSE;
	}
}

void Mpp::OpIAM(void)     // 71 Input (A) words to (m)
{
	if (!ppu.busy)
	{
		ppu.opF = opF;
		ppu.opD = opD;

		activeChannel = mfr->channel + (ppu.opD & 037);
		ppu.busy = TRUE;

		ppu.mem[0] = ppu.regP;
		ppu.regP = ppu.mem[ppu.regP] & Mask12;
		activeChannel->delayStatus = 0;
	}
	else
	{
		activeChannel = mfr->channel + (ppu.opD & 037);
	}

	channelCheckIfActive();
	if (!activeChannel->active)
	{
		/*
		**  Disconnect device except for hardwired devices.
		*/
		if (!activeChannel->hardwired)
		{
			activeChannel->ioDevice = NULL;
		}

		/*
		**  Channel becomes empty (must not call channelSetEmpty(), otherwise we
		**  get a spurious empty pulse).
		*/
		activeChannel->full = FALSE;

		/*
		**  Terminate transfer and set next location to zero.
		*/
		ppu.mem[ppu.regP] = 0;
		ppu.regP = ppu.mem[0];
		Increment(ppu.regP);
		ppu.busy = FALSE;
		return;
	}

	channelCheckIfFull();
	if (!activeChannel->full)
	{
		/*
		**  Handle possible input.
		*/
		channelIo();
	}

	if (activeChannel->full || activeChannel->id == ChClock)
	{
		/*
		**  Handle input (note that the clock channel has always data pending,
		**  but appears full on some models, empty on others).
		*/
		channelIn();
		channelSetEmpty();
		ppu.mem[ppu.regP] = activeChannel->data & Mask12;
		ppu.regP = (ppu.regP + 1) & Mask12;
		ppu.regA = (ppu.regA - 1) & Mask18;
		activeChannel->inputPending = FALSE;

		if (activeChannel->discAfterInput)
		{
			activeChannel->discAfterInput = FALSE;
			activeChannel->delayDisconnect = 0;
			activeChannel->active = FALSE;
			activeChannel->ioDevice = NULL;
			if (ppu.regA != 0)
			{
				ppu.mem[ppu.regP] = 0;
			}
			ppu.regP = ppu.mem[0];
			Increment(ppu.regP);
			ppu.busy = FALSE;
		}
		else if (ppu.regA == 0)
		{
			ppu.regP = ppu.mem[0];
			Increment(ppu.regP);
			ppu.busy = FALSE;
		}
	}
}

void Mpp::OpOAN(void)     // 72 Output one word from A
{
	if (!ppu.busy)
	{
		ppu.opF = opF;
		ppu.opD = opD;
		activeChannel->delayStatus = 0;
	}

	noHang = (ppu.opD & 040) != 0;
	activeChannel = mfr->channel + (ppu.opD & 037);
	ppu.busy = TRUE;

	channelCheckIfActive();
	if (!activeChannel->active)
	{
		if (noHang)
		{
			ppu.busy = FALSE;
		}
		return;
	}

	channelCheckIfFull();
	if (!activeChannel->full)
	{
		activeChannel->data = (PpWord)ppu.regA & Mask12;
		channelOut();
		channelSetFull();
		ppu.busy = FALSE;
	}

	/*
	**  Handle possible output.
	*/
	channelIo();
}

void Mpp::OpOAM(void)     // 73 Output (A) words from (m)
{
	if (!ppu.busy)
	{
		ppu.opF = opF;
		ppu.opD = opD;

		activeChannel = mfr->channel + (ppu.opD & 037);
		ppu.busy = TRUE;

		ppu.mem[0] = ppu.regP;
		ppu.regP = ppu.mem[ppu.regP] & Mask12;
		activeChannel->delayStatus = 0;
	}
	else
	{
		activeChannel = mfr->channel + (ppu.opD & 037);
	}

	channelCheckIfActive();
	if (!activeChannel->active)
	{
		/*
		**  Disconnect device except for hardwired devices.
		*/
		if (!activeChannel->hardwired)
		{
			activeChannel->ioDevice = NULL;
		}

		/*
		**  Channel becomes empty (must not call channelSetEmpty(), otherwise we
		**  get a spurious empty pulse).
		*/
		activeChannel->full = FALSE;

		/*
		**  Terminate transfer.
		*/
		ppu.regP = ppu.mem[0];
		Increment(ppu.regP);
		ppu.busy = FALSE;
		return;
	}

	channelCheckIfFull();
	if (!activeChannel->full)
	{
		activeChannel->data = ppu.mem[ppu.regP] & Mask12;
		ppu.regP = (ppu.regP + 1) & Mask12;
		ppu.regA = (ppu.regA - 1) & Mask18;
		channelOut();
		channelSetFull();

		if (ppu.regA == 0)
		{
			ppu.regP = ppu.mem[0];
			Increment(ppu.regP);
			ppu.busy = FALSE;
		}
	}

	/*
	**  Handle possible output.
	*/
	channelIo();
}

void Mpp::OpACN(void)     // 74 ACTIVATE Channel
{
	if (!ppu.busy)
	{
		ppu.opF = opF;
		ppu.opD = opD;
	}

	noHang = (ppu.opD & 040) != 0;
	activeChannel = mfr->channel + (ppu.opD & 037);

	channelCheckIfActive();
	if (activeChannel->active)
	{
		if (!noHang)
		{
			ppu.busy = TRUE;
		}
		return;
	}

	channelActivate();
	ppu.busy = FALSE;
}

void Mpp::OpDCN(void)     // 75 DEACTIVATE Channel
{
	if (!ppu.busy)
	{
		ppu.opF = opF;
		ppu.opD = opD;
	}

	noHang = (ppu.opD & 040) != 0;
	activeChannel = mfr->channel + (ppu.opD & 037);

	/*
	**  RTC, Interlock and S/C register channel can not be deactivated.
	*/
	if (activeChannel->id == ChClock)
	{
		return;
	}

	if (activeChannel->id == ChInterlock && (features & HasInterlockReg) != 0)
	{
		return;
	}

	if (activeChannel->id == ChStatusAndControl && (features & HasStatusAndControlReg) != 0)
	{
		return;
	}

	channelCheckIfActive();
	if (!activeChannel->active)
	{
		if (!noHang)
		{
			ppu.busy = TRUE;
		}
		return;
	}

	channelDisconnect();
	ppu.busy = FALSE;
}

void Mpp::OpFAN(void)     // 76 Function from A
{
	if (!ppu.busy)
	{
		ppu.opF = opF;
		ppu.opD = opD;
	}

	noHang = (ppu.opD & 040) != 0;
	activeChannel = mfr->channel + (ppu.opD & 037);

	/*
	**  Interlock register channel ignores functions.
	*/
	if (activeChannel->id == ChInterlock && (features & HasInterlockReg) != 0)
	{
		return;
	}

	channelCheckIfActive();
	if (activeChannel->active)
	{
		if (!noHang)
		{
			ppu.busy = TRUE;
		}
		return;
	}

	channelFunction((PpWord)(ppu.regA & Mask12));
	ppu.busy = FALSE;
}

void Mpp::OpFNC(void)     // 77 Function from m
{
	if (!ppu.busy)
	{
		ppu.opF = opF;
		ppu.opD = opD;
	}

	noHang = (ppu.opD & 040) != 0;
	activeChannel = mfr->channel + (ppu.opD & 037);

	/*
	**  Interlock register channel ignores functions.
	*/
	if (activeChannel->id == ChInterlock && (features & HasInterlockReg) != 0)
	{
		return;
	}

	channelCheckIfActive();
	if (activeChannel->active)
	{
		if (!noHang)
		{
			ppu.busy = TRUE;
		}
		return;
	}

	channelFunction((PpWord)(ppu.mem[ppu.regP] & Mask12));
	Increment(ppu.regP);
	ppu.busy = FALSE;
}



/*---------------------------  End Of File  ------------------------------*/


