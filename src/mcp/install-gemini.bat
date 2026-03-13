@echo off
echo Installing 6502 Simulator Plugin for Gemini CLI...

set GEMINI_PLUGIN_DIR=%USERPROFILE%\.gemini\plugins
set PLUGIN_NAME=6502-simulator
set PLUGIN_DIR=%GEMINI_PLUGIN_DIR%\%PLUGIN_NAME%

if not exist "%GEMINI_PLUGIN_DIR%" (
    mkdir "%GEMINI_PLUGIN_DIR%"
)

if exist "%PLUGIN_DIR%" (
    echo Plugin already installed. Overwriting...
    rmdir /s /q "%PLUGIN_DIR%"
)

mkdir "%PLUGIN_DIR%"

xcopy /s /e /i . "%PLUGIN_DIR%"

echo Installation complete.
echo To use the MCP server, add the following to your gemini config file (%%USERPROFILE%%\.gemini\settings.json):
echo.

echo   "mcpServers": {
echo     "6502-simulator": {
echo       "command": "node",
echo       "args": ["%PLUGIN_DIR%\index.js"]
echo     }
echo   }
echo.