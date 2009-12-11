/*
 * zlprg.c
 *
 */
#include <termios.h>	// tc[gs]etattr()
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <time.h>	// nanosleep()

#include "zlprg.h"




// returns checksum of a code
static unsigned short checksum(struct zlprg_code *code)
{
 int            i;
 unsigned short sum;

 // compute checksum:
 for(i=0, sum=0; i<code->len; i++)
   sum+=code->byte[i];

 return sum;
} // checksum()




// convers single 'writen' hex to digit
static int hex2dig(char h)
{
 if('a'<=h)
   return h-'a'+10;
 if('A'<=h)
   return h-'A'+10;
 return h-'0';
} // hex2dig()




// makes uC name and params from it's type
static int uC_fill_by_type(struct zlprg_uC *uC)
{
 switch(uC->type)
 {
   case 0xff:	// none
        strcpy(uC->name, "<none>");
        uC->eeprom=-1;
        break;
   case 0xfe:	// ZL6PROG...
        strcpy(uC->name, "AT89C2051/4051");
        uC->eeprom=2048;	// C2051
        //uC->eeprom=4096;	// C4051
        break;
   case 0x51:	// at89C51
        strcpy(uC->name, "AT89C51");
        uC->eeprom=4096;
        break;
   default:	// ????
        strcpy(uC->name, "<unknown> (assuming: 2048B EEPROM)");
        uC->eeprom=2048;	// by default we use 2048
        break;
 }

 return 0;
}




// returns next byte from port
static int port_read_byte(struct zlprg *prg)
{
 unsigned char c;

 // read just one byte
 if( read(prg->fd, &c, 1)!=1 )
   return -1;
//fprintf(stderr, "got %3d = 0x%2X = '%c'\n", c,c,(c>30)?c:'.');

 return c;
} // port_read_byte()





// returns string from input, despiting ending "ok" and prompt
static int port_read_str(struct zlprg *prg, char buf[], int min, int max,
                         int skip_ctrl)
{
 int i;		// for()

 // let's read the data!
 for(i=0; i<max; i++)
 {
   char *tmp;	// temporary bufor pointer
   int   byte;	// recieved data

   byte  =port_read_byte(prg);
   buf[i]=byte;
   if(byte==-1)	// error?
     return -1;
   // if we need to skip some extra control chars
   if(skip_ctrl)
     if(byte==0x11 || byte==0x13)			// XON/XOFF?
     {
       i--;
       continue;
     }

   // seek for the end of transmition?
   // (after: "\n ok\n >")
   if( min<=i && 4<=i+1 /* && i<max*/)
   {
     tmp=buf+i-4+1;
     if( memcmp(tmp, "\n\n >", 4)==0 ||
         memcmp(tmp, "\n\r >", 4)==0    )	// end of data?
     {
       *tmp=0;			// we don't want that appendix

       // maby we could also remove leading "\n\r ok"?
       if(i>=5+4)
       {
         tmp-=5-1;
         if( memcmp(tmp, "\n\n ok", 5)==0 ||
             memcmp(tmp, "\n\r ok", 5)==0    )	// has also this part?
           *tmp=0;		// remove it as well!
       }
       break;
     }
   }
 } // for(bytes)

 buf[i]=0;			// let's make it string... :)

 return 0;
} // port_read_str()





// returns next byte from port
static int port_write_bytes(struct zlprg *prg, unsigned char c[], int len,
                            int xon, int xoff, int echo)
{
 int i;
 int x;		// XON/XOFF

 // writing...
 for(i=0; i<len; i++)
 {
   struct timespec dt={0,30*1000*1000};
   // XON control...
   if(xon)
   {
     x=port_read_byte(prg);
     if(x!=0x11 && x!=0x13)	// not XON/XOFF??
       return -3;
   } // if(xon)
   else
     nanosleep(&dt,NULL);
   // write
   if( write(prg->fd, c+i, 1)!=1 )
     return -1;
   // echo...
   if(echo)
   {
     x=port_read_byte(prg);
     if(x!=c[i])		// wrong echo?
       return -4;
   } // if(echo)
   // XOFF control...
   if(xoff)
   {
     x=port_read_byte(prg);
     if(x!=0x11 && x!=0x13)	// not XON/XOFF??
       return -2;
   } // if(xoff)
 } // for(data)

 return 0;
} // port_write_byte()





