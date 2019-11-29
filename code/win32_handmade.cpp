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
#include <dsound.h>

#define internal_function static
#define local_persistent static
#define global_variable static

typedef int32_t bool32;

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

struct HandmadeAudioInfo
{
	int32_t samplesPerSec;
	int32_t bufferSize;
	int32_t cTonesPerSec;//hz of C frequency
	int32_t squareWavePeriod;
	int32_t halfPeriod;
	int32_t bytesPerSample;
	uint32_t soundCounter;
	uint16_t soundVolume;
	bool startOver;
};

//static auto declares to 0
global_variable bool GLOBAL_GameRunning;

global_variable BufferData BackBuffer;
global_variable RectDimensions BufferDimensions{ 1280, 720 };
global_variable LPDIRECTSOUNDBUFFER secondaryBuffer;
global_variable HandmadeAudioInfo audioInf;

//trick for loading Xinput1_3.dll ourselves.
//could probably use the 1_4 version, but 1_3 is more reliable to be on older PCs

//now "X_INPUT_GET_STATE(name)" defines a new function with that name
#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE* pState) 
//this line creates the "xinput_get_state" type and makes it so that 
//X_INPUT_GET_STATE(something of "xinput_get_state" type) does: 
//DWORD WINAPI "name of var"(DWORD dwUserIndex, XINPUT_STATE* pState)
typedef X_INPUT_GET_STATE(xinput_get_state);
//declare a default function just for safety
X_INPUT_GET_STATE(XInputGetState_id) //this line is equal to: DWORD WINAPI XInputGetState_id(DWORD dwUserIndex, XINPUT_STATE* pState) 
{
	return (ERROR_DEVICE_NOT_CONNECTED);
}
//now we get our GetState function with a slightly different name than original(for conflicts)
//and set it to our default value
global_variable xinput_get_state* XInputGetState_ = XInputGetState_id;
//and do the trick of defining the original name as our new variable
#define XInputGetState XInputGetState_

//same for XInputSetState
#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION* pVibration)
typedef X_INPUT_SET_STATE(xinput_set_state);

X_INPUT_SET_STATE(XInputSetState_id)
{
	return (ERROR_DEVICE_NOT_CONNECTED);
}
global_variable xinput_set_state* XInputSetState_ = XInputSetState_id;
#define XInputSetState XInputSetState_

//DirectSound definitions

#define DIRECTSOUND_CREATE(name) HRESULT WINAPI name(LPCGUID pcGuidDevice, LPDIRECTSOUND8* ppDS8, LPUNKNOWN pUnkOuter)
typedef DIRECTSOUND_CREATE(direct_sound_create);

//extra info of windows:
//CALLBACK means that it calls US
//WINAPI means that we call windows

internal_function void LoadSound(HWND window, int32_t samplesPerSec, int32_t bufferSize)
{
	//Load Lib
	HMODULE directSoundStatus = LoadLibrary("dsound.dll");
	direct_sound_create* directSoundCreation = nullptr;
	if (directSoundStatus)
	{
		directSoundCreation = (direct_sound_create*)GetProcAddress(directSoundStatus, "DirectSoundCreate8");
	}
	else
	{
		//TODO: log error loading sound
		return;
	}
	if (!directSoundCreation)
	{
		//TODO: error loading function
		return;
	}

	LPDIRECTSOUND8 dsObject = {};
	if (!SUCCEEDED(directSoundCreation(0, &dsObject, 0)))
	{
		//TODO: log error crteating DS
		return;
	}
	//now we set the cooperative level (it is necessary after creation)
	if (!SUCCEEDED(dsObject->SetCooperativeLevel(window, DSSCL_PRIORITY)))
	{
		//TODO: log error crteating DS
		return;
	}

	//primary buffer
	DSBUFFERDESC primaryBufferDesc = {};
	primaryBufferDesc.dwSize = sizeof(primaryBufferDesc);
	primaryBufferDesc.dwFlags = DSBCAPS_PRIMARYBUFFER;

	LPDIRECTSOUNDBUFFER primaryBuffer;
	if (!SUCCEEDED(dsObject->CreateSoundBuffer(&primaryBufferDesc, &primaryBuffer, 0)))
	{
		//TODO: log error crteating first buffer
		return;
	}
	WAVEFORMATEX bufferFormat = {};
	bufferFormat.wFormatTag = WAVE_FORMAT_PCM;
	bufferFormat.nChannels = 2;
	bufferFormat.nSamplesPerSec = samplesPerSec;
	bufferFormat.wBitsPerSample = 16;//could also be 8, but 16 is better quality
	bufferFormat.nBlockAlign = bufferFormat.nChannels*bufferFormat.wBitsPerSample/8;
	bufferFormat.nAvgBytesPerSec = samplesPerSec* bufferFormat.nBlockAlign;
	bufferFormat.cbSize = 0;
	if (!SUCCEEDED(primaryBuffer->SetFormat(&bufferFormat)))
	{
		//TODO: log error setting buffer format
		return;
	}

	//secondary buffer
	DSBUFFERDESC secondaryBufferDesc = {};
	secondaryBufferDesc.dwSize = sizeof(secondaryBufferDesc);
	secondaryBufferDesc.dwBufferBytes = bufferSize;
	secondaryBufferDesc.lpwfxFormat = &bufferFormat;

	if (!SUCCEEDED(dsObject->CreateSoundBuffer(&secondaryBufferDesc, &secondaryBuffer, 0)))
	{
		//TODO: log error creating second buffer
		return;
	}
	
}

