$ErrorActionPreference = "Stop"

$vendorFolder = "vendor"
$SDLLib = "vendor\SDL2"

if (-not(Test-Path -Path $vendorFolder)) {
     try {
         $null = New-Item -ItemType Directory -Path $vendorFolder -Force -ErrorAction Stop
         Write-Host "[$vendorFolder] folder created has been created."
     }
     catch {
         throw $_.Exception.Message
     }
 }

if (-not(Test-Path -Path $SDLLib)) {
	if(-not(Test-Path -Path (Join-Path $vendorFolder SDL2-dev.zip)))
	{
		Write-Host "downloading SDL libraries ..."
		iwr -outf vendor\SDL2-dev.zip https://www.libsdl.org/release/SDL2-devel-2.0.16-VC.zip
		Write-Host "successfully downloaded SDL libraries"
	}

	if(-not(Test-Path -Path (Join-Path $vendorFolder SDL2-2.0.16)))
	{
		Write-Host "extracting downloaded SDL libraries ..."
		Expand-Archive -Path vendor\SDL2-dev.zip -DestinationPath vendor
		Write-Host "successfully extracted SDL libraries"
	}
	
	Write-Host "renaming extrated archive..."
	Rename-Item -Path vendor\SDL2-2.0.16 -NewName SDL2
	Write-Host "Cleaning download artifacts..."
	Remove-Item -Path vendor\SDL2-dev.zip -Force -ErrorAction Continue
}

if (-not(Test-Path -Path (Join-Path $SDLLib SDL2))) {
	Rename-Item -Path vendor\SDL2\include -NewName SDL2
	Write-Host "renaming include directries ..."
}

if (-not(Test-Path -Path (Join-Path $SDLLib sdl2-config.cmake))) {
	Write-Host "Copying cmake files..."
	Copy-Item sdl2-config -Destination vendor\SDL2\sdl2-config.cmake
}

Write-Host "MSVC setup complete"
Write-Host "Press any key to exit"
$null = $Host.UI.RawUI.ReadKey('NoEcho,IncludeKeyDown');
