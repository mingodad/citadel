#!/bin/sh
ICAL=/usr/local/ctdlsupport/include/libical/ical.h
if test -f /usr/include/libical/ical.h; then 
    ICAL=/usr/include/libical/ical.h
fi

if test ! -f ${ICAL}; then 
    echo "failed to locate libical headers - please install the libical development packages or heardes"
    exit 500
fi

ICALTYPES="icalproperty_kind"\
" icalcomponent_kind"\
" icalrequeststatus"\
" ical_unknown_token_handling"\
" icalrecurrencetype_frequency"\
" icalrecurrencetype_weekday"\
" icalvalue_kind"\
" icalproperty_action"\
" icalproperty_carlevel"\
" icalproperty_class"\
" icalproperty_cmd"\
" icalproperty_method"\
" icalproperty_querylevel"\
" icalproperty_status"\
" icalproperty_transp"\
" icalproperty_xlicclass"\
" icalparameter_kind"\
" icalparameter_action"\
" icalparameter_cutype"\
" icalparameter_enable"\
" icalparameter_encoding"\
" icalparameter_fbtype"\
" icalparameter_local"\
" icalparameter_partstat"\
" icalparameter_range"\
" icalparameter_related"\
" icalparameter_reltype"\
" icalparameter_role"\
" icalparameter_rsvp"\
" icalparameter_value"\
" icalparameter_xliccomparetype"\
" icalparameter_xlicerrortype"\
" icalparser_state"\
" icalerrorenum"\
" icalerrorstate"\
" icalrestriction_kind"

(
    printf '#include "webcit.h"\n\n\n'

    for icaltype in $ICALTYPES; do 
	printf "typedef struct _Ical_${icaltype} {\n"\
"	const char *Name;\n"\
"	long NameLen;\n"\
"	${icaltype} map;\n"\
"} Ical_${icaltype};\n\n\n"

    done

    for icaltype in $ICALTYPES; do 
	cat ./scripts/get_ical_data__template.sed | \
	    sed -e "s;__ICALTYPE__;$icaltype;g" > \
	    /tmp/get_ical_data.sed
    
	printf "Ical_${icaltype} ${icaltype}_map[] = {\n"
	cat ${ICAL} |\
sed -e 's;/\*.*\*/;;' -e 's;\t;;g' |\
sed -nf /tmp/get_ical_data.sed |\
sed -e "s;.*typedef *enum *${icaltype} *{\(.*\)} ${icaltype} *\;.*;\1,;" \
	    -e 's;/\*.*\*/;;' \
	    -e 's;/;\n/\n;g' \
	    -e 's;,;,\n;g' \
	    -e 's; *;;g' \
	    -e 's;^t*;;g' \
	    -e 's;\=[0-9]*;;g'|\
sed -e 's;\(.*\),;{HKEY("\1"), \1},;'
	printf '{"", 0, 0}\n};\n\n\n' 
	
    done


    printf "void \nInitModule_ICAL_MAPS\n(void)\n{\n\tint i;\n"
    for icaltype in $ICALTYPES; do 
	printf "\tfor (i=0; ${icaltype}_map[i].NameLen > 0; i++)\n"\
"		RegisterTokenParamDefine (\n"\
"			${icaltype}_map[i].Name,\n"\
"			${icaltype}_map[i].NameLen,\n"\
"			${icaltype}_map[i].map);\n"\

    done
    printf "\n}\n\n"

) > ical_maps.c