internal_function void loadControllerLib()
{
	//load controller input library
	HMODULE loadStatus = LoadLibrary("Xinput1_4.dll");
	if (!loadStatus)
	{
		loadStatus = LoadLibrary("Xinput1_3.dll");
	}
	if (loadStatus)
	{
		//now we load the function
		XInputSetState = (xinput_set_state*)GetProcAddress(loadStatus, "XInputSetState");
		XInputGetState = (xinput_get_state*)GetProcAddress(loadStatus, "XInputGetState");
	}
}

internal_function void InputTreating(int index, XINPUT_STATE* inputState, int* offsetX, int* offsetY)
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

	int16_t rightThumbX = inputState->Gamepad.sThumbRX;
	int16_t rightThumbY = inputState->Gamepad.sThumbRY;

	//short 0-255
	short leftTrigger = inputState->Gamepad.bLeftTrigger;
	short rightTrigger = inputState->Gamepad.bRightTrigger;

	if (up)
	{
		*offsetY+= 2;
	}
	if (down)
	{
		*offsetY -= 2;
	}
	if (left)
	{
		*offsetX += 2;
	}
	if (right)
	{
		*offsetX -= 2;
	}
	short threshold = 10000;
	if (abs(leftThumbY) > threshold)
	{
		*offsetY += int(leftThumbY >> 13);
	}
	if (abs(leftThumbX) > threshold)
	{
		*offsetX -= int(leftThumbX >> 13);
	}

	if (abs(rightThumbY) > threshold)
	{
		*offsetY += int(rightThumbY >> 13);
	}
	if (abs(rightThumbX) > threshold)
	{
		*offsetX -= int(rightThumbX >> 13);
	}
	_XINPUT_VIBRATION vib;
	vib.wLeftMotorSpeed = 0;
	vib.wRightMotorSpeed = 0;
	if (leftTrigger > 250 || 
		rightTrigger > 250)
	{
		vib.wLeftMotorSpeed = 65000;
		vib.wRightMotorSpeed = 65000;
		XInputSetState(index, &vib);
	}
	else
	{
		XInputSetState(index, &vib);
	}
}

internal_function void ControllerInputTreating(int* offsetX, int* offsetY)
{
	//Dword = 32bit = int
	//Word: 16bit
	//byte : 8bit
	//short 16bit signed
	for (int controllerIndex = 0; controllerIndex < XUSER_MAX_COUNT; ++controllerIndex)
	{
		XINPUT_STATE controllerState;
		int res = XInputGetState(controllerIndex, &controllerState);
		if (res == ERROR_SUCCESS)
		{
			//succeded
			InputTreating(controllerIndex, &controllerState, offsetX, offsetY);
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
			uint8_t R = (uint8_t)i + gradYOffset;
			uint8_t G = (uint8_t)j + gradXOffset;
			uint8_t B = (uint8_t)(gradXOffset + gradYOffset);

			//we store it as said above
			*pixel = (uint32_t)(B | (G << 8) | (R<<16));
			++pixel;
		}
		row += Buffer.Pitch;
	}
}

