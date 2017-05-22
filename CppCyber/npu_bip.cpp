/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2011, Tom Hunter
**  C++ adaptation by Dale Sinder 2017
**
**  Name: npu_bip.cpp
**
**  Description:
**      Perform emulation of the Block Interface Protocol (BIP) in an NPU
**      consisting of a CDC 2550 HCP running CCP.
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
#define NumBuffs        1000

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

/*
**--------------------------------------------------------------------------
**
**  Public Functions
**
**--------------------------------------------------------------------------
*/

/*--------------------------------------------------------------------------
**  Purpose:        Initialize BIP.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void npuBipInit(u8 mfrId)
{
	MMainFrame *mfr = BigIron->chasis[mfrId];

	/*
	**  Allocate data buffer pool.
	*/
	mfr->bufCount = NumBuffs;
	mfr->bufPool = static_cast<NpuBuffer*>(calloc(NumBuffs, sizeof(NpuBuffer)));
	if (mfr->bufPool == nullptr)
	{
		fprintf(stderr, "Failed to allocate NPU data buffer pool\n");
		exit(1);
	}

	/*
	**  Link buffers into pool and link in data.
	*/
	NpuBuffer *bp = mfr->bufPool;
	for (int count = NumBuffs - 1; count > 0; count--)
	{
		/*
		**  Link to next buffer.
		*/
		NpuBuffer *np = bp + 1;
		bp->next = np;
		bp = np;
	}

	bp->next = nullptr;

	/*
	**  Allocate upline buffer queue.
	*/
	mfr->bipUplineQueue = static_cast<NpuQueue*>(calloc(1, sizeof(NpuQueue)));
	if (mfr->bipUplineQueue == nullptr)
	{
		fprintf(stderr, "Failed to allocate NPU buffer queue\n");
		exit(1);
	}
}

/*--------------------------------------------------------------------------
**  Purpose:        Reset BIP.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void npuBipReset(u8 mfrId)
{
	MMainFrame *mfr = BigIron->chasis[mfrId];

	if (mfr->bipUplineBuffer != nullptr)
	{
		npuBipBufRelease(mfr->bipUplineBuffer, mfrId);
	}

	while ((mfr->bipUplineBuffer = npuBipQueueExtract(mfr->bipUplineQueue)) != nullptr)
	{
		npuBipBufRelease(mfr->bipUplineBuffer, mfrId);
	}

	mfr->bipUplineBuffer = nullptr;

	if (mfr->bipDownlineBuffer != nullptr)
	{
		npuBipBufRelease(mfr->bipDownlineBuffer, mfrId);
		mfr->bipDownlineBuffer = nullptr;
	}

	mfr->bipState = mfr->BipIdle;
}

/*--------------------------------------------------------------------------
**  Purpose:        Return current buffer count.
**
**  Parameters:     Name        Description.
**
**  Returns:        Current buffer count.
**
**------------------------------------------------------------------------*/
int npuBipBufCount(u8 mfrId)
{
	MMainFrame *mfr = BigIron->chasis[mfrId];

	return (mfr->bufCount);
}

/*--------------------------------------------------------------------------
**  Purpose:        Allocate NPU buffer from pool.
**
**  Parameters:     Name        Description.
**
**  Returns:        Pointer to newly allocated buffer or nullptr if pool
**                  is empty.
**
**------------------------------------------------------------------------*/
NpuBuffer *npuBipBufGet(u8 mfrId)
{
	MMainFrame *mfr = BigIron->chasis[mfrId];

	/*
	**  Allocate buffer from pool.
	*/
	NpuBuffer *bp = mfr->bufPool;
	if (bp != nullptr)
	{
		/*
		**  Unlink allocated buffer.
		*/
		mfr->bufPool = bp->next;
		mfr->bufCount -= 1;

		/*
		**  Initialise buffer.
		*/
		bp->next = nullptr;
		bp->offset = 0;
		bp->numBytes = 0;
		bp->blockSeqNo = 0;
		//        memset(bp->data, 0, MaxBuffer);     // debug only
	}
	else
	{
		npuLogMessage("BIP: Out of buffers");
		printf("Fatal error: BIP: Out of buffers - limping on\n");
	}

	return(bp);
}

/*--------------------------------------------------------------------------
**  Purpose:        Free NPU buffer back to pool.
**
**  Parameters:     Name        Description.
**                  bp          Pointer to NPU buffer.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void npuBipBufRelease(NpuBuffer *bp, u8 mfrId)
{
	MMainFrame *mfr = BigIron->chasis[mfrId];

	if (bp != nullptr)
	{
		/*
		**  Link buffer back into the pool.
		*/
		bp->next = mfr->bufPool;
		mfr->bufPool = bp;
		mfr->bufCount += 1;
	}
}

