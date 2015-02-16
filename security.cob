       Identification Division.
       Program-id. "security".
      *> cobc -x -free -fintrinsics=all jssecurity.cob
       Environment Division.
       Data Division.
       Working-storage Section.
        	1 IO_STREAM		pic x(2048) value spaces.
        	1 Username		Pic X(32) Value spaces.
        	1 Password		Pic X(32) Value spaces.
        	1 Passed		Pic 9(9) Comp Value 1.
        	1 Failed		Pic 9(9) Comp Value 0.
       Procedure Division.

        	Call "sleep" using 10.
        	Move Spaces to IO_STREAM.
        	Display "Username? ".
         	Accept IO_STREAM.
        	Move trim(IO_STREAM) To Username.
        	Move Space to IO_STREAM.
        	Display "Password? ".
        	Accept IO_STREAM.
        	Move trim(IO_STREAM) to Password.
         	
        	If Username = "michael" and Password = "asdqwe123"
        		Display "Call jssecurity_login using Passed"
        	Else
        		Display "Call jssecurity_login Using Failed".
                goback.