internal_function void HandmadePlaySound()
{
	DWORD playCursor = 0;
	DWORD writeCursor = 0;
	if (!SUCCEEDED(secondaryBuffer->GetCurrentPosition(&playCursor, &writeCursor)))
	{
		//TODO: log error getting position
		return;
	}
	//lockOffset is the size from the start to the point where lock begins
	DWORD lockOffset;
	if (audioInf.startOver)
	{
		audioInf.soundCounter = lockOffset = writeCursor;
		audioInf.startOver = false;
	}
	else
	{
		lockOffset = audioInf.soundCounter * audioInf.bytesPerSample % audioInf.bufferSize;
	}
	DWORD bytesToWrite = 0;
	if (lockOffset > playCursor)
	{
		//case we are in front of write cursor
		bytesToWrite = audioInf.bufferSize - lockOffset + playCursor;
	}
	else
	{
		//case we are behind write cursor
		bytesToWrite = playCursor - lockOffset;
	}
	if (bytesToWrite == 0)
	{
		return;
	}
	VOID* firstLockedPart;
	VOID* secondLockedPart;
	DWORD firstLockedSize;
	DWORD secondLockedSize;
	HRESULT resLock = secondaryBuffer->Lock(lockOffset, bytesToWrite, &firstLockedPart, &firstLockedSize,
		&secondLockedPart, &secondLockedSize, 0);
	if (!SUCCEEDED(resLock))
	{
		//TODO: log error locking buffer
		OutputDebugStringA("ERROR LOCK");
		return;
	}
	//fill buffer at first locked part
	int16_t* bufferPointer = (int16_t*)firstLockedPart;
	DWORD firstLockedSamples = firstLockedSize / audioInf.bytesPerSample;
	for (DWORD iterator = 0; iterator < firstLockedSamples; ++iterator)
	{
		int16_t sampleValue;
		if (audioInf.soundCounter / audioInf.halfPeriod % 2 == 0)
		{
			//case high wave
			sampleValue = audioInf.soundVolume;
		}
		else
		{
			//case low wave
			sampleValue = -audioInf.soundVolume;
		}
		
		*bufferPointer++ = sampleValue;//left ear sample
		*bufferPointer++ = sampleValue;//right ear sample
		++audioInf.soundCounter;
	}
	//fill buffer at second locked part
	bufferPointer = (int16_t*)secondLockedPart;
	DWORD secondLockedSamples = secondLockedSize / audioInf.bytesPerSample;
	for (DWORD iterator = 0; iterator < secondLockedSamples; ++iterator)
	{
		int16_t sampleValue;
		if (audioInf.soundCounter / audioInf.halfPeriod % 2 == 0)
		{
			//case high wave
			sampleValue = audioInf.soundVolume;
		}
		else
		{
			//case low wave
			sampleValue = -audioInf.soundVolume;
		}
		*bufferPointer++ = sampleValue;//left ear sample
		*bufferPointer++ = sampleValue;//right ear sample
		++audioInf.soundCounter;
	}

	HRESULT resUnlock = secondaryBuffer->Unlock(firstLockedPart, firstLockedSize,
		secondLockedPart, secondLockedSize);
	if (!SUCCEEDED(resUnlock))
	{
		//TODO: log error unlocking
		OutputDebugStringA("ERROR UNLOCK");
		return;
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
	Buffer->BufferMemory =  VirtualAlloc(0, BitMapMemorySize, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);

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

bool ChangedTone(int32_t A, int32_t B)
{
	return (A != B);
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

		//keyboard input messages
		case WM_SYSKEYDOWN:
		case WM_SYSKEYUP:
		case WM_KEYDOWN:
		case WM_KEYUP:
		{
			
			//checking if key is just pressed
			bool wasPressed = ((Lparam >> 30 & 1) == 0);
			bool isReleased = ((Lparam >> 31 & 0) == 1);
			bool32 altPressed = (Lparam >> 29 & 1) == 1;

			//checking keys
			if (wasPressed && !isReleased)
			{
				int32_t previousTone = audioInf.cTonesPerSec;
				if (Wparam == VK_ESCAPE)
				{
					OutputDebugStringA("Escape");
				}
				if (Wparam == 'Q')
				{
					OutputDebugStringA("Q");
					audioInf.cTonesPerSec = 261;//C, Do
					audioInf.squareWavePeriod = audioInf.samplesPerSec / audioInf.cTonesPerSec;
					audioInf.halfPeriod = audioInf.squareWavePeriod / 2;
				}
				if (Wparam == 'W')
				{
					OutputDebugStringA("W");
					audioInf.cTonesPerSec = 293;//D, Re
					audioInf.squareWavePeriod = audioInf.samplesPerSec / audioInf.cTonesPerSec;
					audioInf.halfPeriod = audioInf.squareWavePeriod / 2;
				}
				if (Wparam == 'E')
				{
					OutputDebugStringA("E");
					audioInf.cTonesPerSec = 329;//E, Mi
					audioInf.squareWavePeriod = audioInf.samplesPerSec / audioInf.cTonesPerSec;
					audioInf.halfPeriod = audioInf.squareWavePeriod / 2;
					
				}
				if (Wparam == 'A')
				{
					OutputDebugStringA("A");
					audioInf.cTonesPerSec = 349;//F, Fa
					audioInf.squareWavePeriod = audioInf.samplesPerSec / audioInf.cTonesPerSec;
					audioInf.halfPeriod = audioInf.squareWavePeriod / 2;
					
				}
				if (Wparam == 'S')
				{
					OutputDebugStringA("S");
					audioInf.cTonesPerSec = 392;//G, Sol
					audioInf.squareWavePeriod = audioInf.samplesPerSec / audioInf.cTonesPerSec;
					audioInf.halfPeriod = audioInf.squareWavePeriod / 2;
				}
				if (Wparam == 'D')
				{
					OutputDebugStringA("D");
					audioInf.cTonesPerSec = 440;//A, La
					audioInf.squareWavePeriod = audioInf.samplesPerSec / audioInf.cTonesPerSec;
					audioInf.halfPeriod = audioInf.squareWavePeriod / 2;
				}
				if (Wparam == 'Z')
				{
					OutputDebugStringA("Z");
					audioInf.cTonesPerSec = 493;//B, Si
					audioInf.squareWavePeriod = audioInf.samplesPerSec / audioInf.cTonesPerSec;
					audioInf.halfPeriod = audioInf.squareWavePeriod / 2;
				}
				if (Wparam == 'X')
				{
					OutputDebugStringA("X");
					audioInf.cTonesPerSec = 523;//C, Do
					audioInf.squareWavePeriod = audioInf.samplesPerSec / audioInf.cTonesPerSec;
					audioInf.halfPeriod = audioInf.squareWavePeriod / 2;
				}
				if (Wparam == VK_SPACE)
				{
					OutputDebugStringA("SPACEBAR");
				}
				if (Wparam == VK_UP)
				{
					OutputDebugStringA("ARROW UP");
				}
				if (Wparam == VK_LEFT)
				{
					OutputDebugStringA("ARROW LEFT");
				}
				if (Wparam == VK_RIGHT)
				{
					OutputDebugStringA("ARROW RIGHT");
				}
				if (Wparam == VK_DOWN)
				{
					OutputDebugStringA("ARROW DOWN");
				}
				if (altPressed)
				{
					if (Wparam == VK_F4)
					{
						GLOBAL_GameRunning = false;
					}
				}
				audioInf.startOver = ChangedTone(previousTone, audioInf.cTonesPerSec);
			}
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
	loadControllerLib();

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
			audioInf = {};
			//initialize sound variables
			audioInf.samplesPerSec = 48000;
			audioInf.bufferSize = 48000 * 2 * sizeof(int16_t);
			audioInf.cTonesPerSec = 261;//hz of C frequency
			audioInf.squareWavePeriod = audioInf.samplesPerSec / audioInf.cTonesPerSec;
			audioInf.halfPeriod = audioInf.squareWavePeriod / 2;
			audioInf.bytesPerSample = 2 * sizeof(int16_t);
			audioInf.soundVolume = 150;

			audioInf.soundCounter = 0;

			//DirectSound loading
			LoadSound(WindowHandle, audioInf.samplesPerSec, audioInf.bufferSize);
			secondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);

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
				ControllerInputTreating(&gradientXoffset, &gradientYoffset);

				//our gameloop
				//update our bitmap
				renderGradient(BackBuffer, gradientXoffset, gradientYoffset);

				//audio output function
				HandmadePlaySound();

				//actually paint the bitmap
				//first we get device context
				HDC WindowContext = GetDC(WindowHandle);
				RectDimensions clientWindowRect = GetContextDimensions(WindowHandle);

				HandmadeUpdateWindow(BackBuffer, WindowContext, 0, 0, clientWindowRect.width, clientWindowRect.height);

				/*++gradientXoffset;
				++gradientYoffset;*/
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
