#define SLEEPING	180			/* TCP connection timeout */
#define PORT_NUM	32764			/* port number to listen on */
#define SERVER		"WebCit v2.0 (Velma)"	/* who's in da house */
#define DEVELOPER_ID	0
#define CLIENT_ID	4
#define CLIENT_VERSION	200
#define DEFAULT_HOST	"uncnsrd.mt-kisco.ny.us"
#define DEFAULT_PORT	"504"

struct webcontent {
	struct webcontent *next;
	char w_data[256];
	};

struct urlcontent {
	struct urlcontent *next;
	char url_key[32];
	char *url_data;
	};

struct serv_info {
	int serv_pid;
	char serv_nodename[32];
	char serv_humannode[64];
	char serv_fqdn[64];
	char serv_software[64];
	int serv_rev_level;
	char serv_bbs_city[64];
	char serv_sysadm[64];
	char serv_moreprompt[256];
	int serv_ok_floors;
	};

extern char wc_host[256];
extern char wc_port[256];
extern char wc_username[256];
extern char wc_password[256];
extern char wc_roomname[256];
extern int connected;
extern int logged_in;
extern int serv_sock;
extern struct serv_info serv_info;

void serv_printf(const char *format, ...);
char *bstr();
