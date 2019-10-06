@REM this line turns off prints. The @ makes this own line not being printed too
@echo off

mkdir C:\Users\Roger\Desktop\Roger\handmade\buildStuff
REM popd removes pushd makings
pushd C:\Users\Roger\Desktop\Roger\handmade\buildStuff
cl /Zi C:\Users\Roger\Desktop\Roger\handmade\RogerHandmade\code\win32_handmade.cpp user32.lib gdi32.lib
REM cl /Zi ..\code\win32_handmade.cpp
popd
