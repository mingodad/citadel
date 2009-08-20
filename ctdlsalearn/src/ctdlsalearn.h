/*
 * This is a small subset of 'struct config' ... don't worry about the rest; we
 * only need to snarf the c_ipgm_secret.
 */
struct partial_config {
	char c_nodename[16];		/* Unqualified "short" nodename     */
	char c_fqdn[64];		/* Fully Qualified Domain Name      */
	char c_humannode[21];		/* Long name of system              */
	char c_phonenum[16];		/* Dialup number of system          */
	uid_t c_ctdluid;		/* UID under which we run Citadel   */
	char c_creataide;		/* room creator = room aide  flag   */
	int c_sleeping;			/* watchdog timer setting           */
	char c_initax;			/* initial access level             */
	char c_regiscall;		/* call number to register on       */
	char c_twitdetect;		/* twit detect flag                 */
	char c_twitroom[128];		/* twit detect msg move to room     */
	char c_moreprompt[80];		/* paginator prompt                 */
	char c_restrict;		/* restrict Internet mail flag      */
	long c_niu_1;			/* (not in use)                     */
	char c_site_location[32];	/* physical location of server      */
	char c_sysadm[26];		/* name of system administrator     */
	char c_niu_2[15];		/* (not in use)                     */
	int c_setup_level;		/* what rev level we've setup to    */
	int c_maxsessions;		/* maximum concurrent sessions      */
	char c_ip_addr[20];		/* IP address to listen on          */
	int c_port_number;		/* Cit listener port (usually 504)  */
	int c_ipgm_secret;		/* Internal program authentication  */
};
