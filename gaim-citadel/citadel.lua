-- citadel.lua
-- Gaim Citadel plugin.
--
-- © 2006 David Given.
-- This code is licensed under the GPL v2. See the file COPYING in this
-- directory for the full license text.
--
-- $Id:citadel.lua 4326 2006-02-18 12:26:22Z hjalfi $

-----------------------------------------------------------------------------
--                                 GLOBALS                                 --
-----------------------------------------------------------------------------

local _
local username, servername, port
local ga, gc
local fd, gsc
local timerhandle
local noblist
local buddies = {}

-----------------------------------------------------------------------------
--                                CONSTANTS                                --
-----------------------------------------------------------------------------

-- Our version number. Remember to update!

local VERSION_NUMBER = "0.3"

-- Special values returned as Citadel's response codes.

local LISTING_FOLLOWS         = 100
local CIT_OK                  = 200
local MORE_DATA               = 300
local SEND_LISTING            = 400
local ERROR                   = 500
local BINARY_FOLLOWS          = 600
local SEND_BINARY             = 700
local START_CHAT_MODE         = 800

local INTERNAL_ERROR          = 10
local TOO_BIG                 = 11
local ILLEGAL_VALUE           = 12
local NOT_LOGGED_IN           = 20
local CMD_NOT_SUPPORTED       = 30
local PASSWORD_REQUIRED       = 40
local ALREADY_LOGGED_IN       = 41
local USERNAME_REQUIRED       = 42
local HIGHER_ACCESS_REQUIRED  = 50
local MAX_SESSIONS_EXCEEDED   = 51
local RESOURCE_BUSY           = 52
local RESOURCE_NOT_OPEN       = 53
local NOT_HERE                = 60
local INVALID_FLOOR_OPERATION = 61
local NO_SUCH_USER            = 70
local FILE_NOT_FOUND          = 71
local ROOM_NOT_FOUND          = 72
local NO_SUCH_SYSTEM          = 73
local ALREADY_EXISTS          = 74
local MESSAGE_NOT_FOUND       = 75

local ASYNC_MSG               = 900
local ASYNC_GEXP              = 02

-- Other Citadel settings.

local CITADEL_DEFAULT_PORT    = 504
local CITADEL_CONFIG_ROOM     = "My Citadel Config"
local WAITING_ROOM            = "Sent/Received Pages"
local CITADEL_BUDDY_MSG       = "__ Buddy List __"
local CITADEL_POLL_INTERVAL   = 60

-----------------------------------------------------------------------------
--                                UTILITIES                                --
-----------------------------------------------------------------------------

--local stderr = io.stderr

local function log(...)
	local s = {}
	for _, i in ipairs(arg) do
		table.insert(s, tostring(i))
	end
	s = table.concat(s)
	gaim_debug_info("citadel", (string.gsub(s, "%%", "%%")))
end

local function unexpectederror()
	error("The Citadel server said something unexpected. Giving up.")
end

local function warning(...)
	local s = {}
	for _, i in ipairs(arg) do
		table.insert(s, tostring(i))
	end
	gaim_connection_notice(gc, s)
end

local olderror = error
error = function(e)
	log("error: ", e)
	log("traceback: ", debug.traceback())
	olderror(e)
end

-----------------------------------------------------------------------------
--                                SCHEDULER                                --
-----------------------------------------------------------------------------

local taskqueue = {}
local idle
local inscheduler = false

local yield = coroutine.yield

local function schedule_now()
	if not inscheduler then
		inscheduler = true
		
		while taskqueue[1] do
			-- Pull the first task off the queue, creating it if necessary.
		
			local task = taskqueue[1]
			if (type(task) == "function") then
				task = coroutine.create(task)
				taskqueue[1] = task
			end
			
			-- Run it.
			
			local s, e = coroutine.resume(task)
			if not s then
				log("error: ", e)
				log("traceback: ", debug.traceback())
				gaim_connection_error(gc, e)
			end
	
			-- If it's not dead, then it must have yielded --- return back to C.
				
			if (coroutine.status(task) ~= "dead") then
				break
			end
			
			-- Otherwise, remove it from the queue and go again.
			
			table.remove(taskqueue, 1)
		end
		
		inscheduler = false
	end
end

local function queue(func)
	table.insert(taskqueue, func)
--[[
	table.insert(taskqueue, function()
		local i, e = pcall(func)
		if not i then
			log("coroutine died with error! ", e)
			gaim_connection_error(gc, e)
		end
	end)
--]]
end

