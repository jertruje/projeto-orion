@echo off
setlocal enabledelayedexpansion

set "inputFile=C:\Users\danim\Downloads\projeto orion\Makefile"
set "tempFile=%inputFile%.tmp"

> "%tempFile%" (
    for /f "usebackq delims=" %%a in ("%inputFile%") do (
        echo %%a
        if "%%a"=="KERNEL_OBJECTS += /user-mode.o /user-space.o \" (
            echo   /power/acpi.o ^
        )
    )
)

move /y "%tempFile%" "%inputFile%" >nul
endlocal
