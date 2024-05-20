@echo off

@REM bldr make nova nova-pack
out\pack.exe src examples
bldr make nova nova-pack-output nova-examples