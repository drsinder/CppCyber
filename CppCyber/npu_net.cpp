/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2011, Tom Hunter
**  C++ adaptation by Dale Sinder 2017
**
**  Name: npu_net.cpp
**
**  Description:
**      Provides TCP/IP networking interface to the ASYNC TIP in an NPU
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
//#include "npu.h"
// ReSharper disable once CppUnusedIncludeDirective
#include <sys/types.h>
// ReSharper disable once CppUnusedIncludeDirective
#include <memory.h>
#if defined(_WIN32)
#include <winsock.h>
#else
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#endif

/*
**  -----------------
**  Private Constants
**  -----------------
*/
#define Ms200       200000

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

///*
//**  Registered NPU connection types.
//*/
//typedef struct npuConnType
//{
//	u16                 tcpPort;
//	int                 numConns;
//	u8                  connType;
//	u8					mfrId;
//	Tcb                 *startTcb;
//} NpuConnType;

/*
**  ---------------------------
**  Private Function Prototypes
**  ---------------------------
*/
static void npuNetCreateThread(u8 mfrID);
#if defined(_WIN32)
static void npuNetThread(void *param);
static void npuNetThread1(void *param);
#else
static void *npuNetThread(void *param);
static void *npuNetThread1(void *param);
#endif
static void npuNetProcessNewConnection(int acceptFd, NpuConnType *ct, u8 mfrId);
static void npuNetQueueOutput(Tcb *tp, u8 *data, int len, u8 mfrId);
static void npuNetTryOutput(Tcb *tp, u8 mfrId);

/*
**  ----------------
**  Public Variables
**  ----------------
*/
//u16 npuNetTcpConns = 0;

/*
**  -----------------
**  Private Variables
**  -----------------
*/

static const char connectingMsg[] = "\r\nConnecting to host - please wait ...\r\n";
static const char connectedMsg[] = "\r\nConnected\r\n\n";
static const char abortMsg[] = "\r\nConnection aborted\r\n";
static const char networkDownMsg[] = "Network going down - connection aborted\r\n";
static const char notReadyMsg[] = "\r\nHost not ready to accept connections - please try again later.\r\n";
static const char noPortsAvailMsg[] = "\r\nNo free ports available - please try again later.\r\n";

/*
**--------------------------------------------------------------------------
**
**  Public Functions
**
**--------------------------------------------------------------------------
*/

/*--------------------------------------------------------------------------
**  Purpose:        Register connection type
**
**  Parameters:     Name        Description.
**                  tcpPort     TCP port number
**                  numConns    Number of connections on this TCP port
**                  connType    Connection type (raw/pterm/rs232)
**
**  Returns:        NpuNetRegOk: successfully registered
**                  NpuNetRegOvfl: too many connection types
**                  NpuNetRegDupl: duplicate TCP port specified
**
**------------------------------------------------------------------------*/
int npuNetRegister(int tcpPort, int numConns, int connType, u8 mfrId)
{
	MMainFrame *mfr = BigIron->chasis[mfrId];
	/*
	** Check for too many registrations.
	*/
	if (mfr->numConnTypes >= MaxConnTypes)
	{
		return(NpuNetRegOvfl);
	}

	/*
	**  Check for duplicate TCP ports.
	*/
	for (int i = 0; i <mfr->numConnTypes; i++)
	{
		if (mfr->connTypes[i].tcpPort == tcpPort)
		{
			return(NpuNetRegDupl);
		}
	}

	/*
	**  Register this port.
	*/
	mfr->connTypes[mfr->numConnTypes].tcpPort = tcpPort;
	mfr->connTypes[mfr->numConnTypes].numConns = numConns;
	mfr->connTypes[mfr->numConnTypes].connType = connType;
	mfr->connTypes[mfr->numConnTypes].mfrId = mfrId;
	mfr->numConnTypes += 1;
	mfr->npuNetTcpConns += numConns;

	return(NpuNetRegOk);
}

