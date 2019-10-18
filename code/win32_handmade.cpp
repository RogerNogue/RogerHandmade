/*
  $File: $
  $Date: $
  $Revision: $
  $Creator: Roger Nogue $
 */

#include <Windows.h>

 //creates uint8_t adapted to what we want, since unsigned size may be different than what we want
#include <stdint.h>

#define internal_function static
#define local_persistent static
#define global_variable static

//adapting types to every platform
typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

//static auto declares to 0
global_variable bool GameRunning;
global_variable BITMAPINFO BitMapInfo;
global_variable void* BitMapMemory;
global_variable int BitmapWidth;
global_variable int BitmapHeight;
global_variable int BytesPerPixel;
global_variable int ClientWindowWidth;
global_variable int ClientWindowHeight;

//extra info of windows:
//CALLBACK means that it calls US
//WINAPI means that we call windows


//function that paints a gradient
internal_function void renderGradient(int gradXOffset, int gradYOffset)
{
	//lets paint pixels, first we call a small raw pointer to manage memory
	uint8* row = (uint8*)BitMapMemory;
	int pitch = ClientWindowWidth * BytesPerPixel;

	for (int i = 0; i < BitmapHeight; ++i)
	{
		uint32* pixel = (uint32*)row;
		for (int j = 0; j < BitmapWidth; ++j)
		{
			/*
			IMPORTANT! MEM explanation
			Little endian architecture: the first byte goes to the last part and so on.
			
			REASON: you have to store it BB GG RR XX because then when windows load the pixel,
			they want it to be seen as XX RR GG BB.

			and to have in mem BB GG RR XX, you need in the uint32: 0x XX RR GG BB.
			
			If we did it by uint8, we would have to first store BB at the first byte, then GG,
			then RR, and finally XX.
			*/

			//now we paint the pixel in a more direct manner
			uint8 R = (uint8)i + gradXOffset;
			uint8 G = (uint8)j + gradYOffset;
			uint8 B = (uint8)(gradXOffset + gradYOffset);

			//we store it as said above
			*pixel = (uint32)(B | (G << 8) | (R<<16));
			++pixel;
		}
		row += pitch;
	}
}

/*
function that initializes / resizes the window.
internal typename adapts strings to the needed width.
*/
internal_function void HandmadeResizeDIBSection()
{
	/*
	TODO: if creation failed maybe we should not have freed.
	But then not having enough memory for both new and old at the same time
	can be a big problem too.
	*/

	//we just re allocate if we need more mem
	if (BitMapMemory)
	{
		VirtualFree(BitMapMemory, 0, MEM_RELEASE);
	}
	BitmapWidth = ClientWindowWidth;
	BitmapHeight = ClientWindowHeight;

	BitMapInfo.bmiHeader.biSize = sizeof(BitMapInfo.bmiHeader);
	BitMapInfo.bmiHeader.biWidth = BitmapWidth;
	BitMapInfo.bmiHeader.biHeight = -BitmapHeight;//to make our DIB top-down
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
	/*
	we are gonna allocate memory for the bitMapInfo in the following way
	8 bits for red, 8 for green and 8 for blue. Since this is 3 bytes, we are gonna
	get 8 more bits just for alignment so that every pixel colo info can be aligned to 
	4 bytes, which is way easier to do that align it to 3
	*/
	BytesPerPixel = 4;
	int BitMapMemorySize = (BitmapWidth * BitmapHeight)*BytesPerPixel;
	
	//call alloc, VirtualFree deallocates
	BitMapMemory =  VirtualAlloc(0, BitMapMemorySize, MEM_COMMIT, PAGE_READWRITE);

	//note VirtualProtect() allows to trigger a breakpoint whenever the protected
	//memory is accessed in any way. This is good for bug detection
	

}

internal_function void HandmadeUpdateWindow(HDC DeviceContext, RECT* WindowRect, int X, int Y, int Width, int Height)
{
	/*StretchDIBits(	DeviceContext,
					X, Y, Width, Height,
					X, Y, Width, Height,
					BitMapMemory,
					&BitMapInfo,
					DIB_RGB_COLORS, SRCCOPY);*/

	/*
	We have a top-down DIB (cuz the way we declared our BitMapInfo)
	StretchDiBits starts at top left, 
	*/
	int WindowWidth = WindowRect->right - WindowRect->left;
	int WindowHeight = WindowRect->bottom - WindowRect->top;

	StretchDIBits(DeviceContext,
		0, 0, BitmapWidth, BitmapHeight,
		0, 0, WindowWidth, WindowHeight,
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

			ClientWindowWidth = clientWindowRect.right - clientWindowRect.left;
			ClientWindowHeight = clientWindowRect.bottom - clientWindowRect.top;
			HandmadeResizeDIBSection();
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

			HandmadeUpdateWindow(DeviceContext, &Paint.rcPaint, X, Y, Width, Height);

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

			int gradientXoffset = 0;
			int gradientYoffset = 0;
			//we dont pass the window handle since we want to treat ALL the messages sent to us, not just to the window
			while (GameRunning)
			{
				//BOOL returnValue = GetMessageA(&Message, 0, 0, 0);

				while (PeekMessageA(&Message, WindowHandle, 0, 0, PM_REMOVE))
				{
					if (Message.message == WM_CLOSE || Message.message == WM_QUIT )
					{
						GameRunning = false;
					}

					//we actually treat the message
					TranslateMessage(&Message);//messages need a little bit of processing
					DispatchMessageA(&Message);//now dispatch it
				}
				//our gameloop
				//update our bitmap
				renderGradient(gradientXoffset, gradientYoffset);

				//actually paint the bitmap
				//first we get device context
				HDC WindowContext = GetDC(WindowHandle);
				tagRECT clientWindowRect;
				GetClientRect(WindowHandle, &clientWindowRect);

				HandmadeUpdateWindow(WindowContext, &clientWindowRect, 0, 0, ClientWindowWidth, ClientWindowHeight);

				++gradientXoffset;
				++gradientYoffset;
				//release device context
				ReleaseDC(WindowHandle, WindowContext);
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
