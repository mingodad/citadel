#ifndef _ICQ_H_
#define _ICQ_H_

#define ICQLIBVER 013

#include <time.h>

#define ICQ_LOG_OFF     0
#define ICQ_LOG_FATAL	1
#define ICQ_LOG_ERROR	2
#define ICQ_LOG_WARNING	7
#define ICQ_LOG_MESSAGE	9

#define STATUS_OFFLINE		  (-1L)
#define STATUS_ONLINE		    0x00
#define STATUS_INVISIBLE	  0x100
#define STATUS_INVISIBLE_2	0x10
#define STATUS_NA           0x05
#define STATUS_FREE_CHAT	  0x20
#define STATUS_OCCUPIED		  0x11
#define STATUS_AWAY         0x01
#define STATUS_DND          0x13

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

extern unsigned long icq_Status;
extern unsigned char icq_Russian;
extern unsigned char icq_LogLevel;

extern void (*icq_Logged)(void);
extern void (*icq_Disconnected)(void);
extern void (*icq_RecvMessage)(unsigned long uin, unsigned char hour, unsigned char minute, unsigned char day, unsigned char month, unsigned short year, const char *msg);
extern void (*icq_RecvURL)(unsigned long uin, unsigned char hour, unsigned char minute, unsigned char day, unsigned char month, unsigned short year, const char *url, const char *descr);
extern void (*icq_RecvAdded)(unsigned long uin, unsigned char hour, unsigned char minute, unsigned char day, unsigned char month, unsigned short year, const char *nick, const char *first, const char *last, const char *email);
extern void (*icq_RecvAuthReq)(unsigned long uin, unsigned char hour, unsigned char minute, unsigned char day, unsigned char month, unsigned short year, const char *nick, const char *first, const char *last, const char *email, const char *reason);
extern void (*icq_UserFound)(unsigned long uin, const char *nick, const char *first, const char *last, const char *email, char auth);
extern void (*icq_SearchDone)(void);
extern void (*icq_UserOnline)(unsigned long uin, unsigned long status, unsigned long ip, unsigned long port, unsigned long real_ip);
extern void (*icq_UserOffline)(unsigned long uin);
extern void (*icq_UserStatusUpdate)(unsigned long uin, unsigned long status);
extern void (*icq_InfoReply)(unsigned long uin, const char *nick, const char *first, const char *last, const char *email, char auth);
extern void (*icq_ExtInfoReply)(unsigned long uin, const char *city, unsigned short country_code, char country_stat, const char *state, unsigned short age, char gender, const char *phone, const char *hp, const char *about);
extern void (*icq_Log)(time_t time, unsigned char level, const char *str);
extern void (*icq_SrvAck)(unsigned short seq);

void icq_SetProxy(const char *phost, unsigned short pport, int pauth, const char *pname, const char *ppass);
void icq_UnsetProxy();
void icq_Init(unsigned long uin, const char *password);
int icq_Connect(const char *hostname, int port);
void icq_Disconnect();
int icq_GetSok();
int icq_GetProxySok();
void icq_HandleServerResponse();  /* should be called when data received */
void icq_HandleProxyResponse(); /* should be called when data arrived on proxy socket */
void icq_Main();
void icq_KeepAlive();
void icq_Login(unsigned long status);
void icq_Logout();
void icq_SendContactList();
void icq_SendVisibleList();
void icq_SendNewUser(unsigned long uin);
unsigned short icq_SendMessage(unsigned long uin, const char *text);
unsigned short icq_SendURL(unsigned long uin, const char *url, const char *descr);
void icq_ChangeStatus(unsigned long status);
unsigned short icq_SendInfoReq(unsigned long uin);
unsigned short icq_SendExtInfoReq(unsigned long uin);
void icq_SendAuthMsg(unsigned long uin);
void icq_SendSearchReq(const char *email, const char *nick, const char* first, const char* last);
void icq_SendSearchUINReq(unsigned long uin);

const char *icq_GetCountryName(int code);
void icq_RegNewUser(const char *pass);

void icq_ContAddUser(unsigned long cuin);
void icq_ContDelUser(unsigned long cuin);
void icq_ContClear();
void icq_ContSetVis(unsigned long cuin);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _ICQ_H_ */
#ifndef _ICQLIB_H_
#define _ICQLIB_H_

#include <time.h>
#include <stdlib.h>

typedef unsigned long DWORD;
typedef unsigned short WORD;
typedef unsigned char BYTE;
typedef signed long S_DWORD;
typedef signed short S_WORD;
typedef signed char S_BYTE;
typedef unsigned char BOOL;

typedef void (*cback)(void);

//#define SOCKREAD(s,p,l) read(s,p,l)
//#define SOCKWRITE(s,p,l) write(s,p,l)
#define SOCKCLOSE(s) close(s)

#ifndef TRUE
  #define TRUE 1
#endif

#ifndef FALSE
  #define FALSE 0
#endif

#define ICQ_VER 0x0002