/*--------------------------------------------------------------------------
**  Purpose:        Initialise network connection handler.
**
**  Parameters:     Name        Description.
**                  startup     FALSE when restarting (NAM restart),
**                              TRUE on first call during initialisation.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void npuNetInit(bool startup, u8 mfrId)
{
	int i;
	MMainFrame *mfr = BigIron->chasis[mfrId];

	//if (mfrId == 0)
	{

		/*
		**  Initialise network part of TCBs.
		*/
		Tcb *tp = mfr->npuTcbs;
		for (i = 0; i < mfr->npuNetTcpConns; i++, tp++)
		{
			tp->state = StTermIdle;
			tp->connFd = 0;
		}

		/*
		** Initialise connection type specific TCB values.
		*/
		tp = mfr->npuTcbs;
		for (i = 0; i < mfr->numConnTypes; i++)
		{
			mfr->connTypes[i].startTcb = tp;
			int numConns = mfr->connTypes[i].numConns;
			u8 connType = mfr->connTypes[i].connType;

			for (int j = 0; j < numConns; j++, tp++)
			{
				tp->connType = connType;
				tp->mfrId = mfr->connTypes[i].mfrId;
			}
		}

		/*
		**  Setup for input data processing.
		*/
		mfr->pollIndex = mfr->npuNetTcpConns;

	}
	/*
	**  Only do the following when the emulator starts up.
	*/
	if (startup)
	{
		/*
		**  Disable SIGPIPE which some non-Win32 platform generate on disconnect.
		*/
#ifndef WIN32
		signal(SIGPIPE, SIG_IGN);
#endif

		/*
		**  Create the thread which will deal with TCP connections.
		*/
		npuNetCreateThread(mfrId);
	}
}

/*--------------------------------------------------------------------------
**  Purpose:        Reset network connection handler when network is going
**                  down.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void npuNetReset(u8 mfrId)
{
	MMainFrame *mfr = BigIron->chasis[mfrId];

	Tcb *tp = mfr->npuTcbs;

	/*
	**  Iterate through all TCBs.
	*/
	for (int i = 0; i < mfr->npuNetTcpConns; i++, tp++)
	{
		if (tp->state != StTermIdle)
		{
			/*
			**  Notify user that network is going down and then disconnect.
			*/
			send(tp->connFd, networkDownMsg, sizeof(networkDownMsg) - 1, 0);

#if defined(_WIN32)
			closesocket(tp->connFd);
#else
			close(tp->connFd);
#endif

			tp->state = StTermIdle;
			tp->connFd = 0;
		}
	}
}


/*--------------------------------------------------------------------------
**  Purpose:        Signal from host that connection has been established.
**
**  Parameters:     Name        Description.
**                  tp          TCB pointer
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void npuNetConnected(Tcb *tp)
{
	tp->state = StTermHostConnected;
	send(tp->connFd, connectedMsg, sizeof(connectedMsg) - 1, 0);
}

/*--------------------------------------------------------------------------
**  Purpose:        Signal from host that connection has been terminated.
**
**  Parameters:     Name        Description.
**                  tp          TCB pointer
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void npuNetDisconnected(Tcb *tp)
{
	/*
	**  Received disconnect - close socket.
	*/
#if defined(_WIN32)
	closesocket(tp->connFd);
#else
	close(tp->connFd);
#endif

	/*
	**  Cleanup connection.
	*/
	tp->state = StTermIdle;
	npuLogMessage("npuNet: Connection dropped on port %d\n", tp->portNumber);
}

/*--------------------------------------------------------------------------
**  Purpose:        Prepare to send data to terminal.
**
**  Parameters:     Name        Description.
**                  tp          TCB pointer
**                  data        data address
**                  len         data length
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void npuNetSend(Tcb *tp, u8 *data, int len, u8 mfrId)
{
	u8 *p;
	int count;

	// ReSharper disable once CppDefaultCaseNotHandledInSwitchStatement
	switch (tp->connType)
	{
	case ConnTypePterm:
		/*
		**  Telnet escape processing as required by Pterm.
		*/
		for (p = data; len > 0; len -= 1)
		{
			// ReSharper disable once CppDefaultCaseNotHandledInSwitchStatement
			switch (*p++)
			{
			case 0xFF:
				/*
				**  Double FF to escape the Telnet IAC code making it a real FF.
				*/
				count = static_cast<int>(p - data);
				npuNetQueueOutput(tp, data, count, mfrId);
				npuNetQueueOutput(tp, reinterpret_cast<u8 *>("\xFF"), 1, mfrId);
				data = p;
				break;

			case 0x0D:
				/*
				**  Append zero to CR otherwise real zeroes will be stripped by Telnet.
				*/
				count = static_cast<int>(p - data);
				npuNetQueueOutput(tp, data, count, mfrId);
				npuNetQueueOutput(tp, reinterpret_cast<u8 *>("\x00"), 1, mfrId);
				data = p;
				break;
			}
		}

		if ((count = static_cast<int>(p - data)) > 0)
		{
			npuNetQueueOutput(tp, data, count, mfrId);
		}
		break;

	case ConnTypeRaw:
	case ConnTypeRs232:
		/*
		**  Standard (non-Telnet) TCP connection.
		*/
		npuNetQueueOutput(tp, data, len, mfrId);
		break;
	}
}

