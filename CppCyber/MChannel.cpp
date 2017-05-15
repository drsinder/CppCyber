#include "stdafx.h"
#include "MChannel.h"


MChannel::MChannel()
{
}


MChannel::~MChannel()
{
}

u8 MChannel::ch;

void MChannel::Init(u8 count)
{
	channelCount = count;

	/*
	**  Initialise all channels.
	*/
	for (ch = 0; ch < MaxChannels; ch++)
	{
		channels[ch] = new MChannel();
		channels[ch]->id = ch;
	}

	/*
	**  Print a friendly message.
	*/
	printf("Channels initialised (number of channels %o)\n", channelCount);
}


void MChannel::Terminate()
{
}


