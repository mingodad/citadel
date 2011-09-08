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


void network_spoolout_room(char *room_to_spool);
void network_do_spoolin(void);
void network_consolidate_spoolout(void);
void free_spoolcontrol_struct(SpoolControl **scc);
int writenfree_spoolcontrol_file(SpoolControl **scc, char *filename);
int read_spoolcontrol_file(SpoolControl **scc, char *filename);
int is_recipient(SpoolControl *sc, const char *Name);
