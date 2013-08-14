/*
 * Copyright (c) 1996-2013 by the citadel.org team
 *
 * This program is open source software.  You can redistribute it and/or
 * modify it under the terms of the GNU General Public License, version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

int uds_connectsock(char *);
int tcp_connectsock(char *, char *);
int serv_getln(char *strbuf, int bufsize);
int StrBuf_ServGetln(StrBuf *buf);

/*
 * parse & check the server reply 
 *
 * Line		the line containing the server reply
 * FullState	if you need more than just the major number, this is returns it. Ignored if NULL.
 * PutImportantMessage if you want to forward the text part of the server reply to the user, specify 1; 
 * 		the result will be put into the 'Important Message' framework.
 * MajorOK	in case of which major number not to put the ImportantMessage? 0 for all.
 *
 * returns the most significant digit of the server status
 */

int GetServerStatusMsg(StrBuf *Line, long* FullState, int PutImportantMessage, int MajorOK);

/*
 * to migrate old calls.... 
 */
#define GetServerStatus(a, b) GetServerStatusMsg(a, b, 0, 0)

int serv_puts(const char *string);

int serv_write(const char *buf, int nbytes);
int serv_putbuf(const StrBuf *string);
int serv_printf(const char *format,...)__attribute__((__format__(__printf__,1,2)));
int serv_read_binary(StrBuf *Ret, size_t total_len, StrBuf *Buf);
int StrBuf_ServGetBLOB(StrBuf *buf, long BlobSize);
int StrBuf_ServGetBLOBBuffered(StrBuf *buf, long BlobSize);
int read_server_text(StrBuf *Buf, long *nLines);

void text_to_server(char *ptr);
void text_to_server_qp(char *ptr);
void server_to_text(void);
int lingering_close(int fd);
