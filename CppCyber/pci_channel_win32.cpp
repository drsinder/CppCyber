/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2011, Tom Hunter
**  C++ adaptation by Dale Sinder 2017
**
**  Name: pci_channel.cpp
**
**  Description:
**      Interface to PCI channel adapter.
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
#define DEBUG 0
#include <windows.h>
#include <setupapi.h>

// ReSharper disable once CppUnusedIncludeDirective
#include <winioctl.h>
#include "cyber_channel_win32.h"

/*
**  -----------------
**  Private Constants
**  -----------------
*/

#define PciCmdNop           0x0000
#define PciCmdFunction      0x2000
#define PciCmdFull          0x4000
#define PciCmdEmpty         0x6000
#define PciCmdActive        0x8000
#define PciCmdInactive      0xA000
#define PciCmdClear         0xC000
#define PciCmdMasterClear   0xE000

#define PciStaFull          0x2000
#define PciStaActive        0x4000
#define PciStaBusy          0x8000

#define PciMaskData         0x0FFF
#define PciMaskParity       0x1000
#define PciShiftParity      12

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
typedef struct pciParam
{
	PpWord		data;
} PciParam;

/*
**  ---------------------------
**  Private Function Prototypes
**  ---------------------------
*/
static FcStatus pciFunc(PpWord funcCode, u8 mfrId);
static void pciIo(u8 mfrId);
static PpWord pciIn();
static void pciOut(PpWord data);
static void pciFull();
static void pciEmpty();
static void pciActivate(u8 mfrId);
static void pciDisconnect(u8 mfrId);
static u16 pciFlags();
static void pciCmd(PpWord data);
static u16 pciStatus();
static u16 pciParity(PpWord val);
static BOOL GetDevicePath();
static BOOL GetDeviceHandle();

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
static PciParam *pci;
static PSP_DEVICE_INTERFACE_DETAIL_DATA pDeviceInterfaceDetail = nullptr;
static HANDLE hDevice = INVALID_HANDLE_VALUE;
static HDEVINFO hDevInfo;

#if DEBUG
static FILE *pciLog = nullptr;
static bool active = false;
#endif

/*
**--------------------------------------------------------------------------
**
**  Public Functions
**
**--------------------------------------------------------------------------
*/
/*--------------------------------------------------------------------------
**  Purpose:        Initialise PCI channel interface.
**
**  Parameters:     Name        Description.
**                  eqNo        equipment number
**                  unitNo      unit number
**                  channelNo   channel number the device is attached to
**                  deviceName  optional device file name
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void pciInit(u8 mfrID, u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName)
{
	(void)unitNo;
	(void)deviceName;

#if DEBUG
	if (pciLog == NULL)
	{
		pciLog = fopen("pcilog.txt", "wt");
	}
#endif

	/*
	**  Attach device to channel and initialise device control block.
	*/
	DevSlot *dp = channelAttach(channelNo, eqNo, DtPciChannel, mfrID);
	dp->activate = pciActivate;
	dp->disconnect = pciDisconnect;
	dp->func = pciFunc;
	dp->io = pciIo;
	dp->flags = pciFlags;
	dp->in = pciIn;
	dp->out = pciOut;
	dp->full = pciFull;
	dp->empty = pciEmpty;

	/*
	**  Allocate and initialise channel parameters.
	*/
	pci = static_cast<PciParam*>(calloc(1, sizeof(PciParam)));
	if (pci == nullptr)
	{
		fprintf(stderr, "Failed to allocate PCI channel context block\n");
		exit(1);
	}

	BOOL retValue = GetDeviceHandle();
	if (!retValue)
	{
		fprintf(stderr, "Can't open CYBER channel interface.\n");
		exit(1);
	}

	pciCmd(PciCmdMasterClear);

	/*
	**  Print a friendly message.
	*/
	printf("PCI channel interface initialised on channel %o unit %o\n", channelNo, unitNo);
}

/*
**--------------------------------------------------------------------------
**
**  Private Functions
**
**--------------------------------------------------------------------------
*/

