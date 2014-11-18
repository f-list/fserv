--[[
 * Copyright (c) 2011-2013, "Kira"
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met: 
 * 
 * 1. Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer. 
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
--]]

--[[ This file contains the event handlers for all chat messages.
	Chat messages that have been moved into native code are listed to trigger errors if they are called somehow, which is a bug.

	Lua states are considered to be disposable. This means that they may vanish and be re-created without warning,
	and any data you store in them will vanish along with it.
--]]

event = {}
rtb = {}

-- Parts a connection from a channel.
function partChannel(chan, con, is_disconnect)
	local cname = c.getName(chan)
	local lcname = string.lower(cname)
	local conname = u.getName(con)
	if is_disconnect ~= true then
		c.sendAll(chan, "LCH", {channel=cname, character=conname})
	end
	c.part(chan, con)
	local chantype = c.getType(chan)

	if (chantype == "private") and (c.canDestroy(chan) == true) and (c.getUserCount(chan) <= 0) then
		c.destroyChannel(lcname)
	elseif (chantype == "pubprivate") and (c.getUserCount(chan) <= 0) and (c.getTopUserCount(chan) < 5) then
		c.destroyChannel(lcname)
	end
end

function joinChannel(chan, con)
	local channame = c.getName(chan)
	local chantype = c.getType(chan)
	c.join(chan, con)
	if chantype == "public" then
		c.sendAll(chan, "JCH", {channel=channame, character={identity=u.getName(con)}, title=channame})
	else
		c.sendAll(chan, "JCH", {channel=channame, character={identity=u.getName(con)}, title=c.getTitle(chan)})
	end
	u.send(con, "COL", {channel=channame, array_oplist=c.getModList(chan)})
	c.sendICH(chan, con)
	u.send(con, "CDS", {channel=channame, description=c.getDescription(chan)})
end

function propagateIgnoreList(con, laction, lcharacter)
	local ignores = u.getIgnoreList(con)
	local account_cons = u.getByAccount(con)
	for i, v in ipairs(account_cons) do
		u.setIgnores(v, ignores)
		u.send(v, "IGN", {action=laction, character=lcharacter})
	end
end

-- Computes dice roll from given arguments
-- Syntax: dice_roll <connection> <args>
function dice_roll (con, diceargs)
	local odice = s.escapeHTML(diceargs)
	local dice = string.gsub(diceargs, "-", "+-")
	local steps = string.split(dice, "+")
	local results = {}
	if #steps > 20 then
		return nil
	end

	for i, step in ipairs(steps) do
		local roll = string.split(step, "d")
		if #roll == 1 then
			local num = tonumber(roll[1])
			if num == nil or num > 10000 then
				return nil
			end
			table.insert(results, num)
		else
			local rolls = tonumber(roll[1])
			local sides = tonumber(roll[2])
			local mod = 0
			if rolls == nil or sides == nil or rolls > 9  or sides > 500 or sides < 2 then
				return nil
			elseif rolls < 0 then
				rolls = math.abs(rolls)
				mod = -1
			else
				mod = 1
			end
			local sum = 0
			for v=1, rolls, 1 do
				sum = sum + math.random(sides)
			end
			table.insert(results, (mod*sum))
		end
	end
	local total = 0
	for i,v in ipairs(results) do
		total = total + v
	end
	local result = string.format("[user]%s[/user] rolls %s: ", u.getName(con), odice)
	local concatresults = ""
	if #results == 1 then
		concatresults = "[b]"..total.."[/b]"
	else
		for i,v in ipairs(results) do
			if v < 0 then
				concatresults = concatresults.." - "..math.abs(v)
			else
				concatresults = concatresults.." + "..v
			end
		end
		if results[1] >= 0 then
			concatresults = string.sub(concatresults, 4)
		end
		concatresults = concatresults.." = [b]"..total.."[/b]"
	end

	return {type="dice", array_rolls=steps, array_results=results, endresult=total, character=u.getName(con), message=result..concatresults}
end

