
struct namelist {
	struct namelist *next;
	char name[SIZ];
};

struct SpoolControl {
	long lastsent;
	struct namelist *listrecps;
};
