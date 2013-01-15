#ifndef _BLOWFISH_H_
#define _BLOWFISH_H_

/* TODO: set correct endian automatically */

#define MAXKEYBYTES 56		/* 448 bits */
#define little_endian 1		/* Eg: Intel */
//#define big_endian 1		/* Eg: Motorola */

unsigned long F(unsigned long x);
void Blowfish_encipher(unsigned long *xl, unsigned long *xr);
void Blowfish_decipher(unsigned long *xl, unsigned long *xr);
void InitializeBlowfish(char key[], short keybytes);

#endif	/* _BLOWFISH_H_ */
