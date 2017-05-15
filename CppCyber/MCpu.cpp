/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2011, Tom Hunter
**  C++ adaptation by Dale Sinder 2017
**
**  Name: MCpu.cpp
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

/*
**  -----------------
**  Constants
**  -----------------
*/

/* Only enable this for testing to pass section 4.A of EJT (divide break-in test) */
#define CcSMM_EJT               0

/*
**  CPU exit conditions.
*/
#define EcNone                  00
#define EcAddressOutOfRange     01
#define EcOperandOutOfRange     02
#define EcIndefiniteOperand     04

/*
**  ECS bank size taking into account the 5k reserve.
*/
#define EcsBankSize             (131072 - 5120)
#define EsmBankSize             131072

// Global vars


MCpu::MCpu()
{
	fprintf(stdout, "Must use two ARG MCpu Constructor.\n");
	exit(1);
}


//
MCpu::MCpu(u8 id, u8 mfrID)
{
	if (id > 1)
	{
		fprintf(stdout, "Too large a CPU ID.\n");
		exit(1);
	}
	if (BigIron->chasis[mfrID]->cpuCnt > MaxCpus - 1)
	{
		fprintf(stdout, "Too many CPUs.\n");
		exit(1);
	}

	floatException = FALSE;

	BigIron->chasis[mfrID]->cpuCnt++;

	cpu.CpuID = id;
}

MCpu::~MCpu()
{
}

/*
**--------------------------------------------------------------------------
**
**  Public Functions
**
**--------------------------------------------------------------------------
*/


void MCpu::InitM(char *model, MMainFrame *mainfr)
{
	cpu.cpuStopped = TRUE;
	cpu.regP = 0;
	mfr = mainfr;
	cpMem = mfr->cpMem;
	cpuMaxMemory = mfr->cpuMaxMemory;
	extMem = BigIron->extMem;
	extMaxMemory = BigIron->extMaxMemory;
	mainFrameID = mfr->mainFrameID;

	/*
	**  Print a friendly message.
	*/
	if (cpu.CpuID == 0)
		printf("CPU model %s initialised \n", model);
#if MaxCpus == 2
	else
		printf("Running with 2 CPUs\n");
#endif 

}


/*--------------------------------------------------------------------------
**  Purpose:        Terminate CPU and optionally persist CM.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void MCpu::Terminate(void)
{
	/*
	**  Optionally save CM.
	*/

	if (mfr->cmHandle != NULL)
	{
		fseek(mfr->cmHandle, 0, SEEK_SET);
		if (fwrite(cpMem, sizeof(CpWord), cpuMaxMemory, mfr->cmHandle) != cpuMaxMemory)
		{
			fprintf(stderr, "Error writing CM backing file\n");
		}

		fclose(mfr->cmHandle);
	}

	/*
	**  Free allocated memory.
	*/
	free(cpMem);
}


/*--------------------------------------------------------------------------
**  Purpose:        Return CPU P register.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
u32 MCpu::GetP(void)
{
	return((cpu.regP) & Mask18);
}

/*--------------------------------------------------------------------------
**  Purpose:        Read CPU memory from PP and verify that address is
**                  within limits.
**
**  Parameters:     Name        Description.
**                  address     Absolute CM address to read.
**                  data        Pointer to 60 bit word which gets the data.
**
**  Returns:        Nothing
**
**------------------------------------------------------------------------*/
void MCpu::PpReadMem(u32 address, CpWord *data)
{
	if ((features & HasNoCmWrap) != 0)
	{
		if (address < cpuMaxMemory)
		{
			*data = cpMem[address] & Mask60;
		}
		else
		{
			*data = (~((CpWord)0)) & Mask60;
		}
	}
	else
	{
		address %= cpuMaxMemory;
		*data = cpMem[address] & Mask60;
	}
}

/*--------------------------------------------------------------------------
**  Purpose:        Write CPU memory from PP and verify that address is
**                  within limits.
**
**  Parameters:     Name        Description.
**                  address     Absolute CM address
**                  data        60 bit word which holds the data to be written.
**
**  Returns:        Nothing
**
**------------------------------------------------------------------------*/
void MCpu::PpWriteMem(u32 address, CpWord data)
{
	if ((features & HasNoCmWrap) != 0)
	{
		if (address < cpuMaxMemory)
		{
			cpMem[address] = data & Mask60;
		}
	}
	else
	{
		address %= cpuMaxMemory;
		cpMem[address] = data & Mask60;
	}
}

/*--------------------------------------------------------------------------
**  Purpose:        Perform exchange jump.
**
**  Parameters:     Name        Description.
**                  addr        Exchange jump address.
**
**  Returns:        TRUE if exchange jump can be performed, FALSE otherwise.
**
**------------------------------------------------------------------------*/
bool MCpu::ExchangeJump(u32 addr, int monitorx, char *xjSource)
{
	CpuContext tmp;
	CpWord *mem;

	/*
	**  Only perform exchange jump on instruction boundary or when stopped.
	*/
	if (opOffset != 60 && !cpu.cpuStopped)
	{
		return(FALSE);
	}

#if CcDebug == 1
	traceExchange(this, addr, "Old", xjSource);
#endif

	/*
	**  Clear any spurious address bits.
	*/
	addr &= Mask18;

	if (addr == 0)
	{
		char stuff[100];
		sprintf(stuff, "\nExchange package addr CPU %d is zero!!\n", cpu.CpuID);
		printf(stuff);
#if CcDebug == 1
		traceCpuPrint(this, stuff);
		//  abort and dump
		opActive = FALSE;
		BigIron->emulationActive = FALSE;
#endif
	}
#if MaxCpus ==2
	if (BigIron->initCpus > 1)
	{
		RESERVE1(&BigIron->chasis[mainFrameID]->XJMutex);
	}
#endif
	if (monitorx < 2)
	{
		if (monitorx == -1 && mfr->monitorCpu == cpu.CpuID)	// clear only if i am monitored
		{
			mfr->monitorCpu = monitorx;
		}
		else if (mfr->monitorCpu == -1) // monitor me if none monitored
		{
			mfr->monitorCpu = cpu.CpuID;
		}
		else
		{
#if MaxCpus ==2
			if (BigIron->initCpus > 1)
			{
				RELEASE1(&mfr->XJMutex);
			}
#endif
			//printf("\nMonitor %d rejected on cpu %d from Source: %s\n", monitorx, cpu.CpuID, xjSource);
			return (FALSE);		// reject
		}
	}
#if MaxCpus ==2
	if (BigIron->initCpus > 1)
	{
		RELEASE1(&mfr->XJMutex);
	}
#endif
	/*
	**  Verify if exchange package is within configured memory.
	*/
	if (addr + 020 >= cpuMaxMemory)
	{
		/*
		**  Pretend that exchange worked, but the address is bad.
		*/
#if MaxCpus == 2
		if (BigIron->initCpus > 1)	// tell any waiting thread it can run now 
			WakeConditionVariable(&mfr->XJDone);
#endif	

		printf("\nXJ addr outside cpuMaxMemory\n");
		return(TRUE);
	}

	/*
	**  Save current context.
	*/
	tmp = cpu;

	/*
	**  Setup new context.
	*/
	mem = cpMem + addr;

	cpu.regP = (u32)((*mem >> 36) & Mask18);
	cpu.regA[0] = (u32)((*mem >> 18) & Mask18);
	cpu.regB[0] = 0;

	mem += 1;
	cpu.regRaCm = (u32)((*mem >> 36) & Mask24);
	cpu.regA[1] = (u32)((*mem >> 18) & Mask18);
	cpu.regB[1] = (u32)((*mem) & Mask18);

	mem += 1;
	cpu.regFlCm = (u32)((*mem >> 36) & Mask24);
	cpu.regA[2] = (u32)((*mem >> 18) & Mask18);
	cpu.regB[2] = (u32)((*mem) & Mask18);

	mem += 1;
	cpu.exitMode = (u32)((*mem >> 36) & Mask24);
	cpu.regA[3] = (u32)((*mem >> 18) & Mask18);
	cpu.regB[3] = (u32)((*mem) & Mask18);

	mem += 1;
	if ((features & IsSeries800) != 0
		&& (cpu.exitMode & EmFlagExpandedAddress) != 0)
	{
		cpu.regRaEcs = (u32)((*mem >> 30) & Mask30Ecs);
	}
	else
	{
		cpu.regRaEcs = (u32)((*mem >> 36) & Mask24Ecs);
	}

	cpu.regA[4] = (u32)((*mem >> 18) & Mask18);
	cpu.regB[4] = (u32)((*mem) & Mask18);

	mem += 1;
	if ((features & IsSeries800) != 0
		&& (cpu.exitMode & EmFlagExpandedAddress) != 0)
	{
		cpu.regFlEcs = (u32)((*mem >> 30) & Mask30Ecs);
	}
	else
	{
		cpu.regFlEcs = (u32)((*mem >> 36) & Mask24Ecs);
	}

	cpu.regA[5] = (u32)((*mem >> 18) & Mask18);
	cpu.regB[5] = (u32)((*mem) & Mask18);

	mem += 1;
	cpu.regMa = (u32)((*mem >> 36) & Mask24);
	cpu.regA[6] = (u32)((*mem >> 18) & Mask18);
	cpu.regB[6] = (u32)((*mem) & Mask18);

	mem += 1;
	cpu.regSpare = (u32)((*mem >> 36) & Mask24);
	cpu.regA[7] = (u32)((*mem >> 18) & Mask18);
	cpu.regB[7] = (u32)((*mem) & Mask18);

	mem += 1;
	cpu.regX[0] = *mem++ & Mask60;
	cpu.regX[1] = *mem++ & Mask60;
	cpu.regX[2] = *mem++ & Mask60;
	cpu.regX[3] = *mem++ & Mask60;
	cpu.regX[4] = *mem++ & Mask60;
	cpu.regX[5] = *mem++ & Mask60;
	cpu.regX[6] = *mem++ & Mask60;
	cpu.regX[7] = *mem++ & Mask60;

	cpu.exitCondition = EcNone;

#if CcDebug == 1
	traceExchange(this, addr, "New", xjSource);
	if (monitorx == -1 && cpu.regMa == 0)
	{
		char mess[100];
		sprintf(mess, "\nExiting monitor mode CPU %d  with MA = zero\n\n", this->cpu.CpuID);
		//printf("\nExiting monitor mode CPU %d with MA = zero\n\n", this->cpu.CpuID);
		traceCpuPrint(this, mess);
	}
#endif

	/*
	**  Save old context.
	*/
	mem = cpMem + addr;

	*mem++ = ((CpWord)(tmp.regP     & Mask18) << 36) | ((CpWord)(tmp.regA[0] & Mask18) << 18);
	*mem++ = ((CpWord)(tmp.regRaCm  & Mask24) << 36) | ((CpWord)(tmp.regA[1] & Mask18) << 18) | ((CpWord)(tmp.regB[1] & Mask18));
	*mem++ = ((CpWord)(tmp.regFlCm  & Mask24) << 36) | ((CpWord)(tmp.regA[2] & Mask18) << 18) | ((CpWord)(tmp.regB[2] & Mask18));
	*mem++ = ((CpWord)(tmp.exitMode & Mask24) << 36) | ((CpWord)(tmp.regA[3] & Mask18) << 18) | ((CpWord)(tmp.regB[3] & Mask18));

	if ((features & IsSeries800) != 0
		&& (tmp.exitMode & EmFlagExpandedAddress) != 0)
	{
		*mem++ = ((CpWord)(tmp.regRaEcs & Mask30Ecs) << 30) | ((CpWord)(tmp.regA[4] & Mask18) << 18) | ((CpWord)(tmp.regB[4] & Mask18));
	}
	else
	{
		*mem++ = ((CpWord)(tmp.regRaEcs & Mask24Ecs) << 36) | ((CpWord)(tmp.regA[4] & Mask18) << 18) | ((CpWord)(tmp.regB[4] & Mask18));
	}

	if ((features & IsSeries800) != 0
		&& (tmp.exitMode & EmFlagExpandedAddress) != 0)
	{
		*mem++ = ((CpWord)(tmp.regFlEcs & Mask30Ecs) << 30) | ((CpWord)(tmp.regA[5] & Mask18) << 18) | ((CpWord)(tmp.regB[5] & Mask18));
	}
	else
	{
		*mem++ = ((CpWord)(tmp.regFlEcs & Mask24Ecs) << 36) | ((CpWord)(tmp.regA[5] & Mask18) << 18) | ((CpWord)(tmp.regB[5] & Mask18));
	}

	*mem++ = ((CpWord)(tmp.regMa    & Mask24) << 36) | ((CpWord)(tmp.regA[6] & Mask18) << 18) | ((CpWord)(tmp.regB[6] & Mask18));
	*mem++ = ((CpWord)(tmp.regSpare & Mask24) << 36) | ((CpWord)(tmp.regA[7] & Mask18) << 18) | ((CpWord)(tmp.regB[7] & Mask18));
	*mem++ = tmp.regX[0] & Mask60;
	*mem++ = tmp.regX[1] & Mask60;
	*mem++ = tmp.regX[2] & Mask60;
	*mem++ = tmp.regX[3] & Mask60;
	*mem++ = tmp.regX[4] & Mask60;
	*mem++ = tmp.regX[5] & Mask60;
	*mem++ = tmp.regX[6] & Mask60;
	*mem++ = tmp.regX[7] & Mask60;

	if ((features & HasInstructionStack) != 0)
	{
		/*
		**  Void the instruction stack.
		*/
		VoidIwStack(~0);
	}

	/*
	**  Activate CPU.
	*/

	cpu.cpuStopped = FALSE;
	FetchOpWord(cpu.regP, &opWord);

#if MaxCpus == 2
	if (BigIron->initCpus > 1)	// tell waiting thread (if any) it can XJ now
		WakeConditionVariable(&mfr->XJDone);
#endif

//////////////////////////


	// From Paul Koning code

	CpWord t;
	t = opWord;
	/*
	**  Check for the idle loop.  Usually that's just an "eq *" but in recent
	**  flavors of NOS it's a few Cxi instructions then "eq *".  If we see
	**  the idle loop, pretend the CPU is stopped.  That way we don't spend
	**  time emulating the idle loop instructions, which will speed up other
	**  stuff (such as the PPUs and their I/O) if the CPU is idle.
	*/
	while ((t >> 54) == 047)
	{
		t = (t << 15) & Mask60;
	}
	if ((t >> 30) == (00400000000 | cpu.regP))
	{
		cpu.cpuStopped = TRUE;
	}


//////////////////////////////

	return(TRUE);
}


