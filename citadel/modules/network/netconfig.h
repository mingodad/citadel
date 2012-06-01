typedef struct NetMap NetMap;

struct  NetMap {
	NetMap *next;
	char nodename[SIZ];
	time_t lastcontact;
	char nexthop[SIZ];
};

char* load_working_ignetcfg(void);
NetMap *read_network_map(void);
void write_and_free_network_map(NetMap **the_netmap, int netmap_changed);
int is_valid_node(char *nexthop, char *secret, char *node, char *working_ignetcfg, NetMap *the_netmap);
