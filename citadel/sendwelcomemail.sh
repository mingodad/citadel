#!/bin/bash


export FOO=2600908b3f21ae7f692b973ed26e212d
export WELCOMEHTML=welcomemail.html
export WELCOMETXT=welcomemail.txt
export FROM=room_citadel_stats@uncensored.citadel.org
export TO=room_lobby
(
    printf "Subject: Welcome to your new citadel installation!\r\n"
    printf "Content-Type: text/html MIME-Version: 1.0\r\n\r\n"
    cat $WELCOMEHTML; 

) | \
	../aidepost  -rLobby -aroom_citadel_support@uncensored.citadel.org 


#    citmail -bm -r "$FROM" "$TO"