#define CMD_ACK            0x000A 
#define CMD_SENDM          0x010E
#define CMD_LOGIN          0x03E8
#define CMD_CONT_LIST      0x0406
#define CMD_SEARCH_UIN     0x041a
#define CMD_SEARCH_USER    0x0424
#define CMD_KEEP_ALIVE     0x042e
#define CMD_KEEP_ALIVE2    0x051e
#define CMD_SEND_TEXT_CODE 0x0438
#define CMD_LOGIN_1        0x044c
#define CMD_INFO_REQ       0x0460
#define CMD_EXT_INFO_REQ   0x046a
#define CMD_CHANGE_PW      0x049c
#define CMD_STATUS_CHANGE  0x04d8
#define CMD_LOGIN_2        0x0528
#define CMD_UPDATE_INFO    0x050A
#define CMD_UPDATE_EXT_INFO   0X04B0
#define CMD_ADD_TO_LIST    0X053C
#define CMD_REQ_ADD_LIST   0X0456
#define CMD_QUERY_SERVERS  0X04BA
#define CMD_QUERY_ADDONS   0X04C4
#define CMD_NEW_USER_1     0X04EC
#define CMD_NEW_USER_INFO  0X04A6
#define CMD_ACK_MESSAGES   0X0442
#define CMD_MSG_TO_NEW_USER   0x0456
#define CMD_REG_NEW_USER   0x3FC
#define CMD_INVIS_LIST  0x06AE

#define SRV_ACK            0x000A
#define SRV_LOGIN_REPLY    0x005A
#define SRV_USER_ONLINE    0x006E
#define SRV_USER_OFFLINE   0x0078
#define SRV_USER_FOUND     0x008C
#define SRV_RECV_MESSAGE   0x00DC
#define SRV_END_OF_SEARCH  0x00A0
#define SRV_INFO_REPLY     0x0118
#define SRV_EXT_INFO_REPLY 0x0122
#define SRV_STATUS_UPDATE  0x01A4
#define SRV_X1             0x021C
#define SRV_X2             0x00E6
#define SRV_UPDATE         0x01E0
#define SRV_UPDATE_EXT     0x00C8
#define SRV_NEW_UIN        0x0046
#define SRV_NEW_USER       0x00B4
#define SRV_QUERY          0x0082
#define SRV_SYSTEM_MESSAGE 0x01C2
#define SRV_SYS_DELIVERED_MESS 0x0104
#define SRV_GO_AWAY        0x00F0
#define SRV_TRY_AGAIN      0x00FA

#define AUTH_MESSAGE	0x0008
#define USER_ADDED_MESS	0x000C
#define AUTH_REQ_MESS	0x0006
#define URL_MESS	0x0004

/*#define LOGIN_X1_DEF 0x00000078 */
#define LOGIN_X1_DEF 0x00040072
#define LOGIN_X2_DEF 0x06
/*#define LOGIN_X3_DEF 0x00000002*/
#define LOGIN_X3_DEF 0x00000003
/*#define LOGIN_X4_DEF 0x00000000*/
#define LOGIN_X4_DEF 0x00000000
/*#define LOGIN_X5_DEF 0x00780008*/
#define LOGIN_X5_DEF 0x00720004

typedef struct
{
  BYTE ver[2];
  BYTE cmd[2];
  BYTE seq[2];
  BYTE uin[4];
} ICQ_PAK;

typedef struct
{
  BYTE ver[2];
  BYTE cmd[2];
  BYTE seq[2];
} SRV_ICQ_PAK;

typedef struct
{
  ICQ_PAK head;
  unsigned char data[1024];
} net_icq_pak;

typedef struct
{
  SRV_ICQ_PAK head;
  unsigned char data[1024];
} srv_net_icq_pak;

typedef struct
{
  BYTE port[4];
  BYTE len[2];
} login_1;

typedef struct
{
  BYTE X1[4];
  BYTE ip[4];
  BYTE X2[1];
  BYTE status[4];
  BYTE X3[4];
  BYTE seq[2];
  BYTE X4[4];
  BYTE X5[4];
} login_2;

typedef struct
{
  BYTE uin[4];
  BYTE year[2];
  BYTE month;
  BYTE day;
  BYTE hour;
  BYTE minute;
  BYTE type[2];
  BYTE len[2];
} RECV_MESSAGE;

typedef struct
{
  BYTE uin[4];
  BYTE type[2];
  BYTE len[2];
} SIMPLE_MESSAGE;

typedef struct icq_ContItem
{
  DWORD uin;
  BOOL vis_list;
  struct icq_ContItem *next;
} icq_ContactItem;

typedef struct
{
  char *nick;
  char *first;
  char *last;
  char *email;
  BOOL auth;
} USER_INFO;

typedef struct 
{
  const char *name;
  WORD code;
} COUNTRY_CODE;

typedef struct AckCBack
{
  WORD seq;
  cback callback;
  struct AckCBack *next;
} AckCallback;

void icq_SendGotMessages();
void icq_SendLogin1();
void icq_StatusUpdate(srv_net_icq_pak pak);
void icq_AckSrv(int seq);
void icq_HandleUserOffline(srv_net_icq_pak pak);
void icq_HandleUserOnline(srv_net_icq_pak pak);
void icq_DoMsg(DWORD type, WORD len, char * data, DWORD uin, BYTE hour, BYTE minute, BYTE day, BYTE month, WORD year);
void icq_RusConv(const char to[4], char *t_in);

WORD Chars_2_Word(unsigned char *buf);
DWORD Chars_2_DW(unsigned char *buf);
void DW_2_Chars(unsigned char *buf, DWORD num);
void Word_2_Chars(unsigned char *buf, WORD num);

BOOL icq_GetServMess(WORD num);
void icq_SetServMess(WORD num);

icq_ContactItem *icq_ContFindByUin(unsigned long cuin);
icq_ContactItem *icq_ContGetFirst();

const char *icq_ConvertStatus2Str(int status);

#endif /* _ICQLIB_H_ */
