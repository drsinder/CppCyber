/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2011, Paul Koning, Tom Hunter
**  C++ adaptation by Dale Sinder 2017
**
**  Name: ddp.c
**
**  Description:
**      Perform emulation of CDC Distributive Data Path
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
**  DDP function and status codes.
*/
#define FcDdpReadECS             05001
#define FcDdpWriteECS            05002
#define FcDdpStatus              05004
#define FcDdpMasterClear         05010

/*
**      Status reply flags
**
**      0001 = ECS abort
**      0002 = ECS accept
**      0004 = ECS parity error
**      0010 = ECS write pending
**      0020 = Channel parity error
**      0040 = 6640 parity error
*/
#define StDdpAbort               00001
#define StDdpAccept              00002
#define StDdpParErr              00004
#define StDdpWrite               00010
#define StDdpChParErr            00020
#define StDdp6640ParErr          00040

/*
**  DDP magical ECS address bits
*/
#define DdpAddrMaint             (1 << 21)
#define DdpAddrReadOne           (1 << 22)
#define DdpAddrFlagReg           (1 << 23)

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

typedef struct
{
	CpWord  curword;
	u32     addr;
	int     dbyte;
	int     abyte;
	int     endaddrcycle;
	PpWord  stat;
} DdpContext;

/*
**  ---------------------------
**  Private Function Prototypes
**  ---------------------------
*/
static FcStatus ddpFunc(PpWord funcCode, u8 mfrId);
static void ddpIo(u8 mfrId);
static void ddpActivate(u8 mfrId);
static void ddpDisconnect(u8 mfrId);
static char *ddpFunc2String(PpWord funcCode);

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

#if DEBUG
static FILE *ddpLog = NULL;
#endif

/*
**--------------------------------------------------------------------------
**
**  Public Functions
**
**--------------------------------------------------------------------------
*/

/*--------------------------------------------------------------------------
**  Purpose:        Initialise DDP.
**
**  Parameters:     Name        Description.
**                  model       Cyber model number
**                  increment   clock increment per iteration.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void ddpInit(u8 mfrID, u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName)
{
	DevSlot *dp;
	DdpContext *dc;

	(void)eqNo;
	(void)unitNo;
	(void)deviceName;

	if (BigIron->extMaxMemory == 0)
	{
		fprintf(stderr, "Cannot configure DDP, no ECS configured\n");
		exit(1);
	}

#if DEBUG
	if (ddpLog == NULL)
	{
		ddpLog = fopen("ddplog.txt", "wt");
	}
#endif

	dp = channelAttach(channelNo, eqNo, DtDdp, mfrID);

	dp->activate = ddpActivate;
	dp->disconnect = ddpDisconnect;
	dp->func = ddpFunc;
	dp->io = ddpIo;

	dc = (DdpContext*)calloc(1, sizeof(DdpContext));
	if (dc == NULL)
	{
		fprintf(stderr, "Failed to allocate DDP context block\n");
		exit(1);
	}

	dp->context[0] = dc;
	dc->stat = StDdpAccept;

	/*
	**  Print a friendly message.
	*/
	printf("DDP initialised on channel %o\n", channelNo);
}

