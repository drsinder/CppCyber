/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2011, Tom Hunter
**  C++ adaptation by Dale Sinder 2017**
**  Name: channel.c
**
**  Description:
**      Perform emulation of CDC 6600 channels.
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
static u8 ch = 0;

/*
**--------------------------------------------------------------------------
**
**  Public Functions
**
**--------------------------------------------------------------------------
*/

/*--------------------------------------------------------------------------
**  Purpose:        Initialise channels.
**
**  Parameters:     Name        Description.
**                  count       channel count
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
ChSlot *channelInit(u8 count, MMainFrame *mfr)
{
	/*
	**  Allocate channel structures.
	*/
	mfr->channelCount = count;
	mfr->channel = (ChSlot*)calloc(MaxChannels, sizeof(ChSlot));
	if (mfr->channel == NULL)
	{
		fprintf(stderr, "Failed to allocate channel control blocks\n");
		exit(1);
	}

	/*
	**  Initialise all channels.
	*/
	for (ch = 0; ch < MaxChannels; ch++)
	{
		mfr->channel[ch].id = ch;
		mfr->channel[ch].mfrID = mfr->mainFrameID;
		mfr->channel[ch].mfr = mfr;
	}

	/*
	**  Print a friendly message.
	*/
	printf("Channels initialised (number of channels %o) for mainframe %d\n", mfr->channelCount, mfr->mainFrameID);

	return mfr->channel;
}

/*--------------------------------------------------------------------------
**  Purpose:        Terminate channels.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void channelTerminate(u8 mfrID)
{
	DevSlot *dp;
	DevSlot *fp;
	u8 i;

	/*
	**  Give some devices a chance to cleanup and free allocated memory of all
	**  devices hanging of this channel.
	*/
	for (ch = 0; ch < BigIron->chasis[mfrID]->channelCount; ch++)
	{
		for (dp = BigIron->chasis[mfrID]->channel[ch].firstDevice; dp != NULL; dp = dp->next)
		{
			if (dp->devType == DtDcc6681)
			{
				dcc6681Terminate(dp);
			}

			if (dp->devType == DtMt669)
			{
				mt669Terminate(dp);
			}

			if (dp->devType == DtMt679)
			{
				mt679Terminate(dp);
			}

			/*
			**  Free all unit contexts and close all open files.
			*/
			for (i = 0; i < MaxUnits; i++)
			{
				if (dp->context[i] != NULL)
				{
					free(dp->context[i]);
				}

				if (dp->fcb[i] != NULL)
				{
					fclose(dp->fcb[i]);
				}
			}
		}

		for (dp = BigIron->chasis[mfrID]->channel[ch].firstDevice; dp != NULL;)
		{
			/*
			**  Free all device control blocks.
			*/
			fp = dp;
			dp = dp->next;
			if (mfrID == 0 )
				free(fp);
		}
	}

	/*
	**  Free all channel control blocks.
	*/
	free(BigIron->chasis[mfrID]->channel);
}

/*--------------------------------------------------------------------------
**  Purpose:        Return device control block attached to a channel.
**
**  Parameters:     Name        Description.
**                  channelNo   channel number to attach to.
**                  devType     device type
**
**  Returns:        Pointer to device slot.
**
**------------------------------------------------------------------------*/
DevSlot *channelFindDevice(u8 channelNo, u8 devType, u8 mfrID)
{
	DevSlot *device;
	ChSlot *cp;

	cp = BigIron->chasis[mfrID]->channel + channelNo;
	device = cp->firstDevice;

	/*
	**  Try to locate device control block.
	*/
	while (device != NULL)
	{
		if (device->devType == devType 
			&& device->mfrID == mfrID)
		{
			return(device);
		}

		device = device->next;
	}

	return(NULL);
}

/*--------------------------------------------------------------------------
**  Purpose:        Attach device to channel.
**
**  Parameters:     Name        Description.
**                  channelNo   channel number to attach to
**                  eqNo        equipment number
**                  devType     device type
**
**  Returns:        Pointer to device slot.
**
**------------------------------------------------------------------------*/
DevSlot *channelAttach(u8 channelNo, u8 eqNo, u8 devType, u8 mfrID)
{
	DevSlot *device;

	activeChannel = BigIron->chasis[mfrID]->channel + channelNo;
	device = activeChannel->firstDevice;

	/*
	**  Try to locate existing device control block.
	*/
	while (device != NULL)
	{
		if (device->devType == devType
			&& device->eqNo == eqNo
			&& device->mfrID == mfrID)
		{
			return(device);
		}

		device = device->next;
	}

	/*
	**  No device control block of this type found, allocate new one.
	*/
	device = (DevSlot*)calloc(1, sizeof(DevSlot));
	if (device == NULL)
	{
		fprintf(stderr, "Failed to allocate control block for Channel %d\n", channelNo);
		exit(1);
	}

	/*
	**  Link device control block into the chain of devices hanging of this channel.
	*/
	device->next = activeChannel->firstDevice;
	activeChannel->firstDevice = device;
	device->channel = activeChannel;
	device->devType = devType;
	device->eqNo = eqNo;
	device->mfrID = mfrID;
	device->mfr = BigIron->chasis[mfrID];

	return(device);
}

