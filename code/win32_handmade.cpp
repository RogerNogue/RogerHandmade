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

#include "handmade.cpp"

/*
Future mostly needed implementations
- Saved game locations
- getting a handle to our own exec file
- asset loading path
- threading
- raw input (support for multiple keyboards)
- sleep/timeBeginPeriod (for laptops and stuffs to save battery)
- ClipCursor() (Multimonitor support)
- WM_SETCURSOR (control cursor visibility)
- QueryCancelAutoplay
-WM_ACTIVATEAPP (for when we are not the active app)
- Blit speed improvements (BitBlt)
- Hardware acceleration (OpenGL or Direct3D or both)
- GetKeyboardLayout (for French keyboards, international WASD support)

*/

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
	int32_t period;
	int32_t bytesPerSample;
	uint32_t soundCounter;
	uint16_t soundVolume;
	bool firstLoop;
	int32_t lockOffset;
	int32_t bytesToWrite;
};

//static auto declares to 0
global_variable bool GLOBAL_GameRunning;

global_variable BufferData BackBuffer;
global_variable RectDimensions BufferDimensions{ 1280, 720 };
global_variable LPDIRECTSOUNDBUFFER secondaryBuffer;
global_variable HandmadeAudioInfo audioInf;

//ArraySize function
#define ArraySize(arrayParameter) (sizeof(arrayParameter) / sizeof((arrayParameter)[0]))

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

internal_function bool PlatformReadEntireFile(char* filename, FileInfo* result)
{
	HANDLE fileHandle = CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, 0,  OPEN_EXISTING, 0, 0);
	if (fileHandle == INVALID_HANDLE_VALUE)
	{
		//TODO: LOG error loading

		return false;
	}
	LARGE_INTEGER fileSize = {};
	if (!GetFileSizeEx(fileHandle, &fileSize))
	{
		//TODO: Log error getting file size
		return false;
	}
	
	Assert(fileSize.QuadPart == (LONGLONG)fileSize.LowPart);//ensure that the size fits in a int32 variable

	result->memPointer = (uint64_t*)VirtualAlloc(0, fileSize.LowPart, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	if (!ReadFile(fileHandle, result->memPointer, fileSize.LowPart, (LPDWORD)&result->memSize, 0))
	{
		//TODO: log error reading file
		CloseHandle(fileHandle);
		return false;
	}

	CloseHandle(fileHandle);

	return true;
}

internal_function bool PlatformFreeFileMemory(FileInfo* result)
{
	result->memSize = 0;
	if (!VirtualFree(result->memPointer, 0, MEM_RELEASE))
	{
		//TODO log error freeing file mem
		return false;
	}
	return true;
}

