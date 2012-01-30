/*
 * Copyright (c) 2005-2012 by the citadel.org team
 *
 *  This program is open source software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3.
 *  
 *  
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  
 *  
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

void wordbreaker(const char *text, int *num_tokens, int **tokens);

void initialize_noise_words(void);
void noise_word_cleanup(void);


typedef struct noise_word noise_word;

struct noise_word {
	unsigned int len;
	char *word;
	noise_word *next;
};
