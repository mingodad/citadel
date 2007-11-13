/*
 * $Id$
 *
 * Default wordbreaker module for full text indexing.
 *
 */


#include "sysdep.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <pwd.h>
#include <errno.h>
#include <sys/types.h>

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#include <sys/wait.h>
#include <ctype.h>
#include <string.h>
#include <limits.h>
#include "citadel.h"
#include "server.h"
#include "sysdep_decls.h"
#include "citserver.h"
#include "support.h"
#include "config.h"
#include "database.h"
#include "msgbase.h"
#include "control.h"
#include "tools.h"
#include "ft_wordbreaker.h"
#include "crc16.h"

/*
 * Noise words are not included in search indices.
 * NOTE: if the noise word list is altered in any way, the FT_WORDBREAKER_ID
 * must also be changed, so that the index is rebuilt.
 */
static char *noise_words[] = {
	"about",
	"after",
	"also",
	"another",
	"because",
	"been",
	"before",
	"being",
	"between",
	"both",
	"came",
	"come",
	"could",
	"each",
	"from",
	"have",
	"here",
	"himself",
	"into",
	"like",
	"make",
	"many",
	"might",
	"more",
	"most",
	"much",
	"must",
	"never",
	"only",
	"other",
	"over",
	"said",
	"same",
	"should",
	"since",
	"some",
	"still",
	"such",
	"take",
	"than",
	"that",
	"their",
	"them",
	"then",
	"there",
	"these",
	"they",
	"this",
	"those",
	"through",
	"under",
	"very",
	"well",
	"were",
	"what",
	"where",
	"which",
	"while",
	"with",
	"would",
	"your"
};

/*
 * Compare function
 */
int intcmp(const void *rec1, const void *rec2) {
	int i1, i2;

	i1 = *(const int *)rec1;
	i2 = *(const int *)rec2;

	if (i1 > i2) return(1);
	if (i1 < i2) return(-1);
	return(0);
}


void wordbreaker(char *text, int *num_tokens, int **tokens) {

	int wb_num_tokens = 0;
	int wb_num_alloc = 0;
	int *wb_tokens = NULL;

	char *ptr;
	char *word_start;
	char *word_end;
	char ch;
	int word_len;
	char word[256];
	int i;
	int word_crc;

	if (text == NULL) {		/* no NULL text please */
		*num_tokens = 0;
		*tokens = NULL;
		return;
	}

	if (text[0] == 0) {		/* no empty text either */
		*num_tokens = 0;
		*tokens = NULL;
		return;
	}

	ptr = text;
	word_start = NULL;
	while (*ptr) {
		ch = *ptr;
		if (isalnum(ch)) {
			if (!word_start) {
				word_start = ptr;
			}
		}
		++ptr;
		ch = *ptr;
		if ( (!isalnum(ch)) && (word_start) ) {
			word_end = ptr;
//			--word_end;

			/* extract the word */
			word_len = word_end - word_start;
			if (word_len >= sizeof word) {
				lprintf(CTDL_DEBUG, "Invalid word length: %d\n", word_len);
				safestrncpy(word, word_start, sizeof word);
				word[(sizeof word) - 1] = 0;
			}
			else {
				safestrncpy(word, word_start, word_len+1);
				word[word_len] = 0;
			}
			word_start = NULL;

			/* are we ok with the length? */
			if ( (word_len >= WB_MIN)
			   && (word_len <= WB_MAX) ) {
				for (i=0; i<word_len; ++i) {
					word[i] = tolower(word[i]);
				}
				/* disqualify noise words */
				for (i=0; i<(sizeof(noise_words)/sizeof(char *)); ++i) {
					if (!strcmp(word, noise_words[i])) {
						word_len = 0;
						break;
					}
				}
				if (word_len == 0)
					continue;

				word_crc = (int)
					CalcCRC16Bytes(word_len, word);

				++wb_num_tokens;
				if (wb_num_tokens > wb_num_alloc) {
					wb_num_alloc += 512;
					wb_tokens = realloc(wb_tokens, (sizeof(int) * wb_num_alloc));
				}
				wb_tokens[wb_num_tokens - 1] = word_crc;
			}
		}
	}

	/* sort and purge dups */
	if (wb_num_tokens > 1) {
		qsort(wb_tokens, wb_num_tokens, sizeof(int), intcmp);
		for (i=0; i<(wb_num_tokens-1); ++i) {
			if (wb_tokens[i] == wb_tokens[i+1]) {
				memmove(&wb_tokens[i], &wb_tokens[i+1],
					((wb_num_tokens - i - 1)*sizeof(int)));
				--wb_num_tokens;
				--i;
			}
		}
	}

	*num_tokens = wb_num_tokens;
	*tokens = wb_tokens;
}

