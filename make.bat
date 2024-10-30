@echo off

if not exist c:\Users\Chris\git\c\handmade-hero\build mkdir c:\Users\Chris\git\c\handmade-hero\build
pushd c:\Users\Chris\git\c\handmade-hero\build
cl -nologo -std:c17 -Zi -FC ..\code\main.c /Fohandmade User32.lib Gdi32.lib
popd
