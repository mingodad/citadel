/*
 * $Id$
 *
 */


/*
 * This is an ID for the wordbreaker module.  If we do pluggable wordbreakers
 * later on, or even if we update this one, we can use a different ID so the
 * system knows it needs to throw away the existing index and rebuild it.
 */
#define	FT_WORDBREAKER_ID	0x001e

/*
 * Minimum and maximum length of words to index
 */
#define WB_MIN			3
#define WB_MAX			40

void wordbreaker(char *text, int *num_tokens, int **tokens);
