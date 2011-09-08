
NetMap *the_netmap;
int netmap_changed;
char *working_ignetcfg;

void load_working_ignetcfg(void);
void read_network_map(void);
FilterList *load_filter_list(void);
void write_network_map(void);
void free_filter_list(FilterList *fl);
int is_valid_node(char *nexthop, char *secret, char *node);
