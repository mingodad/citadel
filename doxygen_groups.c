/*
 * ok, hacky, but gets us nice groups. so we define sub parts to join from other 
 * files here. NO CODE IN HERE! This is comment shouldn't appear in doxygen.
 * we have: 
 * CitadelConfig; WebcitDisplayItems; WebcitHttpServer; WebcitHttpServerGDav;
 * ClientPower; Calendaring; MenuInfrastructure; CitadelCommunitacion;
 * VCards
 * WebcitHttpServerRSS; tools;
 */


/**
 * \defgroup CitadelConfig Configuration Mechanisms
 * \brief Functions about configuring citadel / webcit
 */

/*@{*/
/*@}*/

/**
 * \defgroup tools  Utility Functions
 * \brief Functions that aren't related to webcit topics
 */

/*@{*/
/*@}*/


/**
 * \defgroup WebcitDisplayItems Display some mime types through webcit
 * \brief Functions that format mime types into HTML to the user
 */

/*@{*/
/*@}*/

/**
 * \defgroup WebcitHttpServer the Webserver part
 * \brief Functions that run the HTTP-Deamon
 */

/*@{*/
/*@}*/

/**
 * \defgroup WebcitHttpServerGDav Groupdav Mechanisms
 * \ingroup WebcitHttpServer
 * \brief Functions that handle groupdav requests
 */
/*@{*/
/*@}*/


/**
 * \defgroup WebcitHttpServerRSS RSS Mechanisms
 * \ingroup WebcitHttpServer
 * \brief Functions that handle RSS requests
 */

/*@{*/
/*@}*/

/**
 * \defgroup ClientPower Client powered Functionality
 * \brief Functions that spawn things on the webbrowser
 */

/*@{*/
/*@}*/

/**
 * \defgroup Calendaring Calendaring background
 * \brief Functions that make the Business-logic of the calendaring items
 * \ingroup WebcitDisplayItems
 */

/*@{*/
/*@}*/

/**
 * \defgroup VCards showing / editing VCards
 * \brief Functions that make the Business-logic of the vcard stuff
 * \ingroup WebcitDisplayItems
 */

/*@{*/
/*@}*/

/**
 * \defgroup MenuInfrastructure Things that guide you through the webcit parts
 * \brief Functions that display menues, trees etc. to connect the parts of the 
 *        ui to a whole thing
 * \ingroup WebcitDisplayItems
 */

/*@{*/
/*@}*/

/**
 * \defgroup CitadelCommunitacion Talk to the citadel server
 * \brief Functions that talk to the citadel server and process reviewed entities
 * \ingroup WebcitDisplayItems
 */

/*@{*/
/*@}*/



