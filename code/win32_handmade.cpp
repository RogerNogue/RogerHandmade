/*
  $File: $
  $Date: $
  $Revision: $
  $Creator: Roger Nogue $
 */

#include <Windows.h>

int CALLBACK WinMain(	HINSTANCE hInstance,
			HINSTANCE hPrevInstance,
			LPSTR     lpCmdLine,
			int       nShowCmd)

{
  MessageBox(0, "This is Roger Handmade Hero",
	     "Roger Handmade Hero", MB_OK|MB_ICONINFORMATION);
  
  return(0);
}

