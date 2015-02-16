#include	"unp.h"
static int	read_cnt;
static char	*read_ptr;
static char	read_buf[MAXLINE];
/* ************************************************************** */
static ssize_t Actual_Read(int fd, char *ptr)
{
/*	Actual_Read will read 0 to MAXLINE bytes, or return error -1
	If error -1, then we return, AKA: we're done reading.
	Once the read succeeds then read_cnt contains the number of bytes that were read.
	Each call thereafter to Actual_Read doesn't call 'read', because read_cnt is positive.
	So while read_cnt is positive Actual_Read will increment *read_ptr, char by char, while
	decrementing read_cnt, until read_cnt is zero returning the char at *read_ptr to GetNextLine.

	'GetNextLine' will continue calling Actual_Read 
	until maxlen is reached, or *read_ptr points to a new line '\n' character.

	Actual_Read should be immune to signal interruption.
*/ 
	if (read_cnt <= 0) { // TRUE on first call?
again:
		if ( (read_cnt = read(fd, read_buf, sizeof(read_buf))) < 0) {
			if (errno == EINTR) // The call was interrupted by a signal before any data was read;
				goto again;	    // Go read again. 
				
			return(-1); // read error, and errno is set appropriately. 
		} else if (read_cnt == 0)
			return(0); // End of File
		// read_cnt is > 0
		read_ptr = read_buf; // Update address of read pointer. 
	}
	read_cnt--;
	*ptr = *read_ptr++; // Set pointer to read pointer and advance the read pointer by 1 character.
	return(1);
}
/* ************************************************************** */
ssize_t GetNextLine(int fd, void *vptr, size_t maxlen)
{
	ssize_t	n, rc;
	char	c, *ptr;
	ptr = vptr;
	for (n = 1; n < maxlen; n++) {
		if ( (rc = Actual_Read(fd, &c)) == 1) {
			*ptr++ = c;
			if (c == '\n')
				break;	/* newline is stored, like fgets() */
		} else if (rc == 0) {
			*ptr = 0;
			return(n - 1);	/* EOF, n - 1 bytes were read */
		} else
			return(-1);		/* error, errno set by read() */
	}
	*ptr = 0;	/* null terminate like fgets() */
	return(n);
}

/* **************************Readline**************************** */
/*                   E N T R Y    P O I N T                       */
/*                 ReadLine calls GetNextLine                     */
/*                GetNextLine calls ActualRead                    */
/*    GetNextLine will not return until a newline (/n) is read    */
/* ************************************************************** */
ssize_t Readline(int fd, void *ptr, size_t maxlen)
{
	ssize_t		n;
	if ( (n = GetNextLine(fd, ptr, maxlen)) < 0)
		syslog(LOG_ERR,"GetNextLine error");
	return(n);
}
