void extract_key(char *target, char *source, char *key);

void mime_parser(char *content,
		void (*CallBack)
			(char *cbname,
			char *cbfilename,
			char *cbencoding,
			void *cbcontent,
			char *cbtype,
			size_t cblength)
		);
