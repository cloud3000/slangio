Identification Division.
Program-id. "security".
	*> ================   Techtonics  ===============
	*> cobc -x -free -fintrinsics=all security.cob
	*> cp -p ./security /volume1/applications/appmain
	*> ==============================================
Environment Division.
Data Division.
Working-storage Section.
	1 IO_STREAM		pic x(2048) value spaces.
	1 Username		Pic X(32) Value spaces.
	1 Password		Pic X(32) Value spaces.
	1 Passed		Pic 9(9) Comp Value 1.
	1 Failed		Pic 9(9) Comp Value 0.
Procedure Division.
	Move Spaces 			to IO_STREAM.
	Display "Username? ".
	Accept IO_STREAM.
	Move trim(IO_STREAM) 	to Username.
	Move Spaces 			to IO_STREAM.
	Display "Password? ".
	Accept IO_STREAM.
	Move trim(IO_STREAM) 	to Password.

	If Username = "michael" and Password = "asdqwe123"
		Display "Call jssecurity_login using Passed"
	Else
		Display "Call jssecurity_login Using Failed".
		
    Move low-values to IO_STREAM.
    String "echo Username=[" trim(Username) "] Password=[" trim(Password) 
           "] > /home/j3k/secdata.txt"
    	Delimited By Size Into IO_STREAM.
    	
    call "system" using IO_STREAM.
	goback.
