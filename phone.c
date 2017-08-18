/*****************************************************************************

Name    :   Phone.c
Purpose :   Display and store data from phone system, via serial port
Date    :   13/05/97 11:09
Version :   v1.1.8
By      :   Matthew Bushell

History

MRB 1.0.0   13/05/97 11:09 Created
MRB 1.1.0   13/05/97 15:30 Added Report, displays # incoming / outgoing calls
                           Central Memory, Recognises and displays it.
MRB 1.1.1   15/05/97 15:00 Totals up time each extension has been on the phone
MRB 1.1.2   19/05/97 16:30 Fixed Total time bug, and IN calls heading.
                           Added Close and Open port options.
MRB 1.1.3   21/05/97 14:41 Fixed bug in displaying minutes, rewrote reporting
                           to use callreport(); Some Optimization.
                           Added save and load stats options.
MRB 1.1.4   22/05/97 14:45 Loads data on startup.
MRB 1.1.5   27/05/97 10:18 Fixed bug that occasionally lets the program 
                           display junk.
MRB 1.1.6   29/05/97 09:41 Fixed load/save bug.  Saves stats after each call
MRB 1.1.7   06/06/97 16:02 Fixed array bug. Keeps a record of when the stats 
                           are from.
MRB 1.1.8   11/06/97 15:35 Added border color change. Blue during i/o and 
                           red for i/o error.

*****************************************************************************/

#include <conio.h>
#include <dos.h>
#include <int.h>
#include <time.h>
#include <stdio.h>
#include <string.h>

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

/*[ Globals ]*/

UWORD   Port = 0;

#define     bufsize 10240
            BOOL    fCommActive = FALSE;
            
volatile    CHAR    asc_buf[bufsize];
volatile    UWORD   asc_in  = 0;
            UWORD   asc_out = 0;
            
            WORD    pic_mask    = 0x21;     // 8259 interrupt-mask port 
            WORD    pic_eoi     = 0x20;     // 8259 end-of-interrupt port

            // Port numbers etc... for serial I/O chip

            WORD    com_data[4] = { 0x3F8,	0x2F8,	0x3E8,	0x2E8	};      // I/O address	
            WORD    com_ier[4]  = { 0x3F9,	0x2F9,	0x3E9,	0x2E9	};
            WORD    com_mcr[4]  = { 0x3FC,	0x2FC,	0x3EC,	0x2EC	};
            WORD    com_sts[4]  = { 0x3FD,	0x2FD,	0x3ED,	0x2ED	};
            WORD    com_int[4]  = { 0x0C,	0x0B,	0x0C,	0x0B	};      // interrupt number 
            WORD    int_mask[4] = { 0x10,	0x08,	0x10,	0x08	};      // IRQ Mask 

volatile    BOOL    TxStopped = FALSE;

            ULONG   TxTimeOut = 40;

            ext     exten[9];

            time_t  ntime;
            char    date[25];

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

void border( int n )
{

    union   REGS regz;

    regz.h.ah = 0x0B;
    regz.h.bh = 0x00;
    regz.h.bl = n;

    int86( 0x10, &regz, &regz );
}


/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

int IntHandler( struct INT_DATA *NotWanted )
{
    char c;

	c = inp( com_data[Port] );

	if (c == XON)
	{
		TxStopped = FALSE;
	} else
	if (c == XOFF)
	{
		TxStopped = TRUE;
	}else
	{
		asc_buf[asc_in] = c & 0x7F; /* Lose parity bit */

		asc_in++;

		if (asc_in == bufsize) asc_in = 0;
	}

	outp( pic_eoi, 0x20 ); /* Tell 8259 that it's ok for next interrupt */

	return 1;
}

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

WORD OpenCommPort ( UWORD Speed )

{
	union REGS		REGS, Junk;
	WORD			SpeedBits;

	if ((Port < 0) || (Port > 3)) return 14; /*unknown unit*/

	/* Now a crappy BIOS call to INT 14H,
	   Function 00H Initialise comm port */

	REGS.h.ah = 00;

	SpeedBits = 5;	/* 2400 baud */

	if (Speed == 9600) SpeedBits = 7;

	REGS.h.al = ( SpeedBits << 5 );	/* bits 765 */

	/* really want 7 bit with parity 0, but only PS2 does it */

	REGS.h.al = REGS.h.al | 3;	/* 8 bit data,
								   no parity
								   1 stop bit */

	REGS.x.dx = Port;

	int86( 0x14, &REGS, &Junk );

	{
		WORD		Mask;

		int_intercept( com_int[Port], IntHandler, 0 );

		outp( com_mcr[Port], 0x0b ); /* Modem Control reg DTR RTS & OUT2 */
		outp( com_ier[Port], 0x01 ); /* Interrupt enable reg */

		/*	Now read PIC mask & set something in it.
			This turns on the serial port interrupts.	*/

		Mask = inp( pic_mask );
		Mask = Mask & (~int_mask[Port]); /* ~ is binary NOT */
		outp( pic_mask, Mask );

		fCommActive = TRUE;
	}

	asc_in		= 0;
	asc_out		= 0;
	TxStopped	= FALSE;

	return 0; /* can't really tell if it went wrong. */

}

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

