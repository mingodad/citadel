/* 
 * Function to go through an ical component set and convert all non-UTC
 * date/time properties to UTC.  It also strips out any VTIMEZONE
 * subcomponents afterwards, because they're irrelevant.
 */


#include "sysdep.h"
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <syslog.h>
#include <libical/ical.h>
#include <libcitadel.h>
#include "citadel.h"
#include "server.h"
#include "citserver.h"
#include "sysdep_decls.h"
#include "support.h"
#include "config.h"
#include "ical_dezonify.h"

#include "ctdl_module.h"


/*
 * Figure out which time zone needs to be used for timestamps that are
 * not UTC and do not have a time zone specified.
 */
icaltimezone *get_default_icaltimezone(void) {

        icaltimezone *zone = NULL;
	char *default_zone_name = config.c_default_cal_zone;
	//char *default_zone_name = "America/New_York";

        if (!zone) {
                zone = icaltimezone_get_builtin_timezone(default_zone_name);
        }
        if (!zone) {
		syslog(LOG_ALERT,
			"Unable to load '%s' time zone.  Defaulting to UTC.\n",
			default_zone_name);
                zone = icaltimezone_get_utc_timezone();
	}
	if (!zone) {
		syslog(LOG_ALERT, "Unable to load UTC time zone!\n");
	}
        return zone;
}


/*
 * Back end function for ical_dezonify()
 *
 * We supply this with the master component, the relevant component,
 * and the property (which will be a DTSTART, DTEND, etc.)
 * which we want to convert to UTC.
 */
void ical_dezonify_backend(icalcomponent *cal,
			icalcomponent *rcal,
			icalproperty *prop) {

	icaltimezone *t = NULL;
	icalparameter *param;
	const char *tzid = NULL;
	struct icaltimetype TheTime;
	int utc_declared_as_tzid = 0;	/**< Component declared 'TZID=GMT' instead of using Z syntax */

	/* Give me nothing and I will give you nothing in return. */
	if (cal == NULL) return;

	/* Hunt for a TZID parameter in this property. */
	param = icalproperty_get_first_parameter(prop, ICAL_TZID_PARAMETER);

	/* Get the stringish name of this TZID. */
	if (param != NULL) {
		tzid = icalparameter_get_tzid(param);

		/* Convert it to an icaltimezone type. */
		if (tzid != NULL) {
			/* syslog(LOG_DEBUG, "                * Stringy supplied timezone is: '%s'\n", tzid); */
			if ( (!strcasecmp(tzid, "UTC")) || (!strcasecmp(tzid, "GMT")) ) {
				utc_declared_as_tzid = 1;
				/* syslog(LOG_DEBUG, "                * ...and we handle that internally.\n"); */
			}
			else {
				/* try attached first */
				t = icalcomponent_get_timezone(cal, tzid);
/*
				syslog(LOG_DEBUG, "                * ...and I %s have tzdata for that zone.\n",
					(t ? "DO" : "DO NOT")
				);
*/
				/* then try built-in timezones */
				if (!t) {
					t = icaltimezone_get_builtin_timezone(tzid);
/*
					if (t) {
						syslog(LOG_DEBUG, "                * Using system tzdata!\n");
					}
*/
				}
			}
		}

	}

	/* Now we know the timezone.  Convert to UTC. */

	if (icalproperty_isa(prop) == ICAL_DTSTART_PROPERTY) {
		TheTime = icalproperty_get_dtstart(prop);
	}
	else if (icalproperty_isa(prop) == ICAL_DTEND_PROPERTY) {
		TheTime = icalproperty_get_dtend(prop);
	}
	else if (icalproperty_isa(prop) == ICAL_DUE_PROPERTY) {
		TheTime = icalproperty_get_due(prop);
	}
	else if (icalproperty_isa(prop) == ICAL_EXDATE_PROPERTY) {
		TheTime = icalproperty_get_exdate(prop);
	}
	else {
		return;
	}

	/* syslog(LOG_DEBUG, "                * Was: %s\n", icaltime_as_ical_string(TheTime)); */

	if (TheTime.is_utc) {
		/* syslog(LOG_DEBUG, "                * This property is ALREADY UTC.\n"); */
	}

	else if (utc_declared_as_tzid) {
		/* syslog(LOG_DEBUG, "                * Replacing '%s' TZID with 'Z' suffix.\n", tzid); */
		TheTime.is_utc = 1;
	}

	else {
		/* Do the conversion. */
		if (t != NULL) {
			/* syslog(LOG_DEBUG, "                * Timezone prop found.  Converting to UTC.\n"); */
		}
		else {
			/* syslog(LOG_DEBUG, "                * Converting default timezone to UTC.\n"); */
		}

		if (t == NULL) {
			t = get_default_icaltimezone();
		}

		icaltimezone_convert_time(&TheTime,
					t,
					icaltimezone_get_utc_timezone()
		);
		TheTime.is_utc = 1;
	}

	icalproperty_remove_parameter_by_kind(prop, ICAL_TZID_PARAMETER);
	/* syslog(LOG_DEBUG, "                * Now: %s\n", icaltime_as_ical_string(TheTime)); */

	/* Now add the converted property back in. */
	if (icalproperty_isa(prop) == ICAL_DTSTART_PROPERTY) {
		icalproperty_set_dtstart(prop, TheTime);
	}
	else if (icalproperty_isa(prop) == ICAL_DTEND_PROPERTY) {
		icalproperty_set_dtend(prop, TheTime);
	}
	else if (icalproperty_isa(prop) == ICAL_DUE_PROPERTY) {
		icalproperty_set_due(prop, TheTime);
	}
	else if (icalproperty_isa(prop) == ICAL_EXDATE_PROPERTY) {
		icalproperty_set_exdate(prop, TheTime);
	}
}