/*--------------------------------------------------------------------------
**  Purpose:        Execute function code on channel.
**
**  Parameters:     Name        Description.
**                  funcCode    function code
**
**  Returns:        FcStatus
**
**------------------------------------------------------------------------*/
static FcStatus pciFunc(PpWord funcCode, u8 mfrId)
{
#if DEBUG
	fprintf(pciLog, "\n%06d PP:%02o CH:%02o f:%04o >   ",
		traceSequenceNo,
		activePpu->id,
		activeChannel->id,
		funcCode);
#endif

	pciCmd(static_cast<u16>(PciCmdFunction | funcCode | (pciParity(funcCode) << PciShiftParity)));

	return(FcAccepted);
}

/*--------------------------------------------------------------------------
**  Purpose:        Perform I/O on channel (not used).
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void pciIo(u8 mfrId)
{
}

/*--------------------------------------------------------------------------
**  Purpose:        Perform input from PCI channel.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static PpWord pciIn()
{
	PpWord data = pciStatus() & Mask12;

#if DEBUG
	fprintf(pciLog, " I(%03X)", data);
#endif

	return(data);
}

/*--------------------------------------------------------------------------
**  Purpose:        Save output for PCI channel.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void pciOut(PpWord data)
{
	pci->data = data;
}

/*--------------------------------------------------------------------------
**  Purpose:        Set the channel full with data previously saved
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void pciFull()
{
#if DEBUG
	fprintf(pciLog, " O(%03X)", pci->data);
#endif
	pciCmd(static_cast<u16>(PciCmdFull | pci->data | (pciParity(pci->data) << PciShiftParity)));
}

/*--------------------------------------------------------------------------
**  Purpose:        Set the channel empty
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void pciEmpty()
{
#if DEBUG
	fprintf(pciLog, " E");
#endif
	pciCmd(PciCmdEmpty);
}

/*--------------------------------------------------------------------------
**  Purpose:        Handle channel activation.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void pciActivate(u8 mfrId)
{
#if DEBUG
	fprintf(pciLog, " A");
	active = TRUE;
#endif
	pciCmd(PciCmdActive);
}

/*--------------------------------------------------------------------------
**  Purpose:        Handle disconnecting of channel.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void pciDisconnect(u8 mfrId)
{
#if DEBUG
	fprintf(pciLog, " D");
	active = FALSE;
#endif
	pciCmd(PciCmdInactive);
}

/*--------------------------------------------------------------------------
**  Purpose:        Update full/active channel flags.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static u16 pciFlags()
{
#if DEBUG
	u16 s = pciStatus();
	if (active)
	{
		//        fprintf(pciLog, " S(%04X)", s);
	}
	return(s);
#else
	return(pciStatus());
#endif
}

/*--------------------------------------------------------------------------
**  Purpose:        Send a PCI command
**
**  Parameters:     Name        Description.
**                  data        command data
**
**  Returns:        nothing
**
**------------------------------------------------------------------------*/
static void pciCmd(PpWord data)
{
	DWORD bytesReturned;
	u16 status = 0;

	do
	{
		DeviceIoControl(hDevice, IOCTL_CYBER_CHANNEL_GET, nullptr, 0, &status, sizeof(status), &bytesReturned, nullptr);
	} while ((status & PciStaBusy) != 0);

	DeviceIoControl(hDevice, IOCTL_CYBER_CHANNEL_PUT, &data, sizeof(data), nullptr, 0, &bytesReturned, nullptr);
}

/*--------------------------------------------------------------------------
**  Purpose:        Get PCI status.
**
**  Parameters:     Name        Description.
**
**  Returns:        PCI status word
**
**------------------------------------------------------------------------*/
static u16 pciStatus()
{
	DWORD bytesReturned;
	u16 data;

	DeviceIoControl(hDevice, IOCTL_CYBER_CHANNEL_GET, nullptr, 0, &data, sizeof(data), &bytesReturned, nullptr);
	return(data);
}

/*--------------------------------------------------------------------------
**  Purpose:        Calculate odd parity over 12 bit PP word.
**
**  Parameters:     Name        Description.
**                  data        12 bit PP word
**
**  Returns:        Parity
**
**------------------------------------------------------------------------*/
static u16 pciParity(PpWord data)
{
	u16 ret = 1;

	while (data != 0)
	{
		ret ^= data & 1;
		data >>= 1;
	}

	return(ret);
}

