#pragma once

/*
**  ----------------
**  Public Variables
**  ----------------
*/
//ChSlot *channel;
//ChSlot *activeChannel;
//DevSlot *activeDevice;
//u8 channelCount;


class MChannel
{
public:
	MChannel();
	~MChannel();
private:
	static u8 ch;

	//MDeviceBase     firstDevice;
	MDeviceBase		ioDevice;
	PpWord          data;               /* channel data */
	PpWord          status;             /* channel status */
	bool            active;             /* channel active flag */
	bool            full;               /* channel full flag */
	bool            discAfterInput;     /* disconnect channel after input flag */
	bool            flag;               /* optional channel flag */
	bool            inputPending;       /* input pending flag */
	bool            hardwired;          /* hardwired devices */
	u8              id;                 /* channel number */
	u8              delayStatus;        /* time to delay change of empty/full status */
	u8              delayDisconnect;    /* time to delay disconnect */
public:
	static void Init(u8 count);
	void Terminate();
};

MChannel *channels[MaxChannels];
