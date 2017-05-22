/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2011, Tom Hunter
**  C++ adaptation by Dale Sinder 2017
**
**  Name: mux6676.cpp
**
**  Description:
**      Perform emulation of CDC 6676 data set controller (terminal mux).
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
// ReSharper disable once CppUnusedIncludeDirective
#include <sys/types.h>
// ReSharper disable once CppUnusedIncludeDirective
#include <memory.h>
#if defined(_WIN32)
#include <winsock.h>
#else
#include <pthread.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif
/*
**  -----------------
**  Private Constants
**  -----------------
*/

/*
**  Function codes.
*/
#define Fc6676Output            00001
#define Fc6676Status            00002
#define Fc6676Input             00003

#define Fc6676EqMask            07000
#define Fc6676EqShift           9

#define St6676ServiceFailure    00001
#define St6676InputRequired     00002
#define St6676ChannelAReserved  00004

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
typedef struct portParam
{
	u8          id;
	bool        active;
	SOCKET         connFd;
} PortParam;

/*
**  ---------------------------
**  Private Function Prototypes
**  ---------------------------
*/
static FcStatus mux6676Func(PpWord funcCode, u8 mfrId);
static void mux6676Io(u8 mfrId);
static void mux6676Activate(u8 mfrId);
static void mux6676Disconnect(u8 mfrId);
static void mux6676CreateThread(DevSlot *dp);
static int mux6676CheckInput(PortParam *mp);
static bool mux6676InputRequired(u8 mfrId);
#if defined(_WIN32)
static void mux6676Thread(void *param);
static void mux6676Thread1(void *param);
#else
static void *mux6676Thread(void *param);
static void *mux6676Thread1(void *param);

#endif

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
**  Purpose:        Initialise terminal multiplexer.
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
void mux6676Init(u8 mfrID, u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName)
{
	(void)unitNo;
	(void)deviceName;

	DevSlot *dp = channelAttach(channelNo, eqNo, DtMux6676, mfrID);

	dp->activate = mux6676Activate;
	dp->disconnect = mux6676Disconnect;
	dp->func = mux6676Func;
	dp->io = mux6676Io;

	/*
	**  Only one MUX6676 unit is possible per equipment.
	*/
	if (dp->context[0] != nullptr)
	{
		fprintf(stderr, "Only one MUX6676 unit is possible per equipment\n");
		exit(1);
	}

	PortParam *mp = static_cast<PortParam*>(calloc(1, sizeof(PortParam) * mux6676TelnetConns));
	if (mp == nullptr)
	{
		fprintf(stderr, "Failed to allocate MUX6676 context block\n");
		exit(1);
	}

	dp->context[0] = mp;

	/*
	**  Initialise port control blocks.
	*/
	for (u8 i = 0; i < mux6676TelnetConns; i++)
	{
		mp->active = false;
		mp->connFd = 0;
		mp->id = i;
		mp += 1;
	}

	/*
	**  Create the thread which will deal with TCP connections.
	*/
	mux6676CreateThread(dp);

	/*
	**  Print a friendly message.
	*/
	printf("MUX6676 initialised on channel %o equipment %o mainframe %o\n", channelNo, eqNo, mfrID);
}

/*--------------------------------------------------------------------------
**  Purpose:        Execute function code on 6676 mux.
**
**  Parameters:     Name        Description.
**                  funcCode    function code
**
**  Returns:        FcStatus
**
**------------------------------------------------------------------------*/
static FcStatus mux6676Func(PpWord funcCode, u8 mfrId)
{
	MMainFrame *mfr = BigIron->chasis[mfrId];

	// ReSharper disable once CppEntityNeverUsed
	PortParam *mp = static_cast<PortParam *>(mfr->activeDevice->context[0]);

	u8 eqNo = (funcCode & Fc6676EqMask) >> Fc6676EqShift;
	if (eqNo != mfr->activeDevice->eqNo)
	{
		/*
		**  Equipment not configured.
		*/
		return(FcDeclined);
	}

	funcCode &= ~Fc6676EqMask;

	switch (funcCode)
	{
	default:
		return(FcDeclined);

	case Fc6676Output:
	case Fc6676Status:
	case Fc6676Input:
		mfr->activeDevice->recordLength = 0;
		break;
	}

	mfr->activeDevice->fcode = funcCode;
	return(FcAccepted);
}