/*--------------------------------------------------------------------------
**  Purpose:        Execute next instruction in the CPU.
**
**  Parameters:     Name        Description.
**
**  Returns:        true if stopped
**
**------------------------------------------------------------------------*/
bool MCpu::Step(void)
{
	if (cpu.cpuStopped)
	{
		return true;
	}

#if CcSMM_EJT
	if (skipStep != 0)
	{
		skipStep -= 1;
		return true;
	}
#endif

	/*
	**  Execute one CM word atomically.
	*/

	do
	{
		/*
		**  Decode based on type.
		*/
		opFm = (u8)((opWord >> (opOffset - 6)) & Mask6);

		opI = (u8)((opWord >> (opOffset - 9)) & Mask3);
		opJ = (u8)((opWord >> (opOffset - 12)) & Mask3);
		opLength = (u8)decodeCpuOpcode[opFm].length;

		if (opLength == 0)
		{
			opLength = cpOp01Length[opI];
		}

		if (opLength == 15)
		{
			opK = (u8)((opWord >> (opOffset - 15)) & Mask3);
			opAddress = 0;

			opOffset -= 15;
		}
		else
		{
			if (opOffset == 15)
			{
				/*
				**  Invalid packing is handled as illegal instruction.
				*/
				OpIllegal("Invalid packing");
				return true;
			}

			opK = 0;
			opAddress = (u32)((opWord >> (opOffset - 30)) & Mask18);

			opOffset -= 30;
		}

		oldRegP = cpu.regP;

		/*
		**  Force B0 to 0.
		*/

		cpu.regB[0] = 0;

		/*
		**  Execute instruction.
		*/
		CALL_MEMBER_FN(*this, decodeCpuOpcode[opFm].execute)();

		/*
		**  Force B0 to 0.
		*/
		cpu.regB[0] = 0;

#if CcDebug == 1
		traceCpu(this, oldRegP, opFm, opI, opJ, opK, opAddress);
#endif

		if (cpu.cpuStopped)
		{
			if (opOffset == 0)
			{
				cpu.regP = (cpu.regP + 1) & Mask18;
			}
//#if CcDebug == 1
			//char reason[100];
			//sprintf(reason, "Stopped RegP=%o\n", cpu.regP);
			//traceCpuPrint(this, reason);
//#endif
			return true;
		}

		/*
		**  Fetch next instruction word if necessary.
		*/
		if (opOffset == 0)
		{
			cpu.regP = (cpu.regP + 1) & Mask18;
			FetchOpWord(cpu.regP, &opWord);
		}
	} while (opOffset != 60);
	return false;
}

/*--------------------------------------------------------------------------
**  Purpose:        Perform ECS flag register operation.
**
**  Parameters:     Name        Description.
**                  ecsAddress  ECS address (flag register function and data)
**
**  Returns:        TRUE if accepted, FALSE otherwise.
**
**------------------------------------------------------------------------*/
bool MCpu::EcsFlagRegister(u32 ecsAddress)
{
	u32 flagFunction = (ecsAddress >> 21) & Mask3;
	u32 flagWord = ecsAddress & Mask18;
#if MaxMainFrames == 2 || MaxCpus == 2
	if (flagFunction != 6)
	{
		if (BigIron->initCpus > 1 || BigIron->initMainFrames > 1)
		{
			RESERVE1(&BigIron->ECSFlagMutex);
		}
	}
#endif
	switch (flagFunction)
	{
	case 4:
		/*
		**  Ready/Select.
		*/
		if ((BigIron->ecsFlagRegister & flagWord) != 0)
		{
			/*
			**  Error exit.
			*/
#if MaxMainFrames == 2 || MaxCpus == 2
			if (BigIron->initCpus > 1 || BigIron->initMainFrames > 1)
			{
				RELEASE1(&BigIron->ECSFlagMutex);
			}
#endif
			return(FALSE);
		}

		BigIron->ecsFlagRegister |= flagWord;

		break;

	case 5:
		/*
		**  Selective set.
		*/
		BigIron->ecsFlagRegister |= flagWord;
		break;

	case 6:
		/*
		**  Status.
		*/
		if ((BigIron->ecsFlagRegister & flagWord) != 0)
		{
			/*
			**  Error exit.
			*/

			return(FALSE);
		}

		break;

	case 7:
		/*
		**  Selective clear,
		*/
		BigIron->ecsFlagRegister = (BigIron->ecsFlagRegister & ~flagWord) & Mask18;
		break;
	default: 
		OpIllegal("EcsFlagRegister");
		break;
	}
#if MaxMainFrames == 2 || MaxCpus == 2
	if (flagFunction != 6)
	{
		if (BigIron->initCpus > 1 || BigIron->initMainFrames > 1)
		{
			RELEASE1(&BigIron->ECSFlagMutex);
		}
	}
#endif
	/*
	**  Normal exit.
	*/
	return(TRUE);
}


/*
**--------------------------------------------------------------------------
**
**  Private Functions
**
**--------------------------------------------------------------------------
*/

/*--------------------------------------------------------------------------
**  Purpose:        Handle illegal instruction
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing
**
**------------------------------------------------------------------------*/
void MCpu::OpIllegal(char * from)
{
	cpu.cpuStopped = TRUE;
	if (cpu.regRaCm < cpuMaxMemory)
	{
		cpMem[cpu.regRaCm] = ((CpWord)cpu.exitCondition << 48) | ((CpWord)(cpu.regP + 1) << 30);
	}

	cpu.regP = 0;

	if ((features & (HasNoCejMej | IsSeries6x00)) == 0 && (mfr->monitorCpu == -1))
	{
		/*
		**  Exchange jump to MA.
		*/

		char xjSource[200];
		sprintf(xjSource, "OpIllegal - From %s", from);

		ExchangeJump(cpu.regMa, cpu.CpuID, xjSource);
	}
}

/*--------------------------------------------------------------------------
**  Purpose:        Check if CPU instruction word address is within limits.
**
**  Parameters:     Name        Description.
**                  address     RA relative address to read.
**                  location    Pointer to u32 which will contain absolute address.
**
**  Returns:        TRUE if validation failed, FALSE otherwise;
**
**------------------------------------------------------------------------*/
bool MCpu::CheckOpAddress(u32 address, u32 *location)
{
	/*
	**  Calculate absolute address.
	*/
	*location = AddRa(address);

	if (address >= cpu.regFlCm || (*location >= cpuMaxMemory && (features & HasNoCmWrap) != 0))
	{
		/*
		**  Exit mode is always selected for RNI or branch.
		*/
		cpu.cpuStopped = TRUE;

		cpu.exitCondition |= EcAddressOutOfRange;
		if (cpu.regRaCm < cpuMaxMemory)
		{
			// not need for RNI or branch - how about other uses?
			if ((cpu.exitMode & EmAddressOutOfRange) != 0)
			{
				cpMem[cpu.regRaCm] = ((CpWord)cpu.exitCondition << 48) | ((CpWord)(cpu.regP) << 30);
			}
		}

		cpu.regP = 0;
		if ((features & (HasNoCejMej | IsSeries6x00)) == 0 && (mfr->monitorCpu == -1))
		{
			/*
			**  Exchange jump to MA.
			*/
			ExchangeJump(cpu.regMa, cpu.CpuID, "CheckOpAddress");
		}

		return(TRUE);
	}

	/*
	**  Calculate absolute address with wraparound.
	*/

	*location %= cpuMaxMemory;

	return(FALSE);
}

