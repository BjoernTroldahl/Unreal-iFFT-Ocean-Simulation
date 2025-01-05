del *.sln
del *.vsconfig
rmdir /s /q .vs
rmdir /s /q Binaries
rmdir /s /q Intermediate
rmdir /s /q Saved
rmdir /s /q DerivedDataCache


set MyUVS="C:\Program Files (x86)\Epic Games\Launcher\Engine\Binaries\Win64\UnrealVersionSelector.exe"
set MyUBT="f:\Program Files\Epic Games\UE_5.4\Engine\Binaries\DotNET\UnrealBuildTool.exe"

rem change OceanTutorial to your own project name
set MyFullPath="%cd%\OceanTutorial"


%MyUVS% /projectfiles %MyFullPath%.uproject

%MyFullPath%.sln