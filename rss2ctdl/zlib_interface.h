#ifndef JG_ZLIB_INTERFACE
#define JG_ZLIB_INTERFACE

enum JG_ZLIB_ERROR {
	JG_ZLIB_ERROR_OLDVERSION = -1,
	JG_ZLIB_ERROR_UNCOMPRESS = -2,
	JG_ZLIB_ERROR_NODATA = -3,
	JG_ZLIB_ERROR_BAD_MAGIC = -4,
	JG_ZLIB_ERROR_BAD_METHOD = -5,
	JG_ZLIB_ERROR_BAD_FLAGS = -6
};

extern int JG_ZLIB_DEBUG;

int jg_zlib_uncompress(void *in_buf, int in_size, 
				       void **out_buf_ptr, int *out_size,
					   int gzip);

int jg_gzip_uncompress(void *in_buf, int in_size, 
					   void **out_buf_ptr, int *out_size);

#endif
