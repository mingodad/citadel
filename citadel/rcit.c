#define UNCOMPRESS "/usr/bin/gunzip"

/* Citadel/UX rcit
 * version 2.9
 *
 * This program functions the same as the standard rnews program for
 * UseNet. It accepts standard input, and looks for rooms to post messages
 * (translated from UseNet format to the Citadel/UX binary message format)
 * in that match the names of the newsgroups. network/rnews.xref is checked
 * in case the sysop wants to cross-reference room names to newsgroup names.
 * If standard input is already in binary, the -c flag should be used.
 *   The netproc program is then called to do the processing.
 *
 * If you have a separate newsreader and don't want to use Citadel for news,
 * just call this program something else (rcit, for example) -- but make sure
 * to tell your Citadel network neighbors the new name of the program to call.
 * 
 * usage:
 *	rnews [-c] [-z] [-s]
 * flags:
 *	-c	Input is already in Citadel binary format
 *		(default is UseNet news format)
 *	-z	Input is compressed, run uncompress on it before processing
 *	-s	Don't run netproc now, just accept the input into spoolin
 */

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include "citadel.h"

void get_config(void);
struct config config;

/*
 * Cross-reference newsgroup names to Citadel room names
 */
int rnewsxref(char *room, char *ngroup) {
	FILE *fp;
	int a,b;
	char aaa[50],bbb[50];
	
	strcpy(room,ngroup);
	fp=fopen("network/rnews.xref","r");
GNA:	strcpy(aaa,""); strcpy(bbb,"");
	do {
		a=getc(fp);
		if (a==',') a=0;
		if (a>0) { b=strlen(aaa); aaa[b]=a; aaa[b+1]=0; }
		} while(a>0);
	do {
		a=getc(fp);
		if (a==10) a=0;
		if (a>0) { b=strlen(bbb); bbb[b]=a; bbb[b+1]=0; }
		} while(a>0);
	if (a<0) {
		fclose(fp);
		return(1);
		}
	if (strcasecmp(ngroup,aaa)) goto GNA;
	fclose(fp);
	strcpy(room,bbb);
	return(0);
	}