local queued = {}
local function lazyqueue(func)
	if not queued[func] then
		queued[func] = true
		queue(
			function()
				queued[func] = nil
				func()
			end)
	end
end

-----------------------------------------------------------------------------
--                             INPUT MANGLING                              --
-----------------------------------------------------------------------------

local inputbuffer = ""

-- Read a single line of text from the server, maing Lua's coroutines do the
-- vast bulk of the work of managing Gaim's state machine for us. Woo!

local function readline()
	-- Always yield at least once. Otherwise, Lua hogs all the CPU time.

	yield()

	while true do
		if fd then
			-- Read some data from the remote server, if any's
			-- available.

			local i = interface_readdata(fd, gsc)
			if not i then
				error("Unexpected disconnection from Citadel server")
			end

			inputbuffer = inputbuffer..i

			-- Have we read a complete line of text?

			local s, e, l = string.find(inputbuffer, "^([^\n]*)\n")
			if l then
				-- If so, return it.

				inputbuffer = string.sub(inputbuffer, e+1)
				return l
			end
		end

		-- Otherwise, wait some more.
	
		yield()
	end
end

local function unpack_citadel_data_line(s, a)
	a = a or {}
	for i in string.gfind(s, "([^|]*)|?") do
		table.insert(a, i)
	end
	return a
end

-- Read in an parse a packet from the Citadel server.

local function get_response()
	local message = {}

	-- The first line of a message is of the format:
	--   123 String|String|String
	--
	-- The 123 is a response code.
		
	local s = readline()
	message.response = tonumber(string.sub(s, 1, 3))
	
	s = string.sub(s, 5)
	unpack_citadel_data_line(s, message)
	
	-- If the response code is LISTING_FOLLOWS, then there's more data
	-- coming.
	
	if (message.response == LISTING_FOLLOWS) then
		message.xargs = {}
		
		while true do
			s = readline()
			if (s == "000") then
				break
			end
			--log("Got xarg: ", s)
			table.insert(message.xargs, s)
		end
	end
	
	-- If the response code is BINARY_FOLLOWS, there's a big binary chunk
	-- coming --- which we don't support.
	
	if (message.response == BINARY_FOLLOWS) then
		error("Server sent a binary chunk, which we don't support yet")
	end
	
	return message
end

-----------------------------------------------------------------------------
--                            OUTPUT MANGLING                              --
-----------------------------------------------------------------------------

local function writeline(...)
	local s = table.concat(arg)
	
	log("send: ", s)
	interface_writedata(fd, gsc, s)
	interface_writedata(fd, gsc, "\n")
end

-----------------------------------------------------------------------------
--                           PRESENCE MANAGEMENT                           --
-----------------------------------------------------------------------------

local function cant_save_buddy_list()
	warning("Unable to send buddy list to server.")
end

local function save_buddy_list()
	writeline("GOTO "..CITADEL_CONFIG_ROOM.."||1")
	local m = get_response()
	if (m.response ~= CIT_OK) then
		cant_save_buddy_list()
		return
	end

	-- Search and destroy any old buddy list.

	writeline("MSGS ALL|0|1")
	m = get_response()
	if (m.response ~= START_CHAT_MODE) then
		cant_save_buddy_list()
		return
	end

	writeline("subj|"..CITADEL_BUDDY_MSG)
	writeline("000")
	m = nil
	while true do
		local s = readline()
		if (s == "000") then
			break
		end
		if (not m) and (s ~= "000") then
			m = s
		end
	end

	if m then
		writeline("DELE "..m)
		m = get_response()
		if (m.response ~= CIT_OK) then
			cant_save_buddy_list()
			return
		end
	end

	-- Save our buddy list.
	
	writeline("ENT0 1||0|1|"..CITADEL_BUDDY_MSG.."|")
	m = get_response()
	if (m.response ~= SEND_LISTING) then
		cant_save_buddy_list()
		return
	end

	for name, _ in pairs(buddies) do
		local b = gaim_find_buddy(ga, name)
		if b then
			local alias = gaim_buddy_get_alias(b) or ""
			local group = gaim_find_buddys_group(b)
			local groupname = gaim_group_get_name(group)
			writeline(name.."|"..alias.."|"..groupname)
		end
	end
	writeline("000")

	-- Go back to the lobby.
	
	writeline("GOTO "..WAITING_ROOM)
	get_response()
end

