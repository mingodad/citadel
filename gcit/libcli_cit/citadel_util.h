#define CITADEL_STR_SIZE	1024

int citadel_parseparms(char *line, citadel_parms *parms);

 /*
  * int citadel_sendparms(int sd, citadel_parms *parms, int expect) - sends
  * the parms structure parms to the server on descriptor sd.  Expect
  * controls whether the client expects to receive a response.
  * This returns 1 if all of the requested functions worked, otherwise it
  * returns 0.
  *
  */

int citadel_sendparms(int sd, citadel_parms *parms, char *cmd, int expect);
int citadel_recvparms(int sd, citadel_parms *parms);
int free_citadel_parms(citadel_parms **parms);

int citadel_receive_listing(int sd, citadel_list **list);
int citadel_send_listing(int sd, citadel_list *list);
int citadel_send_listing_file(int sd, char *filename);
int add_citadel_list(citadel_list **first_list, char *item);

int free_citadel_list(citadel_list **first_list);
citadel_parms *newparms();
int reset_parms(citadel_parms **);
