
extern HashList *MsgHeaderHandler;
extern HashList *MimeRenderHandler;
extern HashList *ReadLoopHandler;
typedef struct wc_mime_attachment wc_mime_attachment;
typedef void (*RenderMimeFunc)(wc_mime_attachment *Mime, StrBuf *RawData, StrBuf *FoundCharset);
typedef struct _RenderMimeFuncStruct {
	RenderMimeFunc f;
} RenderMimeFuncStruct;

struct wc_mime_attachment {
	int level;
	StrBuf *Name;
	StrBuf *FileName;
	StrBuf *PartNum;
	StrBuf *Disposition;
	StrBuf *ContentType;
	StrBuf *Charset;
	StrBuf *Data;
	size_t length;		/* length of the mimeattachment */
	long size_known;
	long lvalue;		/* if we put a long... */
	long msgnum;		/* the message number on the citadel server derived from message_summary */
	const RenderMimeFuncStruct *Renderer;
};
void DestroyMime(void *vMime);


typedef struct _message_summary {
	time_t date;     	/* its creation date */
	long msgnum;		/* the message number on the citadel server */
	int nhdr;
	int format_type;
	StrBuf *from;		/* the author */
	StrBuf *to;		/* the recipient */
	StrBuf *subj;		/* the title / subject */
	StrBuf *reply_inreplyto;
	StrBuf *reply_references;
	StrBuf *reply_to;
	StrBuf *cccc;
	StrBuf *hnod;
	StrBuf *AllRcpt;
	StrBuf *Room;
	StrBuf *Rfca;
	StrBuf *OtherNode;
	const StrBuf *PartNum;

	HashList *Attachments;  /* list of attachments */
	HashList *Submessages;
	HashList *AttachLinks;

	HashList *AllAttach;

	int is_new;
	int hasattachments;


	/* The mime part of the message */
	wc_mime_attachment *MsgBody;
} message_summary;
void DestroyMessageSummary(void *vMsg);
inline message_summary* GetMessagePtrAt(int n, HashList *Summ);

typedef void (*ExamineMsgHeaderFunc)(message_summary *Msg, StrBuf *HdrLine, StrBuf *FoundCharset);

void evaluate_mime_part(message_summary *Msg, wc_mime_attachment *Mime);

enum {
	do_search,
	headers,
	readfwd,
	readnew,
	readold,
	readgt
};

typedef void (*readloop_servcmd)(char *buf, long bufsize);

typedef struct _readloopstruct {
	ConstStr name;
	readloop_servcmd cmd;
} readloop_struct;

extern readloop_struct rlid[];


void readloop(long oper);
int read_message(StrBuf *Target, 
		 const char *tmpl, long tmpllen, 
		 long msgnum, 
		 const StrBuf *section, 
		 const StrBuf **OutMime);
int load_message(message_summary *Msg, 
		 StrBuf *FoundCharset,
		 StrBuf **Error);




typedef struct _SharedMessageStatus{
	long load_seen;        /** should read information be loaded */
	long sortit;           /** should we sort it using the standard sort API? */
	long defaultsortorder; /** if we should sort it, which direction should be the default? */

	long maxload;          /** how many headers should we accept from the server? defaults to 10k */
	long maxmsgs;          /** how many message bodies do you want to load at most?*/
	long reverse;          /** should the load-range be reversed? */

	long startmsg;         /** which is the start message ????? */
	long nummsgs;          /** How many messages are available to your view? */
	long num_displayed;    /** counted up for LoadMsgFromServer */ /* TODO: unclear who should access this and why */

	long lowest_found;     /** smallest Message ID found;  */
	long highest_found;    /** highest Message ID found;  */

}SharedMessageStatus;

int load_msg_ptrs(const char *servcmd, SharedMessageStatus *Stat);

typedef int (*GetParamsGetServerCall_func)(SharedMessageStatus *Stat, 
					   void **ViewSpecific, 
					   long oper, 
					   char *cmd, 
					   long len);

typedef int (*PrintViewHeader_func)(SharedMessageStatus *Stat, void **ViewSpecific);

typedef int (*LoadMsgFromServer_func)(SharedMessageStatus *Stat, 
				      void **ViewSpecific, 
				      message_summary* Msg, 
				      int is_new, 
				      int i);

typedef int (*RenderView_or_Tail_func)(SharedMessageStatus *Stat, 
				       void **ViewSpecific, 
				       long oper);
typedef int (*View_Cleanup_func)(void **ViewSpecific);

void RegisterReadLoopHandlerset(
	/**
	 * RoomType: which View definition are you going to be called for
	 */
	int RoomType,

	/**
	 * GetParamsGetServerCall should do the following:
	 *  * allocate your private context structure
	 *  * evaluate your commandline arguments, put results to your private struct.
	 *  * fill cmd with the command to load the message pointer list:
	 *    * might depend on bstr/oper depending on your needs
	 *    * might stay empty if no list should loaded and LoadMsgFromServer 
	 *      is skipped.
	 *  * influence the behaviour by presetting values on SharedMessageStatus
	 */
	GetParamsGetServerCall_func GetParamsGetServerCall,

	/**
	 * PrintViewHeader is here to print informations infront of your messages.
	 * The message list is already loaded & sorted (if) so you can evaluate 
	 * its result on the SharedMessageStatus struct.
	 */
	PrintViewHeader_func PrintViewHeader,

	/**
	 * LoadMsgFromServer is called for every message in the message list:
	 *  * which is 
	 *    * after 'startmsg'  
	 *    * up to 'maxmsgs' after your 'startmsg'
	 *  * it should load and parse messages from citserer.
	 *  * depending on your needs you might want to print your message here...
	 *  * if cmd was empty, its skipped alltogether.
	 */
	LoadMsgFromServer_func LoadMsgFromServer,

	/**
	 * RenderView_or_Tail is called last; 
	 *  * if you used PrintViewHeader to print messages, you might want to print 
	 *    trailing information here
	 *  * if you just pre-loaded your messages, put your render code here.
	 */
	RenderView_or_Tail_func RenderView_or_Tail,

	/**
	 * ViewCleanup should just clear your private data so all your mem can go back to 
	 * VALgrindHALLA.
	 * it also should release the content for delivery via end_burst() or wDumpContent(1);
	 */
	View_Cleanup_func ViewCleanup
	);
/*
GetParamsGetServerCall

PrintViewHeader

LoadMsgFromServer

RenderView_or_Tail
*/
