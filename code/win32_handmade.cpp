/*
  $File: $
  $Date: $
  $Revision: $
  $Creator: Roger Nogue $
 */

#include <Windows.h>

//extra info of windows:
//CALLBACK means that it calls US
//WINAPI means that we call windows

//Since it is an "Application-defined function" we gotta define it
LRESULT CALLBACK MainWindowCallback(	HWND   Window,
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
			OutputDebugStringA("WM_SIZE\n");
		}
		break;

		case WM_DESTROY:
		{
			OutputDebugStringA("WM_DESTROY\n");
		}
		break;

		case WM_CLOSE:
		{
			OutputDebugStringA("WM_CLOSE\n");
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
			HDC Device	= BeginPaint(Window, &Paint);

			//set stuff up for painting the whole window
			int X = Paint.rcPaint.left;
			int Y = Paint.rcPaint.top;
			int Width = Paint.rcPaint.right - Paint.rcPaint.left;
			int Height = Paint.rcPaint.bottom - Paint.rcPaint.top;
			static DWORD Operation = WHITENESS;
			PatBlt(	Device, X, Y, Width, Height, Operation);
			Operation = BLACKNESS;
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
	WindowClass.style = CS_OWNDC|CS_HREDRAW|CS_VREDRAW;
	
	WindowClass.lpfnWndProc = MainWindowCallback;
	
	//we got this from winMain, but could also use GetModuleHandleA() if we didnt have it.
	//GetModuleHandleA gives info about the current running code instance
	//it would be: WindowClass.hInstance = GetModuleHandleA(0);
	WindowClass.hInstance = Instance;
	
	//  HICON     hIcon;//for the future
	WindowClass.lpszClassName = "RogerDoingHandmadeStuff";//necessary name to create the window, the L is cuz long pointer
	
	/*Done initializing*/

	/*Time to Register the window class in order to open it*/
	//this call returns an ATOM that may be used for some stuff
	if (RegisterClass(&WindowClass))
	{
		//create the window
		HWND WindowHandle = CreateWindowEx(
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
			bool correctMessage = true;
			//we dont pass the window handle since we want to treat ALL the messages sent to us, not just to the window
			while (correctMessage)
			{
				BOOL returnValue = GetMessage(&Message, 0, 0, 0);
				correctMessage = (returnValue > 0);
				if (correctMessage)
				{
					//we actually treat the message
					TranslateMessage(&Message);//messages need a little bit of processing
					DispatchMessage(&Message);//now dispatch it
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


	
	return(0);
}
