/*
  $File: $
  $Date: $
  $Revision: $
  $Creator: Roger Nogue $
 */

#include <Windows.h>

 //creates uint8_t adapted to what we want, since unsigned size may be different than what we want
#include <stdint.h>
#include <xinput.h>

#define internal_function static
#define local_persistent static
#define global_variable static

struct BufferData
{
	BITMAPINFO BufferInfo;
	void* BufferMemory;
	int BufferWidth;
	int BufferHeight;
	int Pitch;
	int BytesPerPixel;
};

struct RectDimensions
{
	int width;
	int height;
};

//static auto declares to 0
global_variable bool GLOBAL_GameRunning;

global_variable BufferData BackBuffer;
global_variable RectDimensions BufferDimensions{ 1280, 720 };

//extra info of windows:
//CALLBACK means that it calls US
//WINAPI means that we call windows

internal_function void InputTreating(int index, XINPUT_STATE* inputState)
{
	//TODO:
	//state has a packet number that indicates how many times the state of the controller changed
	//this may give us info about having to increase this function call frequency rate in the future.
	
	//for now we will not care about controller index
	bool up = inputState->Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP;
	bool down = inputState->Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN;
	bool left = inputState->Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT;
	bool right = inputState->Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT;
	bool start = inputState->Gamepad.wButtons & XINPUT_GAMEPAD_START;
	bool back = inputState->Gamepad.wButtons & XINPUT_GAMEPAD_BACK;
	bool leftShoulder = inputState->Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER;
	bool rightShoulder = inputState->Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER;

	int16_t leftThumbX = inputState->Gamepad.sThumbLX;
	int16_t leftThumbY = inputState->Gamepad.sThumbLY;

}

internal_function void ControllerInputTreating()
{
	//Dword = 32bit = int
	//Word: 16bit
	//byte : 8bit
	//short 16bit signed
	for (int controllerIndex; controllerIndex < XUSER_MAX_COUNT; ++controllerIndex)
	{
		XINPUT_STATE* controllerState;
		if (XInputGetState(controllerIndex, controllerState) == ERROR_SUCCESS)
		{
			//succeded
			InputTreating(controllerIndex, controllerState);
		}
		else
		{
			//error or controller not connected
		}
	}

}

//function that paints a gradient
internal_function void renderGradient(const BufferData& Buffer, int gradXOffset, int gradYOffset)
{
	//lets paint pixels, first we call a small raw pointer to manage memory
	//row has to be in bytes, if not when we do pointer arithmetic we would
	//increase the pointer way more space than we need and problems would occur.
	uint8_t* row = (uint8_t*)Buffer.BufferMemory;

	for (int i = 0; i < Buffer.BufferHeight; ++i)
	{
		uint32_t* pixel = (uint32_t*)row;
		for (int j = 0; j < Buffer.BufferWidth; ++j)
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
			uint8_t R = (uint8_t)i + gradXOffset;
			uint8_t G = (uint8_t)j + gradYOffset;
			uint8_t B = (uint8_t)(gradXOffset + gradYOffset);

			//we store it as said above
			*pixel = (uint32_t)(B | (G << 8) | (R<<16));
			++pixel;
		}
		row += Buffer.Pitch;
	}
}

internal_function RectDimensions GetContextDimensions(HWND Window)
{
	tagRECT clientWindowRect;
	GetClientRect(Window, &clientWindowRect);
	RectDimensions dim{ clientWindowRect.right - clientWindowRect.left, clientWindowRect.bottom - clientWindowRect.top };
	return dim;
}