-- Spins the bottle for a channel / private message
-- Syntax: bottle_spin <connection> <bottlers> <args>
function bottle_spin (con, bottlers)
	local conname = u.getName(con)
	if #bottlers ~= 0 then
		local picked = bottlers[math.random(#bottlers)]
		return {character=conname, type="bottle", target=picked, message=string.format("[user]%s[/user] spins the bottle: [user]%s[/user]", conname, picked)}
	end
	return nil
end

-- Bans a person by their account.
-- Syntax: ACB <character>
event.ACB =
function (con, args)
	if args.character == nil then
		return const.FERR_BAD_SYNTAX
	end

	if (u.isAdmin(con) ~= true) and (u.isGlobMod(con) ~= true) then
		return const.FERR_NOT_OP
	end

	local found, char = u.getConnection(string.lower(args.character))
	if found ~= true then
		return const.FERR_USER_NOT_FOUND
	end

	if (u.isAdmin(char) == true) or (u.isGlobMod(char) == true) then
		return const.FERR_DENIED_ON_OP
	end

	s.logAction(con, "ACB", args)
	s.addBan(char)
	u.send(con, "SYS", {message=u.getName(char).." is now account banned."})
	local account_cons = u.getByAccount(char)
	for i, v in ipairs(account_cons) do
		u.sendError(v, const.FERR_BANNED_FROM_SERVER)
		u.close(v)
	end
	return const.FERR_OK
end

-- Adds a user to the global moderator list.
-- Syntax: AOP <character>
event.AOP =
function (con, args)
	if args.character == nil then
		return const.FERR_BAD_SYNTAX
	end

	if u.isAdmin(con) ~= true then
		return const.FERR_NOT_ADMIN
	end

	local lname = string.lower(args.character)
	local found, char = u.getConnection(lname)
	if found ~= true then
		return const.FERR_USER_NOT_FOUND
	end
	local name = u.getName(char)

	if s.isOp(name) == true then
		return const.FERR_ALREADY_OP
	end

	s.logAction(con, "AOP", args)
	s.addOp(name)
	local found, char = u.getConnection(string.lower(args.character))
	if found == true then
		u.setGlobMod(char, true)
	end
	s.broadcast("AOP", {character=name})
	u.send(con, "SYS", {message=name.." has been added as a global moderator."})
	return const.FERR_OK
end

-- Adds an alt watch on a character.
-- Syntax: AWC <character>
event.AWC =
function (con, args)
	return const.FERR_NOT_IMPLEMENTED
end

-- Sends a broadcast message to the entire server.
-- Syntax: BRO <message>
event.BRO =
function (con, args)
	if args.message == nil then
		return const.FERR_BAD_SYNTAX
	end

	if u.isAdmin(con) ~= true then
		return const.FERR_NOT_ADMIN
	end

	s.logAction(con, "BRO", args)
	local mesg = s.escapeHTML(args.message)
	local lname = u.getName(con)
	s.broadcast("BRO", {character=lname, message="[b]Broadcast from "..lname..":[/b] "..mesg})
	return const.FERR_OK
end

-- Lists the bans for a channel.
-- Syntax: CBL <channel>
event.CBL =
function (con, args)
	if args.channel == nil then
		return const.FERR_BAD_SYNTAX
	end

	local found, chan = c.getChannel(string.lower(args.channel))
	if found ~= true then
		return const.FERR_CHANNEL_NOT_FOUND
	end

	--if c.isMod(chan, con) ~= true then
	--	return const.FERR_NOT_OP
	--end

	local banmessage = "Channel bans for "
	if c.getType(chan) == "public" then
		banmessage = banmessage..c.getName(chan)..": "
	else
		banmessage = banmessage..c.getTitle(chan)..": "
	end
	local banlist = c.getBanList(chan)
	local banl = ""
	for c, ban in ipairs(banlist) do
		banl= banl..", "..ban
	end
	banl = string.sub(banl, 3)
	u.send(con, "SYS", {channel=args.channel, message=banmessage..banl})
	return const.FERR_OK
end

-- Bans a user from a channel.
-- Syntax:: CKU <channel> <character>
event.CBU =
function (con, args)
	if args.channel == nil or args.character == nil then
		return const.FERR_BAD_SYNTAX
	end

	local fchan, chan = c.getChannel(string.lower(args.channel))
	if fchan ~= true then
		return const.FERR_CHANNEL_NOT_FOUND
	end

	if c.isMod(chan, con) ~= true then
		return const.FERR_NOT_OP
	end

	if c.inChannel(chan, con) ~= true then
		return const.FERR_USER_NOT_IN_CHANNEL
	end

	local targetonline, char = u.getConnection(string.lower(args.character))
	local chantype = c.getType(chan)
	if chantype == "public" then
		if targetonline == true and c.isMod(chan, char) == true then
			return const.FERR_DENIED_ON_OP
		end
		s.logAction(con, "CBU", args)
	elseif chantype == "private" then
		c.removeInvite(chan, string.lower(args.character))
	end

	if targetonline == false then
		char = args.character
		if c.isBanned(chan, string.lower(char)) == true then
			return const.FERR_ALREADY_CHANNEL_BANNED
		end
		c.sendAll(chan, "CBU", {channel=args.channel, operator=u.getName(con), character=char})
		c.ban(chan, con, string.lower(char))
	else
		if c.isBanned(chan, char) == true then
			return const.FERR_ALREADY_CHANNEL_BANNED
		end
		c.sendAll(chan, "CBU", {channel=args.channel, operator=u.getName(con), character=u.getName(char)})
		c.ban(chan, con, char)
		if c.inChannel(chan, char) == true then
			partChannel(chan, char)
		end
	end

	return const.FERR_OK
end

-- Creates a new private channel.
-- Syntax: CCR <channel>
event.CCR =
function (con, args)
	if args.channel == nil then
		return const.FERR_BAD_SYNTAX
	end

	local title = s.escapeHTML(args.channel)
	if #title > const.MAX_TITLE_LEN then
		u.sendError(con, 67, "Channel titles may not exceed "..const.MAX_TITLE_LEN.." characters in length.")
		return const.FERR_OK
	end
	local name, chan = c.createPrivateChannel(con, title)

	joinChannel(chan, con)
	return const.FERR_OK
end

-- Sets the channel description.
-- Syntax: CDS <channel> <description>
event.CDS =
function (con, args)
	if args.channel == nil or args.description == nil then
		return const.FERR_BAD_SYNTAX
	end

	local found, chan = c.getChannel(string.lower(args.channel))
	if found ~= true then
		return const.FERR_CHANNEL_NOT_FOUND
	end

	if c.isMod(chan, con) then
		if c.getType(chan) == "public" then
			s.logAction(con, "CDS", args)
		end
		local newdesc = s.escapeHTML(args.description)
		c.setDescription(chan, newdesc)
		c.sendAll(chan, "CDS", {channel=args.channel, description=newdesc})
	else
		return const.FERR_NOT_OP
	end

	return const.FERR_OK
end

-- Gets the list of public channels.
-- Syntax: CHA
event.CHA =
function (con, args)
	c.sendCHA(con)
	return const.FERR_OK
end

-- Sends an invite for a private channel to another users.
-- Syntax: CIU <channel> <character>
event.CIU =
function (con, args)
	if args.channel == nil or args.character == nil then
		return const.FERR_BAD_SYNTAX
	end

	local found, chan = c.getChannel(string.lower(args.channel))
	if found ~= true then
		return const.FERR_CHANNEL_NOT_FOUND
	end

	local fchar, char = u.getConnection(string.lower(args.character))
	if fchar ~= true then
		return const.FERR_USER_NOT_FOUND
	end

	local chantype = c.getType(chan)

	if chantype == "public" then
		return const.FERR_INVITE_TO_PUBLIC
	end

	if chantype ~= "pubprivate" and c.isMod(chan, con) ~= true then
		return const.FERR_NOT_OP
	end

	if chantype == "private" then
		c.invite(chan, char)
	end
	u.send(char, "CIU", {sender=u.getName(con), title=c.getTitle(chan), name=c.getName(chan)})
	u.send(con, "SYS", {message="Your invitation has been sent."})
	return const.FERR_OK
end

-- Kicks a user from a channel.
-- Syntax:: CKU <channel> <character>
event.CKU =
function (con, args)
	if args.channel == nil or args.character == nil then
		return const.FERR_BAD_SYNTAX
	end

	local fchan, chan = c.getChannel(string.lower(args.channel))
	if fchan ~= true then
		return const.FERR_CHANNEL_NOT_FOUND
	end

	local fchar, char = u.getConnection(string.lower(args.character))
	if fchar ~= true then
		return const.FERR_USER_NOT_FOUND
	end

	if c.isMod(chan, con) ~= true then
		return const.FERR_NOT_OP
	end

	if (c.inChannel(chan, con) ~= true) or (c.inChannel(chan, char) ~= true) then
		return const.FERR_USER_NOT_IN_CHANNEL
	end

	local chantype = c.getType(chan)
	if chantype == "public" then
		if c.isMod(chan, char) == true then
			return const.FERR_DENIED_ON_OP
		end
		s.logAction(con, "CKU", args)
	elseif chantype == "private" then
		c.removeInvite(chan, string.lower(args.character))
	end

	c.sendAll(chan, "CKU", {channel=args.channel, operator=u.getName(con), character=u.getName(char)})
	partChannel(chan, char)
	return const.FERR_OK
end

-- Adds a moderator to a channel.
-- Syntax: COA <channel> <character>
event.COA =
function (con, args)
	if args.channel == nil or args.character == nil then
		return const.FERR_BAD_SYNTAX
	end

	local found, chan = c.getChannel(string.lower(args.channel))
	if found ~= true then
		return const.FERR_CHANNEL_NOT_FOUND
	end

	if c.isOwner(chan, con) ~= true then
		return const.FERR_NOT_OP
	end

	if (c.isMod(chan, args.character) == false) or (u.isAdmin(con) == true) or (u.isGlobMod(con) == true) then
		c.addMod(chan, con, args.character)

		local modmessage = args.character.." has been added to the moderator list for "
		if c.getType(chan) == "public" then
			s.logAction(con, "COA", args)
			modmessage = modmessage..c.getName(chan)
		else
			modmessage = modmessage..c.getTitle(chan)
		end

		if args.silent == nil then
			c.sendAll(chan, "SYS", {channel=args.channel, message=modmessage})
		end
		c.sendAll(chan, "COL", {channel=args.channel, array_oplist=c.getModList(chan)})
		c.sendAll(chan, "COA", {channel=args.channel, character=args.character})
	end

	return const.FERR_OK
end

-- Lists the moderators for a channel.
-- Syntax: COL <channel>
event.COL =
function (con, args)
	if args.channel == nil then
		return const.FERR_BAD_SYNTAX
	end

	local found, chan = c.getChannel(string.lower(args.channel))
	if found ~= true then
		return const.FERR_CHANNEL_NOT_FOUND
	end

	--if c.isMod(chan, con) ~= true then
	--	return const.FERR_NOT_OP
	--end

	local opmessage = "Channel moderators for "
	if c.getType(chan) == "public" then
		opmessage = opmessage..c.getName(chan)..": "
	else
		opmessage = opmessage..c.getTitle(chan)..": "
	end
	local oplist = c.getModList(chan)
	local opl = ""
	for c, op in ipairs(oplist) do
		if op ~= "" then
			opl= opl..", "..op
		end
	end
	opl = string.sub(opl, 3)
	u.send(con, "SYS", {channel=args.channel, message=opmessage..opl})
	return const.FERR_OK
end

-- Removes a moderator from a channel.
-- Syntax: COR <channel> <character>
event.COR =
function (con, args)
	if args.channel == nil or args.character == nil then
		return const.FERR_BAD_SYNTAX
	end

	local found, chan = c.getChannel(string.lower(args.channel))
	if found ~= true then
		return const.FERR_CHANNEL_NOT_FOUND
	end

	if c.isOwner(chan, con) ~= true then
		return const.FERR_NOT_OP
	end

	if c.isMod(chan, args.character) == true then
		c.removeMod(chan, args.character)

		local modmessage = args.character.." has been removed from the moderator list for "
		if c.getType(chan) == "public" then
			s.logAction(con, "COR", args)
			modmessage = modmessage..c.getName(chan)
			local cfound, opchan = c.getChannel("adh-uberawesomestaffroom")
			if cfound == true then
				c.removeInvite(opchan, string.lower(args.character))
			end
		else
			modmessage = modmessage..c.getTitle(chan)
		end

		if args.silent == nil then
			c.sendAll(chan, "SYS", {channel=args.channel, message=modmessage})
		end
		c.sendAll(chan, "COL", {channel=args.channel, array_oplist=c.getModList(chan)})
		c.sendAll(chan, "COR", {channel=args.channel, character=args.character})
	end

	return const.FERR_OK
end

-- Creates a new public channel.
-- Syntax: CRC <channel>
event.CRC =
function (con, args)
	if args.channel == nil then
		return const.FERR_BAD_SYNTAX
	end

	if u.isAdmin(con) or u.isGlobMod(con) then
		local found, chan = c.getChannel(string.lower(args.channel))
		if found ~= true then
			c.createChannel(args.channel)
			u.send(con, "SYS", {message=args.channel .. " has been created as a public channel."})
		end
	else
		return const.NOT_OP
	end

	return const.FERR_OK
end

-- Sets a new channel owner.
-- Syntax: CSO <channel> <character>
event.CSO =
function (con, args)
	if args.channel == nil or args.character == nil then
		return const.FERR_BAD_SYNTAX
	end

	local found, chan = c.getChannel(string.lower(args.channel))
	if found ~= true then
		return const.FERR_CHANNEL_NOT_FOUND
	end

	if c.isOwner(chan, con) ~= true then
		return const.FERR_NOT_OP
	end

	local ufound, char = u.getConnection(string.lower(args.character))
	if ufound ~= true then
		return const.FERR_USER_NOT_FOUND
	end

	c.setOwner(chan, char)

	local modmessage = args.character.." is now channel owner of "
	if c.getType(chan) == "public" then
		modmessage = modmessage..c.getName(chan)
		s.logAction(con, "CSO", args)
	else
		modmessage = modmessage..c.getTitle(chan)
	end

	c.sendAll(chan, "SYS", {channel=args.channel, message=modmessage})
	c.sendAll(chan, "COL", {channel=args.channel, array_oplist=c.getModList(chan)})
	c.sendAll(chan, "CSO", {channel=args.channel, character=u.getName(char)})
	c.sendAll(chan, "COA", {channel=args.channel, character=u.getName(char)})

	return const.FERR_OK
end

-- Times out a user from a channel.
-- Syntax:: CTU <channel> <character> <length>
event.CTU =
function (con, args)
	if args.channel == nil or args.character == nil or args.length == nil then
		return const.FERR_BAD_SYNTAX
	end

	local length = tonumber(args.length)
	if length == nil or length < 1 then
		return const.FERR_BAD_TIMEOUT_FORMAT
	end
	length = length*60

	local fchan, chan = c.getChannel(string.lower(args.channel))
	if fchan ~= true then
		return const.FERR_CHANNEL_NOT_FOUND
	end

	if c.isMod(chan, con) ~= true then
		return const.FERR_NOT_OP
	end

	if c.inChannel(chan, con) ~= true then
		return const.FERR_USER_NOT_IN_CHANNEL
	end

	local targetonline, char = u.getConnection(string.lower(args.character))
	local chantype = c.getType(chan)
	if chantype == "public" then
		if c.isMod(chan, char) == true then
			return const.FERR_DENIED_ON_OP
		end
		s.logAction(con, "CTU", args)
	elseif chantype == "private" then
		c.removeInvite(chan, string.lower(args.character))
	end

	if targetonline == false then
		char = args.character
		if c.isBanned(chan, string.lower(char)) == true then
			return const.FERR_ALREADY_CHANNEL_BANNED
		end
		c.sendAll(chan, "CTU", {channel=args.channel, operator=u.getName(con), character=char, length=tonumber(args.length)})
		c.timeout(chan, con, string.lower(char), length)
	else
		if c.isBanned(chan, char) == true then
			return const.FERR_ALREADY_CHANNEL_BANNED
		end
		c.sendAll(chan, "CTU", {channel=args.channel, operator=u.getName(con), character=u.getName(char), length=tonumber(args.length)})
		c.timeout(chan, con, char, length)
		if c.inChannel(chan, char) == true then
			partChannel(chan, char)
		end
	end

	return const.FERR_OK
end

-- Unbans a user from a channel.
-- Syntax:: CUB <channel> <character>
event.CUB =
function (con, args)
	if args.channel == nil or args.character == nil then
		return const.FERR_BAD_SYNTAX
	end

	local fchan, chan = c.getChannel(string.lower(args.channel))
	if fchan ~= true then
		return const.FERR_CHANNEL_NOT_FOUND
	end

	if c.isMod(chan, con) ~= true then
		return const.FERR_NOT_OP
	end

	if c.isBanned(chan, string.lower(args.character)) ~= true then
		return const.FERR_NOT_CHANNEL_BANNED
	end

	if c.getType(chan) == "public" then
		s.logAction(con, "CUB", args)
	end

	c.unban(chan, string.lower(args.character))
	u.send(con, "SYS", {channel=args.channel, message=args.character.." has been removed from the channel ban list."})
	return const.FERR_OK
end

-- Removes a global moderator.
-- Syntax: DOP <character>
event.DOP =
function (con, args)
	if args.character == nil then
		return const.FERR_BAD_SYNTAX
	end

	if u.isAdmin(con) ~= true then
		return const.FERR_NOT_ADMIN
	end

	if s.isOp(args.character) ~= true then
		return const.FERR_NOT_AN_OP
	end

	s.logAction(con, "DOP", args)
	s.removeOp(args.character)
	local cfound, staffchan = c.getChannel("adh-staffroomforstaffppl")
	if cfound == true then
		c.removeInvite(staffchan, string.lower(args.character))
	end
	local found, char = u.getConnection(string.lower(args.character))
	if found == true then
		u.setGlobMod(char, false)
	end
	s.broadcast("DOP", {character=args.character})
	u.send(con, "SYS", {message=args.character.." has been removed as a global moderator."})
	return const.FERR_OK
end

-- Finds other characters by kinks or other properties.
-- Syntax: FKS <kinks>
event.FKS =
function (con, args)
	return const.FERR_NOT_IMPLEMENTED
end

-- Gets the friend list for a connection.
-- Syntax: FRL
event.FRL =
function (con, args)
	u.send(con, "FRL", {array_characters=u.getFriendList(con)})
	return const.FERR_OK
end

-- Identification command.
-- Syntax: IDN <Complex stuff goes here.>
event.IDN =
function (con, args)
	error("This function is native and should not be called.")
end

-- Processes ignore lists.
-- Syntax: IGN <It's complicated>
event.IGN =
function (con, args)
	if args.action == nil then
		return const.FERR_BAD_SYNTAX
	end

	if args.action == "list" then
		u.send(con, "IGN", {action="list", array_characters=u.getIgnoreList(con)})
	elseif args.action == "add" and args.character ~= nil then
		if #u.getIgnoreList(con) < 100 then
			u.addIgnore(con, string.lower(args.character))
			propagateIgnoreList(con, "add", args.character)
		else
			u.sendError(con, 64, "Your ignore list may not exceed 100 people.")
			return const.FERR_OK
		end
	elseif args.action == "delete" and args.character == "*" then
		local ignores = u.getIgnoreList(con)
		for i, v in ipairs(ignores) do
			u.removeIgnore(con, string.lower(v))
		end
		local account_cons = u.getByAccount(con)
		for i, v in ipairs(account_cons) do
			u.setIgnores(v, ignores)
			u.send(con, "IGN", {action="init", array_characters=u.getIgnoreList(con)})
		end
	elseif args.action == "delete" and args.character ~= nil then
		u.removeIgnore(con, string.lower(args.character))
		propagateIgnoreList(con, "delete", args.character)
	elseif args.action == "notify" and args.character ~= nil then
		if u.checkUpdateTimer(con, "ign", const.IGN_FLOOD) ~= true then
			local found, char = u.getConnection(string.lower(args.character))
			if found == true then
				u.sendError(char, 20, u.getName(con).." does not wish to receive messages from you.")
			end
		end
	else
		return const.FERR_BAD_SYNTAX
	end
	return const.FERR_OK
end

-- Requests to join a channel.
-- Syntax: JCH <channel>
event.JCH =
function (con, args)
	if args.channel == nil then
		return const.FERR_BAD_SYNTAX
	end

	local found, chan = c.getChannel(string.lower(args.channel))
	if found ~= true then
		return const.FERR_CHANNEL_NOT_FOUND
	end

	if c.inChannel(chan, con) == true then
		return const.FERR_ALREADY_IN_CHANNEL
	end

	if c.isOwner(chan, con) ~= true and c.isBanned(chan, con) ~= false then
		local banned, ban = c.getBan(chan, con)
		if banned == true and ban.timeout ~= 0 then
			u.sendError(con, const.FERR_CHANNEL_BANNED, string.format("You are banned from the channel for another %.2f minute(s).", ((ban.timeout-s.getTime())/60) ))
			return const.FERR_OK
		end
		return const.FERR_CHANNEL_BANNED
	end

	local chantype = c.getType(chan)
	if chantype == "private" and (c.isInvited(chan, con) ~= true and c.isOwner(chan, con) ~= true) then
		return const.FERR_NOT_INVITED
	end

	joinChannel(chan, con)
	return const.FERR_OK
end

-- Destroys a channel.
-- Syntax: KIC <channel>
event.KIC =
function (con, args)
	if args.channel == nil then
		return const.FERR_BAD_SYNTAX
	end

	local found, chan = c.getChannel(string.lower(args.channel))
	if found ~= true then
		return const.FERR_CHANNEL_NOT_FOUND
	end

	if c.isOwner(chan, con) then
		s.logAction(con, "KIC", args)
		c.sendAll(chan, "BRO", {message="You are being removed from the channel ".. c.getName(chan) ..". The channel is being destroyed."})
		c.destroyChannel(string.lower(args.channel))
		u.send(con, "SYS", {message=args.channel .. " has been removed as a public channel."})
	else
		return const.FERR_NOT_OP
	end

	return const.FERR_OK
end

-- Kicks a user from the server.
-- Syntax:: KIK <character>
event.KIK =
function (con, args)
	if args.character == nil then
		return const.FERR_BAD_SYNTAX
	end

	if (u.isAdmin(con) ~= true) and (u.isGlobMod(con) ~= true) then
		return const.FERR_NOT_OP
	end

	local found, char = u.getConnection(string.lower(args.character))
	if found ~= true then
		return const.FERR_USER_NOT_FOUND
	end

	s.logAction(con, "KIK", args)
	u.sendError(char, const.FERR_KICKED)
	u.send(con, "SYS", {message=u.getName(char).." has been kicked from chat."})
	u.close(char)
	return const.FERR_OK
end

-- Sends a characters custom kinks.
-- Syntax: KIN <character>
event.KIN =
function (con, args)
	if args.character == nil then
		return const.FERR_BAD_SYNTAX
	end

	if u.checkUpdateTimer(con, "kin", const.KIN_FLOOD) == true then
		return const.FERR_THROTTLE_KINKS
	end

	local found, char = u.getConnection(string.lower(args.character))
	if found ~= true then
		return const.FERR_USER_NOT_FOUND
	end

	local charname = u.getName(char)

	u.send(con, "KID", {type="start", character=charname, message="Custom kinks of "..charname})

	local customkinks = u.getCustomKinks(char)
	for k,v in pairs(customkinks) do
		u.send(con, "KID", {type="custom", character=charname, key=k, value=v})
	end

	u.send(con, "KID", {type="end", character=charname, message="End of custom kinks."})

	return const.FERR_OK
end

-- Leaves a channel.
-- Syntax: LCH <channel>
event.LCH =
function (con, args)
	if args.channel == nil then
		return const.FERR_BAD_SYNTAX
	end

	local charname = u.getName(con)

	local found, chan = c.getChannel(string.lower(args.channel))
	if found ~= true then
		u.send(con, "LCH", {channel=args.channel, character=charname})
		return const.FERR_CHANNEL_NOT_FOUND
	end

	if c.inChannel(chan, con) ~= true then
		u.send(con, "LCH", {channel=args.channel, character=charname})
		return const.FERR_OK
	end

	partChannel(chan, con)
	return const.FERR_OK
end

-- Sends an RP ad to a channel.
-- Syntax: LRP <channel> <message>
event.LRP =
function (con, args)
	if args.channel == nil or args.message == nil then
		return const.FERR_BAD_SYNTAX
	end

	if u.checkUpdateTimer(con, "msg", const.MSG_FLOOD) == true then
		return const.FERR_THROTTLE_MESSAGE
	end

	local found, chan = c.getChannel(string.lower(args.channel))
	if found ~= true then
		return const.FERR_CHANNEL_NOT_FOUND
	end

	if c.getMode(chan) == "chat" then
		return const.FERR_CHAT_ONLY
	end

	if c.checkUpdateTimer(chan, con, const.LRP_FLOOD) == true then
		return const.FERR_THROTTLE_AD
	end

	if #args.message > const.LRP_MAX then
		return const.FERR_MESSAGE_TOO_LONG
	end

	if c.inChannel(chan, con) ~= true then
		return const.FERR_NOT_IN_CHANNEL
	end

	if u.getMiscData(con, "hellban") ~= nil then
		return const.FERR_OK
	end

	c.sendChannel(chan, con, "LRP", {channel=c.getName(chan), character=u.getName(con), message=s.escapeHTML(args.message)})
	return const.FERR_OK
end

-- Sends a chat message to a channel.
-- Syntax: MSG <channel> <message>
event.MSG =
function (con, args)
	if args.channel == nil or args.message == nil then
		return const.FERR_BAD_SYNTAX
	end

	if u.checkUpdateTimer(con, "msg", const.MSG_FLOOD) == true then
		return const.FERR_THROTTLE_MESSAGE
	end

	local found, chan = c.getChannel(string.lower(args.channel))
	if found ~= true then
		return const.FERR_CHANNEL_NOT_FOUND
	end

	if c.getMode(chan) == "ads" then
		return const.FERR_ADS_ONLY
	end

	local chantype = c.getType(chan)
	if (chantype == "private" or chantype == "pubprivate") and #args.message > const.PRI_MAX then
		return const.FERR_MESSAGE_TOO_LONG
	elseif #args.message > const.MSG_MAX then
		return const.FERR_MESSAGE_TOO_LONG
	end

	if c.inChannel(chan, con) ~= true then
		return const.FERR_NOT_IN_CHANNEL
	end

	if u.getMiscData(con, "hellban") ~= nil then
		return const.FERR_OK
	end

	c.sendChannel(chan, con, "MSG", {channel=c.getName(chan), character=u.getName(con), message=s.escapeHTML(args.message)})
	return const.FERR_OK
end

-- Gets the list of open private channels.
-- Syntax: ORS
event.ORS =
function (con, args)
	c.sendORS(con)
	return const.FERR_OK
end

-- Sends a private message to another user.
-- Syntax: PRI <recipient> <message>
event.PRI =
function (con, args)
	if args.recipient == nil or args.message == nil then
		return const.FERR_BAD_SYNTAX
	end

	if u.checkUpdateTimer(con, "msg", const.MSG_FLOOD) == true then
		return const.FERR_THROTTLE_MESSAGE
	end

	if #args.message > const.PRI_MAX then
		return const.FERR_MESSAGE_TOO_LONG
	end

	local found, target = u.getConnection(string.lower(args.recipient))
	if found ~= true then
		return const.FERR_USER_NOT_FOUND
	end

	if u.getMiscData(con, "hellban") ~= nil then
		return const.FERR_OK
	end

	u.send(target, "PRI", {character=u.getName(con), message=s.escapeHTML(args.message), recipient=args.recipient})
	return const.FERR_OK
end

-- Sends a characters mini profile.
-- Syntax:: PRO <character>
event.PRO =
function (con, args)
	if args.character == nil then
		return const.FERR_BAD_SYNTAX
	end

	if u.checkUpdateTimer(con, "pro", const.PRO_FLOOD) == true then
		return const.FERR_THROTTLE_PROFILE
	end

	local found, char = u.getConnection(string.lower(args.character))
	if found ~= true then
		return const.FERR_USER_NOT_FOUND
	end

	local charname = u.getName(char)

	u.send(con, "PRD", {type="start", character=charname, message="Profile of "..charname})

	local infotags = u.getInfoTags(char)
	for k,v in pairs(infotags) do
		u.send(con, "PRD", {type="info", character=charname, key=k, value=v})
	end

	u.send(con, "PRD", {type="end", character=charname, message="End of profile."})

	return const.FERR_OK
end

-- Reloads and optionally saves ops/bans
-- Syntax: RLD <?save>
event.RLD =
function (con, args)
	if u.isAdmin(con) ~= true then
		return const.FERR_NOT_ADMIN
	end

	s.logAction(con, "RLD", args)
	if args.save ~= nil then
		s.reload(true)
		u.send(con, "SYS", {message="Saved ops, bans, and channels to disk."})
	else
		s.reload(false)
	end
	u.send(con, "SYS", {message="Reloaded config variables, ops, and bans from disk."})
	return const.FERR_OK
end

-- Rolls dice or spins the bottle in a channel or private message.
-- Syntax: RLL <channel> <dice>
event.RLL =
function (con, args)
	if args.dice == nil then
		return const.FERR_BAD_SYNTAX
	end
	
	local haschannel = args.channel ~= nil
	local hasrecipient = args.recipient ~= nil
	if not haschannel and not hasrecipient then
		return const.FERR_BAD_SYNTAX
	end
	
	if u.checkUpdateTimer(con, "msg", const.MSG_FLOOD) == true then
		return const.FERR_THROTTLE_MESSAGE
	end

	local lowerchanname = nil
	local channelfound, chan, targetfound, target = nil
	if haschannel then
		lowerchanname = string.lower(args.channel)
		channelfound, chan = c.getChannel(lowerchanname)
		if channelfound ~= true then
			return const.FERR_CHANNEL_NOT_FOUND
		end

		lowerchanname = string.lower(c.getName(chan))
		if lowerchanname == "frontpage" then
			u.sendError(con, -10, "You may not roll dice or spin the bottle in Frontpage.")
			return const.FERR_OK
		end
	
		if c.getMode(chan) == "ads" then
			return const.FERR_ADS_ONLY
		end
	else
		targetfound, target = u.getConnection(string.lower(args.recipient))
		if targetfound ~= true then
			return const.FERR_USER_NOT_FOUND
		end
	end

	-- Hellban comes last	
	if u.getMiscData(con, "hellban") ~= nil then
		return const.FERR_OK
	end
	
	if args.dice == "bottle" then
		-- gets the right bottle people!
		local bottlers = haschannel and c.getBottleList(chan, con) or { u.getName(con), u.getName(target) }
		local bottle = bottle_spin(con, bottlers, args)
		if bottle == nil then
			u.send(con, "SYS", {message="Couldn't locate anyone who is available to have the bottle land on them."})
			return FERR_BAD_SYNTAX
		else
			if haschannel then
				bottle.channel = args.channel
				c.sendAll(chan, "RLL", bottle)
			else
				bottle.recipient = args.recipient
				u.send(target, "RLL", bottle)
				u.send(con, "RLL", bottle)
			end
		end
		return const.FERR_OK
	end	

	-- Assume dice roll if not bottle
	local roll = dice_roll(con, args.dice)
	if roll == nil then
		return const.FERR_BAD_ROLL_FORMAT
	end
	if haschannel then
		roll.channel = c.getName(chan)
		c.sendAll(chan, "RLL", roll)
	else
		roll.recipient = u.getName(target)
		u.send(target, "RLL", roll)
		u.send(con, "RLL", roll)
	end
	
	return const.FERR_OK
end

-- Set the message mode for a channel.
-- Syntax: RMO <channel> <mode(chat/ads/both)>
event.RMO =
function (con, args)
	if args.channel == nil or args.mode == nil then
		return const.FERR_BAD_SYNTAX
	end

	local newmode = string.lower(args.mode)
	if (newmode ~= "both") and (newmode ~= "ads") and (newmode ~= "chat") then
		return const.FERR_BAD_SYNTAX
	end

	local found, chan = c.getChannel(string.lower(args.channel))
	if found ~= true then
		return const.FERR_CHANNEL_NOT_FOUND
	end

	if c.isOwner(chan, con) ~= true then
		return const.FERR_NOT_OP
	end

	c.setMode(chan, newmode)
	c.sendAll(chan, "RMO", {channel=c.getName(chan), mode=newmode})
	return const.FERR_OK
end

-- Sets a private channels public status.
-- Syntax: RST <status(public/closed)>
event.RST =
function (con, args)
	if args.channel == nil or args.status == nil then
		return const.FERR_BAD_SYNTAX
	end

	local newpub = string.lower(args.status)
	if (newpub ~= "public") and (newpub ~= "private") then
		return const.FERR_BAD_SYNTAX
	end

	local found, chan = c.getChannel(string.lower(args.channel))
	if found ~= true then
		return const.FERR_CHANNEL_NOT_FOUND
	end

	if c.isOwner(chan, con) ~= true then
		return const.FERR_NOT_OP
	end

	local chantype = c.getType(chan)
	if (chantype ~= "private") and (chantype ~= "pubprivate") then
		return const.FERR_BAD_SYNTAX
	end

	local pubstatus = false
	local statusmsg = "closed."
	if newpub == "public" then
		pubstatus = true
		statusmsg = "public."
	end

	c.setPublic(chan, pubstatus)
	u.send(con, "SYS", {channel=string.lower(args.channel), message="This channel is now [b]"..statusmsg.."[/b]"})
	return const.FERR_OK
end

-- Rewards a character with a crown status.
-- Syntax:: RWD <character>
event.RWD =
function (con, args)
	if args.character == nil then
		return const.FERR_BAD_SYNTAX
	end

	if u.isAdmin(con) ~= true then
		return const.FERR_NOT_ADMIN
	end

	local found, target = u.getConnection(string.lower(args.character))
	if found ~= true then
		return const.FERR_USER_NOT_FOUND
	end

	s.logAction(con, "RWD", args)
	local oldstatus, statusmesg = u.getStatus(target)
	u.setStatus(con, "crown", statusmesg)

	s.broadcast("STA", {character=u.getName(target), status="crown", statusmsg=statusmesg})
	return const.FERR_OK
end

-- Processes a staff call request.
-- Syntax: SFC <action> (<character> <report>/<callid> <moderator>)
event.SFC =
function (con, args)
	if args.action == nil then
		return const.FERR_BAD_SYNTAX
	end

	if args.action == "report" then
		if args.report == nil then
			return const.FERR_BAD_SYNTAX
		end
		if u.isGlobMod(con) ~= true and u.isAdmin(con) ~= true then
			if u.checkUpdateTimer(con, "sfc", const.SFC_FLOOD) == true then
				return const.FERR_THROTTLE_STAFF_CALL
			end
		end
		local lname = u.getName(con)
		local ltimestamp = s.getTime()
		local lcallid = ltimestamp..":"..lname
		local llogid = tonumber(args.logid)
		local lreport = s.escapeHTML(args.report)
		if llogid == 0 or llogid == nil then
			llogid = -1
		end
		s.addStaffCall(lcallid, lname, lreport, llogid)
		local lsfc = {callid=lcallid, action="report", report=lreport, timestamp=ltimestamp, character=lname}
		if llogid ~= -1 then
			lsfc.logid = llogid
		end
		s.broadcastOps("SFC", lsfc)
		u.send(con, "SYS", {message="The moderators have been alerted."})
	elseif args.action == "confirm" then
		if u.isGlobMod(con) ~= true and u.isAdmin(con) ~= true then
			return const.FERR_NOT_OP
		end
		local call = s.getStaffCall(args.callid)
		if call == false then
			return const.FERR_OK
		end
		s.removeStaffCall(args.callid)
		s.broadcastOps("SFC", {action="confirm", moderator=u.getName(con), character=call.character, timestamp=call.timestamp})
	else
		return const.FERR_BAD_SYNTAX
	end
	return const.FERR_OK
end

-- Sets a connections status, and status message.
-- Syntax STA <status> ?<statusmessage>
event.STA =
function (con, args)
	if u.checkUpdateTimer(con, "sta", const.STA_FLOOD) == true then
		return const.FERR_THROTTLE_MESSAGE
	end

	if args.status == nil then
		return const.FERR_BAD_SYNTAX
	end

	local newstatus = string.lower(args.status)
	if const.status[newstatus] == nil then
		newstatus = "online"
	end

	local statusmessage = args.statusmsg
	if statusmessage == nil then
		statusmessage = ""
	end

	if #statusmessage > 255 then
		statusmessage = string.sub(statusmessage, 0, 255)
	end
	statusmessage = s.escapeHTML(statusmessage)

	u.setStatus(con, newstatus, statusmessage)
	s.broadcast("STA", {character=u.getName(con), status=newstatus, statusmsg=statusmessage})
	return const.FERR_OK
end

-- Sets a timeout on an account.
-- Syntax: TMO <character> <time> <reason>
event.TMO =
function (con, args)
	if args.character == nil or args.time == nil or args.reason == nil then
		return const.FERR_BAD_SYNTAX
	end

	local length = tonumber(args.time)
	if length == nil or length < 1 then
		return const.FERR_BAD_TIMEOUT_FORMAT
	end

	if (u.isAdmin(con) ~= true) and (u.isGlobMod(con) ~= true) then
		return const.FERR_NOT_OP
	end

	local found, char = u.getConnection(string.lower(args.character))
	if found ~= true then
		return const.FERR_USER_NOT_FOUND
	end

	if (u.isAdmin(char) == true) or (u.isGlobMod(char) == true) then
		return const.FERR_DENIED_ON_OP
	end

	local reason = s.escapeHTML(args.reason)

	s.logAction(con, "TMO", args)
	s.addTimeout(char, length*60)
	u.send(con, "SYS", {message=u.getName(char).." has been given a "..length.." minute time out for: "..reason})
	local account_cons = u.getByAccount(char)
	for i, v in ipairs(account_cons) do
		u.sendError(v, const.FERR_TIMED_OUT, "You have been given a time out by "..u.getName(con).." for "..length.." minute(s). The reason given was: "..reason)
		u.close(v)
	end
	return const.FERR_OK
end

-- Sends a typing status message to another character.
-- Syntax: TPN <target> <status>
event.TPN =
function (con, args)
	if args.character == nil or args.status == nil then
		return const.FERR_BAD_SYNTAX
	end

	local found, target = u.getConnection(string.lower(args.character))
	if found ~= true then
		return const.FERR_OK
	end
	u.send(target, "TPN", {character=u.getName(con), status=args.status})
	return const.FERR_OK
end

-- Removes a global server ban.
-- Syntax: UNB <character>
event.UNB =
function (con, args)
	if args.character == nil then
		return const.FERR_BAD_SYNTAX
	end

	if (u.isAdmin(con) ~= true) and (u.isGlobMod(con) ~= true) then
		return const.FERR_NOT_OP
	end

	s.logAction(con, "UNB", args)
	s.removeTimeout(string.lower(args.character))
	if s.removeBan(string.lower(args.character)) == true then
		u.send(con, "SYS", {message="Removed ban successfully."})
	else
		u.send(con, "SYS", {message="Could not find the ban in question."})
	end
	return const.FERR_OK
end

-- Returns various stats about the server.
-- Syntax: UPT
event.UPT =
function (con, args)
	local lusers, lmax_users, lchannels, lstart_time, ltime, laccepted, lstart_string = s.getStats()
	u.send(con, "UPT", {users=lusers, maxusers=lmax_users, channels=lchannels, starttime=lstart_time, time=ltime, accepted=laccepted, startstring=lstart_string})
	return const.FERR_OK
end

-- Debug console command.
event.ZZZ =
function (con, args)
	error("This function is native and should not be called.")
end

-- -----------------------------------------------------------------------------------------------------------------------------------

event.ident_callback =
function (con, args)
	if args.error ~= "" then
		print("Error returned from login server was: "..args.error)
		return const.FERR_IDENT_FAILED
	end

	if u.getIPCount(con) >= const.IP_MAX then
		return const.FERR_TOO_MANY_FROM_IP
	end

	local name = args.char.name
	local lname = string.lower(name)

	local oldconfound, oldcon = u.getConnection(lname)
	if oldconfound ~= false then
		u.sendError(oldcon, const.FERR_LOGGED_IN_AGAIN)
		event.pre_disconnect(oldcon)
		u.closef(oldcon)
		oldcon = nil
	end

	if tonumber(args.account.banned) ~= 0 or tonumber(args.account.timeout) >  s.getTime() then
		return const.FERR_BANNED_FROM_SERVER
	end

	if u.setIdent(con, name, lname) ~= true then
		return const.FERR_IDENT_FAILED
	end

	if tonumber(args.account.account_id) == 0 then
		print("Failed a login because the account id was zero")
		return const.FERR_IDENT_FAILED
	end
	u.setAccountID(con, args.account.account_id)

	local isadmin = false
	if args.account.admin == "1" or lname == "kira" or lname == "vera" or lname == "melvin" then
		u.setAdmin(con, true)
		isadmin = true
	end

	local isop = false
	if s.isOp(name) == true then
		u.setGlobMod(con, true)
		isop = true
	end

	if (s.isBanned(con) ~= false) and (isop ~= true) and (isadmin ~= true) then
		return const.FERR_BANNED_FROM_SERVER
	end

	local timedout, left = s.isTimedOut(con)
	if (timedout ~= false) and (isop ~= true) and (isadmin ~= true) then
		u.sendError(con, const.FERR_BANNED_FROM_SERVER, string.format("You have been timed out from chat. There are %.2f minute(s) left on your time out.", (left/60)))
		u.close(con)
		return const.FERR_OK
	end

	if args.select ~= nil then
		if args.select.Gender ~= nil then
			u.setGender(con, args.select.Gender)
		else
			args.select.Gender="None"
			u.setGender(con, "None")
		end
		if args.select.Orientation == nil then
			args.select.Orientation = "None"
		end
		if args.select.Position == nil then
			args.select.Position = "None"
		end
		if args.select["Language preference"] == nil then
			args.select["Language preference"] = "None"
		end
		if args.select["Furry preference"] == nil then
			args.select["Furry preference"] = "None"
		end
		if args.select["Dom/Sub Role"] == nil then
			args.select["Dom/Sub Role"] = "None"
		end
		u.setInfoTags(con, args.select)
	else
		local newselect = {}
		newselect.Gender = "None"
		newselect.Orientation = "None"
		newselect["Language preference"] = "None"
		newselect["Furry preference"] = "None"
		newselect["Dom/Sub Role"] = "None"
		newselect.Position = "None"
		u.setInfoTags(con, newselect)
		u.setGender(con, "None")
	end

	if args.array_kinks ~= nil then
		u.setKinks(con, args.array_kinks)
	else
		local newkinks = {-1}
		u.setKinks(con, newkinks)
	end

	if args.info ~= nil then
		u.setInfoTags(con, args.info)
	end

	if args.custom ~= nil then
		u.setCustomKinks(con, args.custom)
	end

	if args.array_friends ~= nil then
		u.setFriends(con, args.array_friends)
	end

	if args.array_ignores ~= nil then
		u.setIgnores(con, args.array_ignores)
	end

	local isstaff = false
	if args.bits ~= nil then
		if args.bits.hellban == 1 then
			u.setMiscData(con, "hellban", "yes")
		end
		if (args.bits.coder == 1) or (args.bits.admin == 1) or (args.bits.moderator == 1) or (args.bits.helpdesk == 1)
			or (args.bits.chanop == 1) or (args.bits.chatop == 1) then
			isstaff = true
		end
	end

	u.send(con, "IDN", {character=name})
	u.send(con, "VAR", {variable="chat_max", value=const.MSG_MAX})
	u.send(con, "VAR", {variable="priv_max", value=const.PRI_MAX})
	u.send(con, "VAR", {variable="lfrp_max", value=const.LRP_MAX})
	u.send(con, "VAR", {variable="lfrp_flood", value=const.LRP_FLOOD})
	u.send(con, "VAR", {variable="msg_flood", value=const.MSG_FLOOD})
	u.send(con, "VAR", {variable="permissions", value=args.permissions})

	u.send(con, "HLO", {message="Welcome. Running F-Chat (".. const.VERSION .."). Enjoy your stay."})
	u.send(con, "CON", {count=s.getUserCount()})

	if args.array_friends ~= nil then
		u.send(con, "FRL", {array_characters=args.array_friends})
	end
	if args.array_ignores ~= nil then
		u.send(con, "IGN", {action="init", array_characters=args.array_ignores})
	end
	u.send(con, "ADL", {array_ops=s.getOpList()})

	s.sendUserList(con, "LIS", 100)

	s.broadcast("NLN", {identity=name, status="online", gender=u.getGender(con)})

	if isop then
		local found, chan = c.getChannel("adh-staffroomforstaffppl")
		if found == true then
			c.invite(chan, con)
			joinChannel(chan, con)
		end
		s.sendStaffCalls(con)
	end

	if isstaff or isop or (s.isChanOp(con) == true) then
		local found, chan = c.getChannel("adh-uberawesomestaffroom")
		if found == true then
			c.invite(chan, con)
			joinChannel(chan, con)
		end
	end

	return const.FERR_OK
end

event.pre_disconnect =
function (con)
	local name = u.getName(con)
	local channels = u.getChannels(con)
	for i,v in ipairs(channels) do
		partChannel(v, con, true)
	end
	s.broadcast("FLN", {character=name})
end

rtb.RTB =
function (args)
	local account_cons = u.getByAccountID(args.a)
	for i, v in ipairs(account_cons) do
		u.sendRaw(v, "RTB " .. args.c)
	end
	return const.FERR_OK
end

rtb.HLB =
function (args)
	local account_cons = u.getByAccountID(args.a)
	for i, v in ipairs(account_cons) do
		u.setMiscData(v, "hellban", "yes")
	end
	s.broadcastOps("SYS", {message="Hellban against account id: "..args['a'].." was successful."})
	return const.FERR_OK
end

rtb.KIK =
function (args)
	local account_cons = u.getByAccountID(args.a)
	for i, v in ipairs(account_cons) do
		u.sendError(v, const.FERR_KICKED)
		u.close(v)
	end
	s.broadcastOps("SYS", {message="Chat kick applied against account id: "..args['a'].." was successful."})
	return const.FERR_OK
end

--[[ While this function is called before most other Lua functions,	it is discouraged that you store anything in Lua
		that is not entirely disposable.
--]]
function chat_init()
	const.MSG_MAX = s.getConfigDouble("msg_max")
	const.PRI_MAX = s.getConfigDouble("priv_max")
	const.LRP_MAX = s.getConfigDouble("lfrp_max")
	const.LRP_FLOOD = s.getConfigDouble("lfrp_flood")
	const.MSG_FLOOD = s.getConfigDouble("msg_flood")
	const.KIN_FLOOD = s.getConfigDouble("kinks_flood")
	const.PRO_FLOOD = s.getConfigDouble("profile_flood")
	const.FKS_FLOOD = s.getConfigDouble("find_flood")
	const.SFC_FLOOD = s.getConfigDouble("staffcall_flood")
	const.IGN_FLOOD = s.getConfigDouble("ignore_flood")
	const.STA_FLOOD = 5
	const.VERSION = s.getConfigString("version")
	const.IP_MAX = s.getConfigDouble("max_per_ip")
	const.MAX_TITLE_LEN = 64.4999
	if c.getChannel("adh-staffroomforstaffppl") ~= true then
		local staffchan = c.createSpecialPrivateChannel("ADH-STAFFROOMFORSTAFFPPL", "Moderation Staff Room")
		c.setDescription(staffchan, "This room is CHAT STAFF ONLY. You can /invite regular users if necessary for staff discussion. [b]Everything other people need to know about goes on the [url=http://www.f-list.net/group.php?group=staff%20discussion]staff board[/url].[/b]")
	end
	if c.getChannel("adh-uberawesomestaffroom") ~= true then
		local chanopchan = c.createSpecialPrivateChannel("ADH-UBERAWESOMESTAFFROOM", "Staff Room")
		c.setDescription(chanopchan, "This room is for website and chat staff.")
	end
	-- The numbers have no meaning.
	const.gender = {}
	const.gender["none"] = 1
	const.gender["male"] = 2
	const.gender["female"] = 3
	const.gender["shemale"] = 4
	const.gender["transgender"] = 5
	const.gender["herm"] = 6
	const.gender["male-herm"] = 7
	const.gender["cunt-boy"] = 8

	const.status = {}
	const.status["online"] = 1
	const.status["looking"] = 2
	const.status["busy"] = 3
	const.status["dnd"] = 4
	const.status["idle"] = 5
	const.status["away"] = 6
	--const.status["crown"] = 5
end

function string:split(sep)
	local sep, fields = sep or ":", {}
	local pattern = string.format("([^%s]+)", sep)
	self:gsub(pattern, function(c) fields[#fields+1] = c end)
	return fields
end
