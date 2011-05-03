#include <event_client.h>
#include <curl/curl.h>

typedef struct _evcurl_global_data {
	int magic;
	CURLM *mhnd;
	ev_timer timeev;
	int nrun;
} evcurl_global_data;

typedef struct _evcurl_request_data 
{
	evcurl_global_data *global;
	CURL *chnd;
	char errdesc[CURL_ERROR_SIZE];
	int attached;
	char* PlainPostData;
	long PlainPostDataLen;
	StrBuf *PostData;
	StrBuf *ReplyData;
	ParsedURL *URL;
	struct curl_slist * headers;
} evcurl_request_data;

typedef struct _sockwatcher_data 
{
	evcurl_global_data *global;
	ev_io ioev;
} sockwatcher_data;




#define OPT(s, v) \
	do { \
		sta = curl_easy_setopt(chnd, (CURLOPT_##s), (v)); \
		if (sta)  {						\
			CtdlLogPrintf(CTDL_ERR, "error setting option " #s " on curl handle: %s", curl_easy_strerror(sta)); \
	} } while (0)


int evcurl_init(evcurl_request_data *handle, 
		void *CustomData, 
		const char* Desc,
		int CallBack);

void evcurl_handle_start(evcurl_request_data *handle);
