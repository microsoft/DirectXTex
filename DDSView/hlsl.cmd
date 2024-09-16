@echo off
rem Copyright (c) Microsoft Corporation.
rem Licensed under the MIT License.

setlocal
set error=0

if %PROCESSOR_ARCHITECTURE%.==ARM64. (set FXCARCH=arm64) else (if %PROCESSOR_ARCHITECTURE%.==AMD64. (set FXCARCH=x64) else (set FXCARCH=x86))

set FXCOPTS=/nologo /WX /Ges /Qstrip_reflect /Qstrip_debug

if defined LegacyShaderCompiler goto fxcviaenv
set PCFXC="%WindowsSdkVerBinPath%%FXCARCH%\fxc.exe"
if exist %PCFXC% goto continue
set PCFXC="%WindowsSdkBinPath%%WindowsSDKVersion%\%FXCARCH%\fxc.exe"
if exist %PCFXC% goto continue
set PCFXC="%WindowsSdkDir%bin\%WindowsSDKVersion%\%FXCARCH%\fxc.exe"
if exist %PCFXC% goto continue

set PCFXC=fxc.exe
goto continue

:fxcviaenv
set PCFXC="%LegacyShaderCompiler%"
if not exist %PCFXC% goto needfxc
goto continue

:continue
if not defined CompileShadersOutput set CompileShadersOutput=Shaders
set StrTrim=%CompileShadersOutput%##
set StrTrim=%StrTrim: ##=%
set CompileShadersOutput=%StrTrim:##=%
@if not exist "%CompileShadersOutput%" mkdir "%CompileShadersOutput%"

set FXC=%PCFXC% ddsview.hlsl %FXCOPTS% /EVS /Tvs_4_1 /Fh%CompileShadersOutput%\ddsview_vs.inc
echo %FXC%
%FXC%
@if ERRORLEVEL 1 goto error

set FXC=%PCFXC% ddsview.hlsl %FXCOPTS% /EPS_1D /Tps_4_1 /Fh%CompileShadersOutput%\ddsview_ps1D.inc
echo %FXC%
%FXC%
@if ERRORLEVEL 1 goto error

set FXC=%PCFXC% ddsview.hlsl %FXCOPTS% /EPS_1DArray /Tps_4_1 /Fh%CompileShadersOutput%\ddsview_ps1Darray.inc
echo %FXC%
%FXC%
@if ERRORLEVEL 1 goto error

set FXC=%PCFXC% ddsview.hlsl %FXCOPTS% /EPS_2D /Tps_4_1 /Fh%CompileShadersOutput%\ddsview_ps2D.inc
echo %FXC%
%FXC%
@if ERRORLEVEL 1 goto error

set FXC=%PCFXC% ddsview.hlsl %FXCOPTS% /EPS_2DArray /Tps_4_1 /Fh%CompileShadersOutput%\ddsview_ps2Darray.inc
echo %FXC%
%FXC%
@if ERRORLEVEL 1 goto error

set FXC=%PCFXC% ddsview.hlsl %FXCOPTS% /EPS_3D /Tps_4_1 /Fh%CompileShadersOutput%\ddsview_ps3D.inc
echo %FXC%
%FXC%
@if ERRORLEVEL 1 goto error

set FXC=%PCFXC% ddsview.hlsl %FXCOPTS% /EPS_Cube /Tps_4_1 /Fh%CompileShadersOutput%\ddsview_psCube.inc
echo %FXC%
%FXC%
@if ERRORLEVEL 1 goto error

@echo --- Shaders built ok ---
endlocal
exit /b 0

:error
@echo --- ERROR: Shader build failed ---
exit /b 1

:needfxc
echo ERROR: CompileShaders requires FXC.EXE
exit /b 1
:end
