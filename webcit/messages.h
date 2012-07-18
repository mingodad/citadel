
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

#define MSGFLAG_READ (1<<0)

typedef struct _message_summary {
	long msgnum;		/* the message number on the citadel server */
	int Flags;

	time_t date;     	/* its creation date */
	int nhdr;
	int format_type;
	StrBuf *euid;
	StrBuf *from;		/* the author */
	StrBuf *to;		/* the recipient */
	StrBuf *subj;		/* the title / subject */
	StrBuf *reply_inreplyto;
	StrBuf *reply_references;
	StrBuf *ReplyTo;
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

	int hasattachments;


	/* The mime part of the message */
	wc_mime_attachment *MsgBody;
} message_summary;
void DestroyMessageSummary(void *vMsg);



static inline message_summary* GetMessagePtrAt(int n, HashList *Summ)
{
	const char *Key;
	long HKLen;
	void *vMsg;

	if (Summ == NULL)
		return NULL;
	GetHashAt(Summ, n, &HKLen, &Key, &vMsg);
	return (message_summary*) vMsg;
}

typedef void (*ExamineMsgHeaderFunc)(message_summary *Msg, StrBuf *HdrLine, StrBuf *FoundCharset);

void evaluate_mime_part(message_summary *Msg, wc_mime_attachment *Mime);


typedef enum _eCustomRoomRenderer {
	eUseDefault = VIEW_JOURNAL + 100, 
	eReadEUIDS
}eCustomRoomRenderer;

enum {
	do_search,
	headers,
	readfwd,
	readnew,
	readold,
	readgt,
	readlt
};

/**
 * @brief function to parse the | separated message headers list
 * @param Line the raw line with your message data
 * @param Msg put your parser results here...
 * @param ConversionBuffer if you need some workbuffer, don't free me!
 * @returns 0: failure, trash this message. 1: all right, store it
 */
typedef int (*load_msg_ptrs_detailheaders) (StrBuf *Line, 
					    const char **pos, 
					    message_summary *Msg, 
					    StrBuf *ConversionBuffer);

typedef void (*readloop_servcmd)(char *buf, long bufsize);

typedef struct _readloopstruct {
	ConstStr name;
	readloop_servcmd cmd;
} readloop_struct;

extern readloop_struct rlid[];

void readloop(long oper, eCustomRoomRenderer ForceRenderer);
int read_message(StrBuf *Target, 
		 const char *tmpl, long tmpllen, 
		 long msgnum, 
		 const StrBuf *section, 
		 const StrBuf **OutMime);
int load_message(message_summary *Msg, 
		 StrBuf *FoundCharset,
		 StrBuf **Error);




typedef struct _SharedMessageStatus {
	long load_seen;        /* should read information be loaded */
	long sortit;           /* should we sort it using the standard sort API? */
	long defaultsortorder; /* if we should sort it, which direction should be the default? */

	long maxload;          /* how many headers should we accept from the server? defaults to 10k */
	long maxmsgs;          /* how many message bodies do you want to load at most?*/

	long startmsg;         /* which is the start message? */
	long nummsgs;          /* How many messages are available to your view? */
	long num_displayed;    /* counted up for LoadMsgFromServer */ /* TODO: unclear who should access this and why */

	long lowest_found;     /* smallest Message ID found;  */
	long highest_found;    /* highest Message ID found;  */

} SharedMessageStatus;

int load_msg_ptrs(const char *servcmd,
		  const char *filter,
		  SharedMessageStatus *Stat, 
		  load_msg_ptrs_detailheaders LH);

typedef int (*GetParamsGetServerCall_func)(SharedMessageStatus *Stat, 
					   void **ViewSpecific, 
					   long oper, 
					   char *cmd, 
					   long len,
					   char *filter,
					   long flen);

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
	 * PrintpageHeader prints the surrounding information like iconbar, header etc.
	 * by default, output_headers() is called.
	 *
	 */
	PrintViewHeader_func PrintPageHeader,

	/**
	 * PrintViewHeader is here to print informations infront of your messages.
	 * The message list is already loaded & sorted (if) so you can evaluate 
	 * its result on the SharedMessageStatus struct.
	 */
	PrintViewHeader_func PrintViewHeader,

	/**
	 * LH is the function, you specify if you want to load more than just message
	 * numbers from the server during the listing fetch operation.
	 */
	load_msg_ptrs_detailheaders LH,

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


int ParseMessageListHeaders_Detail(StrBuf *Line, 
				   const char **pos, 
				   message_summary *Msg, 
				   StrBuf *ConversionBuffer);



/**
 * @brief function to register the availability to render a specific message
 * @param HeaderName Mimetype we know howto display
 * @param HdrNLen length...
 * @param InlineRenderable Should we announce to citserver that we want to receive these mimeparts immediately?
 * @param Priority if multipart/alternative; which mimepart/Renderer should be prefered? (only applies if InlineRenderable)
 */
void RegisterMimeRenderer(const char *HeaderName, long HdrNLen, 
			  RenderMimeFunc MimeRenderer,
			  int InlineRenderable,
			  int Priority);
