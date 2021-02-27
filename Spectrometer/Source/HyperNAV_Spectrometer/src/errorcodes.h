# ifndef ERRORCODES_H_
# define ERRORCODES_H_

//	Generic Codes

# define CEC_Ok                            0
# define CEC_Failed                        1  /* Unknown reason */

# define CEC_UnknownShellCommand		   5
# define CEC_EmptyShellCommand			   6

# define CEC_PermissionDenied			   9

# define CEC_UnknownAccessOption		 101
# define CEC_WrongAccessPassword		 102

# define CEC_No_Integrated_Wiper		 201
# define CEC_Integrated_Wiper_Is_Off	 202


# if 0

# define CEC_CmdUnknown                    2
# define CEC_CmdMisformatted			   3
# define CEC_CmdNotImplemented             9

# define CEC_CmdSndUnknown                13
# define CEC_CmdDelUnknown                14
# define CEC_CmdDspUnknown                15

# define CEC_CmdConfUnknown               21
# define CEC_CmdConfPortUnknown           22
# define CEC_CmdWavelengthsInvalid		  23
# define CEC_CmdPortInvalid               27

# define CEC_CmdInfoUnknown               31
# define CEC_CmdDACUnknown                41


# define CEC_FilesNotPresent            1002
# define CEC_RcvFileTooBig              1005
# define CEC_FailedChecksum             1009
# define CEC_DspFailed                  1010
# define CEC_InvalidLogFileLetter       1012
# define CEC_InvalidDataFileCounter     1013
# define CEC_InvalidSettings            1014
# define CEC_FailedPicoVersion          1015

//	Errors when changing configuration
# define CEC_ConfigFailedToSave         2001

# define CEC_MessageLevelInvalid		2100
# define CEC_DebugFlagInvalid			2101
# define CEC_MessageFlagInvalid			2101
# define CEC_OpModeInvalid				2102
# define CEC_SensorTypeInvalid			2103

//define CEC_Ports....      			2104
//define CEC_NumChann.......			2105
# define CEC_DataModeInvalid			2106
# define CEC_WatchDogFlagInvalid		2107

# define CEC_TempCompFlagInvalid		2108
# define CEC_SaltyFitFlagInvalid		2109
# define CEC_LampShutterFlagInvalid		2110
# define CEC_TimeReslFlagInvalid		2111

# define CEC_BaselineInvalid			2112
# define CEC_FitConcsInvalid			2113
# define CEC_DatFSizeInvalid			2114
//define CEC_TimeZoneInvalid			2115

# define CEC_DrkAversInvalid			2116
# define CEC_LgtAversInvalid			2117
# define CEC_PreScansInvalid			2118
# define CEC_OutFrTypInvalid			2119

# define CEC_LogFrTypInvalid			2120

//define CEC_SerialNoInvalid			2216
# define CEC_CountdwnInvalid			2217
# define CEC_APFAtOffInvalid			2218
# define CEC_WarmTimeInvalid			2219

# define CEC_ErrFSizeInvalid			2220
# define CEC_CleanTimInvalid			2221
# define CEC_RefSmplsInvalid			2222
# define CEC_RefLimitInvalid			2223

# define CEC_SpIntPerInvalid			2224
# define CEC_LgtSmplsInvalid			2225
# define CEC_DrkSmplsInvalid			2226

# define CEC_WlensFitInvalid			2316
# define CEC_WlensDatInvalid			2318

# define CEC_FixdDuraInvalid			2320
//define CEC_SenSleepInvalid			2321
# define CEC_AbCutOffInvalid			2322

//# define CEC_BaudrateInvalid            2011
//# define CEC_DeployCntInvalid           2012

//# define CEC_InvalidPortNum             2501
//# define CEC_UnknownPortType            2502
//# define CEC_WrongPortType              2503
//# define CEC_PortNotConfigured          2504
//# define CEC_ConfigPortFailedToSave     2505
//# define CEC_PortBaudrateOverload       2506
//# define CEC_ImpossibleFrameVarLen      2507
//# define CEC_ImpossibleFrameFixedLen    2508
//# define CEC_CannotRemovePort           2509
//# define CEC_WrongTSType                2510

//# define CEC_PortBaudrateInvalid        2511
//# define CEC_DatabitsInvalid            2512
//# define CEC_ParityInvalid              2513

//# define CEC_FrameSyncTooLong           2521
//# define CEC_FrameLengthTooLong         2522

//# define CEC_UnknownPortState           2531

//# define CEC_WrongSamplingRange         2541

//# define CEC_HeaderTooLong              2551
//# define CEC_TerminatorTooLong          2553

# define CEC_UpCommandTooLong           2561
# define CEC_DownCommandTooLong         2562

# define CEC_InvalidTime                4001
# define CEC_InvalidTimeString          4002

# endif

# endif
