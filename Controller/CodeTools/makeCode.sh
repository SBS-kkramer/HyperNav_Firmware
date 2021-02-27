#!/bin/sh

#
#	For the automatic generation of config code to work:
#	Edit/Save Firmware_Configuration_Parameters.ods
#	Save Firmware_Configuration_Parameters.ods
#	as a csv file: Firmware_Configuration_Parameters.csv
#
#	(1) Save as... [Choose Save as type: TEXT CSV (.csv)]
#	(2) Confirm that you want to replace the exisitng csv file.
#	(3) Confirm that you want to [Keep Current Format].
#	(4) At "Export of Text File" dialog:
#		Character Set = Western European
#		Field Delimiter = {Tab}
#		Text Delimiter = "
#		[x] Save cell content as shown
#		[ ] Fixed column width
#
#	Then, run this script
#	

EFILE=e.h
rm -f $EFILE
touch $EFILE

HFILE=c.h
H_PRE=${HFILE}.pre
H_TYP=${HFILE}.typ
H_DEF=${HFILE}.def
H_POS=${HFILE}.pos

rm -f $H_TYP
touch $H_TYP
rm -f $H_DEF
touch $H_DEF

CFILE=c.c
C_PRE=${CFILE}.pre
C_TYP=${CFILE}.typ
C_SAV=${CFILE}.sav
C_GEN=${CFILE}.gen
C_DEF=${CFILE}.def
C_COD=${CFILE}.code
C_GET=${CFILE}.get
C_SET=${CFILE}.set
C_PRN=${CFILE}.print
C_OOR=${CFILE}.oor
C_POS=${CFILE}.post

rm -f $C_TYP
touch $C_TYP
rm -f $C_SAV
touch $C_SAV
rm -f $C_GEN
touch $C_GEN
rm -f $C_DEF
touch $C_DEF
rm -f $C_COD
touch $C_COD
rm -f $C_GET
touch $C_GET
rm -f $C_SET
touch $C_SET
rm -f $C_PRN
touch $C_PRN
rm -f $C_OOR
touch $C_OOR

sed 's/"//g' Firmware_Configuration_Parameters.csv | awk -F '\t' \
-v H_CEC=$EFILE \
-v H_DEF=$H_DEF \
-v H_TYP=$H_TYP \
-v C_TYP=$C_TYP \
-v C_GEN=$C_GEN \
-v C_DEF=$C_DEF \
-v C_SAV=$C_SAV \
-v C_COD=$C_COD \
-v C_GET=$C_GET \
-v C_SET=$C_SET \
-v C_PRN=$C_PRN \
-v C_OOR=$C_OOR \
-f buildCode.awk -

cat $H_PRE $H_TYP $H_DEF $H_POS                                           > $HFILE
cat $C_PRE $C_TYP $C_SAV $C_GEN $C_DEF $C_COD $C_GET $C_SET $C_PRN $C_OOR $C_POS > $CFILE

cp $HFILE ../Source/HyperNAV_Controller/src/config.controller.h
cp $CFILE ../Source/HyperNAV_Controller/src/config.controller.c
cp $EFILE ../Source/HyperNAV_Controller/src/config.errorcodes.controller.h

rm -f $EFILE

rm -f $HFILE
rm -f $H_TYP
rm -f $H_DEF

rm -f $CFILE
rm -f $C_TYP
rm -f $C_SAV
rm -f $C_GEN
rm -f $C_DEF
rm -f $C_COD
rm -f $C_GET
rm -f $C_SET
rm -f $C_PRN
rm -f $C_OOR

