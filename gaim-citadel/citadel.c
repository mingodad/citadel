/* citadel.c
 * Gaim Citadel plugin.
 * 
 * © 2006 David Given.
 * This code is licensed under the GPL v2. See the file COPYING in this
 * directory for the full license text.
 *
 * $Id: auth.c 4258 2006-01-29 13:34:44 +0000 (Sun, 29 Jan 2006) dothebart $
 */

#define GAIM_PLUGINS
#include "internal.h"
#include "accountopt.h"
#include "blist.h"
#include "conversation.h"
#include "debug.h"
#include "notify.h"
#include "prpl.h"
#include "plugin.h"
#include "util.h"
#include "version.h"
#include "sslconn.h"

#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

#include "interface.h"

extern int tolua_gaim_open(lua_State* L);
extern void tolua_gaim_close(lua_State* L);

#define VERSION "0.2"
#define CITADEL_DEFAULT_SERVER "uncensored.citadel.org"
#define CITADEL_DEFAULT_PORT 504
#define CITADEL_POLL_INTERVAL 10
#define LUA_MICROCODE "/plugindata/citadel.lua"

struct citadel {
	GaimAccount* ga;
	GaimConnection* gc;
	GaimSslConnection* gsc;
	int fd;
	lua_State* L;
};

static GaimConnection* hackgc;

/* ======================================================================= */
/*                              LUA CALLIN                                 */
/* ======================================================================= */

static void wrappedcall(lua_State* L, int inparams, int outparams)
{
	int i;
	
	i = lua_pcall(L, inparams, outparams, 0);
	if (i)
	{
		gaim_debug(GAIM_DEBUG_MISC, "citadel", "lua error: %s\n",
			lua_tostring(L, -1));
		gaim_connection_error(hackgc, _("Internal error in plugin"));
		return;
	}
	
	lua_getglobal(L, "citadel_schedule_now");
	i = lua_pcall(L, 0, 0, 0);
	if (i)
	{
		gaim_debug(GAIM_DEBUG_MISC, "citadel", "lua error in scheduler: %s\n",
			lua_tostring(L, -1));
		gaim_connection_error(hackgc, _("Internal error in plugin"));
		return;
	}
}

/* ======================================================================= */
/*                              LUA CALLOUT                                */
/* ======================================================================= */

/* --- Input ------------------------------------------------------------- */

static void input_cb(gpointer data, gint fd, GaimInputCondition cond)
{
	GaimConnection* gc = data;
	lua_State* L = gc->proto_data;
	
	lua_getglobal(L, "citadel_input");
	wrappedcall(L, 0, 0);	
}

static void input_ssl_cb(gpointer data, GaimSslConnection* gsc, GaimInputCondition cond)
{
	GaimConnection* gc = data;
	lua_State* L = gc->proto_data;
	
	lua_getglobal(L, "citadel_input");
	wrappedcall(L, 0, 0);	
}

char* interface_readdata(int fd, GaimSslConnection* gsc)
{
	static char buffer[1024];
	int len;

	/* Read in some data. */
	
	if (gsc)
		len = gaim_ssl_read(gsc, buffer, sizeof(buffer)-1);
	else
		len = read(fd, buffer, sizeof(buffer)-1);

	if (len <= 0)
		return NULL;
		
	buffer[len] = '\0';
	return buffer;
}

int interface_writedata(int fd, GaimSslConnection* gsc, char* buffer)
{
	int len = strlen(buffer);
	char* p = buffer;

	while (len > 0)
	{
		int i;
		
		if (gsc)
			i = gaim_ssl_write(gsc, p, len);
		else
			i = write(fd, p, len);

		if (i < 0)
			return 1;
		
		p += i;
		len -= i;
	}
	
	return 0;
}

/* --- Connection -------------------------------------------------------- */

static void login_cb(gpointer data, gint fd, GaimInputCondition cond)
{
	GaimConnection* gc = data;
	lua_State* L = gc->proto_data;
	
	if (fd < 0)
	{
		gaim_connection_error(gc, _("Couldn't connect to host"));
		return;
	}

	if (!g_list_find(gaim_connections_get_all(), gc))
	{
		close(fd);
		return;
	}

	/* Register the input event handler. */
	
	gc->inpa = gaim_input_add(fd, GAIM_INPUT_READ, input_cb, gc);
	
	/* Register this file descriptor and tell Lua. */

	lua_getglobal(L, "citadel_setfd");
	lua_pushnumber(L, fd);
	wrappedcall(L, 1, 0);
}

