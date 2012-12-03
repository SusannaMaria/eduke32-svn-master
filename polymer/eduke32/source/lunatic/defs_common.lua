
-- This file contains LuaJIT definitions of stuff that's common to the game and
-- editor.  The 'decl' function is expected to be defined in the global
-- environment.

local ffi = require("ffi")
local ffiC = ffi.C

local string = require("string")

local error = error
local pairs = pairs
local setmetatable = setmetatable

local decl = decl
local getfenv = getfenv

local print=print


module(...)


--== Core engine structs ==--
local SPRITE_STRUCT = [[
{
    int32_t x, y, z;
    uint16_t cstat;
    const int16_t picnum;
    int8_t shade;
    uint8_t pal, clipdist, filler;
    uint8_t xrepeat, yrepeat;
    int8_t xoffset, yoffset;
    const int16_t sectnum, statnum;
    int16_t ang;
    // NOTE: yvel is often used as player index in game code. Make xvel/zvel
    // "const" for consistency, too.
    const int16_t owner, xvel, yvel, zvel;
    int16_t lotag, hitag, extra;
}
]]

-- Converts a template struct definition to an internal, unrestricted one.
function strip_const(structstr)
    return string.gsub(structstr, "const ", "");
end

-- NOTE for FFI definitions: we're compiling EDuke32 with -funsigned-char, so
-- we need to take care to declare chars as unsigned whenever it matters, for
-- example if it represents a palette index.  (I think it's harmless for stuff
-- like passing a function argument, but it should be done there for clarity.)

-- TODO: provide getters for unsigned {hi,lo}tag?
ffi.cdef([[
#pragma pack(push,1)
typedef struct
{
    const int16_t wallptr, wallnum;
    int32_t ceilingz, floorz;
    uint16_t ceilingstat, floorstat;
    const int16_t ceilingpicnum;
    int16_t ceilingheinum;
    int8_t ceilingshade;
    uint8_t ceilingpal, ceilingxpanning, ceilingypanning;
    const int16_t floorpicnum;
    int16_t floorheinum;
    int8_t floorshade;
    uint8_t floorpal, floorxpanning, floorypanning;
    uint8_t visibility, filler;
    int16_t lotag, hitag, extra;
} sectortype;

typedef struct
{
    int32_t x, y;
    const int16_t point2, nextwall, nextsector;
    uint16_t cstat;
    const int16_t picnum, overpicnum;
    int8_t shade;
    uint8_t pal, xrepeat, yrepeat, xpanning, ypanning;
    int16_t lotag, hitag, extra;
} walltype;

typedef struct
]].. SPRITE_STRUCT ..[[
spritetype;

typedef struct
]].. strip_const(SPRITE_STRUCT) ..[[
spritetype_u_t;

typedef struct {
    const uint32_t mdanimtims;
    const int16_t mdanimcur;
    int16_t angoff, pitch, roll;
    int32_t xoff, yoff, zoff;
    uint8_t flags;
    uint8_t xpanning, ypanning;
    const uint8_t filler;
    float alpha;
    const int32_t _do_not_use1;
    const int32_t _do_not_use2;
} spriteext_t;

typedef struct {
    int32_t x, y, z;
} vec3_t;

typedef struct {
    vec3_t pos;
    int16_t sprite, wall, sect;
} hitdata_t;

typedef struct {
    unsigned char r,g,b,f;
} palette_t;
#pragma pack(pop)
]])

local vec3_ct = ffi.typeof("vec3_t")
local hitdata_ct = ffi.typeof("hitdata_t")

ffi.cdef[[const int32_t engine_main_arrays_are_static, engine_v8;]]


--== Engine data and functions ==--


-- NOTE TO SELF: This is not C, never EVER write
--    if (x)
-- when checking a C variable x for 'thuthiness'
if (ffiC.engine_main_arrays_are_static ~= 0) then
    decl[[
    sectortype sector[];
    walltype wall[];
    spritetype sprite[];
    spriteext_t spriteext[];
    ]]
else
    decl[[
    sectortype *sector;
    walltype *wall;
    spritetype *sprite;
    spriteext_t *spriteext;
    ]]
end

if (ffiC.engine_v8 == 0) then
    -- V7
    ffi.cdef[[
enum
{
    MAXSECTORS = 1024,
    MAXWALLS = 8192,
    MAXSPRITES = 4096,
}
]]
else
    -- V8
    ffi.cdef[[
enum
{
    MAXSECTORS = 4096,
    MAXWALLS = 16384,
    MAXSPRITES = 16384,
}
]]
end

ffi.cdef[[
enum {
    MAXSTATUS = 1024,
    MAXTILES = 30720,

    MAXBUNCHES = 256,
    CEILING = 0,
    FLOOR = 1,

    CLIPMASK0 = (1<<16)+1,  // blocking
    CLIPMASK1 = (256<<16)+64,  // hittable
};
]]

