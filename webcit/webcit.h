#define SLEEPING	180			/* TCP connection timeout */
#define PORT_NUM	32767			/* port number to listen on */
#define SERVER		"WebCit v2.0 (Velma)"	/* who's in da house */
#define DEFAULT_HOST	"uncnsrd.mt-kisco.ny.us"
#define DEFAULT_PORT	"504"

extern char wc_host[256];
extern char wc_port[256];
extern char wc_username[256];
extern char wc_password[256];
extern char wc_roomname[256];
extern int connected;
extern int logged_in;
extern int serv_sock;

struct webcontent {
	struct webcontent *next;
	char w_data[256];
	};

void serv_printf(const char *format, ...);
