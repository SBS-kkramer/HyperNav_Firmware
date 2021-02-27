#!/bin/sh

gcc \
     -DFW_SIMULATION \
     -DBAUDRATE=57600 \
     -o Profile_Manager \
     -I ../Spectrometer/Source/HyperNAV_Spectrometer/src/avr32rlib/Utils/zlib-1.2.8 \
     -I ../Shared/FirmwareDefinitions \
     -I ../Controller/Source/HyperNAV_Controller/src/avr32rlib/Config/ \
     -I ../Controller/Source/HyperNAV_Controller/src/ \
     -I ../Spectrometer/Source/HyperNAV_Spectrometer/src/ \
     -I ../Controller/Source/HyperNAV_Controller/src/avr32rlib/Utils/Syslog/ \
     ../Controller/Source/HyperNAV_Controller/src/profile_packet.controller.c \
     ../Spectrometer/Source/HyperNAV_Spectrometer/src/profile_packet.spectrometer.c \
     ../Controller/Source/HyperNAV_Controller/src/avr32rlib/Utils/Syslog/syslog.c \
     ../Spectrometer/Source/HyperNAV_Spectrometer/src/avr32rlib/Utils/zlib-1.2.8/crc32.c \
     ../Spectrometer/Source/HyperNAV_Spectrometer/src/avr32rlib/Utils/zlib-1.2.8/adler32.c \
     ../Spectrometer/Source/HyperNAV_Spectrometer/src/avr32rlib/Utils/zlib-1.2.8/deflate.c \
     ../Spectrometer/Source/HyperNAV_Spectrometer/src/avr32rlib/Utils/zlib-1.2.8/inflate.c \
     ../Spectrometer/Source/HyperNAV_Spectrometer/src/avr32rlib/Utils/zlib-1.2.8/trees.c \
     ../Spectrometer/Source/HyperNAV_Spectrometer/src/avr32rlib/Utils/zlib-1.2.8/inftrees.c \
     ../Spectrometer/Source/HyperNAV_Spectrometer/src/avr32rlib/Utils/zlib-1.2.8/inffast.c \
     ../Spectrometer/Source/HyperNAV_Spectrometer/src/avr32rlib/Utils/zlib-1.2.8/zutil.c \
     profile_transmit.controller.c \
     ../Spectrometer/Source/HyperNAV_Spectrometer/src/profile_transmit.spectrometer.c \
     code_verify.c \
     profile_acquire.c \
     profile_description.c \
     Profile_Manager.c \
     profile_receive.c \
     profiles_list.c \
     sensor_data.c

#    profile_transmit.c \
#    ../Spectrometer/Source/HyperNAV_Spectrometer/src/avr32rlib/Utils/zlib-1.2.8/*.c \
#    profile_packet.c \
