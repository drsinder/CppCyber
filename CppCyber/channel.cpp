/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2011, Tom Hunter
**  C++ adaptation by Dale Sinder 2017**
**
**  Name: channel.cpp
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
	mfr->channel = static_cast<ChSlot*>(calloc(MaxChannels, sizeof(ChSlot)));
	if (mfr->channel == nullptr)
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

	/*
	**  Give some devices a chance to cleanup and free allocated memory of all
	**  devices hanging of this channel.
	*/
	for (ch = 0; ch < BigIron->chasis[mfrID]->channelCount; ch++)
	{
		for (dp = BigIron->chasis[mfrID]->channel[ch].firstDevice; dp != nullptr; dp = dp->next)
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
			for (u8 i = 0; i < MaxUnits; i++)
			{
				if (dp->context[i] != nullptr)
				{
					free(dp->context[i]);
				}

				if (dp->fcb[i] != nullptr)
				{
					fclose(dp->fcb[i]);
				}
			}
		}

		for (dp = BigIron->chasis[mfrID]->channel[ch].firstDevice; dp != nullptr;)
		{
			/*
			**  Free all device control blocks.
			*/
			DevSlot *fp = dp;
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
	ChSlot *cp = BigIron->chasis[mfrID]->channel + channelNo;
	DevSlot *device = cp->firstDevice;

	/*
	**  Try to locate device control block.
	*/
	while (device != nullptr)
	{
		if (device->devType == devType 
			&& device->mfrID == mfrID)
		{
			return(device);
		}

		device = device->next;
	}

	return(nullptr);
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
	MMainFrame *mfr = BigIron->chasis[mfrID];
	mfr->activeChannel = BigIron->chasis[mfrID]->channel + channelNo;
	DevSlot *device = mfr->activeChannel->firstDevice;

	/*
	**  Try to locate existing device control block.
	*/
	while (device != nullptr)
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
	device = static_cast<DevSlot*>(calloc(1, sizeof(DevSlot)));
	if (device == nullptr)
	{
		fprintf(stderr, "Failed to allocate control block for Channel %d\n", channelNo);
		exit(1);
	}

	/*
	**  Link device control block into the chain of devices hanging of this channel.
	*/
	device->next = mfr->activeChannel->firstDevice;
	mfr->activeChannel->firstDevice = device;
	device->channel = mfr->activeChannel;
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
void channelFunction(PpWord funcCode, u8 mfrId)
{
	FcStatus status = FcDeclined;

	MMainFrame *mfr = BigIron->chasis[mfrId];

	mfr->activeChannel->full = false;
	for (mfr->activeDevice = mfr->activeChannel->firstDevice; mfr->activeDevice != nullptr; mfr->activeDevice = mfr->activeDevice->next)
	{
		status = mfr->activeDevice->func(funcCode, mfrId);
		if (status == FcAccepted)
		{
			/*
			**  Device has claimed function code - select it for I/O.
			*/
			mfr->activeChannel->ioDevice = mfr->activeDevice;
			break;
		}

		if (status == FcProcessed)
		{
			/*
			**  Device has processed function code - no need for I/O.
			*/
			mfr->activeChannel->ioDevice = nullptr;
			break;
		}
	}

	if (mfr->activeDevice == nullptr || status == FcDeclined)
	{
		/*
		**  No device has claimed the function code - keep channel active
		**  and full, but disconnect device.
		*/
		mfr->activeChannel->ioDevice = nullptr;
		mfr->activeChannel->full = true;
		mfr->activeChannel->active = true;
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
void channelActivate(u8 mfrId)
{
	MMainFrame *mfr = BigIron->chasis[mfrId];

	mfr->activeChannel->active = true;

	if (mfr->activeChannel->ioDevice != nullptr)
	{
		mfr->activeDevice = mfr->activeChannel->ioDevice;
		mfr->activeDevice->activate(mfrId);
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
void channelDisconnect(u8 mfrId)
{
	MMainFrame *mfr = BigIron->chasis[mfrId];

	mfr->activeChannel->active = false;

	if (mfr->activeChannel->ioDevice != nullptr)
	{
		mfr->activeDevice = mfr->activeChannel->ioDevice;
		mfr->activeDevice->disconnect(mfrId);
	}
	else
	{
		mfr->activeChannel->full = false;
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
void channelIo(u8 mfrId)
{
	MMainFrame *mfr = BigIron->chasis[mfrId];

	/*
	**  Perform request.
	*/
	if ((mfr->activeChannel->active || mfr->activeChannel->id == ChClock)
		&& mfr->activeChannel->ioDevice != nullptr)
	{
		mfr->activeDevice = mfr->activeChannel->ioDevice;
		mfr->activeDevice->io(mfrId);
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
void channelCheckIfActive(u8 mfrId)
{
	MMainFrame *mfr = BigIron->chasis[mfrId];

	if (mfr->activeChannel->ioDevice != nullptr)
	{
		mfr->activeDevice = mfr->activeChannel->ioDevice;
		if (mfr->activeDevice->devType == DtPciChannel)
		{
			u16 flags = mfr->activeDevice->flags();
			mfr->activeChannel->active = (flags & MaskActive) != 0;
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
void channelCheckIfFull(u8 mfrId)
{
	MMainFrame *mfr = BigIron->chasis[mfrId];

	if (mfr->activeChannel->ioDevice != nullptr)
	{
		mfr->activeDevice = mfr->activeChannel->ioDevice;
		if (mfr->activeDevice == nullptr)
			return;

		if (mfr->activeDevice->devType == DtPciChannel)
		{
			u16 flags = mfr->activeDevice->flags();
			mfr->activeChannel->full = (flags & MaskFull) != 0;
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
void channelOut(u8 mfrId)
{
	MMainFrame *mfr = BigIron->chasis[mfrId];

	if (mfr->activeChannel->ioDevice != nullptr)
	{
		mfr->activeDevice = mfr->activeChannel->ioDevice;
		if (mfr->activeDevice->devType == DtPciChannel)
		{
			mfr->activeDevice->out(mfr->activeChannel->data);
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
void channelIn(u8 mfrId)
{
	MMainFrame *mfr = BigIron->chasis[mfrId];

	if (mfr->activeChannel->ioDevice != nullptr )  //nullptr)
	{
		mfr->activeDevice = mfr->activeChannel->ioDevice;
		if (mfr->activeDevice->devType == DtPciChannel)
		{
			mfr->activeChannel->data = mfr->activeDevice->in();
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
void channelSetFull(u8 mfrId)
{
	MMainFrame *mfr = BigIron->chasis[mfrId];

	if (mfr->activeChannel->ioDevice != nullptr)
	{
		mfr->activeDevice = mfr->activeChannel->ioDevice;
		if (mfr->activeDevice->devType == DtPciChannel)
		{
			mfr->activeDevice->full();
		}
	}

	mfr->activeChannel->full = true;
}

/*--------------------------------------------------------------------------
**  Purpose:        Set channel empty.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void channelSetEmpty(u8 mfrId)
{
	MMainFrame *mfr = BigIron->chasis[mfrId];

	if (mfr->activeChannel->ioDevice != nullptr)
	{
		mfr->activeDevice = mfr->activeChannel->ioDevice;
		if (mfr->activeDevice == nullptr)
			goto out;

		if (mfr->activeDevice->devType == DtPciChannel)
		{
			mfr->activeDevice->empty();
		}
	}
	out:
	mfr->activeChannel->full = false;
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
	/*
	**  Process any delayed disconnects.
	*/
	for (ch = 0; ch < BigIron->chasis[mfrID]->channelCount; ch++)
	{
		ChSlot *cc = &BigIron->chasis[mfrID]->channel[ch];
		if (cc->delayDisconnect != 0)
		{
			cc->delayDisconnect -= 1;
			if (cc->delayDisconnect == 0)
			{
				cc->active = false;
				cc->discAfterInput = false;
			}
		}

		if (cc->delayStatus != 0)
		{
			cc->delayStatus -= 1;
		}
	}
}

/*---------------------------  End Of File  ------------------------------*/
