objcopy  -R.userpage -Obinary %1.elf %1.bin
..\AVR32_FirmwarePackager_1.0.exe -B0x4000 %1.bin %1.sfw

set dt=%date:~10,4%-%date:~4,2%-%date:~7,2%-%time:~0,2%-%time:~3,2%-%time:~6,2%
copy %1.elf %1_%dt%.elf
copy %1.hex %1_%dt%.hex
copy %1.swf %1_%dt%.swf
