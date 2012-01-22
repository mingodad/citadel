
typedef struct namelist namelist;

struct namelist {
	namelist *next;
	char name[SIZ];
};


void free_netfilter_list(void);
void load_network_filter_list(void);



void network_queue_room(struct ctdlroom *, void *);
////void destroy_network_queue_room(void);
void network_bounce(struct CtdlMessage *msg, char *reason);
int network_usetable(struct CtdlMessage *msg);