/*--------------------------------------------------------------------------
**  Purpose:        Determine the Windows path for the CYBER channel PCI
**                  board driver.
**
**  Parameters:     Name        Description.
**
**  Returns:        TRUE if successful, FALSE otherwise.
**
**------------------------------------------------------------------------*/
static BOOL GetDevicePath()
{
	SP_DEVICE_INTERFACE_DATA DeviceInterfaceData;
	SP_DEVINFO_DATA DeviceInfoData;

	ULONG size;
	// ReSharper disable once CppInitializedValueIsAlwaysRewritten
	BOOL status = true;
	TCHAR *DeviceName = nullptr;
	TCHAR *DeviceLocation = nullptr;

	//
	//  Retrieve the device information for all devices.
	//
	hDevInfo = SetupDiGetClassDevs(const_cast<LPGUID>(&GUID_DEVINTERFACE_CYBER_CHANNEL),
		nullptr,
		nullptr,
		DIGCF_DEVICEINTERFACE |
		DIGCF_PRESENT);

	//
	//  Initialize the SP_DEVICE_INTERFACE_DATA Structure.
	//
	DeviceInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

	//
	//  Determine how many devices are present.
	//
	int count = 0;
	while (SetupDiEnumDeviceInterfaces(hDevInfo,
		nullptr,
		const_cast<LPGUID>(&GUID_DEVINTERFACE_CYBER_CHANNEL),
		count++,  //Cycle through the available devices.
		&DeviceInterfaceData)
		// ReSharper disable once CppPossiblyErroneousEmptyStatements
		);

	//
	// Since the last call fails when all devices have been enumerated,
	// decrement the count to get the true device count.
	//
	count--;

	//
	//  If the count is zero then there are no devices present.
	//
	if (count == 0)
	{
		printf("No PLX devices are present and enabled in the system.\n");
		return FALSE;
	}

	//
	//  Initialize the appropriate data structures in preparation for
	//  the SetupDi calls.
	//
	DeviceInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
	DeviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

	//
	//  Loop through the device list to allow user to choose
	//  a device.  If there is only one device, select it
	//  by default.
	//
	int i = 0;
	while (SetupDiEnumDeviceInterfaces(hDevInfo,
		nullptr,
		const_cast<LPGUID>(&GUID_DEVINTERFACE_CYBER_CHANNEL),
		i,
		&DeviceInterfaceData))
	{

		//
		// Determine the size required for the DeviceInterfaceData
		//
		SetupDiGetDeviceInterfaceDetail(hDevInfo,
			&DeviceInterfaceData,
			nullptr,
			0,
			&size,
			nullptr);

		if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
		{
			printf("SetupDiGetDeviceInterfaceDetail failed, Error: %d", GetLastError());
			return FALSE;
		}

		pDeviceInterfaceDetail = static_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA>(malloc(size));

		if (!pDeviceInterfaceDetail)
		{
			printf("Insufficient memory.\n");
			return FALSE;
		}

		//
		// Initialize structure and retrieve data.
		//
		pDeviceInterfaceDetail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
		status = SetupDiGetDeviceInterfaceDetail(hDevInfo,
			&DeviceInterfaceData,
			pDeviceInterfaceDetail,
			size,
			nullptr,
			&DeviceInfoData);

		free(pDeviceInterfaceDetail);

		if (!status)
		{
			printf("SetupDiGetDeviceInterfaceDetail failed, Error: %d", GetLastError());
			return status;
		}

		//
		//  Get the Device Name
		//  Calls to SetupDiGetDeviceRegistryProperty require two consecutive
		//  calls, first to get required buffer size and second to get
		//  the data.
		//
		SetupDiGetDeviceRegistryProperty(hDevInfo,
			&DeviceInfoData,
			SPDRP_DEVICEDESC,
			nullptr,
			reinterpret_cast<PBYTE>(DeviceName),
			0,
			&size);

		if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
		{
			printf("SetupDiGetDeviceRegistryProperty failed, Error: %d", GetLastError());
			return FALSE;
		}

		DeviceName = static_cast<TCHAR*>(malloc(size));
		if (!DeviceName)
		{
			printf("Insufficient memory.\n");
			return FALSE;
		}

		status = SetupDiGetDeviceRegistryProperty(hDevInfo,
			&DeviceInfoData,
			SPDRP_DEVICEDESC,
			nullptr,
			reinterpret_cast<PBYTE>(DeviceName),
			size,
			nullptr);
		if (!status)
		{
			printf("SetupDiGetDeviceRegistryProperty failed, Error: %d", GetLastError());
			free(DeviceName);
			return status;
		}

		//
		//  Now retrieve the Device Location.
		//
		SetupDiGetDeviceRegistryProperty(hDevInfo,
			&DeviceInfoData,
			SPDRP_LOCATION_INFORMATION,
			nullptr,
			reinterpret_cast<PBYTE>(DeviceLocation),
			0,
			&size);

		if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
		{
			DeviceLocation = static_cast<TCHAR*>(malloc(size));

			if (DeviceLocation != nullptr)
			{
				status = SetupDiGetDeviceRegistryProperty(hDevInfo,
					&DeviceInfoData,
					SPDRP_LOCATION_INFORMATION,
					nullptr,
					reinterpret_cast<PBYTE>(DeviceLocation),
					size,
					nullptr);
				if (!status)
				{
					free(DeviceLocation);
					DeviceLocation = nullptr;
				}
			}

		}
		else
		{
			DeviceLocation = nullptr;
		}

		//
		// Print description.
		//
		printf("%d - ", i);
		printf("%s\n", DeviceName);

		if (DeviceLocation)
		{
			printf("        %s\n", DeviceLocation);
		}

		free(DeviceName);
		DeviceName = nullptr;

		if (DeviceLocation)
		{
			free(DeviceLocation);
			DeviceLocation = nullptr;
		}

		i++; // Cycle through the available devices.
	}

	//
	//  Select device.
	//
	int index = 0;
	if (count > 1)
	{
		printf("Too many CYBER channel boards\n");
		exit(1);
	}

	//
	//  Get information for specific device.
	//
	status = SetupDiEnumDeviceInterfaces(hDevInfo,
		nullptr,
		const_cast<LPGUID>(&GUID_DEVINTERFACE_CYBER_CHANNEL),
		index,
		&DeviceInterfaceData);

	if (!status)
	{
		printf("SetupDiEnumDeviceInterfaces failed, Error: %d", GetLastError());
		return status;
	}

	//
	// Determine the size required for the DeviceInterfaceData
	//
	SetupDiGetDeviceInterfaceDetail(hDevInfo,
		&DeviceInterfaceData,
		nullptr,
		0,
		&size,
		nullptr);

	if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
	{
		printf("SetupDiGetDeviceInterfaceDetail failed, Error: %d", GetLastError());
		return false;
	}

	pDeviceInterfaceDetail = static_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA>(malloc(size));

	if (!pDeviceInterfaceDetail)
	{
		printf("Insufficient memory.\n");
		return false;
	}

	//
	// Initialize structure and retrieve data.
	//
	pDeviceInterfaceDetail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

	status = SetupDiGetDeviceInterfaceDetail(hDevInfo,
		&DeviceInterfaceData,
		pDeviceInterfaceDetail,
		size,
		nullptr,
		&DeviceInfoData);
	if (!status)
	{
		printf("SetupDiGetDeviceInterfaceDetail failed, Error: %d", GetLastError());
		return status;
	}

	return status;
}

/*--------------------------------------------------------------------------
**  Purpose:        Open the CYBER channel PCI board driver.
**
**  Parameters:     Name        Description.
**
**  Returns:        PCI status word
**
**------------------------------------------------------------------------*/
static BOOL GetDeviceHandle()
{
	BOOL status = TRUE;

	if (pDeviceInterfaceDetail == nullptr)
	{
		status = GetDevicePath();
	}

	if (pDeviceInterfaceDetail == nullptr)
	{
		status = FALSE;
	}

	if (status)
	{
		//
		//  Get handle to device.
		//
		printf("\nDevice path = %s\n", pDeviceInterfaceDetail->DevicePath);
		hDevice = CreateFile(pDeviceInterfaceDetail->DevicePath,
			GENERIC_READ | GENERIC_WRITE,
			FILE_SHARE_READ | FILE_SHARE_WRITE,
			nullptr,
			OPEN_EXISTING,
			0,
			nullptr);

		if (hDevice == INVALID_HANDLE_VALUE)
		{
			status = FALSE;
			printf("CreateFile failed.  Error:%d", GetLastError());
		}
	}

	return(status);
}

/*---------------------------  End Of File  ------------------------------*/