/*--------------------------------------------------------------------------
**  Purpose:        Execute function code on DDP device.
**
**  Parameters:     Name        Description.
**                  funcCode    function code
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static FcStatus ddpFunc(PpWord funcCode, u8 mfrId)
{
	DdpContext *dc;
	MMainFrame *mfr = BigIron->chasis[mfrId];

	dc = (DdpContext *)(mfr->activeDevice->context[0]);

#if DEBUG
	fprintf(ddpLog, "\n%06d PP:%02o CH:%02o f:%04o T:%-25s  >   ",
		traceSequenceNo,
		activePpu->id,
		activeDevice->channel->id,
		funcCode,
		ddpFunc2String(funcCode));
#endif

	switch (funcCode)
	{
	default:
#if DEBUG
		fprintf(ddpLog, " FUNC not implemented & declined!");
#endif
		break;

	case FcDdpWriteECS:
		dc->curword = 0;
	case FcDdpReadECS:
	case FcDdpStatus:
		dc->abyte = 0;
		dc->dbyte = 0;
		dc->addr = 0;
		mfr->activeDevice->fcode = funcCode;
		return(FcAccepted);

	case FcDdpMasterClear:
		mfr->activeDevice->fcode = 0;
		dc->stat = StDdpAccept;
		return(FcProcessed);
	}

	return(FcDeclined);
}


/*--------------------------------------------------------------------------
**  Purpose:        Transfer on 60 bit word to/from DDP/ECS.
**
**  Parameters:     Name        Description.
**                  ecsAddress  ECS word address
**                  data        Pointer to 60 bit word
**                  writeToEcs  TRUE if this is a write to ECS, FALSE if
**                              this is a read.
**
**  Returns:        TRUE if accepted, FALSE otherwise.
**
**------------------------------------------------------------------------*/
bool DdpTransfer(u32 ecsAddress, CpWord *data, bool writeToEcs)
{
	/*
	**  Normal (non flag-register) access must be within ECS boundaries.
	*/
	if (ecsAddress >= BigIron->extMaxMemory)
	{
		/*
		**  Abort.
		*/
		return(FALSE);
	}

	/*
	**  Perform the transfer.
	*/
	if (writeToEcs)
	{
		BigIron->extMem[ecsAddress] = *data & Mask60;
	}
	else
	{
		*data = BigIron->extMem[ecsAddress] & Mask60;
	}

	/*
	**  Normal accept.
	*/
	return(TRUE);
}

/*--------------------------------------------------------------------------
**  Purpose:        Perform I/O.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void ddpIo(u8 mfrId)
{
	DdpContext *dc;
	MMainFrame *mfr = BigIron->chasis[mfrId];


	dc = (DdpContext *)(mfr->activeDevice->context[0]);

	switch (mfr->activeDevice->fcode)
	{
	default:
		return;

	case FcDdpStatus:
		if (!mfr->activeChannel->full)
		{
			mfr->activeChannel->data = dc->stat;
			mfr->activeChannel->full = TRUE;
			mfr->activeDevice->fcode = 0;
			// ? activeChannel->discAfterInput = TRUE;
#if DEBUG
			fprintf(ddpLog, " %04o", activeChannel->data);
#endif
		}
		break;

	case FcDdpReadECS:
	case FcDdpWriteECS:
		if (dc->abyte < 2)
		{
			/*
			**  We need to get two address bytes from the PPU.
			*/
			if (mfr->activeChannel->full)
			{
				dc->addr <<= 12;
				dc->addr += mfr->activeChannel->data;
				dc->abyte++;
				mfr->activeChannel->full = FALSE;
			}

			if (dc->abyte == 2)
			{
#if DEBUG
				fprintf(ddpLog, " ECS addr: %08o Data: ", dc->addr);
#endif

				if (mfr->activeDevice->fcode == FcDdpReadECS)
				{
					/*
					**  Delay a bit before we set channel full.
					*/
					dc->endaddrcycle = mfr->activeDevice->mfr->cycles;

					/*
					**  A flag register reference occurs when bit 23 is set address.
					*/
					if ((dc->addr & DdpAddrFlagReg) != 0)
					{
						//printf("\nDDP EcsFlagRegister\n");
						if (mfr->activeDevice->mfr->Acpu[0]->EcsFlagRegister(dc->addr))
						{
							dc->stat = StDdpAccept;
						}
						else
						{
							mfr->activeChannel->discAfterInput = TRUE;
							dc->stat = StDdpAbort;
						}

						dc->dbyte = 0;
						dc->curword = 0;
					}
					else
					{
						dc->dbyte = -1;
					}
				}
			}

			break;
		}

		if (mfr->activeDevice->fcode == FcDdpReadECS)
		{
			if (!mfr->activeChannel->full && labs(mfr->activeDevice->mfr->cycles - dc->endaddrcycle) > 20)
			{
				if (dc->dbyte == -1)
				{
					/*
					**  Fetch next 60 bits from ECS.
					*/
					if (DdpTransfer(dc->addr, &dc->curword, FALSE))    //DRS??
					{
						dc->stat = StDdpAccept;
					}
					else
					{
						mfr->activeChannel->discAfterInput = TRUE;
						dc->stat = StDdpAbort;
					}

					dc->dbyte = 0;
				}

				/*
				**  Return next byte to PPU.
				*/
				mfr->activeChannel->data = (PpWord)((dc->curword >> 48) & Mask12);
				mfr->activeChannel->full = TRUE;

#if DEBUG
				fprintf(ddpLog, " %04o", activeChannel->data);
#endif

				/*
				**  Update admin stuff.
				*/
				dc->curword <<= 12;
				if (++dc->dbyte == 5)
				{
					if (dc->addr & (DdpAddrReadOne | DdpAddrFlagReg))
					{
						mfr->activeChannel->discAfterInput = TRUE;
					}

					dc->dbyte = -1;
					dc->addr++;
				}
			}
		}
		else if (mfr->activeChannel->full)
		{
			dc->stat = StDdpAccept;
			dc->curword <<= 12;
			dc->curword += mfr->activeChannel->data;
			mfr->activeChannel->full = FALSE;

#if DEBUG
			fprintf(ddpLog, " %04o", activeChannel->data);
#endif

			if (++dc->dbyte == 5)
			{
				/*
				**  Write next 60 bit to ECS.
				*/
				if (!DdpTransfer(dc->addr, &dc->curword, TRUE))   //DRS??
				{
					mfr->activeChannel->active = FALSE;
					dc->stat = StDdpAbort;
					return;
				}

				dc->curword = 0;
				dc->dbyte = 0;
				dc->addr++;
			}
		}
	}
}