/*--------------------------------------------------------------------------
**  Purpose:        Read CPU instruction word and verify that address is
**                  within limits.
**
**  Parameters:     Name        Description.
**                  address     RA relative address to read.
**                  data        Pointer to 60 bit word which gets the data.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void MCpu::FetchOpWord(u32 address, CpWord *data)
{
	u32 location;

	if (CheckOpAddress(address, &location))
	{
		return;
	}

	if ((features & HasInstructionStack) != 0)
	{
		int i;

		/*
		**  Check if instruction word is in stack.
		**  (optimise this by starting search from last match).
		*/
		for (i = 0; i < MaxIwStack; i++)
		{
			if (cpu.iwValid[i] && cpu.iwAddress[i] == location)
			{
				*data = cpu.iwStack[i];
				break;
			}
		}

		if (i == MaxIwStack)
		{
			/*
			**  No hit, fetch the instruction from CM and enter it into the stack.
			*/
			cpu.iwRank = (cpu.iwRank + 1) % MaxIwStack;
			cpu.iwAddress[cpu.iwRank] = location;
			cpu.iwStack[cpu.iwRank] = cpMem[location] & Mask60;
			cpu.iwValid[cpu.iwRank] = TRUE;
			*data = cpu.iwStack[cpu.iwRank];
		}

		if ((features & HasIStackPrefetch) != 0 && (i == MaxIwStack || i == cpu.iwRank))
		{
#if 0
			/*
			**  Prefetch two instruction words. <<<< _two_ appears to be too greedy and causes FL problems >>>>
			*/
			for (i = 2; i > 0; i--)
			{
				address += 1;
				if (CheckOpAddress(address, &location))
				{
					return;
				}

				cpu.iwRank = (cpu.iwRank + 1) % MaxIwStack;
				cpu.iwAddress[cpu.iwRank] = location;
				cpu.iwStack[cpu.iwRank] = cpMem[location] & Mask60;
				cpu.iwValid[cpu.iwRank] = TRUE;
			}
#else
			/*
			**  Prefetch one instruction word.
			*/
			address += 1;
			if (CheckOpAddress(address, &location))
			{
				return;
			}

			cpu.iwRank = (cpu.iwRank + 1) % MaxIwStack;
			cpu.iwAddress[cpu.iwRank] = location;
			cpu.iwStack[cpu.iwRank] = cpMem[location] & Mask60;
			cpu.iwValid[cpu.iwRank] = TRUE;
#endif
		}
	}
	else
	{
		/*
		**  Fetch the instruction from CM.
		*/
		*data = cpMem[location] & Mask60;
	}

	opOffset = 60;

	return;
}

/*--------------------------------------------------------------------------
**  Purpose:        Void the instruction stack unless branch target is
**                  within stack (or unconditionally if address is ~0).
**
**  Parameters:     Name        Description.
**                  branchAddr  Target location for a branch or ~0 for
**                              unconditional voiding.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void MCpu::VoidIwStack(u32 branchAddr)
{
	u32 location;
	int i;

	if (branchAddr != ~0)
	{
		location = AddRa(branchAddr);

		for (i = 0; i < MaxIwStack; i++)
		{
			if (cpu.iwValid[i] && cpu.iwAddress[i] == location)
			{
				/*
				**  Branch target is within stack - do nothing.
				*/
				return;
			}
		}
	}

	/*
	**  Branch target is NOT within stack or unconditional voiding required.
	*/
	for (i = 0; i < MaxIwStack; i++)
	{
		cpu.iwValid[i] = FALSE;
	}

	cpu.iwRank = 0;
}

/*--------------------------------------------------------------------------
**  Purpose:        Read CPU memory and verify that address is within limits.
**
**  Parameters:     Name        Description.
**                  address     RA relative address to read.
**                  data        Pointer to 60 bit word which gets the data.
**
**  Returns:        TRUE if access failed, FALSE otherwise;
**
**------------------------------------------------------------------------*/
bool MCpu::ReadMem(u32 address, CpWord *data)
{
	u32 location;

	if (address >= cpu.regFlCm)
	{
		cpu.exitCondition |= EcAddressOutOfRange;

		/*
		**  Clear the data.
		*/
		*data = 0;

		if ((cpu.exitMode & EmAddressOutOfRange) != 0)
		{
			/*
			**  Exit mode selected.
			*/
			cpu.cpuStopped = TRUE;

			if (cpu.regRaCm < cpuMaxMemory)
			{
				cpMem[cpu.regRaCm] = ((CpWord)cpu.exitCondition << 48) | ((CpWord)(cpu.regP + 1) << 30);
			}

			cpu.regP = 0;

			if ((features & IsSeries170) == 0)
			{
				/*
				**  All except series 170 clear the data.
				*/
				*data = 0;
			}

			if ((features & (HasNoCejMej | IsSeries6x00)) == 0 && (mfr->monitorCpu == -1))
			{
				/*
				**  Exchange jump to MA.
				*/
				ExchangeJump(cpu.regMa, cpu.CpuID, "ReadMem");
			}
			return(TRUE);
		}

		return(FALSE);
	}

	/*
	**  Calculate absolute address.
	*/
	location = AddRa(address);

	/*
	**  Wrap around or fail gracefully if wrap around is disabled.
	*/
	if (location >= cpuMaxMemory)
	{
		if ((features & HasNoCmWrap) != 0)
		{
			*data = (~((CpWord)0)) & Mask60;
			return(FALSE);
		}

		location %= cpuMaxMemory;
	}

	/*
	**  Fetch the data.
	*/
	*data = cpMem[location] & Mask60;

	return(FALSE);
}

/*--------------------------------------------------------------------------
**  Purpose:        Write CPU memory and verify that address is within limits.
**
**  Parameters:     Name        Description.
**                  address     RA relative address to write.
**                  data        Pointer to 60 bit word which holds the data.
**
**  Returns:        TRUE if access failed, FALSE otherwise;
**
**------------------------------------------------------------------------*/
bool MCpu::WriteMem(u32 address, CpWord *data)
{
	u32 location;

	if (address >= cpu.regFlCm)
	{
		cpu.exitCondition |= EcAddressOutOfRange;

		if ((cpu.exitMode & EmAddressOutOfRange) != 0)
		{
			/*
			**  Exit mode selected.
			*/
			cpu.cpuStopped = TRUE;

			if (cpu.regRaCm < cpuMaxMemory)
			{
				cpMem[cpu.regRaCm] = ((CpWord)cpu.exitCondition << 48) | ((CpWord)(cpu.regP + 1) << 30);
			}

			cpu.regP = 0;

			if ((features & (HasNoCejMej | IsSeries6x00)) == 0 && (mfr->monitorCpu == -1))
			{
				/*
				**  Exchange jump to MA.
				*/
				ExchangeJump(cpu.regMa, cpu.CpuID, "WriteMem");
			}

			return(TRUE);
		}

		return(FALSE);
	}

	/*
	**  Calculate absolute address.
	*/
	location = AddRa(address);

	/*
	**  Wrap around or fail gracefully if wrap around is disabled.
	*/
	if (location >= cpuMaxMemory)
	{
		if ((features & HasNoCmWrap) != 0)
		{
			return(FALSE);
		}

		location %= cpuMaxMemory;
	}

	/*
	**  Store the data.
	*/
	cpMem[location] = *data & Mask60;

	return(FALSE);
}

/*--------------------------------------------------------------------------
**  Purpose:        Implement A register sematics.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void MCpu::RegASemantics(void)
{
	if (opI == 0)
	{
		return;
	}

	if (opI <= 5)
	{
		/*
		**  Read semantics.
		*/
		ReadMem(cpu.regA[opI], cpu.regX + opI);
	}
	else
	{
		/*
		**  Write semantics.
		*/
		if ((cpu.exitMode & EmFlagStackPurge) != 0)
		{
			/*
			**  Instruction stack purge flag is set - do an
			**  unconditional void.
			*/
			VoidIwStack(~0);
		}

		WriteMem(cpu.regA[opI], cpu.regX + opI);
	}
}

/*--------------------------------------------------------------------------
**  Purpose:        Addition of 18 or 21 bit RA and 18 bit offset in
**                  ones-complement with subtractive adder
**
**  Parameters:     Name        Description.
**                  op          18 bit offset
**
**  Returns:        18 or 21 bit result.
**
**------------------------------------------------------------------------*/
u32 MCpu::AddRa(u32 op)
{
	if ((features & IsSeries800) != 0)
	{
		acc21 = (cpu.regRaCm & Mask21) - (~op & Mask21);
		if ((acc21 & Overflow21) != 0)
		{
			acc21 -= 1;
		}

		return(acc21 & Mask21);
	}

	acc18 = (cpu.regRaCm & Mask18) - (~op & Mask18);
	if ((acc18 & Overflow18) != 0)
	{
		acc18 -= 1;
	}

	return(acc18 & Mask18);
}

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
u32 MCpu::Add18(u32 op1, u32 op2)
{
	acc18 = (op1 & Mask18) - (~op2 & Mask18);
	if ((acc18 & Overflow18) != 0)
	{
		acc18 -= 1;
	}

	return(acc18 & Mask18);
}

