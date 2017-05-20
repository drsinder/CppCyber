/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2011, Tom Hunter
**  C++ adaptation by Dale Sinder 2017
**
**  Name: dcc6681.c
**
**  Description:
**      Perform emulation of CDC 6681 or 6684 data channel converter.
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
#include "dcc6681.h"

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
typedef struct dccControl
{
	DevSlot     *device3000[MaxEquipment];
	bool        interrupting[MaxEquipment];
	i8          connectedEquipment;
	bool        selected;
	PpWord      ios;
	PpWord      bcd;
	int         status;
} DccControl;

/*
**  ---------------------------
**  Private Function Prototypes
**  ---------------------------
*/
static FcStatus dcc6681Func(PpWord funcCode, u8 mfrId);
static void dcc6681Io(u8 mfrId);
//static void dcc6681Load(DevSlot *, int, char *);
static void dcc6681Activate(u8 mfrId);
static void dcc6681Disconnect(u8 mfrId);

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

/*
**--------------------------------------------------------------------------
**
**  Public Functions
**
**--------------------------------------------------------------------------
*/

/*--------------------------------------------------------------------------
**  Purpose:        Attach 3000 equipment to 6681 data channel converter.
**
**  Parameters:     Name        Description.
**                  channelNo   channel number the device is attached to
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
DevSlot *dcc6681Attach(u8 channelNo, u8 eqNo, u8 unitNo, u8 devType, u8 mfrID)
{
	DevSlot *dp;
	DccControl *cp;
	DevSlot *device;

	dp = channelAttach(channelNo, 0, DtDcc6681, mfrID);

	dp->activate = dcc6681Activate;
	dp->disconnect = dcc6681Disconnect;
	dp->func = dcc6681Func;
	dp->io = dcc6681Io;

	/*
	**  Allocate converter context when first created.
	*/
	if (dp->context[0] == NULL)
	{
		cp = (DccControl *)calloc(1, sizeof(DccControl));
		if (cp == NULL)
		{
			fprintf(stderr, "Failed to allocate dcc6681 context block\n");
			exit(1);
		}

		cp->selected = TRUE;
		cp->connectedEquipment = -1;
		dp->context[0] = (void *)cp;
	}
	else
	{
		cp = (DccControl *)dp->context[0];
	}

	/*
	**  Allocate 3000 series device control block.
	*/
	if (cp->device3000[eqNo] == NULL)
	{
		device = (DevSlot*)calloc(1, sizeof(DevSlot));
		if (device == NULL)
		{
			fprintf(stderr, "Failed to allocate device control block for converter on channel %d\n", channelNo);
			exit(1);
		}

		cp->device3000[eqNo] = device;
		device->devType = devType;
		device->channel = BigIron->chasis[mfrID]->channel + channelNo;
		device->eqNo = eqNo;
	}
	else
	{
		device = (DevSlot *)cp->device3000[eqNo];
	}

	/*
	**  Print a friendly message.
	*/
	printf("Equipment %02o, Unit %02o attached to DCC6681 on channel %o\n", eqNo, unitNo, channelNo);

	/*
	** Return the allocated 3000 series control block pointer
	*/
	return(device);
}

/*--------------------------------------------------------------------------
**  Purpose:        Terminate channel converter context.
**
**  Parameters:     Name        Description.
**                  dp          Device pointer.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void dcc6681Terminate(DevSlot *dp)
{
	u8 i, j;

	DccControl *cp = (DccControl*)dp->context[0];

	if (cp != NULL)
	{
		for (i = 0; i < MaxEquipment; i++)
		{
			if (cp->device3000[i] != NULL)
			{
				for (j = 0; j < MaxEquipment; j++)
				{
					if (cp->device3000[i]->context[j] != NULL)
					{
						free(cp->device3000[i]->context[j]);
					}
				}

				free(cp->device3000[i]);
			}
		}
	}
}

/*--------------------------------------------------------------------------
**  Purpose:        Return device control block attached to a channel converter.
**
**  Parameters:     Name        Description.
**                  channelNo   channel number
**                  equipmentNo equipment number
**                  devType     device type
**
**  Returns:        Pointer to device slot.
**
**------------------------------------------------------------------------*/
DevSlot *dcc6681FindDevice(u8 mfrID, u8 channelNo, u8 equipmentNo, u8 devType)
{
	DevSlot *dp;
	DccControl *cp;

	/*
	**  First find the channel converter.
	*/
	dp = channelFindDevice(channelNo, DtDcc6681, mfrID);
	if (dp == NULL)
	{
		return(NULL);
	}

	/*
	**  Locate channel converter context.
	*/
	cp = (DccControl *)dp->context[0];
	if (cp == NULL)
	{
		return(NULL);
	}

	/*
	**  Lookup and verify equipment.
	*/
	dp = cp->device3000[equipmentNo];
	if (dp == NULL || dp->devType != devType)
	{
		return(NULL);
	}

	/*
	**  Return series 3000 device control block.
	*/
	return(dp);
}

