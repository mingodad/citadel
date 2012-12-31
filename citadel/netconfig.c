/*
 * This module handles shared rooms, inter-Citadel mail, and outbound
 * mailing list processing.
 *
 * Copyright (c) 2000-2012 by the citadel.org team
 *
 *  This program is open source software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License, version 3.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */

#include "sysdep.h"
#include <stdio.h>

#ifdef HAVE_SYSCALL_H
# include <syscall.h>
#else 
# if HAVE_SYS_SYSCALL_H
#  include <sys/syscall.h>
# endif
#endif

#include <libcitadel.h>

#include "include/ctdl_module.h"
HashList *CfgTypeHash = NULL;



void RegisterRoomCfgType(const char* Name, long len, RoomNetCfg eCfg, CfgLineParser p, int uniq, CfgLineSerializer s, CfgLineDeAllocator d)
{
	CfgLineType *pCfg;

	pCfg = (CfgLineType*) malloc(sizeof(CfgLineType));
	pCfg->Parser = p;
	pCfg->Serializer = s;
	pCfg->C = eCfg;
	pCfg->Str.Key = Name;
	pCfg->Str.len = len;
	pCfg->IsSingleLine = uniq;

	if (CfgTypeHash == NULL)
		CfgTypeHash = NewHash(1, NULL);
	Put(CfgTypeHash, Name, len, pCfg, NULL);
}


const CfgLineType *GetCfgTypeByStr(const char *Key, long len)
{
	void *pv;
	
	if (GetHash(CfgTypeHash, Key, len, &pv) && (pv != NULL))
	{
		return (const CfgLineType *) pv;
	}
	else
	{
		return NULL;
	}
}

const CfgLineType *GetCfgTypeByEnum(RoomNetCfg eCfg, HashPos *It)
{
	const char *Key;
	long len;
	void *pv;
	CfgLineType *pCfg;

	RewindHashPos(CfgTypeHash, It, 1);
	while (GetNextHashPos(CfgTypeHash, It, &len, &Key, &pv) && (pv != NULL))
	{
		pCfg = (CfgLineType*) pv;
		if (pCfg->C == eCfg)
			return pCfg;
	}
	return NULL;
}
void ParseGeneric(const CfgLineType *ThisOne, StrBuf *Line, const char *LinePos, OneRoomNetCfg *OneRNCFG)
{
	RoomNetCfgLine *nptr;

	nptr = (RoomNetCfgLine *)
		malloc(sizeof(RoomNetCfgLine));
	nptr->next = OneRNCFG->NetConfigs[ThisOne->C];
	nptr->Value = NewStrBufPlain(LinePos, StrLength(Line) - ( LinePos - ChrPtr(Line)) );
	OneRNCFG->NetConfigs[ThisOne->C] = nptr;
}

void SerializeGeneric(const CfgLineType *ThisOne, StrBuf *OutputBuffer, OneRoomNetCfg *OneRNCFG, RoomNetCfgLine *data)
{
	StrBufAppendBufPlain(OutputBuffer, CKEY(ThisOne->Str), 0);
	StrBufAppendBuf(OutputBuffer, data->Value, 0);
	StrBufAppendBufPlain(OutputBuffer, HKEY("\n"), 0);
}

void DeleteGenericCfgLine(const CfgLineType *ThisOne, RoomNetCfgLine **data)
{
	FreeStrBuf(&(*data)->Value);
	free(*data);
	*data = NULL;
}
int read_spoolcontrol_file(OneRoomNetCfg **pOneRNCFG, char *filename)
{
	int fd;
	const char *ErrStr = NULL;
	const char *Pos;
	const CfgLineType *pCfg;
	StrBuf *Line;
	StrBuf *InStr;
	OneRoomNetCfg *OneRNCFG;

	fd = open(filename, O_NONBLOCK|O_RDONLY);
	if (fd == -1) {
		*pOneRNCFG = NULL;
		return 0;
	}
	OneRNCFG = malloc(sizeof(OneRoomNetCfg));
	memset(OneRNCFG, 0, sizeof(OneRoomNetCfg));
	*pOneRNCFG = OneRNCFG;

	while (StrBufTCP_read_line(Line, &fd, 0, &ErrStr) >= 0) {
		if (StrLength(Line) == 0)
			continue;
		Pos = NULL;
		InStr = NewStrBufPlain(NULL, StrLength(Line));
		StrBufExtract_NextToken(InStr, Line, &Pos, '|');

		pCfg = GetCfgTypeByStr(SKEY(InStr));
		if (pCfg != NULL)
		{
			pCfg->Parser(pCfg, Line, Pos, OneRNCFG);
		}
		else
		{
			if (OneRNCFG->misc == NULL)
			{
				OneRNCFG->misc = NewStrBufDup(Line);
			}
			else
			{
				if(StrLength(OneRNCFG->misc) > 0)
					StrBufAppendBufPlain(OneRNCFG->misc, HKEY("\n"), 0);
				StrBufAppendBuf(OneRNCFG->misc, Line, 0);
			}
		}
	}
	if (fd > 0)
		close(fd);
	FreeStrBuf(&InStr);
	FreeStrBuf(&Line);
	return 1;
}