/*--------------------------------------------------------------------------
**  Purpose:        Store block sequence number to acknowledge when send
**                  has completed in last buffer.
**
**  Parameters:     Name        Description.
**                  tp          TCB pointer
**                  blockSeqNo  block sequence number to acknowledge.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void npuNetQueueAck(Tcb *tp, u8 blockSeqNo, u8 mfrId)
{
	/*
	**  Try to use the last pending buffer unless it carries a sequence number
	**  which must be acknowledged. If there is none, get a new one and queue it.
	*/
	NpuBuffer *bp = npuBipQueueGetLast(&tp->outputQ);
	if (bp == nullptr || bp->blockSeqNo != 0)
	{
		bp = npuBipBufGet(mfrId);
		npuBipQueueAppend(bp, &tp->outputQ);
	}

	if (bp != nullptr)
	{
		bp->blockSeqNo = blockSeqNo;
	}

	/*
	**  Try to output the data on the network connection.
	*/
	npuNetTryOutput(tp, mfrId);
}

/*--------------------------------------------------------------------------
**  Purpose:        Check for network status.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void npuNetCheckStatus(u8 mfrId)
{
	static fd_set readFds;
	static fd_set writeFds;
	struct timeval timeout;
	MMainFrame *mfr = BigIron->chasis[mfrId];

	// ReSharper disable once CppInitializedValueIsAlwaysRewritten
	int readySockets = 0;
	Tcb *tp;

	timeout.tv_sec = 0;
	timeout.tv_usec = 0;

	while (mfr->pollIndex < mfr->npuNetTcpConns)
	{
		tp = mfr->npuTcbs + mfr->pollIndex++;

		if (tp->state == StTermIdle)
		{
			continue;
		}

		/*
		**  Handle transparent input timeout.
		*/
		if (tp->xInputTimerRunning && labs(mfr->activeChannel->mfr->cycles - tp->xStartCycle) >= Ms200)
		{
			npuAsyncFlushUplineTransparent(tp, mfrId);
		}

		/*
		**  Handle network traffic.
		*/
		FD_ZERO(&readFds);
		FD_ZERO(&writeFds);
		FD_SET(tp->connFd, &readFds);
		FD_SET(tp->connFd, &writeFds);
		readySockets = select(tp->connFd + 1, &readFds, &writeFds, nullptr, &timeout);
		if (readySockets <= 0)
		{
			continue;
		}

		if (npuBipQueueNotEmpty(&tp->outputQ) && FD_ISSET(tp->connFd, &writeFds))
		{
			/*
			**  Send data if any is pending.
			*/
			npuNetTryOutput(tp, mfrId);
		}

		if (FD_ISSET(tp->connFd, &readFds))
		{
			/*
			**  Receive a block of data.
			*/
			tp->inputCount = recv(tp->connFd, reinterpret_cast<char*>(tp->inputData), sizeof(tp->inputData), 0);
			if (tp->inputCount <= 0)
			{
				/*
				**  Received disconnect - close socket.
				*/
#if defined(_WIN32)
				closesocket(tp->connFd);
#else
				close(tp->connFd);
#endif

				npuLogMessage("npuNet: Connection dropped on port %d\n", tp->portNumber);

				/*
				**  Notify SVM.
				*/
				npuSvmDiscRequestTerminal(tp, mfrId);
			}
			else if (tp->state == StTermHostConnected)
			{
				/*
				**  Hand up to the ASYNC TIP.
				*/
				npuAsyncProcessUplineData(tp, mfrId);
			}

			/*
			**  The following return ensures that we resume with polling the next
			**  connection in sequence otherwise low-numbered connections would get
			**  preferential treatment.
			*/
			return;
		}
	}

	mfr->pollIndex = 0;
}

