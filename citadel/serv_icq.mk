modules/serv_icq.so: serv_icq.mo
	gcc -shared -o modules/serv_icq.so serv_icq.mo

serv_icq.mo: serv_icq.c
	gcc -g -O2 -I. -DHAVE_CONFIG_H \
	-D_REENTRANT -fPIC -DPIC -c serv_icq.c -o serv_icq.mo