// writes string to port
static int port_write_str(struct zlprg *prg, char data[],
                          int xon, int xoff, int echo)
{
 // write data
//fprintf(stderr, "writing '%s'\n", data);
 return port_write_bytes(prg, (unsigned char*)data, strlen(data), xon, xoff, echo);
} // port_write()





//
// FULL COMMANDS
//

static int cmd_set_size(struct zlprg *prg, int size)
{
 char buf[32];		// send buffer

 // send request
 if( port_write_str(prg, "s", 0,0, 1)!=0 )
   return -3;

 // set size to write
 sprintf(buf, "%d\n", size);			// sets the size
 if( port_write_str(prg, buf, 1,0, 1)!=0 )
   return -1;

 // finish...
 if( port_read_str(prg, buf, 0, 32-1, 1)!=0 )	// get empty answer
   return -2;

 return 0;
} // cmd_set_size()




static int cmd_erase(struct zlprg *prg)
{
 char buf[32];

 // erase
 if( port_write_str(prg, "e", 0,0, 1)!=0 )		// erase uC
   return -1;
 // get confirmation
 if( port_read_str(prg, buf, 0, 32-1, 1)!=0 )		// "ok"
   return -2;

 return 0;
} // cmd_erase()




static int cmd_write(struct zlprg *prg, char b[], int len)
{
 char buf[32];

 // write data itself
 if( port_write_str(prg, "w", 0,0, 1)!=0 )	// write request to uC
   return -1;

// ???????????????????????????????
// int i;
// for(i=0; i<len; i++)
//   b[i]=(b[i]+0x80)%256;
 // start writing data
 if( port_write_bytes(prg, (unsigned char*)b, len, 1,1, 0)!=0 )
   return -2;
// ?????????????????????????????????
// for(i=0; i<len; i++)
//   b[i]=(b[i]+0x80)%256;

 // "ok >"
 if( port_read_str(prg, buf, 0, 32-1, 1)!=0 )	// get empty answer
   return -3;

 return 0;
} // cmd_write()




static int cmd_read(struct zlprg *prg, char b[], int len)
{
 char buf[32];		// I/O buf
 int  i;		// for()

 // read data request
 if( port_write_str(prg, "r", 0,0, 1)!=0 )	// read from uC
   return -1;

 // read in HEX
 for(i=0; i<len; i++)
 {
   buf[1]=port_read_byte(prg);
   buf[0]=port_read_byte(prg);
   if(buf[0]<0 || buf[1]<0)
     return -2;
   // convert 2xHEX to BYTE
   b[i]  =hex2dig(buf[1])*16 + hex2dig(buf[0]);
//   b[i]  =(b[i]+0x80)%256
 } // for(HEX)

 // skip regular output "\n ok\n >"
 if( port_read_str(prg, buf, 0, 31-1, 1)!=0 )
   return -3;

 return 0;
} // cmd_write()




// sets the parameters of uC in writer
static int cmd_params(struct zlprg *prg, struct zlprg_uC *uC)
{
 char buf[32];

 if(prg->has_params)
 {
   // get info on uC
   if( port_write_str(prg, "p", 0,0, 1)!=0 )	// get info
     return -1;
   if( port_read_str(prg, buf, 0, 32-1, 1)!=0 )	// get answer!
     return -2;

   // read data from string
   sscanf(buf, "%2X,%d,%d", &uC->type, &uC->non_blank, &uC->bytes);
 }
 else		// ZL6PRG?
 {
   uC->type     =0xfe;
   uC->non_blank=-1;
   uC->bytes    =-1;
 }

 // and decode uC name
 if( uC_fill_by_type(uC)!=0 )
   return -3;

 return 0;
} // cmd_params()




// read's the writer's intro string
static int cmd_intro(struct zlprg *prg, char intro[])
{
 // write the request
 if( port_write_str(prg, "\n", 0,0, 0)!=0 )
   return -1;
 // get the answer as name
 if( port_read_str(prg, intro, 0, NAME_MAX, 1)!=0 )
   return -2;

 // we need to remove leading "\n\n\r " separetly
 memmove(intro, intro+4, strlen(intro+4)+1 );

 // does we have to deal with ZL6PROG?
 prg->has_params=( strstr(intro, "89C2051/4051")!=NULL )?0:1;

 return 0;
} // cmd_intro()





//
// FUNCTIONS FOR THE OUTSIDE
//