/*--------------------------------------------------------------------------
**  Purpose:        24 bit ones-complement addition with subtractive adder
**
**  Parameters:     Name        Description.
**                  op1         24 bit operand1
**                  op2         24 bit operand2
**
**  Returns:        24 bit result.
**
**------------------------------------------------------------------------*/
u32 MCpu::Add24(u32 op1, u32 op2)
{
	acc24 = (op1 & Mask24) - (~op2 & Mask24);
	if ((acc24 & Overflow24) != 0)
	{
		acc24 -= 1;
	}

	return(acc24 & Mask24);
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
u32 MCpu::Subtract18(u32 op1, u32 op2)
{
	acc18 = (op1 & Mask18) - (op2 & Mask18);
	if ((acc18 & Overflow18) != 0)
	{
		acc18 -= 1;
	}

	return(acc18 & Mask18);
}

/*--------------------------------------------------------------------------
**  Purpose:        Transfer word to/from UEM initiated by a CPU instruction.
**
**  Parameters:     Name        Description.
**                  writeToUem  TRUE if this is a write to UEM, FALSE if
**                              this is a read.
**
**  Returns:        Nothing
**
**------------------------------------------------------------------------*/
void MCpu::UemWord(bool writeToUem)
{
	u32 uemAddress;

	/*
	**  Calculate source or destination addresses.
	*/
	uemAddress = (u32)(cpu.regX[opK] & Mask24);

	/*
	**  Check for UEM range.
	*/
	if (cpu.regFlEcs <= uemAddress)
	{
		cpu.exitCondition |= EcAddressOutOfRange;
		if ((cpu.exitMode & EmAddressOutOfRange) != 0)
		{
			/*
			**  Exit mode selected.
			*/
			cpu.cpuStopped = TRUE;

			if (cpu.regRaCm < cpuMaxMemory)
			{
				cpMem[cpu.regRaCm] = ((CpWord)cpu.exitCondition << 48) | ((CpWord)(cpu.regP + 1) << 30);
			}

			cpu.regP = 0;

			if ((features & (HasNoCejMej | IsSeries6x00)) == 0 && (mfr->monitorCpu == -1))
			{
				/*
				**  Exchange jump to MA.
				*/
				ExchangeJump(cpu.regMa, cpu.CpuID,  "UemWord");
			}
		}

		return;
	}

	/*
	**  Add base address.
	*/
	uemAddress += cpu.regRaEcs;

	/*
	**  Perform the transfer.
	*/
	if (writeToUem)
	{
		if (uemAddress < cpuMaxMemory && (uemAddress & (3 << 21)) == 0)
		{
			cpMem[uemAddress++] = cpu.regX[opJ] & Mask60;
		}
	}
	else
	{
		if (uemAddress >= cpuMaxMemory || (uemAddress & (3 << 21)) != 0)
		{
			/*
			**  If bits 21 or 22 are non-zero, zero Xj.
			*/
			cpu.regX[opJ] = 0;
		}
		else
		{
			cpu.regX[opJ] = cpMem[uemAddress] & Mask60;
		}
	}
}

/*--------------------------------------------------------------------------
**  Purpose:        Transfer word to/from ECS initiated by a CPU instruction.
**
**  Parameters:     Name        Description.
**                  writeToEcs  TRUE if this is a write to ECS, FALSE if
**                              this is a read.
**
**  Returns:        Nothing
**
**------------------------------------------------------------------------*/
void MCpu::EcsWord(bool writeToEcs)
{
	u32 ecsAddress;

	/*
	**  ECS must exist.
	*/
	if (extMaxMemory == 0)
	{
		OpIllegal("EcsWord");
		return;
	}

	ecsAddress = (u32)(cpu.regX[opK] & Mask24);

	/*
	**  Check for ECS range.
	*/
	if (cpu.regFlEcs <= ecsAddress)
	{
		cpu.exitCondition |= EcAddressOutOfRange;
		if ((cpu.exitMode & EmAddressOutOfRange) != 0)
		{
			/*
			**  Exit mode selected.
			*/
			cpu.cpuStopped = TRUE;

			if (cpu.regRaCm < cpuMaxMemory)
			{
				cpMem[cpu.regRaCm] = ((CpWord)cpu.exitCondition << 48) | ((CpWord)(cpu.regP + 1) << 30);
			}

			cpu.regP = 0;

			if ((features & (HasNoCejMej | IsSeries6x00)) == 0 && (mfr->monitorCpu == -1))
			{
				/*
				**  Exchange jump to MA.
				*/
				ExchangeJump(cpu.regMa, cpu.CpuID, "EcsWord");
			}
		}

		return;
	}

	/*
	**  Add base address.
	*/
	ecsAddress += cpu.regRaEcs;

	/*
	**  Perform the transfer.
	*/
	if (writeToEcs)
	{
		if (ecsAddress < extMaxMemory)
		{
			extMem[ecsAddress++] = cpu.regX[opJ] & Mask60;
		}
	}
	else
	{
		if (ecsAddress >= extMaxMemory)
		{
			/*
			**  Zero Xj.
			*/
			cpu.regX[opJ] = 0;
		}
		else
		{
			cpu.regX[opJ] = extMem[ecsAddress++] & Mask60;
		}
	}
}

/*--------------------------------------------------------------------------
**  Purpose:        Transfer block to/from UEM initiated by a CPU instruction.
**
**  Parameters:     Name        Description.
**                  writeToUem  TRUE if this is a write to UEM, FALSE if
**                              this is a read.
**
**  Returns:        Nothing
**
**------------------------------------------------------------------------*/
void MCpu::UemTransfer(bool writeToUem)
{
	u32 wordCount;
	u32 uemAddress;
	u32 cmAddress;

	/*
	**  Instruction must be located in the upper 30 bits.
	*/
	if (opOffset != 30)
	{
		OpIllegal("UemTransfer");
		return;
	}

	/*
	**  Calculate word count, source and destination addresses.
	*/
	wordCount = Add18(cpu.regB[opJ], opAddress);
	uemAddress = (u32)(cpu.regX[0] & Mask30);

	if ((cpu.exitMode & EmFlagEnhancedBlockCopy) != 0)
	{
		cmAddress = (u32)((cpu.regX[0] >> 30) & Mask21);
	}
	else
	{
		cmAddress = cpu.regA[0] & Mask18;
	}

	/*
	**  Deal with possible negative zero word count.
	*/
	if (wordCount == Mask18)
	{
		wordCount = 0;
	}

	/*
	**  Check for positive word count, CM and UEM range.
	*/
	if ((wordCount & Sign18) != 0
		|| cpu.regFlCm  < cmAddress + wordCount
		|| cpu.regFlEcs < uemAddress + wordCount)
	{
		cpu.exitCondition |= EcAddressOutOfRange;
		if ((cpu.exitMode & EmAddressOutOfRange) != 0)
		{
			/*
			**  Exit mode selected.
			*/
			cpu.cpuStopped = TRUE;

			if (cpu.regRaCm < cpuMaxMemory)
			{
				cpMem[cpu.regRaCm] = ((CpWord)cpu.exitCondition << 48) | ((CpWord)(cpu.regP + 1) << 30);
			}

			cpu.regP = 0;

			if ((features & (HasNoCejMej | IsSeries6x00)) == 0 && (mfr->monitorCpu == -1))
			{
				/*
				**  Exchange jump to MA.
				*/
				ExchangeJump(cpu.regMa, cpu.CpuID, "UemTransfer");
			}
		}
		else
		{
			cpu.regP = (cpu.regP + 1) & Mask18;
			FetchOpWord(cpu.regP, &opWord);
		}

		return;
	}

	/*
	**  Add base addresses.
	*/
	cmAddress = AddRa(cmAddress);
	cmAddress %= cpuMaxMemory;

	uemAddress += cpu.regRaEcs;

	/*
	**  Perform the transfer.
	*/
	if (writeToUem)
	{
		while (wordCount--)
		{
			if (uemAddress >= cpuMaxMemory || (uemAddress & (3 << 21)) != 0)
			{
				/*
				**  If bits 21 or 22 are non-zero, error exit to lower
				**  30 bits of instruction word.
				*/
				return;
			}

			cpMem[uemAddress++] = cpMem[cmAddress] & Mask60;

			/*
			**  Increment CM address.
			*/
			cmAddress = Add24(cmAddress, 1);
			cmAddress %= cpuMaxMemory;
		}
	}
	else
	{
		bool takeErrorExit = FALSE;

		while (wordCount--)
		{
			if (uemAddress >= cpuMaxMemory || (uemAddress & (3 << 21)) != 0)
			{
				/*
				**  If bits 21 or 22 are non-zero, zero CM, but take error exit
				**  to lower 30 bits once zeroing is finished.
				>>>>>>>>>>>> manual says to only do this when the condition is true on instruction start <<<<<<<<<<<<<<<<
				>>>>>>>>>>>> NOS 2 now works by specifiying an address > cpuMaxMemory with bit 24 set?!? <<<<<<<<<<<<<<<<
				>>>>>>>>>>>> Maybe the manual is wrong about bits 21/22 and it should be bit 24 instead? <<<<<<<<<<<<<<<<
				*/
				cpMem[cmAddress] = 0;
				takeErrorExit = TRUE;
			}
			else
			{
				cpMem[cmAddress] = cpMem[uemAddress++] & Mask60;
			}

			/*
			**  Increment CM address.
			*/
			cmAddress = Add24(cmAddress, 1);
			cmAddress %= cpuMaxMemory;
		}

		if (takeErrorExit)
		{
			/*
			**  Error exit to lower 30 bits of instruction word.
			*/
			return;
		}
	}

	/*
	**  Normal exit to next instruction word.
	*/
	cpu.regP = (cpu.regP + 1) & Mask18;
	FetchOpWord(cpu.regP, &opWord);
}

/*--------------------------------------------------------------------------
**  Purpose:        Transfer block to/from ECS initiated by a CPU instruction.
**
**  Parameters:     Name        Description.
**                  writeToEcs  TRUE if this is a write to ECS, FALSE if
**                              this is a read.
**
**  Returns:        Nothing
**
**------------------------------------------------------------------------*/
void MCpu::EcsTransfer(bool writeToEcs)
{
	u32 wordCount;
	u32 ecsAddress;
	u32 cmAddress;

	/*
	**  ECS must exist and instruction must be located in the upper 30 bits.
	*/
	if (extMaxMemory == 0 || opOffset != 30)
	{
		OpIllegal("EcsTransfer");
		return;
	}

	/*
	**  Calculate word count, source and destination addresses.
	*/
	wordCount = Add18(cpu.regB[opJ], opAddress);
	ecsAddress = (u32)(cpu.regX[0] & Mask24);

	if ((cpu.exitMode & EmFlagEnhancedBlockCopy) != 0)
	{
		cmAddress = (u32)((cpu.regX[0] >> 30) & Mask24);
	}
	else
	{
		cmAddress = cpu.regA[0] & Mask18;
	}

	/*
	**  Check if this is a flag register access.
	**
	**  The ECS book (60225100) says that a flag register reference occurs
	**  when bit 23 is set in the relative address AND in the ECS FL.
	**
	**  Note that the ECS RA is NOT added to the relative address.
	*/
	if ((ecsAddress   & ((u32)1 << 23)) != 0
		&& (cpu.regFlEcs & ((u32)1 << 23)) != 0)
	{
		if (!EcsFlagRegister(ecsAddress))
		{
			return;
		}

		/*
		**  Normal exit.
		*/
		cpu.regP = (cpu.regP + 1) & Mask18;
		FetchOpWord(cpu.regP, &opWord);
		return;
	}

	/*
	**  Deal with possible negative zero word count.
	*/
	if (wordCount == Mask18)
	{
		wordCount = 0;
	}

	/*
	**  Check for positive word count, CM and ECS range.
	*/
	if ((wordCount & Sign18) != 0
		|| cpu.regFlCm  < cmAddress + wordCount
		|| cpu.regFlEcs < ecsAddress + wordCount)
	{
		cpu.exitCondition |= EcAddressOutOfRange;
		if ((cpu.exitMode & EmAddressOutOfRange) != 0)
		{
			/*
			**  Exit mode selected.
			*/
			cpu.cpuStopped = TRUE;

			if (cpu.regRaCm < cpuMaxMemory)
			{
				cpMem[cpu.regRaCm] = ((CpWord)cpu.exitCondition << 48) | ((CpWord)(cpu.regP + 1) << 30);
			}

			cpu.regP = 0;

			if ((features & (HasNoCejMej | IsSeries6x00)) == 0 && (mfr->monitorCpu == -1))
			{
				/*
				**  Exchange jump to MA.
				*/
				ExchangeJump(cpu.regMa, cpu.CpuID, "EcsTransfer");
			}
		}
		else
		{
			cpu.regP = (cpu.regP + 1) & Mask18;
			FetchOpWord(cpu.regP, &opWord);
		}

		return;
	}

	/*
	**  Add base addresses.
	*/
	cmAddress = AddRa(cmAddress);
	cmAddress %= cpuMaxMemory;

	ecsAddress += cpu.regRaEcs;

	/*
	**  Perform the transfer.
	*/
	if (writeToEcs)
	{
		while (wordCount--)
		{
			if (ecsAddress >= extMaxMemory)
			{
				/*
				**  Error exit to lower 30 bits of instruction word.
				*/
				return;
			}

			extMem[ecsAddress++] = cpMem[cmAddress] & Mask60;

			/*
			**  Increment CM address.
			*/
			cmAddress = Add24(cmAddress, 1);
			cmAddress %= cpuMaxMemory;
		}
	}
	else
	{
		bool takeErrorExit = FALSE;

		while (wordCount--)
		{
			if (ecsAddress >= extMaxMemory)
			{
				/*
				**  Zero CM, but take error exit to lower 30 bits once zeroing is finished.
				*/
				cpMem[cmAddress] = 0;
				takeErrorExit = TRUE;
			}
			else
			{
				cpMem[cmAddress] = extMem[ecsAddress++] & Mask60;
			}

			/*
			**  Increment CM address.
			*/
			cmAddress = Add24(cmAddress, 1);
			cmAddress %= cpuMaxMemory;
		}

		if (takeErrorExit)
		{
			/*
			**  Error exit to lower 30 bits of instruction word.
			*/
			return;
		}
	}

	/*
	**  Normal exit to next instruction word.
	*/
	cpu.regP = (cpu.regP + 1) & Mask18;
	FetchOpWord(cpu.regP, &opWord);
}

/*--------------------------------------------------------------------------
**  Purpose:        CMU get a byte.
**
**  Parameters:     Name        Description.
**                  address     CM word address
**                  pos         character position
**                  byte        pointer to byte
**
**  Returns:        TRUE if access failed, FALSE otherwise.
**
**------------------------------------------------------------------------*/
bool MCpu::CmuGetByte(u32 address, u32 pos, u8 *byte)
{
	u32 location;
	CpWord data;

	/*
	**  Validate access.
	*/
	if (address >= cpu.regFlCm || cpu.regRaCm + address >= cpuMaxMemory)
	{
		cpu.exitCondition |= EcAddressOutOfRange;
		if ((cpu.exitMode & EmAddressOutOfRange) != 0)
		{
			/*
			**  Exit mode selected.
			*/
			cpu.cpuStopped = TRUE;

			if (cpu.regRaCm < cpuMaxMemory)
			{
				cpMem[cpu.regRaCm] = ((CpWord)cpu.exitCondition << 48) | ((CpWord)(cpu.regP + 1) << 30);
			}

			cpu.regP = 0;

			if ((features & (HasNoCejMej | IsSeries6x00)) == 0 && (mfr->monitorCpu == -1))
			{
				/*
				**  Exchange jump to MA.
				*/
				ExchangeJump(cpu.regMa, cpu.CpuID, "CmuGetByte");
			}
		}

		return(TRUE);
	}

	/*
	**  Calculate absolute address with wraparound.
	*/
	location = AddRa(address);
	location %= cpuMaxMemory;

	/*
	**  Fetch the word.
	*/
	data = cpMem[location] & Mask60;

	/*
	**  Extract and return the byte.
	*/
	*byte = (u8)((data >> ((9 - pos) * 6)) & Mask6);

	return(FALSE);
}

/*--------------------------------------------------------------------------
**  Purpose:        CMU put a byte.
**
**  Parameters:     Name        Description.
**                  address     CM word address
**                  pos         character position
**                  byte        data byte to put
**
**  Returns:        TRUE if access failed, FALSE otherwise.
**
**------------------------------------------------------------------------*/
bool MCpu::CmuPutByte(u32 address, u32 pos, u8 byte)
{
	u32 location;
	CpWord data;

	/*
	**  Validate access.
	*/
	if (address >= cpu.regFlCm || cpu.regRaCm + address >= cpuMaxMemory)
	{
		cpu.exitCondition |= EcAddressOutOfRange;
		if ((cpu.exitMode & EmAddressOutOfRange) != 0)
		{
			/*
			**  Exit mode selected.
			*/
			cpu.cpuStopped = TRUE;

			if (cpu.regRaCm < cpuMaxMemory)
			{
				cpMem[cpu.regRaCm] = ((CpWord)cpu.exitCondition << 48) | ((CpWord)(cpu.regP + 1) << 30);
			}

			cpu.regP = 0;

			if ((features & (HasNoCejMej | IsSeries6x00)) == 0 && (mfr->monitorCpu == -1))
			{
				/*
				**  Exchange jump to MA.
				*/
				ExchangeJump(cpu.regMa, cpu.CpuID, "CmuPutByte");
			}
		}

		return(TRUE);
	}

	/*
	**  Calculate absolute address with wraparound.
	*/
	location = AddRa(address);
	location %= cpuMaxMemory;

	/*
	**  Fetch the word.
	*/
	data = cpMem[location] & Mask60;

	/*
	**  Mask the destination position.
	*/
	data &= ~(((CpWord)Mask6) << ((9 - pos) * 6));

	/*
	**  Store byte into position
	*/
	data |= (((CpWord)byte) << ((9 - pos) * 6));

	/*
	**  Store the word.
	*/
	cpMem[location] = data & Mask60;

	return(FALSE);
}

/*--------------------------------------------------------------------------
**  Purpose:        CMU move indirect.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing
**
**------------------------------------------------------------------------*/
void MCpu::CmuMoveIndirect(void)
{
	CpWord descWord;
	u32 k1, k2;
	u32 c1, c2;
	u32 ll;
	u8 byte;
	bool failed;

	//<<<<<<<<<<<<<<<<<<<<<<<< don't forget to optimise c1 == c2 cases.

	/*
	**  Fetch the descriptor word.
	*/
	opAddress = (u32)((opWord >> 30) & Mask18);
	opAddress = Add18(cpu.regB[opJ], opAddress);
	failed = ReadMem(opAddress, &descWord);
	if (failed)
	{
		return;
	}

	/*
	**  Decode descriptor word.
	*/
	k1 = (u32)(descWord >> 30) & Mask18;
	k2 = (u32)(descWord >> 0) & Mask18;
	c1 = (u32)(descWord >> 22) & Mask4;
	c2 = (u32)(descWord >> 18) & Mask4;
	ll = (u32)((descWord >> 26) & Mask4) | (u32)((descWord >> (48 - 4)) & (Mask9 << 4));

	/*
	**  Check for address out of range.
	*/
	if (c1 > 9 || c2 > 9)
	{
		cpu.exitCondition |= EcAddressOutOfRange;
		if ((cpu.exitMode & EmAddressOutOfRange) != 0)
		{
			/*
			**  Exit mode selected.
			*/
			cpu.cpuStopped = TRUE;

			if (cpu.regRaCm < cpuMaxMemory)
			{
				cpMem[cpu.regRaCm] = ((CpWord)cpu.exitCondition << 48) | ((CpWord)(cpu.regP + 1) << 30);
			}

			cpu.regP = 0;

			if ((features & (HasNoCejMej | IsSeries6x00)) == 0 && (mfr->monitorCpu == -1))
			{
				/*
				**  Exchange jump to MA.
				*/
				ExchangeJump(cpu.regMa, cpu.CpuID, "CmuMoveIndirect");
			}
		}

		/*
		**  No transfer.
		*/
		ll = 0;
	}

	/*
	**  Perform the actual move.
	*/
	while (ll--)
	{
		/*
		**  Transfer one byte, but abort if access fails.
		*/
		if (CmuGetByte(k1, c1, &byte)
			|| CmuPutByte(k2, c2, byte))
		{
			if (cpu.cpuStopped) //????????????????????????
			{
				return;
			}

			/*
			**  Exit to next instruction.
			*/
			break;
		}

		/*
		**  Advance addresses.
		*/
		if (++c1 > 9)
		{
			c1 = 0;
			k1 += 1;
		}

		if (++c2 > 9)
		{
			c2 = 0;
			k2 += 1;
		}
	}

	/*
	**  Clear register X0 after the move.
	*/
	cpu.regX[0] = 0;

	/*
	**  Normal exit to next instruction word.
	*/
	cpu.regP = (cpu.regP + 1) & Mask18;
	FetchOpWord(cpu.regP, &opWord);
}

/*--------------------------------------------------------------------------
**  Purpose:        CMU move direct.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing
**
**------------------------------------------------------------------------*/
void MCpu::CmuMoveDirect(void)
{
	u32 k1, k2;
	u32 c1, c2;
	u32 ll;
	u8 byte;

	//<<<<<<<<<<<<<<<<<<<<<<<< don't forget to optimise c1 == c2 cases.

	/*
	**  Decode opcode word.
	*/
	k1 = (u32)(opWord >> 30) & Mask18;
	k2 = (u32)(opWord >> 0) & Mask18;
	c1 = (u32)(opWord >> 22) & Mask4;
	c2 = (u32)(opWord >> 18) & Mask4;
	ll = (u32)((opWord >> 26) & Mask4) | (u32)((opWord >> (48 - 4)) & (Mask3 << 4));

	/*
	**  Check for address out of range.
	*/
	if (c1 > 9 || c2 > 9)
	{
		cpu.exitCondition |= EcAddressOutOfRange;
		if ((cpu.exitMode & EmAddressOutOfRange) != 0)
		{
			/*
			**  Exit mode selected.
			*/
			cpu.cpuStopped = TRUE;

			if (cpu.regRaCm < cpuMaxMemory)
			{
				cpMem[cpu.regRaCm] = ((CpWord)cpu.exitCondition << 48) | ((CpWord)(cpu.regP + 1) << 30);
			}

			cpu.regP = 0;

			if ((features & (HasNoCejMej | IsSeries6x00)) == 0 && (mfr->monitorCpu == -1))
			{
				/*
				**  Exchange jump to MA.
				*/
				ExchangeJump(cpu.regMa, cpu.CpuID, "CmuMoveDirect");
			}
			return;
		}

		/*
		**  No transfer.
		*/
		ll = 0;
	}

	/*
	**  Perform the actual move.
	*/
	while (ll--)
	{
		/*
		**  Transfer one byte, but abort if access fails.
		*/
		if (CmuGetByte(k1, c1, &byte)
			|| CmuPutByte(k2, c2, byte))
		{
			if (cpu.cpuStopped) //?????????????????????
			{
				return;
			}

			/*
			**  Exit to next instruction.
			*/
			break;
		}

		/*
		**  Advance addresses.
		*/
		if (++c1 > 9)
		{
			c1 = 0;
			k1 += 1;
		}

		if (++c2 > 9)
		{
			c2 = 0;
			k2 += 1;
		}
	}

	/*
	**  Clear register X0 after the move.
	*/
	cpu.regX[0] = 0;

	/*
	**  Normal exit to next instruction word.
	*/
	cpu.regP = (cpu.regP + 1) & Mask18;
	FetchOpWord(cpu.regP, &opWord);
}

/*--------------------------------------------------------------------------
**  Purpose:        CMU compare collated.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing
**
**------------------------------------------------------------------------*/
void MCpu::CmuCompareCollated(void)
{
	CpWord result = 0;
	u32 k1, k2;
	u32 c1, c2;
	u32 ll;
	u32 collTable;
	u8 byte1, byte2;

	/*
	**  Decode opcode word.
	*/
	k1 = (u32)(opWord >> 30) & Mask18;
	k2 = (u32)(opWord >> 0) & Mask18;
	c1 = (u32)(opWord >> 22) & Mask4;
	c2 = (u32)(opWord >> 18) & Mask4;
	ll = (u32)((opWord >> 26) & Mask4) | (u32)((opWord >> (48 - 4)) & (Mask3 << 4));

	/*
	**  Setup collating table.
	*/
	collTable = cpu.regA[0];

	/*
	**  Check for addresses and collTable out of range.
	*/
	if (c1 > 9 || c2 > 9 || collTable >= cpu.regFlCm || cpu.regRaCm + collTable >= cpuMaxMemory)
	{
		cpu.exitCondition |= EcAddressOutOfRange;
		if ((cpu.exitMode & EmAddressOutOfRange) != 0)
		{
			/*
			**  Exit mode selected.
			*/
			cpu.cpuStopped = TRUE;

			if (cpu.regRaCm < cpuMaxMemory)
			{
				cpMem[cpu.regRaCm] = ((CpWord)cpu.exitCondition << 48) | ((CpWord)(cpu.regP + 1) << 30);
			}

			cpu.regP = 0;

			if ((features & (HasNoCejMej | IsSeries6x00)) == 0 && (mfr->monitorCpu == -1))
			{
				/*
				**  Exchange jump to MA.
				*/
				ExchangeJump(cpu.regMa, cpu.CpuID, "CmuCompareCollated");
			}
			return;
		}

		/*
		**  No compare.
		*/
		ll = 0;
	}

	/*
	**  Perform the actual compare.
	*/
	while (ll--)
	{
		/*
		**  Check the two bytes raw.
		*/
		if (CmuGetByte(k1, c1, &byte1)
			|| CmuGetByte(k2, c2, &byte2))
		{
			if (cpu.cpuStopped) //?????????????????????
			{
				return;
			}

			/*
			**  Exit to next instruction.
			*/
			break;
		}

		if (byte1 != byte2)
		{
			/*
			**  Bytes differ - check using collating table.
			*/
			if (CmuGetByte(collTable + ((byte1 >> 3) & Mask3), byte1 & Mask3, &byte1)
				|| CmuGetByte(collTable + ((byte2 >> 3) & Mask3), byte2 & Mask3, &byte2))
			{
				if (cpu.cpuStopped) //??????????????????????
				{
					return;
				}

				/*
				**  Exit to next instruction.
				*/
				break;
			}

			if (byte1 != byte2)
			{
				/*
				**  Bytes differ in their collating sequence as well - terminate comparision
				**  and calculate result.
				*/
				result = ll + 1;
				if (byte1 < byte2)
				{
					result = ~result & Mask60;
				}

				break;
			}
		}

		/*
		**  Advance addresses.
		*/
		if (++c1 > 9)
		{
			c1 = 0;
			k1 += 1;
		}

		if (++c2 > 9)
		{
			c2 = 0;
			k2 += 1;
		}
	}

	/*
	**  Store result in X0.
	*/
	cpu.regX[0] = result;

	/*
	**  Normal exit to next instruction word.
	*/
	cpu.regP = (cpu.regP + 1) & Mask18;
	FetchOpWord(cpu.regP, &opWord);
}

/*--------------------------------------------------------------------------
**  Purpose:        CMU compare uncollated.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing
**
**------------------------------------------------------------------------*/
void MCpu::CmuCompareUncollated(void)
{
	CpWord result = 0;
	u32 k1, k2;
	u32 c1, c2;
	u32 ll;
	u8 byte1, byte2;

	/*
	**  Decode opcode word.
	*/
	k1 = (u32)(opWord >> 30) & Mask18;
	k2 = (u32)(opWord >> 0) & Mask18;
	c1 = (u32)(opWord >> 22) & Mask4;
	c2 = (u32)(opWord >> 18) & Mask4;
	ll = (u32)((opWord >> 26) & Mask4) | (u32)((opWord >> (48 - 4)) & (Mask3 << 4));

	/*
	**  Check for address out of range.
	*/
	if (c1 > 9 || c2 > 9)
	{
		cpu.exitCondition |= EcAddressOutOfRange;
		if ((cpu.exitMode & EmAddressOutOfRange) != 0)
		{
			/*
			**  Exit mode selected.
			*/
			cpu.cpuStopped = TRUE;

			if (cpu.regRaCm < cpuMaxMemory)
			{
				cpMem[cpu.regRaCm] = ((CpWord)cpu.exitCondition << 48) | ((CpWord)(cpu.regP + 1) << 30);
			}

			cpu.regP = 0;

			if ((features & (HasNoCejMej | IsSeries6x00)) == 0 && (mfr->monitorCpu == -1))
			{
				/*
				**  Exchange jump to MA.
				*/
				ExchangeJump(cpu.regMa, cpu.CpuID, "CmuCompareUncollated");
			}
			return;
		}

		/*
		**  No compare.
		*/
		ll = 0;
	}

	/*
	**  Perform the actual compare.
	*/
	while (ll--)
	{
		/*
		**  Check the two bytes raw.
		*/
		if (CmuGetByte(k1, c1, &byte1)
			|| CmuGetByte(k2, c2, &byte2))
		{
			if (cpu.cpuStopped) //?????????????????
			{
				return;
			}

			/*
			**  Exit to next instruction.
			*/
			break;
		}

		if (byte1 != byte2)
		{
			/*
			**  Bytes differ - terminate comparision
			**  and calculate result.
			*/
			result = ll + 1;
			if (byte1 < byte2)
			{
				result = ~result & Mask60;
			}

			break;
		}

		/*
		**  Advance addresses.
		*/
		if (++c1 > 9)
		{
			c1 = 0;
			k1 += 1;
		}

		if (++c2 > 9)
		{
			c2 = 0;
			k2 += 1;
		}
	}

	/*
	**  Store result in X0.
	*/
	cpu.regX[0] = result;

	/*
	**  Normal exit to next instruction word.
	*/
	cpu.regP = (cpu.regP + 1) & Mask18;
	FetchOpWord(cpu.regP, &opWord);
}

/*--------------------------------------------------------------------------
**  Purpose:        Check parameter for floating point infinite and
**                  indefinite and set exit condition accordingly.
**
**  Parameters:     Name        Description.
**                  value       Floating point value to check
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void MCpu::FloatCheck(CpWord value)
{
	int exponent = ((int)(value >> 48)) & Mask12;

	if (exponent == 03777 || exponent == 04000)
	{
		cpu.exitCondition |= EcOperandOutOfRange;
		floatException = TRUE;
	}
	else if (exponent == 01777 || exponent == 06000)
	{
		cpu.exitCondition |= EcIndefiniteOperand;
		floatException = TRUE;
	}
}

/*--------------------------------------------------------------------------
**  Purpose:        Handle floating point exception
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void MCpu::FloatExceptionHandler(void)
{
	if (floatException)
	{
		floatException = FALSE;

		if ((cpu.exitMode & (cpu.exitCondition << 12)) != 0)
		{
			/*
			**  Exit mode selected.
			*/
			cpu.cpuStopped = TRUE;

			if (cpu.regRaCm < cpuMaxMemory)
			{
				cpMem[cpu.regRaCm] = ((CpWord)cpu.exitCondition << 48) | ((CpWord)(cpu.regP + 1) << 30);
			}

			cpu.regP = 0;

			if ((features & (HasNoCejMej | IsSeries6x00)) == 0 && (mfr->monitorCpu == -1))
			{
				/*
				**  Exchange jump to MA.
				*/
				ExchangeJump(cpu.regMa, cpu.CpuID, "FloatExceptionHandler");
			}
		}
	}
}

