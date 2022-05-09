@echo off
rem Copyright (c) Microsoft Corporation.
rem Licensed under the MIT License.

setlocal
set error=0

set FXCOPTS=/nologo /WX /Ges /Zi /Zpc /Qstrip_reflect /Qstrip_debug

set PCFXC="%WindowsSdkVerBinPath%x86\fxc.exe"
if exist %PCFXC% goto continue
set PCFXC="%WindowsSdkBinPath%%WindowsSDKVersion%\x86\fxc.exe"
if exist %PCFXC% goto continue
set PCFXC="%WindowsSdkDir%bin\%WindowsSDKVersion%\x86\fxc.exe"
if exist %PCFXC% goto continue

set PCFXC=fxc.exe

:continue
if not defined CompileShadersOutput set CompileShadersOutput=Compiled
set StrTrim=%CompileShadersOutput%##
set StrTrim=%StrTrim: ##=%
set CompileShadersOutput=%StrTrim:##=%
@if not exist "%CompileShadersOutput%" mkdir "%CompileShadersOutput%"
call :CompileShader BC7Encode TryMode456CS
call :CompileShader BC7Encode TryMode137CS
call :CompileShader BC7Encode TryMode02CS
call :CompileShader BC7Encode EncodeBlockCS

call :CompileShader BC6HEncode TryModeG10CS
call :CompileShader BC6HEncode TryModeLE10CS
call :CompileShader BC6HEncode EncodeBlockCS

echo.

if %error% == 0 (
    echo Shaders compiled ok
) else (
    echo There were shader compilation errors!
    exit /b 1
)

endlocal
exit /b 0

:CompileShader
set fxc=%PCFXC% %1.hlsl %FXCOPTS% /Tcs_4_0 /E%2 "/Fh%CompileShadersOutput%\%1_%2.inc" "/Fd%CompileShadersOutput%\%1_%2.pdb" /Vn%1_%2
echo.
echo %fxc%
%fxc% || set error=1
exit /b
