#include "webcit.h"
#include "webserver.h"

static const char* AvailLang[]=
	{
		"de_DE"


	};

/* TODO: we skip the language weightening so far. */
/* Accept-Language: 'de-de,en-us;q=0.7,en;q=0.3' */
void httplang_to_locale(const char* LocaleString)
{
	char *locale="C";
	char *wanted_locales[10];
	int i=0;
	int j=0;
	size_t len=strlen(LocaleString);
	int nFound=0;
	int nAvail=1;
	char *search=(char*)malloc(len);
	int done=0;
	char *mo;
	char *webcitdir = WEBCITDIR;

	memcpy(search,LocaleString,len);
	search[len+1]='\0';
	len=strlen(search);
	/* the web browser sends '-', we need '_' */
	for (i=0;i<len; i++)
		if (search[i]=='-') search[i]='_';
	i=0;
	while ((search[i]!='\0')&&
		   !done &&
		   (nFound<10))
		{
			if ((search[i]==',')||(search[i]==';'))
		
				{
					if (search[i]==';') done=1;
					search[i]='\0';
					wanted_locales[nFound]=(char*)&search[j];
					j=i+1;
					nFound++;
				}
			
			i++;
		}
	/* todo: weight  */

	for (i=0; i<=nFound; i++)
		{
			for (j=0;j<nAvail; j++)
				{
					int ret=strncasecmp(wanted_locales[i],
										AvailLang[j],
										strlen(wanted_locales[i]));
					if (!ret)
						{
							locale=AvailLang[j];//wanted_locales[i];
							i=nFound+1;
							j=nAvail+1;
							continue;
						}

				}
		}
	setlocale(LC_ALL,"C");
	len=strlen(locale);
	memcpy(search,locale,len);
	memcpy(&search[len],".UTF8",5);
	search[len+5]='\0';
	/*
	len=strlen(search);
	mo=malloc(len+1);
	memcpy(mo,search,len+1);
	*/
	/*
	mo = malloc(strlen(webcitdir) + 20);
	sprintf(mo, "%s/locale", webcitdir);

	lprintf(9, "Message catalog directory: %s\n",
		bindtextdomain("webcit", mo)
		);
	free(mo);
	
	lprintf(9, "Text domain: %s\n",
		textdomain("webcit")
		);
	
	lprintf(9, "Text domain Charset: %s\n",
			bind_textdomain_codeset("webcit","UTF8")
			);
	
	*/
	//setlocale(LC_MESSAGES,mo);//search);
	setlocale(LC_MESSAGES,search);
	//	free(mo);
	free(search);

}