/*
**--------------------------------------------------------------------------
**
**  Private Functions
**
**--------------------------------------------------------------------------
*/

/*--------------------------------------------------------------------------
**  Purpose:        Create thread which will deal with all TCP
**                  connections.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void npuNetCreateThread(u8 mfrId)
{
#if defined(_WIN32)
	DWORD dwThreadId;

	/*
	**  Create TCP thread.
	*/
	HANDLE hThread = CreateThread(
		nullptr,                                       // no security attribute 
		0,                                          // default stack size 
		reinterpret_cast<LPTHREAD_START_ROUTINE>(mfrId == 0 ? npuNetThread : npuNetThread1),
		reinterpret_cast<LPVOID>(mfrId),                               // thread parameter 
		0,                                          // not suspended 
		&dwThreadId);                               // returns thread ID 

	if (hThread == nullptr)
	{
		fprintf(stderr, "Failed to create npuNet thread\n");
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
	rc = pthread_create(&thread, &attr, (mfrId == 0 ? npuNetThread : npuNetThread1), NULL);
	if (rc < 0)
	{
		fprintf(stderr, "Failed to create npuNet thread\n");
		exit(1);
	}
#endif
}

/*--------------------------------------------------------------------------
**  Purpose:        TCP network connection thread.
**
**  Parameters:     Name        Description.
**                  param       unused
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
#if defined(_WIN32)
static void npuNetThread(void *param)
#else
static void *npuNetThread(void *param)
#endif
{
	u8 mfrId = reinterpret_cast<u8>(param);
	MMainFrame *mfr = BigIron->chasis[mfrId];

	static fd_set selectFds;
	static fd_set acceptFds;
	SOCKET listenFd[MaxConnTypes];
	SOCKET maxFd = 0;
	struct sockaddr_in server;
	struct sockaddr_in from;
	int i;
	int optEnable = 1;
#if defined(_WIN32)
	// ReSharper disable once CppJoinDeclarationAndAssignment
	int fromLen;
	u_long blockEnable = 1;
#else
	socklen_t fromLen;
#endif

	FD_ZERO(&selectFds);
	/*
	**  Create a listening socket for every configured connection type.
	*/
	for (i = 0; i < mfr->numConnTypes; i++)
	{
		//if (mfrId == 1)
		//	continue;
		/*
		**  Create TCP socket and bind to specified port.
		*/
		listenFd[i] = socket(AF_INET, SOCK_STREAM, 0);
		if (listenFd[i] < 0)
		// ReSharper disable once CppUnreachableCode
		{
			fprintf(stderr, "npuNet: Can't create socket\n");
#if defined(_WIN32)
			return;
#else
			return(NULL);
#endif
		}

		/*
		**  Accept will block if client drops connection attempt between select and accept.
		**  We can't block so make listening socket non-blocking to avoid this condition.
		*/
#if defined(_WIN32)
		ioctlsocket(listenFd[i], FIONBIO, &blockEnable);
#else
		fcntl(listenFd[i], F_SETFL, O_NONBLOCK);
#endif

		/*
		**  Bind to configured TCP port number
		*/
		setsockopt(listenFd[i], SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char *>(&optEnable), sizeof(optEnable));
		memset(&server, 0, sizeof(server));
		server.sin_family = AF_INET;
		server.sin_addr.s_addr = inet_addr("0.0.0.0");
		server.sin_port = htons(mfr->connTypes[i].tcpPort);

		if (bind(listenFd[i], reinterpret_cast<struct sockaddr *>(&server), sizeof(server)) < 0)
		{
			fprintf(stderr, "npuNet: Can't bind to socket\n");
#if defined(_WIN32)
			return;
#else
			return(NULL);
#endif
		}

		/*
		**  Start listening for new connections on this TCP port number
		*/
		if (listen(listenFd[i], 5) < 0)
		{
			fprintf(stderr, "npuNet: Can't listen\n");
#if defined(_WIN32)
			return;
#else
			return(NULL);
#endif
		}

		/*
		**  Determine highest FD for later select
		*/
		if (maxFd < listenFd[i])
		{
			maxFd = listenFd[i];
		}

		/*
		**  Add to set of listening FDs for later select
		*/
		FD_SET(listenFd[i], &selectFds);
	}

	for (;;)
	{
		/*
		**  Wait for a connection on all sockets for the configured connection types.
		*/
		memcpy(&acceptFds, &selectFds, sizeof(selectFds));
		int rc = select(static_cast<int>(maxFd) + 1, &acceptFds, nullptr, nullptr, nullptr);
		if (rc <= 0)
		{
			fprintf(stderr, "npuNetThread: select returned unexpected %d\n", rc);
#if defined(_WIN32)
			Sleep(1000);
#else
			sleep(1);
#endif
			continue;
		}

		/*
		**  Find the listening socket(s) with pending connections and accept them.
		*/
		for (i = 0; i < mfr->numConnTypes; i++)
		{
			//if (mfrId != mfr->connTypes[i].mfrId)
			//	continue;
			if (FD_ISSET(listenFd[i], &acceptFds))
			{
				// ReSharper disable once CppJoinDeclarationAndAssignment
				fromLen = sizeof(from);
				SOCKET acceptFd = accept(listenFd[i], reinterpret_cast<struct sockaddr *>(&from), &fromLen);
				if (acceptFd == -1)
				{
					printf("npuNetThread: spurious connection attempt\n");
					continue;
				}

				npuNetProcessNewConnection(static_cast<int>(acceptFd), mfr->connTypes + i, mfrId);
			}
		}
	}

