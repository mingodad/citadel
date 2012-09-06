#include "webcit.h"

void display_netconf(void);

CtxType CTX_NODECONF = CTX_NONE;
/*----------------------------------------------------------------------*/
/*              Business Logic                                          */
/*----------------------------------------------------------------------*/

typedef struct _nodeconf {
	int DeleteMe;
	StrBuf *NodeName;
	StrBuf *Secret;
	StrBuf *Host;
	StrBuf *Port;
}NodeConf;

void DeleteNodeConf(void *vNode)
{
	NodeConf *Node = (NodeConf*) vNode;
	FreeStrBuf(&Node->NodeName);
	FreeStrBuf(&Node->Secret);
	FreeStrBuf(&Node->Host);
	FreeStrBuf(&Node->Port);
	free(Node);
}

NodeConf *NewNode(StrBuf *SerializedNode)
{
	NodeConf *Node;

	if (StrLength(SerializedNode) < 8) 
		return NULL; /** we need at least 4 pipes and some other text so its invalid. */
	Node = (NodeConf *) malloc(sizeof(NodeConf));
	Node->DeleteMe = 0;
	Node->NodeName=NewStrBuf();
	StrBufExtract_token(Node->NodeName, SerializedNode, 0, '|');
	Node->Secret=NewStrBuf();
	StrBufExtract_token(Node->Secret, SerializedNode, 1, '|');
	Node->Host=NewStrBuf();
	StrBufExtract_token(Node->Host, SerializedNode, 2, '|');
	Node->Port=NewStrBuf();
	StrBufExtract_token(Node->Port, SerializedNode, 3, '|');
	return Node;
}

NodeConf *HttpGetNewNode(void)
{
	NodeConf *Node;

	if (!havebstr("node") || 
	    !havebstr("secret")||
	    !havebstr("host")||
	    !havebstr("port"))
		return NULL;

	Node = (NodeConf *) malloc(sizeof(NodeConf));
	Node->DeleteMe = 0;
	Node->NodeName = NewStrBufDup(sbstr("node"));
	Node->Secret = NewStrBufDup(sbstr("secret"));
	Node->Host = NewStrBufDup(sbstr("host"));
	Node->Port = NewStrBufDup(sbstr("port"));
	return Node;
}

void SerializeNode(NodeConf *Node, StrBuf *Buf)
{
	StrBufPrintf(Buf, "%s|%s|%s|%s", 
		     ChrPtr(Node->NodeName),
		     ChrPtr(Node->Secret),
		     ChrPtr(Node->Host),
		     ChrPtr(Node->Port));
}


HashList *load_netconf(StrBuf *Target, WCTemplputParams *TP)
{
	StrBuf *Buf;
	HashList *Hash;
	char nnn[64];
	char buf[SIZ];
	int nUsed;
	NodeConf *Node;

	serv_puts("CONF getsys|application/x-citadel-ignet-config");
	serv_getln(buf, sizeof buf);
	if (buf[0] == '1') {
		Hash = NewHash(1, NULL);

		Buf = NewStrBuf();
		while (StrBuf_ServGetln(Buf), strcmp(ChrPtr(Buf), "000")) {
			Node = NewNode(Buf);
			if (Node != NULL) {
				nUsed = GetCount(Hash);
				nUsed = snprintf(nnn, sizeof(nnn), "%d", nUsed+1);
				Put(Hash, nnn, nUsed, Node, DeleteNodeConf); 
			}
		}
		FreeStrBuf(&Buf);
		return Hash;
	}
	return NULL;
}



void save_net_conf(HashList *Nodelist)
{
	char buf[SIZ];
	StrBuf *Buf;
	HashPos *where;
	void *vNode;
	NodeConf *Node;
	const char *Key;
	long KeyLen;

	serv_puts("CONF putsys|application/x-citadel-ignet-config");
	serv_getln(buf, sizeof buf);
	if (buf[0] == '4') {
		if ((Nodelist != NULL) && (GetCount(Nodelist) > 0)) {
			where = GetNewHashPos(Nodelist, 0);
			Buf = NewStrBuf();
			while (GetNextHashPos(Nodelist, where, &KeyLen, &Key, &vNode)) {
				Node = (NodeConf*) vNode;
				if (Node->DeleteMe==0) { 
					SerializeNode(Node, Buf);
					serv_putbuf(Buf);
				}
			}
			FreeStrBuf(&Buf);
			DeleteHashPos(&where);
		}
		serv_puts("000");
	}
}



/*----------------------------------------------------------------------*/
/*              WEB Handlers                                            */
/*----------------------------------------------------------------------*/



/*
 * edit a network node
 */
void edit_node(void) {
	HashList *NodeConfig;
	const StrBuf *Index;
	NodeConf *NewNode;

	if (havebstr("ok_button")) {
		Index = sbstr("index");
	        NewNode = HttpGetNewNode();
		if ((NewNode == NULL) || (Index == NULL)) {
			AppendImportantMessage(_("Invalid Parameter"), -1);
			url_do_template();
			return;
		}
			
		NodeConfig = load_netconf(NULL, &NoCtx);
		Put(NodeConfig, ChrPtr(Index), StrLength(Index), NewNode, DeleteNodeConf);
		save_net_conf(NodeConfig);
		DeleteHash(&NodeConfig);
	}
	url_do_template();
}


