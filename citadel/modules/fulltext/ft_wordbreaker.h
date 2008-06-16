/*
 * $Id$
 *
 */


/*
 * This is an ID for the wordbreaker module.  If we do pluggable wordbreakers
 * later on, or even if we update this one, we can use a different ID so the
 * system knows it needs to throw away the existing index and rebuild it.
 */
#define	FT_WORDBREAKER_ID	0x0021

/*
 * Minimum and maximum length of words to index
 */
#define WB_MIN			4	// nothing with 3 or less chars
#define WB_MAX			40

void wordbreaker(char *text, int *num_tokens, int **tokens);

void initialize_noise_words(void);
void noise_word_cleanup(void);


typedef struct noise_word noise_word;

struct noise_word {
	unsigned int len;
	char *word;
	noise_word *next;
};