/*
 * Recursive portion of ical_dezonify()
 */
void ical_dezonify_recurse(icalcomponent *cal, icalcomponent *rcal) {
	icalcomponent *c;
	icalproperty *p;

	/*
	 * Recurse through all subcomponents *except* VTIMEZONE ones.
	 */
	for (c=icalcomponent_get_first_component(
					rcal, ICAL_ANY_COMPONENT);
		c != NULL;
		c = icalcomponent_get_next_component(
					rcal, ICAL_ANY_COMPONENT)
	) {
		if (icalcomponent_isa(c) != ICAL_VTIMEZONE_COMPONENT) {
			ical_dezonify_recurse(cal, c);
		}
	}

	/*
	 * Now look for DTSTART and DTEND properties
	 */
	for (p=icalcomponent_get_first_property(rcal, ICAL_ANY_PROPERTY);
		p != NULL;
		p = icalcomponent_get_next_property(rcal, ICAL_ANY_PROPERTY)
	) {
		if (
			(icalproperty_isa(p) == ICAL_DTSTART_PROPERTY)
			|| (icalproperty_isa(p) == ICAL_DTEND_PROPERTY)
			|| (icalproperty_isa(p) == ICAL_DUE_PROPERTY)
			|| (icalproperty_isa(p) == ICAL_EXDATE_PROPERTY)
		   ) {
			ical_dezonify_backend(cal, rcal, p);
		}
	}
}


/*
 * Convert all DTSTART and DTEND properties in all subcomponents to UTC.
 * This function will search any VTIMEZONE subcomponents to learn the
 * relevant timezone information.
 */
void ical_dezonify(icalcomponent *cal) {
	icalcomponent *vt = NULL;

	/* syslog(LOG_DEBUG, "ical_dezonify() started\n"); */

	/* Convert all times to UTC */
	ical_dezonify_recurse(cal, cal);

	/* Strip out VTIMEZONE subcomponents -- we don't need them anymore */
	while (vt = icalcomponent_get_first_component(
			cal, ICAL_VTIMEZONE_COMPONENT), vt != NULL) {
		icalcomponent_remove_component(cal, vt);
		icalcomponent_free(vt);
	}

	/* syslog(LOG_DEBUG, "ical_dezonify() completed\n"); */
}
