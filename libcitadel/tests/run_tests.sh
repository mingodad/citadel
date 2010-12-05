#!/bin/bash

RUN_TEST=

#./stringbuf_IO_test -p 6666 -i 0.0.0.0 -s 2 -l 50


#./stringbuf_IO_test -p 6666 -i 0.0.0.0 -s2 -b55    &
#cat posttest_blob.txt |nc 127.0.0.1 6666

#./stringbuf_IO_test -p 6666 -i 0.0.0.0 -s2 -b482    &
#cat posttest_headers.txt |nc 127.0.0.1 6666

#./stringbuf_IO_test -p 6666 -i 0.0.0.0 -s2 -b537    &
#cat posttest.txt |nc 127.0.0.1 6666


#./stringbuf_IO_test -p 6666 -i 0.0.0.0 -s2 -n1    &
#cat posttest_blob.txt |nc 127.0.0.1 6666

#./stringbuf_IO_test -p 6666 -i 0.0.0.0 -s2 -n13    &
#cat posttest_headers.txt |nc 127.0.0.1 6666

#./stringbuf_IO_test -p 6666 -i 0.0.0.0 -s2 -n14    &
#cat posttest.txt |nc 127.0.0.1 6666


echo running hashlist test
$RUN_TEST ./hashlist_test

echo running strbuf conversion tests

./stringbuf_conversion_test
cat testdata/emailaddresses/email_recipientstrings.txt |$RUN_TEST stringbuf_conversion_test -i

echo running general stringbuffer tests
$RUN_TEST ./stringbuf_test


echo running mimeparser tests

for i in testdata/mime/*; do 
	$RUN_TEST ./mimeparser_test -p -f $i
	$RUN_TEST ./mimeparser_test -p -d -f $i
	for j in `./mimeparser_test -p -d -f $i |grep part=|sed "s;part=.*|.*|\([0-9\.]*\)|.*|.*|.*|.*|;\1;"`; do 
	    $RUN_TEST ./mimeparser_test -p -d -f $i -P $j > /dev/null
	done
done

echo running XDG-mimetype lookup tests

for i in ../../webcit/static/bgcolor.gif  ../../webcit/static/resizecorner.png ../../webcit/static/roomops.js ./mimeparser_test.c; do 
    $RUN_TEST ./mime_xdg_lookup_test -f $i -x -i /usr/share/icons/gnome/24x24/mimetypes
    $RUN_TEST ./mime_xdg_lookup_test -f $i -i /usr/share/icons/gnome/24x24/mimetypes
done

for i in test.txt test.css test.htc test.jpg test.png test.ico test.vcf test.html test.htm test.wml test.wmls test.wmlc test.wmlsc test.wbmp test.blarg a.1 a; do 
    $RUN_TEST ./mime_xdg_lookup_test -f $i -x -i /usr/share/icons/gnome/24x24/mimetypes
done