/*
 * modify an existing node
 */
void display_edit_node(void)
{
	WCTemplputParams SubTP;
	HashList *NodeConfig;
	const StrBuf *Index;
	void *vNode;
	const StrBuf *Tmpl;

	Index = sbstr("index");
	if (Index == NULL) {
		AppendImportantMessage(_("Invalid Parameter"), -1);
		url_do_template();
		return;
	}

	NodeConfig = load_netconf(NULL, &NoCtx);
	if (!GetHash(NodeConfig, ChrPtr(Index), StrLength(Index), &vNode) || 
	    (vNode == NULL)) {
		AppendImportantMessage(_("Invalid Parameter"), -1);
		url_do_template();
		DeleteHash(&NodeConfig);
		return;
	}
	StackContext(NULL, &SubTP, vNode, CTX_NODECONF, 0, NULL);
	{
		begin_burst();
		Tmpl = sbstr("template");
		output_headers(1, 0, 0, 0, 1, 0);
		DoTemplate(SKEY(Tmpl), NULL, &SubTP);
		end_burst();
	}
	UnStackContext(&SubTP);
	DeleteHash(&NodeConfig);
	
}


/*
 * display all configured nodes
 */
void display_netconf(void)
{
	wDumpContent(1);
}

/*
 * display the dialog to verify the deletion
 */
void display_confirm_delete_node(void)
{
	wDumpContent(1);
}


/*
 * actually delete the node
 */
void delete_node(void)
{
	HashList *NodeConfig;
	const StrBuf *Index;
	NodeConf *Node;
	void *vNode;

	Index = sbstr("index");
	if (Index == NULL) {
		AppendImportantMessage(_("Invalid Parameter"), -1);
		url_do_template();
		return;
	}

	NodeConfig = load_netconf(NULL, &NoCtx);
	if (!GetHash(NodeConfig, ChrPtr(Index), StrLength(Index), &vNode) || 
	    (vNode == NULL)) {
		AppendImportantMessage(_("Invalid Parameter"), -1);
		url_do_template();
		DeleteHash(&NodeConfig);
		return;
	}
	Node = (NodeConf *) vNode;
	Node->DeleteMe = 1;
       	save_net_conf(NodeConfig);
	DeleteHash(&NodeConfig);
	
	url_do_template();

}


void tmplput_NodeName(StrBuf *Target, WCTemplputParams *TP)
{
	NodeConf *Node = (NodeConf*) CTX(CTX_NODECONF);	
	StrBufAppendTemplate(Target, TP, Node->NodeName, 0);
}

void tmplput_Secret(StrBuf *Target, WCTemplputParams *TP)
{
	NodeConf *Node = (NodeConf*) CTX(CTX_NODECONF);
	StrBufAppendTemplate(Target, TP, Node->Secret, 0);
}

void tmplput_Host(StrBuf *Target, WCTemplputParams *TP) 
{
	NodeConf *Node= (NodeConf*) CTX(CTX_NODECONF);
	StrBufAppendTemplate(Target, TP, Node->Host, 0);
}

void tmplput_Port(StrBuf *Target, WCTemplputParams *TP)
{
	NodeConf *Node= (NodeConf*) CTX(CTX_NODECONF);
	StrBufAppendTemplate(Target, TP, Node->Port, 0);
}

void 
InitModule_NETCONF
(void)
{
	RegisterCTX(CTX_NODECONF);
	WebcitAddUrlHandler(HKEY("display_edit_node"), "", 0, display_edit_node, 0);

	WebcitAddUrlHandler(HKEY("aide_ignetconf_edit_node"), "", 0, edit_node, 0);
	WebcitAddUrlHandler(HKEY("display_netconf"), "", 0, display_netconf, 0);
	WebcitAddUrlHandler(HKEY("display_confirm_delete_node"), "", 0, display_confirm_delete_node, 0);
	WebcitAddUrlHandler(HKEY("delete_node"), "", 0, delete_node, 0);

                                                                                          
        RegisterNamespace("CFG:IGNET:NODE", 0, 1, tmplput_NodeName, NULL, CTX_NODECONF);
        RegisterNamespace("CFG:IGNET:SECRET", 0, 1, tmplput_Secret, NULL, CTX_NODECONF);
        RegisterNamespace("CFG:IGNET:HOST", 0, 1, tmplput_Host, NULL, CTX_NODECONF);
        RegisterNamespace("CFG:IGNET:PORT", 0, 1, tmplput_Port, NULL, CTX_NODECONF);

	RegisterIterator("NODECONFIG", 0, NULL, load_netconf, NULL, DeleteHash, CTX_NODECONF, CTX_NONE, IT_NOFLAG);
}