/*--------------------------------------------------------------------------
**  Purpose:        Update interrupt status of current equipment.
**
**  Parameters:     Name        Description.
**                  status      new interrupt status
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void dcc6681Interrupt(bool status, u8 mfrId)
{
	MMainFrame *mfr = BigIron->chasis[mfrId];
	DccControl *mp = (DccControl *)mfr->activeDevice->context[0];
	if (mp->connectedEquipment >= 0)
	{
		mp->interrupting[mp->connectedEquipment] = status;
	}
}

/*--------------------------------------------------------------------------
**  Purpose:        Execute function code on 6681 data channel converter.
**
**  Parameters:     Name        Description.
**                  funcCode    function code
**
**  Returns:        FcStatus
**
**------------------------------------------------------------------------*/
static FcStatus dcc6681Func(PpWord funcCode, u8 mfrId)
{
	MMainFrame *mfr = BigIron->chasis[mfrId];
	DccControl *mp = (DccControl *)mfr->activeDevice->context[0];
	DevSlot *device;
	PpWord rc;
	i8 u;
	i8 e;

	/*
	**  Clear old function code.
	*/
	mfr->activeDevice->fcode = 0;

	/*
	**  If not selected, we recognize only a select.
	*/
	if (!mp->selected && funcCode != Fc6681Select)
	{
		return(FcDeclined);
	}

	// ReSharper disable once CppDefaultCaseNotHandledInSwitchStatement
	switch (funcCode)
	{
	case Fc6681Select:
		mp->selected = TRUE;
		mp->status = StFc6681Ready;
		return(FcProcessed);

	case Fc6681DeSelect:
		mp->selected = FALSE;
		mp->status = StFc6681Ready;
		return(FcProcessed);

	case Fc6681ConnectMode2:
	case Fc6681FunctionMode2:
	case Fc6681DccStatusReq:
		mfr->activeDevice->fcode = funcCode;
		return(FcAccepted);

	case Fc6681MasterClear:
		mp->status = StFc6681Ready;
		for (e = 0; e < MaxEquipment; e++)
		{
			mp->interrupting[e] = FALSE;
			mfr->active3000Device = mp->device3000[e];
			if (mfr->active3000Device != NULL)
			{
				mfr->active3000Device->selectedUnit = -1;
				(mfr->active3000Device->func)(funcCode, mfrId);
			}
		}

		mp->connectedEquipment = -1;
		return(FcProcessed);
	}

	// ReSharper disable once CppDefaultCaseNotHandledInSwitchStatement
	switch (funcCode & Fc6681IoModeMask)
	{
	case Fc6681DevStatusReq:
		funcCode &= Fc6681IoModeMask;
		e = mp->connectedEquipment;
		if (e < 0)
		{
			mfr->activeDevice->fcode = Fc6681DccStatusReq;
			mp->status = StFc6681IntReject;
			return(FcAccepted);
		}
		mfr->active3000Device = mp->device3000[e];
		mfr->activeDevice->fcode = funcCode;
		return((mfr->active3000Device->func)(funcCode, mfrId));

	case Fc6681InputToEor:
	case Fc6681Input:
	case Fc6681Output:
		e = mp->connectedEquipment;
		if (e < 0)
		{
			mp->status = StFc6681IntReject;
			return(FcProcessed);
		}

		mfr->active3000Device = mp->device3000[e];
		mfr->activeDevice->fcode = funcCode;
		mp->ios = funcCode & Fc6681IoIosMask;
		mp->bcd = funcCode & Fc6681IoBcdMask;
		mp->status = StFc6681Ready;
		funcCode &= Fc6681IoModeMask;
		return((mfr->active3000Device->func)(funcCode,  mfrId));
	}

	// ReSharper disable once CppDefaultCaseNotHandledInSwitchStatement
	switch (funcCode & Fc6681ConnectEquipmentMask)
	{
	case Fc6681Connect4Mode1:
	case Fc6681Connect5Mode1:
	case Fc6681Connect6Mode1:
	case Fc6681Connect7Mode1:
		e = (funcCode & Fc6681ConnectEquipmentMask) >> 9;
		u = funcCode & Fc6681ConnectUnitMask;
		device = mp->device3000[e];
		if (device == NULL || device->context[u] == NULL)
		{
			mp->connectedEquipment = -1;
			mp->status = StFc6681IntReject;
			return(FcProcessed);
		}

		mp->connectedEquipment = e;
		device->selectedUnit = u;
		mp->status = StFc6681Ready;
		return(FcProcessed);

	case Fc6681FunctionMode1:
		e = mp->connectedEquipment;
		if (e < 0)
		{
			mp->status = StFc6681IntReject;
			return(FcProcessed);
		}

		mfr->active3000Device = mp->device3000[e];
		funcCode &= Fc6681ConnectFuncMask;
		mp->status = StFc6681Ready;
		rc = (mfr->active3000Device->func)(funcCode, mfrId);
		if (rc == FcDeclined)
		{
			mp->status = StFc6681IntReject;
		}
		else
		{
			mp->status = StFc6681Ready;
		}

		return((FcStatus)rc);
	}

	mp->status = StFc6681IntReject;
	return(FcProcessed);
}

