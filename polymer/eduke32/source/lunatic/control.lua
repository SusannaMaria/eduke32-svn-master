-- Game control module for Lunatic.

local require = require
local ffi = require("ffi")
local ffiC = ffi.C
local jit = require("jit")

-- Lua C API functions, this comes from El_PushCFunctions() in lunatic_game.c.
local CF = CF

local bit = require("bit")
local io = require("io")
local math = require("math")
local table = require("table")

local geom = require("geom")
local bcheck = require("bcheck")
local con_lang = require("con_lang")

local byte = require("string").byte
local setmetatable = setmetatable

local assert = assert
local error = error
local ipairs = ipairs
local pairs = pairs
local print = print
local rawget = rawget
local rawset = rawset
local tostring = tostring
local type = type
local unpack = unpack

local format = require("string").format

local actor, player = assert(actor), assert(player)
local dc = require("defs_common")
local cansee, hitscan, neartag = dc.cansee, dc.hitscan, dc.neartag
local inside = dc.inside

local sector, wall, sprite = dc.sector, dc.wall, dc.sprite
local wallsofsect = dc.wallsofsect
local spritesofsect, spritesofstat = dc.spritesofsect, dc.spritesofstat

local OUR_NAME = "_con"
local OUR_REQUIRE_STRING = "local "..OUR_NAME.."=require'con'"


module(...)


local lastid = { action=0, move=0, ai=0 }
local def = {
    action = { NO=ffi.new("con_action_t") },
    move = { NO=ffi.new("con_move_t") },
    ai = { NO=ffi.new("con_ai_t") },
}

local function forbidden() error("newindex forbidden", 2) end

AC = setmetatable({}, { __index=def.action, __newindex=forbidden })
MV = setmetatable({}, { __index=def.move, __newindex=forbidden })
AI = setmetatable({}, { __index=def.ai, __newindex=forbidden })

