@echo off

bldr make %* nova nova-pack nova-glsl nova-slang
if %errorlevel% neq 0 exit /b %errorlevel%
out-packer\pack.exe out\example src examples assets assets-external
bldr make %* nova-embeds nova-examples
