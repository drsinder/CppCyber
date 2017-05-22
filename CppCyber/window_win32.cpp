/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2011, Tom Hunter
**  C++ adaptation by Dale Sinder 2017
**
**  Name: window_win32.cpp
**
**  Description:
**      Simulate CDC 6612 or CC545 console display on MS Windows.
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


#include <windows.h>

#include "resource.h"


/*
**  -----------------
**  Private Constants
**  -----------------
*/
//#define ListSize            5000
// useful for more stable screen shots
#define ListSize            50000
#define FontName            "Lucida Console"
#if CcLargeWin32Screen == 1
#define FontSmallHeight     15
#define FontMediumHeight    20
#define FontLargeHeight     30
#define ScaleX              11
#define ScaleY              18
#else
#define FontSmallHeight     10
#define FontMediumHeight    15
#define FontLargeHeight     20
#define ScaleX              10
#define ScaleY              10
#endif

#define TIMER_ID        1

#if CcDebug == 0
#define TIMER_RATE      50
#else
#define TIMER_RATE      300
#endif
//#define TIMER_RATE      75

//#define TIMER_RATE      1

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
typedef struct dispList
{
	u16             xPos;           /* horizontal position */
	u16             yPos;           /* vertical position */
	u8              fontSize;       /* size of font */
	u8              ch;             /* character to be displayed */
} DispList;

typedef enum { ModeLeft, ModeCenter, ModeRight } DisplayMode;

/*
**  ---------------------------
**  Private Function Prototypes
**  ---------------------------
*/
static void windowThread(LPVOID param);
ATOM windowRegisterClass(HINSTANCE hInstance);
BOOL windowCreate();
static void windowClipboard(HWND hWnd);
LRESULT CALLBACK windowProcedure(HWND, UINT, WPARAM, LPARAM);
void windowDisplay(HWND hWnd);

#if MaxMainFrames == 2
static void windowThread1(LPVOID param);
ATOM windowRegisterClass1(HINSTANCE hInstance1);
BOOL windowCreate1();
LRESULT CALLBACK windowProcedure1(HWND, UINT, WPARAM, LPARAM);
void windowDisplay1(HWND hWnd);
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

// for mainframe 0

static u8 currentFont;
static i16 currentX = -1;
static i16 currentY = -1;
static DispList display[ListSize];
static u32 listEnd;
static HWND hWnd;
static HFONT hSmallFont = nullptr;
static HFONT hMediumFont = nullptr;
static HFONT hLargeFont = nullptr;
static HPEN hPen = nullptr;
static HINSTANCE hInstance = nullptr;
static char *lpClipToKeyboard = nullptr;
static char *lpClipToKeyboardPtr = nullptr;
static u8 clipToKeyboardDelay = 0;
static DisplayMode displayMode = ModeCenter;
static bool displayModeNeedsErase = false;
static BOOL shifted = false;

#if MaxMainFrames == 2
// for mainframe 1

static u8 currentFont1;
static i16 currentX1 = -1;
static i16 currentY1 = -1;
static DispList display1[ListSize];
static u32 listEnd1;
static HWND hWnd1;
static HFONT hSmallFont1 = nullptr;
static HFONT hMediumFont1 = nullptr;
static HFONT hLargeFont1 = nullptr;
static HPEN hPen1 = nullptr;
static HINSTANCE hInstance1 = nullptr;
static char *lpClipToKeyboard1 = nullptr;
static char *lpClipToKeyboardPtr1 = nullptr;
static u8 clipToKeyboardDelay1 = 0;
static DisplayMode displayMode1 = ModeCenter;
static bool displayModeNeedsErase1 = false;
static BOOL shifted1 = false;
#endif

/*--------------------------------------------------------------------------
**  Purpose:        Create WIN32 thread which will deal with all windows
**                  functions.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void windowInit(u8 mfrID)
{
	DWORD dwThreadId;
	HANDLE hThread;

	/*
	**  Create windowing thread.
	*/
	if (mfrID == 0)
	{
		/*
		**  Create display list pool.
		*/
		listEnd = 0;

		/*
		**  Get our instance
		*/
		hInstance = GetModuleHandle(nullptr);

		hThread = CreateThread(
			nullptr,                                       // no security attribute 
			0,                                          // default stack size 
			reinterpret_cast<LPTHREAD_START_ROUTINE>(windowThread),
			reinterpret_cast<LPVOID>(mfrID),                              // thread parameter 
			0,                                          // not suspended 
			&dwThreadId);                               // returns thread ID 
	}
#if MaxMainFrames == 2
	else
	{
		/*
		**  Create display list pool.
		*/
		listEnd1 = 0;

		/*
		**  Get our instance
		*/
		hInstance1 = GetModuleHandle(nullptr);

		hThread = CreateThread(
			nullptr,                                       // no security attribute 
			0,                                          // default stack size 
			reinterpret_cast<LPTHREAD_START_ROUTINE>(windowThread1),
			reinterpret_cast<LPVOID>(mfrID),                              // thread parameter 
			0,                                          // not suspended 
			&dwThreadId);                               // returns thread ID 

	}
#endif
	// ReSharper disable once CppDeclaratorMightNotBeInitialized
	if (hThread == nullptr)
	{
		MessageBox(nullptr, "thread creation failed", "Error", MB_OK);
		exit(1);
	}
}