/*--------------------------------------------------------------------------
**  Purpose:        Functions to implement all opcodes
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/

void MCpu::Op00(void)
{
	/*
	**  PS or Error Exit to MA.
	*/
	if ((features & (HasNoCejMej | IsSeries6x00)) != 0 || (mfr->monitorCpu == -1))
	{
		if ((features & HasNoCejMej) != 0)
			printf("HasNoCejMej in cpOp00\n");

		cpu.cpuStopped = TRUE;
	}
	else
	{
		OpIllegal("Op00");
	}

}

void MCpu::Op01(void)
{
	u32 oldP = cpu.regP;
	u8 oldOffset = opOffset;

	switch (opI)
	{
	case 0:
		/*
		**  RJ  K
		*/
		acc60 = ((CpWord)0400 << 48) | ((CpWord)((cpu.regP + 1) & Mask18) << 30);
		if (WriteMem(opAddress, &acc60))
		{
			return;
		}

		cpu.regP = opAddress;
		opOffset = 0;

		if ((features & HasInstructionStack) != 0)
		{
			/*
			**  Void the instruction stack.
			*/
			VoidIwStack(~0);
		}

		break;

	case 1:
		/*
		**  REC  Bj+K
		*/
		if ((cpu.exitMode & EmFlagUemEnable) != 0)
		{
			UemTransfer(FALSE);
		}
		else
		{
			EcsTransfer(FALSE);
		}

		if ((features & HasInstructionStack) != 0)
		{
			/*
			**  Void the instruction stack.
			*/
			VoidIwStack(~0);
		}

		break;

	case 2:
		/*
		**  WEC  Bj+K
		*/
		if ((cpu.exitMode & EmFlagUemEnable) != 0)
		{
			UemTransfer(TRUE);
		}
		else
		{
			EcsTransfer(TRUE);
		}

		break;

	case 3:
		/*
		**  XJ  K
		*/
		if ((features & HasNoCejMej) != 0 || opOffset != 30)
		{
			/*
			**  CEJ/MEJ must be enabled and the instruction must be in parcel 0,
			**  if not, it is interpreted as an illegal instruction.
			*/
			OpIllegal("Op01 XJ K");
			return;
		}

		cpu.regP = (cpu.regP + 1) & Mask18;
		cpu.cpuStopped = TRUE;

		bool XJRet;

		if ((mfr->monitorCpu == cpu.CpuID))
		{
			//monitorCpu = -1;  // exit monitor mode
			XJRet = ExchangeJump(opAddress + cpu.regB[opJ], -1, "Op01 XJ K - exit monitor mode");
		}
		else
		{
#if MaxCpus == 2
			if (BigIron->initCpus > 1)
			{
				RESERVE1(&mfr->XJWaitMutex);
			}
			if (mfr->monitorCpu > -1 && mfr->monitorCpu != cpu.CpuID)
			{
#if CcDebug == 1
				traceCpuPrint(this, "Waiting for XJ\n");
#endif
				if (SleepConditionVariableCS(&mfr->XJDone, &mfr->XJWaitMutex, 1) == 0)
				{
					if (mfr->monitorCpu > -1 && mfr->monitorCpu != cpu.CpuID)
					{
#if CcDebug == 1
						traceCpuPrint(this, "Waiting for XJ: timeout- Retry later!\n");
#endif
						//printf("XJ timeout.. Retry\n");
						cpu.regP = oldP;
						opOffset = oldOffset + 30;
#if MaxCpus ==2						
						if (BigIron->initCpus > 1)
						{
							RELEASE1(&mfr->XJWaitMutex);
						}	
#endif
						return;
					}
				}
#if CcDebug == 1
				traceCpuPrint(this, "Waiting for XJ done\n");
#endif
			}
#if MaxCpus == 2
			if (BigIron->initCpus > 1)
			{
				RELEASE1(&mfr->XJWaitMutex);
			}	
#endif
#endif
			XJRet = ExchangeJump(cpu.regMa, cpu.CpuID, "Op01 XJ K - enter monitor mode");
		}

		if (!XJRet)
		{
#if CcDebug == 1
			traceCpuPrint(this, "XJ failed- Retry later!\n");
#endif
			cpu.regP = oldP;
			opOffset = oldOffset + 30;
			return;
		}
		break;

	case 4:
		if (BigIron->modelType != ModelCyber865)
		{
			OpIllegal("Op01 not ModelCyber865 case 4");
			return;
		}

		/*
		**  RXj  Xk
		*/
		if ((cpu.exitMode & EmFlagUemEnable) != 0)
		{
			UemWord(FALSE);
		}
		else
		{
			EcsWord(FALSE);
		}

		break;

	case 5:
		if (BigIron->modelType != ModelCyber865)
		{
			OpIllegal("Op01 not ModelCyber865 case 5");
			return;
		}

		/*
		**  WXj  Xk
		*/
		if ((cpu.exitMode & EmFlagUemEnable) != 0)
		{
			UemWord(TRUE);
		}
		else
		{
			EcsWord(TRUE);
		}

		break;

	case 6:
		if ((features & HasMicrosecondClock) != 0)
		{
			/*
			**  RC  Xj
			*/
			rtcReadUsCounter();
			cpu.regX[opJ] = rtcClock;
		}
		else
		{
			OpIllegal("Op01 RC Xj");
		}

		break;

	case 7:
		/*
		**  7600 instruction (invalid in our context).
		*/
		OpIllegal("7600 instruction (invalid in our context)");
		break;
	default: 
		OpIllegal("Op01");
		break;
	}
}

