#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>


void rgb(int red, int green, int blue)
{
	printf("	\"#%02x%02x%02x\",\n", red, green, blue);
}


main()
{
	// int pastel = 223;
	int pastel = 191;

	rgb (pastel, pastel, pastel);			// pastel grey
	rgb (255, pastel, pastel);			// pastel red
	rgb (pastel, pastel, 255);			// pastel blue
	rgb (255, 255, pastel);				// pastel yellow
	rgb (pastel, 255, pastel);			// pastel green
	rgb (255, pastel, 255);				// pastel magenta
	rgb (pastel, 255, 255);				// pastel cyan
	rgb (255, pastel, 2*pastel-255);		// pastel orange
	rgb (pastel, 2*pastel-255, 2*pastel-255);	// pastel brown

}