int zlprg_init(struct zlprg *prg, char dev[])
{
 // open port
 prg->fd=open(dev, O_RDWR|O_NOCTTY/*|O_NDELAY*/);
 //prg->fd=open(dev, O_RDWR|O_NOCTTY|O_NDELAY);
 if(prg->fd==-1)
   return -1;

 // get current port settings
 if( tcgetattr(prg->fd, &prg->t_prev)!=0 )
 {
   close(prg->fd);
   prg->fd=-1;
   return -2;
 }

 // make copy of recieved settings
 prg->t_now=prg->t_prev;

 // set baudrate to 9600
 if( cfsetispeed(&prg->t_now, B9600)!=0 ||
     cfsetospeed(&prg->t_now, B9600)!=0    )
 {
   close(prg->fd);
   prg->fd=-1;
   return -3;
 }



 // No parity (8N1):
/* prg->t_now.c_cflag&=~PARENB;
 prg->t_now.c_cflag&=~CSTOPB;
 prg->t_now.c_cflag&=~CSIZE;
 prg->t_now.c_cflag|=CS8;
 // enable reciever and don't change "owner" of the port
 prg->t_now.c_cflag|=CREAD|CLOCAL;
 // disable hardware flow control
 //prg->t_now.c_cflag&=~CNEW_RTSCTS;
 //prg->t_now.c_cflag&=~CRTSCTS;
 // enable hardware flow control
 //prg->t_now.c_cflag|=CRTSCTS;
 // raw input
 prg->t_now.c_lflag&=~(ICANON|ECHO|ECHOE|ISIG);
 // raw output
 prg->t_now.c_oflag&=~OPOST;*/

 prg->t_now.c_iflag&=~IGNBRK;
 prg->t_now.c_iflag&=~IGNPAR;
 prg->t_now.c_iflag&=~PARMRK;
 prg->t_now.c_iflag&=~INPCK;
 prg->t_now.c_iflag&=~ISTRIP;
 prg->t_now.c_iflag&=~INLCR;
 prg->t_now.c_iflag&=~IGNCR;
 prg->t_now.c_iflag&=~ICRNL;
 prg->t_now.c_iflag&=~IUCLC;
 prg->t_now.c_iflag&=~IXON;
 prg->t_now.c_iflag&=~IXANY;
 prg->t_now.c_iflag&=~IXOFF;
 prg->t_now.c_iflag&=~IMAXBEL;

 prg->t_now.c_oflag&=~OPOST;
 prg->t_now.c_oflag&=~OLCUC;
 prg->t_now.c_oflag&=~ONLCR;
 prg->t_now.c_oflag&=~OCRNL;
 prg->t_now.c_oflag&=~ONOCR;
 prg->t_now.c_oflag&=~ONLRET;
 prg->t_now.c_oflag&=~OFILL;
 prg->t_now.c_oflag&=~OFDEL;
 prg->t_now.c_oflag&=~NLDLY;
 prg->t_now.c_oflag&=~CRDLY;
 prg->t_now.c_oflag&=~TABDLY;
 prg->t_now.c_oflag&=~BSDLY;
 prg->t_now.c_oflag&=~VTDLY;
 prg->t_now.c_oflag&=~FFDLY;

 prg->t_now.c_cflag&=~PARENB;
 prg->t_now.c_cflag&=~CSTOPB;
 prg->t_now.c_cflag&=~CSIZE;
 prg->t_now.c_cflag|=CS8;
 prg->t_now.c_cflag|=CREAD;
 prg->t_now.c_cflag|=CLOCAL;
 prg->t_now.c_cflag&=~PARODD;
 prg->t_now.c_cflag&=~CRTSCTS;


 prg->t_now.c_lflag&=~ISIG;
 prg->t_now.c_lflag&=~ICANON;
 prg->t_now.c_lflag&=~XCASE;
 prg->t_now.c_lflag&=~ECHO;
 prg->t_now.c_lflag&=~ECHOE;
 prg->t_now.c_lflag&=~ECHOK;
 prg->t_now.c_lflag&=~ECHONL;
 prg->t_now.c_lflag&=~ECHOCTL;
 prg->t_now.c_lflag&=~ECHOPRT;
 prg->t_now.c_lflag&=~ECHOKE;
 //prg->t_now.c_lflag&=~DEFECHO;
 prg->t_now.c_lflag|=FLUSHO;
 prg->t_now.c_lflag&=~NOFLSH;
 prg->t_now.c_lflag&=~PENDIN;
 prg->t_now.c_lflag&=~IEXTEN;


 // write new settings to port
 if( tcsetattr(prg->fd, TCSANOW, &prg->t_now)!=0 )
 {
   close(prg->fd);
   prg->fd=-1;
   return -4;
 }

 // read writer's into message
 if( cmd_intro(prg, prg->name)!=0 )
   return -5;

 return 0;
} // zlprg_init()