void MCpu::Op02(void)
{
	/*
	**  JP  Bi+K
	*/
	cpu.regP = Add18(cpu.regB[opI], opAddress);

	if ((features & HasInstructionStack) != 0)
	{
		/*
		**  Void the instruction stack.
		*/
		VoidIwStack(~0);
	}

	FetchOpWord(cpu.regP, &opWord);
}

void MCpu::Op03(void)
{
	bool jump = FALSE;

	switch (opI)
	{
	case 0:
		/*
		**  ZR  Xj K
		*/
		jump = cpu.regX[opJ] == 0 || cpu.regX[opJ] == NegativeZero;
		break;

	case 1:
		/*
		**  NZ  Xj K
		*/
		jump = cpu.regX[opJ] != 0 && cpu.regX[opJ] != NegativeZero;
		break;

	case 2:
		/*
		**  PL  Xj K
		*/
		jump = (cpu.regX[opJ] & Sign60) == 0;
		break;

	case 3:
		/*
		**  NG  Xj K
		*/
		jump = (cpu.regX[opJ] & Sign60) != 0;
		break;

	case 4:
		/*
		**  IR  Xj K
		*/
		acc60 = cpu.regX[opJ] >> 48;
		jump = acc60 != 03777 && acc60 != 04000;
		break;

	case 5:
		/*
		**  OR  Xj K
		*/
		acc60 = cpu.regX[opJ] >> 48;
		jump = acc60 == 03777 || acc60 == 04000;
		break;

	case 6:
		/*
		**  DF  Xj K
		*/
		acc60 = cpu.regX[opJ] >> 48;
		jump = acc60 != 01777 && acc60 != 06000;
		break;

	case 7:
		/*
		**  ID  Xj K
		*/
		acc60 = cpu.regX[opJ] >> 48;
		jump = acc60 == 01777 || acc60 == 06000;
		break;
	default: 
		OpIllegal("Op03");
		break;
	}

	if (jump)
	{
		if ((features & HasInstructionStack) != 0)
		{
			/*
			**  Void the instruction stack.
			*/
			if ((cpu.exitMode & EmFlagStackPurge) != 0)
			{
				/*
				**  Instruction stack purge flag is set - do an
				**  unconditional void.
				*/
				VoidIwStack(~0);
			}
			else
			{
				/*
				**  Normal conditional void.
				*/
				VoidIwStack(opAddress);
			}
		}

		cpu.regP = opAddress;
		FetchOpWord(cpu.regP, &opWord);
	}
}