local function update_buddy_status()
	writeline("RWHO")
	local m = get_response()
	if (m.response ~= LISTING_FOLLOWS) then
		return
	end
	log("attempting to scan and update buddies")

	local onlinebuddies = {}
	for _, s in ipairs(m.xargs) do
		local name = unpack_citadel_data_line(s)[2]
		if (name ~= "(not logged in)") then
			onlinebuddies[name] = true
		end
	end

	-- Anyone who's not online is offline.

	for s, _ in pairs(buddies) do
		if not onlinebuddies[s] then
			serv_got_update(gc, s, false, 0, 0, 0, 0)
		end
	end

	-- Anyone who's online is, er, online.

	for s, _ in pairs(onlinebuddies) do
		-- If we're in no-buddy-list mode and this buddy isn't on our
		-- list, add them automatically.

		if noblist then
			if not gaim_find_buddy(ga, s) then
				local buddy = gaim_buddy_new(ga, s, s)
				local group = gaim_group_new("Citadel")
				if buddy then
					-- buddy is not garbage collected! This must succeed!
					gaim_blist_add_buddy(buddy, nil, group, nil)
				end
			end
		end

		serv_got_update(gc, s, true, 0, 0, 0, 0)
	end
end

-----------------------------------------------------------------------------
--                               ENTRYPOINTS                               --
-----------------------------------------------------------------------------

function citadel_schedule_now()
	schedule_now()
end

function citadel_input()
	-- If there's no task, create one to handle this input.
	
	if not taskqueue[1] then
		queue(idle)
	end
end

function citadel_setfd(_fd)
	fd = _fd
	log("fd = ", tonumber(fd))
end

function citadel_setgsc(_gsc)
	gsc = tolua.cast(_gsc, "GaimSslConnection")
	log("gsc registered")
end

function citadel_connect(_ga)
	ga = tolua.cast(_ga, "GaimAccount")
	gc = gaim_account_get_connection(ga)
	
	queue(function()
		local STEPS = 13

		username = gaim_account_get_username(ga)
		_, _, username, servername = string.find(username, "^(.*)@(.*)$")
		port = gaim_account_get_int(ga, "port", CITADEL_DEFAULT_PORT)
		noblist = gaim_account_get_bool(ga, "no_blist", false)
		
		log("connect to ", username, " on server ", servername, " port ", port)
		
		-- Make connection.
		
		gaim_connection_update_progress(gc, "Connecting", 1, STEPS)
		local i = interface_connect(ga, gc, servername, port)
		if (i ~= 0) then
			error("Unable to create socket")
		end
		
		local m = get_response()
		if (m.response ~= CIT_OK) then
			error("Unexpected response from server")
		end
		
		-- Switch to TLS mode, if desired.
		
		if gaim_account_get_bool(ga, "use_tls", true) then
			gaim_connection_update_progress(gc, "Requesting TLS", 2, STEPS)
			writeline("STLS")
			m = get_response()
			if (m.response ~= 200) then
				error("This Citadel server does not support TLS.")
			end

			-- This will always work. If the handshake fails, Lua will be
			-- shot and we don't need to worry about cleaning up.
						
			gaim_connection_update_progress(gc, "TLS handshake", 3, STEPS)
			interface_tlson(gc, ga, fd)

			-- Wait for the gsc to be hooked up.

			while not gsc do
				yield()
			end
		end
		
		-- Send username.
		
		gaim_connection_update_progress(gc, "Sending username", 4, STEPS)
		writeline("USER "..username)
		m = get_response()
		if (m.response == (ERROR+NO_SUCH_USER)) then
			error("There is no user with name '", username, "' on this server.")
		end
		if (m.response ~= MORE_DATA) then
			unexpectederror()
		end
		
		-- Send password.
		
		gaim_connection_update_progress(gc, "Sending password", 5, STEPS)
		writeline("PASS "..gaim_account_get_password(ga))
		m = get_response()
		if (m.response ~= CIT_OK) then
			error("Incorrect password.")
		end
		
		-- Tell Citadel who we are.
		
		gaim_connection_update_progress(gc, "Setting up", 6, STEPS)
		writeline("IDEN 226|0|"..VERSION_NUMBER.."|Gaim Citadel plugin|")
		m = get_response()
		
		-- Set asynchronous mode.
		
		gaim_connection_update_progress(gc, "Setting up", 7, STEPS)
		writeline("ASYN 1")
		m = get_response()
		if (m.response ~= CIT_OK) then
			error("This Citadel server does not support instant messaging.")
		end
		
		(function()
			-- Switch to private configuration room.

			gaim_connection_update_progress(gc, "Setting up", 8, STEPS)
			writeline("GOTO "..CITADEL_CONFIG_ROOM.."||1")
			m = get_response()
			if (m.response ~= CIT_OK) then
				warning("Unable to fetch buddy list from server.")
				return
			end

			-- Look for our preferences.

			gaim_connection_update_progress(gc, "Setting up", 9, STEPS)
			writeline("MSGS ALL|0|1")
			m = get_response()
			if (m.response ~= START_CHAT_MODE) then
				warning("Unable to fetch buddy list from server.")
				return
			end

			writeline("subj|"..CITADEL_BUDDY_MSG)
			writeline("000")
			m = nil
			while true do
				local s = readline()
				if (s == "000") then
					break
				end
				if (not m) and (s ~= "000") then
					m = s
				end
			end

			log("preference message in #", m)
			if not m then
				return
			end
			
			gaim_connection_update_progress(gc, "Setting up", 10, STEPS)
			writeline("MSG0 "..m)
			while true do
				local s = readline()
				if (s == "000") then
					return
				end
				if (s == "text") then
					break
				end
			end
			while true do
				local s = readline()
				if (s == "000") then
					break
				end
				
				local name, alias, groupname = unpack(unpack_citadel_data_line(s))
				if not gaim_find_buddy(ga, name) then
					local buddy = gaim_buddy_new(ga, name, alias)
					local group = gaim_group_new(groupname)
					log("adding new buddy ", name)
					if buddy then
						-- buddy is not garbage collected! This must succeed!
						gaim_blist_add_buddy(buddy, nil, group, nil)
					end
				end
			end
		end)()

		-- Update buddy list with who's online.

		gaim_connection_update_progress(gc, "Setting up", 11, STEPS)
		update_buddy_status()

		-- Go back to the Lobby.

		gaim_connection_update_progress(gc, "Setting up", 12, STEPS)
		writeline("GOTO "..WAITING_ROOM)
		get_response()

		-- Switch on the timer.
		
		timerhandle = interface_timeron(gc, 
			gaim_account_get_int(ga, "interval", CITADEL_POLL_INTERVAL)*1000)
			
		-- Done!
		
		gaim_connection_update_progress(gc, "Connected", 13, STEPS)
		gaim_connection_set_state(gc, GAIM_CONNECTED)
	end)
