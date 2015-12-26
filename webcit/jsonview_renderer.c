#include "webcit.h"
#include "webserver.h"
#include "dav.h"

int json_GetParamsGetServerCall(SharedMessageStatus *Stat, 
				void **ViewSpecific, 
				long oper, 
				char *cmd, 
				long len,
				char *filter,
				long flen)
{
	Stat->defaultsortorder = 2;
	Stat->sortit = 1;
	Stat->load_seen = 1;
	/* Generally using maxmsgs|startmsg is not required
	   in mailbox view, but we have a 'safemode' for clients
	   (*cough* Exploder) that simply can't handle too many */
	if (havebstr("maxmsgs"))  Stat->maxmsgs  = ibstr("maxmsgs");
	else                      Stat->maxmsgs  = 9999999;
	if (havebstr("startmsg")) Stat->startmsg = lbstr("startmsg");
	snprintf(cmd, len, "MSGS %s|%s||1",
		 (oper == do_search) ? "SEARCH" : "ALL",
		 (oper == do_search) ? bstr("query") : ""
		);

	return 200;
}
int json_MessageListHdr(SharedMessageStatus *Stat, void **ViewSpecific) 
{
	/* TODO: make a generic function */
	hprintf("HTTP/1.1 200 OK\r\n");
	hprintf("Content-type: application/json; charset=utf-8\r\n");
	hprintf("Server: %s / %s\r\n", PACKAGE_STRING, ChrPtr(WC->serv_info->serv_software));
	hprintf("Connection: close\r\n");
	hprintf("Pragma: no-cache\r\nCache-Control: no-store\r\nExpires:-1\r\n");
	begin_burst();
	return 0;
}

int json_RenderView_or_Tail(SharedMessageStatus *Stat, 
			    void **ViewSpecific, 
			    long oper)
{
	DoTemplate(HKEY("mailsummary_json"),NULL, NULL);
	
	return 0;
}

int json_Cleanup(void **ViewSpecific)
{
	/* Note: wDumpContent() will output one additional </div> tag. */
	/* We ought to move this out into template */
	end_burst();

	return 0;
}

void 
InitModule_JSONRENDERER
(void)
{
	RegisterReadLoopHandlerset(
		VIEW_JSON_LIST,
		json_GetParamsGetServerCall,
		json_MessageListHdr,
		NULL, /* TODO: is this right? */
		ParseMessageListHeaders_Detail,
		NULL,
		json_RenderView_or_Tail,
		json_Cleanup,
		NULL);

}
