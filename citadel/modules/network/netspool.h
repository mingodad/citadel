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

	char *working_ignetcfg;
	NetMap *the_netmap;
};


void network_spoolout_room(char *room_to_spool, 		       
			   char *working_ignetcfg,
			   NetMap *the_netmap);
void network_do_spoolin(char *working_ignetcfg, NetMap **the_netmap, int *netmap_changed);
void network_consolidate_spoolout(char *working_ignetcfg, NetMap *the_netmap);
void free_spoolcontrol_struct(SpoolControl **scc);
int writenfree_spoolcontrol_file(SpoolControl **scc, char *filename);
int read_spoolcontrol_file(SpoolControl **scc, char *filename);
int is_recipient(SpoolControl *sc, const char *Name);
