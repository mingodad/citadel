/*
 * Copyright (c) 1996-2012 by the citadel.org team
 *
 * This program is open source software.  You can redistribute it and/or
 * modify it under the terms of the GNU General Public License, version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "webcit.h"

extern IcalEnumMap icalproperty_kind_map[];

HashList *IcalComponentMap = NULL;

#if 0
void SortPregetMatter(HashList *Cals)
{
	disp_cal *Cal;
	void *vCal;
	const char *Key;
        long KLen;
	IcalEnumMap *SortMap[10];
	IcalEnumMap *Map;
	void *vSort;
	const char *Next = NULL;
	const StrBuf *SortVector;
	StrBuf *SortBy;
	int i = 0;
	HashPos *It;

	SortVector = SBSTR("ICALSortVec");
	if (SortVector == NULL)
		return;

	for (i = 0; i < 10; i++) SortMap[i] = NULL;
	SortBy = NewStrBuf();
	while (StrBufExtract_NextToken(SortBy, SortVector, &Next, ':') > 0) {
		GetHash(IcalComponentMap, SKEY(SortBy), &vSort);
		Map = (IcalEnumMap*) vSort;
		SortMap[i] = Map;
		i++;
		if (i > 9)
			break;
	}

	if (i == 0)
		return;

	switch (SortMap[i - 1]->map) {
		///	case 

	default:
		break;
	}

	It = GetNewHashPos(Cals, 0);
	while (GetNextHashPos(Cals, It, &KLen, &Key, &vCal)) {
		i = 0;
		Cal = (disp_cal*) vCal;
		Cal->Status = icalcomponent_get_status(Cal->cal);
		Cal->SortBy = Cal->cal;
		

		while ((SortMap[i] != NULL) && 
		       (Cal->SortBy != NULL)) 
		{
			/****Cal->SortBy = icalcomponent_get_first_property(Cal->SortBy, SortMap[i++]->map); */
		}
	}
}
#endif


void tmplput_ICalItem(StrBuf *Target, WCTemplputParams *TP)
{

}

void CreateIcalComponendKindLookup(void)
{
	int i = 0;

	IcalComponentMap = NewHash (1, NULL);
	while (icalproperty_kind_map[i].NameLen != 0) {
		RegisterNS(icalproperty_kind_map[i].Name, 
			   icalproperty_kind_map[i].NameLen, 
			   0, 
			   10, 
			   tmplput_ICalItem,
			   NULL, 
			   CTX_ICAL);
		Put(IcalComponentMap, 
		    icalproperty_kind_map[i].Name, 
		    icalproperty_kind_map[i].NameLen, 
		    &icalproperty_kind_map[i],
		    reference_free_handler);
			   
			   
		i++;
	}
}
















void 
InitModule_ICAL_SUBST
(void)
{
	CreateIcalComponendKindLookup ();

}

void 
ServerShutdownModule_ICAL
(void)
{
	DeleteHash(&IcalComponentMap);
}