int main(int argc, char **argv)
{
	char aaa[128],bbb[128],ccc[128];
	char author[128],recipient[128],room[128],node[128],path[512];
	char subject[128];
	char orgname[128];
	long mid = 0L;
	time_t now;
	long bcount,aa;
	int a;
	char flnm[128],tname[128];
	FILE *minput,*mout,*mtemp;
	char binary_input = 0;
	char compressed_input = 0;
	char spool_only = 0;

	get_config();
	sprintf(flnm,"./network/spoolin/rnews.%d",getpid());
	sprintf(tname,"/tmp/rnews.%d",getpid());

	for (a=1; a<argc; ++a) {
		if (!strcmp(argv[a],"-c")) binary_input = 1;
		if (!strcmp(argv[a],"-z")) compressed_input = 1;
		if (!strcmp(argv[a],"-s")) spool_only = 1;
		}

	minput=stdin;
	if (compressed_input) minput=popen(UNCOMPRESS,"r");
	if (minput==NULL) fprintf(stderr,"rnews: can't open input!!!!\n");

	mout=fopen(flnm,"w");

	/* process Citadel/UX binary format input */
	if (binary_input) {
		while ((a=getc(minput))>=0) putc(a,mout);
		goto END;
		}

	/* process UseNet news input */
A:	if (fgets(aaa,128,minput)==NULL) goto END;
	aaa[strlen(aaa)-1]=0;
	if (strncmp(aaa,"#! rnews ",9)) goto A;
	bcount=atol(&aaa[9]);
	mtemp=fopen(tname,"w");
	for (aa=0L; aa<bcount; ++aa) {
		a=getc(minput);
		if (a<0) goto NMA;
		if (a>=0) putc(a,mtemp);
		}
NMA:	fclose(mtemp);
	if (a<0) {
		fprintf(stderr,"rnews: EOF unexpected\n");
		goto END;
		}

	mtemp=fopen(tname,"r");
	strcpy(author,"");
	strcpy(recipient,"");
	strcpy(room,"");
	strcpy(node,"");
	strcpy(path,"");
	strcpy(orgname,"");
	strcpy(subject,"");

B:	if (fgets(aaa,128,mtemp)==NULL) goto ABORT;
	aaa[strlen(aaa)-1]=0;
	if (strlen(aaa)==0) goto C;


	if (!strncmp(aaa,"From: ",6)) {
		strcpy(author,&aaa[6]);
		while((author[0]==' ')&&(strlen(author)>0))
			strcpy(author,&author[1]);
		for (a=0; a<strlen(author); ++a) {
			if (author[a]=='<') author[a-1]=0;
			if (author[a]==')') author[a]=0;
			if (author[a]=='(') {
				strcpy(author,&author[a+1]);
				a=0;
				}
			}
		if (!strcmp(author,")")) {
			strcpy(author,&aaa[6]);
			for (a=0; a<strlen(author); ++a)
				if (author[a]=='@') author[a]=0;
			}
		strcpy(node,&aaa[6]);
		for (a=0; a<strlen(node); ++a) {
			if ((node[a]=='<')||(node[a]=='@')) {
				strcpy(node,&node[a+1]);
				a=0;
				}
			if (node[a]=='>') node[a]=0;
			if (node[a]=='(') node[a-1]=0;
			}
		for (a=0; a<strlen(author); ++a)
			if (author[a]=='@') author[a]=0;
		}

	if (!strncmp(aaa,"Path: ",6)) strcpy(path,&aaa[6]);
	if (!strncmp(aaa,"To: ",4)) strcpy(recipient,&aaa[4]);
	if (!strncmp(aaa,"Subject: ",9)) strcpy(subject,&aaa[9]);
	if (!strncmp(aaa,"Organization: ",14)) strcpy(orgname,&aaa[14]);

	if (!strncmp(aaa,"Newsgroups: ",11)) {
		strcpy(room,&aaa[12]);
		for (a=0; a<strlen(aaa); ++a) if (aaa[a]==',') aaa[a]=0;
		goto B;
		}

	if (!strncmp(aaa,"Message-ID: ",11)) {
		strcpy(bbb,&aaa[13]);
		for (a=0; a<strlen(bbb); ++a) if (bbb[a]=='@') bbb[a]=0;
		mid=atol(bbb);
		while((aaa[0]!='@')&&(aaa[0]!=0)) {
			strcpy(&aaa[0],&aaa[1]);
			}
		strcpy(&aaa[0],&aaa[1]);
		for (a=0; a<strlen(aaa); ++a) if (aaa[a]=='>') aaa[a]=0;
		strcpy(node,aaa);
		goto B;
		}
		goto B;

C:	if ((author[0]==0)||(room[0]==0)||(node[0]==0)) goto ABORT;
	putc(255,mout);			/* start of message */
	putc(MES_NORMAL,mout);		/* not anonymous */
	putc(1,mout);			/* not formatted */
	time(&now);

	fprintf(mout,"I%ld",mid); putc(0,mout);
	fprintf(mout,"P%s",path); putc(0,mout);
	fprintf(mout,"T%ld",now); putc(0,mout);
	fprintf(mout,"A%s",author); putc(0,mout);
	strcpy(ccc,room);
	rnewsxref(room,ccc);
	fprintf(mout,"O%s",room); putc(0,mout);
	fprintf(mout,"N%s",node); putc(0,mout);
	if (orgname[0]!=0) {
		fprintf(mout,"H%s",orgname); putc(0,mout);
		}
	if (recipient[0]!=0) {
		fprintf(mout,"R%s",recipient); putc(0,mout);
		}
	if (subject[0]!=0) {
		fprintf(mout,"U%s",subject); putc(0,mout);
		}
	fprintf(mout,"M");
	a=0;
	aaa[0]=0;

	do {
		a=getc(mtemp);
		if (a>0) putc(a,mout);
		} while (a>0);
	putc(0,mout);
ABORT:	fclose(mtemp);
	unlink(tname);
	goto A;

END:	putc(0,mout);
	fclose(mout);
	unlink(tname);
	if (compressed_input) pclose(minput);
	if (!spool_only) execlp("./netproc","netproc",NULL);
	exit(0);
}