/*--------------------------------------------------------------------------
**  Purpose:        Perform I/O on 6676 mux.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void mux6676Io(u8 mfrId)
{
	MMainFrame *mfr = BigIron->chasis[mfrId];
	// ReSharper disable once CppJoinDeclarationAndAssignment
	char x;
	PortParam *cp = static_cast<PortParam *>(mfr->activeDevice->context[0]);
	PortParam *mp;
	u8 portNumber;
	int in;

	switch (mfr->activeDevice->fcode)
	{
	default:
		break;

	case Fc6676Output:
		if (mfr->activeChannel->full)
		{
			/*
			**  Output data.
			*/
			mfr->activeChannel->full = false;
			portNumber = static_cast<u8>(mfr->activeDevice->recordLength++);
			if (portNumber < mux6676TelnetConns)
			{
				mp = cp + portNumber;
				if (mp->active)
				{
					/*
					**  Port with active TCP connection.
					*/
					PpWord function = mfr->activeChannel->data >> 9;
					switch (function)
					{
					case 4:
						/*
						**  Send data with parity stripped off.
						*/
						// ReSharper disable once CppJoinDeclarationAndAssignment
						x = (mfr->activeChannel->data >> 1) & 0x7f;
						send(mp->connFd, &x, 1, 0);
						break;

					case 6:
						/*
						**  Disconnect.
						*/
#if defined(_WIN32)
						closesocket(mp->connFd);
#else
						close(mp->connFd);
#endif
						mp->active = false;
						printf("mux6676: Host closed connection on port %d\n", mp->id);
						break;

					default:
						break;
					}
				}
			}
		}
		break;

	case Fc6676Input:
		if (!mfr->activeChannel->full)
		{
			mfr->activeChannel->data = 0;
			mfr->activeChannel->full = true;
			portNumber = static_cast<u8>(mfr->activeDevice->recordLength++);
			if (portNumber < mux6676TelnetConns)
			{
				mp = cp + portNumber;
				if (mp->active)
				{
					/*
					**  Port with active TCP connection.
					*/
					mfr->activeChannel->data |= 01000;
					if ((in = mux6676CheckInput(mp)) > 0)
					{
						mfr->activeChannel->data |= ((in & 0x7F) << 1) | 04000;
					}
				}
			}
		}
		break;

	case Fc6676Status:
		mfr->activeChannel->data = St6676ChannelAReserved;
		if (mux6676InputRequired(mfrId))
		{
			mfr->activeChannel->data |= St6676InputRequired;
		}

		mfr->activeChannel->full = true;
		break;
	}
}

/*--------------------------------------------------------------------------
**  Purpose:        Handle channel activation.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void mux6676Activate(u8 mfrId)
{
}

/*--------------------------------------------------------------------------
**  Purpose:        Handle disconnecting of channel.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void mux6676Disconnect(u8 mfrId)
{
}

/*--------------------------------------------------------------------------
**  Purpose:        Create thread which will deal with all TCP
**                  connections.
**
**  Parameters:     Name        Description.
**                  dp          pointer to device descriptor
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void mux6676CreateThread(DevSlot *dp)
{
#if defined(_WIN32)
	static bool firstMux = true;
	DWORD dwThreadId;

	if (firstMux)
	{
		firstMux = false;
	}

	/*
	**  Create TCP thread.
	*/
	HANDLE hThread = CreateThread(
		nullptr,                                       // no security attribute 
		0,                                          // default stack size 
		reinterpret_cast<LPTHREAD_START_ROUTINE>(dp->mfrID==0 ? mux6676Thread : mux6676Thread1),
		static_cast<LPVOID>(dp),                                 // thread parameter 
		0,                                          // not suspended 
		&dwThreadId);                               // returns thread ID 

	if (hThread == nullptr)
	{
		fprintf(stderr, "Failed to create mux6676 thread\n");
		exit(1);
	}
