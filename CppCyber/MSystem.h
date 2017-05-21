/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2011, Tom Hunter
**  C++ adaptation by Dale Sinder 2017
**
**  Name: Mpp.h
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

#pragma once
class MSystem
{
public:
	MSystem();
	~MSystem();

	void Terminate() const;

	void InitStartup(char *config);
	void FinishInitFile() const;
	void CreateMainFrames();

	void InitDeadstart(u8 mfrId);
	void InitNpuConnections(u8 mfrId);
	void InitEquipment(u8 mfrId);
	static u32 ConvertEndian(u32 value);

	bool bigEndian;
	bool emulationActive;

	long clockIncrement;
	long setMHz;

#if MaxMainFrames == 2
	CRITICAL_SECTION SysPpMutex;
#endif
#if MaxMainFrames == 2 || MaxCpus == 2
	CRITICAL_SECTION ECSFlagMutex;
	CRITICAL_SECTION TraceMutex;
#endif
	long cpuRatio;

	ModelType modelType;

	long autoRemovePaper;
	long initCpus;
	long initMainFrames;
	char model[40];
	long memory;
	long ecsBanks;
	long esmBanks;
	long pps;

	CpWord *extMem;
	u32 extMaxMemory;

	u32 ecsFlagRegister;

	FILE *ecsHandle;

	MMainFrame *chasis[MaxMainFrames];

private:
	FILE *fcb;
	long sectionStart;
	char *startupFile = "cyber.ini";
	char deadstart[80];
	char equipment[80];
	char npuConnections[80];
	long chCount;
	
	union
	{
		u32 number;
		u8 bytes[4];
	} endianCheck;


	void InitCyber(char *config);

	bool initOpenSection(char *name);
	char *initGetNextLine() const;
	bool initGetOctal(char *entry, int defValue, long *value) const;
	bool initGetInteger(char *entry, int defValue, long *value) const;
	bool initGetDouble(char *entry, int defValue, double *value) const;
	bool initGetString(char *entry, char *defString, char *str, int strLen) const;

};


/*---------------------------  End Of File  ------------------------------*/