int interface_connect(GaimAccount* ga, GaimConnection* gc,
		char* server, int port)
{
	return gaim_proxy_connect(ga, server, port, login_cb, gc);
}

void interface_disconnect(int fd, GaimSslConnection* gsc)
{
	if (gsc)
		gaim_ssl_close(gsc);
	else if (fd >= 0)
		close(fd);
}
		
/* --- TLS setup --------------------------------------------------------- */

static void ssl_setup_cb(gpointer data, GaimSslConnection* gsc,
		GaimInputCondition cond)
{
	GaimConnection* gc = data;
	lua_State* L = gc->proto_data;

	if (!g_list_find(gaim_connections_get_all(), gc))
	{
		gaim_ssl_close(gsc);
		return;
	}

	gaim_debug(GAIM_DEBUG_MISC, "citadel", "using gsc %p\n", gsc);
	gaim_ssl_input_add(gsc, input_ssl_cb, gc);
	
	/* Register this file descriptor and tell Lua. */

	lua_getglobal(L, "citadel_setgsc");
	{
		GaimSslConnection** o = lua_newuserdata(L, sizeof(GaimSslConnection));
		*o = gsc;
	}
	wrappedcall(L, 1, 0);
}

static void ssl_failure_cb(GaimSslConnection *gsc, GaimSslErrorType error,
		gpointer data)
{
	GaimConnection* gc = data;

	switch(error)
	{
		case GAIM_SSL_CONNECT_FAILED:
			gaim_connection_error(gc, _("Connection Failed"));
			break;
			
		case GAIM_SSL_HANDSHAKE_FAILED:
			gaim_connection_error(gc, _("SSL Handshake Failed"));
			break;
	}
}

void interface_tlson(GaimConnection* gc, GaimAccount* ga, int fd)
{
	gaim_input_remove(gc->inpa);
	gc->inpa = 0;
	gaim_ssl_connect_fd(ga, fd, ssl_setup_cb, ssl_failure_cb, gc);
}

/* --- Timer ------------------------------------------------------------- */

static gboolean timer_cb(gpointer data)
{
	struct lua_State* L = data;
	
	lua_getglobal(L, "citadel_timer");
	wrappedcall(L, 0, 0);
	return TRUE;
}

int interface_timeron(GaimConnection* gc, time_t interval)
{
	return gaim_timeout_add(interval, timer_cb, gc->proto_data);
}

void interface_timeroff(GaimConnection* gc, int timerhandle)
{
	gaim_timeout_remove(timerhandle);
}

/* ======================================================================= */
/*                           CONNECT/DISCONNECT                            */
/* ======================================================================= */

static void citadel_login(GaimAccount *account)
{
	GaimConnection* gc;
	lua_State* L;
	int i;
	
	/* Set up account settings. */
	
	hackgc = gc = gaim_account_get_connection(account);
	gc->flags |= GAIM_CONNECTION_NO_BGCOLOR
	           | GAIM_CONNECTION_FORMATTING_WBFO
	           | GAIM_CONNECTION_NO_FONTSIZE
               | GAIM_CONNECTION_NO_URLDESC
               | GAIM_CONNECTION_NO_IMAGES;
    
    /* Initialise our private data. */
    
	gc->proto_data = L = lua_open();
	luaopen_base(L);
	luaopen_table(L);
	luaopen_string(L);
	luaopen_math(L);
	luaopen_debug(L);
	luaopen_io(L);
	tolua_gaim_open(L);
	
	/* Register our private library. */
	
//	luaL_openlib(L, "gaimi", gaim_library, 0);
	
	/* Load in our 'microcode'. */
	
	{
		GString* microcode = g_string_new(gaim_user_dir());
		g_string_append(microcode, LUA_MICROCODE);
		
		
		gaim_debug(GAIM_DEBUG_MISC, "citadel", "loading %s\n", microcode->str);
		i = luaL_loadfile(L, microcode->str) ||
			lua_pcall(L, 0, 0, 0);
		g_string_free(microcode, TRUE);
		
		if (i)
		{
			gaim_debug(GAIM_DEBUG_MISC, "citadel", "lua error on load: %s\n",
				lua_tostring(L, -1));
			gaim_connection_error(gc, _("Unable to initialise plugin"));
			return;
		}
	}

	/* Set the reentrancy counter. */
	
	lua_pushnumber(L, 0);
	lua_setglobal(L, " entrycount");
	
	/* Tell the script to start connecting. */
	
	lua_getglobal(L, "citadel_connect");
	{
		GaimAccount** o = lua_newuserdata(L, sizeof(GaimAccount*));
		*o = account;
	}
	wrappedcall(L, 1, 0);
}

