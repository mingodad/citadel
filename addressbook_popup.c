/*
 * $Id:  $
 *//**
 * \defgroup AjaxAutoCompletion ajax-powered autocompletion...
 * \ingroup ClientPower
 */

/*@{*/
#include "webcit.h"

/**
 * \brief Address book popup results
 */
void display_address_book_inner_div(void) {
	begin_ajax_response();

	wprintf("Lorem ipsum dolor sit amet, consectetuer adipiscing elit. Nullam sed dui. Donec in nibh id orci viverra auctor. Pellentesque elementum, orci eu lacinia pulvinar, odio lorem consectetuer augue, sed rhoncus est sem tempus nibh. Ut hendrerit rhoncus lectus. Nam sit amet augue. Vestibulum pulvinar, urna a condimentum gravida, dolor dolor congue metus, vel ultrices elit nisl a lorem. Morbi aliquam mauris at enim. Integer tristique. Vestibulum et est. Vestibulum tellus massa, fringilla et, porttitor quis, fringilla sit amet, massa.  Proin neque.");

	end_ajax_response();
}


/** @} */
