struct namelist {
	struct namelist *next;
	char name[SIZ];
};

struct SpoolControl {
	long lastsent;
	struct namelist *listrecps;
	struct namelist *ignet_push_shares;
};

struct RoomProcList {
        struct RoomProcList *next;
        char name[ROOMNAMELEN];
};

struct NetMap {
	struct NetMap *next;
	char nodename[SIZ];
	time_t lastcontact;
	char nexthop[SIZ];
};

struct UseTable {
	struct UseTable *next;
	char *message_id;
	time_t timestamp;
};

/*
 * Operations which can be performed by the network_usetable() function
 */
enum {
	UT_INSERT,
	UT_LOAD,
	UT_SAVE
};
