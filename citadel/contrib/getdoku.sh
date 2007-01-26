#!/bin/bash

BASE_SITE=http://www.citadel.org



#retrieves an index document from the citadel.org website, and filters it 
# 1: URL
# 2: outfile where to put the filtered content at
GetIndex()
{
  cd /tmp/; wget -q "${BASE_SITE}/${1}"
  cat "/tmp/${1}"   | \
    grep /doku.php/ | \
    grep -v "do="   | \
    sed -e "s;.*href=\";;" \
        -e "s;\" .*;;" \
        -e "s;doku.php/;doku.php?id=;"| \
    grep "^/doku" > \
    "/tmp/$2"
}

rm -f /tmp/mainindex /tmp/doku.php*
GetIndex "doku.php?id=faq:start" mainindex

for i in `cat /tmp/mainindex`; do 
    TMPNAME=`echo $i|sed "s;.*=;;"`
    echo $i $TMPNAME
    mkdir /tmp/$TMPNAME
    GetIndex "$i" "$TMPNAME/$TMPNAME"
    for j in `cat /tmp/$TMPNAME/$TMPNAME`; do
	echo "-----------$j----------------"
	cd /tmp/$TMPNAME/; 
	DOCUMENT_NAME=`echo $j|sed -e "s;/doku.php?id=.*:;;"`
	PLAIN_NAME=`grep "$DOCUMENT_NAME" /tmp/doku*$TMPNAME |head -n1  |sed -e "s;','/doku.*;;" -e "s;.*';;"`

	echo "********** retrieving $DOCUMENT_NAME ************"
        wget -q "${BASE_SITE}/${j}&do=export_xhtmlbody"
	mv "/tmp/$TMPNAME/${j}&do=export_xhtmlbody" /tmp/$TMPNAME/$DOCUMENT_NAME

	echo "<li><a href=\"#$DOCUMENT_NAME\">$PLAIN_NAME</a></li>" >>collect_index
	echo "<a name=\"$DOCUMENT_NAME\"></a>" >>collect_bodies
        cat $DOCUMENT_NAME>>collect_bodies
    done
    (
	echo "<html><head>$TMPNAME</head><body><ul>"
	cat "/tmp/$TMPNAME/collect_index"
	echo "<hr></ul>"
	cat "/tmp/$TMPNAME/collect_bodies"
	echo "</body></html>"
	) >/tmp/`echo $TMPNAME|sed "s;:;_;g"`.html
done