#!/bin/bash


export FOO=2600908b3f21ae7f692b973ed26e212d
export WELCOMEHTML=welcomemail.html
export WELCOMETXT=welcomemail.txt
export FROM=room_citadel_stats@uncensored.citadel.org
export TO=room_lobby
(
    printf "MIME-Version: 1.0\r\nContent-Type: multipart/alternative; \r\n boundary=$FOO\r\n\r\nThis is a multi-part message in MIME format.\r\n\r\n--$FOO\r\nContent-Type: text/plain; charset=utf-8\r\nContent-Transfer-Encoding: quoted-printable\r\n\r\n"; 
    cat $WELCOMETXT
    printf "\r\n\r\n--$FOO\r\nContent-Type: text/html; charset=US-ASCII\r\nContent-Transfer-Encoding: quoted-printable\r\n\r\n"
    cat $WELCOMEHTML; 
    printf "\r\n\r\n--$FOO--\r\n\r\n") | \
    citmail -bm -r "$FROM" "$TO"
