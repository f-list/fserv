--[[
 * Copyright (c) 2011-2018, "Kira"
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
httpcb = {}

httpcb.handle_report = function(con, status, resp, extras)
    if status ~= 200 then
        u.send(con, "SYS", { message = "Failed to mark report as handled." })
        return const.FERR_OK
    end
    u.send(con, "SYS", { message = "Ticket marked as handled by you." })
    return const.FERR_OK
end

function broadcastChannelOps(event, message, channel)
    local chanops = c.getModList(channel)
    for i, v in ipairs(chanops) do
        local confound, opcon = u.getConnection(string.lower(v))
        if confound == true and u.hasAnyRole(opcon, { "admin", "global", "super-cop" }) ~= true then
            u.send(opcon, event, message)
        end
    end
end

function hasShortener(input)
    local shorteners = {}
    for _, v in ipairs(shorteners) do
        if string.find(input, v, 1, true) then
            return true
        end
    end
    return false
end

-- Parts a connection from a channel.
function partChannel(chan, con, is_disconnect)
    local cname = c.getName(chan)
    local lcname = string.lower(cname)
    local conname = u.getName(con)
    if is_disconnect ~= true then
        c.sendAll(chan, "LCH", { channel = cname, character = conname })
    end
    c.part(chan, con)
    s.logMessage("channel_leave", con, chan, nil, nil)
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
    s.logMessage("channel_join", con, chan, nil, nil)
    if chantype == "public" then
        c.sendAll(chan, "JCH", { channel = channame, character = { identity = u.getName(con) }, title = channame })
    else
        c.sendAll(chan, "JCH", { channel = channame, character = { identity = u.getName(con) }, title = c.getTitle(chan) })
    end
    u.send(con, "COL", { channel = channame, array_oplist = c.getModList(chan) })
    c.sendICH(chan, con)
    u.send(con, "CDS", { channel = channame, description = c.getDescription(chan) })
end

function propagateIgnoreList(con, laction, lcharacter)
    local ignores = u.getIgnoreList(con)
    local account_cons = u.getByAccount(con)
    for i, v in ipairs(account_cons) do
        u.setIgnores(v, ignores)
        u.send(v, "IGN", { action = laction, character = lcharacter })
    end
end

-- Computes dice roll from given arguments
-- Syntax: dice_roll <connection> <args>
function dice_roll(con, diceargs)
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
            if finite(num) ~= true then
                u.close(con)
                return nil
            end
            if num == nil or num > 10000 or num < -10000 then
                return nil
            end
            table.insert(results, num)
        else
            local rolls = tonumber(roll[1])
            local sides = tonumber(roll[2])
            if finite(rolls) ~= true or finite(sides) ~= true then
                u.close(con)
                return nil
            end
            local mod = 0
            if rolls == nil or sides == nil or rolls > 9 or sides > 500 or sides < 2 then
                return nil
            elseif rolls < 0 then
                rolls = math.abs(rolls)
                mod = -1
            else
                mod = 1
            end
            local sum = 0
            for v = 1, rolls, 1 do
                sum = sum + math.random(sides)
            end
            table.insert(results, (mod * sum))
        end
    end
    local total = 0
    for i, v in ipairs(results) do
        total = total + v
    end
    local result = string.format("[user]%s[/user] rolls %s: ", u.getName(con), odice)
    local concatresults = ""
    if #results == 1 then
        concatresults = "[b]" .. total .. "[/b]"
    else
        for i, v in ipairs(results) do
            if v < 0 then
                concatresults = concatresults .. " - " .. math.abs(v)
            else
                concatresults = concatresults .. " + " .. v
            end
        end
        if results[1] >= 0 then
            concatresults = string.sub(concatresults, 4)
        end
        concatresults = concatresults .. " = [b]" .. total .. "[/b]"
    end

    return { type = "dice", array_rolls = steps, array_results = results, endresult = total, character = u.getName(con), message = result .. concatresults }
end

-- Spins the bottle for a channel / private message
-- Syntax: bottle_spin <connection> <bottlers> <args>
function bottle_spin(con, bottlers)
    local conname = u.getName(con)
    if #bottlers ~= 0 then
        local picked = bottlers[math.random(#bottlers)]
        return { character = conname, type = "bottle", target = picked, message = string.format("[user]%s[/user] spins the bottle: [user]%s[/user]", conname, picked) }
    end
    return nil
end

function public_staff_override(con, chan)
    local chantype = c.getType(chan)
    local staff_override = u.hasAnyRole(con, { "admin", "global" })
    staff_override = staff_override or (chantype == "public" and u.hasRole(con, "super-cop"))
    return staff_override
end

-- Bans a person by their account.
-- Syntax: ACB <character>
event.ACB =
function(con, args)
    if args.character == nil then
        return const.FERR_BAD_SYNTAX
    end

    -- V: 2018-07-25
    if u.hasAnyRole(con, { "admin", "global" }) ~= true then
        return const.FERR_NOT_OP
    end

    local found, char = u.getConnection(string.lower(args.character))
    if found ~= true then
        return const.FERR_USER_NOT_FOUND
    end

    -- V: 2018-07-25
    if u.hasAnyRole(con, { "admin", "global" }) == true then
        return const.FERR_DENIED_ON_OP
    end

    s.logAction(con, "ACB", args)
    s.logMessage("global_ban", con, nil, char, nil)
    s.addBan(char)
    u.send(con, "SYS", { message = u.getName(char) .. " is now account banned." })
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
function(con, args)
    if args.character == nil then
        return const.FERR_BAD_SYNTAX
    end

    -- V: 2018-07-25
    if u.hasRole(con, "admin") ~= true then
        return const.FERR_NOT_ADMIN
    end

    local lname = string.lower(args.character)
    local found, char = u.getConnection(lname)
    if found ~= true then
        return const.FERR_USER_NOT_FOUND
    end
    local name = u.getName(char)

    -- V: 2018-07-25
    if s.isOp(name) == true then
        return const.FERR_ALREADY_OP
    end

    s.logAction(con, "AOP", args)
    s.logMessage("global_op_add", con, nil, char, nil)
    s.addOp(name)
    local found, char = u.getConnection(string.lower(args.character))
    if found == true then
        u.setGlobMod(char, true)
    end
    s.broadcast("AOP", { character = name })
    u.send(con, "SYS", { message = name .. " has been added as a global moderator." })
    return const.FERR_OK
end

-- Adds an alt watch on a character.
-- Syntax: AWC <character>
event.AWC =
function(con, args)
    return const.FERR_NOT_IMPLEMENTED
end

-- Sends a broadcast message to the entire server.
-- Syntax: BRO <message>
event.BRO =
function(con, args)
    if args.message == nil then
        return const.FERR_BAD_SYNTAX
    end

    -- V: 2018-07-25
    if u.hasRole(con, "admin") ~= true then
        return const.FERR_NOT_ADMIN
    end

    s.logAction(con, "BRO", args)
    s.logMessage("broadcast", con, nil, nil, args.message)
    local mesg = s.escapeHTML(args.message)
    local lname = u.getName(con)
    s.broadcast("BRO", { character = lname, message = "[b]Broadcast from " .. lname .. ":[/b] " .. mesg })
    return const.FERR_OK
end

-- Lists the bans for a channel.
-- Syntax: CBL <channel>
event.CBL =
function(con, args)
    if args.channel == nil then
        return const.FERR_BAD_SYNTAX
    end

    local found, chan = c.getChannel(string.lower(args.channel))
    if found ~= true then
        return const.FERR_CHANNEL_NOT_FOUND
    end

    -- V: 2018-07-25
    --if c.isMod(chan, con) ~= true then
    --	return const.FERR_NOT_OP
    --end

    local banmessage = "Channel bans for "
    if c.getType(chan) == "public" then
        banmessage = banmessage .. c.getName(chan) .. ": "
    else
        banmessage = banmessage .. c.getTitle(chan) .. ": "
    end
    local banlist = c.getBanList(chan)
    local banl = ""
    for c, ban in ipairs(banlist) do
        banl = banl .. ", " .. ban
    end
    banl = string.sub(banl, 3)
    u.send(con, "SYS", { channel = args.channel, message = banmessage .. banl })
    return const.FERR_OK
end

-- Bans a user from a channel.
-- Syntax:: CKU <channel> <character>
event.CBU =
function(con, args)
    if args.channel == nil or args.character == nil then
        return const.FERR_BAD_SYNTAX
    end

    local fchan, chan = c.getChannel(string.lower(args.channel))
    if fchan ~= true then
        return const.FERR_CHANNEL_NOT_FOUND
    end

    local staff_override = public_staff_override(con, chan)

    -- V: 2018-07-25
    if c.isMod(chan, con) ~= true and staff_override ~= true then
        return const.FERR_NOT_OP
    end

    -- V: 2018-07-25
    if c.getBanCount(chan) >= const.MAX_CHANNEL_BANS and staff_override ~= true then
        return const.FERR_TOO_MANY_CHANNEL_BANS
    end

    local targetonline, char = u.getConnection(string.lower(args.character))
    local chantype = c.getType(chan)
    if chantype == "public" then
        -- V: 2018-07-25
        if targetonline == true and (c.isMod(chan, char) == true or
                u.hasAnyRole(char, { "admin", "global", "super-cop", "cop" })) then
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
        s.logMessage("channel_ban", con, chan, char, nil)
        c.sendAll(chan, "CBU", { channel = args.channel, operator = u.getName(con), character = char })
        c.ban(chan, con, string.lower(char))
    else
        if c.isBanned(chan, char) == true then
            return const.FERR_ALREADY_CHANNEL_BANNED
        end
        s.logMessage("channel_ban", con, chan, char, nil)
        c.sendAll(chan, "CBU", { channel = args.channel, operator = u.getName(con), character = u.getName(char) })
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
function(con, args)
    if args.channel == nil then
        return const.FERR_BAD_SYNTAX
    end

    local title = s.escapeHTML(args.channel)
    if #title > const.MAX_TITLE_LEN then
        u.sendError(con, 67, "Channel titles may not exceed " .. const.MAX_TITLE_LEN .. " characters in length.")
        return const.FERR_OK
    end
    if #args.channel <= 0 then
        u.sendError(con, 67, "Channel titles may not be empty.")
        return const.FERR_OK
    end
    local name, chan = c.createPrivateChannel(con, title)

    joinChannel(chan, con)
    return const.FERR_OK
end

-- Sets the channel description.
-- Syntax: CDS <channel> <description>
event.CDS =
function(con, args)
    if args.channel == nil or args.description == nil then
        return const.FERR_BAD_SYNTAX
    end

    local found, chan = c.getChannel(string.lower(args.channel))
    if found ~= true then
        return const.FERR_CHANNEL_NOT_FOUND
    end

    if #args.description > const.CDS_MAX then
        return const.FERR_DESCRIPTION_TOO_LONG
    end

    -- V: 2018-07-25
    if c.isMod(chan, con) ~= true and public_staff_override(con, chan) ~= true then
        return const.FERR_NOT_OP
    end

    if c.getType(chan) == "public" then
        s.logAction(con, "CDS", args)
    end
    s.logMessage("channel_description", con, chan, nil, args.description)
    local newdesc = s.escapeHTML(args.description)
    c.setDescription(chan, newdesc)
    c.sendAll(chan, "CDS", { channel = args.channel, description = newdesc })

    return const.FERR_OK
end

-- Gets the list of public channels.
-- Syntax: CHA
event.CHA =
function(con, args)
    if u.checkUpdateTimer(con, "cha", const.CHA_FLOOD) == true then
        return const.FERR_OK
    end
    c.sendCHA(con)
    return const.FERR_OK
end

-- Sends an invite for a private channel to another users.
-- Syntax: CIU <channel> <character>
event.CIU =
function(con, args)
    if args.channel == nil or args.character == nil then
        return const.FERR_BAD_SYNTAX
    end

    local found, chan = c.getChannel(string.lower(args.channel))
    if found ~= true then
        return const.FERR_CHANNEL_NOT_FOUND
    end

    if c.inChannel(chan, con) ~= true then
        return const.FERR_USER_NOT_IN_CHANNEL
    end

    local fchar, char = u.getConnection(string.lower(args.character))
    if fchar ~= true then
        return const.FERR_USER_NOT_FOUND
    end

    local chantype = c.getType(chan)

    if chantype == "public" then
        return const.FERR_INVITE_TO_PUBLIC
    end

    -- V: 2018-07-25
    if chantype ~= "pubprivate" and c.isMod(chan, con) ~= true and c.hasAnyRole(con, { "admin", "global" }) ~= true then
        return const.FERR_NOT_OP
    end

    if chantype == "private" then
        c.invite(chan, char)
    end
    u.send(char, "CIU", { sender = u.getName(con), title = c.getTitle(chan), name = c.getName(chan) })
    u.send(con, "SYS", { message = "Your invitation has been sent." })
    return const.FERR_OK
end

-- Kicks a user from a channel.
-- Syntax:: CKU <channel> <character>
event.CKU =
function(con, args)
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

    -- V: 2018-07-25
    if c.isMod(chan, con) ~= true and public_staff_override(con, chan) ~= true then
        return const.FERR_NOT_OP
    end

    if (c.inChannel(chan, con) ~= true) or (c.inChannel(chan, char) ~= true) then
        return const.FERR_USER_NOT_IN_CHANNEL
    end

    local chantype = c.getType(chan)
    if chantype == "public" then
        -- V: 2018-07-25
        if c.isMod(chan, char) == true or u.hasAnyRole(char, { "admin", "global", "super-cop", "cop" }) then
            return const.FERR_DENIED_ON_OP
        end
        s.logAction(con, "CKU", args)
    elseif chantype == "private" then
        c.removeInvite(chan, string.lower(args.character))
    end

    s.logMessage("channel_kick", con, chan, char, nil)
    c.sendAll(chan, "CKU", { channel = args.channel, operator = u.getName(con), character = u.getName(char) })
    partChannel(chan, char)
    return const.FERR_OK
end

-- Adds a moderator to a channel.
-- Syntax: COA <channel> <character>
event.COA =
function(con, args)
    if args.channel == nil or args.character == nil then
        return const.FERR_BAD_SYNTAX
    end

    local found, chan = c.getChannel(string.lower(args.channel))
    if found ~= true then
        return const.FERR_CHANNEL_NOT_FOUND
    end

    -- V: 2018-07-25
    if c.isOwner(chan, con) ~= true and u.hasAnyRole(con, { "admin", "global" }) ~= true then
        return const.FERR_NOT_OP
    end

    if (c.isMod(chan, args.character) == false) then
        c.addMod(chan, con, args.character)

        local modmessage = args.character .. " has been added to the moderator list for "
        if c.getType(chan) == "public" then
            s.logAction(con, "COA", args)
            modmessage = modmessage .. c.getName(chan)
        else
            modmessage = modmessage .. c.getTitle(chan)
        end

        s.logMessage("channel_op_add", con, chan, args.character, nil)
        if args.silent == nil then
            c.sendAll(chan, "SYS", { channel = args.channel, message = modmessage })
        end
        c.sendAll(chan, "COL", { channel = args.channel, array_oplist = c.getModList(chan) })
        c.sendAll(chan, "COA", { channel = args.channel, character = args.character })
    end

    return const.FERR_OK
end

-- Lists the moderators for a channel.
-- Syntax: COL <channel>
event.COL =
function(con, args)
    if args.channel == nil then
        return const.FERR_BAD_SYNTAX
    end

    local found, chan = c.getChannel(string.lower(args.channel))
    if found ~= true then
        return const.FERR_CHANNEL_NOT_FOUND
    end

    -- V: 2018-07-25
    --if c.isMod(chan, con) ~= true then
    --	return const.FERR_NOT_OP
    --end

    local opmessage = "Channel moderators for "
    if c.getType(chan) == "public" then
        opmessage = opmessage .. c.getName(chan) .. ": "
    else
        opmessage = opmessage .. c.getTitle(chan) .. ": "
    end
    local oplist = c.getModList(chan)
    local opl = ""
    for c, op in ipairs(oplist) do
        if op ~= "" then
            opl = opl .. ", " .. op
        end
    end
    opl = string.sub(opl, 3)
    u.send(con, "COL", { channel = args.channel, array_oplist = oplist })
    u.send(con, "SYS", { channel = args.channel, message = opmessage .. opl })
    return const.FERR_OK
end

-- Removes a moderator from a channel.
-- Syntax: COR <channel> <character>
event.COR =
function(con, args)
    if args.channel == nil or args.character == nil then
        return const.FERR_BAD_SYNTAX
    end

    local found, chan = c.getChannel(string.lower(args.channel))
    if found ~= true then
        return const.FERR_CHANNEL_NOT_FOUND
    end

    -- V: 2018-07-25
    if c.isOwner(chan, con) ~= true and u.hasAnyRole(con, { "admin", "global" }) ~= true then
        return const.FERR_NOT_OP
    end

    if c.isMod(chan, args.character) == true then
        c.removeMod(chan, args.character)

        local modmessage = args.character .. " has been removed from the moderator list for "
        if c.getType(chan) == "public" then
            s.logAction(con, "COR", args)
            modmessage = modmessage .. c.getName(chan)
            local cfound, opchan = c.getChannel("adh-uberawesomestaffroom")
            if cfound == true then
                c.removeInvite(opchan, string.lower(args.character))
            end
        else
            modmessage = modmessage .. c.getTitle(chan)
        end

        s.logMessage("channel_op_remove", con, chan, args.character, nil)
        if args.silent == nil then
            c.sendAll(chan, "SYS", { channel = args.channel, message = modmessage })
        end
        c.sendAll(chan, "COL", { channel = args.channel, array_oplist = c.getModList(chan) })
        c.sendAll(chan, "COR", { channel = args.channel, character = args.character })
    end

    return const.FERR_OK
end

-- Creates a new public channel.
-- Syntax: CRC <channel>
event.CRC =
function(con, args)
    if args.channel == nil then
        return const.FERR_BAD_SYNTAX
    end

    -- V: 2018-07-25
    if u.hasRole(con, "admin") ~= true then
        return const.NOT_OP
    end

    local found, chan = c.getChannel(string.lower(args.channel))
    if found ~= true then
        c.createChannel(args.channel)
        u.send(con, "SYS", { message = args.channel .. " has been created as a public channel." })
    end

    return const.FERR_OK
end

-- Sets a new channel owner.
-- Syntax: CSO <channel> <character>
event.CSO =
function(con, args)
    if args.channel == nil or args.character == nil then
        return const.FERR_BAD_SYNTAX
    end

    local found, chan = c.getChannel(string.lower(args.channel))
    if found ~= true then
        return const.FERR_CHANNEL_NOT_FOUND
    end

    -- V: 2018-07-25
    if c.isOwner(chan, con) ~= true and u.hasAnyRole(con, { "admin", "global" }) ~= true then
        return const.FERR_NOT_OP
    end

    local ufound, char = u.getConnection(string.lower(args.character))
    if ufound ~= true then
        return const.FERR_USER_NOT_FOUND
    end

    c.setOwner(chan, char)

    local modmessage = args.character .. " is now channel owner of "
    if c.getType(chan) == "public" then
        modmessage = modmessage .. c.getName(chan)
        s.logAction(con, "CSO", args)
    else
        modmessage = modmessage .. c.getTitle(chan)
    end

    s.logMessage("channel_owner_set", con, chan, char, nil)
    c.sendAll(chan, "SYS", { channel = args.channel, message = modmessage })
    c.sendAll(chan, "COL", { channel = args.channel, array_oplist = c.getModList(chan) })
    c.sendAll(chan, "CSO", { channel = args.channel, character = u.getName(char) })
    c.sendAll(chan, "COA", { channel = args.channel, character = u.getName(char) })

    return const.FERR_OK
end

-- Times out a user from a channel.
-- Syntax:: CTU <channel> <character> <length>
event.CTU =
function(con, args)
    if args.channel == nil or args.character == nil or args.length == nil then
        return const.FERR_BAD_SYNTAX
    end

    local length = tonumber(args.length)
    if length == nil or length < 1 then
        return const.FERR_BAD_TIMEOUT_FORMAT
    end
    length = length * 60

    local fchan, chan = c.getChannel(string.lower(args.channel))
    if fchan ~= true then
        return const.FERR_CHANNEL_NOT_FOUND
    end

    local staff_override = public_staff_override(con, chan)

    -- V: 2018-07-25
    if c.isMod(chan, con) ~= true and staff_override ~= true then
        return const.FERR_NOT_OP
    end

    if c.inChannel(chan, con) ~= true then
        return const.FERR_USER_NOT_IN_CHANNEL
    end

    -- V: 2018-07-25
    if c.getBanCount(chan) >= const.MAX_CHANNEL_BANS and staff_override ~= true then
        return const.FERR_TOO_MANY_CHANNEL_BANS
    end

    local targetonline, char = u.getConnection(string.lower(args.character))
    local chantype = c.getType(chan)
    if chantype == "public" then
        -- V: 2018-07-25
        if targetonline == true and (c.isMod(chan, char) == true or u.hasAnyRole(char, { "admin", "global", "super-cop", "cop" })) then
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
        s.logMessage("channel_timeout", con, chan, char, args.length)
        c.sendAll(chan, "CTU", { channel = args.channel, operator = u.getName(con), character = char, length = tonumber(args.length) })
        c.timeout(chan, con, string.lower(char), length)
    else
        if c.isBanned(chan, char) == true then
            return const.FERR_ALREADY_CHANNEL_BANNED
        end
        s.logMessage("channel_timeout", con, chan, char, args.length)
        c.sendAll(chan, "CTU", { channel = args.channel, operator = u.getName(con), character = u.getName(char), length = tonumber(args.length) })
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
function(con, args)
    if args.channel == nil or args.character == nil then
        return const.FERR_BAD_SYNTAX
    end

    local fchan, chan = c.getChannel(string.lower(args.channel))
    if fchan ~= true then
        return const.FERR_CHANNEL_NOT_FOUND
    end

    -- V: 2018-07-25
    if c.isMod(chan, con) ~= true and public_staff_override(con, chan) ~= true then
        return const.FERR_NOT_OP
    end

    if c.isBanned(chan, string.lower(args.character)) ~= true then
        return const.FERR_NOT_CHANNEL_BANNED
    end

    if c.getType(chan) == "public" then
        s.logAction(con, "CUB", args)
    end

    s.logMessage("channel_ban_remove", con, chan, args.character, nil)
    c.unban(chan, string.lower(args.character))
    u.send(con, "SYS", { channel = args.channel, message = args.character .. " has been removed from the channel ban list." })
    return const.FERR_OK
end

-- Removes a global moderator.
-- Syntax: DOP <character>
event.DOP =
function(con, args)
    if args.character == nil then
        return const.FERR_BAD_SYNTAX
    end

    -- V: 2018-07-25
    if u.hasRole(con, "admin") ~= true then
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
    s.logMessage("global_op_remove", con, nil, args.character, nil)
    s.broadcast("DOP", { character = args.character })
    u.send(con, "SYS", { message = args.character .. " has been removed as a global moderator." })
    return const.FERR_OK
end

-- Finds other characters by kinks or other properties.
-- Syntax: FKS <kinks>
event.FKS =
function(con, args)
    return const.FERR_NOT_IMPLEMENTED
end

-- Gets the friend list for a connection.
-- Syntax: FRL
event.FRL =
function(con, args)
    u.send(con, "FRL", { array_characters = u.getFriendList(con) })
    return const.FERR_OK
end

-- Identification command.
-- Syntax: IDN <Complex stuff goes here.>
event.IDN =
function(con, args)
    error("This function is native and should not be called.")
end

-- Processes ignore lists.
-- Syntax: IGN <It's complicated>
event.IGN =
function(con, args)
    if args.action == nil then
        return const.FERR_BAD_SYNTAX
    end

    if args.action == "list" then
        u.send(con, "IGN", { action = "list", array_characters = u.getIgnoreList(con) })
    elseif args.action == "add" and args.character ~= nil then
        if s.isOp(args.character) == true then
            return const.FERR_DENIED_ON_OP
        end
        local ignorecount = #u.getIgnoreList(con)
        local maxignores = const.MAX_IGNORES
        if ignorecount < maxignores then
            s.logMessage("ignore_add", con, nil, args.character, nil)
            u.addIgnore(con, string.lower(args.character))
            propagateIgnoreList(con, "add", args.character)
        else
            u.sendError(con, 64, "Your ignore list may not exceed " .. maxignores .. " people.")
            return const.FERR_OK
        end
    elseif args.action == "delete" and args.character == "*" then
        s.logMessage("ignore_remove", con, nil, args.character, nil)
        local ignores = u.getIgnoreList(con)
        for i, v in ipairs(ignores) do
            u.removeIgnore(con, string.lower(v))
        end
        local account_cons = u.getByAccount(con)
        for i, v in ipairs(account_cons) do
            u.setIgnores(v, ignores)
            u.send(con, "IGN", { action = "init", array_characters = u.getIgnoreList(con) })
        end
    elseif args.action == "delete" and args.character ~= nil then
        s.logMessage("ignore_remove", con, nil, args.character, nil)
        u.removeIgnore(con, string.lower(args.character))
        propagateIgnoreList(con, "delete", args.character)
    elseif args.action == "notify" and args.character ~= nil then
        if u.checkUpdateTimer(con, "ign", const.IGN_FLOOD) ~= true then
            local found, char = u.getConnection(string.lower(args.character))
            if found == true then
                s.logMessage("ignore_notify", con, nil, char, nil)
                u.sendError(char, 20, u.getName(con) .. " does not wish to receive messages from you.")
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
function(con, args)
    if args.channel == nil then
        return const.FERR_BAD_SYNTAX
    end

    if u.getChannelCount(con) >= 75 and u.getMiscData(con, "no_channel_limit") ~= "yes" then
        return const.FERR_TOO_MANY_CHANNELS
    end

    local found, chan = c.getChannel(string.lower(args.channel))
    if found ~= true then
        return const.FERR_CHANNEL_NOT_FOUND
    end

    if c.inChannel(chan, con) == true then
        return const.FERR_ALREADY_IN_CHANNEL
    end

    local staff_override = u.hasAnyRole(con, { "admin", "global" })

    -- V: 2018-07-25
    if c.isOwner(chan, con) ~= true and staff_override ~= true and c.isBanned(chan, con) ~= false then
        local banned, ban = c.getBan(chan, con)
        if banned == true and ban.timeout ~= 0 then
            u.sendError(con, const.FERR_CHANNEL_BANNED, string.format("You are banned from the channel for another %.2f minute(s).", ((ban.timeout - s.getTime()) / 60)))
            return const.FERR_OK
        end
        return const.FERR_CHANNEL_BANNED
    end

    local chantype = c.getType(chan)
    if chantype == "private" and (c.isInvited(chan, con) ~= true and c.isOwner(chan, con) ~= true and staff_override ~= true) then
        return const.FERR_NOT_INVITED
    end

    joinChannel(chan, con)
    return const.FERR_OK
end

-- Destroys a channel.
-- Syntax: KIC <channel>
event.KIC =
function(con, args)
    if args.channel == nil then
        return const.FERR_BAD_SYNTAX
    end

    local found, chan = c.getChannel(string.lower(args.channel))
    if found ~= true then
        return const.FERR_CHANNEL_NOT_FOUND
    end

    local staff_override = u.hasRole(con, "admin")

    -- V: 2018-07-25
    if c.isOwner(chan, con) ~= true and staff_override ~= true then
        return const.FERR_NOT_OP
    end
    if staff_override == true then
        s.logAction(con, "KIC", args)
    end
    s.logMessage("channel_destroy", con, chan, nil, nil)
    c.sendAll(chan, "BRO", { message = "You are being removed from the channel " .. c.getName(chan) .. ". The channel is being destroyed." })
    c.destroyChannel(string.lower(args.channel))
    u.send(con, "SYS", { message = args.channel .. " has been removed as a channel." })

    return const.FERR_OK
end

-- Kicks a user from the server.
-- Syntax:: KIK <character>
event.KIK =
function(con, args)
    if args.character == nil then
        return const.FERR_BAD_SYNTAX
    end

    -- V: 2018-07-25
    if u.hasAnyRole(con, { "admin", "global" }) ~= true then
        return const.FERR_NOT_OP
    end

    local found, char = u.getConnection(string.lower(args.character))
    if found ~= true then
        return const.FERR_USER_NOT_FOUND
    end

    s.logAction(con, "KIK", args)
    s.logMessage("global_kick", con, nil, char, nil)
    u.sendError(char, const.FERR_KICKED)
    u.send(con, "SYS", { message = u.getName(char) .. " has been kicked from chat." })
    u.close(char)
    return const.FERR_OK
end

-- Sends a characters custom kinks.
-- Syntax: KIN <character>
event.KIN =
function(con, args)
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

    u.send(con, "KID", { type = "start", character = charname, message = "Custom kinks of " .. charname })

    local customkinks = u.getCustomKinks(char)
    for k, v in pairs(customkinks) do
        u.send(con, "KID", { type = "custom", character = charname, key = k, value = v })
    end

    u.send(con, "KID", { type = "end", character = charname, message = "End of custom kinks." })

    return const.FERR_OK
end

-- Leaves a channel.
-- Syntax: LCH <channel>
event.LCH =
function(con, args)
    if args.channel == nil then
        return const.FERR_BAD_SYNTAX
    end

    local charname = u.getName(con)

    local found, chan = c.getChannel(string.lower(args.channel))
    if found ~= true then
        u.send(con, "LCH", { channel = args.channel, character = charname })
        return const.FERR_CHANNEL_NOT_FOUND
    end

    if c.inChannel(chan, con) ~= true then
        u.send(con, "LCH", { channel = args.channel, character = charname })
        return const.FERR_OK
    end

    partChannel(chan, con)
    return const.FERR_OK
end

-- Sends an RP ad to a channel.
-- Syntax: LRP <channel> <message>
event.LRP =
function(con, args)
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

    if hasShortener(args.message) ~= false then
        return const.FERR_OK
    end

    if u.getMiscData(con, "hellban") ~= nil then
        return const.FERR_OK
    end

    s.logMessage("message_ad", con, chan, nil, args.message)
    c.sendChannel(chan, con, "LRP", { channel = c.getName(chan), character = u.getName(con), message = s.escapeHTML(args.message) })
    return const.FERR_OK
end

-- Sends a chat message to a channel.
-- Syntax: MSG <channel> <message>
event.MSG =
function(con, args)
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

    if hasShortener(args.message) ~= false then
        return const.FERR_OK
    end

    if u.getMiscData(con, "hellban") ~= nil then
        return const.FERR_OK
    end

    s.logMessage("message", con, chan, nil, args.message)
    c.sendChannel(chan, con, "MSG", { channel = c.getName(chan), character = u.getName(con), message = s.escapeHTML(args.message) })
    return const.FERR_OK
end

-- Gets the list of open private channels.
-- Syntax: ORS
event.ORS =
function(con, args)
    if u.checkUpdateTimer(con, "ors", const.ORS_FLOOD) == true then
        return const.FERR_OK
    end
    c.sendORS(con)
    return const.FERR_OK
end

-- Gets a list of pending chat reports.
-- Syntax: PCR
event.PCR =
function(con, args)
    -- V: 2018-07-25
    if u.hasAnyRole(con, { "admin", "global", "super-cop", "cop" }) ~= true then
        return const.FERR_NOT_OP
    end
    s.sendStaffCalls(con)
    u.send(con, "SYS", { message = "Pending chat reports sent." })
    return const.FERR_OK
end

-- Sends a private message to another user.
-- Syntax: PRI <recipient> <message>
event.PRI =
function(con, args)
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

    s.logMessage("message_private", con, nil, target, args.message)
    u.send(target, "PRI", { character = u.getName(con), message = s.escapeHTML(args.message), recipient = args.recipient })
    return const.FERR_OK
end

-- Sends a characters mini profile.
-- Syntax:: PRO <character>
event.PRO =
function(con, args)
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

    u.send(con, "PRD", { type = "start", character = charname, message = "Profile of " .. charname })

    local infotags = u.getInfoTags(char)
    for k, v in pairs(infotags) do
        u.send(con, "PRD", { type = "info", character = charname, key = k, value = v })
    end

    u.send(con, "PRD", { type = "end", character = charname, message = "End of profile." })

    return const.FERR_OK
end

-- Reloads and optionally saves ops/bans
-- Syntax: RLD <?save>
event.RLD =
function(con, args)
    -- V: 2018-07-25
    if u.hasRole(con, "admin") ~= true then
        return const.FERR_NOT_ADMIN
    end

    s.logAction(con, "RLD", args)
    if args.save ~= nil then
        s.reload(true)
        u.send(con, "SYS", { message = "Saved ops, bans, and channels to disk." })
    else
        s.reload(false)
    end
    u.send(con, "SYS", { message = "Reloaded config variables, ops, and bans from disk." })
    return const.FERR_OK
end

-- Rolls dice or spins the bottle in a channel or private message.
-- Syntax: RLL <channel> <dice>
event.RLL =
function(con, args)
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

        local chantype = c.getType(chan)
        if args.dice == "bottle" and chantype == "public" then
            u.sendError(con, -10, "You spin the bottle, but nobody came.")
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
            u.send(con, "SYS", { message = "Couldn't locate anyone who is available to have the bottle land on them." })
            return FERR_BAD_SYNTAX
        else
            if haschannel then
                s.logMessage("message_bottle", con, chan, nil, bottle.target)
                bottle.channel = args.channel
                c.sendAll(chan, "RLL", bottle)
            else
                s.logMessage("message_bottle", con, nil, target, bottle.target)
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
        s.logMessage("message_roll", con, chan, nil, roll.message)
        roll.channel = c.getName(chan)
        c.sendAll(chan, "RLL", roll)
    else
        s.logMessage("message_roll", con, nil, target, roll.message)
        roll.recipient = u.getName(target)
        u.send(target, "RLL", roll)
        u.send(con, "RLL", roll)
    end

    return const.FERR_OK
end

-- Set the message mode for a channel.
-- Syntax: RMO <channel> <mode(chat/ads/both)>
event.RMO =
function(con, args)
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

    -- V: 2018-07-25
    if c.isOwner(chan, con) ~= true and u.hasAnyRole(con, { "admin", "global" }) ~= true then
        return const.FERR_NOT_OP
    end

    c.setMode(chan, newmode)
    c.sendAll(chan, "RMO", { channel = c.getName(chan), mode = newmode })
    return const.FERR_OK
end

-- Sets a private channels public status.
-- Syntax: RST <status(public/closed)>
event.RST =
function(con, args)
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

    -- V: 2018-07-25
    if c.isOwner(chan, con) ~= true and u.hasAnyRole(con, { "admin", "global" }) ~= true then
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
    u.send(con, "SYS", { channel = string.lower(args.channel), message = "This channel is now [b]" .. statusmsg .. "[/b]" })
    return const.FERR_OK
end

-- Rewards a character with a crown status.
-- Syntax:: RWD <character>
event.RWD =
function(con, args)
    if args.character == nil then
        return const.FERR_BAD_SYNTAX
    end

    -- V: 2018-07-25
    if u.hasRole(con, "admin") ~= true then
        return const.FERR_NOT_ADMIN
    end

    local found, target = u.getConnection(string.lower(args.character))
    if found ~= true then
        return const.FERR_USER_NOT_FOUND
    end

    s.logAction(con, "RWD", args)
    local oldstatus, statusmesg = u.getStatus(target)
    u.setStatus(target, "crown", statusmesg)

    s.broadcast("STA", { character = u.getName(target), status = "crown", statusmsg = statusmesg })
    return const.FERR_OK
end

event.SCP =
function(con, args)
    if args.action == nil and args.character == nil then
        return const.FERR_BAD_SYNTAX
    end

    -- V: 2018-08-05
    if u.hasRole(con, "admin") ~= true then
        return const.FERR_NOT_ADMIN
    end

    if args.action == "add" then
        s.addSCop(args.character)
    elseif args.action == "remove" then
        s.removeSCop(args.character)
    else
        return const.FERR_BAD_SYNTAX
    end

    return const.FERR_OK
end

-- Processes a staff call request.
-- Syntax: SFC <action> (<character> <report>/<callid> <moderator>)
event.SFC =
function(con, args)
    if args.action == nil then
        return const.FERR_BAD_SYNTAX
    end

    if args.action == "report" then
        if args.report == nil then
            return const.FERR_BAD_SYNTAX
        end
        -- V: 2018-07-25
        if u.hasAnyRole(con, { "admin", "global", "super-cop", "cop" }) ~= true then
            if u.checkUpdateTimer(con, "sfc", const.SFC_FLOOD) == true then
                return const.FERR_THROTTLE_STAFF_CALL
            end
        end
        local lname = u.getName(con)
        local ltimestamp = s.getTime()
        local lcallid = ltimestamp .. ":" .. lname
        local llogid = tonumber(args.logid)
        local ltab = "Unknown Tab"
        if args.tab ~= nil then
            ltab = args.tab
        end
        local lreport = s.escapeHTML(args.report)
        if llogid == 0 or llogid == nil then
            llogid = -1
        end
        s.addStaffCall(lcallid, lname, lreport, llogid, ltab)
        local lsfc = { callid = lcallid, action = "report", report = lreport, timestamp = ltimestamp, character = lname, tab = s.escapeHTML(ltab) }
        if llogid ~= -1 then
            lsfc.logid = llogid
        end
        s.broadcastStaffCall("SFC", lsfc)
        local chanfound, chan = c.getChannel(string.lower(ltab))
        if chanfound == true and c.getType(chan) == "public" then
            broadcastChannelOps("SFC", lsfc, chan)
        end
        u.send(con, "SYS", { message = "The moderators have been alerted." })
    elseif args.action == "confirm" then
        local call = s.getStaffCall(args.callid)
        if call == false then
            return const.FERR_OK
        end
        local authorized_ops = nil
        local channel_override = false
        local chanfound, chan = c.getChannel(string.lower(call.tab))
        if chanfound == true and c.getType(chan) ~= "public" then
            chanfound = false
        end
        if chanfound == true then
            authorized_ops = c.getModList(chan)
        end
        if authorized_ops ~= nil then
            local lname = string.lower(u.getName(con))
            for i, v in ipairs(authorized_ops) do
                if string.lower(v) == lname then
                    channel_override = true
                    break
                end
            end
        end
        -- V: 2018-07-25
        if channel_override == false and u.hasAnyRole(con, { "admin", "global", "super-cop" }) ~= true then
            return const.FERR_NOT_OP
        end
        --		if call.logid ~= -1 then
        --			local aid, cid = u.getAccountCharacterIDs(con)
        --			http.post("handle_report", s.getConfigString("handle_report_url"), {
        --				log_id=tostring(call.logid),
        --				moderator=tostring(cid),
        --				secret=s.getConfigString("handle_report_secret")
        --			}, con, nil)
        --		end
        local lsfc = { action = "confirm", moderator = u.getName(con), character = call.character, timestamp = call.timestamp, tab = s.escapeHTML(call.tab), logid = call.logid }
        s.logAction(con, "SFC", lsfc)
        s.removeStaffCall(args.callid)
        s.broadcastStaffCall("SFC", lsfc)
        if chanfound == true then
            broadcastChannelOps("SFC", lsfc, chan)
        end
    else
        return const.FERR_BAD_SYNTAX
    end
    return const.FERR_OK
end

-- Sets a connections status, and status message.
-- Syntax STA <status> ?<statusmessage>
event.STA =
function(con, args)
    if u.checkUpdateTimer(con, "sta", const.STA_FLOOD) == true then
        return const.FERR_THROTTLE_STATUS
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
    s.logMessage("status", con, nil, nil, "Status: " .. newstatus .. " Message: " .. statusmessage)
    s.broadcast("STA", { character = u.getName(con), status = newstatus, statusmsg = statusmessage })
    return const.FERR_OK
end

-- Sets a timeout on an account.
-- Syntax: TMO <character> <time> <reason>
event.TMO =
function(con, args)
    if args.character == nil or args.time == nil or args.reason == nil then
        return const.FERR_BAD_SYNTAX
    end

    local length = tonumber(args.time)
    if length == nil or length < 1 then
        return const.FERR_BAD_TIMEOUT_FORMAT
    end

    -- V: 2018-07-25
    if u.hasAnyRole(con, { "admin", "global" }) ~= true then
        return const.FERR_NOT_OP
    end

    local found, char = u.getConnection(string.lower(args.character))
    if found ~= true then
        return const.FERR_USER_NOT_FOUND
    end

    -- V: 2018-07-25
    if (u.isAdmin(char) == true) or (u.isGlobMod(char) == true) then
        return const.FERR_DENIED_ON_OP
    end

    local reason = s.escapeHTML(args.reason)

    s.logAction(con, "TMO", args)
    s.logMessage("global_timeout", con, nil, char, "Length: " .. length .. " Reason: " .. reason)
    s.addTimeout(char, length * 60)
    u.send(con, "SYS", { message = u.getName(char) .. " has been given a " .. length .. " minute time out for: " .. reason })
    local account_cons = u.getByAccount(char)
    for i, v in ipairs(account_cons) do
        u.sendError(v, const.FERR_TIMED_OUT, "You have been given a time out by " .. u.getName(con) .. " for " .. length .. " minute(s). The reason given was: " .. reason)
        u.close(v)
    end
    return const.FERR_OK
end

-- Sends a typing status message to another character.
-- Syntax: TPN <target> <status>
event.TPN =
function(con, args)
    if args.character == nil or args.status == nil then
        return const.FERR_BAD_SYNTAX
    end

    if const.typing[args.status] == nil then
        return const.FERR_OK
    end

    local found, target = u.getConnection(string.lower(args.character))
    if found ~= true then
        return const.FERR_OK
    end
    u.send(target, "TPN", { character = u.getName(con), status = args.status })
    return const.FERR_OK
end

-- Removes a global server ban.
-- Syntax: UNB <character>
event.UNB =
function(con, args)
    if args.character == nil then
        return const.FERR_BAD_SYNTAX
    end

    -- V: 2018-07-25
    if u.hasAnyRole(con, { "admin", "global" }) ~= true then
        return const.FERR_NOT_OP
    end

    s.logAction(con, "UNB", args)
    s.logMessage("global_unban", con, nil, args.character, nil)
    s.removeTimeout(string.lower(args.character))
    if s.removeBan(string.lower(args.character)) == true then
        u.send(con, "SYS", { message = "Removed ban successfully." })
    else
        u.send(con, "SYS", { message = "Could not find the ban in question." })
    end
    return const.FERR_OK
end

-- Returns various stats about the server.
-- Syntax: UPT
event.UPT =
function(con, args)
    local lusers, lmax_users, lchannels, lstart_time, ltime, laccepted, lstart_string = s.getStats()
    u.send(con, "UPT", { users = lusers, maxusers = lmax_users, channels = lchannels, starttime = lstart_time, time = ltime, accepted = laccepted, startstring = lstart_string })
    return const.FERR_OK
end

-- Debug console command.
event.ZZZ =
function(con, args)
    error("This function is native and should not be called.")
end

-- -----------------------------------------------------------------------------------------------------------------------------------

event.ident_callback =
function(con, args)
    if args.error ~= "" then
        print("Error returned from login server was: " .. args.error)
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

    if tonumber(args.account.banned) ~= 0 or tonumber(args.account.timeout) > s.getTime() then
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
    if tonumber(args.char.character_id) == 0 then
        print("Failed to login because the character id was zero")
        return const.FERR_IDENT_FAILED
    end
    u.setCharacterID(con, args.char.character_id)

    if lname == "adl" then
        u.setMiscData(con, "no_channel_limit", "yes")
    end

    local isadmin = false
    -- V: 2018-07-25
    if args.account.admin == "1" or lname == "kira" then
        u.setAdmin(con, true)
        isadmin = true
    end

    local isop = false
    -- V: 2018-07-25
    if s.isOp(name) == true then
        u.setGlobMod(con, true)
        isop = true
    end

    local issupercop = false
    -- V: 2018-07-25
    local iscop = s.isChanOp(con)
    if iscop then
        u.addRole(con, "cop")
    end

    -- V: 2018-07-25
    if (s.isBanned(con) ~= false) and (isop ~= true) and (isadmin ~= true) then
        return const.FERR_BANNED_FROM_SERVER
    end

    local timedout, left = s.isTimedOut(con)
    -- V: 2018-07-25
    if (timedout ~= false) and (isop ~= true) and (isadmin ~= true) then
        u.sendError(con, const.FERR_BANNED_FROM_SERVER, string.format("You have been timed out from chat. There are %.2f minute(s) left on your time out.", (left / 60)))
        u.close(con)
        return const.FERR_OK
    end

    if args.select ~= nil then
        if args.select.Gender ~= nil and const.gender[string.lower(args.select.Gender)] ~= nil then
            u.setGender(con, args.select.Gender)
        else
            args.select.Gender = "None"
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
        local newkinks = { -1 }
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
        if args.bits.subscribed == 1 then
            u.setMiscData(con, "subscribed", "yes")
        end
        if (args.bits.coder == 1) or (args.bits.admin == 1) or (args.bits.moderator == 1) or (args.bits.helpdesk == 1)
                or (args.bits.chanop == 1) or (args.bits.chatop == 1) then
            isstaff = true
        end
        if args.bits.supercop == 1 then
            u.addRole(con, "super-cop")
            isstaff = true
            issupercop = true
        end
    end

    u.send(con, "IDN", { character = name })
    u.send(con, "VAR", { variable = "chat_max", value = const.MSG_MAX })
    u.send(con, "VAR", { variable = "priv_max", value = const.PRI_MAX })
    u.send(con, "VAR", { variable = "lfrp_max", value = const.LRP_MAX })
    u.send(con, "VAR", { variable = "cds_max", value = const.CDS_MAX })
    u.send(con, "VAR", { variable = "lfrp_flood", value = const.LRP_FLOOD })
    u.send(con, "VAR", { variable = "msg_flood", value = const.MSG_FLOOD })
    u.send(con, "VAR", { variable = "sta_flood", value = const.STA_FLOOD })
    u.send(con, "VAR", { variable = "permissions", value = args.permissions })
    u.send(con, "VAR", { variable = "icon_blacklist", array_value = const.NO_ICON_CHANNELS })

    u.send(con, "HLO", { message = "Welcome. Running F-Chat (" .. const.VERSION .. "). Enjoy your stay." })
    u.send(con, "CON", { count = s.getUserCount() })

    if args.array_friends ~= nil then
        u.send(con, "FRL", { array_characters = args.array_friends })
    end
    if args.array_ignores ~= nil then
        u.send(con, "IGN", { action = "init", array_characters = args.array_ignores })
    end
    u.send(con, "ADL", { array_ops = s.getOpList() })

    s.sendUserList(con, "LIS", 100)

    s.logMessage("connect", con, nil, nil, nil)
    s.broadcast("NLN", { identity = name, status = "online", gender = u.getGender(con) })

    if isop or issupercop then
        s.addToStaffCallTargets(con)
        s.sendStaffCalls(con)
    end

    if isstaff or isop or issupercop or iscop then
        local found, chan = c.getChannel("adh-uberawesomestaffroom")
        if found == true then
            c.invite(chan, con)
            joinChannel(chan, con)
        end
    end

    return const.FERR_OK
end

event.pre_disconnect =
function(con)
    local name = u.getName(con)
    local channels = u.getChannels(con)
    for i, v in ipairs(channels) do
        partChannel(v, con, true)
    end
    s.broadcast("FLN", { character = name })
    s.logMessage("disconnect", con, nil, nil, nil)
    local found, chan = c.getChannel("adh-uberawesomestaffroom")
    if found == true then
        c.removeInvite(chan, string.lower(name))
    end
end

rtb.RTB =
function(args)
    local account_cons = u.getByAccountID(args.a)
    for i, v in ipairs(account_cons) do
        u.sendRaw(v, "RTB " .. args.c)
    end
    return const.FERR_OK
end

rtb.HLB =
function(args)
    local account_cons = u.getByAccountID(args.a)
    for i, v in ipairs(account_cons) do
        u.setMiscData(v, "hellban", "yes")
    end
    s.broadcastOps("SYS", { message = "Hellban against account id: " .. args['a'] .. " was successful." })
    return const.FERR_OK
end

rtb.KIK =
function(args)
    local account_cons = u.getByAccountID(args.a)
    for i, v in ipairs(account_cons) do
        u.sendError(v, const.FERR_KICKED)
        u.close(v)
    end
    if args.s ~= true then
        s.broadcastOps("SYS", { message = "Chat kick applied against account id: " .. args['a'] .. " was successful." })
    end
    return const.FERR_OK
end

rtb.CDL =
function(args)
    local account_cons = u.getByAccountID(args.a)
    local name = string.lower(args.n)
    for i, v in ipairs(account_cons) do
        local cname = string.lower(u.getName(v))
        if cname == name then
            u.sendError(v, const.FERR_KICKED)
            u.close(v)
        end
    end
    return const.FERR_OK
end

--[[ While this function is called before most other Lua functions,	it is discouraged that you store anything in Lua
		that is not entirely disposable.
--]]
function chat_init()
    const.MSG_MAX = s.getConfigDouble("msg_max")
    const.PRI_MAX = s.getConfigDouble("priv_max")
    const.LRP_MAX = s.getConfigDouble("lfrp_max")
    const.CDS_MAX = s.getConfigDouble("cds_max")
    const.LRP_FLOOD = s.getConfigDouble("lfrp_flood")
    const.MSG_FLOOD = s.getConfigDouble("msg_flood")
    const.KIN_FLOOD = s.getConfigDouble("kinks_flood")
    const.PRO_FLOOD = s.getConfigDouble("profile_flood")
    const.FKS_FLOOD = s.getConfigDouble("find_flood")
    const.SFC_FLOOD = s.getConfigDouble("staffcall_flood")
    const.IGN_FLOOD = s.getConfigDouble("ignore_flood")
    const.STA_FLOOD = s.getConfigDouble("sta_flood")
    const.CHA_FLOOD = 5
    const.ORS_FLOOD = 5
    const.VERSION = s.getConfigString("version")
    const.IP_MAX = s.getConfigDouble("max_per_ip")
    const.MAX_TITLE_LEN = 64.4999
    const.MAX_IGNORES = 300
    const.MAX_CHANNEL_BANS = 300
    const.NO_ICON_CHANNELS = { "frontpage", "sex driven lfrp", "story driven lfrp" }
    if c.getChannel("adh-uberawesomestaffroom") ~= true then
        local chanopchan = c.createSpecialPrivateChannel("ADH-UBERAWESOMESTAFFROOM", "Staff Room")
        c.setDescription(chanopchan, "This room is for website and chat staff.\nPlease take any questions and official discussion to Slack.")
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

    const.typing = {}
    const.typing["clear"] = 1
    const.typing["paused"] = 2
    const.typing["typing"] = 3
end

function string:split(sep)
    local sep, fields = sep or ":", {}
    local pattern = string.format("([^%s]+)", sep)
    self:gsub(pattern, function(c) fields[#fields + 1] = c end)
    return fields
end

function finite(num)
    local inf = 1 / 0
    return num == num and num ~= inf and num ~= -inf
end
