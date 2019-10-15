/*
  $File: $
  $Date: $
  $Revision: $
  $Creator: Roger Nogue $
 */

#include <Windows.h>

#define internal_function static
#define local_persistent static
#define global_variable static

//static auto declares to 0
global_variable bool GameRunning;
global_variable BITMAPINFO BitMapInfo;
global_variable void* BitMapMemory;
global_variable HBITMAP BitmapHandle;
global_variable HDC BitmapDeviceContext;
//extra info of windows:
//CALLBACK means that it calls US
//WINAPI means that we call windows

/*
function that initializes / resizes the window.
internal typename adapts strings to the needed width.
*/
void HandmadeResizeDIBSection(int Width, int Height)
{
	/*
	TODO: if creation failed maybe we should not have freed.
	But then not having enough memory for both new and old at the same time
	can be a big problem too.
	*/

	//TODO free DIBSection
	if (BitmapHandle)
	{
		DeleteObject(BitmapHandle);
	}
	if(!BitmapDeviceContext)
	{
		BitmapDeviceContext = CreateCompatibleDC(0);
	}

	BitMapInfo.bmiHeader.biSize = sizeof(BitMapInfo.bmiHeader);
	BitMapInfo.bmiHeader.biWidth = Width;
	BitMapInfo.bmiHeader.biHeight = Height;
	BitMapInfo.bmiHeader.biPlanes = 1;
	BitMapInfo.bmiHeader.biBitCount = 32;
	BitMapInfo.bmiHeader.biCompression = BI_RGB;
	/*
	Since defines set everything to 0, these initializations are not needed:
	BitMapInfo.bmiHeader.biSize = 0;
	BitMapInfo.bmiHeader.biXPelsPerMeter = 0;
	BitMapInfo.bmiHeader.biYPelsPerMeter = 0;
	BitMapInfo.bmiHeader.biClrUsed = 0;
	BitMapInfo.bmiHeader.biClrImportant = 0;
	*/

	BitmapHandle = CreateDIBSection(BitmapDeviceContext, &BitMapInfo,
										DIB_RGB_COLORS, &BitMapMemory,0, 0);
}

void HandmadeUpdateWindow(HDC DeviceContext, int X, int Y, int Width, int Height)
{
	StretchDIBits(	DeviceContext,
					X, Y, Width, Height,
					X, Y, Width, Height,
					BitMapMemory,
					&BitMapInfo,
					DIB_RGB_COLORS, SRCCOPY);
}

//Since it is an "Application-defined function" we gotta define it
LRESULT CALLBACK HandmadeMainWindowCallback(	HWND   Window,
										UINT   Message,
										WPARAM Wparam,
										LPARAM Lparam)
{
	LRESULT Result = 0;

	//This function processes messages sent to a window. 
	//Since we are gonna do our stuff, 
	//Mostly of the messages we are gonna respond to are some of windows general messages
	switch (Message)
	{
		//making blocks of code protects variable names, which is cool
		case WM_SIZE:
		{
			tagRECT clientWindowRect;
			GetClientRect(Window, &clientWindowRect);

			int Width = clientWindowRect.right - clientWindowRect.left;
			int Height = clientWindowRect.bottom - clientWindowRect.top;
			HandmadeResizeDIBSection(Width, Height);
		}
		break;

		case WM_DESTROY:
		{
			//TODO: probably its an error what we want to display
			GameRunning = false;
		}
		break;

		case WM_CLOSE:
		{
			//TODO: maybe display message to de user
			GameRunning = false;
		}
		break;

		case WM_ACTIVATEAPP:
		{
			OutputDebugStringA("WM_ACTIVATEAPP\n");
		}
		break;

		case WM_PAINT:
		{
			//painting process, 
			//this function only gets called when resized or asked to repaint (for now just resized)
			//need this struct
			PAINTSTRUCT Paint;
			//gotta begin, windows fills Paint for us
			HDC DeviceContext = BeginPaint(Window, &Paint);

			//set stuff up for painting the whole window
			int X = Paint.rcPaint.left;
			int Y = Paint.rcPaint.top;
			int Width = Paint.rcPaint.right - Paint.rcPaint.left;
			int Height = Paint.rcPaint.bottom - Paint.rcPaint.top;

			HandmadeUpdateWindow(DeviceContext, X, Y, Width, Height);

			//gotta end
			EndPaint(Window, &Paint);
		}
		break;

		default:
		{
			//OutputDebugStringA("non treated message\n");
			//default handling function for messages that i do not care about
			Result = DefWindowProcA(Window, Message, Wparam, Lparam);
		}
		break;

	}
	return Result;

}

int CALLBACK WinMain(	HINSTANCE Instance,
						HINSTANCE PrevInstance,
						LPSTR     CommandLine,
						int       ShowCode)

{
	WNDCLASS WindowClass = {};//in C++ this initializes everything to 0
	/*Window Class structure data initialization*/
  
	//this flag(OWNDC) asks for our own context to draw so we dont have to share it and manage it a a resource
	//OWNDC asks for a Device Context for each window
	//another flag called CLASSDC asks for a context for the entire class
	//HREDRAW makes re draw the contents if window moved horizontally
	//VREDRAW same but vertically
	//TODO: these flags may not be necessary anymore (legacy stuff)
	//WindowClass.style = CS_OWNDC|CS_HREDRAW|CS_VREDRAW;//it is not.
	
	WindowClass.lpfnWndProc = HandmadeMainWindowCallback;
	
	//we got this from winMain, but could also use GetModuleHandleA() if we didnt have it.
	//GetModuleHandleA gives info about the current running code instance
	//it would be: WindowClass.hInstance = GetModuleHandleA(0);
	WindowClass.hInstance = Instance;
	
	//  HICON     hIcon;//for the future
	WindowClass.lpszClassName = "RogerDoingHandmadeStuff";//necessary name to create the window, the L is cuz long pointer
	
	/*Done initializing*/

	/*Time to Register the window class in order to open it*/
	//this call returns an ATOM that may be used for some stuff
	if (RegisterClassA(&WindowClass))
	{
		//create the window
		HWND WindowHandle = CreateWindowExA(
							0,
							WindowClass.lpszClassName,
							"Roger Handmade",
							WS_OVERLAPPEDWINDOW|WS_VISIBLE,
							CW_USEDEFAULT,
							CW_USEDEFAULT,
							CW_USEDEFAULT,
							CW_USEDEFAULT,
							0,
							0,
							Instance,
							0);
		//check for creation error
		if (WindowHandle)
		{
			/*Window created, now we have to start a message loop since windows
			does not do that by default. This loop treats all the messages that 
			windows sends to our window application*/
			MSG Message;
			GameRunning = true;
			//we dont pass the window handle since we want to treat ALL the messages sent to us, not just to the window
			while (GameRunning)
			{
				BOOL returnValue = GetMessageA(&Message, 0, 0, 0);
				GameRunning = (returnValue > 0);
				if (GameRunning)
				{
					//we actually treat the message
					TranslateMessage(&Message);//messages need a little bit of processing
					DispatchMessageA(&Message);//now dispatch it
				}
			}
		}
		else
		{
			//TODO LOG error
		}
	}
	else
	{
		//TODO LOG error
	}

	//No need to release resources of window and stuff since windows does it for us
	
	return(0);
}
