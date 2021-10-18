@echo off
setlocal

where /q cl || (
  echo ERROR: please run this from MSVC command prompt
  exit /b 1
)

set CFLAGS=/nologo /W3 /Z7 /GS- /Gs999999
set LDFLAGS=/incremental:no /opt:icf /opt:ref

set BASE_FILES=downlist.cpp

rc downlist.rc

call cl -Od -EHsc -Fedownlist_debug_msvc.exe -DDEVELOPMENT=1 %CFLAGS% %BASE_FILES% /link %LDFLAGS% /subsystem:windows downlist.res user32.lib gdi32.lib shell32.lib
call cl -O2 -EHsc -Fedownlist_release_msvc.exe %CFLAGS% %BASE_FILES% /link %LDFLAGS% /subsystem:windows downlist.res user32.lib gdi32.lib shell32.lib