/*--------------------------------------------------------------------------
**  Purpose:        Set font size.
**                  functions.
**
**  Parameters:     Name        Description.
**                  size        font size in points.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void windowSetFont(u8 font)
{
	currentFont = font;
}
#if MaxMainFrames == 2
void windowSetFont1(u8 font)
{
	currentFont1 = font;
}
#endif
/*--------------------------------------------------------------------------
**  Purpose:        Set X coordinate.
**
**  Parameters:     Name        Description.
**                  x           horizontal coordinate (0 - 0777)
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void windowSetX(u16 x)
{
	currentX = x;
}
#if MaxMainFrames == 2
void windowSetX1(u16 x)
{
	currentX1 = x;
}
#endif
/*--------------------------------------------------------------------------
**  Purpose:        Set Y coordinate.
**
**  Parameters:     Name        Description.
**                  y           vertical coordinate (0 - 0777)
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void windowSetY(u16 y)
{
	currentY = 0777 - y;
}
#if MaxMainFrames == 2
void windowSetY1(u16 y)
{
	currentY1 = 0777 - y;
}
#endif
/*--------------------------------------------------------------------------
**  Purpose:        Queue characters.
**
**  Parameters:     Name        Description.
**                  ch          character to be queued.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/

void windowQueue(u8 ch)
{
	if (listEnd >= ListSize
		|| currentX == -1
		|| currentY == -1)
	{
		return;
	}

	if (ch != 0)
	{
		DispList *elem = display + listEnd++;
		elem->ch = ch;
		elem->fontSize = currentFont;
		elem->xPos = currentX;
		elem->yPos = currentY;
	}

	currentX += currentFont;
}
#if MaxMainFrames == 2
void windowQueue1(u8 ch)
{
	if (listEnd1 >= ListSize
		|| currentX1 == -1
		|| currentY1 == -1)
	{
		return;
	}

	if (ch != 0)
	{
		DispList *elem = display1 + listEnd1++;
		elem->ch = ch;
		elem->fontSize = currentFont1;
		elem->xPos = currentX1;
		elem->yPos = currentY1;
	}

	currentX1 += currentFont1;
}
#endif

/*--------------------------------------------------------------------------
**  Purpose:        Update window.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void windowUpdate()
{
}
#if MaxMainFrames == 2
void windowUpdate1()
{
}
#endif
/*--------------------------------------------------------------------------
**  Purpose:        Poll the keyboard (dummy for X11)
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing
**
**------------------------------------------------------------------------*/
void windowGetChar()
{
}
#if MaxMainFrames == 2
void windowGetChar1()
{
}
#endif
/*--------------------------------------------------------------------------
**  Purpose:        Terminate console window.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void windowTerminate()
{
	SendMessage(hWnd, WM_DESTROY, 0, 0);
	Sleep(100);
}
#if MaxMainFrames == 2
void windowTerminate1()
{
	SendMessage(hWnd1, WM_DESTROY, 0, 0);
	Sleep(100);
}
#endif
/*
**--------------------------------------------------------------------------
**
**  Private Functions
**
**--------------------------------------------------------------------------
*/

/*--------------------------------------------------------------------------
**  Purpose:        Windows thread.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void windowThread(LPVOID param)
{
	// ReSharper disable once CppEntityNeverUsed
	u8 mfrID = reinterpret_cast<u8>(param);
	MSG msg;

	/*
	**  Register the window class.
	*/
	windowRegisterClass(hInstance);

	/*
	**  Create the window.
	*/

	if (!windowCreate())
	{
		MessageBox(nullptr, "window creation failed", "Error", MB_OK);
		return;
	}

	/*
	**  Main message loop.
	*/
	while (GetMessage(&msg, nullptr, 0, 0) > 0)
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
}
#if MaxMainFrames == 2
static void windowThread1(LPVOID param)
{
	//u8 mfrID = (u8)param;
	MSG msg;

	/*
	**  Register the window class.
	*/
	windowRegisterClass1(hInstance1);

	/*
	**  Create the window.
	*/

	if (!windowCreate1())
	{
		MessageBox(nullptr, "window creation failed", "Error", MB_OK);
		return;
	}


	/*
	**  Main message loop.
	*/
	while (GetMessage(&msg, nullptr, 0, 0) > 0)
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
}
#endif

/*--------------------------------------------------------------------------
**  Purpose:        Register the window class.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
ATOM windowRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEX wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style = CS_HREDRAW | CS_VREDRAW | CS_NOCLOSE | CS_BYTEALIGNCLIENT;
	wcex.lpfnWndProc = static_cast<WNDPROC>(windowProcedure);
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, reinterpret_cast<LPCTSTR>(IDI_CONSOLE));
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
	wcex.lpszMenuName = MAKEINTRESOURCE(IDC_CONSOLE);
	wcex.lpszClassName = "CONSOLE";
	wcex.hIconSm = LoadIcon(hInstance, reinterpret_cast<LPCTSTR>(IDI_SMALL));

	return RegisterClassEx(&wcex);
}
#if MaxMainFrames == 2
ATOM windowRegisterClass1(HINSTANCE hInstance1)
{
	WNDCLASSEX wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style = CS_HREDRAW | CS_VREDRAW | CS_NOCLOSE | CS_BYTEALIGNCLIENT;
	wcex.lpfnWndProc = static_cast<WNDPROC>(windowProcedure1);
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance1;
	wcex.hIcon = LoadIcon(hInstance1, reinterpret_cast<LPCTSTR>(IDI_CONSOLE));
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
	wcex.lpszMenuName = MAKEINTRESOURCE(IDC_CONSOLE);
	wcex.lpszClassName = "CONSOLE1";
	wcex.hIconSm = LoadIcon(hInstance1, reinterpret_cast<LPCTSTR>(IDI_SMALL));

	return RegisterClassEx(&wcex);
}
#endif
/*--------------------------------------------------------------------------
**  Purpose:        Create the main window.
**
**  Parameters:     Name        Description.
**
**  Returns:        TRUE if successful, FALSE otherwise.
**
**------------------------------------------------------------------------*/
static BOOL windowCreate()
{
#if CcLargeWin32Screen == 1
	hWnd = CreateWindow(
		"CONSOLE",              // Registered class name
		 "Mainframe 0 - "
		DtCyberVersion " - "
		DtCyberCopyright " - "
		DtCyberLicense,         // window name
		WS_OVERLAPPEDWINDOW,    // window style
		//1800-1280,          // horizontal position of window
		CW_USEDEFAULT,          // horizontal position of window
		0,                      // vertical position of window
		1280,                   // window width
		1024,                   // window height
		nullptr,                   // handle to parent or owner window
		nullptr,                   // menu handle or child identifier
		nullptr,                      // handle to application instance
		nullptr);                  // window-creation data
#else
	hWnd = CreateWindow(
		"CONSOLE",              // Registered class name
		"Mainframe 0 - "
		DtCyberVersion " - "
		DtCyberCopyright " - "
		DtCyberLicense,         // window name
		WS_OVERLAPPEDWINDOW,    // window style
		CW_USEDEFAULT,          // horizontal position of window
		CW_USEDEFAULT,          // vertical position of window
		1080,                   // window width
		600,                    // window height
		nullptr,                   // handle to parent or owner window
		nullptr,                   // menu handle or child identifier
		nullptr,                      // handle to application instance
		nullptr);                  // window-creation data
#endif

	if (!hWnd)
	{
		return FALSE;
	}

	ShowWindow(hWnd, SW_SHOW);
	UpdateWindow(hWnd);

	SetTimer(hWnd, TIMER_ID, TIMER_RATE, nullptr);

	return TRUE;
}
#if MaxMainFrames == 2
static BOOL windowCreate1()
{
#if CcLargeWin32Screen == 1
	hWnd1 = CreateWindow(
		"CONSOLE1",              // Registered class name
		"Mainframe 1 - "
		DtCyberVersion " - "
		DtCyberCopyright " - "
		DtCyberLicense,         // window name
		WS_OVERLAPPEDWINDOW,    // window style
		//1800,
		CW_USEDEFAULT,          // horizontal position of window
		0,                      // vertical position of window
		1280,                   // window width
		1024,                   // window height
		nullptr,                   // handle to parent or owner window
		nullptr,                   // menu handle or child identifier
		nullptr,                      // handle to application instance
		nullptr);                  // window-creation data
#else
	hWnd1 = CreateWindow(
		"CONSOLE",              // Registered class name
		"Mainframe 1 - "
		DtCyberVersion " - "
		DtCyberCopyright " - "
		DtCyberLicense,         // window name
		WS_OVERLAPPEDWINDOW,    // window style
		CW_USEDEFAULT,          // horizontal position of window
		CW_USEDEFAULT,          // vertical position of window
		1080,                   // window width
		600,                    // window height
		nullptr,                   // handle to parent or owner window
		nullptr,                   // menu handle or child identifier
		nullptr,                      // handle to application instance
		nullptr);                  // window-creation data
#endif

	if (!hWnd1)
	{
		return FALSE;
	}

	ShowWindow(hWnd1, SW_SHOW);
	UpdateWindow(hWnd1);

	SetTimer(hWnd1, TIMER_ID, TIMER_RATE, nullptr);

	return TRUE;
}
#endif

