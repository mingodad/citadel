#!/bin/bash


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
./hashlist_test

echo running strbuf conversion tests

./stringbuf_conversion_test
cat testdata/emailaddresses/email_recipientstrings.txt |stringbuf_conversion_test -i

echo running general stringbuffer tests
./stringbuf_test


echo running mimeparser tests

for i in testdata/mime/*; do 
	./mimeparser_test -p -f $i
	./mimeparser_test -p -d -f $i
done

echo running XDG-mimetype lookup tests

for i in ../../webcit/static/bgcolor.gif  ../../webcit/static/resizecorner.png ../../webcit/static/roomops.js ./mimeparser_test.c; do 
    ./mime_xdg_lookup_test -f $i -x
    ./mime_xdg_lookup_test -f $i
done