local function check_name(name, what, errlev)
    if (type(name)~="string" or #name > 63) then
        error("bad argument #1 to "..what..": must be a string of length <= 63", errlev+1)
    end
end

local function action_or_move(what, numargs, tab, name, ...)
    if (lastid[what] <= -(2^31)) then
        error("Too many "..what.."s defined", 3);
    end
    check_name(name, what, 3)

    local args = {...}
    if (#args > numargs) then
        error("Too many arguments passed to "..what, 3)
    end

    for i=1,#args do
        local n = args[i]
        if (type(n)~="number" or not (n >= -32768 and n <= 32767)) then
            error("bad argument #".. i+1 .." to "..what..
                  ": must be numbers in [-32768..32767]", 3)
        end
    end
    -- missing fields are initialized to 0 by ffi.new

    -- Named actions or moves have negative ids so that non-negative ones
    -- can be used as (different) placeholders for all-zero ones.
    lastid[what] = lastid[what]-1

    -- ffi.new takes either for initialization: varargs, a table with numeric
    -- indices, or a table with key-value pairs
    -- See http://luajit.org/ext_ffi_semantics.html#init_table
    tab[name] = ffi.new("const con_"..what.."_t", lastid[what], args)
end

---=== ACTION / MOVE / AI ===---

function action(name, ...)
    bcheck.top_level("action")
    action_or_move("action", 5, def.action, name, ...)
end

function move(name, ...)
    bcheck.top_level("move")
    action_or_move("move", 2, def.move, name, ...)
end

-- Get action or move for an 'ai' definition.
local function get_action_or_move(what, val, argi)
    if (val == nil) then
        return {}  -- ffi.new will init the struct to all zeros
    elseif (type(val)=="string") then
        local am = def[what][val]
        if (am==nil) then
            error("no "..what.." '"..val.."' defined", 3)
        end
        return am
    elseif (ffi.istype("con_"..what.."_t", val)) then
        return val
    elseif (type(val)=="number") then
        if (val==0 or val==1) then
            -- Create an action or move with an ID of 0 or 1 but all other
            -- fields cleared.
            return ffi.new("con_"..what.."_t", val)
        end
    end

    error("bad argument #"..argi.." to ai: must be string or (literal) "..what, 3)
end

function ai(name, action, move, flags)
    bcheck.top_level("ai")

    if (lastid.ai <= -(2^31)) then
        error("Too many AIs defined", 2);
    end
    check_name(name, "ai", 2)

    lastid.ai = lastid.ai-1

    local act = get_action_or_move("action", action, 2)
    local mov = get_action_or_move("move", move, 3)

    if (flags~=nil) then
        if (type(flags)~="number" or not (flags>=0 and flags<=32767)) then
            error("bad argument #4 to ai: must be a number in [0..32767]", 2)
        end
    else
        flags = 0
    end

    def.ai[name] = ffi.new("const con_ai_t", lastid.ai, act, mov, flags)
end


---=== RUNTIME CON FUNCTIONS ===---

local check_sector_idx = bcheck.sector_idx
local check_tile_idx = bcheck.tile_idx
local check_sprite_idx = bcheck.sprite_idx
local check_player_idx = bcheck.player_idx
local check_sound_idx = bcheck.sound_idx


local function krandand(mask)
    return bit.band(ffiC.krand(), mask)
end

local function check_isnumber(...)
    local vals = {...}
    for i=1,#vals do
        assert(type(vals[i])=="number")
    end
end

-- Lunatic's "insertsprite" is a wrapper around the game "A_InsertSprite", not
-- the engine "insertsprite".
--
-- Forms:
--  1. table-call: insertsprite{tilenum, pos, sectnum [, owner [, statnum]] [, key=val...]}
--     valid keys are: owner, statnum, shade, xrepeat, yrepeat, xvel, zvel
--  2. position-call: insertsprite(tilenum, pos, sectnum [, owner [, statnum]])
function insertsprite(tab_or_tilenum, ...)
    local tilenum, pos, sectnum  -- mandatory
    -- optional with defaults:
    local owner, statnum
    local shade, xrepeat, yrepeat, ang, xvel, zvel = 0, 48, 48, 0, 0, 0

    if (type(tab_or_tilenum)=="table") then
        local tab = tab_or_tilenum
        tilenum, pos, sectnum = unpack(tab, 1, 3)
        owner = tab[4] or tab.owner or -1
        statnum = tab[5] or tab.statnum or 0
        shade = tab.shade or shade
        xrepeat = tab.xrepeat or xrepeat
        yrepeat = tab.yrepeat or yrepeat
        ang = tab.ang or ang
        xvel = tab.xvel or xvel
        zvel = tab.zvel or zvel
    else
        tilenum = tab_or_tilenum
        local args = {...}
        pos, sectnum = unpack(args, 1, 2)
        owner = args[3] or -1
        statnum = args[4] or 0
    end

    if (type(sectnum)~="number" or type(tilenum) ~= "number") then
        error("invalid insertsprite call: 'sectnum' and 'tilenum' must be numbers", 2)
    end

    check_tile_idx(tilenum)
    check_sector_idx(sectnum)
    check_isnumber(shade, xrepeat, yrepeat, ang, xvel, zvel, owner)

    if (statnum >= ffiC.MAXSTATUS+0ULL) then
        error("invalid 'statnum' argument to insertsprite: must be a status number (0 .. MAXSTATUS-1)", 2)
    end

    return CF.A_InsertSprite(sectnum, pos.x, pos.y, pos.z, tilenum,
                             shade, xrepeat, yrepeat, ang, xvel, zvel,
                             owner, statnum)
end

-- INTERNAL USE ONLY.
function _addtodelqueue(spritenum)
    check_sprite_idx(spritenum)
    CF.A_AddToDeleteQueue(spritenum)
end

-- This corresponds to the first (spawn from parent sprite) form of A_Spawn().
function spawn(parentspritenum, tilenum, addtodelqueue)
    check_sprite_idx(parentspritenum)
    check_tile_idx(tilenum)

    if (addtodelqueue and ffiC.g_spriteDeleteQueueSize == 0) then
        return -1
    end

    local i = CF.A_Spawn(parentspritenum, tilenum)
    if (addtodelqueue) then
        CF.A_AddToDeleteQueue(i)
    end
    return i
end

-- This is the second A_Spawn() form. INTERNAL USE ONLY.
function _spawnexisting(spritenum)
    check_sprite_idx(spritenum)
    return CF.A_Spawn(-1, spritenum)
end

-- A_SpawnMultiple clone
-- ow: parent sprite number
function _spawnmany(ow, tilenum, n)
    local spr = sprite[ow]

    for i=n,1, -1 do
        local j = insertsprite{ tilenum, spr^(ffiC.krand()%(47*256)), spr.sectnum, ow, 5,
                                shade=-32, xrepeat=8, yrepeat=8, ang=krandand(2047) }
        _spawnexisting(j)
        sprite[j].cstat = krandand(8+4)
    end
end

local int16_st = ffi.typeof "struct { int16_t s; }"

function _shoot(i, tilenum, zvel)
    check_sprite_idx(i)
    check_sector_idx(sprite[i].sectnum)  -- accessed in A_ShootWithZvel
    check_tile_idx(tilenum)

    zvel = zvel and int16_st(zvel).s or 0x80000000  -- SHOOT_HARDCODED_ZVEL

    return CF.A_ShootWithZvel(i, tilenum, zvel)
end

local BADGUY_MASK = bit.bor(con_lang.SFLAG.SFLAG_HARDCODED_BADGUY, con_lang.SFLAG.SFLAG_BADGUY)

function isenemytile(tilenum)
    return (bit.band(ffiC.g_tile[tilenum].flags, BADGUY_MASK)~=0)
end

-- The 'rotatesprite' wrapper used by the CON commands.
function _rotspr(x, y, zoom, ang, tilenum, shade, pal, orientation,
                 alpha, cx1, cy1, cx2, cy2)
    check_tile_idx(tilenum)
    orientation = bit.band(orientation, 4095)  -- ROTATESPRITE_MAX-1

    if (bit.band(orientation, 2048) == 0) then  -- ROTATESPRITE_FULL16
        x = 65536*x
        y = 65536*y
    end

    -- XXX: This is the same as the check in gameexec.c, but ideally we'd want
    -- rotatesprite to accept all coordinates and simply draw nothing if the
    -- tile's bounding rectange is beyond the screen.
    -- XXX: Currently, classic rotatesprite() is not correct with some large
    -- zoom values.
    if (not (x >= -320*65536 and x < 640*65536) or not (y >= -200*65536 and y < 400*65536)) then
        error(format("invalid coordinates (%.03f, %.03f)", x, y), 2)
    end

    ffiC.rotatesprite_(x, y, zoom, ang, tilenum, shade, pal, bit.bor(2,orientation),
                       alpha, cx1, cy1, cx2, cy2)
end

-- The external legacy tile drawing function for Lunatic.
function rotatesprite(x, y, zoom, ang, tilenum, shade, pal, orientation,
                      alpha, cx1, cy1, cx2, cy2)
    -- Disallow <<16 coordinates from Lunatic. They only unnecessarily increase
    -- complexity; you already have more precision in the FP number fraction.
    if (bit.band(orientation, 2048) ~= 0) then
        error('left-shift-by-16 coordinates forbidden', 2)
    end

    return _rotspr(x, y, zoom, ang, tilenum, shade, pal, orientation,
                   alpha, cx1, cy1, cx2, cy2)
end

function _myos(x, y, zoom, tilenum, shade, orientation, pal)
    if (pal==nil) then
        local sect = player[ffiC.screenpeek].cursectnum
        pal = (sect>=0) and sector[sect].floorpal or 0
    end

    ffiC.G_DrawTileGeneric(x, y, zoom, tilenum, shade, orientation, pal)
end

function _inittimer(ticspersec)
    if (not (ticspersec >= 1)) then
        error("ticspersec must be >= 1", 2)
    end
    ffiC.G_InitTimer(ticspersec)
end

function _gettimedate()
    local v = ffi.new("int32_t [8]")
    ffiC.G_GetTimeDate(v)
    return v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7]
end

local rshift = bit.rshift

function rnd(x)
    return (rshift(ffiC.krand(), 8) >= (255-x))
end

-- Legacy operators

function _rand(x)
    return rshift(ffiC.krand()*(x+1), 16)
end

function _displayrand(x)
    return rshift(math.random(0, 32767)*(x+1), 15)
end

function _div(a,b)
    if (b==0) then
        error("divide by zero", 2)
    end
    -- NOTE: don't confuse with math.modf!
    return (a - math.fmod(a,b))/b
end

function _mod(a,b)
    if (b==0) then
        error("mod by zero", 2)
    end
    return (math.fmod(a,b))
end

-- Sect_ToggleInterpolation() clone
function _togglesectinterp(sectnum, doset)
    for w in wallsofsect(sectnum) do
        ffiC.G_ToggleWallInterpolation(w, doset)

        local nw = wall[w].nextwall
        if (nw >= 0) then
            ffiC.G_ToggleWallInterpolation(nw, doset)
            ffiC.G_ToggleWallInterpolation(wall[nw].point2, doset)
        end
    end
end

-- Support for translated CON code: get cached sprite, actor and player structs
-- (-fcache-sap option).
function _getsap(aci, pli)
    return (aci>=0) and sprite[aci], (aci>=0) and actor[aci], (pli>=0) and player[pli]
end

--- player/actor/sprite searching functions ---

local xmath = require("xmath")
local abs = math.abs
local dist, ldist = xmath.dist, xmath.ldist

local function A_FP_ManhattanDist(ps, spr)
    local distvec = ps.pos - spr^(28*256)
    return distvec:touniform():mhlen()
end

-- Returns: player index, distance
-- TODO: MP case
function _findplayer(pli, spritenum)
    return 0, A_FP_ManhattanDist(player[pli], sprite[spritenum])
end

local FN_STATNUMS = {
    [false] = { con_lang.STAT.STAT_ACTOR },
    [true] = {},
}

-- TODO: Python-like range() and xrange()?
for i=0,ffiC.MAXSTATUS-1 do
    FN_STATNUMS[true][i+1] = ffiC.MAXSTATUS-1-i
end

local FN_DISTFUNC = {
    d2 = function(s1, s2, d)
        return (xmath.ldist(s1, s2) < d)
    end,

    d3 = function(s1, s2, d)
        return (xmath.dist(s1, s2) < d)
    end,

    z = function(s1, s2, d, zd)
        return (xmath.ldist(s1, s2) < d and abs(s1.z-s2.z) < zd)
    end,
}

function _findnear(spritenum, allspritesp, distkind, picnum, maxdist, maxzdist)
    local statnums = FN_STATNUMS[allspritesp]
    local distfunc = FN_DISTFUNC[distkind]
    local spr = sprite[spritenum]

    for _,st in ipairs(statnums) do
        for i in spritesofstat(st) do
            if (i ~= spritenum and sprite[i].picnum==picnum) then
                if (distfunc(spr, sprite[i], maxdist, maxzdist)) then
                    return i
                end
            end
        end
    end

    return -1
end


---=== Weapon stuff ===---


--- Helper functions (might be exported later) ---

local function have_ammo_at_max(ps, weap)
    return (ps.ammo_amount[weap] >= ps.max_ammo_amount[weap])
end

local function P_AddAmmo(ps, weap, amount)
    if (not have_ammo_at_max(ps, weap)) then
        local curamount = ps.ammo_amount[weap]
        local maxamount = ps.max_ammo_amount[weap]
        -- NOTE: no clamping towards the bottom
        ps.ammo_amount[weap] = math.min(curamount+amount, maxamount)
    end
end

local function P_AddWeaponAmmoCommon(ps, weap, amount)
    P_AddAmmo(ps, weap, amount)

    if (ps.curr_weapon==ffiC.KNEE_WEAPON and ps:have_weapon(weap)) then
        CF.P_AddWeaponMaybeSwitchI(ps.weapon._p, weap);
    end
end


--- Functions that must be exported because they are used by LunaCON generated code,
--- but which are off limits to users.  (That is, we need to think about how to
--- expose the functionality in a better fashion than merely giving access to
--- the C functions.)

-- quotes
local REALMAXQUOTES = con_lang.REALMAXQUOTES
local MAXQUOTELEN = con_lang.MAXQUOTELEN

-- CON redefinequote command
function _definequote(qnum, quotestr)
    -- NOTE: this is more permissive than C-CON: we allow to redefine quotes
    -- that were not previously defined.
    bcheck.quote_idx(qnum, true)
    assert(type(quotestr)=="string")
    ffiC.C_DefineQuote(qnum, quotestr)
    return (#quotestr >= MAXQUOTELEN)
end

function _quote(pli, qnum)
    bcheck.quote_idx(qnum)
    check_player_idx(pli)
    ffiC.P_DoQuote(qnum+REALMAXQUOTES, ffiC.g_player[pli].ps)
end

function _echo(qnum)
    local cstr = bcheck.quote_idx(qnum)
    ffiC.OSD_Printf("%s\n", cstr)
end

function _userquote(qnum)
    local cstr = bcheck.quote_idx(qnum)
    -- NOTE: G_AddUserQuote strcpy's the string
    ffiC.G_AddUserQuote(cstr)
end

local function strlen(cstr)
    for i=0,math.huge do
        if (cstr[i]==0) then
            return i
        end
    end
    assert(false)
end

-- NOTE: dst==src is OK (effectively a no-op)
local function strcpy(dst, src)
    local i=-1
    repeat
        i = i+1
        dst[i] = src[i]
    until (src[i]==0)
end

function _qstrlen(qnum)
    return strlen(bcheck.quote_idx(qnum))
end

function _qstrcpy(qdst, qsrc)
    local cstr_dst = bcheck.quote_idx(qdst)
    local cstr_src = bcheck.quote_idx(qsrc)
    strcpy(cstr_dst, cstr_src)
end

-- NOTE: qdst==qsrc is OK (duplicates the quote)
function _qstrcat(qdst, qsrc)
    local cstr_dst = bcheck.quote_idx(qdst)
    local cstr_src = bcheck.quote_idx(qsrc)

    if (cstr_src[0]==0) then
        return
    end

    if (cstr_dst[0]==0) then
        return strcpy(cstr_dst, cstr_src)
    end

    -- From here on: destination and source quote (potentially aliased) are
    -- nonempty.

    local slen_dst = strlen(cstr_dst)
    assert(slen_dst <= MAXQUOTELEN-1)

    if (slen_dst == MAXQUOTELEN-1) then
        return
    end

    local i = slen_dst
    local j = 0

    repeat
        -- NOTE: don't copy the first char yet, so that the qdst==qsrc case
        -- works correctly.
        i = i+1
        j = j+1
        cstr_dst[i] = cstr_src[j]
    until (i >= MAXQUOTELEN-1 or cstr_src[j]==0)

    -- Now copy the first char!
    cstr_dst[slen_dst] = cstr_src[0]
    cstr_dst[i] = 0
end

local buf = ffi.new("char [?]", MAXQUOTELEN)

function _qsprintf(qdst, qsrc, ...)
    -- NOTE: more permissive than C-CON, see _definequote
    if (bcheck.quote_idx(qdst, true) == nil) then
        ffiC.C_DefineQuote(qdst, "")  -- allocate quote
    end

    local dst = bcheck.quote_idx(qdst)
    local src = bcheck.quote_idx(qsrc)
    local vals = {...}

    local i, j, vi = 0, 0, 1

    while (true) do
        local ch = src[j]
        local didfmt = false

        if (ch==0) then
            break
        end

        if (ch==byte'%') then
            local nch = src[j+1]
            if (nch==byte'd' or (nch==byte'l' and src[j+2]==byte'd')) then
                -- number
                didfmt = true

                if (vi > #vals) then
                    break
                end

                local numstr = tostring(vals[vi])
                assert(type(numstr)=="string")
                vi = vi+1

                local ncopied = math.min(#numstr, MAXQUOTELEN-1-i)
                ffi.copy(buf+i, numstr, ncopied)

                i = i+ncopied
                j = j+1+(nch==byte'd' and 1 or 2)
            elseif (nch==byte's') then
                -- string
                didfmt = true
                if (vi > #vals) then
                    break
                end

                local k = -1
                local tmpsrc = bcheck.quote_idx(vals[vi])
                vi = vi+1

                i = i-1
                repeat
                    i = i+1
                    k = k+1
                    buf[i] = tmpsrc[k]
                until (i >= MAXQUOTELEN-1 or tmpsrc[k]==0)

                j = j+2
            end
        end

        if (not didfmt) then
            buf[i] = src[j]
            i = i+1
            j = j+1
        end

        if (i >= MAXQUOTELEN-1) then
            break
        end
    end

    buf[i] = 0
    strcpy(dst, buf)
end

function _getkeyname(qdst, gfuncnum, which)
    local cstr_dst = bcheck.quote_idx(qdst)

    if (gfuncnum >= ffiC.NUMGAMEFUNCTIONS+0ULL) then
        error("invalid game function number "..gfuncnum, 2)
    end

    if (which >= 3+0ULL) then
        error("third argument to getkeyname must be 0, 1 or 2", 2)
    end

    local cstr_src

    for i = (which==2 and 0 or which), (which==2 and 1 or which) do
        local scancode = ffiC.ud.config.KeyboardKeys[gfuncnum][i]
        cstr_src = ffiC.KB_ScanCodeToString(scancode)
        if (cstr_src[0] ~= 0) then
            break
        end
    end

    if (cstr_src[0] ~= 0) then
        -- All key names are short, no problem strcpy'ing them
        strcpy(cstr_dst, cstr_src)
    end
end

local EDUKE32_VERSION_STR = "EDuke32 2.0.0devel "..ffi.string(ffiC.s_buildRev)

local function quote_strcpy(dst, src)
    local i=-1
    repeat
        i = i+1
        dst[i] = src[i]
    until (src[i]==0 or i==MAXQUOTELEN-1)
    dst[i] = 0
end

function _qgetsysstr(qdst, what, pli)
    local dst = bcheck.quote_idx(qdst)

    local idx = ffiC.ud.volume_number*con_lang.MAXLEVELS + ffiC.ud.level_number
    local MAXIDX = ffi.sizeof(ffiC.MapInfo) / ffi.sizeof(ffiC.MapInfo[0])

    if (what == ffiC.STR_MAPNAME) then
        assert(not (idx >= MAXIDX+0ULL))
        local src = ffiC.MapInfo[idx].name
        assert(src ~= nil)
        quote_strcpy(dst, src)
    elseif (what == ffiC.STR_MAPFILENAME) then
        assert(not (idx >= MAXIDX+0ULL))
        local src = ffiC.MapInfo[idx].filename
        assert(src ~= nil)
        quote_strcpy(dst, src)
    elseif (what == ffiC.STR_PLAYERNAME) then
        ffi.copy(dst, ffiC.g_player[pli].user_name, ffi.sizeof(ffiC.g_player[0].user_name))
    elseif (what == ffiC.STR_VERSION) then
        ffi.copy(dst, EDUKE32_VERSION_STR)
    elseif (what == ffiC.STR_GAMETYPE) then
        ffi.copy(dst, "multiplayer not yet implemented")  -- TODO_MP
    elseif (what == ffiC.STR_VOLUMENAME) then
        ffi.copy(dst, "STR_VOLUMENAME: NYI")
    else
        error("unknown system string ID "..what, 2)
    end
end


-- switch statement support
function _switch(swtab, testval, aci,pli,dist)
    local func = swtab[testval] or swtab.default
    if (func) then
        func(aci, pli, dist)
    end
end


-- text rendering
function _minitext(x, y, qnum, shade, pal)
    local cstr = bcheck.quote_idx(qnum)
    ffiC.minitext_(x, y, cstr, shade, pal, 2+8+16)
end

function _digitalnumber(tilenum, x, y, num, shade, pal,
                        orientation, cx1, cy1, cx2, cy2, zoom)
    if (tilenum >= ffiC.MAXTILES-9+0ULL) then
        error("invalid base tile number "..tilenum, 2)
    end

    ffiC.G_DrawTXDigiNumZ(tilenum, x, y, num, shade, pal,
                          orientation, cx1, cy1, cx2, cy2, zoom)
end

function _gametext(tilenum, x, y, qnum, shade, pal, orientation,
                   cx1, cy1, cx2, cy2, zoom)
    if (tilenum >= ffiC.MAXTILES-255+0ULL) then
        error("invalid base tile number "..tilenum, 2)
    end

    local cstr = bcheck.quote_idx(qnum)

    orientation = bit.band(orientation, 4095)  -- ROTATESPRITE_MAX-1
    ffiC.G_PrintGameText(0, tilenum, bit.arshift(x,1), y, cstr, shade, pal,
                         orientation, cx1, cy1, cx2, cy2, zoom)
end
-- XXX: JIT-compiling FFI calls to G_PrintGameText crashes LuaJIT somewhere in
-- its internal routines.  I'm not sure who is to blame here but I suspect we
-- have some undefined behavior somewhere.  Reproducible with DukePlus 2.35 on
-- x86 when clicking wildly through its menu.
jit.off(_gametext)

local D = {
    -- TODO: dynamic tile remapping
    ACTIVATOR = 2,
    MASTERSWITCH = 8,
    RESPAWN = 9,
    APLAYER = 1405,

    FIRSTAID = 53,
    STEROIDS = 55,
    AIRTANK = 56,
    JETPACK = 57,
    HEATSENSOR = 59,
    BOOTS = 61,
    HOLODUKE = 1348,

    STATUE = 753,
    NAKED1 = 603,
    PODFEM1 = 1294,
    FEM1 = 1312,
    FEM2 = 1317,
    FEM3 = 1321,
    FEM5 = 1323,
    FEM4 = 1325,
    FEM6 = 1334,
    FEM8 = 1336,
    FEM7 = 1395,
    FEM9 = 3450,
    FEM10 = 4864,

    ATOMICHEALTH = 100,
    GLASSPIECES = 1031,
    TRANSPORTERSTAR = 1630,
    COMMANDER = 1920,
    JIBS2 = 2250,
    SCRAP1 = 2400,
    BLIMP = 3400,
}

function _A_DoGuts(i, gutstile, n)
    check_tile_idx(gutstile)
    local spr = sprite[i]
    local smallguts = spr.xrepeat < 16 and spr:isenemy()
    local xsz = smallguts and 8 or 32
    local ysz = xsz
    local z = math.min(spr.z, sector[spr.sectnum]:floorzat(spr)) - 8*256

    if (spr.picnum == D.COMMANDER) then
        z = z - (24*256)
    end

    for i=n,1, -1 do
        local pos = geom.vec3(spr.x+krandand(255)-128, spr.y+krandand(255)-128, z-krandand(8191))
        local j = insertsprite{ gutstile, pos, spr.sectnum, i, 5, shade=-32, xrepeat=xsz, yrepeat=ysz,
                                ang=krandand(2047), xvel=48+krandand(31), zvel=-512-krandand(2047) }
        local newspr = sprite[j]
        if (newspr.picnum==D.JIBS2) then
            -- This looks silly, but EVENT_EGS code could have changed the size
            -- between the insertion and here.
            newspr.xrepeat = newspr.xrepeat/4
            newspr.yrepeat = newspr.yrepeat/4
        end
        newspr.pal = spr.pal
    end
end

function _debris(i, dtile, n)
    local spr = sprite[i]
    if (spr.sectnum >= ffiC.numsectors+0ULL) then
        return
    end

    for j=n-1,0, -1 do
        local isblimpscrap = (spr.picnum==D.BLIMP and dtile==D.SCRAP1)
        local picofs = isblimpscrap and 0 or krandand(3)
        local pos = spr + geom.vec3(krandand(255)-128, krandand(255)-128, -(8*256)-krandand(8191))
        local jj = insertsprite{ dtile+picofs, pos, spr.sectnum, i, 5,
                                 shade=spr.shade, xrepeat=32+krandand(15), yrepeat=32+krandand(15),
                                 ang=krandand(2047), xvel=32+krandand(127), zvel=-krandand(2047) }
        -- NOTE: BlimpSpawnSprites[14] (its array size is 15) will never be chosen
        sprite[jj]:_set_yvel(isblimpscrap and ffiC.BlimpSpawnSprites[math.mod(jj, 14)] or -1)
        sprite[jj].pal = spr.pal
    end
end

function _A_SpawnGlass(i, n)
    local spr = sprite[i]

    for j=n,1, -1 do
        local k = insertsprite{ D.GLASSPIECES+n%3, spr^(256*krandand(16)), spr.sectnum, i, 5,
                                shade=krandand(15), xrepeat=36, yrepeat=36, ang=krandand(2047),
                                xvel=32+krandand(63), zvel=-512-krandand(2047) }
        sprite[k].pal = spr.pal
    end
end

function _A_Shoot(i, atwith)
    check_sprite_idx(i)
    check_tile_idx(atwith)
    return CF.A_ShootWithZvel(i, atwith, 0x80000000)  -- SHOOT_HARDCODED_ZVEL
end

function _A_IncurDamage(sn)
    check_sprite_idx(sn)
    return ffiC.A_IncurDamage(sn)
end

function _VM_FallSprite(i)
    check_sprite_idx(i)
    CF.VM_FallSprite(i)
end

function _sizeto(i, xr, yr)
    local spr = sprite[i]
    local dr = (xr-spr.xrepeat)
    -- NOTE: could "overflow" (e.g. goal repeat is 256, gets converted to 0)
    spr.xrepeat = spr.xrepeat + ((dr == 0) and 0 or (dr < 0 and -1 or 1))
    -- TODO: y stretching is conditional
    dr = (yr-spr.yrepeat)
    spr.yrepeat = spr.yrepeat + ((dr == 0) and 0 or (dr < 0 and -1 or 1))
end

-- NOTE: function args of the C function have overloaded meaning
function _A_Spawn(j, pn)
    local bound_check = sector[sprite[j].sectnum]  -- two in one whack
    check_tile_idx(pn)
    return CF.A_Spawn(j, pn)
end

function _pstomp(ps, i)
    if (ps.knee_incs == 0 and sprite[ps.i].xrepeat >= 40) then
        local spr = sprite[i]
        if (cansee(spr^(4*256), spr.sectnum, ps.pos^(-16*256), sprite[ps.i].sectnum)) then
            for j=ffiC.playerswhenstarted-1,0 do
                if (player[j].actorsqu == i) then
                    return
                end
            end
            ps.actorsqu = i
            ps.knee_incs = 1
            if (ps.weapon_pos == 0) then
                ps.weapon_pos = -1
            end
        end
    end
end

function _pkick(ps, spr)
    -- TODO: multiplayer branch
    if (spr.picnum~=D.APLAYER and ps.quick_kick==0) then
        ps.quick_kick = 14
    end
end

function _VM_ResetPlayer2(snum)
    check_player_idx(snum)
    return (CF.VM_ResetPlayer2(snum)~=0)
end

local PALBITS = { [0]=1, [21]=2, [23]=4 }
local ICONS = {
    [ffiC.GET_FIRSTAID] = 1,  -- ICON_FIRSTAID
    [ffiC.GET_STEROIDS] = 2,
    [ffiC.GET_HOLODUKE] = 3,
    [ffiC.GET_JETPACK] = 4,
    [ffiC.GET_HEATS] = 5,
    [ffiC.GET_SCUBA] = 6,
    [ffiC.GET_BOOTS] = 7,
}

function _addinventory(ps, inv, amount, i)
    if (inv == ffiC.GET_ACCESS) then
        local pal = sprite[i].pal
        if (PALBITS[pal]) then
            ps.got_access = bit.bor(ps.got_access, PALBITS[pal])
        end
    else
        if (ICONS[inv]) then
            ps.inven_icon = ICONS[inv]
        end

        if (inv == ffiC.GET_SHIELD) then
            amount = math.min(ps.max_shield_amount, amount)
        end
        -- NOTE: this is more permissive than CON, e.g. allows
        -- GET_DUMMY1 too.
        ps.inv_amount[inv] = amount
    end
end

function _checkpinventory(ps, inv, amount, i)
    if (inv==ffiC.GET_SHIELD) then
        return ps.inv_amount[inv] ~= ps.max_shield_amount
    elseif (inv==ffiC.GET_ACCESS) then
        local palbit = PALBITS[sprite[i].pal]
        return palbit and (bit.band(ps.got_access, palbit)~=0)
    else
        return ps.inv_amount[inv] ~= amount
    end
end

local INV_SELECTION_ORDER = {
    ffiC.GET_FIRSTAID,
    ffiC.GET_STEROIDS,
    ffiC.GET_JETPACK,
    ffiC.GET_HOLODUKE,
    ffiC.GET_HEATS,
    ffiC.GET_SCUBA,
    ffiC.GET_BOOTS,
}

-- checkavailinven CON command
function _selectnextinv(ps)
    for _,inv in ipairs(INV_SELECTION_ORDER) do
        if (ps.inv_amount[inv] > 0) then
            ps.inven_icon = ICONS[inv]
            return
        end
    end

    ps.inven_icon = 0
end

function _checkavailweapon(pli)
    check_player_idx(pli)
    CF.P_CheckWeaponI(pli)
end

function _addphealth(ps, aci, hlthadd)
    if (ps.newowner >= 0) then
        ffiC.G_ClearCameraView(ps)
    end

    if (ffiC.ud.god ~= 0) then
        return
    end

    local notatomic = (sprite[aci].picnum ~= D.ATOMICHEALTH)
    local j = sprite[ps.i].extra

    if (notatomic and j > ps.max_player_health and hlthadd > 0) then
        return
    end

    if (j > 0) then
        j = j + hlthadd
    end

    if (notatomic) then
        if (hlthadd > 0) then
            j = math.min(j, ps.max_player_health)
        end
    else
        j = math.min(j, 2*ps.max_player_health)
    end

    j = math.max(j, 0)

    if (hlthadd > 0) then
        local qmaxhlth = bit.rshift(ps.max_player_health, 2)
        if (j-hlthadd < qmaxhlth and j >= qmaxhlth) then
            -- XXX: DUKE_GOTHEALTHATLOW
            _sound(aci, 229)
        end

        ps.last_extra = j
    end

    sprite[ps.i].extra = j
end

-- The return value is true iff the ammo was at the weapon's max.
-- In that case, no action is taken.
function _addammo(ps, weap, amount)
    return have_ammo_at_max(ps, weap) or P_AddWeaponAmmoCommon(ps, weap, amount)
end

function _addweapon(ps, weap, amount)
    if (weap >= ffiC.MAX_WEAPONS+0ULL) then
        error("Invalid weapon ID "..weap, 2)
    end

    if (not ps:have_weapon(weap)) then
        CF.P_AddWeaponMaybeSwitchI(ps.weapon._p, weap);
    elseif (have_ammo_at_max(ps, weap)) then
        return true
    end

    P_AddWeaponAmmoCommon(ps, weap, amount)
end

function _A_RadiusDamage(i, r, hp1, hp2, hp3, hp4)
    check_sprite_idx(i)
    check_isnumber(r, hp1, hp2, hp3, hp4)
    CF.A_RadiusDamage(i, r, hp1, hp2, hp3, hp4)
end

function _testkey(pli, synckey)
    local bound_check = player[pli]
    if (synckey >= 32ULL) then
        error("Invalid argument #2 to _testkey: must be in [0..31]", 2)
    end
    local bits = ffiC.g_player[pli].sync.bits
    return (bit.band(bits, bit.lshift(1,synckey)) ~= 0)
end

function _operate(spritenum)
    local NEAROP = {
        [9] = true,
        [15] = true,
        [16] = true,
        [17] = true,
        [18] = true,
        [19] = true,
        [20] = true,
        [21] = true,
        [22] = true,
        [23] = true,
        [25] = true,
        [26] = true,
        [29] = true,
    }

    local spr = sprite[spritenum]

    if (sector[spr.sectnum].lotag == 0) then
        local tag = neartag(spr^(32*256), spr.sectnum, spr.ang, 768, 4+1)
        if (tag.sector >= 0) then
            local sect = sector[tag.sector]
            local lotag = sect.lotag
            if (NEAROP[bit.band(lotag, 0xff)]) then
                if (lotag==23 or sect.floorz==sect.ceilingz) then
                    if (bit.band(lotag, 32768+16384) == 0) then
                        for j in spritesofsect(tag.sector) do
                            if (sprite[j].picnum==D.ACTIVATOR) then
                                return
                            end
                        end
                        CF.G_OperateSectors(tag.sector, spritenum)
                    end
                end
            end
        end
    end
end

function _operatesectors(sectnum, spritenum)
    check_sector_idx(sectnum)
    check_sprite_idx(spritenum)  -- XXX: -1 permissible under certain circumstances?
    CF.G_OperateSectors(sectnum, spritenum)
end

function _operateactivators(tag, playernum)
    check_player_idx(playernum)
    -- NOTE: passing oob playernum would be safe because G_OperateActivators
    -- bound-checks it
    assert(type(tag)=="number")
    CF.G_OperateActivators(tag, playernum)
end

function _activatebysector(sectnum, spritenum)
    local didit = false
    for i in spriteofsect(sectnum) do
        if (sprite[i].picnum==D.ACTIVATOR) then
            CF.G_OperateActivators(sprite[i].lotag, -1)
        end
    end
    if (didit) then
        _operatesectors(sectnum, spritenum)
    end
end

function _checkactivatormotion(tag)
    return ffiC.G_CheckActivatorMotion(tag)
end

function _endofgame(pli, timebeforeexit)
    player[pli].timebeforeexit = timebeforeexit
    player[pli].customexitsound = -1
    ffiC.ud.eog = 1
end

function _bulletnear(i)
    return (ffiC.A_Dodge(sprite[i]) == 1)
end

-- d is a distance
function _awayfromwall(spr, d)
    local vec2 = geom.vec2
    local vecs = { vec2(d,d), vec2(-d,-d), vec2(d,-d), vec2(-d,d) }
    for i=1,4 do
        if (not inside(vecs[i]+spr, spr.sectnum)) then
            return false
        end
    end
    return true
end

local function cossinb(bang)
    return xmath.cosb(bang), xmath.sinb(bang)
end

local function manhatdist(v1, v2)
    return abs(v1.x-v2.x) + abs(v1.y-v2.y)
end

-- "otherspr" is either player or holoduke sprite
local function A_FurthestVisiblePoint(aci, otherspr)
    if (bit.band(actor[aci]:get_t_data(0), 63) ~= 0) then
        return
    end

    local angincs = (ffiC.ud.player_skill < 3) and 1024 or 2048/(1+krandand(1))

    local j = 0
    repeat
        local c, s = cossinb(otherspr.ang + j)
        local hit = hitscan(otherspr^(16*256), otherspr.sectnum,
                            c, s, 16384-krandand(32767), ffiC.CLIPMASK1)
        local dother = manhatdist(hit.pos, otherspr)
        local dactor = manhatdist(hit.pos, sprite[aci])

        if (dother < dactor and hit.sect >= 0) then
            if (cansee(hit.pos, hit.sect, otherspr^(16*256), otherspr.sectnum)) then
                return hit
            end
        end

        j = j + (angincs - krandand(511))
    until (j >= 2048)
end

local MAXSLEEPDIST = 16384
local SLEEPTIME = 1536

function _cansee(aci, ps)
    -- Select sprite for monster to target.
    local spr = sprite[aci]
    local s = sprite[ps.i]

    -- This is kind of redundant, but points the error messages to the CON code.
    check_sector_idx(spr.sectnum)
    check_sector_idx(s.sectnum)

    if (ps.holoduke_on >= 0) then
        -- If holoduke is on, let them target holoduke first.
        local hs = sprite[ps.holoduke_on]

        if (cansee(spr^krandand(8191), spr.sectnum, s, s.sectnum)) then
            s = hs
        end
    end

    -- Can they see player (or player's holoduke)?
    local can = cansee(spr^krandand(47*256), spr.sectnum, s^(24*256), s.sectnum)

    if (not can) then
        -- Search around for target player.
        local hit = A_FurthestVisiblePoint(aci, s)
        if (hit ~= nil) then
            can = true
            actor[aci].lastvx = hit.pos.x
            actor[aci].lastvy = hit.pos.y
        end
    else
        -- Else, they did see it. Save where we were looking...
        actor[aci].lastvx = s.x
        actor[aci].lastvy = s.y
    end

    if (can and (spr.statnum==ffiC.STAT_ACTOR or spr.statnum==ffiC.STAT_STANDABLE)) then
        actor[aci].timetosleep = SLEEPTIME
    end

    return can
end

function _canseespr(s1, s2)
    local spr1, spr2 = sprite[s1], sprite[s2]
    -- Redundant, but points the error messages to the CON code:
    check_sector_idx(spr1.sectnum)
    check_sector_idx(spr2.sectnum)
    return cansee(spr1, spr1.sectnum, spr2, spr2.sectnum) and 1 or 0
end

-- TODO: replace ivec3 allocations with stores to a static ivec3, like in
-- updatesector*?

-- CON "hitscan" command
function _hitscan(x, y, z, sectnum, vx, vy, vz, cliptype)
    local srcv = geom.ivec3(x, y, z)
    local hit = hitscan(srcv, sectnum, vx, vy, vz, cliptype)
    return hit.sect, hit.wall, hit.sprite, hit.pos.x, hit.pos.y, hit.pos.z
end

-- CON "neartag" command
function _neartag(x, y, z, sectnum, ang, range, tagsearch)
    local pos = geom.ivec3(x, y, z)
    local near = neartag(pos, sectnum, ang, range, tagsearch)
    return near.sector, near.wall, near.sprite, near.dist
end

-- CON "getzrange" command
function _getzrange(x, y, z, sectnum, walldist, clipmask)
    check_sector_idx(sectnum)
    local ipos = geom.ivec3(x, y, z)
    local hit = sector[sectnum]:zrangeat(ipos, walldist, clipmask)
    -- return: ceilz, ceilhit, florz, florhit
    return hit.c.z, hit.c.num + (hit.c.spritep and 49152 or 16384),
           hit.f.z, hit.f.num + (hit.f.spritep and 49152 or 16384)
end

-- CON "clipmove" and "clipmovenoslide" commands
function _clipmovex(x, y, z, sectnum, xv, yv, wd, cd, fd, clipmask, noslidep)
    check_sector_idx(sectnum)
    local ipos = geom.ivec3(x, y, z)
    local sect = ffi.new("int16_t [1]")
    local ret = ffiC.clipmovex(ipos, sect, xv, yv, wd, cd, fd, clipmask, noslidep)
    -- Return: clipmovex() return value; updated x, y, sectnum
    return ret, ipos.x, ipos.y, sect[0]
end

function _sleepcheck(aci, dist)
    local acs = actor[aci]
    if (dist > MAXSLEEPDIST and acs.timetosleep == 0) then
        acs.timetosleep = SLEEPTIME
    end
end

function _canseetarget(spr, ps)
    -- NOTE: &41 ?
    return cansee(spr^(256*krandand(41)), spr.sectnum,
                  ps.pos, sprite[ps.i].sectnum)
end

function _movesprite(spritenum, x, y, z, cliptype)
    check_sprite_idx(spritenum)
    local vel = geom.ivec3(x, y, z)
    return ffiC.A_MoveSprite(spritenum, vel, cliptype)
end

-- NOTE: returns two args (in C version, hit sprite is a pointer input arg)
local function A_CheckHitSprite(spr, angadd)
    local zoff = (spr:isenemy() and 42*256) or (spr.picnum==D.APLAYER and 39*256) or 0

    local c, s = cossinb(spr.ang+angadd)
    local hit = hitscan(spr^zoff, spr.sectnum, c, s, 0, ffiC.CLIPMASK1)
    if (hit.wall >= 0 and wall[hit.wall]:ismasked() and spr:isenemy()) then
        return -1, nil
    end

    local dx = hit.pos.x-spr.x
    local dy = hit.pos.y-spr.y
    return hit.sprite, math.sqrt(dx*dx+dy*dy)  -- TODO: use "ldist" approximation for authenticity
end

function _canshoottarget(dist, aci)
    if (dist > 1024) then
        local spr = sprite[aci]

        local hitspr, hitdist = A_CheckHitSprite(spr, 0)
        if (hitdist == nil) then
            return true
        end

        local bigenemy = (spr:isenemy() and spr.xrepeat > 56)

        local sclip = bigenemy and 3084 or 768
        local angdif = bigenemy and 48 or 16

        local sclips = { sclip, sclip, 768 }
        local angdifs = { 0, angdif, -angdif }

        for i=1,3 do
            if (i > 1) then
                hitspr, hitdist = A_CheckHitSprite(spr, angdifs[i])
            end

            if (hitspr >= 0 and sprite[hitspr].picnum == spr.picnum) then
                if (hitdist > sclips[i]) then
                    return false
                end
            end
        end
    end

    return true
end

function _getlastpal(spritenum)
    local spr = sprite[spritenum]
    if (spr.picnum == D.APLAYER) then
        spr.pal = player[spr.yvel].palookup
    else
        if (spr.pal == 1 and spr.extra == 0) then  -- hack for frozen
            spr.extra = spr.extra+1
        end
        spr.pal = actor[spritenum].tempang
    end
    actor[spritenum].tempang = 0
end

-- G_GetAngleDelta(a1, a2)
function _angdiff(a1, a2)
    a1 = bit.band(a1, 2047)
    a2 = bit.band(a2, 2047)
    -- a1 and a2 are in [0, 2047]
    if (abs(a2-a1) < 1024) then
        return abs(a2-a1)
    end
    -- |a2-a1| >= 1024
    if (a2 > 1024) then a2=a2-2048 end
    if (a1 > 1024) then a1=a1-2048 end
    -- a1 and a2 is in [-1023, 1024]
    return a2-a1
end

function _angdiffabs(a1, a2)
    return abs(_angdiff(a1, a2))
end

function _angtotarget(aci)
    local spr = sprite[aci]
    return ffiC.getangle(actor[aci].lastvx-spr.x, actor[aci].lastvy-spr.y)
end

function _hypot(a, b)
    return math.sqrt(a*a + b*b)
end

function _rotatepoint(pivotx, pivoty, posx, posy, ang)
    local pos = geom.ivec3(posx, posy)
    local pivot = geom.ivec3(pivotx, pivoty)
    pos = xmath.rotate(pos, pivot, ang):toivec3()
    return pos.x, pos.y
end

local SK = {
    CROUCH = 1,
    RUN = 5,
}

function _ifp(flags, pli, aci)
    local l = flags
    local ps = player[pli]
    local vel = sprite[ps.i].xvel
    local band = bit.band

    if (band(l,8)~=0 and ps.on_ground and _testkey(pli, SK.CROUCH)) then
        return true
    elseif (band(l,16)~=0 and ps.jumping_counter == 0 and not ps.on_ground and ps.vel.z > 2048) then
        return true
    elseif (band(l,32)~=0 and ps.jumping_counter > 348) then
        return true
    elseif (band(l,1)~=0 and vel >= 0 and vel < 8) then
        return true
    elseif (band(l,2)~=0 and vel >= 8 and not _testkey(pli, SK.RUN)) then
        return true
    elseif (band(l,4)~=0 and vel >= 8 and _testkey(pli, SK.RUN)) then
        return true
    elseif (band(l,64)~=0 and ps.pos.z < (sprite[aci].z-(48*256))) then
        return true
    elseif (band(l,128)~=0 and vel <= -8 and not _testkey(pli, SK.RUN)) then
        return true
    elseif (band(l,256)~=0 and vel <= -8 and _testkey(pli, SK.RUN)) then
        return true
    elseif (band(l,512)~=0 and (ps.quick_kick > 0 or (ps.curr_weapon == ffiC.KNEE_WEAPON and ps.kickback_pic > 0))) then
        return true
    elseif (band(l,1024)~=0 and sprite[ps.i].xrepeat < 32) then
        return true
    elseif (band(l,2048)~=0 and ps.jetpack_on) then
        return true
    elseif (band(l,4096)~=0 and ps.inv_amount.STEROIDS > 0 and ps.inv_amount.STEROIDS < 400) then
        return true
    elseif (band(l,8192)~=0 and ps.on_ground) then
        return true
    elseif (band(l,16384)~=0 and sprite[ps.i].xrepeat > 32 and sprite[ps.i].extra > 0 and ps.timebeforeexit == 0) then
        return true
    elseif (band(l,32768)~=0 and sprite[ps.i].extra <= 0) then
        return true
    elseif (band(l,65536)~=0) then
        -- TODO: multiplayer branch
        if (_angdiffabs(ps.ang, ffiC.getangle(sprite[aci].x-ps.pos.x, sprite[aci].y-ps.pos.y)) < 128) then
            return true
        end
    end

    return false
end

function _squished(aci, pli)
    check_sprite_idx(aci)
    check_player_idx(pli)
    check_sector_idx(sprite[aci].sectnum)

    return (ffiC.VM_CheckSquished2(aci, pli)~=0)
end

function _checkspace(sectnum, floorp)
    local sect = sector[sectnum]
    local picnum = floorp and sect.floorpicnum or sect.ceilingpicnum
    local stat = floorp and sect.floorstat or sect.ceilingstat
    return bit.band(stat,1)~=0 and sect.ceilingpal == 0 and
        (picnum==D.MOONSKY1 or picnum==D.BIGORBIT1)
end

function _flash(spr, ps)
   spr.shade = -127
   ps.visibility = -127  -- XXX
   ffiC.lastvisinc = ffiC.totalclock+32
end

function _G_OperateRespawns(tag)
    for i in spritesofstat(ffiC.STAT_FX) do
        local spr = sprite[i]

        if (spr.lotag==tag and spr.picnum==D.RESPAWN) then
            if (ffiC.ud.monsters_off~=0 and isenemytile(spr.hitag)) then
                return
            end

            local j = spawn(i, D.TRANSPORTERSTAR)
            sprite[j].z = sprite[j].z - (32*256)

            -- Just a way to killit (see G_MoveFX(): RESPAWN__STATIC)
            spr.extra = 66-12
        end
    end
end

function _G_OperateMasterSwitches(tag)
    for i in spritesofstat(ffiC.STAT_STANDABLE) do
        local spr = sprite[i]
        if (spr.picnum==D.MASTERSWITCH and spr.lotag==tag and spr.yvel==0) then
            spr:_set_yvel(1)
        end
    end
end

local RESPAWN_USE_YVEL =
{
    [D.STATUE] = true,
    [D.NAKED1] = true,
    [D.PODFEM1] = true,
    [D.FEM1] = true,
    [D.FEM2] = true,
    [D.FEM3] = true,
    [D.FEM5] = true,
    [D.FEM4] = true,
    [D.FEM6] = true,
    [D.FEM8] = true,
    [D.FEM7] = true,
    [D.FEM9] = true,
    [D.FEM10] = true,
}

function _respawnhitag(spr)
    if (RESPAWN_USE_YVEL[spr.picnum]) then
        if (spr.yvel ~= 0) then
            _G_OperateRespawns(spr.yvel)
        end
    else
        _G_OperateRespawns(spr.hitag)
    end
end

local INVENTILE = {
    [D.FIRSTAID] = true,
    [D.STEROIDS] = true,
    [D.AIRTANK] = true,
    [D.JETPACK] = true,
    [D.HEATSENSOR] = true,
    [D.BOOTS] = true,
    [D.HOLODUKE] = true,
}

function _checkrespawn(spr)
    if (spr:isenemy()) then
        return (ffiC.ud.respawn_monsters~=0)
    end
    if (INVENTILE[spr.picnum]) then
        return (ffiC.ud.respawn_inventory~=0)
    end
    return (ffiC.ud.respawn_items~=0)
end

-- SOUNDS
function _ianysound(aci)
    check_sprite_idx(aci)
    return (ffiC.A_CheckAnySoundPlaying(aci)~=0)
end

function _sound(aci, sndidx)
    check_sprite_idx(aci)
    -- A_PlaySound() returns early if the sound index is oob, but IMO it's good
    -- style to throw an error instead of silently failing.
    check_sound_idx(sndidx)
    CF.A_PlaySound(sndidx, aci)
end

function _globalsound(pli, sndidx)
    -- TODO: conditional on coop, fake multimode
    if (pli==ffiC.screenpeek) then
        _sound(player[pli].i, sndidx)
    end
end

-- This is a macro for EDuke32 (game.h)
local function S_StopSound(sndidx)
    ffiC.S_StopEnvSound(sndidx, -1)
end

function _soundplaying(aci, sndidx)
    check_sprite_idx(aci)
    check_sound_idx(sndidx)
    return (ffiC.S_CheckSoundPlaying(aci, sndidx) ~= 0)
end

function _stopsound(aci, sndidx)
    -- XXX: This is weird: the checking is done wrt a sprite, but the sound not.
    -- NOTE: S_StopSound() stops sound <sndidx> that started playing most recently.
    if (_soundplaying(aci, sndidx)) then
        S_StopSound(sndidx)
    end
end

function _stopactorsound(aci, sndidx)
    if (_soundplaying(aci, sndidx)) then
        ffiC.S_StopEnvSound(sndidx, aci)
    end
end

function _soundonce(aci, sndidx)
    if (not _soundplaying(aci, sndidx)) then
        _sound(aci, sndidx)
    end
end

function _stopallsounds(pli)
    if (ffiC.screenpeek==pli) then
        ffiC.FX_StopAllSounds()
    end
end

function _setactorsoundpitch(aci, sndidx, pitchoffset)
    check_sprite_idx(aci)
    check_sound_idx(sndidx)
    ffiC.S_ChangeSoundPitch(sndidx, aci, pitchoffset)
end

function _starttrack(level)
    bcheck.level_idx(level)

    if (ffiC.G_StartTrack(level) ~= 0) then
        error("null music for volume "..ffiC.ud.volume_number..
              " level "..level, 2)
    end
end

function _startlevel(volume, level)
    bcheck.volume_idx(volume)
    bcheck.level_idx(level)

    ffiC.ud.m_volume_number = volume
    ffiC.ud.volume_number = volume
    ffiC.ud.m_level_number = level
    ffiC.ud.level_number = level

    ffiC.ud.display_bonus_screen = 0

    -- TODO_MP
    player[0].gm = bit.bor(player[0].gm, 0x00000008)  -- MODE_EOL
end

function _setaspect(viewingrange, yxaspect)
    if (viewingrange==0) then
        error('invalid argument #1: must be nonzero', 2)
    end
    if (yxaspect==0) then
        error('invalid argument #2: must be nonzero', 2)
    end

    -- XXX: surely not all values are sane
    ffiC.setaspect(viewingrange, yxaspect)
end

function _setgamepalette(pli, basepal)
    ffiC.P_SetGamePalette(player[pli], basepal, 2+16)
end

-- Map state persistence.
-- TODO: saving/restoration of per-player or per-actor gamevars.
function _savemapstate()
    ffiC.G_SaveMapState()
end

function _loadmapstate()
    ffiC.G_RestoreMapState()
end

-- Gamevar persistence in the configuration file

function _savegamevar(name, val)
    if (ffiC.ud.config.scripthandle < 0) then
        return
    end

    assert(type(name)=="string")
    assert(type(val)=="number")

    ffiC.SCRIPT_PutNumber(ffiC.ud.config.scripthandle, "Gamevars", name,
                          val, 0, 0);
end

function _readgamevar(name)
    if (ffiC.ud.config.scripthandle < 0) then
        return
    end

    assert(type(name)=="string")

    local v = ffi.new("int32_t [1]")
    ffiC.SCRIPT_GetNumber(ffiC.ud.config.scripthandle, "Gamevars", name, v);
    -- NOTE: doesn't examine SCRIPT_GetNumber() return value and returns 0 if
    -- there was no such gamevar saved, like C-CON.
    return v[0]
end


--- Wrapper of kopen4load file functions in a Lua-like file API
-- TODO: move to common side?

local kfile_mt = {
    __gc = function(self)
        self:close()
    end,

    __index = {
        close = function(self)
            if (self.fd > 0) then
                ffiC.kclose(self.fd)
                self.fd = -1
            end
        end,

        seek = function(self, whence, offset)
            local w = whence=="set" and 0  -- SEEK_SET
                or whence=="end" and 2  -- SEEK_END
                or error("invalid 'whence' for seek", 2)  -- "cur" NYI

            local pos = ffiC.klseek(self.fd, offset or 0, w)

            if (pos >= 0) then
                return pos
            else
                return nil, "?"
            end
        end,

        read = function(self, nbytes)
            assert(type(nbytes)=="number")  -- other formats NYI
            assert(nbytes > 0)

            local bytes = ffi.new("char [?]", nbytes)
            local bytesread = ffiC.kread(self.fd, bytes, nbytes)

            if (bytesread ~= nbytes) then
                return nil
            end

            return ffi.string(bytes, nbytes)
        end,

        -- Read <nints> little-endian 32-bit integers.
        read_le_int32 = function(self, nints)
            local ints = ffi.new("int32_t [?]", nints)
            local bytesread = ffiC.kread(self.fd, ints, nints*4)

            if (bytesread ~= nints*4) then
                return nil
            end

            if (ffi.abi("be")) then
                for i=0,nints-1 do
                    ints[i] = bit.bswap(ints[i])
                end
            end

            return ints
        end,
    },
}

local kfile_t = ffi.metatype("struct { int32_t fd; }", kfile_mt)

local function kopen4load(fn, searchfirst)
    local fd = ffiC.kopen4load(fn, searchfirst)

    if (fd < 0) then
        return nil, "no such file?"
    end

    return kfile_t(fd)
end


local function serialize_value(strtab, i, v)
    -- Save only user values (i.e. not 'meta-fields' like '_size').
    if (type(i)=="number" and v~=nil) then
        strtab[#strtab+1] = "["..i.."]="..tostring(v)..","
    end
end

-- Common serialization function for gamearray and actorvar.
local function serialize_array(ar, strtab, maxnum)
--    if (ffiC._DEBUG_LUNATIC ~= 0) then
        -- Iterate in numeric order. XXX: also for non-debug?
        for i=0,maxnum-1 do
            serialize_value(strtab, i, rawget(ar, i))
        end
--    else
--        for i,v in pairs(ar) do
--            serialize_value(strtab, i, v)
--        end
--    end

    strtab[#strtab+1] = "})"

    return table.concat(strtab)
end

--- Game arrays ---

local function moddir_filename(cstr_fn)
    local fn = ffi.string(cstr_fn)
    local moddir = ffi.string(ffiC.g_modDir);

    if (moddir=="/") then
        return fn
    else
        return format("%s/%s", moddir, fn)
    end
end

local GAR_FOOTER = "\001\002EDuke32GameArray\003\004"
local GAR_FOOTER_SIZE = #GAR_FOOTER

local function gamearray_file_common(qnum, writep)
    local fn = moddir_filename(bcheck.quote_idx(qnum))
    local f, errmsg

    if (writep) then
        f, errmsg = io.open(fn)
        if (f == nil) then
            -- file, numints, isnewgar, filename
            return nil, nil, true, fn
        end
    else
        f, errmsg = kopen4load(fn, 0)
        if (f == nil) then
            if (f==false) then
                error(format([[failed opening "%s" for reading: %s]], fn, errmsg), 3)
            else
                return
            end
        end
    end

    local fsize = assert(f:seek("end"))

    local isnewgar = false
    if (fsize >= GAR_FOOTER_SIZE) then
        assert(f:seek("end", -GAR_FOOTER_SIZE))
        isnewgar = (assert(f:read(GAR_FOOTER_SIZE)) == GAR_FOOTER)
        if (isnewgar) then
            fsize = fsize - GAR_FOOTER_SIZE
        end
    end

    return f, math.floor(fsize/4), isnewgar, fn
end

local function check_gamearray_idx(gar, idx, addstr)
    if (idx >= gar._size+0ULL) then
        addstr = addstr or ""
        error("invalid "..addstr.."array index "..idx, 3)
    end
end

local intbytes_t = ffi.typeof("union { int32_t i; uint8_t b[4]; }")

local gamearray_methods = {
    resize = function(gar, newsize)
        -- NOTE: size 0 is valid (then, no index is valid)
        if (newsize < 0) then
            error("invalid new array size "..newsize, 2)
        end

        local MAXELTS = math.floor(0x7fffffff/4)
        if (newsize > MAXELTS) then
            -- mainly for some sanity with kread() (which we don't use, but still)
            error("new array size "..newsize.." too large (max="..MAXELTS.." elements)", 2)
        end

        -- clear trailing elements in case we're shrinking
        for i=gar._size,newsize-1 do
            rawset(gar, i, nil)
        end

        gar._size = newsize
    end,

    copyto = function(sar, sidx, dar, didx, numelts)
        -- XXX: Strictest bound checking, see later if we need to relax it.
        check_gamearray_idx(sar, sidx, "lower source ")
        check_gamearray_idx(sar, sidx+numelts-1, "upper source ")
        check_gamearray_idx(dar, didx, "lower destination ")
        check_gamearray_idx(dar, didx+numelts-1, "upper destination ")
        for i=0,numelts-1 do
            rawset(dar, didx+i, rawget(sar, sidx+i))
        end
    end,

    read = function(gar, qnum)
        local f, nelts, isnewgar = gamearray_file_common(qnum, false)

        if (f==nil) then
            return
        end

        assert(f:seek("set"))
        local ints = f:read_le_int32(nelts)
        if (ints == nil) then
            error("failed reading whole file into gamearray", 2)
        end

        gar:resize(nelts)

        for i=0,nelts-1 do
            rawset(gar, i, (ints[i]==0) and nil or ints[i])
        end

        f:close()
    end,

    write = function(gar, qnum)
        local f, _, isnewgar, fn = gamearray_file_common(qnum, true)

        if (f ~= nil) then
            f:close()
        end

        if (not isnewgar) then
            error("refusing to overwrite a file not created by a previous `writearraytofile'", 2)
        end

        local f, errmsg = io.open(fn, "w+")
        if (f == nil) then
            error([[failed opening "%s" for writing: %s]], fn, errmsg, 3)
        end

        local nelts = gar._size
        local cstr = ffi.new("uint8_t [?]", 4*nelts)
        local isbe = ffi.abi("be")  -- is big-endian?

        for i=0,nelts-1 do
            local diskval = intbytes_t(isbe and bit.bswap(gar[i]) or gar[i])
            for bi=0,3 do
                cstr[4*i+bi] = diskval.b[bi]
            end
        end

        f:write(ffi.string(cstr, 4*nelts))
        f:write(GAR_FOOTER)

        f:close()
    end,


    --- Internal routines ---

    --  * All values equal to the default one (0) are cleared.
    _cleanup = function(gar)
        for i=0,gar._size-1 do
            if (rawget(gar, i)==0) then
                rawset(gar, i, nil)
            end
        end
    end,


    --- Serialization ---

    _get_require = function(gar)
        return OUR_REQUIRE_STRING
    end,

    _serialize = function(gar)
        local strtab = { OUR_NAME.."._gamearray(", tostring(gar._size), ",{" }
        gar:_cleanup()
        return serialize_array(gar, strtab, gar._size)
    end,
}

local gamearray_mt = {
    __index = function(gar, key)
        if (type(key)=="number") then
            check_gamearray_idx(gar, key)
            return 0
        else
            return gamearray_methods[key]
        end
    end,

    __newindex = function(gar, idx, val)
        check_gamearray_idx(gar, idx)
        rawset(gar, idx, val)
    end,

    __metatable = "serializeable",
}

-- Common constructor helper for gamearray and actorvar.
local function set_values_from_table(ar, values)
    if (values ~= nil) then
        for i,v in pairs(values) do
            ar[i] = v
        end
    end
    return ar
end

-- NOTE: Gamearrays are internal because users are encouraged to use tables
-- from Lua code.
-- <values>: optional, a table of <index>=value
function _gamearray(size, values)
    local gar = setmetatable({ _size=size }, gamearray_mt)
    return set_values_from_table(gar, values)
end


--- More functions of the official API ---

-- Non-local control flow. These ones call the original error(), not our
-- redefinition in defs.ilua.
function longjmp()
    error(false)
end

function killit()
    -- TODO: guard against deletion of player sprite?
    error(true)
end


-- Per-actor variable.
local actorvar_methods = {
    --- Internal routines ---

    --  * All values for sprite not in the game world are cleared.
    --  * All values equal to the default one are cleared.
    _cleanup = function(acv)
        for i=0,ffiC.MAXSPRITES-1 do
            if (ffiC.sprite[i].statnum == ffiC.MAXSTATUS or rawget(acv, i)==acv._defval) then
                rawset(acv, i, nil)
            end
        end
    end,


    --- Serialization ---

    _get_require = function(acv)
        return OUR_REQUIRE_STRING
    end,

    _serialize = function(acv)
        local strtab = { OUR_NAME..".actorvar(", tostring(acv._defval), ",{" }
        -- TODO: Must clean up sometime if not saving, too. (That is, what is
        -- A_ResetVars() in the C-CON build.)
        acv:_cleanup()
        return serialize_array(acv, strtab, ffiC.MAXSPRITES)
    end,
}

-- XXX: How about types other than numbers?
local actorvar_mt = {
    __index = function(acv, idx)
        if (type(idx)=="number") then
            check_sprite_idx(idx)
            return acv._defval
        else
            return actorvar_methods[idx]
        end
    end,

    __newindex = function(acv, idx, val)
        check_sprite_idx(idx)
        rawset(acv, idx, val)
    end,

    __metatable = "serializeable",
}

-- <initval>: default value for per-actor variable.
-- <values>: optional, a table of <spritenum>=value
function actorvar(initval, values)
    local acv = setmetatable({ _defval=initval }, actorvar_mt)
    return set_values_from_table(acv, values)
end
