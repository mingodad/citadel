#define SLEEPING	180			/* TCP connection timeout */
#define PORT_NUM	32767			/* port number to listen on */
#define SERVER		"WebCit v2.0 (Velma)"	/* who's in da house */

struct webcontent {
	struct webcontent *next;
	char w_data[256];
	};
