Identification Division.
Program-id. "application".
      *> cobc -x -free -fintrinsics=all application.cob
Environment Division.
Data Division.
Working-storage Section.
	1 IO_STREAM		pic x(2048) value spaces.
	1 iocmd			pic x(2048) value spaces.
	1 ssnidx		Pic s9(4) comp value 0.
	1 x			Pic s9(4) comp value 0.
	1 disp			Pic zzz9.
	1 Hell-Freezes-Over-Sw	Pic 9 Value 0.
		88 Hell-Freezes-Over    value 1, false 0.
Procedure Division.
	Perform Varying ssnidx from 1 by 1 until Hell-Freezes-Over
		accept IO_STREAM
		if lower-case(IO_STREAM(1:4)) = "exit"
			Set Hell-Freezes-Over to true
		else
			move ssnidx to disp
			Display trim(disp) 
			Display "[" 
			Display reverse(trim(IO_STREAM)) 
			Display "]" 
		end-if
	end-perform.
	goback.