#if !defined(_WIN32)
	return(NULL);
#endif
}

#if defined(_WIN32)
static void npuNetThread1(void *param)
#else
static void *npuNetThread1(void *param)
#endif
{
	u8 mfrId = reinterpret_cast<u8>(param);
	MMainFrame *mfr = BigIron->chasis[mfrId];

	static fd_set selectFds;
	static fd_set acceptFds;
	SOCKET listenFd[MaxConnTypes];
	// ReSharper disable once CppJoinDeclarationAndAssignment
	SOCKET acceptFd;
	SOCKET maxFd = 0;
	struct sockaddr_in server;
	struct sockaddr_in from;
	int i;
	int optEnable = 1;
#if defined(_WIN32)
	// ReSharper disable once CppJoinDeclarationAndAssignment
	int fromLen;
	u_long blockEnable = 1;
#else
	socklen_t fromLen;
#endif

	FD_ZERO(&selectFds);
	/*
	**  Create a listening socket for every configured connection type.
	*/
	for (i = 0; i < mfr->numConnTypes; i++)
	{
		//if (mfrId == 0)
		//	continue;

		/*
		**  Create TCP socket and bind to specified port.
		*/
		listenFd[i] = socket(AF_INET, SOCK_STREAM, 0);
		if (listenFd[i] < 0)
			// ReSharper disable once CppUnreachableCode
		{
			fprintf(stderr, "npuNet: Can't create socket\n");
#if defined(_WIN32)
			return;
#else
			return(NULL);
#endif
		}

		/*
		**  Accept will block if client drops connection attempt between select and accept.
		**  We can't block so make listening socket non-blocking to avoid this condition.
		*/
#if defined(_WIN32)
		ioctlsocket(listenFd[i], FIONBIO, &blockEnable);
#else
		fcntl(listenFd[i], F_SETFL, O_NONBLOCK);
#endif

		/*
		**  Bind to configured TCP port number
		*/
		setsockopt(listenFd[i], SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char *>(&optEnable), sizeof(optEnable));
		memset(&server, 0, sizeof(server));
		server.sin_family = AF_INET;
		server.sin_addr.s_addr = inet_addr("0.0.0.0");
		server.sin_port = htons(mfr->connTypes[i].tcpPort);

		if (bind(listenFd[i], reinterpret_cast<struct sockaddr *>(&server), sizeof(server)) < 0)
		{
			fprintf(stderr, "npuNet: Can't bind to socket\n");
#if defined(_WIN32)
			return;
#else
			return(nullptr);
#endif
		}

		/*
		**  Start listening for new connections on this TCP port number
		*/
		if (listen(listenFd[i], 5) < 0)
		{
			fprintf(stderr, "npuNet: Can't listen\n");
#if defined(_WIN32)
			return;
#else
			return(NULL);
#endif
		}

		/*
		**  Determine highest FD for later select
		*/
		if (maxFd < listenFd[i])
		{
			maxFd = listenFd[i];
		}

		/*
		**  Add to set of listening FDs for later select
		*/
		FD_SET(listenFd[i], &selectFds);
	}

	for (;;)
	{
		//if (mfrId != mfr->connTypes[i].mfrId)
		//	continue;
		/*
		**  Wait for a connection on all sockets for the configured connection types.
		*/
		memcpy(&acceptFds, &selectFds, sizeof(selectFds));
		int rc = select(static_cast<int>(maxFd) + 1, &acceptFds, nullptr, nullptr, nullptr);
		if (rc <= 0)
		{
			fprintf(stderr, "npuNetThread: select returned unexpected %d\n", rc);
#if defined(_WIN32)
			Sleep(1000);
#else
			sleep(1);
#endif
			continue;
		}

		/*
		**  Find the listening socket(s) with pending connections and accept them.
		*/
		for (i = 0; i < mfr->numConnTypes; i++)
		{
			if (FD_ISSET(listenFd[i], &acceptFds))
			{
				// ReSharper disable once CppJoinDeclarationAndAssignment
				fromLen = sizeof(from);
				// ReSharper disable once CppJoinDeclarationAndAssignment
				acceptFd = accept(listenFd[i], reinterpret_cast<struct sockaddr *>(&from), &fromLen);
				if (acceptFd == -1)
				{
					printf("npuNetThread: spurious connection attempt\n");
					continue;
				}

				npuNetProcessNewConnection(static_cast<int>(acceptFd), mfr->connTypes + i, mfrId);
			}
		}
	}

#if !defined(_WIN32)
	return(NULL);
#endif
}


