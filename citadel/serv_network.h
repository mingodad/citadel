struct namelist {
	struct namelist *next;
	char name[SIZ];
};

struct SpoolControl {
	long lastsent;
	struct namelist *listrecps;
	struct namelist *ignet_push_shares;
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
