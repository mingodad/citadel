
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

int network_talking_to(const char *nodename, long len, int operation);

/*
 * Operations that can be performed by network_talking_to()
 */
enum {
        NTT_ADD,
        NTT_REMOVE,
        NTT_CHECK
};