void CloseCommPort()

{
	WORD	Mask;


	if (fCommActive)
	{

		/*	Now read PIC mask & put it back the way it was.
			This will turn serial port interrupts off */

		Mask = inp( pic_mask );
		Mask = Mask | int_mask[Port];
		outp( pic_mask, Mask );

		outp( com_mcr[Port], 0x00 ); /* Modem Control reg */

		outp( com_ier[Port], 0x00 ); /* Interrupt enable reg */

		int_restore( com_int[Port] );

		fCommActive = FALSE;

        printf("Port %d closed\n",Port);
	}
    else
        printf("No Ports open\n");    
}

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

WORD ReadFromComm( CHAR *DataRet )

{

	UWORD	Erc = 0;

	if ( asc_in != asc_out )
	{
		DataRet[0] = asc_buf[asc_out];

		asc_out++;

		if  (asc_out == bufsize) asc_out = 0;

	} else Erc = 7;

	return Erc;
}

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
int savedata()
{
 
    FILE    *fp;

    border( 1 );

    fp = fopen("STATS.DAT","wb+");

    if (!fp)
    {
        printf("Cant create stats.dat\n");
        border( 4 );
        return 1;
     }

    fwrite(exten, 1, sizeof( exten ), fp );
    fwrite(date, 1, sizeof( date ), fp );

    fclose( fp );

    border( 0 );

    return 0;
}

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
int loaddata()
{
    FILE    *fp;

    fp = fopen("STATS.DAT","rb");

    border( 1 );

    if (!fp)
    {
        printf("Cant open stats.dat\n");
        time( &ntime );
        sprintf(date,"%s",ctime( &ntime ));
        border( 4 );
        return 1;
    }

    fread(exten, 1, sizeof( exten ), fp );
    fread(date, 1, sizeof( date ), fp );

    fclose( fp );

    printf("Data Loaded\n");

    border( 0 );

    return 0;
}

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


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

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

int dooption()
{
    char    key = 0;
    int     n = 0;

    printf("==============================================================================\n");
    printf("Options\n");
    printf("==============================================================================\n");
    printf("SPACE = Return to monitoring\n");
    printf("C/c   = Close port\n");
    printf("0..1  = Open port (x)\n");
    printf("Q/q   = Quit\n");
    printf("R/r   = Show report\n");
    printf("S/s   = Save call data\n");
    printf("L/l   = Load call data\n");
    printf("==============================================================================\n");
    printf("Please make your choice:");

    key = getch();

    printf("%c\n",key);

    if ( ( key=='q' ) OR ( key=='Q' ) )        // Q = Quit
        return 1;

    if ( ( key=='c' ) OR ( key=='C' ) )        // C = Close Comm Port
        CloseCommPort();

    if ( ( key=='s' ) OR ( key=='S' ) )
    {
        if ( !savedata() )
            printf("Data saved\n");
          
    }

    if ( ( key=='l' ) OR ( key=='L' ) )
        loaddata();

    if ( ( key=='0' ) OR ( key=='1' ) )
    {
        if (!fCommActive)
        {
            Port = ( key - '0' );
            OpenCommPort(9600);
            printf("Monitoring Port %d\n",Port);
        }
        else
            printf("Port %d is already open\n",Port);
    }

    if ( ( key=='r' ) OR ( key=='R' ) )        // R = Report
    {
        printf("==============================================================================\n");
        printf("Report - Data file begun %s",date);
        printf("==============================================================================\n");
        printf("Monitoring Port %d\n",Port);
        printf("EXT   IN      OUT     TOTL    TIME IN    TIME OUT   TIME\n"); 
        for( n = 0; n < 9; n++ )
            callreport(n);
        
    }

    printf("==============================================================================\n");
    printf("DATE          TIME     TK NUMBER DIALLED     ACCNT DURATION MET COST   EXT OPR\n");

    return 0;
}

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

