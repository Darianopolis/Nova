@echo off

bldr make %* nova nova-pack
if %errorlevel% neq 0 exit /b %errorlevel%
out\pack.exe src examples assets
bldr make %* nova-pack-output nova-examples