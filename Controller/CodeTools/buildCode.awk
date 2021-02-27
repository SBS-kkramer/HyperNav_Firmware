BEGIN {
	declaredFT = 0;
	Catg = "Category";

	allowUniqueFirstLetters = 0;
	useEnumAsEnum = 1;

	printf "typedef struct {\r\n"			>> C_TYP
	printf "\tU8\tversion;\r\n"				>> C_TYP

	printf "S16 CFG_VarSaveToUPG ( char* m, U16 location, U8* src, U8 size ) {\r\n"				>> C_SAV
	printf "\t//\tUser page has 512 (=0x200) bytes. Restrict write to that region.\r\n"			>> C_SAV
	printf "\tif ( location >= 0x200 || location+size > 0x200 ) return CFG_FAIL;\r\n"			>> C_SAV
	printf "\tflashc_memcpy ( (void*)(AVR32_FLASHC_USER_PAGE_ADDRESS+location), (void*)src, (size_t)size, true );\r\n"		>> C_SAV
	printf "\treturn CFG_OK;\r\n"																>> C_SAV
	printf "}\r\n\r\n"																			>> C_SAV

	printf "S16 CFG_VarSaveToRTC ( char* m, U16 location, U8* src, U8 size ) {\r\n"				>> C_SAV
	printf "\t//\tRTC has 256 (=0x100) bytes. Restrict write to that region.\r\n"				>> C_SAV
	printf "\tif ( location >= 0x100 || location+size > 0x100 ) return -1;\r\n"					>> C_SAV
	printf "\treturn BBV_OK == bbv_write (location, src, size ) ? CFG_OK : CFG_FAIL;\r\n"		>> C_SAV
	printf "}\r\n\r\n"																			>> C_SAV

	printf "static S16 cfg_VarSaveToBackup ( char* m, U16 location, U8* src, U8 size ) {\r\n"	>> C_SAV
	printf "\t//\tBackup has 512 (=0x200) bytes. Restrict write to that region.\r\n"			>> C_SAV
	printf "\tif ( location >= 0x200 || location+size > 0x200 ) return -1;\r\n"					>> C_SAV
	printf "\tmemcpy ( m+location, src, size );\r\n"											>> C_SAV
	printf "\treturn CFG_OK;\r\n"																>> C_SAV
	printf "}\r\n\r\n"																			>> C_SAV

	printf "S16 CFG_Save ( bool useBackup ) {\r\n"												>> C_SAV
	printf "\r\n"																				>> C_SAV
	printf "\tS16 rv = CFG_OK;\r\n"																>> C_SAV
	printf "\r\n"																				>> C_SAV
	printf "\tU32 mVar;\r\n"																	>> C_SAV
	if ( useEnumAsEnum ) {
		printf "\tU8 enum_asU8;\r\n"															>> C_SAV
	}
	printf "\r\n"																				>> C_SAV
	printf "\tS16 (*saveToU)( char*, U16, U8*, U8 );\r\n"										>> C_SAV
	printf "\tS16 (*saveToR)( char*, U16, U8*, U8 );\r\n"										>> C_SAV
	printf "\tchar* backupMem = 0;\r\n"															>> C_SAV
	printf "\r\n"																				>> C_SAV
	printf "\tif ( useBackup ) {\r\n"															>> C_SAV
	printf "\t\tif ( 0 == (backupMem = pvPortMalloc( CFG_BACK_SIZE ) ) ) {\r\n"					>> C_SAV
	printf "\t\t\treturn CFG_FAIL;\r\n"															>> C_SAV
	printf "\t\t}\r\n"																			>> C_SAV
	printf "\t\tsaveToU = cfg_VarSaveToBackup;\r\n"												>> C_SAV
	printf "\t\tsaveToR = cfg_VarSaveToBackup;\r\n"												>> C_SAV
	printf "\t} else {\r\n"																		>> C_SAV
	printf "\t\tsaveToU = CFG_VarSaveToUPG;\r\n"												>> C_SAV
	printf "\t\tsaveToR = CFG_VarSaveToRTC;\r\n"												>> C_SAV
	printf "\t\t\r\n"																			>> C_SAV
	printf "\t}\r\n"																			>> C_SAV
	printf "\r\n"																				>> C_SAV

	printf "static S16 cfg_VarGetFrmUPG ( char* m, U16 location, U8* dest, U8 size ) {\r\n"		>> C_GEN
	printf "\t//\tUser page has 512 (=0x200) bytes. Restrict read to that region.\r\n"			>> C_GEN
	printf "\tif ( location >= 0x200 || location+size > 0x200 ) return -1;\r\n"					>> C_GEN
	printf "\tmemcpy ( (void*)dest, (void*)(AVR32_FLASHC_USER_PAGE_ADDRESS+location), (size_t)size );\r\n"		>> C_GEN
	printf "\treturn CFG_OK;\r\n"																>> C_GEN
	printf "}\r\n\r\n"																			>> C_GEN

	printf "static S16 cfg_VarGetFrmRTC ( char* m, U16 location, U8* dest, U8 size ) {\r\n"		>> C_GEN
	printf "\t//\tRTC has 256 (=0x100) bytes. Restrict read to that region.\r\n"				>> C_GEN
	printf "\tif ( location >= 0x100 || location+size > 0x100 ) return -1;\r\n"					>> C_GEN
	printf "\treturn BBV_OK == bbv_read ( location, dest, size ) ? CFG_OK : CFG_FAIL;\r\n"		>> C_GEN
	printf "}\r\n\r\n"																			>> C_GEN

	printf "static S16 cfg_VarGetFrmBackup ( char* m, U16 location, U8* dest, U8 size ) {\r\n"	>> C_GEN
	printf "\t//\tBackup has 512 (=0x200) bytes. Restrict read to that region.\r\n"				>> C_GEN
	printf "\tif ( location >= 0x200 || location+size > 0x200 ) return -1;\r\n"					>> C_GEN
	printf "\tmemcpy ( dest, m+location, size );\r\n"											>> C_GEN
	printf "\treturn CFG_OK;\r\n"																>> C_GEN
	printf "}\r\n\r\n"																			>> C_GEN

	printf "S16 CFG_Retrieve ( bool useBackup ) {\r\n"											>> C_GEN
	printf "\r\n"																				>> C_GEN
	printf "\tS16 rv = CFG_OK;\r\n"																>> C_GEN
	printf "\r\n"																				>> C_GEN
	printf "\tU32 mVar;\r\n"																	>> C_GEN
	if ( useEnumAsEnum ) {
		printf "\tU8 enum_asU8;\r\n"															>> C_GEN
	}
	printf "\r\n"																				>> C_GEN
	printf "\tS16 (*getFrmU)( char*, U16, U8*, U8 );\r\n"										>> C_GEN
	printf "\tS16 (*getFrmR)( char*, U16, U8*, U8 );\r\n"										>> C_GEN
	printf "\tchar* backupMem = 0;\r\n"															>> C_GEN
	printf "\r\n"																				>> C_GEN
	printf "\tif ( useBackup ) {\r\n"															>> C_GEN
	printf "\t\tif ( 0 == ( backupMem = pvPortMalloc( CFG_BACK_SIZE ) ) ) {\r\n"				>> C_GEN
	printf "\t\t\treturn CFG_FAIL;\r\n"															>> C_GEN
	printf "\t\t}\r\n"																			>> C_GEN
	printf "\t\tif ( CFG_FAIL == cfg_ReadFromBackup( backupMem ) ) {\r\n"						>> C_GEN
	printf "\t\t\tvPortFree ( backupMem );\r\n"													>> C_GEN
	printf "\t\t\treturn CFG_FAIL;\r\n"															>> C_GEN
	printf "\t\t}\r\n"																			>> C_GEN
	printf "\t\tgetFrmU = cfg_VarGetFrmBackup;\r\n"												>> C_GEN
	printf "\t\tgetFrmR = cfg_VarGetFrmBackup;\r\n"												>> C_GEN
	printf "\t} else {\r\n"																		>> C_GEN
	printf "\t\tgetFrmU = cfg_VarGetFrmUPG;\r\n"												>> C_GEN
	printf "\t\tgetFrmR = cfg_VarGetFrmRTC;\r\n"												>> C_GEN
	printf "\t\t\r\n"																			>> C_GEN
	printf "\t}\r\n"																			>> C_GEN
	printf "\r\n"																				>> C_GEN

	printf "S16 CFG_CmdGet ( char* option, char* result, S16 r_max_len );\r\n"				>> H_TYP
	printf "S16 CFG_CmdSet ( char* option, char* value, Access_Mode_t access_mode );\r\n"	>> H_TYP

	printf "S16 CFG_CmdGet ( char* option, char* result, S16 r_max_len ) {\r\n"				>> C_GET
	printf "\tS16 const r_max_len_m1 = r_max_len-1;\r\n"									>> C_GET

	printf "S16 CFG_CmdSet ( char* option, char* value, Access_Mode_t access_mode ) {\r\n"	>> C_SET

	printf "\r\n"																			>> C_SET
	printf "\tif ( 0 == option || 0 == *option ) return CEC_Failed;\r\n"					>> C_SET
	printf "\tif ( 0 == value  || 0 == *value  ) return CEC_Failed;\r\n"					>> C_SET
	printf "\r\n"																			>> C_SET
	printf "\tS16 cec = CEC_Ok;\r\n"														>> C_SET
	printf "\r\n"																			>> C_SET

	printf "\r\n"																			>> C_GET
	printf "\tS16 cec = CEC_Ok;\r\n"														>> C_GET
	printf "\r\n"																			>> C_GET
	printf "\tif ( !result || r_max_len < 16 ) {\r\n"										>> C_GET
	printf "\t\treturn CEC_Failed;\r\n"														>> C_GET
	printf "\t}\r\n"																		>> C_GET
	printf "\r\n"																			>> C_GET
	printf "\tresult[0] = 0;\r\n"															>> C_GET
	printf "\r\n"																			>> C_GET
	cmd_ee="";

	printf "void CFG_Print ();\r\n"								>> H_TYP

	printf "void CFG_Print () {\r\n"							>> C_PRN
	printf "\r\n"												>> C_PRN
	printf "\tio_out_string ( \"\\r\\n\" );\r\n"				>> C_PRN
	printf "\r\n"												>> C_PRN

	printf "S16 CFG_OutOfRangeCounter ( bool correctOOR, bool setRTCToDefault );\r\n"		>> H_TYP

	printf "S16 CFG_OutOfRangeCounter ( bool correctOOR, bool setRTCToDefault ) {\r\n"		>> C_OOR
	printf "\r\n"																			>> C_OOR
	printf "\tS16 cnt = 0;\r\n"																>> C_OOR
	printf "\r\n"																			>> C_OOR


	BaseCode=4500
	StartCode=BaseCode+100
	printf "//\r\n"																	>> H_CEC
	printf "//\tError Codes for Configuration Set Commands\r\n"						>> H_CEC
	printf "//\tCode values are in [%d:%d]\r\n", StartCode, StartCode+399			>> H_CEC
	printf "//\r\n"																	>> H_CEC
	printf "# define CEC_ConfigVariableUnknown	%d\r\n", BaseCode+1					>> H_CEC
	printf "//\r\n"																	>> H_CEC
	printf "\r\n"																	>> H_CEC

	RTC_Size = 256;

	printf "void wipe_RTC( U8 value );\r\n"											>> H_TYP

	printf "void wipe_RTC( U8 value ) {\r\n"										>> C_COD
	printf "\r\n"																	>> C_COD
	printf "\tint i;\r\n"															>> C_COD
	printf "\r\n"																	>> C_COD
	printf "\tfor ( i=0; i<%d; i++ ) {\r\n", RTC_Size								>> C_COD
	printf "\t\tbbv_write( i, &value, 1 );\r\n"										>> C_COD
	printf "\t}\r\n"																>> C_COD
	printf "}\r\n"																	>> C_COD
	printf "\r\n"																	>> C_COD

	printf "void dump_RTC();\r\n"													>> H_TYP

	printf "void dump_RTC() {\r\n"													>> C_COD
	printf "\r\n"																	>> C_COD
	printf "\tU32 rtcVal;\r\n"														>> C_COD
	printf "\tint i;\r\n"															>> C_COD
	printf "\r\n"																	>> C_COD
	printf "\tio_out_string(\"RTC Dump:\\r\\n\");\r\n"								>> C_COD
	printf "\r\n"																	>> C_COD
	printf "\tfor ( i=0; i<%d; i+=4 ) {\r\n", RTC_Size								>> C_COD
	printf "\t\tbbv_read( i, (U8*)&rtcVal, 4 );\r\n"								>> C_COD
	printf "\t\tif ( 0 == i%32 ) io_out_string ( \"\\r\\n\" );\r\n"					>> C_COD
	printf "\t\tif ( 0 == i%4  ) io_out_string ( \" \" );\r\n"						>> C_COD
	printf "\t\tio_dump_X32 ( rtcVal, 0 );\r\n"										>> C_COD
	printf "\t}\r\n"																>> C_COD
	printf "\r\n"																	>> C_COD
	printf "\tio_out_string(\"\\r\\n\\r\\n\");\r\n"									>> C_COD
	printf "}\r\n"																	>> C_COD
	printf "\r\n"																	>> C_COD

	UPG_Offset = 64;

	printf "void wipe_UPG( U8 value );\r\n"											>> H_TYP

	printf "void wipe_UPG( U8 value ) {\r\n"										>> C_COD
	printf "\r\n"																	>> C_COD
	printf "\tchar mem[AVR32_FLASHC_USER_PAGE_SIZE];\r\n"							>> C_COD
	printf "\r\n"																	>> C_COD
	printf "\tint i;\r\n"															>> C_COD
	printf "\r\n"																	>> C_COD
	printf "\tfor ( i=%d; i<AVR32_FLASHC_USER_PAGE_SIZE; i++ ) mem[i] = value;\r\n", UPG_Offset		>> C_COD
	printf "\r\n"																	>> C_COD
	printf "\tflashc_memcpy( (void*)AVR32_FLASHC_USER_PAGE_ADDRESS+%d, (void*)mem+%d, AVR32_FLASHC_USER_PAGE_SIZE-%d, true );\r\n", UPG_Offset, UPG_Offset, UPG_Offset	>> C_COD
	printf "}\r\n"																	>> C_COD
	printf "\r\n"																	>> C_COD

	printf "void dump_UPG();\r\n"													>> H_TYP

	printf "void dump_UPG() {\r\n"													>> C_COD
	printf "\r\n"																	>> C_COD
	printf "\tchar mem[AVR32_FLASHC_USER_PAGE_SIZE];\r\n"							>> C_COD
	printf "\r\n"																	>> C_COD
	printf "\tint i;\r\n"															>> C_COD
	printf "\r\n"																	>> C_COD
	printf "\tfor ( i=0; i<AVR32_FLASHC_USER_PAGE_SIZE; i++ ) mem[i] = 0;\r\n"		>> C_COD
	printf "\r\n"																	>> C_COD
	printf "\tmemcpy( mem, (void*)AVR32_FLASHC_USER_PAGE_ADDRESS, AVR32_FLASHC_USER_PAGE_SIZE );\r\n"	>> C_COD
	printf "\r\n"																	>> C_COD
	printf "\tio_out_string(\"UPG Dump:\\r\\n\");\r\n"								>> C_COD
	printf "\r\n"																	>> C_COD
	printf "\tfor ( i=0; i<AVR32_FLASHC_USER_PAGE_SIZE; i++ ) {\r\n"				>> C_COD
	printf "\t\tif ( 0 == i%32 ) io_out_string ( \"\\r\\n\" );\r\n"					>> C_COD
	printf "\t\tif ( 0 == i%4  ) io_out_string ( \" \" );\r\n"						>> C_COD
	printf "\t\tio_dump_X8 ( mem[i], 0 );\r\n"										>> C_COD
	printf "\t}\r\n"																>> C_COD
	printf "\r\n"																	>> C_COD
	printf "\tio_out_string(\"\\r\\n\\r\\n\");\r\n"									>> C_COD
	printf "\r\n"																	>> C_COD
	printf "}\r\n"																	>> C_COD
	printf "\r\n"																	>> C_COD
}
{
	if ( 1 == NR ) {

		for ( i=1; i<=NF; i++ ) {
			if ( $i == "Category" ) {
				idxCatg = i;
			} else if ( $i == "Parameter" ) {
				idxName = i;
			} else if ( $i == "Units" ) {
				idxUnit = i;
			} else if ( $i == "Possible" ) {
				idxValu = i;
			} else if ( $i == "Default" ) {
				idxDefl = i;
			} else if ( $i == "D-Type" ) {
				idxDTyp = i;
			} else if ( $i == "Format" ) {
				idxFrmt = i;
			} else if ( $i == "#bytes" ) {
				idxSize = i;
			} else if ( $i == "MemPtr" ) {
				idxPntr = i;
			} else if ( $i == "Short" ) {
				idxShrt = i;
			} else if ( $i == "Modify" ) {
				idxModi = i;
			} else if ( $i == "Access" ) {
				idxAccs = i;
			} else if ( $i == "Setup" ) {
				idxStup = i;
			} else if ( $i == "Store" ) {
				idxStor = i;
			} else if ( $i == "Used" ) {
				idxUsed = i;
			} else if ( $i == "Comment" ) {
				idxComm = i;
			}
		}

		if ( 0 == idxCatg \
		  || 0 == idxName \
		  || 0 == idxUnit \
		  || 0 == idxValu \
		  || 0 == idxDefl \
		  || 0 == idxDTyp \
		  || 0 == idxFrmt \
		  || 0 == idxSize \
		  || 0 == idxPntr \
		  || 0 == idxShrt \
		  || 0 == idxModi \
		  || 0 == idxAccs \
		  || 0 == idxStup \
		  || 0 == idxStor \
		  || 0 == idxUsed \
		  || 0 == idxComm ) {

			printf "Catg at %d\n", idxCatg;
			printf "Name at %d\n", idxName;
			printf "Unit at %d\n", idxUnit;
			printf "Valu at %d\n", idxValu;
			printf "Defl at %d\n", idxDefl;
			printf "DTyp at %d\n", idxDTyp;
			printf "Frmt at %d\n", idxFrmt;
			printf "Size at %d\n", idxSize;
			printf "Pntr at %d\n", idxPntr;
			printf "Shrt at %d\n", idxShrt;
			printf "Modi at %d\n", idxModi;
			printf "Accs at %d\n", idxAccs;
			printf "Stup at %d\n", idxStup;
			printf "Stor at %d\n", idxStor;
			printf "Used at %d\n", idxUsed;
			printf "Comm at %d\n", idxComm;

			printf "Error: Missing header.\n"
			exit 1
		} else {
			#	printf "Catg at %d\n", idxCatg;
			#	printf "Name at %d\n", idxName;
			#	printf "Unit at %d\n", idxUnit;
			#	printf "Valu at %d\n", idxValu;
			#	printf "Defl at %d\n", idxDefl;
			#	printf "DTyp at %d\n", idxDTyp;
			#	printf "Size at %d\n", idxSize;
			#	printf "Pntr at %d\n", idxPntr;
			#	printf "Shrt at %d\n", idxShrt;
			#	printf "Usag at %d\n", idxModi;
			#	printf "Usag at %d\n", idxUsed;
			#	printf "Comm at %d\n", idxComm;
			#
			#	printf "Header ok.\n"
		}

	} else if ( NF > 6 && $1 != "" && $2 != "" && "#" != substr($1,1,1) ) {

	  #	for ( i=1; i<=NF; i++ ) {
	  #		printf "  %2d\t%s\n", i, $i;
	  #	}

	  #	printf "Catg at %d is %s\n", idxCatg, $idxCatg;
	  #	printf "Name at %d is %s\n", idxName, $idxName;
	  #	printf "Unit at %d is %s\n", idxUnit, $idxUnit;
	  #	printf "Valu at %d is %s\n", idxValu, $idxValu;
	  #	printf "Defl at %d is %s\n", idxDefl, $idxDefl;
	  #	printf "DTyp at %d is %s\n", idxDTyp, $idxDTyp;
	  #	printf "Size at %d is %s\n", idxSize, $idxSize;
	  #	printf "Pntr at %d is %s\n", idxPntr, $idxPntr;
	  #	printf "Shrt at %d is %s\n", idxShrt, $idxShrt;
	  #	printf "Usag at %d is %s\n", idxModi, $idxModi;
	  #	printf "Usag at %d is %s\n", idxUsed, $idxUsed;
	  #	printf "Comm at %d is %s\n", idxComm, $idxComm;

	  if ( Catg != $idxCatg ) isNewCategory = 1;
	  else			isNewCategory = 0;
	  Catg      = $idxCatg;

	  Name      = $idxName;
	  gsub ( " ", "_", Name );
	  Units     = $idxUnit;
	  Value     = $idxValu;
	  Default   = $idxDefl;
	  DataType  = $idxDTyp;
	  Format    = $idxFrmt;
	  Size      = $idxSize;
	  Pntr      = $idxPntr;
	  Short     = $idxShrt;
	  if ( Short == "wdat_low" ) {
			WlDoubleSet = -1;
			WlType = "Dat";
			WlTypeUpper = "DAT";
	  } else if ( Short == "wdat_hgh" ) {
			WlDoubleSet = 1;
			WlType = "Dat";
			WlTypeUpper = "DAT";
	  } else if ( Short == "wfit_low" ) {
			WlDoubleSet = -1;
			WlType = "Fit";
			WlTypeUpper = "FIT";
	  } else if ( Short == "wfit_hgh" ) {
			WlDoubleSet = 1;
			WlType = "Fit";
			WlTypeUpper = "FIT";
	  } else {
			WlDoubleSet = 0;
	  }
	  if ( Short == "lamptime" ) {
			IsLampTime = 1;
	  } else {
			IsLampTime = 0;
	  }
	  ShortUppr = toupper ( Short );
	  Modify    = $idxModi;
	  Access    = $idxAccs;
	  Setup     = $idxStup;
	  Store     = $idxStor;
	  Usage     = $idxUsed;
	  Comment   = $idxComm;

	  if ( "No" != Usage ) {

		if ( isNewCategory ) {
		printf "\r\n//\t%s Parameters\r\n\r\n", Catg											>> H_DEF
		}
		printf "\r\n//\r\n//\tConfiguration Parameter %s\r\n//\t%s\r\n//\r\n", Name, Comment	>> H_DEF

		if ( isNewCategory ) {
		printf "\r\n//\t%s Parameters\r\n\r\n", Catg											>> C_COD
		}
		printf "\r\n//\r\n//\tConfiguration Parameter %s\r\n//\t%s\r\n//\r\n", Name, Comment	>> C_COD

		nEVal = 0;
		nMVal = 0;
		isEnum = 0;

		if ( Store == "UP" ) {
		printf "# define UPG_LOC_%s (%d)\r\n\r\n", ShortUppr, Pntr	>> H_DEF
		} else {
		printf "# define BBV_LOC_%s %s\r\n\r\n", ShortUppr, Pntr								>> H_DEF
		}

		if ( DataType == "Enum" ) {
			isEnum = 1;
			DType = sprintf ( "CFG_%s", Name );
			DTypeDec = DType;
			nEVal = split ( Value, eVal, "[, ]*" );
			if ( useEnumAsEnum ) {
			  printf "typedef enum {\r\n"														>> H_DEF
			  for ( iVal=1; iVal<=nEVal; iVal++ ) {
				sep = (iVal<nEVal)?",":"";
				printf "\tCFG_%s_%s = %d%s\r\n", Name, eVal[iVal], iVal, sep					>> H_DEF
			  }
			  printf "} %s;\r\n", DType															>> H_DEF
			} else {
			  printf "typedef U8 %s;\r\n", DType												>> H_DEF
			  for ( iVal=1; iVal<=nEVal; iVal++ ) {
				printf "# define CFG_%s_%s %d\r\n", Name, eVal[iVal], iVal						>> H_DEF
			  }
			}

			printf "\r\n# define %s_DEF CFG_%s_%s\r\n\r\n", \
								 ShortUppr, Name, Default										>> H_DEF
		} else if ( DataType == "String" ) {
			DType = "char*";
			DTypeDec = "char";
			printf "\r\n# define %s_DEF \"\"\r\n", ShortUppr									>> H_DEF
		} else {
			DType = DataType;
			DTypeDec = DType;
			printf "# define %s_DEF %s\r\n", ShortUppr, Default									>> H_DEF
			#	Min/Max if present
			if ( Value != "*" ) {
				nMVal = split ( Value, mVal, "[:]" );
				if ( nMVal == 2 ) {
					printf "# define %s_MIN %s\r\n", ShortUppr, mVal[1]							>> H_DEF
					printf "# define %s_MAX %s\r\n", ShortUppr, mVal[2]							>> H_DEF
				} else {
					#	ERROR
				}
			}
		}

		#	Check if first letter in Enum List is unique.
		#	If so, will Accept single letter as input string in Set_***_AsString().
		if ( nEVal > 0 ) {

			uniqFirstELetter = 1;

			lC["A"] = 0; lC["B"] = 0; lC["C"] = 0; lC["D"] = 0; lC["E"] = 0;
			lC["F"] = 0; lC["G"] = 0; lC["H"] = 0; lC["I"] = 0; lC["J"] = 0;
			lC["K"] = 0; lC["L"] = 0; lC["M"] = 0; lC["N"] = 0; lC["O"] = 0;
			lC["P"] = 0; lC["Q"] = 0; lC["R"] = 0; lC["S"] = 0; lC["T"] = 0;
			lC["U"] = 0; lC["V"] = 0; lC["W"] = 0; lC["X"] = 0; lC["Y"] = 0;
			lC["Z"] = 0;

			for ( iVal=1; iVal<=nEVal; iVal++ ) {
				letter = toupper( substr ( eVal[iVal], 1, 1 ) );
				if ( lC[letter] == 1 ) uniqFirstELetter = 0;
				else lC[letter] = 1;
			}

		} else {
			uniqFirstELetter = 0;
		}

		AccessCheck="";
		SystemAcc=0
		
		if ( Access == "A" ) { AccessCheck = "(access_mode>=Access_Admin) &&"; }
		else if ( Access == "F" ) { AccessCheck = "(access_mode>=Access_Factory) &&"; }
		else if ( Access == "S" ) { SystemAcc = 1; AccessCheck = "(access_mode>=Access_Factory) &&"; }
		
		ProcessingAcc=0;
		if ( Setup == "P" ) ProcessingAcc = 1;

		if ( isNewCategory ) {
		printf "\r\n\t//\t%s Parameters\r\n\r\n", Catg																			>> C_TYP
		}
	#X	if ( !IsLampTime ) {
		    if ( DataType == "String" ) {
			printf "\t%s\t%s[%s+1];\r\n", DTypeDec, Short, Size																	>> C_TYP
		    } else {
			printf "\t%s\t%s;\r\n", DTypeDec, Short																				>> C_TYP
		    }
	#X	}

		########	C_SAV	BEGIN

		if ( isNewCategory ) {
		printf "\r\n\t//\t%s Parameters\r\n\r\n", Catg																			>> C_SAV
		}
	#X	if ( !IsLampTime ) {
		    if ( DataType == "String" ) {
		      if ( Store == "UP" ) {
				printf "\tif ( CFG_OK != saveToU ( backupMem, UPG_LOC_%s, (U8*)cfg_data.%s, %s ) ) {\r\n", ShortUppr, Short, Size		>> C_SAV
		      } else {
				printf "\tif ( CFG_OK != saveToR ( backupMem, BBV_LOC_%s, (U8*)cfg_data.%s, %s ) ) {\r\n", ShortUppr, Short, Size		>> C_SAV
		      }
		      printf "\t\trv = CFG_FAIL;\r\n" 																					>> C_SAV
		    } else {
		      if ( Store == "UP" ) {
				if ( Size < 4 ) {
				  printf "\tmVar = (U32)cfg_data.%s;\r\n", Short																	>> C_SAV
				  printf "\tif ( CFG_OK != saveToU ( backupMem, UPG_LOC_%s, (U8*)&mVar, 4 ) ) {\r\n", ShortUppr							>> C_SAV
				} else {
				  printf "\tif ( CFG_OK != saveToU ( backupMem, UPG_LOC_%s, (U8*)&(cfg_data.%s), %s ) ) {\r\n", ShortUppr, Short, Size	>> C_SAV
				}
		      } else {
				if ( useEnumAsEnum && 1 == isEnum ) {
				  printf "\tenum_asU8 = cfg_data.%s;\r\n", Short																>> C_SAV
				  printf "\tif ( CFG_OK != saveToR ( backupMem, BBV_LOC_%s, &enum_asU8, %s ) ) {\r\n", ShortUppr, Size					>> C_SAV
				} else {
				  printf "\tif ( CFG_OK != saveToR ( backupMem, BBV_LOC_%s, (U8*)&(cfg_data.%s), %s ) ) {\r\n", ShortUppr, Short, Size	>> C_SAV
				}
		      }
		      printf "\t\trv = CFG_FAIL;\r\n"																					>> C_SAV

		    }
		    printf "\t}\r\n"																									>> C_SAV
	#X	}

		########	C_SAV	END

		########	C_GEN	BEGIN

		if ( isNewCategory ) {
		printf "\r\n\t//\t%s Parameters\r\n\r\n", Catg																			>> C_GEN
		}
	#X	if ( !IsLampTime ) {
		    if ( DataType == "String" ) {
		      if ( Store == "UP" ) {
				printf "\tif ( CFG_OK != getFrmU ( backupMem, UPG_LOC_%s, (U8*)cfg_data.%s, %s ) ) {\r\n", ShortUppr, Short, Size		>> C_GEN
		      } else {
				printf "\tif ( CFG_OK != getFrmR ( backupMem, BBV_LOC_%s, (U8*)cfg_data.%s, %s ) ) {\r\n", ShortUppr, Short, Size		>> C_GEN
		      }

		      ########	printf "\t\tstrncpy ( cfg_data.%s, %s_DEF, %s-1 );\r\n", Short, ShortUppr, Size									>> C_DEF C_GEN

		      printf "\t\trv = CFG_FAIL;\r\n"																							>> C_GEN
		    } else {
		      if ( Store == "UP" ) {
				if ( Size < 4 ) {
				  printf "\tif ( CFG_OK != getFrmU ( backupMem, UPG_LOC_%s, (U8*)&mVar, 4 ) ) {\r\n", ShortUppr							>> C_GEN
				} else {
				  printf "\tif ( CFG_OK != getFrmU ( backupMem, UPG_LOC_%s, (U8*)&(cfg_data.%s), %s ) ) {\r\n", ShortUppr, Short, Size	>> C_GEN
				}
		      } else {
				if ( useEnumAsEnum && 1 == isEnum ) {
				  printf "\tif ( CFG_OK != getFrmR ( backupMem, BBV_LOC_%s, &enum_asU8, %s ) ) {\r\n", ShortUppr, Size					>> C_GEN
				} else {
				  printf "\tif ( CFG_OK != getFrmR ( backupMem, BBV_LOC_%s, (U8*)&(cfg_data.%s), %s ) ) {\r\n", ShortUppr, Short, Size	>> C_GEN
				}
		      }

#X			if ( Store == "RTC" ) {
#X
#X			  if ( nMVal == 2 ) {
#X				  printf "\tif ( %s_MIN > cfg_data.%s || cfg_data.%s > %s_MAX ) {\r\n", ShortUppr, Short, Short, ShortUppr					>> C_DEF
#X			  }
#X
#X			  if ( nEVal > 0 ) {
#X				printf "\tif ( CFG_Frame_Type_%s > cfg_data.%s || cfg_data.%s > CFG_Frame_Type_%s ) {\r\n", eVal[1], Short, Short, eVal[nEVal]	>> C_DEF
#X			  }
#X
#X			  if ( nMVal == 2 || nEVal>0 ) {
#X
#X				  printf "\t\tcfg_data.%s = %s_DEF;\r\n", Short, ShortUppr																		>> C_DEF
#X
#X				if ( 0 == SystemAcc ) {
#X					## if ( 1 == isEnum ) {
#X					## 	printf "\tio_out_string ( \"%s \" ); ", ShortUppr																>> C_DEF
#X					## 	printf "\tio_out_string ( CFG_Get_%s_AsString() ); io_out_string ( \"\\r\\n\" );\r\n", Name						>> C_DEF
#X					## } else if ( "String" == DataType ) {
#X					## 	printf "\tio_out_string ( CFG_Get_%s() ); io_out_string ( \"\\r\\n\" );\r\n", Name								>> C_DEF
#X					## } else if ( "F32" == DataType ) {
#X					## 	printf "\tio_out_F32 ( \"%s\\r\\n\", CFG_Get_%s() );\r\n", Format, Name											>> C_DEF
#X					## } else {
#X					## 	printf "\tio_out_S32 ( \"%s\\r\\n\", (S32)CFG_Get_%s() );\r\n", "%ld", Name										>> C_DEF
#X					## }
#X				}
#X
#X				  printf "\t}\r\n"																											>> C_DEF
#X
#X			  }
#X
#X			  ########	printf "\t\tcfg_data.%s = %s_DEF;\r\n", Short, ShortUppr																	>> C_DEF C_GEN
#X			}

			  printf "\t\trv = CFG_FAIL;\r\n"																							>> C_GEN

			if ( Store == "UP" && Size < 4 ) {
				printf "\t} else {\r\n\t\tcfg_data.%s = mVar;\r\n", Short																>> C_GEN
			} else if ( useEnumAsEnum && Store == "RTC" && 1 == isEnum ) {
				printf "\t} else {\r\n\t\tcfg_data.%s = enum_asU8;\r\n", Short															>> C_GEN
			}
		    }
		    printf "\t}\r\n"																											>> C_GEN
	#X	}

		########	C_GEN	END

		#
		#	GET function
		#

		printf "%s CFG_Get_%s(void);\r\n", DType, Name																			>> H_DEF

		printf "%s CFG_Get_%s(void) {\r\n", DType, Name																			>> C_COD
	#X	if ( IsLampTime ) {
	#X	    printf "\tU32 lt;\r\n"																								>> C_COD
	#X	    printf "\tif ( LAMPTIME_OK == lt_getLampTime ( &lt ) )\r\n"															>> C_COD
	#X	    printf "\t\treturn lt;\r\n"																							>> C_COD
	#X	    printf "\telse\r\n"																									>> C_COD
	#X	    printf "\t\treturn 0;\r\n"																							>> C_COD
	#X	} else {
		    printf "\treturn cfg_data.%s;\r\n", Short																			>> C_COD
	#X	}
		printf "}\r\n"																											>> C_COD
		printf "\r\n"																											>> C_COD

		#
		#	SET function
		#

		printf "S16 CFG_Set_%s( %s );\r\n", Name, DType																			>> H_DEF

		printf "S16 CFG_Set_%s( %s new_value ) {\r\n", Name, DType																>> C_COD

		if ( Store == "UP" ) {
		#	printf "\tdump_UPG();\r\n"																							>> C_COD
		}

		if ( ShortUppr == "PERDIVAL"  ) {
			printf "\tif ( CFG_Scheduling_Missing == CFG_Get_Scheduling() ) return CFG_FAIL;\r\n"		>> C_COD
		} else if ( ShortUppr == "PERDOFFS"  ) {
			printf "\tif ( CFG_Scheduling_Missing == CFG_Get_Scheduling() ) return CFG_FAIL;\r\n"		>> C_COD
		} else if ( ShortUppr == "PERDDURA"  ) {
			printf "\tif ( CFG_Scheduling_Missing == CFG_Get_Scheduling() ) return CFG_FAIL;\r\n"		>> C_COD
		} else if ( ShortUppr == "PERDSMPL"  ) {
			printf "\tif ( CFG_Scheduling_Missing == CFG_Get_Scheduling() ) return CFG_FAIL;\r\n"		>> C_COD
		}

		if ( nMVal == 2 ) {
			if ( "U8" == DType && "0" == mVal[1] && "255" == mVal[2] ) {
				## No need to check. new_value always within bounds
				indent = "\t";
				didRangeCheck = 0;
			} else if ( "U8" == DType && "0" == mVal[1] ) {
				## Only upper range check
				printf "\tif ( new_value <= %s_MAX ) {\r\n", ShortUppr							>> C_COD
				indent = "\t\t";
				didRangeCheck = 1;
			} else if ( "U16" == DType && "0" == mVal[1] && "65535" == mVal[2] ) {
				## No need to check. new_value always within bounds
				indent = "\t";
				didRangeCheck = 0;
			} else if ( "U16" == DType && "0" == mVal[1] ) {
				## Only upper range check
				printf "\tif ( new_value <= %s_MAX ) {\r\n", ShortUppr							>> C_COD
				indent = "\t\t";
				didRangeCheck = 1;
			} else if ( "U16" == DType && "65535" == mVal[2] ) {
				## Only upper range check
				printf "\tif ( %s_MIN <= new_value ) {\r\n", ShortUppr							>> C_COD
				indent = "\t\t";
				didRangeCheck = 1;
			} else {
				printf "\tif ( %s_MIN <= new_value && new_value <= %s_MAX ) {\r\n", ShortUppr, ShortUppr							>> C_COD
				indent = "\t\t";
				didRangeCheck = 1;
			}
		} else {
			indent = "\t";
			didRangeCheck = 0;
		}
	#X	if ( IsLampTime ) {
	#X	    printf "%sif ( LAMPTIME_OK == lt_resetLampTime ( new_value ) ) {\r\n", indent											>> C_COD
	#X	} else if ( DataType == "String" ) {
		if ( DataType == "String" ) {
		  if ( Store == "UP" ) {
		    printf "%sif ( CFG_OK == CFG_VarSaveToUPG ( 0, UPG_LOC_%s, (U8*)new_value, %s ) ) {\r\n", indent, ShortUppr, Size		>> C_COD
		  } else {
		    printf "%sif ( CFG_OK == CFG_VarSaveToRTC ( 0, BBV_LOC_%s, (U8*)new_value, %s ) ) {\r\n", indent, ShortUppr, Size		>> C_COD
		  }
		    printf "%s\tstrncpy ( cfg_data.%s, new_value, %s );\r\n", indent, Short, Size										>> C_COD
			printf "%s\tcfg_data.%s[%s] = '\\0';\r\n", indent, Short, Size														>> C_COD
		} else {
		  if ( Store == "UP" ) {
		    if ( Size < 4 ) {
		      printf "%sU32 mVar = new_value;\r\n", indent																		>> C_COD
		      printf "%sif ( CFG_OK == CFG_VarSaveToUPG( 0, UPG_LOC_%s, (void*)&mVar, 4 ) ) {\r\n", indent, ShortUppr						>> C_COD
		    } else {
		      printf "%sif ( CFG_OK == CFG_VarSaveToUPG( 0, UPG_LOC_%s, (void*)&new_value, %s ) ) {\r\n", indent, ShortUppr, Size			>> C_COD
		    }
		  } else {
			if ( useEnumAsEnum && 1 == isEnum ) {
			  printf "%sU8 new_value_asU8 = new_value;\r\n", indent																>> C_COD
		      printf "%sif ( CFG_OK == CFG_VarSaveToRTC ( 0, BBV_LOC_%s, (U8*)&new_value_asU8, %s ) ) {\r\n", indent, ShortUppr, Size		>> C_COD
			} else {
		      printf "%sif ( CFG_OK == CFG_VarSaveToRTC ( 0, BBV_LOC_%s, (U8*)&new_value, %s ) ) {\r\n", indent, ShortUppr, Size			>> C_COD
			}
		  }
	#X	  if ( IsLampTime ) {
	#X	  printf "%s\tlt_logLampUse  ( new_value, 'Z' );\r\n", indent															>> C_COD
  	#X	  }
		  printf "%s\tcfg_data.%s = new_value;\r\n", indent, Short																>> C_COD
		}

		if ( 1 == ProcessingAcc ) {
		printf "%s\tCFG_Set_Data_File_Needs_Header ( CFG_Data_File_Needs_Header_Yes );\r\n", indent								>> C_COD
		}
		printf "%s\treturn CFG_OK;\r\n", indent																					>> C_COD
		printf "%s} else {\r\n", indent																							>> C_COD

		printf "%s\treturn CFG_FAIL;\r\n", indent																				>> C_COD
		printf "%s}\r\n", indent																								>> C_COD
		if ( 1 == didRangeCheck ) {
			printf "\t} else {\r\n"																								>> C_COD
			printf "\t\treturn CFG_FAIL;\r\n"																					>> C_COD
			printf "\t}\r\n"																									>> C_COD
		}
		printf "}\r\n"																											>> C_COD
		printf "\r\n"																											>> C_COD

		#
		#	SETUP function
		#

		if ( Setup == "Y" ) {

		  printf "void CFG_Setup_%s();\r\n", Name, DType																		>> H_DEF

		  printf "void CFG_Setup_%s() {\r\n", Name, DType																		>> C_COD
		  printf "\r\n"																											>> C_COD
		  printf "\tchar input[64];\r\n"																						>> C_COD
		  printf "\r\n"																											>> C_COD
		  if ( "String" == DataType ) {
		  printf "\r\n"																											>> C_COD
		  printf "\tif(!isprint(CFG_Get_%s()[0]))\r\n", Name																	>> C_COD
		  printf "\t\tCFG_Set_%s(\".\");\r\n\r\n", Name																			>> C_COD
		  }
		  printf "\tdo {\r\n"																									>> C_COD
		  printf "\t\ttlm_flushRecv();\r\n"																						>> C_COD
		  printf "\t\tio_out_string ( \"\\r\\n%s [\" );\r\n", Name																>> C_COD
		  if ( isEnum ) {
		    printf "\t\tio_out_string ( CFG_Get_%s_AsString() );\r\n", Name														>> C_COD
		    printf "\t\tio_out_string( \"]: \" );\r\n"																			>> C_COD
		  } else if ( "String" == DataType ) {
		    printf "\t\tio_out_string ( CFG_Get_%s() );\r\n", Name																>> C_COD
		    printf "\t\tio_out_string( \"]: \" );\r\n"																			>> C_COD
		  } else if ( "F64" == DataType ) {
		    printf "\t\tio_out_F64 ( \"%s]: \", CFG_Get_%s() );\r\n", Format, Name												>> C_COD
		  } else if ( "F32" == DataType ) {
		    printf "\t\tio_out_F32 ( \"%s]: \", CFG_Get_%s() );\r\n", Format, Name												>> C_COD
		  } else {
		    printf "\t\tio_out_S32 ( \"%s]: \", (S32)CFG_Get_%s() );\r\n", "%ld", Name											>> C_COD
		  }
		  printf "\t\tio_in_getstring( input, sizeof(input), 0, 0 );\r\n"													>> C_COD
		  if ( isEnum ) {
		    printf "\t\tif ( !input[0] ) strcpy ( input, CFG_Get_%s_AsString() );\r\n", Name									>> C_COD
		  } else if ( "String" == DataType ) {
		    printf "\t\tif ( !input[0] ) strcpy ( input, CFG_Get_%s() );\r\n", Name												>> C_COD
		  } else if ( DataType == "String" ) {
		    printf "\t\tif ( !input[0] ) strcpy ( input, CFG_Get_%s() );\r\n", Name												>> C_COD
		  } else if ( "F64" == DataType ) {
		    printf "\t\tif ( !input[0] ) snprintf ( input, sizeof(input), \"%s\", printAbleF64 ( CFG_Get_%s() ) );\r\n", Format, Name 			>> C_COD
		  } else if ( "F32" == DataType ) {
		    printf "\t\tif ( !input[0] ) snprintf ( input, sizeof(input), \"%s\", printAbleF32 ( CFG_Get_%s() ) );\r\n", Format, Name 			>> C_COD
		  } else {
		    printf "\t\tif ( !input[0] ) snprintf ( input, sizeof(input), \"%s\", CFG_Get_%s() );\r\n", Format, Name			>> C_COD
		  }
		  if ( DataType != "String" ) {
		    printf "\t} while ( CFG_FAIL == CFG_Set_%s_AsString( input ) );\r\n", Name											>> C_COD
		  } else {
		    printf "\t} while ( CFG_FAIL == CFG_Set_%s( input ) );\r\n", Name													>> C_COD
		  }

		  printf "\tio_out_string ( \"%s \" );\r\n", Name																		>> C_COD
		  if ( isEnum ) {
		    printf "\tio_out_string ( CFG_Get_%s_AsString() );\r\n", Name														>> C_COD
		    printf "\tio_out_string ( \"\\r\\n\" );\r\n"																		>> C_COD
		  } else if ( "String" == DataType ) {
		    printf "\t\tio_out_string ( CFG_Get_%s() );\r\n", Name																>> C_COD
		    printf "\tio_out_string ( \"\\r\\n\" );\r\n"																		>> C_COD
		  } else if ( "F64" == DataType ) {
		    printf "\tio_out_F64 ( \"%s\\r\\n\", CFG_Get_%s() );\r\n", Format, Name												>> C_COD
		  } else if ( "F32" == DataType ) {
		    printf "\tio_out_F32 ( \"%s\\r\\n\", CFG_Get_%s() );\r\n", Format, Name												>> C_COD
		  } else {
		    printf "\tio_out_S32 ( \"%s\\r\\n\", (S32)CFG_Get_%s() );\r\n", "%ld", Name											>> C_COD
		  }
		  printf "\r\n"																											>> C_COD
		  printf "}\r\n"																										>> C_COD


		}

		#
		#	GET As String function
		#

		if ( 0 == SystemAcc ) {

		if ( "Enum" == DataType ) {
			printf "char* CFG_Get_%s_AsString(void);\r\n", Name																	>> H_DEF

			printf "char* CFG_Get_%s_AsString(void) {\r\n", Name																>> C_COD
			printf "\tswitch ( CFG_Get_%s() ) {\r\n", Name																		>> C_COD
			for ( iVal=1; iVal<=nEVal; iVal++ ) {
				printf "\tcase CFG_%s_%s: return \"%s\";\r\n", Name, eVal[iVal], eVal[iVal]										>> C_COD
			}
				printf "\tdefault: return \"N/A\";\r\n"																			>> C_COD
			printf "\t}\r\n"																									>> C_COD
			printf "}\r\n"																										>> C_COD
			printf "\r\n"																										>> C_COD
		}

		if ( Name == "Baud_Rate" ) {
			printf "U32 CFG_Get_%s_AsValue(void);\r\n", Name																	>> H_DEF

			printf "U32 CFG_Get_%s_AsValue(void) {\r\n", Name																	>> C_COD
			printf "\tswitch ( CFG_Get_%s() ) {\r\n", Name																		>> C_COD
			for ( iVal=1; iVal<=nEVal; iVal++ ) {
				printf "\tcase CFG_%s_%s: return %sL;\r\n", Name, eVal[iVal], eVal[iVal]										>> C_COD
			}
				printf "\tdefault: return 9600;\r\n"																			>> C_COD
			printf "\t}\r\n"																									>> C_COD
			printf "}\r\n"																										>> C_COD
			printf "\r\n"																										>> C_COD
		}


		if ( 0 == WlDoubleSet && "U64" != DataType ) {

		if ( DataType != "String" ) {
		printf "S16 CFG_Set_%s_AsString( char* );\r\n", Name																	>> H_DEF

		#
		#	Set_***_AsString()
		#		Enum: String comparisons, then assign Enum Value
		#
		printf "S16 CFG_Set_%s_AsString( char* new_value ) {\r\n", Name															>> C_COD

		if ( "Enum" == DataType ) {
			ee = "";
			for ( iVal=1; iVal<=nEVal; iVal++ ) {
				printf "\t%sif ( 0 == strcasecmp ( new_value, \"%s\" ) ) return CFG_Set_%s ( CFG_%s_%s );\r\n", ee, eVal[iVal], Name, Name, eVal[iVal]	>> C_COD
				ee = "else ";
				if ( allowUniqueFirstLetters && uniqFirstELetter ) {
				printf "\t%sif ( 0 == strcasecmp ( new_value, \"%s\" ) ) return CFG_Set_%s ( CFG_%s_%s );\r\n", ee, substr ( eVal[iVal], 1, 1), Name, Name, eVal[iVal]	>> C_COD
				}
			}
			printf "\telse return CFG_FAIL;\r\n"																				>> C_COD
		} else {
			printf "\terrno = 0;\r\n"																							>> C_COD
		  if ( "F64" == DataType ) {
			printf "\tF64 new_num = strtod(new_value, (char**)0 );\r\n"														>> C_COD
			printf "\tif ( 0 == errno )\r\n"																					>> C_COD
			printf "\t\treturn CFG_Set_%s ( new_num );\r\n", Name																>> C_COD
			printf "\telse\r\n"																									>> C_COD
			printf "\t\treturn CFG_FAIL;\r\n"																					>> C_COD
	      } else if ( "F32" == DataType ) {
			printf "\tF32 new_num = strtof(new_value, (char**)0 );\r\n"														>> C_COD
			printf "\tif ( 0 == errno )\r\n"																					>> C_COD
			printf "\t\treturn CFG_Set_%s ( new_num );\r\n", Name																>> C_COD
			printf "\telse\r\n"																									>> C_COD
			printf "\t\treturn CFG_FAIL;\r\n"																					>> C_COD
		  } else if ( "U32" == DataType ) {
			printf "\tU32 new_num = strtoul(new_value, (char**)0, 0 );\r\n", DataType											>> C_COD
			printf "\tif ( 0 == errno )\r\n"																					>> C_COD
			printf "\t\treturn CFG_Set_%s ( new_num );\r\n", Name																>> C_COD
			printf "\telse\r\n"																									>> C_COD
			printf "\t\treturn CFG_FAIL;\r\n"																					>> C_COD
		  } else if ( "U16" == DataType ) {
			printf "\tU32 new_num = strtoul(new_value, (char**)0, 0 );\r\n", DataType											>> C_COD
			printf "\tif ( 0 == errno && new_num < 0x10000 )\r\n"																>> C_COD
			printf "\t\treturn CFG_Set_%s ( (U16)new_num );\r\n", Name															>> C_COD
			printf "\telse\r\n"																									>> C_COD
			printf "\t\treturn CFG_FAIL;\r\n"																					>> C_COD
		  } else if ( "U8" == DataType ) {
			printf "\tU32 new_num = strtoul(new_value, (char**)0, 0 );\r\n", DataType											>> C_COD
			printf "\tif ( 0 == errno && new_num < 0x100 )\r\n"																	>> C_COD
			printf "\t\treturn CFG_Set_%s ( (U8)new_num );\r\n", Name															>> C_COD
			printf "\telse\r\n"																									>> C_COD
			printf "\t\treturn CFG_FAIL;\r\n"																					>> C_COD
		  } else {
			printf "\t%s new_num = strtol(new_value, (char**)0, 0 );\r\n", DataType											>> C_COD
			printf "\tif ( 0 == errno )\r\n"																					>> C_COD
			printf "\t\treturn CFG_Set_%s ( new_num );\r\n", Name																>> C_COD
			printf "\telse\r\n"																									>> C_COD
			printf "\t\treturn CFG_FAIL;\r\n"																					>> C_COD
		  }
		}
		printf "}\r\n"																											>> C_COD
		printf "\r\n"																											>> C_COD
		}

#		} else if ( 1 == WlDoubleSet ) {
#
#		printf "S16 CFG_Set_%s_Wavelengths_AsString( char* );\r\n", WlType														>> H_DEF
#
#		printf "S16 CFG_Set_%s_Wavelengths_AsString( char* new_value ) {\r\n", WlType											>> C_COD
#
#		printf "\tchar* comma_position = strchr ( new_value, ',' );\r\n"														>> C_COD
#		printf "\tif ( !comma_position ) return CFG_FAIL;\r\n"																	>> C_COD
#		printf "\r\n"																											>> C_COD
#		printf "\t//\tExpect %%f,%%f format\r\n"																				>> C_COD
#		printf "\terrno = 0;\r\n"																								>> C_COD
#		printf "\tF32 new_low = strtof ( new_value, 0 );\r\n"																	>> C_COD
#		printf "\tif ( errno ) return CFG_FAIL;\r\n"																			>> C_COD
#		printf "\tF32 new_hgh = strtof ( comma_position+1, 0 );\r\n"															>> C_COD
#		printf "\r\n"																											>> C_COD
#		printf "\tif ( !errno ) {\r\n"																							>> C_COD
#		printf "\t\t//\tFurther sanity checks on values handled inside following call\r\n"										>> C_COD
#		printf "\t\treturn CFG_Set_%s_Wavelengths ( new_low, new_hgh );\r\n", WlType											>> C_COD
#		printf "\t} else {\r\n"																									>> C_COD
#		printf "\t\treturn CFG_FAIL;\r\n"																						>> C_COD
#		printf "\t}\r\n"																										>> C_COD
#		printf "}\r\n"																											>> C_COD
#		printf "\r\n"																											>> C_COD

		}

		}	#	SystemAcc

		if ( isNewCategory ) {
		printf "\r\n\t//\t%s Parameters\r\n\r\n", Catg																			>> C_GET
		printf "\r\n\t//\t%s Parameters\r\n\r\n", Catg																			>> C_SET
		printf "\r\n\t//\t%s Parameters\r\n", Catg																				>> C_PRN
		}

		if ( 0 == SystemAcc ) {

		  printf "\t%sif ( 0 == strcasecmp ( option, \"%s\" ) ) {\r\n", cmd_ee, ShortUppr										>> C_GET

		  if ( 1==isEnum ) {
		    printf "\t\tstrncpy ( result, CFG_Get_%s_AsString(), r_max_len_m1 );\r\n", Name										>> C_GET
		  } else if ( "String" == DataType ) {
		    printf "\t\tstrncpy ( result, CFG_Get_%s(), r_max_len_m1 );\r\n", Name													>> C_GET
		  } else if ( "F64" == DataType ) {
		    printf "\t\tsnprintf ( result, r_max_len, \"%s\", printAbleF64 ( CFG_Get_%s() ) );\r\n", Format, Name				>> C_GET
		  } else if ( "F32" == DataType ) {
		    printf "\t\tsnprintf ( result, r_max_len, \"%s\", printAbleF32 ( CFG_Get_%s() ) );\r\n", Format, Name				>> C_GET
		  } else {
		    printf "\t\tsnprintf ( result, r_max_len, \"%s\", CFG_Get_%s() );\r\n", Format, Name								>> C_GET
		  }

		  if ( 0 == WlDoubleSet && "U64" != DataType ) {
		    printf "\t%sif (%s 0 == strcasecmp ( option, \"%s\" ) ) {\r\n", cmd_ee, AccessCheck, ShortUppr						>> C_SET
		    if ( DataType != "String" ) {
			  if ( IsLampTime ) {
		      printf "\t\tif ( CFG_FAIL == CFG_Set_%s_AsString ( value ) ) { cec = CEC_%s_Invalid;\r\n", Name, Name				>> C_SET
			  printf "\t\t} else { lt_logLampUse  ( strtoul(value, (char**)0, 0 ), 'Z' ); }\r\n"								>> C_SET
  			  } else {
		      printf "\t\tif ( CFG_FAIL == CFG_Set_%s_AsString ( value ) ) cec = CEC_%s_Invalid;\r\n", Name, Name				>> C_SET
	  		  }
		    } else {
		      printf "\t\tif ( CFG_FAIL == CFG_Set_%s ( value ) ) cec = CEC_%s_Invalid;\r\n", Name, Name						>> C_SET
		    }
		    printf "# define CEC_%s_Invalid	%d\r\n", Name, StartCode+Pntr														>> H_CEC

#		  } else if ( 1 == WlDoubleSet ) {
#		    printf "\t%sif (%s 0 == strcasecmp ( option, \"W%sBOTH\" ) ) {\r\n", \
#					cmd_ee, AccessCheck, WlTypeUpper																					>> C_SET
#		    printf "\t\tif ( CFG_FAIL == CFG_Set_%s_Wavelengths_AsString ( value ) ) cec = CEC_Fit_Wavelengths_Invalid;\r\n", WlType	>> C_SET
#		    printf "# define CEC_%s_Wavelengths_Invalid	%d\r\n", WlType, StartCode+Pntr											>> H_CEC
		  }
		  printf "\r\n"																											>> C_SET

		  if ( !( ShortUppr == "ADMIN_PW" || ShortUppr == "FCTRY_PW" ) ) {
			printf "\tio_out_string ( \"%s \" ); ", ShortUppr																	>> C_PRN
			if ( 1 == isEnum ) {
				printf "\tio_out_string ( CFG_Get_%s_AsString() ); io_out_string ( \"\\r\\n\" );\r\n", Name						>> C_PRN
			} else if ( "String" == DataType ) {
				printf "\tio_out_string ( CFG_Get_%s() ); io_out_string ( \"\\r\\n\" );\r\n", Name								>> C_PRN
			} else if ( "F64" == DataType ) {
				printf "\tio_out_F64 ( \"%s\\r\\n\", CFG_Get_%s() );\r\n", Format, Name											>> C_PRN
			} else if ( "F32" == DataType ) {
				printf "\tio_out_F32 ( \"%s\\r\\n\", CFG_Get_%s() );\r\n", Format, Name											>> C_PRN
			} else if ( "U64" == DataType ) {
				printf "\tio_dump_X64 ( CFG_Get_%s(), \"\\r\\n\" );\r\n", Name													>> C_PRN
			} else {
				printf "\tio_out_S32 ( \"%s\\r\\n\", (S32)CFG_Get_%s() );\r\n", "%ld", Name									>> C_PRN
			}
		  }

		  #	Code for function to recognize out-of-range values

		  if ( nMVal == 2 ) {

			  if ( "U8" == DType && "0" == mVal[1] && "255" == mVal[2] ) {
				## No need to check. value always within bounds
				didRangeCheck = 0;
			  } else if ( "U8" == DType && "0" == mVal[1] ) {
				## Only upper range check
			    printf "\tif ( cfg_data.%s > %s_MAX ) {\r\n", Short, ShortUppr								>> C_OOR
				didRangeCheck = 1;
			  } else if ( "U16" == DType && "0" == mVal[1] && "65535" == mVal[2] ) {
				## No need to check. new_value always within bounds
				didRangeCheck = 0;
			  } else if ( "U16" == DType && "0" == mVal[1] ) {
				## Only upper range check
			    printf "\tif ( cfg_data.%s > %s_MAX ) {\r\n", Short, ShortUppr								>> C_OOR
				didRangeCheck = 1;
			  } else if ( "U16" == DType && "65535" == mVal[2] ) {
				## Only upper range check
			    printf "\tif ( %s_MIN > cfg_data.%s ) {\r\n", ShortUppr, Short							>> C_OOR
				didRangeCheck = 1;
			  } else {
			    printf "\tif ( %s_MIN > cfg_data.%s || cfg_data.%s > %s_MAX ) {\r\n", ShortUppr, Short, Short, ShortUppr								>> C_OOR
				didRangeCheck = 1;
			  }

			  if ( didRangeCheck ) {
				printf "\t\tcnt++;\r\n"																												>> C_OOR

				if ( Store == "RTC" ) {
				  printf "\t\tif ( correctOOR ) cfg_data.%s = %s_DEF;\r\n", Short, ShortUppr														>> C_OOR
		 		}
				printf "\t}\r\n"																														>> C_OOR
			  }
			  if ( Store == "RTC" ) {
				  printf "\tif ( setRTCToDefault ) cfg_data.%s = %s_DEF;\r\n\r\n", Short, ShortUppr													>> C_OOR
		 	  }
		  } else {

		   if ( nEVal > 0 ) {
			  printf "\tif ( CFG_%s_%s > cfg_data.%s || cfg_data.%s > CFG_%s_%s ) {\r\n", Name, eVal[1], Short, Short, Name, eVal[nEVal]			>> C_OOR
			  printf "\t\tcnt++;\r\n"																												>> C_OOR
			  if ( Store == "RTC" ) {
				  printf "\t\tif ( correctOOR ) cfg_data.%s = %s_DEF;\r\n", Short, ShortUppr														>> C_OOR
		 	  }
			  printf "\t}\r\n"																														>> C_OOR
			  if ( Store == "RTC" ) {
				  printf "\tif ( setRTCToDefault ) cfg_data.%s = %s_DEF;\r\n\r\n", Short, ShortUppr													>> C_OOR
		 	  }
		   } else {
			  if ( "LAMPTIME" != ShortUppr && Store == "RTC" ) {
				  printf "\tif ( setRTCToDefault ) cfg_data.%s = %s_DEF;\r\n\r\n", Short, ShortUppr													>> C_OOR
		 	  }
		   }
		  }

		  cmd_ee = "} else ";

		}	#	SystemAcc

	  } # No Usage
	}



}
END {
	printf "\r\n"																	>> C_TYP
	printf "} cfg_data_struct;\r\n\r\nstatic cfg_data_struct cfg_data;\r\n\r\n"		>> C_TYP

	printf "\r\n"																	>> C_SAV
	printf "\tif ( useBackup ) {\r\n"												>> C_SAV
	printf "\t\tif ( CFG_FAIL == cfg_SaveToBackup( backupMem ) ) {\r\n"				>> C_SAV
	printf "\t\t\trv = CFG_FAIL;\r\n"												>> C_SAV
	printf "\t\t}\r\n"																>> C_SAV
	printf "\t\tvPortFree ( backupMem );\r\n"										>> C_SAV
	printf "\t}\r\n"																>> C_SAV
	printf "\r\n"																	>> C_SAV
	printf "\treturn rv;\r\n"														>> C_SAV
	printf "}\r\n"																	>> C_SAV
	printf "\r\n"																	>> C_SAV

	printf "\r\n"																	>> C_GEN
	printf "\tif ( useBackup ) {\r\n"												>> C_GEN
	printf "\t\tvPortFree ( backupMem );\r\n"										>> C_GEN
	printf "\t}\r\n"																>> C_GEN
	printf "\r\n"																	>> C_GEN
	printf "\treturn rv;\r\n"														>> C_GEN
	printf "}\r\n"																	>> C_GEN
	printf "\r\n"																	>> C_GEN

#X	printf "}\r\n"																	>> C_DEF
#X	printf "\r\n"																	>> C_DEF

	printf "\t} else if ( 0 == strcasecmp ( option, \"Clock\" ) ) {\r\n"						>> C_SET
	printf "\t\tstruct tm tt;\r\n"																>> C_SET
	printf "\t\tif ( strptime ( value, DATE_TIME_IO_FORMAT, &tt ) ) {\r\n"						>> C_SET
	printf "\t\t\tstruct timeval tv;\r\n"														>> C_SET
	printf "\t\t\ttv.tv_sec = mktime ( &tt );\r\n"												>> C_SET
	printf "\t\t\ttv.tv_usec = 0;\r\n"															>> C_SET
	printf "\t\t\tsettimeofday ( &tv, 0 );\r\n"													>> C_SET
	printf "\t\t\ttime_sys2ext();\r\n"															>> C_SET
	printf "\t\t\tcec = CEC_Ok;\r\n"															>> C_SET
	printf "\t\t} else {\r\n"																	>> C_SET
	printf "\t\t\tcec = CEC_Failed;\r\n"														>> C_SET
	printf "\t\t}\r\n"																			>> C_SET

#	printf "\t} else if ( 0 == strcasecmp ( option, \"ActiveCalFile\" ) ) {\r\n"				>> C_SET
#	printf "\t\tcec = FSYS_SetActiveCalFile ( value );\r\n"										>> C_SET

	printf "\t} else {\r\n"																		>> C_SET
	printf "\t\tcec = CEC_ConfigVariableUnknown;\r\n"											>> C_SET
	printf "\t}\r\n"																			>> C_SET
	printf "\r\n"																				>> C_SET
	printf "\treturn cec;\r\n"																	>> C_SET
	printf "}\r\n"																				>> C_SET
	printf "\r\n"																				>> C_SET

	printf "\t} else if ( 0 == strcasecmp ( option, \"FirmwareVersion\" ) ) {\r\n"													>> C_GET
	printf "\t\tstrncpy ( result, HNV_FW_VERSION_MAJOR \".\" HNV_FW_VERSION_MINOR \".\" HNV_CTRL_FW_VERSION_PATCH, r_max_len_m1 );\r\n"			>> C_GET

	printf "\t} else if ( 0 == strcasecmp ( option, \"ExtPower\" ) ) {\r\n" 		>> C_GET
	printf "\t\tF32 v, p3v3;\r\n"															>> C_GET
	printf "\t\tif ( SYSMON_OK == sysmon_getVoltages(&v, &p3v3) ) {\r\n"					>> C_GET
	printf "\t\t\tcec = CEC_Ok;\r\n"												>> C_GET
	printf "\t\t\tif(v >= 6)\r\n"													>> C_GET
	printf "\t\t\t\tstrncpy ( result, \"On\", r_max_len_m1 );\r\n"					>> C_GET
	printf "\t\t\telse\r\n"															>> C_GET
	printf "\t\t\t\tstrncpy ( result, \"Off\", r_max_len_m1 );\r\n"					>> C_GET
	printf "\t\t} else {\r\n"														>> C_GET
	printf "\t\t\tcec = CEC_Failed;\r\n"											>> C_GET
	printf "\t\t}\r\n"																>> C_GET

	printf "\t} else if ( 0 == strcasecmp ( option, \"USBCom\" ) ) {\r\n" 			>> C_GET
	printf "\t\tif ( Is_usb_vbus_high() && Is_device_enumerated() ) {\r\n"			>> C_GET
	printf "\t\t\tstrncpy ( result, \"On\", r_max_len_m1 );\r\n"					>> C_GET
	printf "\t\t} else {\r\n"														>> C_GET
	printf "\t\t\tstrncpy ( result, \"Off\", r_max_len_m1 );\r\n"					>> C_GET
	printf "\t\t}\r\n"																>> C_GET
	printf "\t\tcec = CEC_Ok;\r\n"													>> C_GET

	printf "\t} else if ( 0 == strcasecmp ( option, \"PkgCRC\" ) ) {\r\n"															>> C_GET
	printf "\t\tcec = FSYS_CmdCRC ( \"PKG\", 0, result, r_max_len );\r\n"															>> C_GET

	printf "\t} else if ( 0 == strcasecmp ( option, \"Clock\" ) ) {\r\n"															>> C_GET
	printf "\t\tstrncpy ( result, str_time_formatted ( time( (time_t*)0 ), DATE_TIME_IO_FORMAT ), r_max_len_m1 );\r\n"					>> C_GET
	printf "\t\tcec = CEC_Ok;\r\n"																									>> C_GET

	printf "\t} else if ( 0 == strncasecmp ( option, \"Disk\", 4 ) ) {\r\n"															>> C_GET
	printf "\t\tU64 total;\r\n"																										>> C_GET
	printf "\t\tU64 avail;\r\n"																										>> C_GET
	printf "\t\tif ( FILE_OK != f_space ( EMMC_DRIVE_CHAR, &avail, &total ) ) {\r\n"												>> C_GET
	printf "\t\t\tcec = CEC_Failed;\r\n"																							>> C_GET
	printf "\t\t} else {\r\n"																										>> C_GET
	printf "\t\t\tif ( 0 == strcasecmp ( \"Total\", option+4 ) ) {\r\n"																>> C_GET
	printf "\t\t\t\tsnprintf ( result, r_max_len, \"%s\", total );\r\n", "%llu"														>> C_GET
	printf "\t\t\t\tcec = CEC_Ok;\r\n"																								>> C_GET
	printf "\t\t\t} else if ( 0 == strcasecmp ( \"Free\", option+4 ) ) {\r\n"														>> C_GET
	printf "\t\t\t\tsnprintf ( result, r_max_len, \"%s\", avail );\r\n", "%llu"														>> C_GET
	printf "\t\t\t\tcec = CEC_Ok;\r\n"																								>> C_GET
	printf "\t\t\t} else {\r\n"																										>> C_GET
	printf "\t\t\t\tcec = CEC_Failed;\r\n"																							>> C_GET
	printf "\t\t\t}\r\n"																											>> C_GET
	printf "\t\t}\r\n"																												>> C_GET

	printf "\t} else if ( 0 == strcasecmp ( option, \"cfg\" ) ) {\r\n"																>> C_GET
	printf "\t\tCFG_Print();\r\n"																									>> C_GET

	printf "\t} else {\r\n"																											>> C_GET
	printf "\t\tcec = CEC_ConfigVariableUnknown;\r\n"																				>> C_GET
	printf "\t}\r\n"																												>> C_GET
	printf "\r\n"																													>> C_GET
	printf "\treturn cec;\r\n"																										>> C_GET
	printf "}\r\n"																													>> C_GET
	printf "\r\n"																													>> C_GET

	printf "}\r\n"																	>> C_PRN
	printf "\r\n"																	>> C_PRN

	printf "\r\n"																	>> C_OOR
	printf "\treturn cnt;\r\n"														>> C_OOR
	printf "}\r\n"																	>> C_OOR
	printf "\r\n"																	>> C_OOR

	printf "\r\n"																	>> H_CEC
	printf "//\r\n"																	>> H_CEC
	printf "//\tEnd of Error Codes for Configuration Commands\r\n"					>> H_CEC
	printf "//\r\n"																	>> H_CEC
	printf "\r\n"																	>> H_CEC
}