/*--------------------------------------------------------------------------
**  Purpose:        Enqueue a buffer at the tail of a queue.
**
**  Parameters:     Name        Description.
**                  bp          Pointer to NPU buffer.
**                  queue       Pointer to queue.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void npuBipQueueAppend(NpuBuffer *bp, NpuQueue *queue)
{
	if (bp != nullptr)
	{
		if (queue->first == nullptr)
		{
			queue->first = bp;
		}
		else
		{
			queue->last->next = bp;
		}

		queue->last = bp;
		bp->next = nullptr;
	}
}

/*--------------------------------------------------------------------------
**  Purpose:        Enqueue a buffer at the head of a queue.
**
**  Parameters:     Name        Description.
**                  bp          Pointer to NPU buffer.
**                  queue       Pointer to queue.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void npuBipQueuePrepend(NpuBuffer *bp, NpuQueue *queue)
{
	if (bp != nullptr)
	{
		if (queue->first == nullptr)
		{
			queue->last = bp;
		}

		bp->next = queue->first;
		queue->first = bp;
	}
}

/*--------------------------------------------------------------------------
**  Purpose:        Extract a buffer from the head of the queue.
**
**  Parameters:     Name        Description.
**                  queue       Pointer to queue.
**
**  Returns:        Pointer to a buffer or nullptr if queue is empty.
**
**------------------------------------------------------------------------*/
NpuBuffer *npuBipQueueExtract(NpuQueue *queue)
{
	NpuBuffer *bp = queue->first;

	if (bp != nullptr)
	{
		queue->first = bp->next;
		if (queue->first == nullptr)
		{
			queue->last = nullptr;
		}
	}

	return(bp);
}

/*--------------------------------------------------------------------------
**  Purpose:        Get a pointer to the last buffer in queue (but don't
**                  extract it).
**
**  Parameters:     Name        Description.
**                  queue       Pointer to queue.
**
**  Returns:        Pointer to a buffer or nullptr if queue is empty.
**
**------------------------------------------------------------------------*/
NpuBuffer *npuBipQueueGetLast(NpuQueue *queue)
{
	return(queue->last);
}

/*--------------------------------------------------------------------------
**  Purpose:        Determine if queue has anything in it.
**
**  Parameters:     Name        Description.
**                  queue       Pointer to queue.
**
**  Returns:        TRUE if queue is empty, FALSE otherwise.
**                  queue is empty.
**
**------------------------------------------------------------------------*/
bool npuBipQueueNotEmpty(NpuQueue *queue)
{
	return(queue->first != nullptr);
}

/*--------------------------------------------------------------------------
**  Purpose:        Respond to service message order word
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void npuBipNotifyServiceMessage(u8 mfrId)
{
	MMainFrame *mfr = BigIron->chasis[mfrId];

	mfr->bipDownlineBuffer = npuBipBufGet(mfrId);
	if (npuHipDownlineBlock(mfr->bipDownlineBuffer, mfrId))
	{
		mfr->bipState = mfr->BipDownSvm;
	}
	else
	{
		npuBipBufRelease(mfr->bipDownlineBuffer, mfrId);
		mfr->bipDownlineBuffer = nullptr;
	}
}

/*--------------------------------------------------------------------------
**  Purpose:        Respond to output order word.
**
**  Parameters:     Name        Description.
**                  priority    output priority
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void npuBipNotifyData(int priority, u8 mfrId)
{
	MMainFrame *mfr = BigIron->chasis[mfrId];

	mfr->bipDownlineBuffer = npuBipBufGet(mfrId);
	if (npuHipDownlineBlock(mfr->bipDownlineBuffer, mfrId))
	{
		mfr->bipState = static_cast<MMainFrame::BipState>(mfr->BipDownDataLow + priority);
	}
	else
	{
		npuBipBufRelease(mfr->bipDownlineBuffer, mfrId);
		mfr->bipDownlineBuffer = nullptr;
	}
}

/*--------------------------------------------------------------------------
**  Purpose:        Respond to input retry order word.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void npuBipRetryInput(u8 mfrId)
{
	MMainFrame *mfr = BigIron->chasis[mfrId];

	/*
	**  Check if any more upline buffer is pending and send if necessary.
	*/
	if (mfr->bipUplineBuffer != nullptr)
	{
		npuHipUplineBlock(mfr->bipUplineBuffer, mfrId);
	}
}