void MCpu::Op04(void)
{
	/*
	**  EQ  Bi Bj K
	*/
	if (cpu.regB[opI] == cpu.regB[opJ])
	{
		if ((features & HasInstructionStack) != 0)
		{
			/*
			**  Void the instruction stack.
			*/
			VoidIwStack(opAddress);
		}

		cpu.regP = opAddress;
		FetchOpWord(cpu.regP, &opWord);
	}
}

void MCpu::Op05(void)
{
	/*
	**  NE  Bi Bj K
	*/
	if (cpu.regB[opI] != cpu.regB[opJ])
	{
		if ((features & HasInstructionStack) != 0)
		{
			/*
			**  Void the instruction stack.
			*/
			VoidIwStack(opAddress);
		}

		cpu.regP = opAddress;
		FetchOpWord(cpu.regP, &opWord);
	}
}

void MCpu::Op06(void)
{
	/*
	**  GE  Bi Bj K
	*/
	i32 signDiff = (cpu.regB[opI] & Sign18) - (cpu.regB[opJ] & Sign18);
	if (signDiff > 0)
	{
		return;
	}

	if (signDiff == 0)
	{
		acc18 = (cpu.regB[opI] & Mask18) - (cpu.regB[opJ] & Mask18);
		if ((acc18 & Overflow18) != 0 && (acc18 & Mask18) != 0)
		{
			acc18 -= 1;
		}

		if ((acc18 & Sign18) != 0)
		{
			return;
		}
	}

	if ((features & HasInstructionStack) != 0)
	{
		/*
		**  Void the instruction stack.
		*/
		VoidIwStack(opAddress);
	}

	cpu.regP = opAddress;
	FetchOpWord(cpu.regP, &opWord);
}

void MCpu::Op07(void)
{
	/*
	**  LT  Bi Bj K
	*/
	i32 signDiff = (cpu.regB[opI] & Sign18) - (cpu.regB[opJ] & Sign18);
	if (signDiff < 0)
	{
		return;
	}

	if (signDiff == 0)
	{
		acc18 = (cpu.regB[opI] & Mask18) - (cpu.regB[opJ] & Mask18);
		if ((acc18 & Overflow18) != 0 && (acc18 & Mask18) != 0)
		{
			acc18 -= 1;
		}

		if ((acc18 & Sign18) == 0 || acc18 == 0)
		{
			return;
		}
	}

	if ((features & HasInstructionStack) != 0)
	{
		/*
		**  Void the instruction stack.
		*/
		VoidIwStack(opAddress);
	}

	cpu.regP = opAddress;
	FetchOpWord(cpu.regP, &opWord);
}

void MCpu::Op10(void)
{
	/*
	**  BXi Xj
	*/
	cpu.regX[opI] = cpu.regX[opJ] & Mask60;
}

void MCpu::Op11(void)
{
	/*
	**  BXi Xj*Xk
	*/
	cpu.regX[opI] = (cpu.regX[opJ] & cpu.regX[opK]) & Mask60;
}

void MCpu::Op12(void)
{
	/*
	**  BXi Xj+Xk
	*/
	cpu.regX[opI] = (cpu.regX[opJ] | cpu.regX[opK]) & Mask60;
}

void MCpu::Op13(void)
{
	/*
	**  BXi Xj-Xk
	*/
	cpu.regX[opI] = (cpu.regX[opJ] ^ cpu.regX[opK]) & Mask60;
}

void MCpu::Op14(void)
{
	/*
	**  BXi -Xj
	*/
	cpu.regX[opI] = ~cpu.regX[opK] & Mask60;
}

void MCpu::Op15(void)
{
	/*
	**  BXi -Xk*Xj
	*/
	cpu.regX[opI] = (cpu.regX[opJ] & ~cpu.regX[opK]) & Mask60;
}

void MCpu::Op16(void)
{
	/*
	**  BXi -Xk+Xj
	*/
	cpu.regX[opI] = (cpu.regX[opJ] | ~cpu.regX[opK]) & Mask60;
}

void MCpu::Op17(void)
{
	/*
	**  BXi -Xk-Xj
	*/
	cpu.regX[opI] = (cpu.regX[opJ] ^ ~cpu.regX[opK]) & Mask60;
}

void MCpu::Op20(void)
{
	/*
	**  LXi jk
	*/
	u8 jk;

	jk = (u8)((opJ << 3) | opK);
	cpu.regX[opI] = shiftLeftCircular(cpu.regX[opI] & Mask60, jk);
}

void MCpu::Op21(void)
{
	/*
	**  AXi jk
	*/
	u8 jk;

	jk = (u8)((opJ << 3) | opK);
	cpu.regX[opI] = shiftRightArithmetic(cpu.regX[opI] & Mask60, jk);
}

void MCpu::Op22(void)
{
	/*
	**  LXi Bj Xk
	*/
	u32 count;

	count = cpu.regB[opJ] & Mask18;
	acc60 = cpu.regX[opK] & Mask60;

	if ((count & Sign18) == 0)
	{
		count &= Mask6;
		cpu.regX[opI] = shiftLeftCircular(acc60, count);
	}
	else
	{
		count = ~count;
		count &= Mask11;
		if ((count & ~Mask6) != 0)
		{
			cpu.regX[opI] = 0;
		}
		else
		{
			cpu.regX[opI] = shiftRightArithmetic(acc60, count);
		}
	}
}

void MCpu::Op23(void)
{
	/*
	**  AXi Bj Xk
	*/
	u32 count;

	count = cpu.regB[opJ] & Mask18;
	acc60 = cpu.regX[opK] & Mask60;

	if ((count & Sign18) == 0)
	{
		count &= Mask11;
		if ((count & ~Mask6) != 0)
		{
			cpu.regX[opI] = 0;
		}
		else
		{
			cpu.regX[opI] = shiftRightArithmetic(acc60, count);
		}
	}
	else
	{
		count = ~count;
		count &= Mask6;
		cpu.regX[opI] = shiftLeftCircular(acc60, count);
	}
}

void MCpu::Op24(void)
{
	/*
	**  NXi Bj Xk
	*/
	FloatCheck(cpu.regX[opK]);
	cpu.regX[opI] = shiftNormalize(cpu.regX[opK], &cpu.regB[opJ], FALSE);
	FloatExceptionHandler();
}

void MCpu::Op25(void)
{
	/*
	**  ZXi Bj Xk
	*/
	FloatCheck(cpu.regX[opK]);
	cpu.regX[opI] = shiftNormalize(cpu.regX[opK], &cpu.regB[opJ], TRUE);
	FloatExceptionHandler();
}

void MCpu::Op26(void)
{
	/*
	**  UXi Bj Xk
	*/
	if (opJ == 0)
	{
		cpu.regX[opI] = shiftUnpack(cpu.regX[opK], NULL);
	}
	else
	{
		cpu.regX[opI] = shiftUnpack(cpu.regX[opK], &cpu.regB[opJ]);
	}
}

void MCpu::Op27(void)
{
	/*
	**  PXi Bj Xk
	*/
	if (opJ == 0)
	{
		cpu.regX[opI] = shiftPack(cpu.regX[opK], 0);
	}
	else
	{
		cpu.regX[opI] = shiftPack(cpu.regX[opK], cpu.regB[opJ]);
	}
}

void MCpu::Op30(void)
{
	/*
	**  FXi Xj+Xk
	*/
	FloatCheck(cpu.regX[opJ]);
	FloatCheck(cpu.regX[opK]);
	cpu.regX[opI] = floatAdd(cpu.regX[opJ], cpu.regX[opK], FALSE, FALSE);
	FloatExceptionHandler();
}

void MCpu::Op31(void)
{
	/*
	**  FXi Xj-Xk
	*/
	FloatCheck(cpu.regX[opJ]);
	FloatCheck(cpu.regX[opK]);
	cpu.regX[opI] = floatAdd(cpu.regX[opJ], (~cpu.regX[opK] & Mask60), FALSE, FALSE);
	FloatExceptionHandler();
}

void MCpu::Op32(void)
{
	/*
	**  DXi Xj+Xk
	*/
	FloatCheck(cpu.regX[opJ]);
	FloatCheck(cpu.regX[opK]);
	cpu.regX[opI] = floatAdd(cpu.regX[opJ], cpu.regX[opK], FALSE, TRUE);
	FloatExceptionHandler();
}

void MCpu::Op33(void)
{
	/*
	**  DXi Xj-Xk
	*/
	FloatCheck(cpu.regX[opJ]);
	FloatCheck(cpu.regX[opK]);
	cpu.regX[opI] = floatAdd(cpu.regX[opJ], (~cpu.regX[opK] & Mask60), FALSE, TRUE);
	FloatExceptionHandler();
}

void MCpu::Op34(void)
{
	/*
	**  RXi Xj+Xk
	*/
	FloatCheck(cpu.regX[opJ]);
	FloatCheck(cpu.regX[opK]);
	cpu.regX[opI] = floatAdd(cpu.regX[opJ], cpu.regX[opK], TRUE, FALSE);
	FloatExceptionHandler();
}

void MCpu::Op35(void)
{
	/*
	**  RXi Xj-Xk
	*/
	FloatCheck(cpu.regX[opJ]);
	FloatCheck(cpu.regX[opK]);
	cpu.regX[opI] = floatAdd(cpu.regX[opJ], (~cpu.regX[opK] & Mask60), TRUE, FALSE);
	FloatExceptionHandler();
}