static void citadel_close(GaimConnection* gc)
{
	lua_State* L = gc->proto_data;

	if (gc->inpa)
	{
		gaim_input_remove(gc->inpa);
		gc->inpa = 0;
	}

	if (L)
	{
		gaim_debug(GAIM_DEBUG_MISC, "citadel", "telling lua to disconnect\n");
		lua_getglobal(L, "citadel_close");
		wrappedcall(L, 0, 0);
	}

	gaim_debug(GAIM_DEBUG_MISC, "citadel", "destroying lua VM\n");
	lua_close(L);
}

/* ======================================================================= */
/*                                MESSAGING                                */
/* ======================================================================= */

static int citadel_send_im(GaimConnection* gc, const char* who,
	const char* what, GaimConvImFlags flags)
{
	lua_State* L = gc->proto_data;
	
	lua_getglobal(L, "citadel_send_im");
	lua_pushstring(L, who);
	lua_pushstring(L, what);
	lua_pushnumber(L, flags);
	wrappedcall(L, 3, 0);
	
	return 1;
}

/* ======================================================================= */
/*                                PRESENCE                                 */
/* ======================================================================= */

void citadel_add_buddy(GaimConnection* gc, GaimBuddy* buddy, GaimGroup* group)
{
	lua_State* L = gc->proto_data;

	lua_getglobal(L, "citadel_add_buddy");
	lua_pushstring(L, buddy->name);
	wrappedcall(L, 1, 0);
}

void citadel_remove_buddy(GaimConnection* gc, GaimBuddy* buddy, GaimGroup* group)
{
	lua_State* L = gc->proto_data;

	lua_getglobal(L, "citadel_remove_buddy");
	lua_pushstring(L, buddy->name);
	wrappedcall(L, 1, 0);
}

void citadel_alias_buddy(GaimConnection* gc, const char* who, const char* alias)
{
	lua_State* L = gc->proto_data;

	lua_getglobal(L, "citadel_alias_buddy");
	lua_pushstring(L, who);
	wrappedcall(L, 1, 0);
}

void citadel_group_buddy(GaimConnection* gc, const char *who,
		const char *old_group, const char *new_group)
{
	lua_State* L = gc->proto_data;

	lua_getglobal(L, "citadel_group_buddy");
	lua_pushstring(L, who);
	lua_pushstring(L, old_group);
	lua_pushstring(L, new_group);
	wrappedcall(L, 3, 0);
}

/* This is really just to fill out a hole in the gaim API. */

const char* gaim_group_get_name(GaimGroup* group)
{
	return group->name;
}

/* ======================================================================= */
/*                                USER INFO                                */
/* ======================================================================= */

void citadel_get_info(GaimConnection* gc, const char* name)
{
	lua_State* L = gc->proto_data;

	lua_getglobal(L, "citadel_get_info");
	lua_pushstring(L, name);
	wrappedcall(L, 1, 0);
}

/* ======================================================================= */
/*                              MISCELLANEOUS                              */
/* ======================================================================= */

static void citadel_keepalive(GaimConnection* gc)
{
	lua_State* L = gc->proto_data;
	
	lua_getglobal(L, "citadel_keepalive");
	wrappedcall(L, 0, 0);
}

static const char* citadel_list_icon(GaimAccount* a, GaimBuddy* b)
{
	return "citadel";
}

static void citadel_list_emblems(GaimBuddy* b, char** se, char** sw,
		char** nw, char** ne)
{
	if (b->present == GAIM_BUDDY_OFFLINE)
		*se = "offline";
}

/* ======================================================================= */
/*                              PLUGIN SETUP                               */
/* ======================================================================= */

