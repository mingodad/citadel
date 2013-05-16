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
#include <dirent.h>

#include <libcitadel.h>

#include "include/ctdl_module.h"
#include "serv_extensions.h"

void vFreeRoomNetworkStruct(void *vOneRoomNetCfg);
void FreeRoomNetworkStructContent(OneRoomNetCfg *OneRNCfg);

HashList *CfgTypeHash = NULL;
HashList *RoomConfigs = NULL;
/*-----------------------------------------------------------------------------*
 *                       Per room network configs                              *
 *-----------------------------------------------------------------------------*/
void RegisterRoomCfgType(const char* Name, long len, RoomNetCfg eCfg, CfgLineParser p, int uniq,  int nSegments, CfgLineSerializer s, CfgLineDeAllocator d)
{
	CfgLineType *pCfg;

	pCfg = (CfgLineType*) malloc(sizeof(CfgLineType));
	pCfg->Parser = p;
	pCfg->Serializer = s;
	pCfg->DeAllocator = d;
	pCfg->C = eCfg;
	pCfg->Str.Key = Name;
	pCfg->Str.len = len;
	pCfg->IsSingleLine = uniq;
 	pCfg->nSegments = nSegments;
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
void ParseGeneric(const CfgLineType *ThisOne, StrBuf *Line, const char *LinePos, OneRoomNetCfg *OneRNCfg)
{
	RoomNetCfgLine *nptr;
	int i;

	nptr = (RoomNetCfgLine *)
		malloc(sizeof(RoomNetCfgLine));
	nptr->next = OneRNCfg->NetConfigs[ThisOne->C];
	nptr->Value = malloc(sizeof(StrBuf*) * ThisOne->nSegments);
	nptr->nValues = 0;
	memset(nptr->Value, 0, sizeof(StrBuf*) * ThisOne->nSegments);
	if (ThisOne->nSegments == 1)
	{
		nptr->Value[0] = NewStrBufPlain(LinePos, StrLength(Line) - ( LinePos - ChrPtr(Line)) );
		nptr->nValues = 1;
	}
	else for (i = 0; i < ThisOne->nSegments; i++)
	{
		nptr->nValues++;
		nptr->Value[i] = NewStrBufPlain(NULL, StrLength(Line) - ( LinePos - ChrPtr(Line)) );
		StrBufExtract_NextToken(nptr->Value[i], Line, &LinePos, '|');
	}

	OneRNCfg->NetConfigs[ThisOne->C] = nptr;
}

void SerializeGeneric(const CfgLineType *ThisOne, StrBuf *OutputBuffer, OneRoomNetCfg *OneRNCfg, RoomNetCfgLine *data)
{
	int i;

	StrBufAppendBufPlain(OutputBuffer, CKEY(ThisOne->Str), 0);
	StrBufAppendBufPlain(OutputBuffer, HKEY("|"), 0);
	for (i = 0; i < ThisOne->nSegments; i++)
	{
		StrBufAppendBuf(OutputBuffer, data->Value[i], 0);
		if (i + 1 < ThisOne->nSegments)
			StrBufAppendBufPlain(OutputBuffer, HKEY("|"), 0);
	}
	StrBufAppendBufPlain(OutputBuffer, HKEY("\n"), 0);
}

void DeleteGenericCfgLine(const CfgLineType *ThisOne, RoomNetCfgLine **data)
{
	int i;

	if (*data == NULL)
		return;

	for (i = 0; i < (*data)->nValues; i++)
	{
		FreeStrBuf(&(*data)->Value[i]);
	}
	free ((*data)->Value);
	free(*data);
	*data = NULL;
}
RoomNetCfgLine *DuplicateOneGenericCfgLine(const RoomNetCfgLine *data)
{
	int i;
	RoomNetCfgLine *NewData;

	NewData = (RoomNetCfgLine*)malloc(sizeof(RoomNetCfgLine));
	memset(NewData, 0, sizeof(RoomNetCfgLine));
	NewData->Value = (StrBuf **)malloc(sizeof(StrBuf*) * data->nValues);
	memset(NewData->Value, 0, sizeof(StrBuf*) * data->nValues);

	for (i = 0; i < data->nValues; i++)
	{
		NewData->Value[i] = NewStrBufDup(data->Value[i]);
	}
	NewData->nValues = data->nValues;
	return NewData;
}
int ReadRoomNetConfigFile(OneRoomNetCfg **pOneRNCfg, char *filename)
{
	int fd;
	const char *ErrStr = NULL;
	const char *Pos;
	const CfgLineType *pCfg;
	StrBuf *Line;
	StrBuf *InStr;
	OneRoomNetCfg *OneRNCfg = NULL;

	fd = open(filename, O_NONBLOCK|O_RDONLY);
	if (fd == -1) {
		*pOneRNCfg = NULL;
		return 0;
	}
	if (*pOneRNCfg != NULL)
	{
		OneRNCfg = *pOneRNCfg;
		FreeRoomNetworkStructContent (OneRNCfg);
	}
	else
		OneRNCfg = malloc(sizeof(OneRoomNetCfg));
	memset(OneRNCfg, 0, sizeof(OneRoomNetCfg));
	*pOneRNCfg = OneRNCfg;
	Line = NewStrBuf();
	InStr = NewStrBuf();

	while (StrBufTCP_read_line(Line, &fd, 0, &ErrStr) >= 0) {
		if (StrLength(Line) == 0)
			continue;
		Pos = NULL;
		StrBufExtract_NextToken(InStr, Line, &Pos, '|');

		pCfg = GetCfgTypeByStr(SKEY(InStr));
		if (pCfg != NULL)
		{
			pCfg->Parser(pCfg, Line, Pos, OneRNCfg);
		}
		else
		{
			if (OneRNCfg->misc == NULL)
			{
				OneRNCfg->misc = NewStrBufDup(Line);
			}
			else
			{
				if(StrLength(OneRNCfg->misc) > 0)
					StrBufAppendBufPlain(OneRNCfg->misc, HKEY("\n"), 0);
				StrBufAppendBuf(OneRNCfg->misc, Line, 0);
			}
		}
	}
	if (fd > 0)
		close(fd);
	FreeStrBuf(&InStr);
	FreeStrBuf(&Line);
	return 1;
}

int SaveRoomNetConfigFile(OneRoomNetCfg *OneRNCfg, char *filename)
{
	RoomNetCfg eCfg;
	StrBuf *Cfg = NULL;
	StrBuf *OutBuffer = NULL;
	char tempfilename[PATH_MAX];
	int TmpFD;
	long len;
	time_t unixtime;
	struct timeval tv;
	long reltid; /* if we don't have SYS_gettid, use "random" value */
	int rc;
	HashPos *CfgIt;

	len = strlen(filename);
	memcpy(tempfilename, filename, len + 1);

#if defined(HAVE_SYSCALL_H) && defined (SYS_gettid)
	reltid = syscall(SYS_gettid);
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
		FreeStrBuf(&Cfg);
		return 0;
	}
	else {
		OutBuffer = NewStrBuf();
		CfgIt = GetNewHashPos(CfgTypeHash, 1);
		fchown(TmpFD, config.c_ctdluid, 0);
		for (eCfg = subpending; eCfg < maxRoomNetCfg; eCfg ++)
		{
			const CfgLineType *pCfg;
			pCfg = GetCfgTypeByEnum(eCfg, CfgIt);
			if (pCfg->IsSingleLine)
			{
				pCfg->Serializer(pCfg, OutBuffer, OneRNCfg, NULL);
			}
			else
			{
				RoomNetCfgLine *pName = OneRNCfg->NetConfigs[pCfg->C];
				while (pName != NULL)
				{
					pCfg->Serializer(pCfg, OutBuffer, OneRNCfg, pName);
					pName = pName->next;
				}
				
				
			}

		}
		DeleteHashPos(&CfgIt);


		if (OneRNCfg->misc != NULL) {
			StrBufAppendBuf(OutBuffer, OneRNCfg->misc, 0);
		}

		rc = write(TmpFD, ChrPtr(OutBuffer), StrLength(OutBuffer));
		if ((rc >=0 ) && (rc == StrLength(OutBuffer))) 
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
	FreeStrBuf(&Cfg);
	return rc;
}

void SaveModifiedRooms(struct ctdlroom *qrbuf, void *data, OneRoomNetCfg *OneRNCfg)
{
	char filename[PATH_MAX];

	if (OneRNCfg->changed)
	{
		assoc_file_name(filename, sizeof filename, qrbuf, ctdl_netcfg_dir);
		SaveRoomNetConfigFile(OneRNCfg, filename);
		OneRNCfg->changed = 0;
	}
}
void SaveChangedConfigs(void)
{
	CtdlForEachNetCfgRoom(SaveModifiedRooms,
			      NULL, 
			      maxRoomNetCfg);
}


void AddRoomCfgLine(OneRoomNetCfg *OneRNCfg, struct ctdlroom *qrbuf, RoomNetCfg LineType, RoomNetCfgLine *Line)
{
	int new = 0;
	RoomNetCfgLine **pLine;
	char filename[PATH_MAX];

	if (OneRNCfg == NULL)
	{
		new = 1;
		OneRNCfg = (OneRoomNetCfg*) malloc(sizeof(OneRoomNetCfg));
		memset(OneRNCfg, 0, sizeof(OneRoomNetCfg));
	}
	pLine = &OneRNCfg->NetConfigs[LineType];

	while(*pLine != NULL) pLine = &((*pLine)->next);
	*pLine = Line;

	assoc_file_name(filename, sizeof filename, qrbuf, ctdl_netcfg_dir);
	SaveRoomNetConfigFile(OneRNCfg, filename);
	OneRNCfg->changed = 0;
	if (new)
	{
		Put(RoomConfigs, LKEY(qrbuf->QRnumber), OneRNCfg, vFreeRoomNetworkStruct);
	}
}

void FreeRoomNetworkStructContent(OneRoomNetCfg *OneRNCfg)
{
	RoomNetCfg eCfg;
	HashPos *CfgIt;

	CfgIt = GetNewHashPos(CfgTypeHash, 1);
	for (eCfg = subpending; eCfg < maxRoomNetCfg; eCfg ++)
	{
		const CfgLineType *pCfg;
		RoomNetCfgLine *pNext, *pName;
		
		pCfg = GetCfgTypeByEnum(eCfg, CfgIt);
		pName= OneRNCfg->NetConfigs[eCfg];
		while (pName != NULL)
		{
			pNext = pName->next;
			if (pCfg != NULL)
			{
				pCfg->DeAllocator(pCfg, &pName);
			}
			else
			{
				DeleteGenericCfgLine(NULL, &pName);
			}
			pName = pNext;
		}
	}
	DeleteHashPos(&CfgIt);

	FreeStrBuf(&OneRNCfg->Sender);
	FreeStrBuf(&OneRNCfg->RoomInfo);
	FreeStrBuf(&OneRNCfg->misc);
	memset(OneRNCfg, 0, sizeof(OneRoomNetCfg));
}
void vFreeRoomNetworkStruct(void *vOneRoomNetCfg)
{
	OneRoomNetCfg *OneRNCfg;
	OneRNCfg = (OneRoomNetCfg*)vOneRoomNetCfg;
	FreeRoomNetworkStructContent(OneRNCfg);
	free(OneRNCfg);
}
void FreeRoomNetworkStruct(OneRoomNetCfg **pOneRNCfg)
{
	vFreeRoomNetworkStruct(*pOneRNCfg);
	*pOneRNCfg=NULL;
}

OneRoomNetCfg* CtdlGetNetCfgForRoom(long QRNumber)
{
	void *pv;
	GetHash(RoomConfigs, LKEY(QRNumber), &pv);
	return (OneRoomNetCfg*)pv;
}


void LoadAllNetConfigs(void)
{
	DIR *filedir = NULL;
	struct dirent *d;
	struct dirent *filedir_entry;
	int d_type = 0;
        int d_namelen;
	long RoomNumber;
	OneRoomNetCfg *OneRNCfg;
	int IsNumOnly;
	const char *pch;
	char path[PATH_MAX];

	RoomConfigs = NewHash(1, NULL);
	filedir = opendir (ctdl_netcfg_dir);
	if (filedir == NULL) {
		return ; /// todo: panic!
	}

	d = (struct dirent *)malloc(offsetof(struct dirent, d_name) + PATH_MAX + 1);
	if (d == NULL) {
		closedir(filedir);
		return ;
	}

	while ((readdir_r(filedir, d, &filedir_entry) == 0) &&
	       (filedir_entry != NULL))
	{
#ifdef _DIRENT_HAVE_D_NAMLEN
		d_namelen = filedir_entry->d_namelen;
#else
		d_namelen = strlen(filedir_entry->d_name);
#endif

#ifdef _DIRENT_HAVE_D_TYPE
		d_type = filedir_entry->d_type;
#else

#ifndef DT_UNKNOWN
#define DT_UNKNOWN     0
#define DT_DIR         4
#define DT_REG         8
#define DT_LNK         10

#define IFTODT(mode)   (((mode) & 0170000) >> 12)
#define DTTOIF(dirtype)        ((dirtype) << 12)
#endif
		d_type = DT_UNKNOWN;
#endif
		if ((d_namelen > 1) && filedir_entry->d_name[d_namelen - 1] == '~')
			continue; /* Ignore backup files... */

		if ((d_namelen == 1) && 
		    (filedir_entry->d_name[0] == '.'))
			continue;

		if ((d_namelen == 2) && 
		    (filedir_entry->d_name[0] == '.') &&
		    (filedir_entry->d_name[1] == '.'))
			continue;

		snprintf(path, PATH_MAX, "%s/%s", 
			 ctdl_netcfg_dir, filedir_entry->d_name);

		if (d_type == DT_UNKNOWN) {
			struct stat s;
			if (lstat(path, &s) == 0) {
				d_type = IFTODT(s.st_mode);
			}
		}

		switch (d_type)
		{
		case DT_DIR:
			break;
		case DT_LNK: /* TODO: check whether its a file or a directory */
		case DT_REG:
			IsNumOnly = 1;
			pch = filedir_entry->d_name;
			while (*pch != '\0')
			{
				if (!isdigit(*pch))
				{
					IsNumOnly = 0;
				}
				pch ++;
			}
			if (IsNumOnly)
			{
				OneRNCfg = NULL;
				RoomNumber = atol(filedir_entry->d_name);
				ReadRoomNetConfigFile(&OneRNCfg, path);

				if (OneRNCfg != NULL)
					Put(RoomConfigs, LKEY(RoomNumber), OneRNCfg, vFreeRoomNetworkStruct);
				/* syslog(9, "[%s | %s]\n", ChrPtr(OneWebName), ChrPtr(FileName)); */
			}
			break;
		default:
			break;
		}


	}
	free(d);
	closedir(filedir);
}


/*-----------------------------------------------------------------------------*
 *              Per room network configs : exchange with client                *
 *-----------------------------------------------------------------------------*/
void cmd_gnet(char *argbuf)
{
	char filename[PATH_MAX];
	char buf[SIZ];
	FILE *fp;


	if (!IsEmptyStr(argbuf))
	{
		if (CtdlAccessCheck(ac_aide)) return;
		if (strcmp(argbuf, FILE_MAILALIAS))
		{
			cprintf("%d No such file or directory\n",
				ERROR + INTERNAL_ERROR);
			return;
		}
		safestrncpy(filename, file_mail_aliases, sizeof(filename));
		cprintf("%d Settings for <%s>\n",
			LISTING_FOLLOWS,
			filename);
	}
	else
	{
		if ( (CC->room.QRflags & QR_MAILBOX) && (CC->user.usernum == atol(CC->room.QRname)) ) {
			/* users can edit the netconfigs for their own mailbox rooms */
		}
		else if (CtdlAccessCheck(ac_room_aide)) return;
		
		assoc_file_name(filename, sizeof filename, &CC->room, ctdl_netcfg_dir);
		cprintf("%d Network settings for room #%ld <%s>\n",
			LISTING_FOLLOWS,
			CC->room.QRnumber, CC->room.QRname);
	}

	fp = fopen(filename, "r");
	if (fp != NULL) {
		while (fgets(buf, sizeof buf, fp) != NULL) {
			buf[strlen(buf)-1] = 0;
			cprintf("%s\n", buf);
		}
		fclose(fp);
	}

	cprintf("000\n");
}

#define nForceAliases 5
const ConstStr ForceAliases[nForceAliases] = {
	{HKEY("bbs,")},
	{HKEY("root,")},
	{HKEY("Auto,")},
	{HKEY("postmaster,")},
	{HKEY("abuse,")}
};
void cmd_snet(char *argbuf)
{
	struct CitContext *CCC = CC;
	char tempfilename[PATH_MAX];
	char filename[PATH_MAX];
	int TmpFD;
	StrBuf *Line;
	struct stat StatBuf;
	long len;
	int rc;
	int IsMailAlias = 0;
	int MailAliasesFound[nForceAliases];

	unbuffer_output();

	if (!IsEmptyStr(argbuf))
	{
		if (CtdlAccessCheck(ac_aide)) return;
		if (strcmp(argbuf, FILE_MAILALIAS))
		{
			cprintf("%d No such file or directory\n",
				ERROR + INTERNAL_ERROR);
			return;
		}
		len = safestrncpy(filename, file_mail_aliases, sizeof(filename));
		memset(MailAliasesFound, 0, sizeof(MailAliasesFound));
		memcpy(tempfilename, filename, len + 1);
		IsMailAlias = 1;
	}
	else
	{
		if ( (CCC->room.QRflags & QR_MAILBOX) && (CCC->user.usernum == atol(CCC->room.QRname)) ) {
			/* users can edit the netconfigs for their own mailbox rooms */
		}
		else if (CtdlAccessCheck(ac_room_aide)) return;
		
		len = assoc_file_name(filename, sizeof filename, &CCC->room, ctdl_netcfg_dir);
		memcpy(tempfilename, filename, len + 1);
	}
	memset(&StatBuf, 0, sizeof(struct stat));
	if ((stat(filename, &StatBuf)  == -1) || (StatBuf.st_size == 0))
		StatBuf.st_size = 80; /* Not there or empty? guess 80 chars line. */

	sprintf(tempfilename + len, ".%d", CCC->cs_pid);
	errno = 0;
	TmpFD = open(tempfilename, O_CREAT|O_EXCL|O_RDWR, S_IRUSR|S_IWUSR);

	if ((TmpFD > 0) && (errno == 0))
	{
		char *tmp = malloc(StatBuf.st_size * 2);
		memset(tmp, ' ', StatBuf.st_size * 2);
		rc = write(TmpFD, tmp, StatBuf.st_size * 2);
		free(tmp);
		if ((rc <= 0) || (rc != StatBuf.st_size * 2))
		{
			close(TmpFD);
			cprintf("%d Unable to allocate the space required for %s: %s\n",
				ERROR + INTERNAL_ERROR,
				tempfilename,
				strerror(errno));
			unlink(tempfilename);
			return;
		}	
		lseek(TmpFD, SEEK_SET, 0);
	}
	else {
		cprintf("%d Unable to allocate the space required for %s: %s\n",
			ERROR + INTERNAL_ERROR,
			tempfilename,
			strerror(errno));
		unlink(tempfilename);
		return;
	}
	Line = NewStrBuf();

	cprintf("%d %s\n", SEND_LISTING, tempfilename);

	len = 0;
	while (rc = CtdlClientGetLine(Line), 
	       (rc >= 0))
	{
		if ((rc == 3) && (strcmp(ChrPtr(Line), "000") == 0))
			break;
		if (IsMailAlias)
		{
			int i;

			for (i = 0; i < nForceAliases; i++)
			{
				if ((!MailAliasesFound[i]) && 
				    (strncmp(ForceAliases[i].Key, 
					     ChrPtr(Line),
					     ForceAliases[i].len) == 0)
					)
				    {
					    MailAliasesFound[i] = 1;
					    break;
				    }
			}
		}

		StrBufAppendBufPlain(Line, HKEY("\n"), 0);
		write(TmpFD, ChrPtr(Line), StrLength(Line));
		len += StrLength(Line);
	}
	FreeStrBuf(&Line);
	ftruncate(TmpFD, len);
	close(TmpFD);

	if (IsMailAlias)
	{
		int i, state;
		/*
		 * Sanity check whether all aliases required by the RFCs were set
		 * else bail out.
		 */
		state = 1;
		for (i = 0; i < nForceAliases; i++)
		{
			if (!MailAliasesFound[i]) 
				state = 0;
		}
		if (state == 0)
		{
			cprintf("%d won't do this - you're missing an RFC required alias.\n",
				ERROR + INTERNAL_ERROR);
			unlink(tempfilename);
			return;
		}
	}

	/* Now copy the temp file to its permanent location.
	 * (We copy instead of link because they may be on different filesystems)
	 */
	begin_critical_section(S_NETCONFIGS);
	rename(tempfilename, filename);
	if (!IsMailAlias)
	{
		OneRoomNetCfg *RNCfg;
		RNCfg = CtdlGetNetCfgForRoom(CCC->room.QRnumber);
		if (RNCfg != NULL)
		{
			ReadRoomNetConfigFile(&RNCfg, filename);
		}
		else
		{
			ReadRoomNetConfigFile(&RNCfg, filename);
			Put(RoomConfigs, LKEY(CCC->room.QRnumber), RNCfg, vFreeRoomNetworkStruct);
		}

		PerformRoomHooks(&CCC->room);
	}
	end_critical_section(S_NETCONFIGS);
}


/*-----------------------------------------------------------------------------*
 *                       Per node network configs                              *
 *-----------------------------------------------------------------------------*/
void DeleteCtdlNodeConf(void *vNode)
{
	CtdlNodeConf *Node = (CtdlNodeConf*) vNode;
	FreeStrBuf(&Node->NodeName);
	FreeStrBuf(&Node->Secret);
	FreeStrBuf(&Node->Host);
	FreeStrBuf(&Node->Port);
	free(Node);
}

CtdlNodeConf *NewNode(StrBuf *SerializedNode)
{
	const char *Pos = NULL;
	CtdlNodeConf *Node;

	/* we need at least 4 pipes and some other text so its invalid. */
	if (StrLength(SerializedNode) < 8)
		return NULL;
	Node = (CtdlNodeConf *) malloc(sizeof(CtdlNodeConf));

	Node->DeleteMe = 0;

	Node->NodeName=NewStrBuf();
	StrBufExtract_NextToken(Node->NodeName, SerializedNode, &Pos, '|');

	Node->Secret=NewStrBuf();
	StrBufExtract_NextToken(Node->Secret, SerializedNode, &Pos, '|');

	Node->Host=NewStrBuf();
	StrBufExtract_NextToken(Node->Host, SerializedNode, &Pos, '|');

	Node->Port=NewStrBuf();
	StrBufExtract_NextToken(Node->Port, SerializedNode, &Pos, '|');
	return Node;
}


/*
 * Load or refresh the Citadel network (IGnet) configuration for this node.
 */
HashList* CtdlLoadIgNetCfg(void)
{
	const char *LinePos;
	char       *Cfg;
	StrBuf     *Buf;
	StrBuf     *LineBuf;
	HashList   *Hash;
	CtdlNodeConf   *Node;

	Cfg =  CtdlGetSysConfig(IGNETCFG);
	if ((Cfg == NULL) || IsEmptyStr(Cfg)) {
		if (Cfg != NULL)
			free(Cfg);
		return NULL;
	}

	Hash = NewHash(1, NULL);
	Buf = NewStrBufPlain(Cfg, -1);
	free(Cfg);
	LineBuf = NewStrBufPlain(NULL, StrLength(Buf));
	LinePos = NULL;
	do
	{
		StrBufSipLine(LineBuf, Buf, &LinePos);
		if (StrLength(LineBuf) != 0) {
			Node = NewNode(LineBuf);
			if (Node != NULL) {
				Put(Hash, SKEY(Node->NodeName), Node, DeleteCtdlNodeConf);
			}
		}
	} while (LinePos != StrBufNOTNULL);
	FreeStrBuf(&Buf);
	FreeStrBuf(&LineBuf);
	return Hash;
}


int is_recipient(OneRoomNetCfg *RNCfg, const char *Name)
{
	const RoomNetCfg RecipientCfgs[] = {
		listrecp,
		digestrecp,
		participate,
		maxRoomNetCfg
	};
	int i;
	RoomNetCfgLine *nptr;
	size_t len;
	
	len = strlen(Name);
	i = 0;
	while (RecipientCfgs[i] != maxRoomNetCfg)
	{
		nptr = RNCfg->NetConfigs[RecipientCfgs[i]];
		
		while (nptr != NULL)
		{
			if ((StrLength(nptr->Value[0]) == len) && 
			    (!strcmp(Name, ChrPtr(nptr->Value[0]))))
			{
				return 1;
			}
			nptr = nptr->next;
		}
		i++;
	}
	return 0;
}



int CtdlNetconfigCheckRoomaccess(
	char *errmsgbuf, 
	size_t n,
	const char* RemoteIdentifier)
{
	OneRoomNetCfg *RNCfg;
	int found;

	if (RemoteIdentifier == NULL)
	{
		snprintf(errmsgbuf, n, "Need sender to permit access.");
		return (ERROR + USERNAME_REQUIRED);
	}

	begin_critical_section(S_NETCONFIGS);
	RNCfg = CtdlGetNetCfgForRoom (CC->room.QRnumber);
	if (RNCfg == NULL)
	{
		end_critical_section(S_NETCONFIGS);
		snprintf(errmsgbuf, n,
			 "This mailing list only accepts posts from subscribers.");
		return (ERROR + NO_SUCH_USER);
	}
	found = is_recipient (RNCfg, RemoteIdentifier);
	end_critical_section(S_NETCONFIGS);

	if (found) {
		return (0);
	}
	else {
		snprintf(errmsgbuf, n,
			 "This mailing list only accepts posts from subscribers.");
		return (ERROR + NO_SUCH_USER);
	}
}



/*
 * cmd_netp() - authenticate to the server as another Citadel node polling
 *	      for network traffic
 */
void cmd_netp(char *cmdbuf)
{
	struct CitContext *CCC = CC;
	HashList *working_ignetcfg;
	char *node;
	StrBuf *NodeStr;
	long nodelen;
	int v;
	long lens[2];
	const char *strs[2];

	const StrBuf *secret = NULL;
	const StrBuf *nexthop = NULL;
	char err_buf[SIZ] = "";

	/* Authenticate */
	node = CCC->curr_user;
	nodelen = extract_token(CCC->curr_user, cmdbuf, 0, '|', sizeof CCC->curr_user);
	NodeStr = NewStrBufPlain(node, nodelen);
	/* load the IGnet Configuration to check node validity */
	working_ignetcfg = CtdlLoadIgNetCfg();
	v = CtdlIsValidNode(&nexthop, &secret, NodeStr, working_ignetcfg, NULL);
	if (v != 0) {
		snprintf(err_buf, sizeof err_buf,
			"An unknown Citadel server called \"%s\" attempted to connect from %s [%s].\n",
			node, CCC->cs_host, CCC->cs_addr
		);
		syslog(LOG_WARNING, "%s", err_buf);
		cprintf("%d authentication failed\n", ERROR + PASSWORD_REQUIRED);

		strs[0] = CCC->cs_addr;
		lens[0] = strlen(CCC->cs_addr);
		
		strs[1] = "SRV_UNKNOWN";
		lens[1] = sizeof("SRV_UNKNOWN" - 1);

		CtdlAideFPMessage(
			err_buf,
			"IGNet Networking.",
			2, strs, (long*) &lens,
			CCC->cs_pid, 0,
			time(NULL));

		DeleteHash(&working_ignetcfg);
		FreeStrBuf(&NodeStr);
		return;
	}

	extract_token(CCC->user.password, cmdbuf, 1, '|', sizeof CCC->user.password);
	if (strcasecmp(CCC->user.password, ChrPtr(secret))) {
		snprintf(err_buf, sizeof err_buf,
			"A Citadel server at %s [%s] failed to authenticate as network node \"%s\".\n",
			CCC->cs_host, CCC->cs_addr, node
		);
		syslog(LOG_WARNING, "%s", err_buf);
		cprintf("%d authentication failed\n", ERROR + PASSWORD_REQUIRED);

		strs[0] = CCC->cs_addr;
		lens[0] = strlen(CCC->cs_addr);
		
		strs[1] = "SRV_PW";
		lens[1] = sizeof("SRV_PW" - 1);

		CtdlAideFPMessage(
			err_buf,
			"IGNet Networking.",
			2, strs,
			(long*) &lens,
			CCC->cs_pid, 0,
			time(NULL));

		DeleteHash(&working_ignetcfg);
		FreeStrBuf(&NodeStr);
		return;
	}

	if (CtdlNetworkTalkingTo(node, nodelen, NTT_CHECK)) {
		syslog(LOG_WARNING, "Duplicate session for network node <%s>", node);
		cprintf("%d Already talking to %s right now\n", ERROR + RESOURCE_BUSY, node);
		DeleteHash(&working_ignetcfg);
		FreeStrBuf(&NodeStr);
		return;
	}
	nodelen = safestrncpy(CCC->net_node, node, sizeof CCC->net_node);
	CtdlNetworkTalkingTo(CCC->net_node, nodelen, NTT_ADD);
	syslog(LOG_NOTICE, "Network node <%s> logged in from %s [%s]\n",
		CCC->net_node, CCC->cs_host, CCC->cs_addr
	);
	cprintf("%d authenticated as network node '%s'\n", CIT_OK, CCC->net_node);
	DeleteHash(&working_ignetcfg);
	FreeStrBuf(&NodeStr);
}


/*-----------------------------------------------------------------------------*
 *                 Network maps: evaluate other nodes                          *
 *-----------------------------------------------------------------------------*/

void DeleteNetMap(void *vNetMap)
{
	CtdlNetMap *TheNetMap = (CtdlNetMap*) vNetMap;
	FreeStrBuf(&TheNetMap->NodeName);
	FreeStrBuf(&TheNetMap->NextHop);
	free(TheNetMap);
}

CtdlNetMap *NewNetMap(StrBuf *SerializedNetMap)
{
	const char *Pos = NULL;
	CtdlNetMap *NM;

	/* we need at least 3 pipes and some other text so its invalid. */
	if (StrLength(SerializedNetMap) < 6)
		return NULL;
	NM = (CtdlNetMap *) malloc(sizeof(CtdlNetMap));

	NM->NodeName=NewStrBuf();
	StrBufExtract_NextToken(NM->NodeName, SerializedNetMap, &Pos, '|');

	NM->lastcontact = StrBufExtractNext_long(SerializedNetMap, &Pos, '|');

	NM->NextHop=NewStrBuf();
	StrBufExtract_NextToken(NM->NextHop, SerializedNetMap, &Pos, '|');

	return NM;
}

HashList* CtdlReadNetworkMap(void)
{
	const char *LinePos;
	char       *Cfg;
	StrBuf     *Buf;
	StrBuf     *LineBuf;
	HashList   *Hash;
	CtdlNetMap     *TheNetMap;

	Hash = NewHash(1, NULL);
	Cfg =  CtdlGetSysConfig(IGNETMAP);
	if ((Cfg == NULL) || IsEmptyStr(Cfg)) {
		if (Cfg != NULL)
			free(Cfg);
		return Hash;
	}

	Buf = NewStrBufPlain(Cfg, -1);
	free(Cfg);
	LineBuf = NewStrBufPlain(NULL, StrLength(Buf));
	LinePos = NULL;
	while (StrBufSipLine(Buf, LineBuf, &LinePos))
	{
		TheNetMap = NewNetMap(LineBuf);
		if (TheNetMap != NULL) { /* TODO: is the NodeName Uniq? */
			Put(Hash, SKEY(TheNetMap->NodeName), TheNetMap, DeleteNetMap);
		}
	}
	FreeStrBuf(&Buf);
	FreeStrBuf(&LineBuf);
	return Hash;
}

StrBuf *CtdlSerializeNetworkMap(HashList *Map)
{
	void *vMap;
	const char *key;
	long len;
	StrBuf *Ret = NewStrBuf();
	HashPos *Pos = GetNewHashPos(Map, 0);

	while (GetNextHashPos(Map, Pos, &len, &key, &vMap))
	{
		CtdlNetMap *pMap = (CtdlNetMap*) vMap;
		StrBufAppendBuf(Ret, pMap->NodeName, 0);
		StrBufAppendBufPlain(Ret, HKEY("|"), 0);

		StrBufAppendPrintf(Ret, "%ld", pMap->lastcontact, 0);
		StrBufAppendBufPlain(Ret, HKEY("|"), 0);

		StrBufAppendBuf(Ret, pMap->NextHop, 0);
		StrBufAppendBufPlain(Ret, HKEY("\n"), 0);
	}
	DeleteHashPos(&Pos);
	return Ret;
}


/*
 * Learn topology from path fields
 */
void NetworkLearnTopology(char *node, char *path, HashList *the_netmap, int *netmap_changed)
{
	CtdlNetMap *pNM = NULL;
	void *vptr;
	char nexthop[256];
	CtdlNetMap *nmptr;

	if (GetHash(the_netmap, node, strlen(node), &vptr) && 
	    (vptr != NULL))/* TODO: is the NodeName Uniq? */
	{
		pNM = (CtdlNetMap*)vptr;
		extract_token(nexthop, path, 0, '!', sizeof nexthop);
		if (!strcmp(nexthop, ChrPtr(pNM->NextHop))) {
			pNM->lastcontact = time(NULL);
			(*netmap_changed) ++;
			return;
		}
	}

	/* If we got here then it's not in the map, so add it. */
	nmptr = (CtdlNetMap *) malloc(sizeof (CtdlNetMap));
	nmptr->NodeName = NewStrBufPlain(node, -1);
	nmptr->lastcontact = time(NULL);
	nmptr->NextHop = NewStrBuf ();
	StrBufExtract_tokenFromStr(nmptr->NextHop, path, strlen(path), 0, '!');
	/* TODO: is the NodeName Uniq? */
	Put(the_netmap, SKEY(nmptr->NodeName), nmptr, DeleteNetMap);
	(*netmap_changed) ++;
}


/*
 * Check the network map and determine whether the supplied node name is
 * valid.  If it is not a neighbor node, supply the name of a neighbor node
 * which is the next hop.  If it *is* a neighbor node, we also fill in the
 * shared secret.
 */
int CtdlIsValidNode(const StrBuf **nexthop,
		    const StrBuf **secret,
		    StrBuf *node,
		    HashList *IgnetCfg,
		    HashList *the_netmap)
{
	void *vNetMap;
	void *vNodeConf;
	CtdlNodeConf *TheNode;
	CtdlNetMap *TheNetMap;

	if (StrLength(node) == 0) {
		return(-1);
	}

	/*
	 * First try the neighbor nodes
	 */
	if (GetCount(IgnetCfg) == 0) {
		syslog(LOG_INFO, "IgnetCfg is empty!\n");
		if (nexthop != NULL) {
			*nexthop = NULL;
		}
		return(-1);
	}

	/* try to find a neigbour with the name 'node' */
	if (GetHash(IgnetCfg, SKEY(node), &vNodeConf) && 
	    (vNodeConf != NULL))
	{
		TheNode = (CtdlNodeConf*)vNodeConf;
		if (secret != NULL)
			*secret = TheNode->Secret;
		return 0;		/* yup, it's a direct neighbor */
	}

	/*
	 * If we get to this point we have to see if we know the next hop
	 *//* TODO: is the NodeName Uniq? */
	if ((GetCount(the_netmap) > 0) &&
	    (GetHash(the_netmap, SKEY(node), &vNetMap)))
	{
		TheNetMap = (CtdlNetMap*)vNetMap;
		if (nexthop != NULL)
			*nexthop = TheNetMap->NextHop;
		return(0);
	}

	/*
	 * If we get to this point, the supplied node name is bogus.
	 */
	syslog(LOG_ERR, "Invalid node name <%s>\n", ChrPtr(node));
	return(-1);
}


void destroy_network_cfgs(void)
{
	HashList *pCfgTypeHash = CfgTypeHash;
	HashList *pRoomConfigs = RoomConfigs;

	RoomConfigs = NULL;
	DeleteHash(&pRoomConfigs);

	CfgTypeHash = NULL;
	DeleteHash(&pCfgTypeHash);
}

/*
 * Module entry point
 */
CTDL_MODULE_INIT(netconfig)
{
	if (!threading)
	{
		CtdlRegisterCleanupHook(destroy_network_cfgs);
		LoadAllNetConfigs ();
		CtdlRegisterProtoHook(cmd_gnet, "GNET", "Get network config");
		CtdlRegisterProtoHook(cmd_snet, "SNET", "Set network config");
		CtdlRegisterProtoHook(cmd_netp, "NETP", "Identify as network poller");
	}
	return "netconfig";
}