#else
	int rc;
	pthread_t thread;
	pthread_attr_t attr;

	/*
	**  Create POSIX thread with default attributes.
	*/
	pthread_attr_init(&attr);
	rc = pthread_create(&thread, &attr, mux6676Thread, dp);
	if (rc < 0)
	{
		fprintf(stderr, "Failed to create mux6676 thread\n");
		exit(1);
	}
#endif
}

/*--------------------------------------------------------------------------
**  Purpose:        TCP thread.
**
**  Parameters:     Name        Description.
**                  mp          pointer to mux parameters.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
#if defined(_WIN32)
static void mux6676Thread(void *param)
#else
static void *mux6676Thread(void *param)
#endif
{
	DevSlot *dp = static_cast<DevSlot *>(param);
	struct sockaddr_in server;
	struct sockaddr_in from;
	u8 i;
	int reuse = 1;
#if defined(_WIN32)
#else
	socklen_t fromLen;
#endif

	/*
	**  Create TCP socket and bind to specified port.
	*/
	SOCKET listenFd = socket(AF_INET, SOCK_STREAM, 0);
	if (listenFd < 0)
	// ReSharper disable once CppUnreachableCode
	{
		printf("mux6676: Can't create socket\n");
#if defined(_WIN32)
		return;
#else
		return(NULL);
#endif
	}

	setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char *>(&reuse), sizeof(reuse));
	memset(&server, 0, sizeof(server));
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = inet_addr("0.0.0.0");
	server.sin_port = htons(mux6676TelnetPort + dp->mfrID);

	if (bind(listenFd, reinterpret_cast<struct sockaddr *>(&server), sizeof(server)) < 0)
	{
		printf("mux6676: Can't bind to socket\n");
#if defined(_WIN32)
		return;
#else
		return(NULL);
#endif
	}

	if (listen(listenFd, 5) < 0)
	{
		printf("mux6676: Can't listen\n");
#if defined(_WIN32)
		return;
#else
		return(NULL);
#endif
	}

	while (true)
	{
		/*
		**  Find a free port control block.
		*/
		PortParam *mp = static_cast<PortParam *>(dp->context[dp->selectedUnit]);
		for (i = 0; i < mux6676TelnetConns; i++)
		{
			if (!mp->active)
			{
				break;
			}
			mp += 1;
		}

		if (i == mux6676TelnetConns)
		{
			/*
			**  No free port found - wait a bit and try again.
			*/
#if defined(_WIN32)
			Sleep(1000);
#else
			sleep(1);
#endif
			continue;
		}

		/*
		**  Wait for a connection.
		*/
		int fromLen = sizeof(from);
		mp->connFd = accept(listenFd, reinterpret_cast<struct sockaddr *>(&from), &fromLen);

		/*
		**  Mark connection as active.
		*/
		mp->active = true;
		printf("mux6676: Received connection on port %d\n", mp->id);
	}

#if !defined(_WIN32)
	return(NULL);
#endif
}

