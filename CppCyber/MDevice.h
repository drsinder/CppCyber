#pragma once
class MDevice
{
public:
	MDevice();

	//virtual MDevice(u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName, u64 extra);  // ...

	virtual ~MDevice();

	typedef void (MDevice::*MDeviceMbrFn)(void);

	virtual void ShowStatus(void) = 0;
	virtual void Load(char *params) = 0;
	virtual void UnLoad(char *params) = 0;


	virtual void Dump(char *params) = 0;



};

