void extract_inet_email_addrs(char *, size_t, char *, size_t, struct vCard *v, int local_addrs_only);
struct vCard *vcard_get_user(struct ctdluser *u);

enum {
	V2L_WRITE,
	V2L_DELETE
};