#if defined(_WIN32)
static void mux6676Thread1(void *param)
#else
static void *mux6676Thread1(void *param)
#endif
{
	DevSlot *dp = static_cast<DevSlot *>(param);
	struct sockaddr_in server;
	struct sockaddr_in from;
	u8 i;
	int reuse = 1;
#if defined(_WIN32)
#else
	socklen_t fromLen;
#endif

	/*
	**  Create TCP socket and bind to specified port.
	*/
	SOCKET listenFd = socket(AF_INET, SOCK_STREAM, 0);
	if (listenFd < 0)
		// ReSharper disable once CppUnreachableCode
	{
		printf("mux6676: Can't create socket\n");
#if defined(_WIN32)
		return;
#else
		return(NULL);
#endif
	}

	setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char *>(&reuse), sizeof(reuse));
	memset(&server, 0, sizeof(server));
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = inet_addr("0.0.0.0");
	server.sin_port = htons(mux6676TelnetPort + dp->mfrID);

	if (bind(listenFd, reinterpret_cast<struct sockaddr *>(&server), sizeof(server)) < 0)
	{
		printf("mux6676: Can't bind to socket\n");
#if defined(_WIN32)
		return;
#else
		return(NULL);
#endif
	}

	if (listen(listenFd, 5) < 0)
	{
		printf("mux6676: Can't listen\n");
#if defined(_WIN32)
		return;
#else
		return(NULL);
#endif
	}

	while (true)
	{
		/*
		**  Find a free port control block.
		*/
		PortParam *mp = static_cast<PortParam *>(dp->context[dp->selectedUnit]);
		for (i = 0; i < mux6676TelnetConns; i++)
		{
			if (!mp->active)
			{
				break;
			}
			mp += 1;
		}

		if (i == mux6676TelnetConns)
		{
			/*
			**  No free port found - wait a bit and try again.
			*/
#if defined(_WIN32)
			Sleep(1000);
#else
			sleep(1);
#endif
			continue;
		}

		/*
		**  Wait for a connection.
		*/
		int fromLen = sizeof(from);
		mp->connFd = accept(listenFd, reinterpret_cast<struct sockaddr *>(&from), &fromLen);

		/*
		**  Mark connection as active.
		*/
		mp->active = true;
		printf("mux6676: Received connection on port %d\n", mp->id);
	}

#if !defined(_WIN32)
	return(NULL);
#endif
}



/*--------------------------------------------------------------------------
**  Purpose:        Check for input.
**
**  Parameters:     Name        Description.
**                  mp          pointer to mux parameters.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static int mux6676CheckInput(PortParam *mp)
{
	fd_set readFds;
	fd_set exceptFds;
	struct timeval timeout;
	char data;

	FD_ZERO(&readFds);
	FD_ZERO(&exceptFds);
	FD_SET(mp->connFd, &readFds);
	FD_SET(mp->connFd, &exceptFds);

	timeout.tv_sec = 0;
	timeout.tv_usec = 0;

	select(static_cast<int>(mp->connFd) + 1, &readFds, nullptr, &exceptFds, &timeout);
	if (FD_ISSET(mp->connFd, &readFds))
	{
		int i = recv(mp->connFd, &data, 1, 0);
		if (i == 1)
		{
			return(data);
		}
		else
		{
#if defined(_WIN32)
			closesocket(mp->connFd);
#else
			close(mp->connFd);
#endif
			mp->active = false;
			printf("mux6676: Connection dropped on port %d\n", mp->id);
			return(-1);
		}
	}
	else if (FD_ISSET(mp->connFd, &exceptFds))
	{
#if defined(_WIN32)
		closesocket(mp->connFd);
#else
		close(mp->connFd);
#endif
		mp->active = false;
		printf("mux6676: Connection dropped on port %d\n", mp->id);
		return(-1);
	}
	else
	{
		return(0);
	}
}

/*--------------------------------------------------------------------------
**  Purpose:        Determine if input is required.
**
**  Parameters:     Name        Description.
**
**  Returns:        TRUE if input is required, FALSE otherwise.
**
**------------------------------------------------------------------------*/
static bool mux6676InputRequired(u8 mfrId)
{
	MMainFrame *mfr = BigIron->chasis[mfrId];

	PortParam *cp = static_cast<PortParam *>(mfr->activeDevice->context[0]);
	PortParam *mp;
	fd_set readFds;
	fd_set exceptFds;
	struct timeval timeout;

	FD_ZERO(&readFds);
	FD_ZERO(&exceptFds);
	for (int i = 0; i < mux6676TelnetConns; i++)
	{
		mp = cp + i;
		if (mp->active)
		{
			FD_SET(mp->connFd, &readFds);
			FD_SET(mp->connFd, &exceptFds);
		}
	}

	timeout.tv_sec = 0;
	timeout.tv_usec = 0;

	// ReSharper disable once CppCStyleCast
	// ReSharper disable once CppDeclaratorMightNotBeInitialized
	int numSocks = select((int)mp->connFd + 1, &readFds, nullptr, &exceptFds, &timeout);

	return(numSocks > 0);
}

/*---------------------------  End Of File  ------------------------------*/