/*--------------------------------------------------------------------------
**  Purpose:        Copy clipboard data to keyboard buffer.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void windowClipboard(HWND hWnd)
{
	if (!IsClipboardFormatAvailable(CF_TEXT)
		|| !OpenClipboard(hWnd))
	{
		return;
	}

	HANDLE hClipMemory = GetClipboardData(CF_TEXT);
	if (hClipMemory == nullptr)
	{
		CloseClipboard();
		return;
	}

	lpClipToKeyboard = static_cast<char*>(malloc(GlobalSize(hClipMemory)));
	if (lpClipToKeyboard != nullptr)
	{
		char *lpClipMemory = static_cast<char*>(GlobalLock(hClipMemory));
		strcpy(lpClipToKeyboard, lpClipMemory);
		GlobalUnlock(hClipMemory);
		lpClipToKeyboardPtr = lpClipToKeyboard;
	}

	CloseClipboard();
}
#if MaxMainFrames == 2
static void windowClipboard1(HWND hWnd)
{
	if (!IsClipboardFormatAvailable(CF_TEXT)
		|| !OpenClipboard(hWnd))
	{
		return;
	}

	HANDLE hClipMemory = GetClipboardData(CF_TEXT);
	if (hClipMemory == nullptr)
	{
		CloseClipboard();
		return;
	}

	lpClipToKeyboard1 = static_cast<char*>(malloc(GlobalSize(hClipMemory)));
	if (lpClipToKeyboard1 != nullptr)
	{
		char *lpClipMemory = static_cast<char*>(GlobalLock(hClipMemory));
		strcpy(lpClipToKeyboard1, lpClipMemory);
		GlobalUnlock(hClipMemory);
		lpClipToKeyboardPtr = lpClipToKeyboard1;
	}

	CloseClipboard();
}
#endif
/*--------------------------------------------------------------------------
**  Purpose:        Process messages for the main window.
**
**  Parameters:     Name        Description.
**
**  Returns:        LRESULT
**
**------------------------------------------------------------------------*/
static LRESULT CALLBACK windowProcedure(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	MMainFrame *mfr = BigIron->chasis[0];

	// ReSharper disable once CppJoinDeclarationAndAssignment
	int wmId;;
	LOGFONT lfTmp;
	RECT rt;

	switch (message)
	{
		/*
		**  Process the application menu.
		*/
	case WM_COMMAND:
		// ReSharper disable once CppJoinDeclarationAndAssignment
		wmId = LOWORD(wParam);

		switch (wmId)
		{
		case IDM_EXIT:
			DestroyWindow(hWnd);
			break;

		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
		break;

	case WM_ERASEBKGND:
		return(1);

	case WM_CREATE:
		hPen = CreatePen(PS_SOLID, 1, RGB(0, 255, 0));
		if (!hPen)
		{
			MessageBox(GetFocus(),
				"Unable to get green pen",
				"CreatePen Error",
				MB_OK);
		}

		memset(&lfTmp, 0, sizeof(lfTmp));
		lfTmp.lfPitchAndFamily = FIXED_PITCH;
		strcpy(lfTmp.lfFaceName, FontName);
		lfTmp.lfWeight = FW_THIN;
		lfTmp.lfOutPrecision = OUT_TT_PRECIS;
		lfTmp.lfHeight = FontSmallHeight;
		hSmallFont = CreateFontIndirect(&lfTmp);
		if (!hSmallFont)
		{
			MessageBox(GetFocus(),
				"Unable to get font in 15 point",
				"CreateFont Error",
				MB_OK);
		}

		memset(&lfTmp, 0, sizeof(lfTmp));
		lfTmp.lfPitchAndFamily = FIXED_PITCH;
		strcpy(lfTmp.lfFaceName, FontName);
		lfTmp.lfWeight = FW_THIN;
		lfTmp.lfOutPrecision = OUT_TT_PRECIS;
		lfTmp.lfHeight = FontMediumHeight;
		hMediumFont = CreateFontIndirect(&lfTmp);
		if (!hMediumFont)
		{
			MessageBox(GetFocus(),
				"Unable to get font in 20 point",
				"CreateFont Error",
				MB_OK);
		}

		memset(&lfTmp, 0, sizeof(lfTmp));
		lfTmp.lfPitchAndFamily = FIXED_PITCH;
		strcpy(lfTmp.lfFaceName, FontName);
		lfTmp.lfWeight = FW_THIN;
		lfTmp.lfOutPrecision = OUT_TT_PRECIS;
		lfTmp.lfHeight = FontLargeHeight;
		hLargeFont = CreateFontIndirect(&lfTmp);
		if (!hLargeFont)
		{
			MessageBox(GetFocus(),
				"Unable to get font in 30 point",
				"CreateFont Error",
				MB_OK);
		}

		return DefWindowProc(hWnd, message, wParam, lParam);

	case WM_DESTROY:
		if (hSmallFont)
		{
			DeleteObject(hSmallFont);
		}
		if (hMediumFont)
		{
			DeleteObject(hMediumFont);
		}
		if (hLargeFont)
		{
			DeleteObject(hLargeFont);
		}
		if (hPen)
		{
			DeleteObject(hPen);
		}
		PostQuitMessage(0);
		break;

	case WM_TIMER:
		if (lpClipToKeyboard != nullptr)
		{
			if (clipToKeyboardDelay == 0)
			{
				BigIron->chasis[0]->ppKeyIn = *lpClipToKeyboardPtr++;
				if (BigIron->chasis[0]->ppKeyIn == 0)
				{
					free(lpClipToKeyboard);
					lpClipToKeyboard = nullptr;
					lpClipToKeyboardPtr = nullptr;
				}
				else if (BigIron->chasis[0]->ppKeyIn == '\r')
				{
					clipToKeyboardDelay = 10;
				}
				else if (BigIron->chasis[0]->ppKeyIn == '\n')
				{
					BigIron->chasis[0]->ppKeyIn = 0;
				}
			}
			else
			{
				clipToKeyboardDelay -= 1;
			}
		}

		GetClientRect(hWnd, &rt);
		InvalidateRect(hWnd, &rt, TRUE);
		break;

		/*
		**  Paint the main window.
		*/
	case WM_PAINT:
		windowDisplay(hWnd);
		break;

		/*
		**  Handle input characters.
		*/
#if CcDebug == 1
	case WM_KEYDOWN:
		if (GetKeyState(VK_CONTROL) & 0x8000)
		{
			switch (wParam)
			{
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				dumpRunningPpu((u8)(wParam - '0'));
				break;

			case 'C':
			case 'c':
				dumpRunningCpu(0);
				break;
			}
		}

		break;
#endif

	case WM_SYSCHAR:
		// ReSharper disable once CppDefaultCaseNotHandledInSwitchStatement
		switch (wParam)
		{
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			mfr->traceMask ^= (1 << (static_cast<u32>(wParam) - '0' + (shifted ? 10 : 0)));
			break;

		case 'C':
			mfr->traceMask ^= TraceCpu1;
			//traceMask ^= TraceExchange;
			break;

		case 'c':
			mfr->traceMask ^= TraceCpu;
			//traceMask ^= TraceExchange;
			break;

		case 'E':
		case 'e':
			mfr->traceMask ^= TraceExchange;
			break;

		case 'X':
		case 'x':
			if (mfr->traceMask == 0)
			{
				mfr->traceMask = static_cast<u32>(~0L);
			}
			else
			{
				mfr->traceMask = 0;
			}
			break;

		case 'D':
		case 'd':
			mfr->traceMask ^= TraceCpu | TraceCpu1 | TraceExchange | 2;
			break;

		case 'L':
		case 'l':
		case '[':
			displayMode = ModeLeft;
			displayModeNeedsErase = true;
			break;

		case 'R':
		case 'r':
		case ']':
			displayMode = ModeRight;
			displayModeNeedsErase = true;
			break;

		case 'M':
		case 'm':
		case '\\':
			displayMode = ModeCenter;
			break;

		case 'P':
		case 'p':
			windowClipboard(hWnd);
			break;

		case 's':
		case 'S':
			shifted = !shifted;
		}
		break;

	case WM_CHAR:
		BigIron->chasis[0]->ppKeyIn = static_cast<char>(wParam);
		break;


	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}

	return 0;
}
#if MaxMainFrames == 2
static LRESULT CALLBACK windowProcedure1(HWND hWnd1, UINT message, WPARAM wParam, LPARAM lParam)
{
	// ReSharper disable once CppJoinDeclarationAndAssignment
	int wmId;
	LOGFONT lfTmp;
	RECT rt;
	MMainFrame *mfr = BigIron->chasis[1];
	// ReSharper disable once CppEntityNeverUsed
	int wmEvent = HIWORD(wParam);

	switch (message)
	{
		/*
		**  Process the application menu.
		*/
	case WM_COMMAND:
		// ReSharper disable once CppJoinDeclarationAndAssignment
		wmId = LOWORD(wParam);
		// ReSharper disable once CppEntityNeverUsed

		switch (wmId)
		{
		case IDM_EXIT:
			DestroyWindow(hWnd1);
			break;

		default:
			return DefWindowProc(hWnd1, message, wParam, lParam);
		}
		break;

	case WM_ERASEBKGND:
		return(1);

	case WM_CREATE:
		hPen1 = CreatePen(PS_SOLID, 1, RGB(0, 255, 0));
		if (!hPen1)
		{
			MessageBox(GetFocus(),
				"Unable to get green pen",
				"CreatePen Error",
				MB_OK);
		}

		memset(&lfTmp, 0, sizeof(lfTmp));
		lfTmp.lfPitchAndFamily = FIXED_PITCH;
		strcpy(lfTmp.lfFaceName, FontName);
		lfTmp.lfWeight = FW_THIN;
		lfTmp.lfOutPrecision = OUT_TT_PRECIS;
		lfTmp.lfHeight = FontSmallHeight;
		hSmallFont1 = CreateFontIndirect(&lfTmp);
		if (!hSmallFont1)
		{
			MessageBox(GetFocus(),
				"Unable to get font in 15 point",
				"CreateFont Error",
				MB_OK);
		}

		memset(&lfTmp, 0, sizeof(lfTmp));
		lfTmp.lfPitchAndFamily = FIXED_PITCH;
		strcpy(lfTmp.lfFaceName, FontName);
		lfTmp.lfWeight = FW_THIN;
		lfTmp.lfOutPrecision = OUT_TT_PRECIS;
		lfTmp.lfHeight = FontMediumHeight;
		hMediumFont1 = CreateFontIndirect(&lfTmp);
		if (!hMediumFont1)
		{
			MessageBox(GetFocus(),
				"Unable to get font in 20 point",
				"CreateFont Error",
				MB_OK);
		}

		memset(&lfTmp, 0, sizeof(lfTmp));
		lfTmp.lfPitchAndFamily = FIXED_PITCH;
		strcpy(lfTmp.lfFaceName, FontName);
		lfTmp.lfWeight = FW_THIN;
		lfTmp.lfOutPrecision = OUT_TT_PRECIS;
		lfTmp.lfHeight = FontLargeHeight;
		hLargeFont1 = CreateFontIndirect(&lfTmp);
		if (!hLargeFont1)
		{
			MessageBox(GetFocus(),
				"Unable to get font in 30 point",
				"CreateFont Error",
				MB_OK);
		}

		return DefWindowProc(hWnd1, message, wParam, lParam);

	case WM_DESTROY:
		if (hSmallFont1)
		{
			DeleteObject(hSmallFont1);
		}
		if (hMediumFont1)
		{
			DeleteObject(hMediumFont1);
		}
		if (hLargeFont1)
		{
			DeleteObject(hLargeFont1);
		}
		if (hPen1)
		{
			DeleteObject(hPen1);
		}
		PostQuitMessage(0);
		break;

	case WM_TIMER:
		if (lpClipToKeyboard != nullptr)
		{
			if (clipToKeyboardDelay1 == 0)
			{
				BigIron->chasis[1]->ppKeyIn = *lpClipToKeyboardPtr++;
				if (BigIron->chasis[1]->ppKeyIn == 0)
				{
					free(lpClipToKeyboard);
					lpClipToKeyboard = nullptr;
					lpClipToKeyboardPtr = nullptr;
				}
				else if (BigIron->chasis[1]->ppKeyIn == '\r')
				{
					clipToKeyboardDelay = 10;
				}
				else if (BigIron->chasis[1]->ppKeyIn == '\n')
				{
					BigIron->chasis[1]->ppKeyIn = 0;
				}
			}
			else
			{
				clipToKeyboardDelay1 -= 1;
			}
		}

		GetClientRect(hWnd1, &rt);
		InvalidateRect(hWnd1, &rt, TRUE);
		break;

		/*
		**  Paint the main window.
		*/
	case WM_PAINT:
		windowDisplay1(hWnd1);
		break;

		/*
		**  Handle input characters.
		*/
#if CcDebug == 1
	case WM_KEYDOWN:
		if (GetKeyState(VK_CONTROL) & 0x8000)
		{
			switch (wParam)
			{
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				dumpRunningPpu((u8)(wParam - '0'));
				break;

			case 'C':
			case 'c':
				dumpRunningCpu(1);
				break;
			}
		}

		break;
#endif

	case WM_SYSCHAR:
		// ReSharper disable once CppDefaultCaseNotHandledInSwitchStatement
		switch (wParam)
		{
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			mfr->traceMask ^= (1 << (static_cast<u32>(wParam) - '0' + (shifted ? 10 : 0)));
			break;

		case 'C':
			mfr->traceMask ^= TraceCpu1;
			//traceMask ^= TraceExchange;
			break;

		case 'c':
			mfr->traceMask ^= TraceCpu;
			//traceMask ^= TraceExchange;
			break;

		case 'E':
		case 'e':
			mfr->traceMask ^= TraceExchange;
			break;

		case 'X':
		case 'x':
			if (mfr->traceMask == 0)
			{
				mfr->traceMask = static_cast<u32>(~0L);
			}
			else
			{
				mfr->traceMask = 0;
			}
			break;

		case 'D':
		case 'd':
			mfr->traceMask ^= TraceCpu | TraceCpu1 | TraceExchange | 2;
			break;

		case 'L':
		case 'l':
		case '[':
			displayMode = ModeLeft;
			displayModeNeedsErase = true;
			break;

		case 'R':
		case 'r':
		case ']':
			displayMode = ModeRight;
			displayModeNeedsErase = true;
			break;

		case 'M':
		case 'm':
		case '\\':
			displayMode = ModeCenter;
			break;

		case 'P':
		case 'p':
			windowClipboard(hWnd1);
			break;

		case 's':
		case 'S':
			shifted = !shifted;
		}
		break;

	case WM_CHAR:
		BigIron->chasis[1]->ppKeyIn = static_cast<char>(wParam);
		break;


	default:
		return DefWindowProc(hWnd1, message, wParam, lParam);
	}

	return 0;
}
#endif
/*--------------------------------------------------------------------------
**  Purpose:        Display current list.
**
**  Parameters:     Name        Description.
**                  hWnd        window handle.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void windowDisplay(HWND hWnd)
{
	// ReSharper disable once CppEntityNeverUsed
	static int refreshCount = 0;
	char str[2] = " ";
	// ReSharper disable once CppInitializedValueIsAlwaysRewritten
	u8 oldFont = 0;

	RECT rect;
	PAINTSTRUCT ps;

	// ReSharper disable once CppEntityNeverUsed
	HDC hdc = BeginPaint(hWnd, &ps);

	GetClientRect(hWnd, &rect);

	/*
	**  Create a compatible DC.
	*/

	HDC hdcMem = CreateCompatibleDC(ps.hdc);

	/*
	**  Create a bitmap big enough for our client rect.
	*/
	HGDIOBJ hbmMem = CreateCompatibleBitmap(ps.hdc,
	                                        rect.right - rect.left,
	                                        rect.bottom - rect.top);

	/*
	**  Select the bitmap into the off-screen dc.
	*/
	HGDIOBJ hbmOld = SelectObject(hdcMem, hbmMem);

	HBRUSH hBrush = CreateSolidBrush(RGB(0, 0, 0));
	FillRect(hdcMem, &rect, hBrush);
	if (displayModeNeedsErase)
	{
		displayModeNeedsErase = false;
		FillRect(ps.hdc, &rect, hBrush);
	}
	DeleteObject(hBrush);

	SetBkMode(hdcMem, TRANSPARENT);
	SetBkColor(hdcMem, RGB(0, 0, 0));
	SetTextColor(hdcMem, RGB(0, 255, 0));

	HGDIOBJ hfntOld = SelectObject(hdcMem, hSmallFont);
	oldFont = FontSmall;

#if CcCycleTime
	{
		extern double cycleTime;
		char buf[80];

		//    sprintf(buf, "Cycle time: %.3f", cycleTime);
		sprintf(buf, "Cycle time: %10.3f    NPU Buffers: %5d", cycleTime, npuBipBufCount());
		TextOut(hdcMem, 0, 0, buf, strlen(buf));
	}
#endif

#if CcDebug == 1
	{
		char buf[160];

		MMainFrame *mfr = BigIron->chasis[0];

		/*
		**  Display P registers of PPUs and CPU and current trace mask.
		*/
		sprintf(buf, "Refresh: %-10d  PP P-reg: %04o %04o %04o %04o %04o %04o %04o %04o %04o %04o   CPU0 P-reg: %06o",
			refreshCount++,
			mfr->ppBarrel[0]->ppu.regP,
			mfr->ppBarrel[1]->ppu.regP,
			mfr->ppBarrel[2]->ppu.regP,
			mfr->ppBarrel[3]->ppu.regP,
			mfr->ppBarrel[4]->ppu.regP,
			mfr->ppBarrel[5]->ppu.regP,
			mfr->ppBarrel[6]->ppu.regP,
			mfr->ppBarrel[7]->ppu.regP,
			mfr->ppBarrel[8]->ppu.regP,
			mfr->ppBarrel[9]->ppu.regP,
			mfr->Acpu[0]->cpu.regP);

		sprintf(buf + strlen(buf), "   Trace0x: %c%c%c%c%c%c%c%c%c%c%c%c %c",
			(mfr->traceMask >> 0) & 1 ? '0' : '_',
			(mfr->traceMask >> 1) & 1 ? '1' : '_',
			(mfr->traceMask >> 2) & 1 ? '2' : '_',
			(mfr->traceMask >> 3) & 1 ? '3' : '_',
			(mfr->traceMask >> 4) & 1 ? '4' : '_',
			(mfr->traceMask >> 5) & 1 ? '5' : '_',
			(mfr->traceMask >> 6) & 1 ? '6' : '_',
			(mfr->traceMask >> 7) & 1 ? '7' : '_',
			(mfr->traceMask >> 8) & 1 ? '8' : '_',
			(mfr->traceMask >> 9) & 1 ? '9' : '_',
			mfr->traceMask & TraceCpu ? 'C' : '_',
			mfr->traceMask & TraceExchange ? 'E' : '_',
			shifted ? ' ' : '<');

		TextOut(hdcMem, 0, 0, buf, (int)strlen(buf));

		if (BigIron->pps == 20)
		{
			/*
			**  Display P registers of second barrel of PPUs.
			*/
			sprintf(buf, "                     PP P-reg: %04o %04o %04o %04o %04o %04o %04o %04o %04o %04o   CPU1 P-reg: %06o",
				mfr->ppBarrel[10]->ppu.regP,
				mfr->ppBarrel[11]->ppu.regP,
				mfr->ppBarrel[12]->ppu.regP,
				mfr->ppBarrel[13]->ppu.regP,
				mfr->ppBarrel[14]->ppu.regP,
				mfr->ppBarrel[15]->ppu.regP,
				mfr->ppBarrel[16]->ppu.regP,
				mfr->ppBarrel[17]->ppu.regP,
				mfr->ppBarrel[18]->ppu.regP,
				mfr->ppBarrel[19]->ppu.regP,
				BigIron->initCpus > 1 ? mfr->Acpu[1]->cpu.regP : 0);

			sprintf(buf + strlen(buf), "   Trace1x: %c%c%c%c%c%c%c%c%c%c%c%c %c",
				(mfr->traceMask >> 10) & 1 ? '0' : '_',
				(mfr->traceMask >> 11) & 1 ? '1' : '_',
				(mfr->traceMask >> 12) & 1 ? '2' : '_',
				(mfr->traceMask >> 13) & 1 ? '3' : '_',
				(mfr->traceMask >> 14) & 1 ? '4' : '_',
				(mfr->traceMask >> 15) & 1 ? '5' : '_',
				(mfr->traceMask >> 16) & 1 ? '6' : '_',
				(mfr->traceMask >> 17) & 1 ? '7' : '_',
				(mfr->traceMask >> 18) & 1 ? '8' : '_',
				(mfr->traceMask >> 19) & 1 ? '9' : '_',
				mfr->traceMask & TraceCpu1 ? 'C' : '_',
				' ',
				shifted ? '<' : ' ');

			TextOut(hdcMem, 0, 12, buf, (int)strlen(buf));
		}
	}
#endif

	if (opActive)
	{
		static char opMessage[] = "Emulation paused";
		hfntOld = SelectObject(hdcMem, hLargeFont);
		oldFont = FontLarge;
		TextOut(hdcMem, (0 * ScaleX) / 10, (256 * ScaleY) / 10, opMessage, static_cast<int>(strlen(opMessage)));
	}

	SelectObject(hdcMem, hPen);

	// ReSharper disable once CppInitializedValueIsAlwaysRewritten
	DispList *curr = display;
	DispList *end = display + listEnd;
	for (curr = display; curr < end; curr++)
	{
		if (oldFont != curr->fontSize)
		{
			oldFont = curr->fontSize;

			// ReSharper disable once CppDefaultCaseNotHandledInSwitchStatement
			switch (oldFont)
			{
			case FontSmall:
				SelectObject(hdcMem, hSmallFont);
				break;

			case FontMedium:
				SelectObject(hdcMem, hMediumFont);
				break;

			case FontLarge:
				SelectObject(hdcMem, hLargeFont);
				break;
			}
		}

		if (curr->fontSize == FontDot)
		{
			SetPixel(hdcMem, (curr->xPos * ScaleX) / 10, (curr->yPos * ScaleY) / 10 + 30, RGB(0, 255, 0));
		}
		else
		{
			str[0] = curr->ch;
			TextOut(hdcMem, (curr->xPos * ScaleX) / 10, (curr->yPos * ScaleY) / 10 + 20, str, 1);
		}
	}

	listEnd = 0;
	currentX = -1;
	currentY = -1;

	if (hfntOld)
	{
		SelectObject(hdcMem, hfntOld);
	}

	/*
	**  Blit the changes to the screen dc.
	*/
	switch (displayMode)
	{
	default:
	case ModeCenter:
		BitBlt(ps.hdc,
			rect.left, rect.top,
			rect.right - rect.left, rect.bottom - rect.top,
			hdcMem,
			0, 0,
			SRCCOPY);
		break;

	case ModeLeft:
		StretchBlt(ps.hdc,
			rect.left + (rect.right - rect.left) / 2 - 512 * ScaleY / 10 / 2, rect.top,
			512 * ScaleY / 10, rect.bottom - rect.top,
			hdcMem,
			OffLeftScreen, 0,
			512 * ScaleX / 10 + FontLarge, rect.bottom - rect.top,
			SRCCOPY);
		break;

	case ModeRight:
		StretchBlt(ps.hdc,
			rect.left + (rect.right - rect.left) / 2 - 512 * ScaleY / 10 / 2, rect.top,
			512 * ScaleY / 10, rect.bottom - rect.top,
			hdcMem,
			OffRightScreen, 0,
			512 * ScaleX / 10 + FontLarge, rect.bottom - rect.top,
			SRCCOPY);
		break;
	}

	/*
	**  Done with off screen bitmap and dc.
	*/
	SelectObject(hdcMem, hbmOld);
	DeleteObject(hbmMem);
	DeleteDC(hdcMem);

	EndPaint(hWnd, &ps);
}
#if MaxMainFrames == 2
void windowDisplay1(HWND hWnd)
{
	// ReSharper disable once CppEntityNeverUsed
	static int refreshCount = 0;
	char str[2] = " ";
	// ReSharper disable once CppJoinDeclarationAndAssignment
	DispList *end;
	// ReSharper disable once CppJoinDeclarationAndAssignment
	u8 oldFont;

	RECT rect;
	PAINTSTRUCT ps;
	// ReSharper disable once CppJoinDeclarationAndAssignment
	HBRUSH hBrush;

	// ReSharper disable once CppJoinDeclarationAndAssignment
	HDC hdcMem;
	// ReSharper disable once CppJoinDeclarationAndAssignment
	HGDIOBJ hbmMem;
	// ReSharper disable once CppJoinDeclarationAndAssignment
	HGDIOBJ hbmOld;
	// ReSharper disable once CppJoinDeclarationAndAssignment
	HGDIOBJ hfntOld;

	// ReSharper disable once CppEntityNeverUsed
	HDC hdc = BeginPaint(hWnd, &ps);

	GetClientRect(hWnd, &rect);

	/*
	**  Create a compatible DC.
	*/

	// ReSharper disable once CppJoinDeclarationAndAssignment
	hdcMem = CreateCompatibleDC(ps.hdc);

	/*
	**  Create a bitmap big enough for our client rect.
	*/
	// ReSharper disable once CppJoinDeclarationAndAssignment
	hbmMem = CreateCompatibleBitmap(ps.hdc,
		rect.right - rect.left,
		rect.bottom - rect.top);

	/*
	**  Select the bitmap into the off-screen dc.
	*/
	// ReSharper disable once CppJoinDeclarationAndAssignment
	hbmOld = SelectObject(hdcMem, hbmMem);

	// ReSharper disable once CppJoinDeclarationAndAssignment
	hBrush = CreateSolidBrush(RGB(0, 0, 0));
	FillRect(hdcMem, &rect, hBrush);
	if (displayModeNeedsErase1)
	{
		displayModeNeedsErase1 = false;
		FillRect(ps.hdc, &rect, hBrush);
	}
	DeleteObject(hBrush);

	SetBkMode(hdcMem, TRANSPARENT);
	SetBkColor(hdcMem, RGB(0, 0, 0));
	SetTextColor(hdcMem, RGB(0, 255, 0));

	// ReSharper disable once CppJoinDeclarationAndAssignment
	hfntOld = SelectObject(hdcMem, hSmallFont1);
	// ReSharper disable once CppJoinDeclarationAndAssignment
	oldFont = FontSmall;

#if CcCycleTime
	{
		extern double cycleTime;
		char buf[80];

		//    sprintf(buf, "Cycle time: %.3f", cycleTime);
		sprintf(buf, "Cycle time: %10.3f    NPU Buffers: %5d", cycleTime, npuBipBufCount());
		TextOut(hdcMem, 0, 0, buf, strlen(buf));
	}
#endif

#if CcDebug == 1
	{
		MMainFrame *mfr = BigIron->chasis[1];

		char buf[160];

		/*
		**  Display P registers of PPUs and CPU and current trace mask.
		*/
		sprintf(buf, "Refresh: %-10d  PP P-reg: %04o %04o %04o %04o %04o %04o %04o %04o %04o %04o   CPU0 P-reg: %06o",
			refreshCount++,
			mfr->ppBarrel[0]->ppu.regP,
			mfr->ppBarrel[1]->ppu.regP,
			mfr->ppBarrel[2]->ppu.regP,
			mfr->ppBarrel[3]->ppu.regP,
			mfr->ppBarrel[4]->ppu.regP,
			mfr->ppBarrel[5]->ppu.regP,
			mfr->ppBarrel[6]->ppu.regP,
			mfr->ppBarrel[7]->ppu.regP,
			mfr->ppBarrel[8]->ppu.regP,
			mfr->ppBarrel[9]->ppu.regP,
			mfr->Acpu[0]->cpu.regP);

		sprintf(buf + strlen(buf), "   Trace0x: %c%c%c%c%c%c%c%c%c%c%c%c %c",
			(mfr->traceMask >> 0) & 1 ? '0' : '_',
			(mfr->traceMask >> 1) & 1 ? '1' : '_',
			(mfr->traceMask >> 2) & 1 ? '2' : '_',
			(mfr->traceMask >> 3) & 1 ? '3' : '_',
			(mfr->traceMask >> 4) & 1 ? '4' : '_',
			(mfr->traceMask >> 5) & 1 ? '5' : '_',
			(mfr->traceMask >> 6) & 1 ? '6' : '_',
			(mfr->traceMask >> 7) & 1 ? '7' : '_',
			(mfr->traceMask >> 8) & 1 ? '8' : '_',
			(mfr->traceMask >> 9) & 1 ? '9' : '_',
			mfr->traceMask & TraceCpu ? 'C' : '_',
			mfr->traceMask & TraceExchange ? 'E' : '_',
			shifted ? ' ' : '<');

		TextOut(hdcMem, 0, 0, buf, (int)strlen(buf));

		if (BigIron->pps == 20)
		{
			/*
			**  Display P registers of second barrel of PPUs.
			*/
			sprintf(buf, "                     PP P-reg: %04o %04o %04o %04o %04o %04o %04o %04o %04o %04o   CPU1 P-reg: %06o",
				mfr->ppBarrel[10]->ppu.regP,
				mfr->ppBarrel[11]->ppu.regP,
				mfr->ppBarrel[12]->ppu.regP,
				mfr->ppBarrel[13]->ppu.regP,
				mfr->ppBarrel[14]->ppu.regP,
				mfr->ppBarrel[15]->ppu.regP,
				mfr->ppBarrel[16]->ppu.regP,
				mfr->ppBarrel[17]->ppu.regP,
				mfr->ppBarrel[18]->ppu.regP,
				mfr->ppBarrel[19]->ppu.regP,
				BigIron->initCpus > 1 ? mfr->Acpu[1]->cpu.regP : 0);

			sprintf(buf + strlen(buf), "   Trace1x: %c%c%c%c%c%c%c%c%c%c%c%c %c",
				(mfr->traceMask >> 10) & 1 ? '0' : '_',
				(mfr->traceMask >> 11) & 1 ? '1' : '_',
				(mfr->traceMask >> 12) & 1 ? '2' : '_',
				(mfr->traceMask >> 13) & 1 ? '3' : '_',
				(mfr->traceMask >> 14) & 1 ? '4' : '_',
				(mfr->traceMask >> 15) & 1 ? '5' : '_',
				(mfr->traceMask >> 16) & 1 ? '6' : '_',
				(mfr->traceMask >> 17) & 1 ? '7' : '_',
				(mfr->traceMask >> 18) & 1 ? '8' : '_',
				(mfr->traceMask >> 19) & 1 ? '9' : '_',
				mfr->traceMask & TraceCpu1 ? 'C' : '_',
				' ',
				shifted ? '<' : ' ');

			TextOut(hdcMem, 0, 12, buf, (int)strlen(buf));
		}
	}
#endif

	if (opActive)
	{
		static char opMessage[] = "Emulation paused";
		hfntOld = SelectObject(hdcMem, hLargeFont1);
		oldFont = FontLarge;
		TextOut(hdcMem, (0 * ScaleX) / 10, (256 * ScaleY) / 10, opMessage, static_cast<int>(strlen(opMessage)));
	}

	SelectObject(hdcMem, hPen1);

	// ReSharper disable once CppInitializedValueIsAlwaysRewritten
	DispList *curr = display1;
	// ReSharper disable once CppJoinDeclarationAndAssignment
	end = display1 + listEnd1;
	for (curr = display1; curr < end; curr++)
	{
		if (oldFont != curr->fontSize)
		{
			oldFont = curr->fontSize;

			// ReSharper disable once CppDefaultCaseNotHandledInSwitchStatement
			switch (oldFont)
			{
			case FontSmall:
				SelectObject(hdcMem, hSmallFont);
				break;

			case FontMedium:
				SelectObject(hdcMem, hMediumFont);
				break;

			case FontLarge:
				SelectObject(hdcMem, hLargeFont);
				break;
			}
		}

		if (curr->fontSize == FontDot)
		{
			SetPixel(hdcMem, (curr->xPos * ScaleX) / 10, (curr->yPos * ScaleY) / 10 + 30, RGB(0, 255, 0));
		}
		else
		{
			str[0] = curr->ch;
			TextOut(hdcMem, (curr->xPos * ScaleX) / 10, (curr->yPos * ScaleY) / 10 + 20, str, 1);
		}
	}

	listEnd1 = 0;
	currentX1 = -1;
	currentY1 = -1;

	if (hfntOld)
	{
		SelectObject(hdcMem, hfntOld);
	}

	/*
	**  Blit the changes to the screen dc.
	*/
	switch (displayMode1)
	{
	default:
	case ModeCenter:
		BitBlt(ps.hdc,
			rect.left, rect.top,
			rect.right - rect.left, rect.bottom - rect.top,
			hdcMem,
			0, 0,
			SRCCOPY);
		break;

	case ModeLeft:
		StretchBlt(ps.hdc,
			rect.left + (rect.right - rect.left) / 2 - 512 * ScaleY / 10 / 2, rect.top,
			512 * ScaleY / 10, rect.bottom - rect.top,
			hdcMem,
			OffLeftScreen, 0,
			512 * ScaleX / 10 + FontLarge, rect.bottom - rect.top,
			SRCCOPY);
		break;

	case ModeRight:
		StretchBlt(ps.hdc,
			rect.left + (rect.right - rect.left) / 2 - 512 * ScaleY / 10 / 2, rect.top,
			512 * ScaleY / 10, rect.bottom - rect.top,
			hdcMem,
			OffRightScreen, 0,
			512 * ScaleX / 10 + FontLarge, rect.bottom - rect.top,
			SRCCOPY);
		break;
	}

	/*
	**  Done with off screen bitmap and dc.
	*/
	SelectObject(hdcMem, hbmOld);
	DeleteObject(hbmMem);
	DeleteDC(hdcMem);

	EndPaint(hWnd, &ps);
}
#endif
/*---------------------------  End Of File  ------------------------------*/