internal_function bool PlatformWriteToFile(char* filename, FileInfo* writeInfo)
{
	HANDLE fileHandle = CreateFileA(filename, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
	if (fileHandle == INVALID_HANDLE_VALUE)
	{
		//TODO: LOG error loading

		return false;
	}
	DWORD bytesWritten = 0;
	if (!WriteFile(fileHandle, writeInfo->memPointer, writeInfo->memSize, &bytesWritten, 0))
	{
		//TODO: log error writing on file
		return false;
	}

	CloseHandle(fileHandle);

	return true;
}

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
//sets the sound buffer contents to 0
internal_function void flushSoundBuffer()
{
	VOID* firstLockedPart;
	VOID* secondLockedPart;
	DWORD firstLockedSize;
	DWORD secondLockedSize;
	HRESULT resLock = secondaryBuffer->Lock(0, audioInf.bufferSize, &firstLockedPart, &firstLockedSize,
		&secondLockedPart, &secondLockedSize, 0);
	if (!SUCCEEDED(resLock))
	{
		//TODO: log error locking buffer
		OutputDebugStringA("ERROR LOCK\n");
		return;
	}
	
	uint8_t* iterPointer = (uint8_t*)firstLockedPart;
	for (DWORD iterator = 0; iterator < (firstLockedSize + secondLockedSize); ++iterator)
	{
		//in case first part is full, start filling the second part(second part is beggining of the buffer)
		if (iterator == firstLockedSize)
		{
			iterPointer = (uint8_t*)secondLockedPart;
		}
		*iterPointer = 0;
		++iterPointer;
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

internal_function void loadControllerLib()
{
	//load controller input library
	HMODULE loadStatus = LoadLibrary("Xinput1_4.dll");
	if (!loadStatus)
	{
		loadStatus = LoadLibrary("Xinput1_3.dll");
	}
	if (!loadStatus)
	{
		loadStatus = LoadLibrary("Xinput9_1_0.dll");
	}
	if (loadStatus)
	{
		//now we load the function
		XInputSetState = (xinput_set_state*)GetProcAddress(loadStatus, "XInputSetState");
		XInputGetState = (xinput_get_state*)GetProcAddress(loadStatus, "XInputGetState");
	}
}

internal_function void SetControllerVibration(GameInput* gameInput)
{
	for (int i = 0; i < ArraySize(gameInput->controllers); ++i)
	{
		_XINPUT_VIBRATION vib;
		vib.wLeftMotorSpeed = gameInput->controllers[i].leftMotorSpeed;
		vib.wRightMotorSpeed = gameInput->controllers[i].rightMotorSpeed;

		XInputSetState(i, &vib);
	}
}

inline internal_function void ControllerBasicInputTreating(ButtonState* oldC, ButtonState* newC,
	int buttonMask, WORD pressedButtons)
{
	newC->pressedAtEnd = (pressedButtons & buttonMask);
	if (oldC->pressedAtEnd != newC->pressedAtEnd)
	{
		newC->transitions = 1;
	}
	else
	{
		newC->transitions = 0;
	}
}

inline internal_function void KeyboardBasicInputTreating(bool wasPressed, bool isReleased, ButtonState* key)
{
	key->pressedAtEnd = !isReleased;
	if ((!wasPressed && !isReleased) || (wasPressed && isReleased))
	{
		key->transitions = 1;
	}
	else
	{
		key->transitions = 0;
	}
}

internal_function void NormalizeJoystick(float* finalV, float* minV,
	float* maxV, SHORT controllerValue)
{
	if (controllerValue < 0)
	{
		*finalV = *minV = *maxV = (float)controllerValue / 32768.0f;
	}
	else
	{
		*finalV = *minV = *maxV = (float)controllerValue / 32767.0f;
	}
}

internal_function void InputTreating(int index, XINPUT_STATE* inputState, 
	int* offsetX, int* offsetY, GameInput* newInput, GameInput* oldInput)
{
	ControllerInput* currentControllerInput = &newInput->controllers[index];
	ControllerInput* lastControllerInput = &oldInput->controllers[index];
	//newInput = newInput->controllers
	
	//TODO:
	//state has a packet number that indicates how many times the state of the controller changed
	//this may give us info about having to increase this function call frequency rate in the future.
	
	//for now we will not care about controller index
	ControllerBasicInputTreating(&lastControllerInput->up, &currentControllerInput->up,
		XINPUT_GAMEPAD_DPAD_UP, inputState->Gamepad.wButtons);
	ControllerBasicInputTreating(&lastControllerInput->down, &currentControllerInput->down,
		XINPUT_GAMEPAD_DPAD_DOWN, inputState->Gamepad.wButtons);
	ControllerBasicInputTreating(&lastControllerInput->left, &currentControllerInput->left,
		XINPUT_GAMEPAD_DPAD_LEFT, inputState->Gamepad.wButtons);
	ControllerBasicInputTreating(&lastControllerInput->right, &currentControllerInput->right,
		XINPUT_GAMEPAD_DPAD_RIGHT, inputState->Gamepad.wButtons);
	ControllerBasicInputTreating(&lastControllerInput->start, &currentControllerInput->start,
		XINPUT_GAMEPAD_START, inputState->Gamepad.wButtons);
	ControllerBasicInputTreating(&lastControllerInput->back, &currentControllerInput->back,
		XINPUT_GAMEPAD_BACK, inputState->Gamepad.wButtons);
	ControllerBasicInputTreating(&lastControllerInput->leftShoulder, &currentControllerInput->leftShoulder,
		XINPUT_GAMEPAD_LEFT_SHOULDER, inputState->Gamepad.wButtons);
	ControllerBasicInputTreating(&lastControllerInput->rightShoulder, &currentControllerInput->rightShoulder,
		XINPUT_GAMEPAD_RIGHT_SHOULDER, inputState->Gamepad.wButtons);

	//joysticks normalizing results, range is -32768 and 32767
	NormalizeJoystick(&currentControllerInput->leftFinalX, &currentControllerInput->leftMaxX,
		&currentControllerInput->leftMinX, inputState->Gamepad.sThumbLX);
	NormalizeJoystick(&currentControllerInput->leftFinalY, &currentControllerInput->leftMaxY,
		&currentControllerInput->leftMinY, inputState->Gamepad.sThumbLY);
	NormalizeJoystick(&currentControllerInput->rightFinalX, &currentControllerInput->rightMaxX,
		&currentControllerInput->rightMinX, inputState->Gamepad.sThumbRX);
	NormalizeJoystick(&currentControllerInput->rightFinalY, &currentControllerInput->rightMaxY,
		&currentControllerInput->rightMinY, inputState->Gamepad.sThumbRY);

	//triggers
	//short 0-255, we get a normalized value
	currentControllerInput->leftTriggerFinal = currentControllerInput->leftTriggerMax =
		currentControllerInput->leftTriggerMin = ((float)inputState->Gamepad.bLeftTrigger) / 255.0f;
	currentControllerInput->rightTriggerFinal = currentControllerInput->rightTriggerMax =
		currentControllerInput->rightTriggerMin = ((float)inputState->Gamepad.bRightTrigger) / 255.0f;
}

internal_function void ControllerInputTreating(int* offsetX, int* offsetY, 
	GameInput* newInput, GameInput* oldInput)
{
	//Dword = 32bit = int
	//Word: 16bit
	//byte : 8bit
	//short 16bit signed
	for (int controllerIndex = 0; controllerIndex < ArraySize(newInput->controllers); ++controllerIndex)
	{
		XINPUT_STATE controllerState;
		int res = XInputGetState(controllerIndex, &controllerState);
		if (res == ERROR_SUCCESS)
		{
			//succeded
			InputTreating(controllerIndex, &controllerState, offsetX, offsetY, newInput, oldInput);
		}
		else
		{
			//error or controller not connected
		}
	}

}

internal_function void HandmadeGetSoundWritingValues()
{
	DWORD playCursor = 0;
	DWORD writeCursor = 0;
	if (!SUCCEEDED(secondaryBuffer->GetCurrentPosition(&playCursor, &writeCursor)))
	{
		//TODO: log error getting position
		return;
	}
	//lockOffset is the size from the start to the point where lock begins
	audioInf.lockOffset = (audioInf.soundCounter * audioInf.bytesPerSample) % audioInf.bufferSize;
	audioInf.bytesToWrite = 0;
	if (audioInf.firstLoop)
	{
		audioInf.soundCounter = audioInf.lockOffset = playCursor;
		audioInf.bytesToWrite = int32_t(audioInf.samplesPerSec * 0.5f);//we fill 0.50 secs of sound
		audioInf.firstLoop = false;
		return;
	}
	DWORD difflockWrite = (DWORD)audioInf.lockOffset - writeCursor;
	if (difflockWrite < 0)
	{
		//case we went back to the beggining of the buffer
		difflockWrite = audioInf.bufferSize - writeCursor + audioInf.lockOffset;
	}
	if ((difflockWrite) < (audioInf.samplesPerSec * 0.15))
	{
		//case we have to write since play cursor is catching up
		audioInf.bytesToWrite = (int32_t)((float)audioInf.samplesPerSec * 0.2f);//we fill 0.20 secs of sound

	}
}
internal_function void HandmadeWriteInSoundBuffer(SoundData* soundInfo)
{
	//if nothing to write, we leave
	if (audioInf.bytesToWrite == 0)
	{
		return;
	}
	VOID* firstLockedPart;
	VOID* secondLockedPart;
	DWORD firstLockedSize;
	DWORD secondLockedSize;
	HRESULT resLock = secondaryBuffer->Lock(audioInf.lockOffset, audioInf.bytesToWrite, &firstLockedPart, &firstLockedSize,
		&secondLockedPart, &secondLockedSize, 0);
	if (!SUCCEEDED(resLock))
	{
		//TODO: log error locking buffer
		OutputDebugStringA("ERROR LOCK\n");
		return;
	}
	//fill buffer
	int16_t* bufferPointer = (int16_t*)firstLockedPart;
	int16_t* gamePointer = (int16_t*)soundInfo->bufferPointer;
	DWORD firstLockedSamples = firstLockedSize / audioInf.bytesPerSample;
	DWORD secondLockedSamples = secondLockedSize / audioInf.bytesPerSample;

	for (DWORD iterator = 0; iterator < (firstLockedSamples+secondLockedSamples); ++iterator)
	{
		//in case first part is full, start filling the second part(second part is beggining of the buffer)
		if (iterator == firstLockedSamples)
		{
			bufferPointer = (int16_t*)secondLockedPart;
		}

		*bufferPointer++ = *gamePointer++;//left ear sample
		*bufferPointer++ = *gamePointer++;//right ear sample
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

internal_function void TreatKeyboardInput(WPARAM Wparam, 
	LPARAM Lparam, KeyboardInput* keyboardInput, bool wasPressed,
	bool isReleased, bool32 altPressed)
{
	//checking keys
	if (Wparam == VK_ESCAPE)
	{
		KeyboardBasicInputTreating(wasPressed, isReleased, &keyboardInput->Escape);
		GLOBAL_GameRunning = false;
	}
	if (Wparam == 'Q')
	{
		KeyboardBasicInputTreating(wasPressed, isReleased, &keyboardInput->Q);
	}
	if (Wparam == 'W')
	{
		KeyboardBasicInputTreating(wasPressed, isReleased, &keyboardInput->W);
	}
	if (Wparam == 'E')
	{
		KeyboardBasicInputTreating(wasPressed, isReleased, &keyboardInput->E);
	}
	if (Wparam == 'A')
	{
		KeyboardBasicInputTreating(wasPressed, isReleased, &keyboardInput->A);
	}
	if (Wparam == 'S')
	{
		KeyboardBasicInputTreating(wasPressed, isReleased, &keyboardInput->S);
	}
	if (Wparam == 'D')
	{
		KeyboardBasicInputTreating(wasPressed, isReleased, &keyboardInput->D);
	}
	if (Wparam == 'Z')
	{
		KeyboardBasicInputTreating(wasPressed, isReleased, &keyboardInput->Z);
	}
	if (Wparam == 'X')
	{
		KeyboardBasicInputTreating(wasPressed, isReleased, &keyboardInput->X);
	}
	if (Wparam == VK_SPACE)
	{
		KeyboardBasicInputTreating(wasPressed, isReleased, &keyboardInput->Spacebar);
	}
	if (Wparam == VK_UP)
	{
		KeyboardBasicInputTreating(wasPressed, isReleased, &keyboardInput->Up);
	}
	if (Wparam == VK_LEFT)
	{
		KeyboardBasicInputTreating(wasPressed, isReleased, &keyboardInput->Left);
	}
	if (Wparam == VK_RIGHT)
	{
		KeyboardBasicInputTreating(wasPressed, isReleased, &keyboardInput->Right);
	}
	if (Wparam == VK_DOWN)
	{
		KeyboardBasicInputTreating(wasPressed, isReleased, &keyboardInput->Down);
	}
	if (altPressed)
	{
		if (Wparam == VK_F4)
		{
			GLOBAL_GameRunning = false;
		}
	}
}

internal_function inline void treatSystemKey(WPARAM Wparam,
	LPARAM Lparam, KeyboardInput* keyboardInput)
{
	//checking if key is just pressed
	bool wasPressed = ((Lparam >> 30 & 1) == 0);
	bool isReleased = ((Lparam >> 31 & 0) == 1);
	bool32 altPressed = (Lparam >> 29 & 1) == 1;

	TreatKeyboardInput(Wparam, Lparam, keyboardInput,
		wasPressed, isReleased, altPressed);
}
internal_function inline void treatNormalKey(WPARAM Wparam,
	LPARAM Lparam, KeyboardInput* keyboardInput)
{
	//checking if key is just pressed
	bool wasPressed = ((Lparam >> 30 & 1) == 1);
	bool isReleased = ((Lparam >> 31 & 1) == 1);
	bool32 altPressed = (Lparam >> 24 & 1) == 1;

	TreatKeyboardInput(Wparam, Lparam, keyboardInput,
		wasPressed, isReleased, altPressed);
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
			audioInf.bufferSize = 48000 * 2 * sizeof(int16_t); //2 seconds of sound
			audioInf.cTonesPerSec = 261;//hz of C frequency
			audioInf.period = audioInf.samplesPerSec / audioInf.cTonesPerSec;
			audioInf.bytesPerSample = 2 * sizeof(int16_t);
			audioInf.soundVolume = 2000;
			audioInf.firstLoop = true;

			//DirectSound loading
			LoadSound(WindowHandle, audioInf.samplesPerSec, audioInf.bufferSize);
			flushSoundBuffer();
			secondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);

			//input structures initialization
			GameInput input[2] = {};
			GameInput* newInput = &input[0];
			GameInput* oldInput = &input[1];

			KeyboardInput keyboardInput = {};

			SoundData gameSoundInfo = {};
			gameSoundInfo.samplesPerSec = audioInf.samplesPerSec;
			//no need to free that since its gonna be used for the whole exec and then windows fill free for us
			gameSoundInfo.bufferPointer = (int16_t*)VirtualAlloc(0, audioInf.bufferSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

			//allocation of game memory
			GameMemory gameMem = {};
			gameMem.persistentMemorySize = Megabytes(64);
			gameMem.transistentMemorySize = Gigabytes((uint64_t)1);

			LPVOID startMemPosition;
#if INTERNAL_BUILD
			startMemPosition = (LPVOID)Terabytes((uint64_t)2);
#else
			startMemPosition = (LPVOID)0;
#endif
			uint64_t totalMemSize = gameMem.persistentMemorySize + gameMem.transistentMemorySize;
			gameMem.persistentMemory = (uint8_t*)VirtualAlloc(startMemPosition, totalMemSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
			//cast to uint64 for calculations. Note that if the cast was to uint664_t* there would be pointer arithmetic and would allocate more than we want
			gameMem.transistentMemory = (void*)((uint64_t)gameMem.persistentMemory + gameMem.persistentMemorySize);

			if (gameSoundInfo.bufferPointer && gameMem.persistentMemory && gameMem.transistentMemory)
			{

				//get our rendering Buffer going:
				RenderBufferData renderingBuffer;
				renderingBuffer.BufferMemory = BackBuffer.BufferMemory;
				renderingBuffer.BufferWidth = BackBuffer.BufferWidth;
				renderingBuffer.BufferHeight = BackBuffer.BufferHeight;
				renderingBuffer.Pitch = BackBuffer.Pitch;
				renderingBuffer.BytesPerPixel = BackBuffer.BytesPerPixel;

				/*Window created, now we have to start a message loop since windows
				does not do that by default. This loop treats all the messages that
				windows sends to our window application*/
				MSG Message;
				GLOBAL_GameRunning = true;

				int gradientXoffset = 0;
				int gradientYoffset = 0;
				//we dont pass the window handle since we want to treat ALL the messages sent to us, not just to the window

				//timers setup
				//first we get the frequency
				LARGE_INTEGER queryFreq = {};
				QueryPerformanceFrequency(&queryFreq);

				//now get counter for first iteration
				LARGE_INTEGER prevCounter = {};
				QueryPerformanceCounter(&prevCounter);
				LARGE_INTEGER currentCounter = {};

				uint64_t prevCycleCounter = __rdtsc();
				int64_t currCycleCounter = 0;

				while (GLOBAL_GameRunning)
				{
					//BOOL returnValue = GetMessageA(&Message, 0, 0, 0);

					while (PeekMessageA(&Message, WindowHandle, 0, 0, PM_REMOVE))
					{
						if (Message.message == WM_CLOSE || Message.message == WM_QUIT)
						{
							GLOBAL_GameRunning = false;
						}
						if (Message.message == WM_SYSKEYUP ||
							Message.message == WM_SYSKEYDOWN)
						{
							treatSystemKey(Message.wParam, Message.lParam, &keyboardInput);
						}
						if (Message.message == WM_KEYUP ||
							Message.message == WM_KEYDOWN)
						{
							treatNormalKey(Message.wParam, Message.lParam, &keyboardInput);
						}
						else
						{
							//we actually treat the message
							TranslateMessage(&Message);//messages need a little bit of processing
							DispatchMessageA(&Message);//now dispatch it
						}
						//keyboard input messages

						
					}
					//controller input checking 
					ControllerInputTreating(&gradientXoffset, &gradientYoffset, newInput, oldInput);

					//get sound buffer info
					HandmadeGetSoundWritingValues();
					gameSoundInfo.sizeToWrite = audioInf.bytesToWrite;
					gameSoundInfo.soundVolume = audioInf.soundVolume;

					//our gameloop
					GameUpdateAndRender(&renderingBuffer, &gameSoundInfo, 
						newInput, &keyboardInput, &gameMem);

					//set controller motor speed
					SetControllerVibration(newInput);
					//swap input new and old values for next iteration
					GameInput* temp = oldInput;
					oldInput = newInput;
					newInput = temp;

					//audio output function
					HandmadeWriteInSoundBuffer(&gameSoundInfo);

					//actually paint the bitmap
					//first we get device context
					HDC WindowContext = GetDC(WindowHandle);
					RectDimensions clientWindowRect = GetContextDimensions(WindowHandle);

					HandmadeUpdateWindow(BackBuffer, WindowContext, 0, 0, clientWindowRect.width, clientWindowRect.height);

					/*++gradientXoffset;
					++gradientYoffset;*/
					//release device context
					ReleaseDC(WindowHandle, WindowContext);

					//timer calculation and output

					//time
					QueryPerformanceCounter(&currentCounter);
					int64_t counterDiff = currentCounter.QuadPart - prevCounter.QuadPart;
					float milsecsPerFrame = (float)counterDiff * 1000.0f / (float)queryFreq.QuadPart;
					float fps = 1.0f / (milsecsPerFrame / 1000.0f);

					//cycle
					currCycleCounter = (uint64_t)__rdtsc();
					uint64_t cycleDiff = currCycleCounter - prevCycleCounter;
					float iterMCycles = (float)cycleDiff / (1000.0f * 1000.0f);

					//output
					char Buffer[256];
					sprintf_s(Buffer, "%0.2f miliseconds, %0.2f fps, %0.2f mega cycles\n", milsecsPerFrame, fps, iterMCycles);
					OutputDebugStringA(Buffer);

					//update prev timer to be the current
					prevCounter = currentCounter;
					prevCycleCounter = currCycleCounter;

				}
			}
			else
			{
			//TODO LOG mem allocation OS error
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
