#define DEBUG

/*
 * citadel_connect connects to the specified host/port combo.  It returns
 * the socket descriptor, or < 0 on failure.
 *
 */
    
int citadel_connect(char *host, u_short port);
    
/*
 * citadel_disconnect - Immediately closes the specified descriptor (sd).
 * Returns 1 on success, < 0 on failure.
 *
 */
        
int citadel_disconnect(int sd);
/*
 * int citadel_send(int sd, char *buf, int buflen) - Sends the buflen
 * bytes in buf to the socket sd.  Returns either the length sent or < 0
 * on failure.
 */
    
int citadel_send(int sd, char *buf, int buflen);
    
/*
 * int citadel_recv(int sd, char *buf, int buflen) - Receives up to buflen
 * bytes in buf from the socket sd.  Returns either the length received
 * or < 0 on failure.
 */
        
int citadel_recv(int sd, char buf[], int buflen);

/*
 * int citadel_recv(int sd, char *buf, int buflen) - Receives up to buflen
 * bytes in buf from the socket sd.  Returns either the length received
 * or < 0 on failure.  Buf will be filled with ONE line.  Note: THis is much
 * less efficient than calling citadel_recv, so only call this when you need
 * to.
 */

int citadel_recv_line(int sd, char buf[], int buflen);
