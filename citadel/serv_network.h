struct namelist {
	struct namelist *next;
	char name[SIZ];
};

struct SpoolControl {
	long lastsent;
	struct namelist *listrecps;
	struct namelist *ignet_push_shares;
};

/*
 * Operations that can be performed by network_talking_to()
 */
enum {
	NTT_ADD,
	NTT_REMOVE,
	NTT_CHECK
};