end

function citadel_close()
	interface_disconnect(fd or -1, gsc)
	if timerhandle then
		interface_timeroff(gc, timerhandle)
	end
	schedule_now = function() end
end

function citadel_send_im(who, what, flags)
	queue(function()
		writeline("SEXP ", who, "|-")
		local m = get_response()
		if (m.response ~= SEND_LISTING) then
			serv_got_im(gc, "Citadel", "Unable to send message", GAIM_MESSAGE_ERROR, 0);
			return
		end
		writeline(what)
		writeline("000")
	end)
end

function citadel_fetch_pending_messages()
	queue(function()
		while true do
			writeline("GEXP")
			local m = get_response()
			if (m.response ~= LISTING_FOLLOWS) then
				break
			end

			local s = table.concat(m.xargs)
			--log("got message from ", m[4], " at ", m[2], ": ", s)
			serv_got_im(gc, m[4], s, GAIM_MESSAGE_RECV, m[2])
		end
	end)
end

function citadel_get_info(name)
	queue(function()
		writeline("RBIO "..name)
		local m = get_response()
		if (m.response ~= LISTING_FOLLOWS) then
			m = "That user has been boojumed."
		else
			m = table.concat(m.xargs, "<br>")
		end

		gaim_notify_userinfo(gc, name, name.."'s biography",
			name, "Biography", m, nil, nil)
	end)
end

-----------------------------------------------------------------------------
--                                BUDDY LIST                               --
-----------------------------------------------------------------------------

function citadel_add_buddy(name)
	if not buddies[name] then
		buddies[name] = true
		lazyqueue(update_buddy_status)
		lazyqueue(save_buddy_list)
	end
end

function citadel_remove_buddy(name)
	if buddies[name] then
		buddies[name] = nil
		lazyqueue(save_buddy_list)
	end
end

function citadel_alias_buddy(name)
	if buddies[name] then
		lazyqueue(save_buddy_list)
	end
end

function citadel_group_buddy(name, oldgroup, newgroup)
	if buddies[name] then
		lazyqueue(save_buddy_list)
	end
end

function citadel_timer()
	log("tick!")
	lazyqueue(update_buddy_status)
end

-----------------------------------------------------------------------------
--                                   IDLE                                  --
-----------------------------------------------------------------------------

idle = function()
	queue(function()
		local m = get_response()
		if (m.response == (ASYNC_MSG+ASYNC_GEXP)) then
			citadel_fetch_pending_messages()
		end
	end)
end
