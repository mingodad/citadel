struct namelist {
	struct namelist *next;
	char name[SIZ];
};

struct SpoolControl {
	long lastsent;
	struct namelist *listrecps;
	struct namelist *ignet_push_shares;
};
