# ifndef FILESYSTEM_ERRORCODES_H_
# define FILESYSTEM_ERRORCODES_H_

//
//	Error Codes for Configuration Set Commands
//	Code values are in [4096:4351]
//

//	Generic file errors	5001-5099
# define CEC_FileNotPresent             5001
# define CEC_CannotOpenFile				5002
# define CEC_FileExists                 5003
# define CEC_FileNameInvalid			5004
# define CEC_FileNameMissing			5005
# define CEC_Fsys_Failed				5011
# define CEC_ZFile_Invalid				5021
# define CEC_InvalidCalFileLetter       5031
# define CEC_CalFileInvalid				5032

//	List related errors 5101-5199
# define CEC_CmdListUnknown             5101

//	Receive related errors 5201-5299
# define CEC_CmdRcvUnknown              5201
# define CEC_RcvFailed                  5204
# define CEC_RcvTimeout                 5206
# define CEC_RcvCancelled				5207

//	Send related errors 5301-:wa
//
# define CEC_CmdSndUnknown				5301
# define CEC_SndFailed                  5304
# define CEC_SndTimeout                 5306
# define CEC_SndCancelled               5307

//	Output related errors 5401-5449
# define CEC_CmdDspUnknown				5401

//	Output related errors 5451-5499
# define CEC_CmdCRCUnknown				5451

//	Delete related errors 5501-5599
# define CEC_CmdDelUnknown				5501
# define CEC_CmdDelNotImplemented		5502
# define CEC_CannotDeleteActiveCalFile	5511

# endif
