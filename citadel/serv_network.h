
struct namelist {
	struct namelist *next;
	char name[256];
};

struct SpoolControl {
	long lastsent;
	struct namelist *listrecps;
};
