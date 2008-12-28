
extern HashList *MsgHeaderHandler;
extern HashList *MimeRenderHandler;

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
	size_t length;			   /* length of the mimeatachment */
	long size_known;
	long lvalue;               /* if we put a long... */
	long msgnum;		/**< the message number on the citadel server derived from message_summary */
	const RenderMimeFuncStruct *Renderer;
};
void DestroyMime(void *vMime);


/*
 * \brief message summary structure. ???
 */
typedef struct _message_summary {
	time_t date;        /**< its creation date */
	long msgnum;		/**< the message number on the citadel server */
	int nhdr;
	int format_type;
	StrBuf *from;		/**< the author */
	StrBuf *to;		/**< the recipient */
	StrBuf *subj;		/**< the title / subject */
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

	HashList *Attachments;  /**< list of Accachments */
	HashList *Submessages;
	HashList *AttachLinks;

	HashList *AllAttach;

	int is_new;         /**< is it yet read? */
	int hasattachments;	/* does it have atachments? */


	/** The mime part of the message */
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
	readold
};

typedef void (*readloop_servcmd)(char *buf, long bufsize);

typedef struct _readloopstruct {
	ConstStr name;
	readloop_servcmd cmd;
} readloop_struct;


void readloop(long oper);
int  read_message(StrBuf *Target, const char *tmpl, long tmpllen, long msgnum, int printable_view, const StrBuf *section);


int load_msg_ptrs(char *servcmd, int with_headers);
void jsonMessageListHdr(void);
void new_summary_view(void);
void jsonMessageList(void);