int save_spoolcontrol_file(OneRoomNetCfg *OneRNCFG, char *filename)
{
	RoomNetCfg eCfg;
	StrBuf *Cfg;
	char tempfilename[PATH_MAX];
	int TmpFD;
	long len;
	time_t unixtime;
	struct timeval tv;
	long reltid; /* if we don't have SYS_gettid, use "random" value */
	StrBuf *OutBuffer;
	int rc;
	HashPos *CfgIt;

	len = strlen(filename);
	memcpy(tempfilename, filename, len + 1);

#if defined(HAVE_SYONERNCFGALL_H) && defined (SYS_gettid)
	reltid = syOneRNCFGall(SYS_gettid);
#endif
	gettimeofday(&tv, NULL);
	/* Promote to time_t; types differ on some OSes (like darwin) */
	unixtime = tv.tv_sec;

	sprintf(tempfilename + len, ".%ld-%ld", reltid, unixtime);
	errno = 0;
	TmpFD = open(tempfilename, O_CREAT|O_EXCL|O_RDWR, S_IRUSR|S_IWUSR);
	Cfg = NewStrBuf();
	if ((TmpFD < 0) || (errno != 0)) {
		syslog(LOG_CRIT, "ERROR: cannot open %s: %s\n",
			filename, strerror(errno));
		unlink(tempfilename);
		return 0;
	}
	else {
		CfgIt = GetNewHashPos(CfgTypeHash, 1);
		fchown(TmpFD, config.c_ctdluid, 0);
		for (eCfg = subpending; eCfg < maxRoomNetCfg; eCfg ++)
		{
			const CfgLineType *pCfg;
			pCfg = GetCfgTypeByEnum(eCfg, CfgIt);
			if (pCfg->IsSingleLine)
			{
				pCfg->Serializer(pCfg, OutBuffer, OneRNCFG, NULL);
			}
			else
			{
				RoomNetCfgLine *pName = OneRNCFG->NetConfigs[pCfg->C];
				while (pName != NULL)
				{
					pCfg->Serializer(pCfg, OutBuffer, OneRNCFG, pName);
					pName = pName->next;
				}
				
				
			}

		}
		DeleteHashPos(&CfgIt);


		if (OneRNCFG->misc != NULL) {
			StrBufAppendBuf(OutBuffer, OneRNCFG->misc, 0);
		}

		rc = write(TmpFD, ChrPtr(OutBuffer), StrLength(OutBuffer));
		if ((rc >=0 ) && (rc == StrLength(Cfg))) 
		{
			close(TmpFD);
			rename(tempfilename, filename);
			rc = 1;
		}
		else {
			syslog(LOG_EMERG, 
				      "unable to write %s; [%s]; not enough space on the disk?\n", 
				      tempfilename, 
				      strerror(errno));
			close(TmpFD);
			unlink(tempfilename);
			rc = 0;
		}
		FreeStrBuf(&OutBuffer);
		
	}
	return rc;
}



void free_spoolcontrol_struct(OneRoomNetCfg **pOneRNCFG)
{
	RoomNetCfg eCfg;
	HashPos *CfgIt;
	OneRoomNetCfg *OneRNCFG;

	OneRNCFG = *pOneRNCFG;
	CfgIt = GetNewHashPos(CfgTypeHash, 1);
	for (eCfg = subpending; eCfg < maxRoomNetCfg; eCfg ++)
	{
		const CfgLineType *pCfg;
		RoomNetCfgLine *pNext, *pName;
		
		pCfg = GetCfgTypeByEnum(eCfg, CfgIt);
		pName= OneRNCFG->NetConfigs[pCfg->C];
		while (pName != NULL)
		{
			pNext = pName->next;
			pCfg->DeAllocator(pCfg, &pName);
			pName = pNext;
		}
	}
	DeleteHashPos(&CfgIt);

	FreeStrBuf(&OneRNCFG->Sender);
	FreeStrBuf(&OneRNCFG->RoomInfo);
	FreeStrBuf(&OneRNCFG->misc);
	free(OneRNCFG);
	*pOneRNCFG=NULL;
}

