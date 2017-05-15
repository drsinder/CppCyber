#pragma once
#include "stdafx.h"
#include "MDevice.h"

/*
**  -----------------
**  Private Constants
**  -----------------
*/

/*
**  Function codes.
*/
#define FcCr405Deselect         00700
#define FcCr405GateToSec        00701
#define FcCr405ReadNonStop      00702
#define FcCr405StatusReq        00704

/*
**  Status codes.
*/
#define StCr405Ready            00000
#define StCr405NotReady         00001
#define StCr405EOF              00002
#define StCr405CompareErr       00004

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
typedef struct cr405Context
{
	const u16 *table;
	u32     getCardCycle;
	int     col;
	PpWord  card[80];
} Cr405Context;



class MCr405 :
	public MDevice
{
public:
	MCr405();
	MCr405(u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName);
	~MCr405();

	virtual void ShowStatus(void);
	virtual void Load(char *params);
	virtual void UnLoad(char *params);

	virtual void Dump(char *params);

	Cr405Context *cc;
	DevSlot *dp;


	typedef void (MCr405::*MCr405MbrFn)(void);

private:
	FcStatus  MCr405::cr405Func(PpWord funcCode);
	void  MCr405::cr405Io(void);
	void  MCr405::cr405Activate(void);
	void  MCr405::cr405Disconnect(void);
	void  MCr405::cr405NextCard(DevSlot *dp);
};