// to write code we need:
//  'sNUMB\n' 'p' + 'e' + 'w' + data...
int zlprg_write(struct zlprg *prg, struct zlprg_code *code)
{
 struct zlprg_uC uC;		// uC info

 // erase uC
 if( cmd_erase(prg)!=0 )
   return -1;

 // set size to write
 if( cmd_set_size(prg, code->len)!=0 )
   return -2;

 // get info on uC
 if( cmd_params(prg, &uC)!=0 )
   return -3;
 if( !has_uC(&uC) )		// no uC in writer?
   return -4;

 // now we do the writing itself
 if( cmd_write(prg, code->byte, code->len)!=0 )	// writing data
   return -5;

// TODO - checksum

 return 0;
} // zlprg_write()





int zlprg_read(struct zlprg *prg, struct zlprg_code *code, int size)
{
 struct zlprg_uC uC;	// uC info

 if( cmd_params(prg, &uC)!=0 )
   return -1;
 if( !has_uC(&uC) )		// no uC in writer?
   return -2;

 if(size<0)		// auto-size?
   size=uC.eeprom;	// set size depending no uC
 if(size>uC.eeprom)	// too big request?
   return -3;

 code->len=size;	// we may need this later :)

 // set data len to read
 if( cmd_set_size(prg, code->len)!=0 )
   return -4;

 // do the reading
 if( cmd_read(prg, code->byte, code->len)!=0 )
   return -5;

 // compute checksum
 code->checksum=checksum(code);

 return 0;
} // zlprg_read()




int zlprg_verify(struct zlprg *prg, struct zlprg_code *code)
{
 struct zlprg_code code_uC;	// code from uC

 // read code in uC
 if( zlprg_read(prg, &code_uC, code->len)!=0 )
   return -1;

 // compare given parts of code
 if( memcmp(code->byte, code_uC.byte, code->len)!=0 )
   return -2;

 // test checksums
 if( code_uC.checksum!=code->checksum )
   return -3;

 return 0;
} // zlprg_verify()




int zlprg_close(struct zlprg *prg)
{
 // write back oryginal port settings
 if( tcsetattr(prg->fd, TCSANOW, &prg->t_prev)!=0 )
 {
   // ??? :/
 }

 // we don't need this file anymore
 close(prg->fd);
 prg->fd=-1;

 return 0;
} // zlprg_close()




int zlprg_uC(struct zlprg *prg, struct zlprg_uC *uC)
{
 if( cmd_params(prg, uC)!=0 )
   return -1;
 if( !has_uC(uC) )		// no uC in writer?
   return -2;

 return 0;
} // zlprg_uC()




int zlprg_file_write_bin(struct zlprg_code *code, char file[])
{
 int fd;		// output file

 // open file
 fd=open(file, O_WRONLY|O_CREAT|O_TRUNC, 0644);
 if(fd==-1)
   return -1;

 // write raw data
 if( write(fd, code->byte, code->len)!=code->len )
 {
   close(fd);
   return -2;
 }

 // close file
 close(fd);

 return 0;
} // zlprg_file_write_bin()




int zlprg_file_read_bin(struct zlprg_code *code, char file[])
{
 int         fd;	// file to read from
 struct stat s;		// file stats

 // open file
 fd=open(file, O_RDONLY);
 if(fd==-1)
   return -1;

 // get & check file size
 fstat(fd, &s);		// get file stats
 if(s.st_size>CODE_MAX)	// file is too big?
 {
   close(fd);
   return -2;
 }

 // read raw data from file
 if( read(fd, code->byte, s.st_size)!=s.st_size )
 {
   close(fd);
   return -3;
 }
 code->len     =s.st_size;	// set proper code size
 code->checksum=checksum(code);	// and set checksum

 // close the file
 close(fd);

 return 0;
} // zlprg_file_read_bin()


