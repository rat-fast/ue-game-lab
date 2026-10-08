@echo off
setlocal
cd /d "E:\agent\game\ue\SkillLab"
"D:\Epic\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" "E:\agent\game\ue\SkillLab\SkillLab.uproject" -game -windowed -ResX=1280 -ResY=720 -d3d11 -log
set "exitcode=%errorlevel%"
echo.
echo UE exited with code %exitcode%.
pause
endlocal & exit /b %exitcode%