/*--------------------------------------------------------------------------
**  Purpose:        Perform I/O on 6681 data channel converter.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing
**
**------------------------------------------------------------------------*/
static void dcc6681Io(u8 mfrId)
{
	MMainFrame *mfr = BigIron->chasis[mfrId];
	DccControl *mp = (DccControl *)mfr->activeDevice->context[0];
	DevSlot *device;
	i8 u;
	i8 e;
	PpWord stat;

	switch (mfr->activeDevice->fcode)
	{
	default:
		return;

	case Fc6681Select:
	case Fc6681DeSelect:
	case Fc6681MasterClear:
	case Fc6681Connect4Mode1:
	case Fc6681Connect5Mode1:
	case Fc6681Connect6Mode1:
	case Fc6681Connect7Mode1:
		printf("unexpected IO for function %04o\n", mfr->activeDevice->fcode);
		break;

	case Fc6681ConnectMode2:
		if (mfr->activeChannel->full)
		{
			mfr->activeChannel->full = FALSE;
			mfr->activeDevice->fcode = 0;
			e = (mfr->activeChannel->data & Fc6681ConnectEquipmentMask) >> 9;
			u = mfr->activeChannel->data & Fc6681ConnectUnitMask;
			device = mp->device3000[e];
			if (device == NULL || device->context[u] == NULL)
			{
				mp->connectedEquipment = -1;
				mp->status = StFc6681IntReject;
				break;
			}

			mp->connectedEquipment = e;
			device->selectedUnit = u;
			mp->status = StFc6681Ready;
		}
		break;

	case Fc6681FunctionMode2:
		if (mfr->activeChannel->full)
		{
			mfr->active3000Device = mp->device3000[mp->connectedEquipment];
			mp->status = StFc6681Ready;
			if ((mfr->active3000Device->func)(mfr->activeChannel->data, mfrId) == FcDeclined)
			{
				mp->status = StFc6681IntReject;
			}
			else
			{
				mp->status = StFc6681Ready;
			}

			mfr->activeChannel->full = FALSE;
			mfr->activeDevice->fcode = 0;
		}
		break;

	case Fc6681InputToEor:
	case Fc6681Input:
	case Fc6681Output:
	case Fc6681DevStatusReq:
		mfr->active3000Device = mp->device3000[mp->connectedEquipment];
		(mfr->active3000Device->io)(mfrId);
		break;

	case Fc6681DccStatusReq:
		if (!mfr->activeChannel->full)
		{
			stat = mp->status;

			/*
			**  Assemble interrupt status.
			*/
			for (e = 0; e < MaxEquipment; e++)
			{
				if (mp->device3000[e] != NULL
					&& mp->interrupting[e])
				{
					stat |= (010 << e);
				}
			}

			/*
			**  Return status.
			*/
			mfr->activeChannel->data = stat;
			mfr->activeChannel->full = TRUE;

			/*
			**  Clear function code and status.
			*/
			mfr->activeDevice->fcode = 0;
			mp->status = StFc6681Ready;
		}

		break;
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
static void dcc6681Activate(u8 mfrId)
{
	MMainFrame *mfr = BigIron->chasis[mfrId];

	DccControl *mp = (DccControl *)mfr->activeDevice->context[0];
	i8 e;

	e = mp->connectedEquipment;
	if (e < 0)
	{
		return;
	}

	mfr->active3000Device = mp->device3000[e];
	if (mfr->active3000Device == NULL)
	{
		return;
	}

	(mfr->active3000Device->activate)(mfrId);
}

/*--------------------------------------------------------------------------
**  Purpose:        Handle disconnecting of channel.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void dcc6681Disconnect(u8 mfrId)
{
	MMainFrame *mfr = BigIron->chasis[mfrId];

	DccControl *mp = (DccControl *)mfr->activeDevice->context[0];
	i8 e;

	e = mp->connectedEquipment;
	if (e < 0)
	{
		return;
	}

	mfr->active3000Device = mp->device3000[e];
	if (mfr->active3000Device == NULL)
	{
		return;
	}

	(mfr->active3000Device->disconnect)(mfrId);
}

/*---------------------------  End Of File  ------------------------------*/
