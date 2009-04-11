#!/bin/sh

(
printf '#include "webcit.h"\n\n\nIcalEnumMap icalproperty_kind_map[] = {\n'
cat /usr/include/libical/ical.h |\
sed 's;/\*.*\*/;;' |\
./get_ical_data.sed |\
sed -e 's;.*icalproperty_kind {\(.*\)} icalproperty_kind.*;\1,;' \
    -e 's;/\*.*\*/;;' \
    -e 's;/;\n/\n;g' \
    -e 's;,;,\n;g' \
    -e 's; *;;g' \
    -e 's;^t*;;g' \
    -e 's;\=0;;g'|\
sed -e 's;\(.*\),;{HKEY("\1"), \1},;'
printf '{"", 0, 0}\n};\n' 

)>ical_maps.c