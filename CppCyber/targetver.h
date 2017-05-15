#pragma once
/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2011, Tom Hunter
**  C++ adaptation by Dale Sinder 2017
**
**  Name: const.h
**
**  Description:
**      This file defines public constants and macros
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
// Including SDKDDKVer.h defines the highest available Windows platform.

// If you wish to build your application for a previous Windows platform, include WinSDKVer.h and
// set the _WIN32_WINNT macro to the platform you wish to support before including SDKDDKVer.h.



//#define _WIN32_WINNT_WINXP                  0x0501 // Windows XP  
//#define _WIN32_WINNT_WS03                   0x0502 // Windows Server 2003  
//#define _WIN32_WINNT_WIN6                   0x0600 // Windows Vista  
//#define _WIN32_WINNT_VISTA                  0x0600 // Windows Vista  
//#define _WIN32_WINNT_WS08                   0x0600 // Windows Server 2008  
//#define _WIN32_WINNT_LONGHORN               0x0600 // Windows Vista  
//#define _WIN32_WINNT_WIN7                   0x0601 // Windows 7  
//#define _WIN32_WINNT_WIN8                   0x0602 // Windows 8  
//#define _WIN32_WINNT_WINBLUE                0x0603 // Windows 8.1  
//#define _WIN32_WINNT_WINTHRESHOLD           0x0A00 // Windows 10  
#define _WIN32_WINNT_WIN10                  0x0A00	// Windows 10  

#define WINVER								0x0A00	// Windows 10

#include <WinSDKVer.h>

#include <SDKDDKVer.h>