/*--------------------------------------------------------------------------
**  Purpose:        Process new TCP connection
**
**  Parameters:     Name        Description.
**                  acceptFd    New connection's FD
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void npuNetProcessNewConnection(int acceptFd, NpuConnType *ct, u8 mfrId)
{
	MMainFrame *mfr = BigIron->chasis[mfrId];

	u8 i;
	int optEnable = 1;
#if defined(_WIN32)
	u_long blockEnable = 1;
#endif

	/*
	**  Set Keepalive option so that we can eventually discover if
	**  a client has been rebooted.
	*/
	setsockopt(acceptFd, SOL_SOCKET, SO_KEEPALIVE, reinterpret_cast<const char *>(&optEnable), sizeof(optEnable));

	/*
	**  Make socket non-blocking.
	*/
#if defined(_WIN32)
	ioctlsocket(acceptFd, FIONBIO, &blockEnable);
#else
	fcntl(acceptFd, F_SETFL, O_NONBLOCK);
#endif

	/*
	**  Check if the host is ready to accept connections.
	*/
	if (!npuSvmIsReady(mfrId))
	{
		/*
		**  Tell the user.
		*/
		send(acceptFd, notReadyMsg, sizeof(notReadyMsg) - 1, 0);

		/*
		**  Wait a bit and then disconnect.
		*/
#if defined(_WIN32)
		Sleep(2000);
		closesocket(acceptFd);
#else
		sleep(2);
		close(acceptFd);
#endif

		return;
	}

	/*
	**  Find a free TCB in the set of ports associated with this connection type.
	*/
	Tcb *tp = ct->startTcb;
	for (i = 0; i < ct->numConns; i++)
	{
		if (tp->state == StTermIdle)
		{
			break;
		}

		tp += 1;
	}

	/*
	**  Did we find a free TCB?
	*/
	if (i == ct->numConns)
	{
		/*
		**  No free port found - tell the user.
		*/
		send(acceptFd, noPortsAvailMsg, sizeof(noPortsAvailMsg) - 1, 0);

		/*
		**  Wait a bit and then disconnect.
		*/
#if defined(_WIN32)
		Sleep(2000);
		closesocket(acceptFd);
#else
		sleep(2);
		close(acceptFd);
#endif

		return;
	}

	/*
	**  Mark connection as active.
	*/
	tp->connFd = acceptFd;
	tp->state = StTermNetConnected;
	npuLogMessage("npuNet: Received connection on port %u\n", tp->portNumber);

	/*
	**  Notify user of connect attempt.
	*/
	send(tp->connFd, connectingMsg, sizeof(connectingMsg) - 1, 0);

	/*
	**  Attempt connection to host.
	*/
	if (!npuSvmConnectTerminal(tp, mfrId))
	{
		/*
		**  No buffers, notify user.
		*/
		send(tp->connFd, abortMsg, sizeof(abortMsg) - 1, 0);

		/*
		**  Wait a bit and then disconnect.
		*/
#if defined(_WIN32)
		Sleep(1000);
		closesocket(tp->connFd);
#else
		sleep(1);
		close(tp->connFd);
#endif

		tp->state = StTermIdle;
	}
}