/*--------------------------------------------------------------------------
**  Purpose:        Handle channel activation.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void ddpActivate(u8 mfrId)
{
#if DEBUG
	fprintf(ddpLog, "\n%06d PP:%02o CH:%02o Activate",
		traceSequenceNo,
		activePpu->id,
		activeDevice->channel->id);
#endif
}

/*--------------------------------------------------------------------------
**  Purpose:        Handle disconnecting of channel.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void ddpDisconnect(u8 mfrId)
{
	DdpContext *dc;
	MMainFrame *mfr = BigIron->chasis[mfrId];

	dc = (DdpContext *)(mfr->activeDevice->context[0]);

#if DEBUG
	fprintf(ddpLog, "\n%06d PP:%02o CH:%02o Disconnect",
		traceSequenceNo,
		activePpu->id,
		activeDevice->channel->id);
#endif

	if (mfr->activeDevice->fcode == FcDdpWriteECS && dc->dbyte != 0)
	{
		/*
		**  Write final 60 bit to ECS padded with zeros.
		*/
		dc->curword <<= 5 - dc->dbyte;
		if (!DdpTransfer(dc->addr, &dc->curword, TRUE))	//DRS??
		{
			mfr->activeChannel->active = FALSE;
			dc->stat = StDdpAbort;
			return;
		}

		dc->curword = 0;
		dc->dbyte = 0;
		dc->addr++;
	}

	/*
	**  Abort pending device disconnects - the PP is doing the disconnect.
	*/
	mfr->activeChannel->discAfterInput = FALSE;
}

/*--------------------------------------------------------------------------
**  Purpose:        Convert function code to string.
**
**  Parameters:     Name        Description.
**                  funcCode    function code
**
**  Returns:        String equivalent of function code.
**
**------------------------------------------------------------------------*/
static char *ddpFunc2String(PpWord funcCode)
{
	static char buf[30];
#if DEBUG
	switch (funcCode)
	{
	case FcDdpReadECS: return "FcDdpReadECS";
	case FcDdpWriteECS: return "FcDdpWriteECS";
	case FcDdpStatus: return "FcDdpStatus";
	case FcDdpMasterClear: return "FcDdpMasterClear";
	}
#endif
	sprintf(buf, "UNKNOWN: %04o", funcCode);
	return(buf);
}

/*---------------------------  End Of File  ------------------------------*/