ffi.cdef[[
const int16_t numsectors, numwalls;
const int32_t numyaxbunches;
const int32_t totalclock;
const int32_t xdim, ydim;
]]

decl[[
const int16_t headspritesect[MAXSECTORS+1], headspritestat[MAXSTATUS+1];
const int16_t prevspritesect[MAXSPRITES], prevspritestat[MAXSPRITES];
const int16_t nextspritesect[MAXSPRITES], nextspritestat[MAXSPRITES];

const int16_t headsectbunch[2][MAXBUNCHES], nextsectbunch[2][MAXSECTORS];

int16_t yax_getbunch(int16_t i, int16_t cf);

int32_t hitscan(const vec3_t *sv, int16_t sectnum, int32_t vx, int32_t vy, int32_t vz,
                hitdata_t *hitinfo, uint32_t cliptype);
int32_t cansee(int32_t x1, int32_t y1, int32_t z1, int16_t sect1,
               int32_t x2, int32_t y2, int32_t z2, int16_t sect2);

int32_t ldist(const spritetype *s1, const spritetype *s2);
int32_t dist(const spritetype *s1, const spritetype *s2);

void updatesector(int32_t x, int32_t y, int16_t *sectnum);
void updatesectorbreadth(int32_t x, int32_t y, int16_t *sectnum);
void updatesectorz(int32_t x, int32_t y, int32_t z, int16_t *sectnum);

void rotatesprite(int32_t sx, int32_t sy, int32_t z, int16_t a, int16_t picnum,
                  int8_t dashade, unsigned char dapalnum, int32_t dastat,
                  int32_t cx1, int32_t cy1, int32_t cx2, int32_t cy2);
]]

-- misc. functions
ffi.cdef[[
double gethitickms(void);

int32_t krand(void);
int32_t ksqrt(uint32_t num);
]]

local ivec3_
local ivec3_mt = {
    -- '^' is the "translate upwards" operator
    __pow = function(v, zofs)
        return ivec3_(v.x, v.y, v.z-zofs)
    end,
}
ivec3_ = ffi.metatype("vec3_t", ivec3_mt)

local spritetype_ptr_ct = ffi.typeof("spritetype_u_t *")

local spritetype_mt = {
    __pow = function(s, zofs)
        return ivec3_(s.x, s.y, s.z-zofs)
    end,

    __index = {
        set_picnum = function(s, tilenum)
            if (tilenum >= ffiC.MAXTILES+0ULL) then
                error("attempt to set invalid picnum "..tilenum, 2)
            end
            ffi.cast(spritetype_ptr_ct, s).picnum = tilenum
        end
    },
}
ffi.metatype("spritetype", spritetype_mt)


---=== Restricted access to C variables from Lunatic ===---

-- set metatable and forbid setting it further
function setmtonce(tab, mt)
    mt.__metatable = true
    return setmetatable(tab, mt)
end

---- indirect C array access ----
local sector_mt = {
    __index = function(tab, key)
        if (key >= 0 and key < ffiC.numsectors) then return ffiC.sector[key] end
        error('out-of-bounds sector[] read access', 2)
    end,

    __newindex = function(tab, key, val) error('cannot write directly to sector[] struct', 2) end,
}

local wall_mt = {
    __index = function(tab, key)
        if (key >= 0 and key < ffiC.numwalls) then return ffiC.wall[key] end
        error('out-of-bounds wall[] read access', 2)
    end,

    __newindex = function(tab, key, val) error('cannot write directly to wall[] struct', 2) end,
}

-- create a safe indirection for an ffi.C array
function creategtab(ctab, maxidx, name)
    local tab = {}
    local tmpmt = {
        __index = function(tab, key)
            if (key>=0 and key < maxidx) then
                return ctab[key]
            end
            error('out-of-bounds '..name..' read access', 2)
        end,
        __newindex = function(tab, key, val)
            error('cannot write directly to '..name, 2)
        end,
    }

    return setmtonce(tab, tmpmt)
end


local vars_to_ignore = {}
for varname,_ in pairs(getfenv(1)) do
--    print("IGNORE "..varname)
    vars_to_ignore[varname] = true
end

--== ALL GLOBALS FROM HERE ON ARE EXPORTED UPWARDS (see create_globals() below) ==--

sector = setmtonce({}, sector_mt)
wall = setmtonce({}, wall_mt)
sprite = creategtab(ffiC.sprite, ffiC.MAXSPRITES, 'sprite[] struct')
spriteext = creategtab(ffiC.spriteext, ffiC.MAXSPRITES, 'spriteext[] struct')

