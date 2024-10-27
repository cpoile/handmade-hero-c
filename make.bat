@echo off

mkdir c:\Users\Chris\git\c\handmade-hero\build
pushd c:\Users\Chris\git\c\handmade-hero\build
cl -Zi -FC ..\code\main.c /Fohandmade User32.lib Gdi32.lib
popd
