/*
 * zlxprg.h
 *
 * description of basic operations used to communicate
 * with writer.
 *
 */
#ifndef INCLUDE_ZLXPRG_H_FILE
#define INCLUDE_ZLXPRG_H_FILE

#include <termios.h>


// maximum length for name of writer
#define NAME_MAX	128
// maximum length of program code
#define CODE_MAX	(64*1024)



// structure representing writer itself
struct zlprg
{
 int            fd;		// descriptor of communication I/O
 char           name[NAME_MAX];	// name of writer
 int            has_params;	// uC allows to check it's params?
 struct termios t_prev;		// poprzednie ustawienia portu
 struct termios t_now;		// obecne ustawienia
}; // struct zlprg



// representation of data to write (or read from device)
struct zlprg_code
{
 int            len;		// number of bytes
 char           byte[CODE_MAX];	// binary code
 unsigned short checksum;	// checksum of code
}; // struct zlprg_code



// macro checks whether uC is in writer
#define	has_uC(uC_def)	( (uC_def)->type!=0xff )


// structure representing current uC
struct zlprg_uC
{
 int  type;			// number representing given uC
 int  non_blank;		// number of nonblank bytes
 int  bytes;			// numer of bytes

 // meta-info:
 char name[NAME_MAX];		// type as string (it's real name)
 int  eeprom;			// EEPROM size
}; // struct zlprg_uC



//
// PROVIDED FUNCTIONS
//


// returns initialized writer description
int zlprg_init(struct zlprg *prg, char dev[]);

// writes data to uC
int zlprg_write(struct zlprg *prg, struct zlprg_code *code);
// reads data from uC - size is number of bytes to read
// (-1 means "autodetect")
int zlprg_read(struct zlprg *prg, struct zlprg_code *code, int size);
// verifies if given code maches code in uC
int zlprg_verify(struct zlprg *prg, struct zlprg_code *code);

// closes connection to writer
int zlprg_close(struct zlprg *prg);

// gives the type of pluged uC
int zlprg_uC(struct zlprg *prg, struct zlprg_uC *uC);


//
// FILE I/O
//

// writes code to bin file
int zlprg_file_write_bin(struct zlprg_code *code, char file[]);
// reads code from bin file
int zlprg_file_read_bin(struct zlprg_code *code, char file[]);


#endif

