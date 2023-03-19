@echo off
rem Copyright (c) Microsoft Corporation.
rem Licensed under the MIT License.

setlocal
set error=0

set FXCOPTS=/nologo /WX /Ges /Qstrip_reflect /Qstrip_debug

set PCFXC="%WindowsSdkVerBinPath%x86\fxc.exe"
if exist %PCFXC% goto continue
set PCFXC="%WindowsSdkBinPath%%WindowsSDKVersion%\x86\fxc.exe"
if exist %PCFXC% goto continue
set PCFXC="%WindowsSdkDir%bin\%WindowsSDKVersion%\x86\fxc.exe"
if exist %PCFXC% goto continue

set PCFXC=fxc.exe

:continue
if not defined CompileShadersOutput set CompileShadersOutput=Shaders
set StrTrim=%CompileShadersOutput%##
set StrTrim=%StrTrim: ##=%
set CompileShadersOutput=%StrTrim:##=%
@if not exist "%CompileShadersOutput%" mkdir "%CompileShadersOutput%"


set FXC=%PCFXC% ddsview.fx %FXCOPTS% /EVS /Tvs_4_1 /Fh%CompileShadersOutput%\ddsview_vs.inc
echo %FXC%
%FXC%
@if ERRORLEVEL 1 goto error

set FXC=%PCFXC% ddsview.fx %FXCOPTS% /EPS_1D /Tps_4_1 /Fh%CompileShadersOutput%\ddsview_ps1D.inc
echo %FXC%
%FXC%
@if ERRORLEVEL 1 goto error

set FXC=%PCFXC% ddsview.fx %FXCOPTS% /EPS_1DArray /Tps_4_1 /Fh%CompileShadersOutput%\ddsview_ps1Darray.inc
echo %FXC%
%FXC%
@if ERRORLEVEL 1 goto error

set FXC=%PCFXC% ddsview.fx %FXCOPTS% /EPS_2D /Tps_4_1 /Fh%CompileShadersOutput%\ddsview_ps2D.inc
echo %FXC%
%FXC%
@if ERRORLEVEL 1 goto error

set FXC=%PCFXC% ddsview.fx %FXCOPTS% /EPS_2DArray /Tps_4_1 /Fh%CompileShadersOutput%\ddsview_ps2Darray.inc
echo %FXC%
%FXC%
@if ERRORLEVEL 1 goto error

set FXC=%PCFXC% ddsview.fx %FXCOPTS% /EPS_3D /Tps_4_1 /Fh%CompileShadersOutput%\ddsview_ps3D.inc
echo %FXC%
%FXC%
@if ERRORLEVEL 1 goto error

set FXC=%PCFXC% ddsview.fx %FXCOPTS% /EPS_Cube /Tps_4_1 /Fh%CompileShadersOutput%\ddsview_psCube.inc
echo %FXC%
%FXC%
@if ERRORLEVEL 1 goto error

@echo --- Shaders built ok ---
@goto end
:error
@echo --- ERROR: Shader build failed ---
exit /b 1
:end