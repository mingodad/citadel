/*

Citadel client_api
A wrapper around the citadel api
Brian Costello
btx@calyx.net

*/

#define MAXARGS 128
#define CITADEL_API_DEVID	4		/* Code for Brian Costello */

#define LISTING_FOLLOWS         100
#define OK                      200
#define MORE_DATA               300
#define SEND_LISTING            400
#define ERROR                   500
#define BINARY_FOLLOWS          600
#define SEND_BINARY             700
#define START_CHAT_MODE         800

#define QR_PERMANENT    1               /* Room does not purge              */
#define QR_PRIVATE      4               /* Set for any type of private room */
#define QR_PASSWORDED   8               /* Set if there's a password too    */
#define QR_GUESSNAME    16              /* Set if it's a guessname room     */
#define QR_DIRECTORY    32              /* Directory room                   */
#define QR_UPLOAD       64              /* Allowed to upload                */
#define QR_DOWNLOAD     128             /* Allowed to download              */
#define QR_VISDIR       256             /* Visible directory                */
#define QR_ANONONLY     512             /* Anonymous-Only room              */
#define QR_ANON2        1024            /* Anonymous-Option room            */
#define QR_NETWORK      2048            /* Shared network room              */
#define QR_PREFONLY     4096            /* Preferred status needed to enter */
#define QR_READONLY     8192            /* Aide status required to post     */



typedef struct
{
   int return_code;     /* The return code (OK, LISTING_FOLLOWS, ETC) */
   char *line;          /* The unmodified line */
   int argc;            /* The number of args */
   char *argv[MAXARGS]; /* The args */
} citadel_parms;
            
typedef struct s_list
{
   char *listitem;              /* The list string */
   struct s_list *next;         /* A pointer to the next one */
} citadel_list;

typedef struct
{
   int  sd;
   int  connected;

   char host[256];
   u_short  port;
   
   char username[32];
   char password[32];
   
   int access_level;
   int times_called;
   int messages_posted;
   int flags;
   int user_number;
   
   int message_waiting;
   
   int devid;
   int cliid;
   int verno;
   char clientstr[256];
   char hostname[256];
   char fake_room[256];
   char fake_host[256];
   
   char roomname[256];
   char selected_room[256];
   char roompass[256];
   char selected_who[256];
   int unread_msg;
   int num_msg;
   int info_flag;
   int room_flags;
   int highest_msg_num;
   int highest_read_msg;
   int is_mail_room;
   int is_room_aide;
   int new_mail_msgs;
   int room_floor_no;
   
   citadel_list *room_msgs;
   citadel_list *next_msg_ptr;
   
   citadel_list *new_msg_rooms;
   citadel_list *next_new_msg_room;
      
} client_context;

int client_connect(citadel_parms **, client_context *, citadel_list **);
int citadel_end_session(client_context *);
int get_serv_info(client_context *);
int post_file(char *, client_context *, citadel_parms *parms);
int get_all_rooms(client_context *, citadel_list **list);
int get_all_new_rooms(client_context *, citadel_list **list);
void get_flagbuf(int, char *, int);
int goto_room(client_context *, char *, char *, citadel_parms *, int);
int get_new_msg_list(client_context *, citadel_list **list);
int get_last_msg_list(client_context *, int num_last, citadel_list **list);
int get_msg_num(client_context *, int msgnum, citadel_list **);
int get_who_list(client_context *, citadel_list **);
int send_page(client_context *, char *pagewho, char *message);
int check_page(client_context *, citadel_list **list);