/*--------------------------------------------------------------------------
**  Purpose:        Queue output to terminal and do basic Telnet formatting.
**
**  Parameters:     Name        Description.
**                  tp          TCB pointer
**                  data        data address
**                  len         data length
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void npuNetQueueOutput(Tcb *tp, u8 *data, int len, u8 mfrId)
{
	/*
	**  Try to use the last pending buffer unless it carries a sequence number
	**  which must be acknowledged. If there is none, get a new one and queue it.
	*/
	NpuBuffer *bp = npuBipQueueGetLast(&tp->outputQ);
	if (bp == nullptr || bp->blockSeqNo != 0)
	{
		bp = npuBipBufGet(mfrId);
		npuBipQueueAppend(bp, &tp->outputQ);
	}

	while (bp != nullptr && len > 0)
	{
		/*
		**  Append data to the buffer.
		*/
		u8 *startAddress = bp->data + bp->offset + bp->numBytes;
		int byteCount = MaxBuffer - bp->offset - bp->numBytes;
		if (byteCount >= len)
		{
			byteCount = len;
		}

		memcpy(startAddress, data, byteCount);
		bp->numBytes += byteCount;

		/*
		**  If there is still data left get a new buffer, queue it and
		**  copy what is left.
		*/
		len -= byteCount;
		if (len > 0)
		{
			bp = npuBipBufGet(mfrId);
			npuBipQueueAppend(bp, &tp->outputQ);
		}
	}
}

/*--------------------------------------------------------------------------
**  Purpose:        Try to send any queued data.
**
**  Parameters:     Name        Description.
**                  tp          TCB pointer
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void npuNetTryOutput(Tcb *tp, u8 mfrId)
{
	NpuBuffer *bp;
	int result;

	/*
	**  Return if we are flow controlled.
	*/
	if (tp->xoff)
	{
		return;
	}

	/*
	**  Process all queued output buffers.
	*/
	while ((bp = npuBipQueueExtract(&tp->outputQ)) != nullptr)
	{
		u8 *data = bp->data + bp->offset;

		/*
		**  Don't call into TCP if there is no data to send.
		*/
		if (bp->numBytes > 0)
		{
			result = send(tp->connFd, reinterpret_cast<const char*>(data), bp->numBytes, 0);
		}
		else
		{
			result = 0;
		}

		if (result >= bp->numBytes)
		{
			/*
			**  The socket took all our data - let TIP know what block sequence
			**  number we processed, free the buffer and then continue.
			*/
			if (bp->blockSeqNo != 0)
			{
				npuTipNotifySent(tp, bp->blockSeqNo, mfrId);
			}

			npuBipBufRelease(bp, mfrId);
			continue;
		}

		/*
		**  Not all has been sent. Put the buffer back into the queue.
		*/
		npuBipQueuePrepend(bp, &tp->outputQ);

		/*
		**  Was there an error?
		*/
		if (result < 0)
		{
			/*
			**  Likely this is a "would block" type of error - no need to do
			**  anything here. The select() call will later tell us when we
			**  can send again. Any disconnects or other errors will be handled
			**  by the receive handler.
			*/
			return;
		}

		/*
		**  The socket did not take all data - update offset and count.
		*/
		if (result > 0)
		{
			bp->offset += result;
			bp->numBytes -= result;
		}
	}
}

/*---------------------------  End Of File  ------------------------------*/
