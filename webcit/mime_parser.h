void extract_key(char *target, char *source, char *key);

void do_something_with_it(char *content,
		int length,
		char *content_type,
		char *content_disposition,
		void (*CallBack)
			(char *cbname,
			char *cbfilename,
			char *cbencoding,
			void *cbcontent,
			char *cbtype,
			size_t cblength)
		);

void handle_part(char *content,
		int part_length,
		char *supplied_content_type,
		void (*CallBack)
			(char *cbname,
			char *cbfilename,
			char *cbencoding,
			void *cbcontent,
			char *cbtype,
			size_t cblength)
		);

void mime_parser(char *content,
		int ContentLength,
		char *ContentType,
		void (*CallBack)
			(char *cbname,
			char *cbfilename,
			char *cbencoding,
			void *cbcontent,
			char *cbtype,
			size_t cblength)
		);