void MCpu::Op36(void)
{
	/*
	**  IXi Xj+Xk
	*/
	acc60 = (cpu.regX[opJ] & Mask60) - (~cpu.regX[opK] & Mask60);
	if ((acc60 & Overflow60) != 0)
	{
		acc60 -= 1;
	}

	cpu.regX[opI] = acc60 & Mask60;
}

void MCpu::Op37(void)
{
	/*
	**  IXi Xj-Xk
	*/
	acc60 = (cpu.regX[opJ] & Mask60) - (cpu.regX[opK] & Mask60);
	if ((acc60 & Overflow60) != 0)
	{
		acc60 -= 1;
	}

	cpu.regX[opI] = acc60 & Mask60;
}

void MCpu::Op40(void)
{
	/*
	**  FXi Xj*Xk
	*/
	FloatCheck(cpu.regX[opJ]);
	FloatCheck(cpu.regX[opK]);
	cpu.regX[opI] = floatMultiply(cpu.regX[opJ], cpu.regX[opK], FALSE, FALSE);
	FloatExceptionHandler();
}

void MCpu::Op41(void)
{
	/*
	**  RXi Xj*Xk
	*/
	FloatCheck(cpu.regX[opJ]);
	FloatCheck(cpu.regX[opK]);
	cpu.regX[opI] = floatMultiply(cpu.regX[opJ], cpu.regX[opK], TRUE, FALSE);
	FloatExceptionHandler();
}

void MCpu::Op42(void)
{
	/*
	**  DXi Xj*Xk
	*/
	FloatCheck(cpu.regX[opJ]);
	FloatCheck(cpu.regX[opK]);
	cpu.regX[opI] = floatMultiply(cpu.regX[opJ], cpu.regX[opK], FALSE, TRUE);
	FloatExceptionHandler();
}

void MCpu::Op43(void)
{
	/*
	**  MXi jk
	*/
	u8 jk;

	jk = (u8)((opJ << 3) | opK);
	cpu.regX[opI] = shiftMask(jk);
}

void MCpu::Op44(void)
{
	/*
	**  FXi Xj/Xk
	*/
	FloatCheck(cpu.regX[opJ]);
	FloatCheck(cpu.regX[opK]);
	cpu.regX[opI] = floatDivide(cpu.regX[opJ], cpu.regX[opK], FALSE);
	FloatExceptionHandler();
#if CcSMM_EJT
	skipStep = 20;
#endif
}

void MCpu::Op45(void)
{
	/*
	**  RXi Xj/Xk
	*/
	FloatCheck(cpu.regX[opJ]);
	FloatCheck(cpu.regX[opK]);
	cpu.regX[opI] = floatDivide(cpu.regX[opJ], cpu.regX[opK], TRUE);
	FloatExceptionHandler();
}

void MCpu::Op46(void)
{
	switch (opI)
	{
	default:
		/*
		**  NO (pass).
		*/
		return;

	case 4:
	case 5:
	case 6:
	case 7:
		if ((features & HasCMU) == 0)
		{
			OpIllegal("Op46 no CMU");
			return;
		}

		if (opOffset != 45)
		{
			if ((features & IsSeries70) == 0)
			{
				/*
				**  Instruction must be in parcel 0, if not, it is interpreted as a
				**  pass instruction (NO) on Cyber 70 series or as illegal on anything
				**  else.
				*/
				OpIllegal("Instruction must be in parcel 0");
			}

			return;
		}
		break;
	}

	switch (opI)
	{
	case 4:
		/*
		**  Move indirect.
		*/
		CmuMoveIndirect();
		break;

	case 5:
		/*
		**  Move direct.
		*/
		CmuMoveDirect();
		break;

	case 6:
		/*
		**  Compare collated.
		*/
		CmuCompareCollated();
		break;

	case 7:
		/*
		**  Compare uncollated.
		*/
		CmuCompareUncollated();
		break;
	default: 
		OpIllegal("Op46");
		break;
	}
}

void MCpu::Op47(void)
{
	/*
	**  CXi Xk
	*/
	acc60 = cpu.regX[opK] & Mask60;
	acc60 = ((acc60 & 0xAAAAAAAAAAAAAAAA) >> 1) + (acc60 & 0x5555555555555555);
	acc60 = ((acc60 & 0xCCCCCCCCCCCCCCCC) >> 2) + (acc60 & 0x3333333333333333);
	acc60 = ((acc60 & 0xF0F0F0F0F0F0F0F0) >> 4) + (acc60 & 0x0F0F0F0F0F0F0F0F);
	acc60 = ((acc60 & 0xFF00FF00FF00FF00) >> 8) + (acc60 & 0x00FF00FF00FF00FF);
	acc60 = ((acc60 & 0xFFFF0000FFFF0000) >> 16) + (acc60 & 0x0000FFFF0000FFFF);
	acc60 = ((acc60 & 0xFFFFFFFF00000000) >> 32) + (acc60 & 0x00000000FFFFFFFF);
	cpu.regX[opI] = acc60 & Mask60;
}

void MCpu::Op50(void)
{
	/*
	**  SAi Aj+K
	*/
	cpu.regA[opI] = Add18(cpu.regA[opJ], opAddress);

	RegASemantics();
}

void MCpu::Op51(void)
{
	/*
	**  SAi Bj+K
	*/
	cpu.regA[opI] = Add18(cpu.regB[opJ], opAddress);

	RegASemantics();
}

void MCpu::Op52(void)
{
	/*
	**  SAi Xj+K
	*/
	cpu.regA[opI] = Add18((u32)cpu.regX[opJ], opAddress);

	RegASemantics();
}

void MCpu::Op53(void)
{
	/*
	**  SAi Xj+Bk
	*/
	cpu.regA[opI] = Add18((u32)cpu.regX[opJ], cpu.regB[opK]);

	RegASemantics();
}

void MCpu::Op54(void)
{
	/*
	**  SAi Aj+Bk
	*/
	cpu.regA[opI] = Add18(cpu.regA[opJ], cpu.regB[opK]);

	RegASemantics();
}

void MCpu::Op55(void)
{
	/*
	**  SAi Aj-Bk
	*/
	cpu.regA[opI] = Subtract18(cpu.regA[opJ], cpu.regB[opK]);

	RegASemantics();
}

void MCpu::Op56(void)
{
	/*
	**  SAi Bj+Bk
	*/
	cpu.regA[opI] = Add18(cpu.regB[opJ], cpu.regB[opK]);

	RegASemantics();
}

void MCpu::Op57(void)
{
	/*
	**  SAi Bj-Bk
	*/
	cpu.regA[opI] = Subtract18(cpu.regB[opJ], cpu.regB[opK]);

	RegASemantics();
}

void MCpu::Op60(void)
{
	/*
	**  SBi Aj+K
	*/
	cpu.regB[opI] = Add18(cpu.regA[opJ], opAddress);
}

void MCpu::Op61(void)
{
	/*
	**  SBi Bj+K
	*/
	cpu.regB[opI] = Add18(cpu.regB[opJ], opAddress);
}

void MCpu::Op62(void)
{
	/*
	**  SBi Xj+K
	*/
	cpu.regB[opI] = Add18((u32)cpu.regX[opJ], opAddress);
}

void MCpu::Op63(void)
{
	/*
	**  SBi Xj+Bk
	*/
	cpu.regB[opI] = Add18((u32)cpu.regX[opJ], cpu.regB[opK]);
}

void MCpu::Op64(void)
{
	/*
	**  SBi Aj+Bk
	*/
	cpu.regB[opI] = Add18(cpu.regA[opJ], cpu.regB[opK]);
}

void MCpu::Op65(void)
{
	/*
	**  SBi Aj-Bk
	*/
	cpu.regB[opI] = Subtract18(cpu.regA[opJ], cpu.regB[opK]);
}

void MCpu::Op66(void)
{
	if (opI == 0 && (features & IsSeries800) != 0)
	{
		/*
		**  CR Xj,Xk
		*/
		ReadMem((u32)(cpu.regX[opK]) & Mask21, cpu.regX + opJ);
		return;
	}

	/*
	**  SBi Bj+Bk
	*/
	cpu.regB[opI] = Add18(cpu.regB[opJ], cpu.regB[opK]);
}

void MCpu::Op67(void)
{
	if (opI == 0 && (features & IsSeries800) != 0)
	{
		/*
		**  CW Xj,Xk
		*/
		WriteMem((u32)(cpu.regX[opK]) & Mask21, cpu.regX + opJ);
		return;
	}

	/*
	**  SBi Bj-Bk
	*/
	cpu.regB[opI] = Subtract18(cpu.regB[opJ], cpu.regB[opK]);
}

void MCpu::Op70(void)
{
	/*
	**  SXi Aj+K
	*/
	acc60 = (CpWord)Add18(cpu.regA[opJ], opAddress);

	if ((acc60 & 0400000) != 0)
	{
		acc60 |= SignExtend18To60;
	}

	cpu.regX[opI] = acc60 & Mask60;
}

void MCpu::Op71(void)
{
	/*
	**  SXi Bj+K
	*/
	acc60 = (CpWord)Add18(cpu.regB[opJ], opAddress);

	if ((acc60 & 0400000) != 0)
	{
		acc60 |= SignExtend18To60;
	}

	cpu.regX[opI] = acc60 & Mask60;
}

void MCpu::Op72(void)
{
	/*
	**  SXi Xj+K
	*/
	acc60 = (CpWord)Add18((u32)cpu.regX[opJ], opAddress);

	if ((acc60 & 0400000) != 0)
	{
		acc60 |= SignExtend18To60;
	}

	cpu.regX[opI] = acc60 & Mask60;
}

void MCpu::Op73(void)
{
	/*
	**  SXi Xj+Bk
	*/
	acc60 = (CpWord)Add18((u32)cpu.regX[opJ], cpu.regB[opK]);

	if ((acc60 & 0400000) != 0)
	{
		acc60 |= SignExtend18To60;
	}

	cpu.regX[opI] = acc60 & Mask60;
}

void MCpu::Op74(void)
{
	/*
	**  SXi Aj+Bk
	*/
	acc60 = (CpWord)Add18(cpu.regA[opJ], cpu.regB[opK]);

	if ((acc60 & 0400000) != 0)
	{
		acc60 |= SignExtend18To60;
	}

	cpu.regX[opI] = acc60 & Mask60;
}

void MCpu::Op75(void)
{
	/*
	**  SXi Aj-Bk
	*/
	acc60 = (CpWord)Subtract18(cpu.regA[opJ], cpu.regB[opK]);


	if ((acc60 & 0400000) != 0)
	{
		acc60 |= SignExtend18To60;
	}

	cpu.regX[opI] = acc60 & Mask60;
}

void MCpu::Op76(void)
{
	/*
	**  SXi Bj+Bk
	*/
	acc60 = (CpWord)Add18(cpu.regB[opJ], cpu.regB[opK]);

	if ((acc60 & 0400000) != 0)
	{
		acc60 |= SignExtend18To60;
	}

	cpu.regX[opI] = acc60 & Mask60;
}

void MCpu::Op77(void)
{
	/*
	**  SXi Bj-Bk
	*/
	acc60 = (CpWord)Subtract18(cpu.regB[opJ], cpu.regB[opK]);

	if ((acc60 & 0400000) != 0)
	{
		acc60 |= SignExtend18To60;
	}

	cpu.regX[opI] = acc60 & Mask60;
}

			
/*---------------------------  End Of File  ------------------------------*/

