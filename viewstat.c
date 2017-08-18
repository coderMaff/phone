/* Phone log stats reader

	MRB ??.??.97 Created
	MRB 03.11.97 Changed it so you must specify a datafile name

*/

#include <stdio.h>

#define     CHAR    char
#define     WORD    short
#define     UWORD   unsigned short
#define     BOOL    short
#define     LONG    long
#define     ULONG   unsigned long
#define     FAST	register int
#define     VOID    void

typedef	    CHAR *	STRPTR;					
typedef     VOID *  APTR;

#define     FALSE   0
#define     TRUE    1
#define     AND     &&
#define     OR      ||

#define     XON     17
#define     XOFF    19

typedef struct
{
    ULONG seconds_in;
    ULONG seconds_out;
    unsigned int callsin;
    unsigned int callsout;
} ext;

ext     exten[9];
char    date[25];
FILE    *fp;

void callreport(int n)
{
    int  sec = 0;
    int  min = 0;
    int  hour = 0;

    printf("22%d : %05d : %05d : %05d :",
            n+1,
            exten[n].callsin,
            exten[n].callsout,
            ( exten[n].callsin + exten[n].callsout)); 

    sec = exten[n].seconds_in % 60;
    min = exten[n].seconds_in / 60;
    hour = min / 60;
    min = min % 60;

    printf(" %02d:%02d:%02d :",hour,min,sec);

    sec  = exten[n].seconds_out % 60;
    min  = exten[n].seconds_out / 60;
    hour = min / 60;
    min  = min % 60;

    printf(" %02d:%02d:%02d :",hour,min,sec);

    sec  = ( exten[n].seconds_out + exten[n].seconds_in ) % 60;
    min  = ( exten[n].seconds_out + exten[n].seconds_in ) / 60;
    hour = min / 60;
    min  = min % 60;

    printf(" %02d:%02d:%02d\n",hour,min,sec);
}

int main(int argc, char **argv)
{
    int n;

	if ( argv[1] == NULL )
	{
		printf("please specify a filename\n");
		return 1;
	}
 
    fp = fopen( argv[1], "rb");

    if (!fp)
    {
        printf("Cant open %s\n", argv[1]);
        return 1;
    }

    fread(exten, 1, sizeof( exten ), fp );
    fread(date, 1, sizeof( date ), fp );

    fclose( fp );

	printf("View Stats v1.0.1 - 03.11.97 File : %s\n",argv[1]); 
    printf("Stats from %s",date);

    for( n = 0; n<9; n++ )
        callreport( n );
}


     



