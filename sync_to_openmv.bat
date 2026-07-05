@echo off
set SRC=D:\A_Project\Smart_glasses\openmv
set DST=E:\

echo === 去 BOM + 烧录 OpenMV ===

rem 先用 PowerShell 去掉 BOM（防 MicroPython 报 NameError）
powershell -Command "Get-ChildItem '%SRC%\*.py' | ForEach-Object { $b = [System.IO.File]::ReadAllBytes($_.FullName); if ($b.Length -ge 3 -and $b[0] -eq 0xEF -and $b[1] -eq 0xBB -and $b[2] -eq 0xBF) { [System.IO.File]::WriteAllBytes($_.FullName, $b[3..($b.Length-1)]); Write-Host ('BOM removed: ' + $_.Name) } }"

rem 删除垃圾目录
for %%d in (dataset downstairs nottactile stairs tactile __pycache__ FOUND.000) do (
    if exist "%DST%%%d" rd /s /q "%DST%%%d" 2>nul
)

rem 同步文件
for %%f in (battery.py config.py decision.py fusion.py glasses.py interaction.py main.py sensors.py stats.py temporal_filter.py vision.py trained.tflite) do (
    copy /y "%SRC%\%%f" "%DST%%%f" >nul && echo   [OK] %%f || echo   [FAIL] %%f
)

echo === 完成，可弹出 SD 卡 ===
pause
