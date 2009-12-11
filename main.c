/*
 * main.c
 *
 * ezdlin is a linux (console) version of original
 * ezdlX.exe for windows. it uses *.bin files as
 * input/output and given /dev/file for port I/O (you
 * need to have permissions to R/W this file).
 *
 */
#include <stdio.h>	// I/O
#include <unistd.h>	// exit()
#include <stdlib.h>	// exit()
#include <string.h>	// strcmp()

#include "zlprg.h"	// zlprg_*()



// prints help on stderr and exit's with '1' return code
void fprintf_help(char pname[])
{
 fprintf(stderr, "%s <dev_file> {-r|-w|-W} <in_out_file_name.bin>\n", pname);
 fprintf(stderr, "\n\
description:\n\
  <dev_file> => serial port\n\
  -r         => read from uC\n\
  -w         => write to uC\n\
  -W         => as -w, but also veryfies\n");
 exit(1);
}


// halts execution with given message and return code
void halt_prg(const char *pname, const char *msg, int rc)
{
 fprintf(stderr, "%s: %s\n", pname, msg);
 exit(rc);
}


//
// MAIN
//
int main(int argc, char *argv[])
{
 char              rw_mode;	// odczyt czy zapis?
 struct zlprg      prg;		// writer
 struct zlprg_code code;	// code to R/W
 struct zlprg_uC   uC;		// uC info

 // program executed is it should be?
 if(argc!=4)
   fprintf_help(argv[0]);

 // Read or Write mode?
 if( strcmp(argv[2], "-r")==0 )
   rw_mode='r';
 else
   if( strcmp(argv[2], "-w")==0 )
     rw_mode='w';
   else
     if( strcmp(argv[2], "-W")==0 )
       rw_mode='W';
     else
       fprintf_help(argv[0]);

 // initialize port
 fprintf(stderr, "connecting to device...\n");
 if( zlprg_init(&prg, argv[1])!=0 )
   halt_prg(argv[0], "zlprg_init() failed to open port", 20);
 printf("%s\n", prg.name);

 // get uC info
 if( zlprg_uC(&prg, &uC)!=0 )
   halt_prg(argv[0], "zlprg_uC() failed to read uC info/no uC", 23);
 printf("installed uC: %s (type 0x%02X) with %dB (0x%04X) EEPROM\n",
         uC.name, uC.type, uC.eeprom, uC.eeprom);

 //
 // what to do?
 //
 switch(rw_mode)
 {
   case 'W':	// WRITE DATA TO uC AND VERIFY
   case 'w':	// WRITE DATA TO uC
        printf("program -> uC\n");
        // read program from file
        if( zlprg_file_read_bin(&code, argv[3])!=0 )
          halt_prg(argv[0], "zlprg_file_read_bin() failed", 30);
        printf("[ %d Bytes ]\n", code.len);
        // write it to uC
        if( zlprg_write(&prg, &code)!=0 )
          halt_prg(argv[0], "zlprg_write() failed", 40);
        // check whether veryfing will pass
        if(rw_mode=='W')
        {
          printf("verifying...\n");
          if( zlprg_verify(&prg, &code)!=0 )
            halt_prg(argv[0], "zlprg_verify() failed", 50);
        }
        break;

   case 'r':	// READ DATA FROM uC
        printf("uC -> program\n");
        if( zlprg_read(&prg, &code, -1)!=0 )
          halt_prg(argv[0], "zlprg_read() failed", 30);
        printf("[ %d Bytes ]\n", code.len);
        // write program to file
        if( zlprg_file_write_bin(&code, argv[3])!=0 )
          halt_prg(argv[0], "zlprg_file_write_bin() failed", 40);
        break;

 } // switch(rw_mode)

 // finish working with port
 if( zlprg_close(&prg)!=0 )
   halt_prg(argv[0], "zlprg_close() failed to close port", 200);

 return 0;
} // main()
