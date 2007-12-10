
typedef struct namelist namelist;

struct namelist {
	namelist *next;
	char name[SIZ];
};

typedef struct maplist maplist;

struct maplist {
	struct maplist *next;
	char remote_nodename[SIZ];
	char remote_roomname[SIZ];
};

typedef struct SpoolControl SpoolControl;

struct SpoolControl {
	long lastsent;
	namelist *listrecps;
	namelist *digestrecps;
	namelist *participates;
	maplist *ignet_push_shares;
	char *misc;
	FILE *digestfp;
	int num_msgs_spooled;
};


typedef struct NetMap NetMap;

struct  NetMap {
	NetMap *next;
	char nodename[SIZ];
	time_t lastcontact;
	char nexthop[SIZ];
};

typedef struct FilterList FilterList;

struct FilterList {
	FilterList *next;
	char fl_user[SIZ];
	char fl_room[SIZ];
	char fl_node[SIZ];
};
extern FilterList *filterlist;

void free_spoolcontrol_struct(SpoolControl **scc);
int writenfree_spoolcontrol_file(SpoolControl **scc, char *filename);
int read_spoolcontrol_file(SpoolControl **scc, char *filename);

int is_recipient(SpoolControl *sc, const char *Name);


void network_queue_room(struct ctdlroom *, void *);
void destroy_network_queue_room(void);