/*--------------------------------------------------------------------------
**  Purpose:        Process downline message.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void npuBipNotifyDownlineReceived(u8 mfrId)
{
	MMainFrame *mfr = BigIron->chasis[mfrId];

	NpuBuffer *bp = mfr->bipDownlineBuffer;

	/*
	**  BIP loses ownership of the downline buffer.
	*/
	mfr->bipDownlineBuffer = nullptr;

	/*
	**  Hand over the buffer to SVM or TIP.
	*/
	// ReSharper disable once CppDefaultCaseNotHandledInSwitchStatement
	// ReSharper disable once CppIncompleteSwitchStatement
	switch (mfr->bipState)
	{
	case MMainFrame::BipState::BipDownSvm:
		npuSvmProcessBuffer(bp, mfrId);
		break;

	case MMainFrame::BipState::BipDownDataLow:
		npuTipProcessBuffer(bp, 0, mfrId);
		break;

	case MMainFrame::BipState::BipDownDataHigh:
		npuTipProcessBuffer(bp, 1, mfrId);
		break;
	}

	mfr->bipState = mfr->BipIdle;

	/*
	**  Check if any more upline buffer is pending and send if necessary.
	*/
	if (mfr->bipUplineBuffer != nullptr)
	{
		npuHipUplineBlock(mfr->bipUplineBuffer, mfrId);
	}
}

/*--------------------------------------------------------------------------
**  Purpose:        Abort downline message.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void npuBipAbortDownlineReceived(u8 mfrId)
{
	MMainFrame *mfr = BigIron->chasis[mfrId];

	/*
	**  Free buffer and reset state.
	*/
	npuBipBufRelease(mfr->bipDownlineBuffer, mfrId);
	mfr->bipDownlineBuffer = nullptr;
	mfr->bipState = mfr->BipIdle;

	/*
	**  Check if any more upline buffer is pending and send if necessary.
	*/
	if (mfr->bipUplineBuffer != nullptr)
	{
		npuHipUplineBlock(mfr->bipUplineBuffer, mfrId);
	}
}

/*--------------------------------------------------------------------------
**  Purpose:        Request upline transfer.
**
**  Parameters:     Name        Description.
**                  bp          pointer to NPU buffer to be transferred
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void npuBipRequestUplineTransfer(NpuBuffer *bp, u8 mfrId)
{
	MMainFrame *mfr = BigIron->chasis[mfrId];

	if (mfr->bipUplineBuffer != nullptr)
	{
		/*
		**  Upline buffer pending, so queue this one for later.
		*/
		npuBipQueueAppend(bp, mfr->bipUplineQueue);
		return;
	}

	/*
	**  Send this block now.
	*/
	mfr->bipUplineBuffer = bp;

	if (mfr->bipState == mfr->BipIdle)
	{
		npuHipUplineBlock(mfr->bipUplineBuffer, mfrId);
	}
}

/*--------------------------------------------------------------------------
**  Purpose:        Request upline transfer of canned SVM message.
**
**  Parameters:     Name        Description.
**                  msg         pointer to canned message data
**                  msgSize     number of bytes in the message
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void npuBipRequestUplineCanned(u8 *msg, int msgSize, u8 mfrId)
{
	//MMainFrame *mfr = BigIron->chasis[mfrId];

	NpuBuffer *bp = npuBipBufGet(mfrId);
	if (bp == nullptr)
	{
		return;
	}

	bp->numBytes = msgSize;
	memcpy(bp->data, msg, bp->numBytes);
	npuBipRequestUplineTransfer(bp, mfrId);
}

/*--------------------------------------------------------------------------
**  Purpose:        Respond to completion of upline transfer.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void npuBipNotifyUplineSent(u8 mfrId)
{
	MMainFrame *mfr = BigIron->chasis[mfrId];

	/*
	**  Transfer finished, so release the buffer.
	*/
	npuBipBufRelease(mfr->bipUplineBuffer, mfrId);

	/*
	**  Check if any more upline queued and send if necessary.
	*/
	mfr->bipUplineBuffer = npuBipQueueExtract(mfr->bipUplineQueue);
	if (mfr->bipUplineBuffer != nullptr)
	{
		npuHipUplineBlock(mfr->bipUplineBuffer, mfrId);
	}
}

/*
**--------------------------------------------------------------------------
**
**  Private Functions
**
**--------------------------------------------------------------------------
*/

/*---------------------------  End Of File  ------------------------------*/