/*--------------------------------------------------------------------------
**  Purpose:        Issue a function code to all attached devices.
**
**  Parameters:     Name        Description.
**                  funcCode    function code
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void channelFunction(PpWord funcCode)
{
	FcStatus status = FcDeclined;

	activeChannel->full = FALSE;
	for (activeDevice = activeChannel->firstDevice; activeDevice != NULL; activeDevice = activeDevice->next)
	{
		status = activeDevice->func(funcCode);
		if (status == FcAccepted)
		{
			/*
			**  Device has claimed function code - select it for I/O.
			*/
			activeChannel->ioDevice = activeDevice;
			break;
		}

		if (status == FcProcessed)
		{
			/*
			**  Device has processed function code - no need for I/O.
			*/
			activeChannel->ioDevice = NULL;
			break;
		}
	}

	if (activeDevice == NULL || status == FcDeclined)
	{
		/*
		**  No device has claimed the function code - keep channel active
		**  and full, but disconnect device.
		*/
		activeChannel->ioDevice = NULL;
		activeChannel->full = TRUE;
		activeChannel->active = TRUE;
	}
}

/*--------------------------------------------------------------------------
**  Purpose:        Activate a channel and let attached device know.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void channelActivate(void)
{
	activeChannel->active = TRUE;

	if (activeChannel->ioDevice != NULL)
	{
		activeDevice = activeChannel->ioDevice;
		activeDevice->activate();
	}
}

/*--------------------------------------------------------------------------
**  Purpose:        Disconnect a channel and let active device know.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void channelDisconnect(void)
{
	activeChannel->active = FALSE;

	if (activeChannel->ioDevice != NULL)
	{
		activeDevice = activeChannel->ioDevice;
		activeDevice->disconnect();
	}
	else
	{
		activeChannel->full = FALSE;
	}
}

/*--------------------------------------------------------------------------
**  Purpose:        IO on a channel.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing
**
**------------------------------------------------------------------------*/
void channelIo(void)
{
	/*
	**  Perform request.
	*/
	if ((activeChannel->active || activeChannel->id == ChClock)
		&& activeChannel->ioDevice != NULL)
	{
		activeDevice = activeChannel->ioDevice;
		activeDevice->io();
	}
}

/*--------------------------------------------------------------------------
**  Purpose:        Check if PCI channel is active.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void channelCheckIfActive(void)
{
	if (activeChannel->ioDevice != NULL)
	{
		activeDevice = activeChannel->ioDevice;
		if (activeDevice->devType == DtPciChannel)
		{
			u16 flags = activeDevice->flags();
			activeChannel->active = (flags & MaskActive) != 0;
		}
	}
}

/*--------------------------------------------------------------------------
**  Purpose:        Check if channel is full.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void channelCheckIfFull(void)
{
	if (activeChannel->ioDevice != NULL)
	{
		activeDevice = activeChannel->ioDevice;
		if (activeDevice == nullptr)
			return;

		if (activeDevice->devType == DtPciChannel)
		{
			u16 flags = activeDevice->flags();
			activeChannel->full = (flags & MaskFull) != 0;
		}
	}
}

/*--------------------------------------------------------------------------
**  Purpose:        Output to channel
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void channelOut(void)
{
	if (activeChannel->ioDevice != NULL)
	{
		activeDevice = activeChannel->ioDevice;
		if (activeDevice->devType == DtPciChannel)
		{
			activeDevice->out(activeChannel->data);
			return;
		}
	}
}

/*--------------------------------------------------------------------------
**  Purpose:        Input from channel
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void channelIn(void)
{
	if (activeChannel->ioDevice != 0 )  //NULL)
	{
		activeDevice = activeChannel->ioDevice;
		if (activeDevice->devType == DtPciChannel)
		{
			activeChannel->data = activeDevice->in();
			return;
		}
	}
}

/*--------------------------------------------------------------------------
**  Purpose:        Set channel full.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void channelSetFull(void)
{
	if (activeChannel->ioDevice != NULL)
	{
		activeDevice = activeChannel->ioDevice;
		if (activeDevice->devType == DtPciChannel)
		{
			activeDevice->full();
		}
	}

	activeChannel->full = TRUE;
}

/*--------------------------------------------------------------------------
**  Purpose:        Set channel empty.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void channelSetEmpty(void)
{
	if (activeChannel->ioDevice != NULL)
	{
		activeDevice = activeChannel->ioDevice;
		if (activeDevice == nullptr)
			goto out;

		if (activeDevice->devType == DtPciChannel)
		{
			activeDevice->empty();
		}
	}
	out:
	activeChannel->full = FALSE;
}

/*--------------------------------------------------------------------------
**  Purpose:        Handle delayed channel disconnect.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void channelStep(u8 mfrID)
{
	ChSlot *cc;

	/*
	**  Process any delayed disconnects.
	*/
	for (ch = 0; ch < BigIron->chasis[mfrID]->channelCount; ch++)
	{
		cc = &BigIron->chasis[mfrID]->channel[ch];
		if (cc->delayDisconnect != 0)
		{
			cc->delayDisconnect -= 1;
			if (cc->delayDisconnect == 0)
			{
				cc->active = FALSE;
				cc->discAfterInput = FALSE;
			}
		}

		if (cc->delayStatus != 0)
		{
			cc->delayStatus -= 1;
		}
	}
}

/*---------------------------  End Of File  ------------------------------*/
