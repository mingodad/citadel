typedef struct NetMap NetMap;

struct  NetMap {
	NetMap *next;
	char nodename[SIZ];
	time_t lastcontact;
	char nexthop[SIZ];
};


NetMap *the_netmap;
int netmap_changed;
char *working_ignetcfg;

void load_working_ignetcfg(void);
void read_network_map(void);
void write_network_map(void);
int is_valid_node(char *nexthop, char *secret, char *node);
