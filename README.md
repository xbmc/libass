libass Windows binaries for XBMC

Tools required:
* Visual Studio 2013

To reproduce:

1. Create harfbuzz-0.9.22.7z
  * git clone git://github.com/blinkseb/harfbuzz.git
  * Open win32/harfbuzz.sln with Visual Studio.
  * Build Release configuration
  * Run xbmc\package.bat to produce the final archive
2. git clone git://github.com/xbmc/libass.git
3. Call DownloadLibass.bat in folder win32\deps to download source code of libass from https://github.com/libass/libass
4. Copy harfbuzz-0.9.22.7z into folder win32\deps\downloads
5. Call DownloadBuildDeps.bat in folder win32\deps to download and extract following dependecies: fontconfig, freetype, harfbuzz, libfribidi, libiconv
6. Grab libenca dependecies from an XBMC build (See http://wiki.xbmc.org/index.php?title=HOW-TO:Compile_XBMC_for_Windows_using_Git):
  * copy XBMC file project\VS2010Express\libs\libenca\Release\libenca.lib into win32\deps\lib
  * copy XBMC file lib\enca\lib\enca.h into win32\deps\include
7. Apply patches in the folder patches
8. Open .sln file in folder win32\VS2013 and compile as Debug and Release
9. Run package.bat in folder win32\package to produce the final archive