headspritesect = creategtab(ffiC.headspritesect, ffiC.MAXSECTORS, 'headspritesect[]')
-- TODO: allow sprite freelist access via the status list for CON compatibility?
headspritestat = creategtab(ffiC.headspritestat, ffiC.MAXSTATUS, 'headspritestat[]')
nextspritesect = creategtab(ffiC.nextspritesect, ffiC.MAXSPRITES, 'nextspritesect[]')
nextspritestat = creategtab(ffiC.nextspritestat, ffiC.MAXSPRITES, 'nextspritestat[]')
prevspritesect = creategtab(ffiC.prevspritesect, ffiC.MAXSPRITES, 'prevspritesect[]')
prevspritestat = creategtab(ffiC.prevspritestat, ffiC.MAXSPRITES, 'prevspritestat[]')

--== Per-sector/per-statnum sprite iterators ==--
local function iter_spritesofsect(sect, i)
    if (i < 0) then
        i = ffiC.headspritesect[sect]
    else
        i = ffiC.nextspritesect[i]
    end

    if (i >= 0) then return i end
end

function spritesofsect(sect)
    if (sect < 0 or sect >= ffiC.numsectors) then
        error("passed invalid sectnum to spritesofsect iterator", 2)
    end

    return iter_spritesofsect, sect, -1
end

local function iter_spritesofstat(stat, i)
    if (i < 0) then
        i = ffiC.headspritestat[stat]
    else
        i = ffiC.nextspritestat[i]
    end

    if (i >= 0) then return i end
end

function spritesofstat(stat)
    if (stat < 0 or stat >= ffiC.MAXSTATUS) then
        error("passed invalid statnum to spritesofstat iterator", 2)
    end

    return iter_spritesofstat, stat, -1
end

--== TROR iterators ==--
local function iter_sectorsofbunch(cf, i)
    if (i < 0) then
        i = ffiC.headsectbunch[cf][-i-1];
    else
        i = ffiC.nextsectbunch[cf][i];
    end

    if (i >= 0) then return i end
end

function sectorsofbunch(bunchnum, cf)
    if (bunchnum < 0 or bunchnum >= ffiC.numyaxbunches) then
        error("passed invalid bunchnum to sectorsofbunch iterator", 2)
    end
    if (cf ~= 0 and cf ~= 1) then
        error("passed invalid 'cf' to sectorsofbunch iterator, must be 0 or 1", 2)
    end

    return iter_sectorsofbunch, cf, -bunchnum-1
end

function getbunch(sectnum, cf)
    if (sectnum < 0 or sectnum >= ffiC.numsectors) then
        error("passed out-of-bounds sector number "..sectnum, 2)
    end
    if (cf ~= 0 and cf ~= 1) then
        error("passed invalid 'cf' to getbunch, must be 0 or 1", 2)
    end

    return ffiC.yax_getbunch(sectnum, cf)
end


---=== Engine functions, wrapped for Lua convenience ===---
-- returns a hitdata_ct
-- TODO: make v[xyz] be passed as one aggregate, too?
-- Additionally, permit different coordinates? (ang&horiz, ...)
function hitscan(pos, sectnum, vx,vy,vz, cliptype)
    if (sectnum >= ffiC.numsectors+0ULL) then
        error("passed out-of-bounds sector number "..sectnum, 2)
    end

    local vec = vec3_ct(pos.x, pos.y, pos.z)
    local hitdata = hitdata_ct()

    ffiC.hitscan(vec, sectnum, vx,vy,vz, hitdata, cliptype)
    return hitdata
end

function cansee(pos1,sect1, pos2,sect2)
    if (sect1 >= ffiC.numsectors+0ULL) then
        error("passed out-of-bounds first sector number "..sect1, 2)
    end
    if (sect2 >= ffiC.numsectors+0ULL) then
        error("passed out-of-bounds second sector number "..sect2, 2)
    end

    local ret = ffiC.cansee(pos1.x,pos1.y,pos1.z, sect1,
                            pos2.x,pos2.y,pos2.z, sect2)
    return (ret~=0)
end

-- TODO: should these rather be one function, and the specific kind of updating
-- controlled by an argument?
function updatesector(pos, sectnum)
    local sect = ffi.new("int16_t [1]")
    sect[0] = sectnum
    ffiC.updatesector(pos.x, pos.y, sect)
    return sect[0]
end

function updatesectorbreadth(pos, sectnum)
    local sect = ffi.new("int16_t [1]")
    sect[0] = sectnum
    ffiC.updatesectorbreadth(pos.x, pos.y, sect)
    return sect[0]
end

function updatesectorz(pos, sectnum)
    local sect = ffi.new("int16_t [1]")
    sect[0] = sectnum
    ffiC.updatesectorz(pos.x, pos.y, pos.z, sect)
    return sect[0]
end


-- This is supposed to be run from the file that 'require's this module to take
-- over the non-local variables from here into the global environment.
function create_globals(_G_their)
    local _G_our = getfenv(1)
    vars_to_ignore["create_globals"] = true

    for varname,obj in pairs(_G_our) do
        if (not vars_to_ignore[varname]) then
--            print("EXPORT "..varname)
            _G_their[varname] = obj
        end
    end
end