int check_kb_buf(void)
{
    union REGS		REGS, Ret;

    REGS.h.ah = 0xb;
    
    int86( 0x21, &REGS, &Ret );

    return Ret.h.al;
}

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

void process( STRPTR S )
{
    FILE    *fp;
    int     extension;
    int     char1;
    int     char2;

    extension = ( S[73] - '0' );

    extension --;

    if ( ( S[26] == 'I' ) AND ( S[27] == 'N' ) )   // Incoming call?
    {
        exten[extension].callsin++;

        char1 = ( S[57] - '0' );
        char2 = ( S[58] - '0' );
        exten[extension].seconds_in += ( ( char1*10 ) + char2 );

        char1 = ( S[54] - '0' );
        char2 = ( S[55] - '0' );
        exten[extension].seconds_in += ( ( ( char1*10 ) + char2 ) * 60 );

        char1 = ( S[51] - '0' );
        char2 = ( S[52] - '0' );
        exten[extension].seconds_in += ( ( ( char1*10 ) + char2 ) * 3600 );
    }
    else
    {
        exten[extension].callsout++;

        char1 = ( S[57] - '0' );
        char2 = ( S[58] - '0' );
        exten[extension].seconds_out += ( ( char1*10 ) + char2 );

        char1 = ( S[54] - '0' );
        char2 = ( S[55] - '0' );
        exten[extension].seconds_out += ( ( ( char1*10 ) + char2 ) * 60 );

        char1 = ( S[51] - '0' );
        char2 = ( S[52] - '0' );
        exten[extension].seconds_out += ( ( ( char1*10 ) + char2 ) * 3600 );
    }
    
    S[79] = 0; // Null terminate string, just incase.

    puts(S);

    border( 1 );

    fp = fopen("Phone.dat","a");

    if (!fp )
    {
        printf("Cant open/create datafile\n");
        border( 4 );
    }
    else
    {
        fwrite(S, 79, 1, fp );
        fwrite("\n", 1, 1, fp );

        fclose( fp );
    }

    border( 0 );

    savedata();
}

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


int main(int argc, char **argv)
{

    char    DataRet;
    char    S[80];
    int     i = 0;
    char    Erc = 0;
    char    quit = 0;
    int     n = 0;

    if ( argv[1][0] == '1' )
        Port = 1;
    else
        Port = 0;

    memset( &exten, 0, sizeof( exten ) );
    memset( &date, 0, sizeof( date ) );

    border( 0 );

/*[ debug code 

    printf("SIZEOF( exten ) = %d",sizeof( exten ));

    exten[3].seconds_in = 5721;
    exten[3].seconds_out = 1520;
    exten[3].callsin = 345;
    exten[3].callsout = 24;

debug code ]*/
        
    printf("Phone Monitor v1.1.8 (C)1997 Chips International Ltd  -  11.06.97 - Com Port %d\n",Port);
    printf("==============================================================================\n");
    loaddata();
    printf("==============================================================================\n");
    printf("DATE          TIME     TK NUMBER DIALLED     ACCNT DURATION MET COST   EXT OPR\n");

    OpenCommPort(9600);

    while(!quit)
    {

	    do
        {

            Erc = ReadFromComm( &DataRet );
    
            if (check_kb_buf())
            {
                getch();
                quit = dooption();
                if ( quit )
                    break;
            }

        } while (Erc != 0);

		if ( DataRet >= 32 ) 
        {
            i++;
    	    S[i] = DataRet;

        }

        if( ((DataRet < 14) AND (i > 0)) OR (i > 77) )
        {
            if ( (S[0]=='C') AND (S[1]=='e') )                  // If central dials 1952 :-
            {
                printf("==============================================================================\n");
                printf("Central Memory Report\n");
                printf("==============================================================================\n");

                do
                {     
                    Erc = ReadFromComm( &DataRet );
                    if (( DataRet >= 32 ) AND ( Erc == 0 ))
                    {
                        n++;
                        if ( DataRet != 'D' )
                            printf("%c",DataRet );
                        if ( n>51 )
                        {
                            n = 0;
                            printf("\n");
                        }
                    }
   
                }while ( DataRet != 'D' );
            
                printf("==============================================================================\n");
            }
            else
            	if ( (i == 77) AND (S[0] >= 'A') AND (S[0] <= 'Z') )     // If a valid call in/out happens :-
                    process( S );
		
			memset( S, 32, sizeof(S) );

    		i = -1;
        }
    }

    CloseCommPort();

    return 0;
}


            
     