static GaimPluginProtocolInfo protocol =
{
	OPT_PROTO_CHAT_TOPIC,
	NULL,					/* user_splits */
	NULL,					/* protocol_options */
	NO_BUDDY_ICONS,			/* icon_spec */
	citadel_list_icon,      /* list_icon */
	citadel_list_emblems,   /* list_emblems */
	NULL,					/* status_text */
	NULL,					/* tooltip_text */
	NULL, //irc_away_states,		/* away_states */
	NULL,					/* blist_node_menu */
	NULL, //irc_chat_join_info,		/* chat_info */
	NULL, //irc_chat_info_defaults,	/* chat_info_defaults */
	citadel_login,          /* login */
	citadel_close,          /* close */
	citadel_send_im,        /* send_im */
	NULL,					/* set_info */
	NULL,					/* send_typing */
	citadel_get_info,                       /* get_info */
	NULL, //irc_set_away,			/* set_away */
	NULL,					/* set_idle */
	NULL,					/* change_passwd */
	citadel_add_buddy,      /* add_buddy */
	NULL,					/* add_buddies */
	citadel_remove_buddy,   /* remove_buddy */
	NULL,					/* remove_buddies */
	NULL,					/* add_permit */
	NULL,					/* add_deny */
	NULL,					/* rem_permit */
	NULL,					/* rem_deny */
	NULL,					/* set_permit_deny */
	NULL,					/* warn */
	NULL, //irc_chat_join,			/* join_chat */
	NULL,					/* reject_chat */
	NULL, //irc_get_chat_name,		/* get_chat_name */
	NULL, //irc_chat_invite,		/* chat_invite */
	NULL, //irc_chat_leave,			/* chat_leave */
	NULL,					/* chat_whisper */
	NULL, //irc_chat_send,			/* chat_send */
	citadel_keepalive,                      /* keepalive */
	NULL,					/* register_user */
	NULL,					/* get_cb_info */
	NULL,					/* get_cb_away */
	citadel_alias_buddy,    /* alias_buddy */
	citadel_group_buddy,    /* group_buddy */
	NULL,					/* rename_group */
	NULL,					/* buddy_free */
	NULL,					/* convo_closed */
	gaim_normalize_nocase,	/* normalize */
	NULL,					/* set_buddy_icon */
	NULL,					/* remove_group */
	NULL,					/* get_cb_real_name */
	NULL, //irc_chat_set_topic,		/* set_chat_topic */
	NULL,					/* find_blist_chat */
	NULL, //irc_roomlist_get_list,	/* roomlist_get_list */
	NULL, //irc_roomlist_cancel,	/* roomlist_cancel */
	NULL,					/* roomlist_expand_category */
	NULL,					/* can_receive_file */
	NULL, //irc_dccsend_send_file	/* send_file */
};

static GaimPluginInfo info =
{
	GAIM_PLUGIN_MAGIC,
	GAIM_MAJOR_VERSION,
	GAIM_MINOR_VERSION,
	GAIM_PLUGIN_PROTOCOL,                             /**< type           */
	NULL,                                             /**< ui_requirement */
	0,                                                /**< flags          */
	NULL,                                             /**< dependencies   */
	GAIM_PRIORITY_DEFAULT,                            /**< priority       */

	"prpl-citadel",                                   /**< id             */
	"Citadel",                                        /**< name           */
	VERSION,                                          /**< version        */
	N_("Citadel Protocol Plugin"),                    /**  summary        */
	N_("Instant Messaging via Citadel"),              /**  description    */
	NULL,                                             /**< author         */
	GAIM_WEBSITE,                                     /**< homepage       */

	NULL,                                             /**< load           */
	NULL,                                             /**< unload         */
	NULL,                                             /**< destroy        */

	NULL,                                             /**< ui_info        */
	&protocol,                                        /**< extra_info     */
	NULL,                                             /**< prefs_info     */
	NULL
};

static void _init_plugin(GaimPlugin *plugin)
{
	GaimAccountUserSplit *split;
	GaimAccountOption *option;

	split = gaim_account_user_split_new(_("Server"), CITADEL_DEFAULT_SERVER, '@');
	protocol.user_splits = g_list_append(protocol.user_splits, split);

	option = gaim_account_option_int_new(_("Port"), "port", CITADEL_DEFAULT_PORT);
	protocol.protocol_options = g_list_append(protocol.protocol_options, option);

	option = gaim_account_option_bool_new(_("Use TLS"), "use_tls", TRUE);
	protocol.protocol_options = g_list_append(protocol.protocol_options, option);

	option = gaim_account_option_int_new(_("Polling interval"), "interval", CITADEL_POLL_INTERVAL);
	protocol.protocol_options = g_list_append(protocol.protocol_options, option);

	gaim_prefs_add_none("/plugins/prpl/citadel");
}

GAIM_INIT_PLUGIN(citadel, _init_plugin, info);
