#!/bin/bash
if test -z "$1"; then
    echo "Usage: $0 mail.aliases"
    exit
fi

CITALIAS=$1
if test -f /etc/aliases; then
    cat /etc/aliases | \
	sed -e "s; *;;g" \
            -e "s;\t*;;g" | \
	grep -v ^root: | \
	grep -v ^# | \
	sed -e "s;:root;,room_aide;" \
            -e "s;:;,;" >/tmp/aliases

    NLINES=`cat /tmp/aliases|wc -l`
    
    for ((i=1; i <= $NLINES; i++)); do 
	ALIAS=`head -n $i /tmp/aliases |tail -n 1`
	ORG=`echo $ALIAS|sed "s;,.*;;"`
	if grep "$ORG" "$CITALIAS"; then
	    echo "Ignoring Alias $ORG as its already there"
	else
	    echo "$ALIAS" >>$CITALIAS
	fi
    done
else
    echo "no /etc/aliases found."
fi 