/*
function that initializes / resizes the window.
internal typename adapts strings to the needed width.
*/
internal_function void HandmadeResizeDIBSection(BufferData* Buffer, int width, int height)
{
	/*
	TODO: if creation failed maybe we should not have freed.
	But then not having enough memory for both new and old at the same time
	can be a big problem too.
	*/
	//we just re allocate if we resized
	if (Buffer->BufferMemory)
	{
		VirtualFree(Buffer->BufferMemory, 0, MEM_RELEASE);
	}
	Buffer->BufferWidth = width;
	Buffer->BufferHeight = height;

	Buffer->BufferInfo.bmiHeader.biSize = sizeof(Buffer->BufferInfo.bmiHeader);
	Buffer->BufferInfo.bmiHeader.biWidth = Buffer->BufferWidth;
	Buffer->BufferInfo.bmiHeader.biHeight = -Buffer->BufferHeight;//to make our DIB top-down
	Buffer->BufferInfo.bmiHeader.biPlanes = 1;
	Buffer->BufferInfo.bmiHeader.biBitCount = 32;
	Buffer->BufferInfo.bmiHeader.biCompression = BI_RGB;
	/*
	we are gonna allocate memory for the bitMapInfo in the following way
	8 bits for red, 8 for green and 8 for blue. Since this is 3 bytes, we are gonna
	get 8 more bits just for alignment so that every pixel colo info can be aligned to 
	4 bytes, which is way easier to do that align it to 3
	*/
	Buffer->BytesPerPixel = 4;
	int BitMapMemorySize = (Buffer->BufferWidth * Buffer->BufferHeight)* Buffer->BytesPerPixel;
	
	//call alloc, VirtualFree deallocates
	Buffer->BufferMemory =  VirtualAlloc(0, BitMapMemorySize, MEM_COMMIT, PAGE_READWRITE);

	//note VirtualProtect() allows to trigger a breakpoint whenever the protected
	//memory is accessed in any way. This is good for bug detection
	Buffer->Pitch = Buffer->BufferWidth * Buffer->BytesPerPixel;

}
//for now we are not gonna make int inline since StretchDIBits() may do more than we expect
internal_function void HandmadeUpdateWindow(const BufferData& Buffer, HDC DeviceContext, int X, int Y, int Width, int Height)
{
	/*
	We have a top-down DIB (cuz the way we declared our BitMapInfo)
	StretchDiBits starts at top left.
	*/
	/*Aspect ratio setting*/

	StretchDIBits(DeviceContext,
		0, 0, Width, Height,
		0, 0, Buffer.BufferWidth, Buffer.BufferHeight,
		Buffer.BufferMemory,
		&Buffer.BufferInfo,
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
		}
		break;

		case WM_DESTROY:
		{
			//TODO: probably its an error what we want to display
			GLOBAL_GameRunning = false;
		}
		break;

		case WM_CLOSE:
		{
			//TODO: maybe display message to de user
			GLOBAL_GameRunning = false;
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

			HandmadeUpdateWindow(BackBuffer, DeviceContext, X, Y, Width, Height);

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

	HandmadeResizeDIBSection(&BackBuffer, BufferDimensions.width, BufferDimensions.height);
  
	//this flag(OWNDC) asks for our own context to draw so we dont have to share it and manage it a a resource
	//OWNDC asks for a Device Context for each window
	//another flag called CLASSDC asks for a context for the entire class
	/*
	HREDRAW and VREDRAW force to re draw the entire window if resized,
	not just the resized section.
	*/
	WindowClass.style = CS_HREDRAW|CS_VREDRAW;
	
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
			GLOBAL_GameRunning = true;

			int gradientXoffset = 0;
			int gradientYoffset = 0;
			//we dont pass the window handle since we want to treat ALL the messages sent to us, not just to the window
			while (GLOBAL_GameRunning)
			{
				//BOOL returnValue = GetMessageA(&Message, 0, 0, 0);

				while (PeekMessageA(&Message, WindowHandle, 0, 0, PM_REMOVE))
				{
					if (Message.message == WM_CLOSE || Message.message == WM_QUIT )
					{
						GLOBAL_GameRunning = false;
					}

					//we actually treat the message
					TranslateMessage(&Message);//messages need a little bit of processing
					DispatchMessageA(&Message);//now dispatch it
				}
				//controller input checking 
				ControllerInputTreating();

				//our gameloop
				//update our bitmap
				renderGradient(BackBuffer, gradientXoffset, gradientYoffset);

				//actually paint the bitmap
				//first we get device context
				HDC WindowContext = GetDC(WindowHandle);
				RectDimensions clientWindowRect = GetContextDimensions(WindowHandle);

				HandmadeUpdateWindow(BackBuffer, WindowContext, 0, 0, clientWindowRect.width, clientWindowRect.height);

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
