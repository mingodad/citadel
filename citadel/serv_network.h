struct namelist {
	struct namelist *next;
	char name[SIZ];
};

struct maplist {
	struct maplist *next;
	char remote_nodename[SIZ];
	char remote_roomname[SIZ];
};

struct SpoolControl {
	long lastsent;
	struct namelist *listrecps;
	struct namelist *digestrecps;
	struct namelist *participates;
	struct maplist *ignet_push_shares;
	char *misc;
	FILE *digestfp;
	int num_msgs_spooled;
};

struct NetMap {
	struct NetMap *next;
	char nodename[SIZ];
	time_t lastcontact;
	char nexthop[SIZ];
};


struct UseTable {
	char ut_msgid[SIZ];
	time_t ut_timestamp;
};

struct FilterList {
	struct FilterList *next;
	char fl_user[SIZ];
	char fl_room[SIZ];
	char fl_node[SIZ];
};

extern struct FilterList *filterlist;

void network_queue_room(struct ctdlroom *, void *);
void destroy_network_queue_room(void);
