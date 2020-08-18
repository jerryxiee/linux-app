#ifndef _AES_ENCRYPT_H_
#define _AES_ENCRYPT_H_

int VAVA_Aes_Encrypt(char* in, char* out, int size);
int VAVA_Aes_Decrypt(char* in, char* out, int size);
int VAVA_Aes_Check_Video(unsigned char *buff, int size);
int VAVA_Aes_Check_Audio(unsigned char *buff, int size);

#endif
