//-------------------------------------------------------------------------
/*
Copyright (C) 1996, 2003 - 3D Realms Entertainment
Copyright (C) 2004, 2007 - EDuke32 developers

This file is part of EDuke32

EDuke32 is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License version 2
as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/
//-------------------------------------------------------------------------

#include "compat.h"
#include "build.h"
#include "editor.h"
#include "pragmas.h"
#include "baselayer.h"
#include "names.h"
#include "osd.h"
#include "osdfuncs.h"
#include "cache1d.h"

#include "mapster32.h"
#include "keys.h"
#include "types.h"
#include "keyboard.h"
#include "scriptfile.h"
#include "crc32.h"

#define VERSION " 1.1.0 svn"

static int floor_over_floor;

// static char *startwin_labeltext = "Starting Mapster32...";
static char setupfilename[BMAX_PATH]= "mapster32.cfg";
static char defaultduke3dgrp[BMAX_PATH] = "duke3d.grp";
static char *duke3dgrp = defaultduke3dgrp;
static int fixmapbeforesaving = 1;
static int NoAutoLoad = 0;

#if !defined(_WIN32)
static int usecwd = 0;
#endif

static struct strllist
{
    struct strllist *next;
    char *str;
}
*CommandPaths = NULL, *CommandGrps = NULL;

#define MAXHELP2D (signed int)(sizeof(Help2d)/sizeof(Help2d[0]))
static char *Help2d[]=
{
    " 'A = Autosave toggle",
    " 'J = Jump to location",
    " 'L = Adjust sprite/wall coords",
    " 'S = Sprite size",
    " '3 = Caption mode",
    " '7 = Swap tags",
    " 'F = Special functions",
    " X  = Horiz. flip selected sects",
    " Y  = Vert. flip selected sects",
    " F5 = Item count",
    " F6 = Actor count/SE help",
    " F7 = Edit sector",
    " F8 = Edit wall/sprite",
    " F9 = Sector tag help",
    " Ctrl-S = Quick save",
    " Alt-F7 = Search sector lotag",
    " Alt-F8 = Search wall/sprite tags",
    " [      = Search forward",
    " ]      = Search backward",
};

static char *SpriteMode[]=
{
    "NONE",
    "SECTORS",
    "WALLS",
    "SPRITES",
    "ALL",
    "ITEMS ONLY",
    "CURRENT SPRITE ONLY",
    "ONLY SECTOREFFECTORS AND SECTORS",
    "NO SECTOREFFECTORS OR SECTORS"
};

#define MAXSKILL 5
static char *SKILLMODE[MAXSKILL]=
{
    "Actor skill display: PIECE OF CAKE",
    "Actor skill display: LET'S ROCK",
    "Actor skill display: COME GET SOME",
    "Actor skill display: DAMN I'M GOOD",
    "Actor skill display: ALL SKILL LEVELS"
};

#define MAXNOSPRITES 4
static char *SPRDSPMODE[MAXNOSPRITES]=
{
    "Sprite display: DISPLAY ALL SPRITES",
    "Sprite display: NO EFFECTORS",
    "Sprite display: NO ACTORS",
    "Sprite display: NO EFFECTORS OR ACTORS"
};

#define MAXHELP3D (signed int)(sizeof(Help3d)/sizeof(Help3d[0]))
static char *Help3d[]=
{
    "Mapster32 3D mode help",
    " ",
    " F1 = TOGGLE THIS HELP DISPLAY",
    " F2 = TOGGLE CLIPBOARD",
    " F3 = MOUSELOOK",
    " F6 = AUTOMATIC SECTOREFFECTOR HELP",
    " F7 = AUTOMATIC SECTOR TAG HELP",
    "",
    " ' A = TOGGLE AUTOSAVE",
    " ' D = CYCLE SPRITE SKILL DISPLAY",
    " ' G = TOGGLE CLIPBOARD GRAPHIC DISPLAY",
    " ' R = TOGGLE FRAMERATE DISPLAY",
    " ' W = TOGGLE SPRITE DISPLAY",
    " ' X = SPRITE SHADE PREVIEW",
    " ' Y = TOGGLE PURPLE BACKGROUND",
    "",
    " ' T = CHANGE LOTAG",
    " ' H = CHANGE HITAG",
    " ' S = CHANGE SHADE",
    " ' M = CHANGE EXTRA",
    " ' V = CHANGE VISIBILITY",
    " ' L = CHANGE OBJECT COORDINATES",
    " ' C = CHANGE GLOBAL SHADE",
    "",
    " ' ENTER = PASTE GRAPHIC ONLY",
    " ' P & ; P = PASTE PALETTE TO ALL SELECTED SECTORS",
    " ; V = SET VISIBILITY ON ALL SELECTED SECTORS",
    " ' DEL = CSTAT=0",
    " CTRL-S = SAVE BOARD",
    " HOME = PGUP/PGDN MODIFIER (256 UNITS)",
    " END = PGUP/PGDN MODIFIER (512 UNITS)",
};

static CACHE1D_FIND_REC *finddirs=NULL, *findfiles=NULL, *finddirshigh=NULL, *findfileshigh=NULL;
static int numdirs=0, numfiles=0;
static int currentlist=0;

static void clearfilenames(void)
{
    klistfree(finddirs);
    klistfree(findfiles);
    finddirs = findfiles = NULL;
    numfiles = numdirs = 0;
}

static int getfilenames(const char *path, char kind[])
{
    CACHE1D_FIND_REC *r;

    clearfilenames();
    finddirs = klistpath(path,"*",CACHE1D_FIND_DIR);
    findfiles = klistpath(path,kind,CACHE1D_FIND_FILE);
    for (r = finddirs; r; r=r->next) numdirs++;
    for (r = findfiles; r; r=r->next) numfiles++;

    finddirshigh = finddirs;
    findfileshigh = findfiles;
    currentlist = 0;
    if (findfileshigh) currentlist = 1;

    return(0);
}

void ExtLoadMap(const char *mapname)
{
    int i;
    int sky=0;
    int j;
    // PreCache Wall Tiles
    /*
        for(j=0;j<numwalls;j++)
            if(waloff[wall[j].picnum] == 0)
            {
                loadtile(wall[j].picnum);
                if (bpp != 8)
                    polymost_precache(wall[j].picnum,wall[j].pal,0);
            }

        for(j=0;j<numsectors;j++)
            if(waloff[sector[j].floorpicnum] == 0 || waloff[sector[j].ceilingpicnum] == 0)
            {
                loadtile(sector[j].floorpicnum);
                loadtile(sector[j].ceilingpicnum);
                if (bpp != 8)
                {
                    polymost_precache(sector[j].floorpicnum,sector[j].floorpal,0);
                    polymost_precache(sector[j].floorpicnum,sector[j].floorpal,0);
                }
            }

        for(j=0;j<numsprites;j++)
            if(waloff[sprite[j].picnum] == 0)
            {
                loadtile(sprite[j].picnum);
                if (bpp != 8)
                    polymost_precache(sprite[j].picnum,sprite[j].pal,1);
            }
    */
    // Presize Sprites
    for (j=0;j<MAXSPRITES;j++)
    {
        if (tilesizx[sprite[j].picnum]==0 || tilesizy[sprite[j].picnum]==0)
            sprite[j].picnum=0;

        if (sprite[j].picnum>=20 && sprite[j].picnum<=59)
        {
            if (sprite[j].picnum==26)
            {
                sprite[j].xrepeat = 8;
                sprite[j].yrepeat = 8;
            }
            else
            {
                sprite[j].xrepeat = 32;
                sprite[j].yrepeat = 32;
            }
        }

    }

    Bstrcpy(levelname,mapname);
    pskyoff[0]=0;
    for (i=0;i<8;i++) pskyoff[i]=0;

    for (i=0;i<numsectors;i++)
    {
        switch (sector[i].ceilingpicnum)
        {
        case MOONSKY1 :
        case BIGORBIT1 : // orbit
        case LA : // la city
            sky=sector[i].ceilingpicnum;
            break;
        }
    }

    switch (sky)
    {
    case MOONSKY1 :
        //        earth          mountian   mountain         sun
        pskyoff[6]=1;
        pskyoff[1]=2;
        pskyoff[4]=2;
        pskyoff[2]=3;
        break;

    case BIGORBIT1 : // orbit
        //       earth1         2           3           moon/sun
        pskyoff[5]=1;
        pskyoff[6]=2;
        pskyoff[7]=3;
        pskyoff[2]=4;
        break;

    case LA : // la city
        //       earth1         2           3           moon/sun
        pskyoff[0]=1;
        pskyoff[1]=2;
        pskyoff[2]=1;
        pskyoff[3]=3;
        pskyoff[4]=4;
        pskyoff[5]=0;
        pskyoff[6]=2;
        pskyoff[7]=3;
        break;
    }

    pskybits=3;
    parallaxtype=0;
    Bsprintf(tempbuf, "Mapster32"VERSION" - %s",mapname);
    wm_setapptitle(tempbuf);
}

void ExtSaveMap(const char *mapname)
{
    UNREFERENCED_PARAMETER(mapname);
    saveboard("backup.map",&posx,&posy,&posz,&ang,&cursectnum);
}

const char *ExtGetSectorCaption(short sectnum)
{
    if (qsetmode != 200 && (!(onnames==1 || onnames==4 || onnames==7) || (onnames==8)))
    {
        tempbuf[0] = 0;
        return(tempbuf);
    }

    if (qsetmode != 200 && (sector[sectnum].lotag|sector[sectnum].hitag) == 0)
    {
        tempbuf[0] = 0;
    }
    else
    {
        switch (sector[sectnum].lotag)
        {
        case 1:
            Bsprintf(lo,"1 WATER (SE 7)");
            break;
        case 2:
            Bsprintf(lo,"2 UNDERWATER (SE 7)");
            break;
        case 9:
            Bsprintf(lo,"9 STAR TREK DOORS");
            break;
        case 15:
            Bsprintf(lo,"15 ELEVATOR TRANSPORT (SE 17)");
            break;
        case 16:
            Bsprintf(lo,"16 ELEVATOR PLATFORM DOWN");
            break;
        case 17:
            Bsprintf(lo,"17 ELEVATOR PLATFORM UP");
            break;
        case 18:
            Bsprintf(lo,"18 ELEVATOR DOWN");
            break;
        case 19:
            Bsprintf(lo,"19 ELEVATOR UP");
            break;
        case 20:
            Bsprintf(lo,"20 CEILING DOOR");
            break;
        case 21:
            Bsprintf(lo,"21 FLOOR DOOR");
            break;
        case 22:
            Bsprintf(lo,"22 SPLIT DOOR");
            break;
        case 23:
            Bsprintf(lo,"23 SWING DOOR (SE 11)");
            break;
        case 25:
            Bsprintf(lo,"25 SLIDE DOOR (SE 15)");
            break;
        case 26:
            Bsprintf(lo,"26 SPLIT STAR TREK DOOR");
            break;
        case 27:
            Bsprintf(lo,"27 BRIDGE (SE 20)");
            break;
        case 28:
            Bsprintf(lo,"28 DROP FLOOR (SE 21)");
            break;
        case 29:
            Bsprintf(lo,"29 TEETH DOOR (SE 22)");
            break;
        case 30:
            Bsprintf(lo,"30 ROTATE RISE BRIDGE");
            break;
        case 31:
            Bsprintf(lo,"31 2 WAY TRAIN (SE=30)");
            break;
        case 32767:
            Bsprintf(lo,"32767 SECRET ROOM");
            break;
        case -1:
            Bsprintf(lo,"65535 END OF LEVEL");
            break;
        default :
            Bsprintf(lo,"%hu",sector[sectnum].lotag);
            break;
        }
        if (sector[sectnum].lotag > 10000 && sector[sectnum].lotag < 32767)
            Bsprintf(lo,"%d 1 TIME SOUND",sector[sectnum].lotag);
        if (qsetmode != 200)
            Bsprintf(tempbuf,"%hu,%s",sector[sectnum].hitag,lo);
        else Bstrcpy(tempbuf,lo);
    }
    return(tempbuf);
}

const char *ExtGetWallCaption(short wallnum)
{
    if (!(onnames==2 || onnames==4))
    {
        tempbuf[0] = 0;
        return(tempbuf);
    }

    // HERE

    if ((wall[wallnum].lotag|wall[wallnum].hitag) == 0)
    {
        tempbuf[0] = 0;
    }
    else
    {
        Bsprintf(tempbuf,"%hu,%hu",wall[wallnum].hitag,wall[wallnum].lotag);
    }
    return(tempbuf);
} //end

const char *SectorEffectorText(short spritenum)
{
    switch (sprite[spritenum].lotag)
    {
    case 0:
        Bsprintf(tempbuf,"SE: ROTATED SECTOR");
        break;
    case 1:
        Bsprintf(tempbuf,"SE: PIVOT SPRITE FOR SE 0");
        break;
    case 2:
        Bsprintf(tempbuf,"SE: EARTHQUAKE");
        break;
    case 3:
        Bsprintf(tempbuf,"SE: RANDOM LIGHTS AFTER SHOT OUT");
        break;
    case 4:
        Bsprintf(tempbuf,"SE: RANDOM LIGHTS");
        break;
    case 6:
        Bsprintf(tempbuf,"SE: SUBWAY");
        break;
    case 7:
        Bsprintf(tempbuf,"SE: TRANSPORT");
        break;
    case 8:
        Bsprintf(tempbuf,"SE: UP OPEN DOOR LIGHTS");
        break;
    case 9:
        Bsprintf(tempbuf,"SE: DOWN OPEN DOOR LIGHTS");
        break;
    case 10:
        Bsprintf(tempbuf,"SE: DOOR AUTO CLOSE (H=DELAY)");
        break;
    case 11:
        Bsprintf(tempbuf,"SE: ROTATE SECTOR DOOR");
        break;
    case 12:
        Bsprintf(tempbuf,"SE: LIGHT SWITCH");
        break;
    case 13:
        Bsprintf(tempbuf,"SE: EXPLOSIVE");
        break;
    case 14:
        Bsprintf(tempbuf,"SE: SUBWAY CAR");
        break;
    case 15:
        Bsprintf(tempbuf,"SE: SLIDE DOOR (ST 25)");
        break;
    case 16:
        Bsprintf(tempbuf,"SE: ROTATE REACTOR SECTOR");
        break;
    case 17:
        Bsprintf(tempbuf,"SE: ELEVATOR TRANSPORT (ST 15)");
        break;
    case 18:
        Bsprintf(tempbuf,"SE: INCREMENTAL SECTOR RISE/FALL");
        break;
    case 19:
        Bsprintf(tempbuf,"SE: CEILING FALL ON EXPLOSION");
        break;
    case 20:
        Bsprintf(tempbuf,"SE: BRIDGE (ST 27)");
        break;
    case 21:
        Bsprintf(tempbuf,"SE: DROP FLOOR (ST 28)");
        break;
    case 22:
        Bsprintf(tempbuf,"SE: TEETH DOOR (ST 29)");
        break;
    case 23:
        Bsprintf(tempbuf,"SE: 1-WAY SE7 DESTINATION (H=SE 7)");
        break;
    case 24:
        Bsprintf(tempbuf,"SE: CONVAYER BELT");
        break;
    case 25:
        Bsprintf(tempbuf,"SE: ENGINE");
        break;
    case 28:
        Bsprintf(tempbuf,"SE: LIGHTNING (H= TILE#4890)");
        break;
    case 27:
        Bsprintf(tempbuf,"SE: CAMERA FOR PLAYBACK");
        break;
    case 29:
        Bsprintf(tempbuf,"SE: FLOAT");
        break;
    case 30:
        Bsprintf(tempbuf,"SE: 2 WAY TRAIN (ST=31)");
        break;
    case 31:
        Bsprintf(tempbuf,"SE: FLOOR RISE");
        break;
    case 32:
        Bsprintf(tempbuf,"SE: CEILING FALL");
        break;
    case 33:
        Bsprintf(tempbuf,"SE: SPAWN JIB W/QUAKE");
        break;
    case 36:
        Bsprintf(tempbuf,"SE: SKRINK RAY SHOOTER");
        break;
    default:
        SpriteName(spritenum,tempbuf);
        break;
    }
    return (tempbuf);
}

const char *ExtGetSpriteCaption(short spritenum)
{
    if ((onnames!=5 && onnames!=6 &&(!(onnames==3 || onnames==4 || onnames==7 || onnames==8))) || (onnames==7 && sprite[spritenum].picnum!=1))
    {
        tempbuf[0] = 0;
        return(tempbuf);
    }

    if (onnames==5)
    {
        switch (sprite[spritenum].picnum)
        {
        case FIRSTGUNSPRITE:
        case CHAINGUNSPRITE :
        case RPGSPRITE:
        case FREEZESPRITE:
        case SHRINKERSPRITE:
        case HEAVYHBOMB:
        case TRIPBOMBSPRITE:
        case SHOTGUNSPRITE:
        case DEVISTATORSPRITE:
        case FREEZEAMMO:
        case AMMO:
        case BATTERYAMMO:
        case DEVISTATORAMMO:
        case RPGAMMO:
        case GROWAMMO:
        case CRYSTALAMMO:
        case HBOMBAMMO:
        case AMMOLOTS:
        case SHOTGUNAMMO:
        case COLA:
        case SIXPAK:
        case FIRSTAID:
        case SHIELD:
        case STEROIDS:
        case AIRTANK:
        case JETPACK:
        case HEATSENSOR:
        case ACCESSCARD:
        case BOOTS:
            break;
        default:
        {
            tempbuf[0] = 0;
            return(tempbuf);
        }
        }
    }

    if (onnames==6 && sprite[spritenum].picnum != sprite[cursprite].picnum)
    {
        tempbuf[0] = 0;
        return(tempbuf);
    }

    tempbuf[0] = 0;
    if ((sprite[spritenum].lotag|sprite[spritenum].hitag) == 0)
    {
        SpriteName(spritenum,lo);
        if (lo[0]!=0)
        {
            if (sprite[spritenum].pal==1) Bsprintf(tempbuf,"%s (MULTIPLAYER)",lo);
            else Bsprintf(tempbuf,"%s",lo);
        }
    }
    else
        if (sprite[spritenum].picnum==SECTOREFFECTOR)
        {
            if (onnames==8)
                tempbuf[0] = 0;
            else
            {
                Bsprintf(lo,"%s: %hu",SectorEffectorText(spritenum),sprite[spritenum].lotag);
                Bsprintf(tempbuf,"%s, %hu",lo,sprite[spritenum].hitag);
            }
        }
        else
        {
            SpriteName(spritenum,lo);
            if (sprite[spritenum].extra != -1)
                Bsprintf(tempbuf,"%hu,%hu,%d %s",sprite[spritenum].hitag,sprite[spritenum].lotag,sprite[spritenum].extra,lo);
            else
                Bsprintf(tempbuf,"%hu,%hu %s",sprite[spritenum].hitag,sprite[spritenum].lotag,lo);
        }
    return(tempbuf);

} //end

//printext16 parameters:
//printext16(int xpos, int ypos, short col, short backcol,
//           char name[82], char fontsize)
//  xpos 0-639   (top left)
//  ypos 0-479   (top left)
//  col 0-15
//  backcol 0-15, -1 is transparent background
//  name
//  fontsize 0=8*8, 1=3*5

//drawline16 parameters:
// drawline16(int x1, int y1, int x2, int y2, char col)
//  x1, x2  0-639
//  y1, y2  0-143  (status bar is 144 high, origin is top-left of STATUS BAR)
//  col     0-15

void ExtShowSectorData(short sectnum)   //F5
{
    short statnum=0;
    int x,x2,y;
    int nexti;
    int i;
    int secrets=0;
    int totalactors1=0,totalactors2=0,totalactors3=0,totalactors4=0;
    int totalrespawn=0;

    UNREFERENCED_PARAMETER(sectnum);
    if (qsetmode==200)
        return;

    for (i=0;i<numsectors;i++)
    {
        if (sector[i].lotag==32767) secrets++;
    }

    statnum=0;
    i = headspritestat[statnum];
    while (i != -1)
    {
        nexti = nextspritestat[i];
        i = nexti;

        {
            switch (sprite[i].picnum)
            {
            case RECON:
            case DRONE:
            case LIZTROOPONTOILET:
            case LIZTROOPSTAYPUT:
            case LIZTROOPSHOOT:
            case LIZTROOPJETPACK:
            case LIZTROOPDUCKING:
            case LIZTROOPRUNNING:
            case LIZTROOP:
            case OCTABRAIN:
            case OCTABRAINSTAYPUT:
            case COMMANDER:
            case COMMANDERSTAYPUT:
            case EGG:
            case PIGCOP:
            case PIGCOPSTAYPUT:
            case PIGCOPDIVE:
            case LIZMAN:
            case LIZMANSTAYPUT:
            case LIZMANSPITTING:
            case LIZMANFEEDING:
            case LIZMANJUMP:
            case ORGANTIC:
            case BOSS1:
            case BOSS2:
            case BOSS3:
            case GREENSLIME:
            case ROTATEGUN:
            case TANK:
            case NEWBEAST:
            case BOSS4:
                if (sprite[i].lotag<=1) totalactors1++;
                if (sprite[i].lotag<=2) totalactors2++;
                if (sprite[i].lotag<=3) totalactors3++;
                if (sprite[i].lotag<=4) totalactors4++;
                break;

            case RESPAWN:
                totalrespawn++;

            default:
                break;
            }
        }
    }

    for (i=0;i<MAXSPRITES;i++) numsprite[i]=0;
    for (i=0;i<MAXSPRITES;i++) multisprite[i]=0;
    for (i=0;i<MAXSPRITES;i++)
    {
        if (sprite[i].statnum==0)
        {
            if (sprite[i].pal!=0) multisprite[sprite[i].picnum]++;
            else numsprite[sprite[i].picnum]++;
        }
    }

    clearmidstatbar16();             //Clear middle of status bar
    Bsprintf(tempbuf,"Level %s",levelname);
    printmessage16(tempbuf);

    x=1;
    x2=14;
    y=4;
    begindrawing();
    printext16(x*8,ydim16+y*8,11,-1,"Item Count",0);
    enddrawing();
    PrintStatus("10%health=",numsprite[COLA],x,y+2,11);
    PrintStatus("",multisprite[COLA],x2,y+2,9);
    PrintStatus("30%health=",numsprite[SIXPAK],x,y+3,11);
    PrintStatus("",multisprite[SIXPAK],x2,y+3,9);
    PrintStatus("Med-Kit  =",numsprite[FIRSTAID],x,y+4,11);
    PrintStatus("",multisprite[FIRSTAID],x2,y+4,9);
    PrintStatus("Atom     =",numsprite[ATOMICHEALTH],x,y+5,11);
    PrintStatus("",multisprite[ATOMICHEALTH],x2,y+5,9);
    PrintStatus("Shields  =",numsprite[SHIELD],x,y+6,11);
    PrintStatus("",multisprite[SHIELD],x2,y+6,9);

    x=17;
    x2=30;
    y=4;
    begindrawing();
    printext16(x*8,ydim16+y*8,11,-1,"Inventory",0);
    enddrawing();
    PrintStatus("Steroids =",numsprite[STEROIDS],x,y+2,11);
    PrintStatus("",multisprite[STEROIDS],x2,y+2,9);
    PrintStatus("Airtank  =",numsprite[AIRTANK],x,y+3,11);
    PrintStatus("",multisprite[AIRTANK],x2,y+3,9);
    PrintStatus("Jetpack  =",numsprite[JETPACK],x,y+4,11);
    PrintStatus("",multisprite[JETPACK],x2,y+4,9);
    PrintStatus("Goggles  =",numsprite[HEATSENSOR],x,y+5,11);
    PrintStatus("",multisprite[HEATSENSOR],x2,y+5,9);
    PrintStatus("Boots    =",numsprite[BOOTS],x,y+6,11);
    PrintStatus("",multisprite[BOOTS],x2,y+6,9);
    PrintStatus("HoloDuke =",numsprite[HOLODUKE],x,y+7,11);
    PrintStatus("",multisprite[HOLODUKE],x2,y+7,9);
    PrintStatus("Multi D  =",numsprite[APLAYER],x,y+8,11);

    x=33;
    x2=46;
    y=4;
    begindrawing();
    printext16(x*8,ydim16+y*8,11,-1,"Weapon Count",0);
    enddrawing();
    PrintStatus("Pistol   =",numsprite[FIRSTGUNSPRITE],x,y+2,11);
    PrintStatus("",multisprite[FIRSTGUNSPRITE],x2,y+2,9);
    PrintStatus("Shotgun  =",numsprite[SHOTGUNSPRITE],x,y+3,11);
    PrintStatus("",multisprite[SHOTGUNSPRITE],x2,y+3,9);
    PrintStatus("Chaingun =",numsprite[CHAINGUNSPRITE],x,y+4,11);
    PrintStatus("",multisprite[CHAINGUNSPRITE],x2,y+4,9);
    PrintStatus("RPG      =",numsprite[RPGSPRITE],x,y+5,11);
    PrintStatus("",multisprite[RPGSPRITE],x2,y+5,9);
    PrintStatus("Pipe Bomb=",numsprite[HEAVYHBOMB],x,y+6,11);
    PrintStatus("",multisprite[HEAVYHBOMB],x2,y+6,9);
    PrintStatus("Shrinker =",numsprite[SHRINKERSPRITE],x,y+7,11);
    PrintStatus("",multisprite[SHRINKERSPRITE],x2,y+7,9);
    PrintStatus("Devastatr=",numsprite[DEVISTATORSPRITE],x,y+8,11);
    PrintStatus("",multisprite[DEVISTATORSPRITE],x2,y+8,9);
    PrintStatus("Trip mine=",numsprite[TRIPBOMBSPRITE],x,y+9,11);
    PrintStatus("",multisprite[TRIPBOMBSPRITE],x2,y+9,9);
    PrintStatus("Freezeray=",numsprite[FREEZESPRITE],x,y+10,11);
    PrintStatus("",multisprite[FREEZESPRITE],x2,y+10,9);

    x=49;
    x2=62;
    y=4;
    begindrawing();
    printext16(x*8,ydim16+y*8,11,-1,"Ammo Count",0);
    enddrawing();
    PrintStatus("Pistol   =",numsprite[AMMO],x,y+2,11);
    PrintStatus("",multisprite[AMMO],x2,y+2,9);
    PrintStatus("Shot     =",numsprite[SHOTGUNAMMO],x,y+3,11);
    PrintStatus("",multisprite[SHOTGUNAMMO],x2,y+3,9);
    PrintStatus("Chain    =",numsprite[BATTERYAMMO],x,y+4,11);
    PrintStatus("",multisprite[BATTERYAMMO],x2,y+4,9);
    PrintStatus("RPG Box  =",numsprite[RPGAMMO],x,y+5,11);
    PrintStatus("",multisprite[RPGAMMO],x2,y+5,9);
    PrintStatus("Pipe Bomb=",numsprite[HBOMBAMMO],x,y+6,11);
    PrintStatus("",multisprite[HBOMBAMMO],x2,y+6,9);
    PrintStatus("Shrinker =",numsprite[CRYSTALAMMO],x,y+7,11);
    PrintStatus("",multisprite[CRYSTALAMMO],x2,y+7,9);
    PrintStatus("Devastatr=",numsprite[DEVISTATORAMMO],x,y+8,11);
    PrintStatus("",multisprite[DEVISTATORAMMO],x2,y+8,9);
    PrintStatus("Expander =",numsprite[GROWAMMO],x,y+9,11);
    PrintStatus("",multisprite[GROWAMMO],x2,y+9,9);
    PrintStatus("Freezeray=",numsprite[FREEZEAMMO],x,y+10,11);
    PrintStatus("",multisprite[FREEZEAMMO],x2,y+10,9);

    begindrawing();
    printext16(65*8,ydim16+4*8,11,-1,"MISC",0);
    enddrawing();
    PrintStatus("Secrets =",secrets,65,6,11);
    begindrawing();
    printext16(65*8,ydim16+8*8,11,-1,"ACTORS",0);
    enddrawing();
    PrintStatus("Skill 1 =",totalactors1,65,10,11);
    PrintStatus("Skill 2 =",totalactors2,65,11,11);
    PrintStatus("Skill 3 =",totalactors3,65,12,11);
    PrintStatus("Skill 4 =",totalactors4,65,13,11);
    PrintStatus("Respawn =",totalrespawn,65,14,11);

}// end ExtShowSectorData

void ExtShowWallData(short wallnum)       //F6
{
    int i,nextfreetag=0,total=0;
    char x,y;

    UNREFERENCED_PARAMETER(wallnum);

    if (qsetmode==200)
        return;

    for (i=0;i<MAXSPRITES;i++)
    {
        if (sprite[i].statnum==0)
            switch (sprite[i].picnum)
            {
                //LOTAG
            case ACTIVATOR:
            case ACTIVATORLOCKED:
            case TOUCHPLATE:
            case MASTERSWITCH:
            case RESPAWN:
            case ACCESSSWITCH:
            case SLOTDOOR:
            case LIGHTSWITCH:
            case SPACEDOORSWITCH:
            case SPACELIGHTSWITCH:
            case FRANKENSTINESWITCH:
            case MULTISWITCH:
            case DIPSWITCH:
            case DIPSWITCH2:
            case TECHSWITCH:
            case DIPSWITCH3:
            case ACCESSSWITCH2:
            case POWERSWITCH1:
            case LOCKSWITCH1:
            case POWERSWITCH2:
            case PULLSWITCH:
            case ALIENSWITCH:
                if (sprite[i].lotag>nextfreetag) nextfreetag=1+sprite[i].lotag;
                break;

                //HITAG
            case SEENINE:
            case OOZFILTER:
            case SECTOREFFECTOR:
                if (sprite[i].lotag==10 || sprite[i].lotag==27 || sprite[i].lotag==28 || sprite[i].lotag==29)
                    break;
                else
                    if (sprite[i].hitag>nextfreetag) nextfreetag=1+sprite[i].hitag;
                break;
            default:
                break;

            }

    } // end sprite loop

    //Count Normal Actors
    for (i=0;i<MAXSPRITES;i++) numsprite[i]=0;
    for (i=0;i<MAXSPRITES;i++) multisprite[i]=0;
    for (i=0;i<MAXSPRITES;i++)
    {
        if (sprite[i].statnum==0)
        {
            if (sprite[i].pal!=0)
                switch (sprite[i].picnum)
                {
                case LIZTROOP :
                case LIZTROOPRUNNING :
                case LIZTROOPSTAYPUT :
                case LIZTROOPSHOOT :
                case LIZTROOPJETPACK :
                case LIZTROOPONTOILET :
                case LIZTROOPDUCKING :
                    numsprite[LIZTROOP]++;
                    break;
                case BOSS1:
                case BOSS1STAYPUT:
                case BOSS1SHOOT:
                case BOSS1LOB:
                case BOSSTOP:
                    multisprite[BOSS1]++;
                    break;
                case BOSS2:
                    multisprite[BOSS2]++;
                    break;
                case BOSS3:
                    multisprite[BOSS3]++;
                    break;

                default:
                    break;
                }
            else
                switch (sprite[i].picnum)
                {
                case LIZTROOP :
                case LIZTROOPRUNNING :
                case LIZTROOPSTAYPUT :
                case LIZTROOPSHOOT :
                case LIZTROOPJETPACK :
                case LIZTROOPONTOILET :
                case LIZTROOPDUCKING :
                    numsprite[LIZTROOP]++;
                    break;
                case PIGCOP:
                case PIGCOPSTAYPUT:
                case PIGCOPDIVE:
                    numsprite[PIGCOP]++;
                    break;
                case LIZMAN:
                case LIZMANSTAYPUT:
                case LIZMANSPITTING:
                case LIZMANFEEDING:
                case LIZMANJUMP:
                    numsprite[LIZMAN]++;
                    break;
                case BOSS1:
                case BOSS1STAYPUT:
                case BOSS1SHOOT:
                case BOSS1LOB:
                case BOSSTOP:
                    numsprite[BOSS1]++;
                    break;
                case COMMANDER:
                case COMMANDERSTAYPUT:
                    numsprite[COMMANDER]++;
                    break;
                case OCTABRAIN:
                case OCTABRAINSTAYPUT:
                    numsprite[OCTABRAIN]++;
                    break;
                case RECON:
                case DRONE:
                case ROTATEGUN:
                case EGG:
                case ORGANTIC:
                case GREENSLIME:
                case BOSS2:
                case BOSS3:
                case TANK:
                case NEWBEAST:
                case BOSS4:

                    numsprite[sprite[i].picnum]++;
                default:
                    break;

                }// end switch
        }// end if
    }//end for
    total=0;
    for (i=0;i<MAXSPRITES;i++) if (numsprite[i]!=0) total+=numsprite[i];
    for (i=0;i<MAXSPRITES;i++) if (multisprite[i]!=0) total+=multisprite[i];

    clearmidstatbar16();

    Bsprintf(tempbuf,"Level %s next tag %d",levelname,nextfreetag);
    printmessage16(tempbuf);

    x=2;
    y=4;
    PrintStatus("Normal Actors =",total,x,y,11);

    PrintStatus(" Liztroop  =",numsprite[LIZTROOP],x,y+1,11);
    PrintStatus(" Lizman    =",numsprite[LIZMAN],x,y+2,11);
    PrintStatus(" Commander =",numsprite[COMMANDER],x,y+3,11);
    PrintStatus(" Octabrain =",numsprite[OCTABRAIN],x,y+4,11);
    PrintStatus(" PigCop    =",numsprite[PIGCOP],x,y+5,11);
    PrintStatus(" Recon Car =",numsprite[RECON],x,y+6,11);
    PrintStatus(" Drone     =",numsprite[DRONE],x,y+7,11);
    PrintStatus(" Turret    =",numsprite[ROTATEGUN],x,y+8,11);
    PrintStatus(" Egg       =",numsprite[EGG],x,y+9,11);
    x+=17;
    PrintStatus("Slimer    =",numsprite[GREENSLIME],x,y+1,11);
    PrintStatus("Boss1     =",numsprite[BOSS1],x,y+2,11);
    PrintStatus("MiniBoss1 =",multisprite[BOSS1],x,y+3,11);
    PrintStatus("Boss2     =",numsprite[BOSS2],x,y+4,11);
    PrintStatus("Boss3     =",numsprite[BOSS3],x,y+5,11);
    PrintStatus("Riot Tank =",numsprite[TANK],x,y+6,11);
    PrintStatus("Newbeast  =",numsprite[NEWBEAST],x,y+7,11);
    PrintStatus("Boss4     =",numsprite[BOSS4],x,y+8,11);

    //Count Respawn Actors
    for (i=0;i<MAXSPRITES;i++) numsprite[i]=0;
    for (i=0;i<MAXSPRITES;i++) multisprite[i]=0;
    for (i=0;i<MAXSPRITES;i++)
    {
        if (sprite[i].statnum==0 && sprite[i].picnum==RESPAWN)
        {
            switch (sprite[i].hitag)
            {
            case LIZTROOP :
            case LIZTROOPRUNNING :
            case LIZTROOPSTAYPUT :
            case LIZTROOPSHOOT :
            case LIZTROOPJETPACK :
            case LIZTROOPONTOILET :
            case LIZTROOPDUCKING :
                numsprite[LIZTROOP]++;
                break;
            case PIGCOP:
            case PIGCOPSTAYPUT:
            case PIGCOPDIVE:
                numsprite[PIGCOP]++;
                break;
            case LIZMAN:
            case LIZMANSTAYPUT:
            case LIZMANSPITTING:
            case LIZMANFEEDING:
            case LIZMANJUMP:
                numsprite[LIZMAN]++;
                break;
            case BOSS1:
            case BOSS1STAYPUT:
            case BOSS1SHOOT:
            case BOSS1LOB:
            case BOSSTOP:
                if (sprite[i].pal!=0) multisprite[BOSS1]++;
                else numsprite[BOSS1]++;
                break;
            case COMMANDER:
            case COMMANDERSTAYPUT:
                numsprite[COMMANDER]++;
                break;
            case OCTABRAIN:
            case OCTABRAINSTAYPUT:
                numsprite[OCTABRAIN]++;
                break;
            case RECON:
            case DRONE:
            case ROTATEGUN:
            case EGG:
            case ORGANTIC:
            case GREENSLIME:
            case BOSS2:
            case BOSS3:
            case TANK:
            case NEWBEAST:
            case BOSS4:
                numsprite[sprite[i].hitag]++;
            default:
                break;
            }//end switch
        }// end if
    }// end for
    total=0;
    for (i=0;i<MAXSPRITES;i++) if (numsprite[i]!=0) total+=numsprite[i];
    for (i=0;i<MAXSPRITES;i++) if (multisprite[i]!=0) total+=multisprite[i];


    x=36;
    y=4;
    PrintStatus("Respawn",total,x,y,11);

    PrintStatus(" Liztroop  =",numsprite[LIZTROOP],x,y+1,11);
    PrintStatus(" Lizman    =",numsprite[LIZMAN],x,y+2,11);
    PrintStatus(" Commander =",numsprite[COMMANDER],x,y+3,11);
    PrintStatus(" Octabrain =",numsprite[OCTABRAIN],x,y+4,11);
    PrintStatus(" PigCop    =",numsprite[PIGCOP],x,y+5,11);
    PrintStatus(" Recon Car =",numsprite[RECON],x,y+6,11);
    PrintStatus(" Drone     =",numsprite[DRONE],x,y+7,11);
    PrintStatus(" Turret    =",numsprite[ROTATEGUN],x,y+8,11);
    PrintStatus(" Egg       =",numsprite[EGG],x,y+9,11);
    x+=17;
    PrintStatus("Slimer    =",numsprite[GREENSLIME],x,y+1,11);
    PrintStatus("Boss1     =",numsprite[BOSS1],x,y+2,11);
    PrintStatus("MiniBoss1 =",multisprite[BOSS1],x,y+3,11);
    PrintStatus("Boss2     =",numsprite[BOSS2],x,y+4,11);
    PrintStatus("Boss3     =",numsprite[BOSS3],x,y+5,11);
    PrintStatus("Riot Tank =",numsprite[TANK],x,y+6,11);
    PrintStatus("Newbeast  =",numsprite[NEWBEAST],x,y+7,11);
    PrintStatus("Boss4     =",numsprite[BOSS4],x,y+8,11);
}// end ExtShowWallData

static void Show2dText(char *name)
{
    int fp,t;
    unsigned char x=0,y=4,xmax=0,xx=0,col=0;
    clearmidstatbar16();
    if ((fp=kopen4load(name,0)) == -1)
    {
        begindrawing();
        printext16(1*4,ydim16+4*8,11,-1,"ERROR: file not found.",0);
        enddrawing();
        return;
    }

    t=65;
    begindrawing();
    while (t!=EOF && col<5)
    {
        t = 0;
        if (kread(fp,&t,1)<=0)
            t = EOF;
        while (t!=EOF && t!='\n' && x<250)
        {
            tempbuf[x]=t;
            t = 0;
            if (kread(fp,&t,1)<=0) t = EOF;
            x++;
            if (x>xmax) xmax=x;
        }
        tempbuf[x]=0;
        printext16(xx*4,ydim16+(y*6)+2,11,-1,tempbuf,1);
        x=0;
        y++;
        if (y>18)
        {
            col++;
            y=6;
            xx+=xmax;
            xmax=0;
        }
    }
    enddrawing();

    kclose(fp);

}// end Show2dText

static void Show3dText(char *name)
{
    int fp,t;
    unsigned char x=0,y=4,xmax=0,xx=0,col=0;

    if ((fp=kopen4load(name,0)) == -1)
    {
        begindrawing();
        printext256(1*4,4*8,whitecol,-1,"ERROR: file not found.",0);
        enddrawing();
        return;
    }
    t=65;
    begindrawing();
    while (t!=EOF && col<5)
    {
        t = 0;
        if (kread(fp,&t,1)<=0)
            t = EOF;
        while (t!=EOF && t!='\n' && x<250)
        {
            tempbuf[x]=t;
            t = 0;
            if (kread(fp,&t,1)<=0) t = EOF;
            x++;
            if (x>xmax) xmax=x;
        }
        tempbuf[x]=0;
        printext256(xx*4,(y*6)+2,whitecol,-1,tempbuf,1);
        x=0;
        y++;
        if (y>18)
        {
            col++;
            y=6;
            xx+=xmax;
            xmax=0;
        }
    }
    enddrawing();

    kclose(fp);
}// end Show3dText

#if 0
static void ShowHelpText(char *name)
{
    BFILE *fp;
    char x=0,y=4;
    UNREFERENCED_PARAMETER(name);
    if ((fp=fopenfrompath("helpdoc.txt","rb")) == NULL)
    {
        begindrawing();
        printext256(1*4,4*8,whitecol,-1,"ERROR: file not found.",0);
        enddrawing();
        return;
    }
    /*
        Bfgets(tempbuf,80,fp);
        while(!Bfeof(fp) && Bstrcmp(tempbuf,"SectorEffector"))
        {
            Bfgets(tempbuf,80,fp);
        }
    */
    y=2;
    Bfgets(tempbuf,80,fp);
    Bstrcat(tempbuf,"\n");
    begindrawing();
    while (!Bfeof(fp) && !(Bstrcmp(tempbuf,"SectorEffector")==0))
    {
        Bfgets(tempbuf,80,fp);
        Bstrcat(tempbuf,"\n");
        printext256(x*4,(y*6)+2,whitecol,-1,tempbuf,1);
        y++;
    }
    enddrawing();

    Bfclose(fp);
}// end ShowHelpText
#endif
void ExtShowSpriteData(short spritenum)   //F6
{
    UNREFERENCED_PARAMETER(spritenum);
    if (qsetmode != 200)
        Show2dText("sehelp.hlp");
    /*    if (qsetmode == 200)                // In 3D mode
            return;

        while (KEY_PRESSED(KEYSC_F6));
        ResetKeys();
        ContextHelp(spritenum);             // Get context sensitive help */
}// end ExtShowSpriteData

// Floor Over Floor (duke3d)

// If standing in sector with SE42 or SE44
// then draw viewing to SE41 and raise all =hi SE43 cielings.

// If standing in sector with SE43 or SE45
// then draw viewing to SE40 and lower all =hi SE42 floors.

int fofsizex = -1;
int fofsizey = -1;
#if 0
static void ResetFOFSize()
{
    if (fofsizex != -1) tilesizx[FOF] = fofsizex;
    if (fofsizey != -1) tilesizy[FOF] = fofsizey;
}
#endif
static void ExtSE40Draw(int spnum,int x,int y,int z,short a,short h)
{
    static int tempsectorz[MAXSECTORS];
    static int tempsectorpicnum[MAXSECTORS];

    int i=0,j=0,k=0;
    int floor1=0,floor2=0,ok=0,fofmode=0,draw_both=0;
    int offx,offy,offz;

    if (sprite[spnum].ang!=512) return;

    // Things are a little different now, as we allow for masked transparent
    // floors and ceilings. So the FOF textures is no longer required
    //	if (!(gotpic[FOF>>3]&(1<<(FOF&7))))
    //		return;
    //	gotpic[FOF>>3] &= ~(1<<(FOF&7));

    if (tilesizx[562])
    {
        fofsizex = tilesizx[562];
        tilesizx[562] = 0;
    }
    if (tilesizy[562])
    {
        fofsizey = tilesizy[562];
        tilesizy[562] = 0;
    }

    floor1=spnum;

    if (sprite[spnum].lotag==42) fofmode=40;
    if (sprite[spnum].lotag==43) fofmode=41;
    if (sprite[spnum].lotag==44) fofmode=40;
    if (sprite[spnum].lotag==45) fofmode=41;

    // fofmode=sprite[spnum].lotag-2;

    // sectnum=sprite[j].sectnum;
    // sectnum=cursectnum;
    ok++;

    /*  recursive?
    for(j=0;j<MAXSPRITES;j++)
    {
    if(
    sprite[j].sectnum==sectnum &&
    sprite[j].picnum==1 &&
    sprite[j].lotag==110
       ) { DrawFloorOverFloor(j); break;}
    }
    */

    // if(ok==0) { Message("no fof",RED); return; }

    for (j=0;j<MAXSPRITES;j++)
    {
        if (sprite[j].picnum==1 && sprite[j].lotag==fofmode && sprite[j].hitag==sprite[floor1].hitag)
        {
            floor1=j;
            fofmode=sprite[j].lotag;
            ok++;
            break;
        }
    }
    // if(ok==1) { Message("no floor1",RED); return; }

    if (fofmode==40) k=41;
    else k=40;

    for (j=0;j<MAXSPRITES;j++)
    {
        if (sprite[j].picnum==1 && sprite[j].lotag==k && sprite[j].hitag==sprite[floor1].hitag)
        {
            floor2=j;
            ok++;
            break;
        }
    }

    i=floor1;
    offx=sprite[floor2].x-sprite[floor1].x;
    offy=sprite[floor2].y-sprite[floor1].y;
    offz=0;

    if (sprite[floor2].ang >= 1024)
        offz = sprite[floor2].z;
    else if (fofmode==41)
        offz = sector[sprite[floor2].sectnum].floorz;
    else
        offz = sector[sprite[floor2].sectnum].ceilingz;

    if (sprite[floor1].ang >= 1024)
        offz -= sprite[floor1].z;
    else if (fofmode==40)
        offz -= sector[sprite[floor1].sectnum].floorz;
    else
        offz -= sector[sprite[floor1].sectnum].ceilingz;

    // if(ok==2) { Message("no floor2",RED); return; }

    for (j=0;j<MAXSPRITES;j++) // raise ceiling or floor
    {
        if (sprite[j].picnum==1 && sprite[j].lotag==k+2 && sprite[j].hitag==sprite[floor1].hitag)
        {
            if (k==40)
            {
                tempsectorz[sprite[j].sectnum]=sector[sprite[j].sectnum].floorz;
                sector[sprite[j].sectnum].floorz+=(((z-sector[sprite[j].sectnum].floorz)/32768)+1)*32768;
                tempsectorpicnum[sprite[j].sectnum]=sector[sprite[j].sectnum].floorpicnum;
                sector[sprite[j].sectnum].floorpicnum=562;
            }
            else
            {
                tempsectorz[sprite[j].sectnum]=sector[sprite[j].sectnum].ceilingz;
                sector[sprite[j].sectnum].ceilingz+=(((z-sector[sprite[j].sectnum].ceilingz)/32768)-1)*32768;
                tempsectorpicnum[sprite[j].sectnum]=sector[sprite[j].sectnum].ceilingpicnum;
                sector[sprite[j].sectnum].ceilingpicnum=562;
            }
            draw_both = 1;
        }
    }

    drawrooms(x+offx,y+offy,z+offz,a,h,sprite[floor2].sectnum);
    ExtAnalyzeSprites();
    drawmasks();

    if (draw_both)
    {
        for (j=0;j<MAXSPRITES;j++) // restore ceiling or floor for the draw both sectors
        {
            if (sprite[j].picnum==1 &&
                    sprite[j].lotag==k+2 &&
                    sprite[j].hitag==sprite[floor1].hitag)
            {
                if (k==40)
                {
                    sector[sprite[j].sectnum].floorz=tempsectorz[sprite[j].sectnum];
                    sector[sprite[j].sectnum].floorpicnum=tempsectorpicnum[sprite[j].sectnum];
                }
                else
                {
                    sector[sprite[j].sectnum].ceilingz=tempsectorz[sprite[j].sectnum];
                    sector[sprite[j].sectnum].ceilingpicnum=tempsectorpicnum[sprite[j].sectnum];
                }
            }// end if
        }// end for

        // Now re-draw
        drawrooms(x+offx,y+offy,z+offz,a,h,sprite[floor2].sectnum);
        ExtAnalyzeSprites();
        drawmasks();
    }

} // end SE40

static void SE40Code(int x,int y,int z,int a,int h)
{
    int i;

    i = 0;
    while (i<MAXSPRITES)
    {
        int t = sprite[i].lotag;
        switch (t)
        {
            //            case 40:
            //            case 41:
            //                ExtSE40Draw(i,x,y,z,a,h);
            //                break;
        case 42:
        case 43:
        case 44:
        case 45:
            if (cursectnum == sprite[i].sectnum)
                ExtSE40Draw(i,x,y,z,a,h);
            break;
        }
        i++;
    }
}

void ExtEditSectorData(short sectnum)    //F7
{
    //    if (qsetmode != 200) Show2dText("sthelp.hlp");
    if (qsetmode == 200)
        return;
    if ((keystatus[0x38]|keystatus[0xb8]) > 0)  //ALT
    {
        keystatus[67] = 0;
        wallsprite=0;
        curwall = 0;
        curwallnum = 0;
        cursearchspritenum = 0;
        cursectornum=0;
        cursector_lotag = sector[sectnum].lotag;
        cursector_lotag=getnumber16("Enter search sector lotag : ", cursector_lotag, 65536L,0);
        Bsprintf(tempbuf,"Search sector lotag %d",cursector_lotag);
        printmessage16(tempbuf);
    }
    else EditSectorData(sectnum);
}// end ExtEditSectorData

void ExtEditWallData(short wallnum)       //F8
{
    if (qsetmode==200)
        return;
    if ((keystatus[0x38]|keystatus[0xb8]) > 0)  //ALT
    {
        wallsprite=1;
        curwall = wallnum;
        curwallnum = 0;
        cursearchspritenum = 0;
        cursectornum = 0;
        search_lotag = wall[curwall].lotag;
        search_hitag = wall[curwall].hitag;
        search_lotag=getnumber16("Enter wall search lotag : ", search_lotag, 65536L,0);
        search_hitag=getnumber16("Enter wall search hitag : ", search_hitag, 65536L,0);
        //    Bsprintf(tempbuf,"Current wall %d lo=%d hi=%d",
        //             curwall,wall[curwall].lotag,wall[curwall].hitag);
        Bsprintf(tempbuf,"Search wall lo=%d hi=%d",search_lotag,search_hitag);
        printmessage16(tempbuf);
    }
    else EditWallData(wallnum);
}

void ExtEditSpriteData(short spritenum)   //F8
{
    if (qsetmode==200)
        return;
    if ((keystatus[0x38]|keystatus[0xb8]) > 0)  //ALT
    {
        wallsprite=2;
        cursearchsprite = spritenum;
        curwallnum = 0;
        cursearchspritenum = 0;
        cursectornum = 0;
        search_lotag = sprite[cursearchsprite].lotag;
        search_hitag = sprite[cursearchsprite].hitag;
        search_lotag=getnumber16("Enter sprite search lotag : ", search_lotag, 65536L,0);
        search_hitag=getnumber16("Enter sprite search hitag : ", search_hitag, 65536L,0);
        Bsprintf(tempbuf,"Search sprite lo=%d hi=%d",search_lotag,search_hitag);
        printmessage16(tempbuf);
    }
    else EditSpriteData(spritenum);
}

static void PrintStatus(char *string,int num,char x,char y,char color)
{
    Bsprintf(tempbuf,"%s %d",string,num);
    begindrawing();
    printext16(x*8,ydim16+y*8,color,-1,tempbuf,0);
    enddrawing();
}

static inline void SpriteName(short spritenum, char *lo2)
{
    Bsprintf(lo2,names[sprite[spritenum].picnum]);
}// end SpriteName

static void ReadPaletteTable()
{
    int i,j,fp;
    char lookup_num;
    if ((fp=kopen4load("lookup.dat",0)) == -1)
    {
        if ((fp=kopen4load("lookup.dat",1)) == -1)
        {
            initprintf("LOOKUP.DAT not found, creating dummy palette lookups\n");
            for (i=0;i<256;i++)
                tempbuf[i] = ((i+32)&255);  //remap colors for screwy palette sectors
            makepalookup(MAXPALOOKUPS,tempbuf,0,0,0,1);
            return;
        }
    }
    initprintf("Loading palette lookups... ");
    kread(fp,&num_tables,1);
    for (j=0;j<num_tables;j++)
    {
        kread(fp,&lookup_num,1);
        kread(fp,tempbuf,256);
        makepalookup(lookup_num,tempbuf,0,0,0,1);
    }
    for (j = 0; j < 256; j++)
        tempbuf[j] = j;
    num_tables++;
    makepalookup(num_tables, tempbuf, 15, 15, 15, 1);
    makepalookup(num_tables + 1, tempbuf, 15, 0, 0, 1);
    makepalookup(num_tables + 2, tempbuf, 0, 15, 0, 1);
    makepalookup(num_tables + 3, tempbuf, 0, 0, 15, 1);

    kread(fp,WATERpalette,768);
    kread(fp,SLIMEpalette,768);
    kread(fp,TITLEpalette,768);
    kread(fp,REALMSpalette,768);
    kread(fp,BOSS1palette,768);
    kclose(fp);
    initprintf("success.\n");
}// end ReadPaletteTable

static void ReadGamePalette()
{
    int i,fp;
    if ((fp=kopen4load("palette.dat",0)) == -1)
        if ((fp=kopen4load("palette.dat",1)) == -1)
        {
            initprintf("!!! PALETTE.DAT NOT FOUND !!!\n");
            Bstrcpy(tempbuf, "Mapster32"VERSION"");
            wm_msgbox(tempbuf,"palette.dat not found");
            exit(0);
        }
    initprintf("Loading game palette... ");
    kread(fp,GAMEpalette,768);
    for (i=0;i<768;++i) GAMEpalette[i]=GAMEpalette[i];
    kclose(fp);
    initprintf("success.\n");
    ReadPaletteTable();
}

static inline void _message(char message[162])
{
    Bstrcpy(getmessage,message);
    getmessageleng = strlen(getmessage);
    getmessagetimeoff = totalclock+120*5;
}

static void message(char message[162])
{
    char tmpbuf[2048];

    _message(message);
    Bstrcpy(tmpbuf,message);
    Bstrcat(tmpbuf,"\n");
    OSD_Printf(tmpbuf);
    lastmessagetime = totalclock;
}

static char lockbyte4094;

static int lastupdate, mousecol, mouseadd = 1, bstatus;

static void m32_showmouse(void)
{
    int i, col;

    if (totalclock > lastupdate)
    {
        mousecol += mouseadd;
        if (mousecol >= 30 || mousecol <= 0)
        {
            mouseadd = -mouseadd;
            mousecol += mouseadd;
        }
        lastupdate = totalclock + 3;
    }

    switch (whitecol)
    {
    case 1:  // Shadow Warrior
        col = whitecol+mousecol;
        break;
    case 31: // Duke Nukem 3D
        col = whitecol-mousecol;
        break;
    default:
        col = whitecol;
        break;
    }

    if (col != whitecol)
    {
        for (i=((xdim > 640)?3:2);i<=((xdim > 640)?7:3);i++)
        {
            plotpixel(searchx+i,searchy,col);
            plotpixel(searchx-i,searchy,col);
            plotpixel(searchx,searchy-i,col);
            plotpixel(searchx,searchy+i,col);
        }
        for (i=1;i<=((xdim > 640)?2:1);i++)
        {
            plotpixel(searchx+i,searchy,whitecol);
            plotpixel(searchx-i,searchy,whitecol);
            plotpixel(searchx,searchy-i,whitecol);
            plotpixel(searchx,searchy+i,whitecol);
        }
        i=((xdim > 640)?8:4);
        plotpixel(searchx+i,searchy,0);
        plotpixel(searchx-i,searchy,0);
        plotpixel(searchx,searchy-i,0);
        plotpixel(searchx,searchy+i,0);
    }

    if (xdim > 640)
    {
        for (i=1;i<=4;i++)
        {
            plotpixel(searchx+i,searchy,whitecol);
            plotpixel(searchx-i,searchy,whitecol);
            plotpixel(searchx,searchy-i,whitecol);
            plotpixel(searchx,searchy+i,whitecol);
        }
    }
}

static int AskIfSure(void)
{
    int retval=1;

    begindrawing(); //{{{
    printext256(0,0,whitecol,0,"Are you sure you want to proceed?",0);
    enddrawing();   //}}}

    showframe(1);

    while ((keystatus[1]|keystatus[0x1c]|keystatus[0x39]|keystatus[0x31]) == 0)
    {
        if (handleevents())
        {
            if (quitevent)
            {
                retval = 1;
                break;
            }
        }
        idle();
        if (keystatus[0x15] != 0)
        {
            keystatus[0x15] = 0;
            retval = 0;
            break;
        }
    }
    while (keystatus[1])
    {
        keystatus[1] = 0;
        retval = 1;
        break;
    }
    return(retval);
}

static int IsValidTile(const int idTile)
{
    int bValid = 0;

    if ((idTile >= 0) && (idTile < MAXTILES))
    {
        if ((tilesizx[idTile] != 0) && (tilesizy[idTile] != 0))
        {
            bValid = 1;
        }
    }

    return bValid;
}

static int SelectAllTiles(int iCurrentTile)
{
    int i;

    if (iCurrentTile < localartlookupnum)
    {
        iCurrentTile = localartlookup[iCurrentTile];
    }
    else
    {
        iCurrentTile = 0;
    }

    localartlookupnum = MAXTILES;

    for (i = 0; i < MAXTILES; i++)
    {
        localartlookup[i] = i;
        localartfreq[i] = 0;
    }

    return iCurrentTile;
}

static int OnGotoTile(int iTile);
static int OnSelectTile(int iTile);
static int s_Zoom = INITIAL_ZOOM;
static int s_TileZoom = 1;

static int DrawTiles(int iTopLeft, int iSelected, int nXTiles, int nYTiles, int TileDim);


static int m32gettile(int idInitialTile)
{
    int gap, temp;
    int nXTiles, nYTiles, nDisplayedTiles;
    int i;
    int iTile, iTopLeftTile;
    int idSelectedTile;

// Enable following line for testing. I couldn't work out how to change vidmode on the fly
// s_Zoom = NUM_ZOOMS - 1;

    if (idInitialTile < 0)
    {
        idInitialTile = 0;
    }
    else if (idInitialTile >= MAXTILES)
    {
        idInitialTile = MAXTILES - 1;
    }

    // Ensure zoom not to big (which can happen if display size
    //   changes whilst Mapster is running)
    do
    {
        nXTiles = xdim / ZoomToThumbSize[s_Zoom];
        nYTiles = ydim / ZoomToThumbSize[s_Zoom];
        nDisplayedTiles  = nXTiles * nYTiles;

        if (!nDisplayedTiles)
        {
            // Eh-up, resolution changed since we were last displaying tiles.
            s_Zoom--;
        }
    }
    while (!nDisplayedTiles);

    keystatus[0x2F] = 0;

    for (i = 0; i < MAXTILES; i++)
    {
        localartfreq[i] = 0;

        localartlookup[i] = i;
    }

    iTile = idSelectedTile = idInitialTile;

    switch (searchstat)
    {
    case 0 :
        for (i = 0; i < numwalls; i++)
        {
            localartfreq[ wall[i].picnum ]++;
        }
        break;

    case 1 :
    case 2 :
        for (i = 0; i < numsectors; i++)
        {
            localartfreq[ sector[i].ceilingpicnum ]++;
            localartfreq[ sector[i].floorpicnum ]++;
        }
        break;

    case 3 :
        for (i=0;i<MAXSPRITES;i++)
        {
            if (sprite[i].statnum < MAXSTATUS)
            {
                localartfreq[ sprite[i].picnum ]++;
            }
        }
        break;

    case 4 :
        for (i = 0; i < numwalls; i++)
        {
            localartfreq[ wall[i].overpicnum ]++;
        }
        break;

    default :
        break;
    }


    //
    //	Sort tiles into frequency order
    //

    gap = MAXTILES / 2;

    do
    {
        for (i = 0; i < MAXTILES-gap; i++)
        {
            temp = i;

            while ((localartfreq[temp] < localartfreq[temp+gap]) && (temp >= 0))
            {
                int tempint;

                tempint = localartfreq[temp];
                localartfreq[temp] = localartfreq[temp+gap];
                localartfreq[temp+gap] = tempint;

                tempint = localartlookup[temp];
                localartlookup[temp] = localartlookup[temp+gap];
                localartlookup[temp+gap] = tempint;

                if (iTile == temp)
                {
                    iTile = temp + gap;
                }
                else if (iTile == temp + gap)
                {
                    iTile = temp;
                }

                temp -= gap;
            }
        }
        gap >>= 1;
    }
    while (gap > 0);

    //
    // Set up count of number of used tiles
    //

    localartlookupnum = 0;
    while (localartfreq[localartlookupnum] > 0)
    {
        localartlookupnum++;
    }

    //
    // Check : If no tiles used at all then switch to displaying all tiles
    //

    if (!localartfreq[0])
    {
        localartlookupnum = MAXTILES;

        for (i = 0; i < MAXTILES; i++)
        {
            localartlookup[i] = i;
            localartfreq[i] = 0; // Terrible bodge : zero tilefreq's not displayed in tile view. Still, when in Rome ... :-)
        }

        iTile = idInitialTile;
    }

    //
    //
    //

    iTopLeftTile = iTile - (iTile % nXTiles);

    if (iTopLeftTile < 0)
    {
        iTopLeftTile = 0;
    }

    if (iTopLeftTile > MAXTILES-nDisplayedTiles)
    {
        iTopLeftTile = MAXTILES-nDisplayedTiles;
    }



    ////////////////////////////////
    // Start of key handling code //
    ////////////////////////////////

    while ((keystatus[0x1c]|keystatus[1]) == 0)	// <- Presumably one of these is escape key ???
    {
        DrawTiles(iTopLeftTile, iTile, nXTiles, nYTiles, ZoomToThumbSize[s_Zoom]);

        if (handleevents())
        {
            if (quitevent) quitevent = 0;
        }
        idle();

        // These two lines are so obvious I don't need to comment them ...;-)
        synctics = totalclock-lockclock;
        lockclock += synctics;

        // Zoom in / out using numeric key pad's / and * keys
        if (((keystatus[0xb5] > 0) && ((unsigned)s_Zoom < NUM_ZOOMS-1))
                || ((keystatus[0x37] > 0) && (s_Zoom > 0)))
        {
            if (keystatus[0xb5])
            {
                keystatus[0xb5] = 0;

                // Watch out : If editor window is small, then the next zoom level
                //  might get so large that even one tile might not fit !
                if ((ZoomToThumbSize[s_Zoom+1] <= xdim)
                        && (ZoomToThumbSize[s_Zoom+1] <= ydim))
                {
                    // Phew, plenty of room.
                    s_Zoom++;
                }
            }
            else
            {
                keystatus[0x37] = 0;
                s_Zoom--;
            }

            // Calculate new num of tiles to display
            nXTiles = xdim / ZoomToThumbSize[s_Zoom];
            nYTiles = ydim / ZoomToThumbSize[s_Zoom];
            nDisplayedTiles  = nXTiles * nYTiles;

            // Determine if the top-left displayed tile needs to
            //   alter in order to display selected tile
            iTopLeftTile = iTile - (iTile % nXTiles);

            if (iTopLeftTile < 0)
            {
                iTopLeftTile = 0;
            }
            else if (iTopLeftTile > MAXTILES - nDisplayedTiles)
            {
                iTopLeftTile = MAXTILES - nDisplayedTiles;
            }

        }

        // LEFT KEYPRESS
        if (keystatus[0xcb] > 0)
        {
            iTile -= (iTile > 0);
            keystatus[0xcb] = 0;
        }

        // RIGHT KEYPRESS
        if (keystatus[0xcd] > 0)
        {
            iTile += (iTile < MAXTILES);
            keystatus[0xcd] = 0;
        }

        // UP KEYPRESS
        if (keystatus[0xc8] > 0)
        {
            iTile -= nXTiles;
            keystatus[0xc8] = 0;
        }

        // DOWN KEYPRESS
        if (keystatus[0xd0] > 0)
        {
            iTile += nXTiles;
            keystatus[0xd0] = 0;
        }

        // PGUP KEYPRESS
        if (keystatus[0xc9] > 0)
        {
            iTile -= nDisplayedTiles;
            keystatus[0xc9] = 0;
        }

        // PGDOWN KEYPRESS
        if (keystatus[0xd1] > 0)
        {
            iTile += nDisplayedTiles;
            keystatus[0xd1] = 0;
        }

        //
        // Ensure tilenum is within valid range
        //

        if (iTile < 0)
        {
            iTile = 0;
        }

        if (iTile >= MAXTILES)	// shouldn't this be the count of num tiles ???
        {
            iTile = MAXTILES-1;
        }

        // 'V'  KEYPRESS
        if (keystatus[KEYSC_V] > 0)
        {
            keystatus[KEYSC_V] = 0;

            iTile = SelectAllTiles(iTile);
        }

        // 'G'  KEYPRESS - Goto frame
        if (keystatus[KEYSC_G] > 0)
        {
            keystatus[KEYSC_G] = 0;

            iTile = OnGotoTile(iTile);
        }

        // 'U'  KEYPRESS : go straight to user defined art
        if (keystatus[KEYSC_U])
        {
            SelectAllTiles(iTile);

            iTile = FIRST_USER_ART_TILE;
            keystatus[KEYSC_U] = 0;
        }

        // 'A'  KEYPRESS : Go straight to start of Atomic edition's art
        if (keystatus[KEYSC_A])
        {
            SelectAllTiles(iTile);

            iTile = FIRST_ATOMIC_TILE;
            keystatus[KEYSC_A] = 0;
        }

        // 'T' KEYPRESS = Select from pre-defined tileset
        if (keystatus[KEYSC_T])
        {
            keystatus[KEYSC_T] = 0;

            iTile = OnSelectTile(iTile);
        }

        // 'E'  KEYPRESS : Go straight to start of extended art
        if (keystatus[KEYSC_E])
        {
            SelectAllTiles(iTile);

            if (iTile == FIRST_EXTENDED_TILE)
                iTile = SECOND_EXTENDED_TILE;
            else iTile = FIRST_EXTENDED_TILE;

            keystatus[KEYSC_E] = 0;
        }

        if (keystatus[KEYSC_Z])
        {
            s_TileZoom = !s_TileZoom;
            keystatus[KEYSC_Z] = 0;
        }

        //
        //	Adjust top-left to ensure tilenum is within displayed range of tiles
        //

        while (iTile < iTopLeftTile)
        {
            iTopLeftTile -= nXTiles;
        }

        while (iTile >= iTopLeftTile + nDisplayedTiles)
        {
            iTopLeftTile += nXTiles;
        }

        if (iTopLeftTile < 0)
        {
            iTopLeftTile = 0;
        }

        if (iTopLeftTile > MAXTILES - nDisplayedTiles)
        {
            iTopLeftTile = MAXTILES - nDisplayedTiles;
        }

        if (keystatus[0x1c] == 0)	// uh ? Not escape key ?
        {
            idSelectedTile = idInitialTile;
        }
        else
        {
            if (iTile < localartlookupnum)
            {
                // Convert tile num from index to actual tile num
                idSelectedTile = localartlookup[iTile];

                // Check : if invalid tile selected, return original tile num
                if (!IsValidTile(idSelectedTile))
                {
                    idSelectedTile = idInitialTile;
                }
            }
            else
            {
                idSelectedTile = idInitialTile;
            }
        }
    }

    keystatus[0x1] = 0;
    keystatus[0x1c] = 0;

    return(idSelectedTile);

}

// Dir = 0 (zoom out) or 1 (zoom in)
//void OnZoomInOut( int *pZoom, int Dir /*0*/ )
//{
//}

static int OnGotoTile(int iTile)
{
    int iTemp, iNewTile;
    char ch;
    char szTemp[128];

    //Automatically press 'V'
    iTile = SelectAllTiles(iTile);

    bflushchars();

    iNewTile = iTemp = 0; //iTile; //PK

    while (keystatus[1] == 0)
    {
        if (handleevents())
        {
            if (quitevent) quitevent = 0;
        }
        idle();

        ch = bgetchar();

        Bsprintf(szTemp, "Goto tile: %d_ ", iNewTile);
        printext256(0, 0, whitecol, 0, szTemp, 0);
        showframe(1);

        if (ch >= '0' && ch <= '9')
        {
            iTemp = (iNewTile*10) + (ch-'0');
            if (iTemp < MAXTILES)
            {
                iNewTile = iTemp;
            }
        }
        else if (ch == 8)
        {
            iNewTile /= 10;
        }
        else if (ch == 13)
        {
            iTile = iNewTile;
            break;
        }
    }

    clearkeys();

    return iTile;
}

static int LoadTileSet(const int idCurrentTile, const int *pIds, const int nIds)
{
    int iNewTile = 0;
    int i;

    localartlookupnum = nIds;

    for (i = 0; i < localartlookupnum; i++)
    {
        localartlookup[i] = pIds[i];
        // REM : Could we still utilise localartfreq[] to mark
        //  which tiles are currently used in the map ? Set to 0xFFFF perhaps ?
        localartfreq[i] = 0;

        if (idCurrentTile == pIds[i])
        {
            iNewTile = i;
        }
    }

    return iNewTile;

}

static int OnSelectTile(int iTile)
{
    int bDone = 0;
    int i;
    char ch;

    for (i = 0; (unsigned)i < tile_groups; i++)
    {
        if (s_TileGroups[i].pIds != NULL)
            break;
    }

    if ((unsigned)i == tile_groups) // no tile groups
        return (iTile);

    SelectAllTiles(iTile);

    bflushchars();

    begindrawing();
    setpolymost2dview();
    clearview(0);

    //
    // Display the description strings for each available tile group
    //

    for (i = 0; (unsigned)i < tile_groups; i++)
    {
        if (s_TileGroups[i].szText != NULL)
        {
            if ((i+2)*16 > ydimgame) break;
            Bsprintf(tempbuf,"(%c) %s",s_TileGroups[i].key1,s_TileGroups[i].szText);
            printext256(10L, (i+1)*16, whitecol, -1, tempbuf, 0);
        }
    }
    showframe(1);

    //
    //	Await appropriate selection keypress.
    //

    bDone = 0;

    while (keystatus[1] == 0 && (!bDone))
    {
        if (handleevents())
        {
            if (quitevent) quitevent = 0;
        }
        idle();

        ch = bgetchar();

        for (i = 0; (unsigned)i < tile_groups; i++)
        {
            if (s_TileGroups[i].pIds != NULL && s_TileGroups[i].key1)
                if ((ch == s_TileGroups[i].key1) || (ch == s_TileGroups[i].key2))
                {
                    iTile = LoadTileSet(iTile, s_TileGroups[i].pIds, s_TileGroups[i].nIds);
                    bDone = 1;
                }
        }
    }

    enddrawing();
    showframe(1);

    clearkeys();

    return iTile;
}

const char * GetTilePixels(const int idTile)
{
    char *pPixelData = 0;

    if ((idTile >= 0) && (idTile < MAXTILES))
    {
        if (!waloff[idTile])
        {
            loadtile(idTile);
        }

        if (IsValidTile(idTile))
        {
            pPixelData = (char *)waloff[idTile];
        }
    }

    return pPixelData;
}

#define PIXELS_PER_DISPLAY_LINE ylookup[1]

static int DrawTiles(int iTopLeft, int iSelected, int nXTiles, int nYTiles, int TileDim)
{
    int XTile, YTile;
    int iTile, idTile;
    int XBox, YBox;
    int XPos, YPos;
    int XOffset, YOffset;
    int i;
    const char * pRawPixels;
    int TileSizeX, TileSizeY;
    int DivInc,MulInc;
    char *pScreen;
    char szT[128];

    begindrawing();

    setpolymost2dview();

    clearview(0);

    for (YTile = 0; YTile < nYTiles; YTile++)
    {
        for (XTile = 0; XTile < nXTiles; XTile++)
        {
            iTile = iTopLeft + XTile + (YTile * nXTiles);

            if (iTile < localartlookupnum)
            {
                idTile = localartlookup[ iTile ];

                // Get pointer to tile's raw pixel data
                pRawPixels = GetTilePixels(idTile);

                if (pRawPixels != NULL)
                {
                    XPos = XTile * TileDim;
                    YPos = YTile * TileDim;

                    if (polymost_drawtilescreen(XPos, YPos, idTile, TileDim, s_TileZoom))
                    {
                        TileSizeX = tilesizx[ idTile ];
                        TileSizeY = tilesizy[ idTile ];

                        DivInc = 1;
                        MulInc = 1;

                        while ((TileSizeX/DivInc > TileDim)
                                || (TileSizeY/DivInc) > TileDim)
                        {
                            DivInc++;
                        }

                        if (DivInc == 1 && s_TileZoom)
                        {
                            while (((TileSizeX*(MulInc+1)) <= TileDim)
                                    && ((TileSizeY*(MulInc+1)) <= TileDim))
                            {
                                MulInc++;
                            }
                        }

                        TileSizeX = (TileSizeX / DivInc) * MulInc;
                        TileSizeY = (TileSizeY / DivInc) * MulInc;

                        pScreen = (char *)ylookup[YPos]+XPos+frameplace;

                        for (YOffset = 0; YOffset < TileSizeY; YOffset++)
                        {
                            for (XOffset = 0; XOffset < TileSizeX; XOffset++)
                            {
                                pScreen[XOffset] = pRawPixels[((YOffset * DivInc) / MulInc) + (((XOffset * DivInc) / MulInc) * tilesizy[idTile])];
                            }

                            pScreen += PIXELS_PER_DISPLAY_LINE;
                        }
                    }
                    if (localartfreq[iTile] != 0)
                    {
                        Bsprintf(szT, "%d", localartfreq[iTile]);
                        printext256(XPos, YPos, whitecol, -1, szT, 1);
                    }
                }
            }
        }
    }

    //
    // Draw white box around currently selected tile
    //

    XBox = (iSelected % nXTiles) * TileDim;
    YBox = ((iSelected - (iSelected % nXTiles) - iTopLeft) / nXTiles) * TileDim;

    for (i = 0; i < TileDim; i++)
    {
        plotpixel(XBox+i,         YBox,           whitecol);
        plotpixel(XBox+i,         YBox + TileDim, whitecol);
        plotpixel(XBox,           YBox + i,       whitecol);
        plotpixel(XBox + TileDim, YBox + i,       whitecol);
    }

    idTile = localartlookup[ iSelected ];

    Bsprintf(szT, "%d" , idTile);
    printext256(0L, ydim-8, whitecol, -1, szT, 0);
    printext256(xdim-(Bstrlen(names[idTile])<<3),ydim-8,whitecol,-1,names[idTile],0);

    Bsprintf(szT,"%dx%d",tilesizx[idTile],tilesizy[idTile]);
    printext256(xdim>>2,ydim-8,whitecol,-1,szT,0);

    Bsprintf(szT,"%d, %d",(picanm[idTile]>>8)&0xFF,(picanm[idTile]>>16)&0xFF);
    printext256((xdim>>2)+100,ydim-8,whitecol,-1,szT,0);

    enddrawing();
    showframe(1);

    return(0);

}

extern char unrealedlook; //PK

static void Keys3d(void)
{
    int i,count,rate,nexti;
    int j, k, tempint = 0, hiz, loz;
    int hitx, hity, hitz, hihit, lohit;
    char smooshyalign=0, repeatpanalign=0, buffer[80];
    short startwall, endwall, dasector, hitsect, hitwall, hitsprite, statnum=0;

    /* start Mapster32 */

    if (sidemode != 0)
    {
        setviewback();
        rotatesprite(320<<15,200<<15,65536,(horiz-100)<<2,4094,0,0,2+4,0,0,0,0);
        lockbyte4094 = 0;
        searchx = ydim-1-searchx;
        searchx ^= searchy;
        searchy ^= searchx;
        searchx ^= searchy;

        //      overwritesprite(160L,170L,1153,0,1+2,0);
        rotatesprite(160<<16,170<<16,65536,(100-horiz+1024)<<3,1153,0,0,2,0,0,0,0);

    }

    if (usedcount && !helpon)
    {
        if (searchstat!=3)
        {
            count=0;
            for (i=0;i<numwalls;i++)
            {
                if (wall[i].picnum == temppicnum) count++;
                if (wall[i].overpicnum == temppicnum) count++;
                if (sector[i].ceilingpicnum == temppicnum) count++;
                if (sector[i].floorpicnum == temppicnum) count++;
            }
        }

        if (searchstat==3)
        {
            count=0;
            statnum=0;
            i = headspritestat[statnum];
            while (i != -1)
            {
                nexti = nextspritestat[i];
                if (sprite[i].picnum == temppicnum) count++;
                i = nexti;
            }
        }

        if (tabgraphic == 1)
            rotatesprite((44+(tilesizx[temppicnum]>>3))<<16,(114)<<16,32768,0,temppicnum,tempshade,temppal,2,0L,0L,xdim-1L,ydim-1L);
        else if (tabgraphic == 2)
            rotatesprite((44+(tilesizx[temppicnum]>>3))<<16,(114)<<16,16384,0,temppicnum,tempshade,temppal,2,0L,0L,xdim-1L,ydim-1L);

        begindrawing();
        j = xdimgame>640?0:1;
        i = (ydimgame>>6)+(j<<2);
        printext256((xdimgame>>6)+2,ydimgame-(ydimgame>>1)+2,0,-1,"Clipboard",j);
        printext256((xdimgame>>6),ydimgame-(ydimgame>>1),whitecol,-1,"Clipboard",j);
        Bsprintf(tempbuf,"Pic: %d %s",temppicnum,names[temppicnum]);
        printext256((xdimgame>>6)+2,ydimgame-(ydimgame>>1)+2+i+i,0,-1,tempbuf,j);
        printext256((xdimgame>>6),ydimgame-(ydimgame>>1)+i+i,whitecol,-1,tempbuf,j);
        Bsprintf(tempbuf,"Shd: %d",tempshade);
        printext256((xdimgame>>6)+2,ydimgame-(ydimgame>>1)+2+i+i+i,0,-1,tempbuf,j);
        printext256((xdimgame>>6),ydimgame-(ydimgame>>1)+i+i+i,whitecol,-1,tempbuf,j);
        Bsprintf(tempbuf,"Pal: %d",temppal);
        printext256((xdimgame>>6)+2,ydimgame-(ydimgame>>1)+2+i+i+i+i,0,-1,tempbuf,j);
        printext256((xdimgame>>6),ydimgame-(ydimgame>>1)+i+i+i+i,whitecol,-1,tempbuf,j);
        Bsprintf(tempbuf,"Cst: %d",tempcstat);
        printext256((xdimgame>>6)+2,ydimgame-(ydimgame>>1)+2+i+i+i+i+i,0,-1,tempbuf,j);
        printext256((xdimgame>>6),ydimgame-(ydimgame>>1)+i+i+i+i+i,whitecol,-1,tempbuf,j);
        Bsprintf(tempbuf,"Lot: %d",templotag);
        printext256((xdimgame>>6)+2,ydimgame-(ydimgame>>1)+2+i+i+i+i+i+i,0,-1,tempbuf,j);
        printext256((xdimgame>>6),ydimgame-(ydimgame>>1)+i+i+i+i+i+i,whitecol,-1,tempbuf,j);
        Bsprintf(tempbuf,"Hit: %d",temphitag);
        printext256((xdimgame>>6)+2,ydimgame-(ydimgame>>1)+2+i+i+i+i+i+i+i,0,-1,tempbuf,j);
        printext256((xdimgame>>6),ydimgame-(ydimgame>>1)+i+i+i+i+i+i+i,whitecol,-1,tempbuf,j);
        Bsprintf(tempbuf,"Ext: %d",tempextra);
        printext256((xdimgame>>6)+2,ydimgame-(ydimgame>>1)+2+i+i+i+i+i+i+i+i,0,-1,tempbuf,j);
        printext256((xdimgame>>6),ydimgame-(ydimgame>>1)+i+i+i+i+i+i+i+i,whitecol,-1,tempbuf,j);
        enddrawing();
    }// end if usedcount

    if ((totalclock > (lastmessagetime + 120*3)))
    {
        char msgbuf[2048];
        switch (searchstat)
        {
        case 0:
        case 4:
        {
            int dax, day, dist;
            dax = wall[searchwall].x-wall[wall[searchwall].point2].x;
            day = wall[searchwall].y-wall[wall[searchwall].point2].y;
            dist = ksqrt(dax*dax+day*day);
            Bsprintf(msgbuf,"Wall %d: length:%d lo:%d hi:%d ex:%d",searchwall,dist,wall[searchwall].lotag,wall[searchwall].hitag,wall[searchwall].extra);
            _message(msgbuf);
            break;
        }
        case 1:
            Bsprintf(msgbuf,"Sector %d ceiling: lo:%s hi:%d ex:%d",searchsector,ExtGetSectorCaption(searchsector),sector[searchsector].hitag,sector[searchsector].extra);
            _message(msgbuf);
            break;
        case 2:
            Bsprintf(msgbuf,"Sector %d floor: lo:%s hi:%d ex:%d",searchsector,ExtGetSectorCaption(searchsector),sector[searchsector].hitag,sector[searchsector].extra);
            _message(msgbuf);
            break;
        case 3:
        {
            if (strlen(names[sprite[searchwall].picnum]) > 0)
            {
                if (sprite[searchwall].picnum==SECTOREFFECTOR)
                    Bsprintf(msgbuf,"Sprite %d %s: lo:%d hi:%d ex:%d",searchwall,SectorEffectorText(searchwall),sprite[searchwall].lotag,sprite[searchwall].hitag,sprite[searchwall].extra);
                else Bsprintf(msgbuf,"Sprite %d %s: lo:%d hi:%d ex:%d",searchwall,names[sprite[searchwall].picnum],sprite[searchwall].lotag,sprite[searchwall].hitag,sprite[searchwall].extra);
            }
            else Bsprintf(msgbuf,"Sprite %d picnum %d: lo:%d hi:%d ex:%d",searchwall,sprite[searchwall].picnum,sprite[searchwall].lotag,sprite[searchwall].hitag,sprite[searchwall].extra);
            _message(msgbuf);
            break;
        }
        }
    }

    if (keystatus[KEYSC_QUOTE]==1 && keystatus[0x2f]==1) // ' V
    {
        keystatus[0x2f] = 0;
        switch (searchstat)
        {
        case 1:
        case 2:
            Bstrcpy(tempbuf,"Sector visibility: ");
            sector[searchsector].visibility =
                getnumber256(tempbuf,sector[searchsector].visibility,256L,0);
            break;
        }
    }

    if (keystatus[KEYSC_SEMI] && keystatus[KEYSC_V])   // ; V
    {
        short currsector;
        unsigned char visval;

        keystatus[KEYSC_V] = 0;

        if (highlightsectorcnt == -1)
        {
            message("You didn't select any sectors!");
            return;
        }
        visval = (unsigned char)getnumber256("Visibility of selected sectors: ",sector[searchsector].visibility,256L,0);
        if (AskIfSure()) return;

        for (i = 0; i < highlightsectorcnt; i++)
        {
            currsector = highlightsector[i];
            sector[currsector].visibility = visval;
        }
        message("Visibility changed on all selected sectors");
    }

    if (keystatus[KEYSC_V] > 0)  //V
    {
        if (searchstat == 0) tempint = wall[searchwall].picnum;
        if (searchstat == 1) tempint = sector[searchsector].ceilingpicnum;
        if (searchstat == 2) tempint = sector[searchsector].floorpicnum;
        if (searchstat == 3) tempint = sprite[searchwall].picnum;
        if (searchstat == 4) tempint = wall[searchwall].overpicnum;
        tempint = m32gettile(tempint);
        if (searchstat == 0) wall[searchwall].picnum = tempint;
        if (searchstat == 1) sector[searchsector].ceilingpicnum = tempint;
        if (searchstat == 2) sector[searchsector].floorpicnum = tempint;
        if (searchstat == 3) sprite[searchwall].picnum = tempint;
        if (searchstat == 4)
        {
            wall[searchwall].overpicnum = tempint;
            if (wall[searchwall].nextwall >= 0)
                wall[wall[searchwall].nextwall].overpicnum = tempint;
        }
        asksave = 1;
        keystatus[0x2f] = 0;
    }

    if (keystatus[0x04] > 0)  /* 3 (toggle floor-over-floor (cduke3d only) */
    {
        floor_over_floor = !floor_over_floor;
        //        if (!floor_over_floor) ResetFOFSize();
        keystatus[0x04] = 0;
    }

    if (keystatus[KEYSC_F3])
    {
        mlook = 1-mlook;
        Bsprintf(tempbuf,"Mouselook: %d",mlook);
        message(tempbuf);
        keystatus[KEYSC_F3] = 0;
    }

// PK
    if (keystatus[KEYSC_F5])
    {
        unrealedlook = 1-unrealedlook;
        Bsprintf(tempbuf,"UnrealEd mouse navigation: %d",unrealedlook);
        message(tempbuf);
        keystatus[KEYSC_F5] = 0;
    }

    if (keystatus[KEYSC_QUOTE]==1 && keystatus[0x0e]==1) // ' del
    {
        keystatus[0x0e] = 0;
        switch (searchstat)
        {
        case 0:
        case 4:
            wall[searchwall].cstat = 0;
            break;
            //            case 1: case 2: sector[searchsector].cstat = 0; break;
        case 3:
            sprite[searchwall].cstat = 0;
            break;
        }
    }

    // 'P - Will copy palette to all sectors highlighted with R-Alt key
    if (keystatus[KEYSC_QUOTE] && keystatus[KEYSC_P])   // ' P
    {
        short w, start_wall, end_wall, currsector;
        signed char pal[4];

        keystatus[KEYSC_P] = 0;

        if (highlightsectorcnt == -1)
        {
            message("You didn't select any sectors!");
            return;
        }

        pal[0] = getnumber256("Ceiling palette: ",-1,MAXPALOOKUPS,1);
        pal[1] = getnumber256("Floor palette: ",-1,MAXPALOOKUPS,1);
        pal[2] = getnumber256("Wall palette: ",-1,MAXPALOOKUPS,1);
        pal[3] = getnumber256("Sprite palette: ",-1,MAXPALOOKUPS,1);
        if (AskIfSure()) return;

        for (i = 0; i < highlightsectorcnt; i++)
        {
            currsector = highlightsector[i];
            if (pal[0] > -1)
                sector[currsector].ceilingpal = pal[0];
            if (pal[1] > -1)
                sector[currsector].floorpal = pal[1];
            // Do all the walls in the sector
            start_wall = sector[currsector].wallptr;
            end_wall = start_wall + sector[currsector].wallnum;

            if (pal[2] > -1)
                for (w = start_wall; w < end_wall; w++)
                {
                    wall[w].pal = pal[2];
                }
            if (pal[3] > -1)
                for (k=0;k<highlightsectorcnt;k++)
                {
                    w = headspritesect[highlightsector[k]];
                    while (w >= 0)
                    {
                        j = nextspritesect[w];
                        sprite[w].pal = pal[3];
                        w = j;
                    }
                }
        }
        message("Palettes changed");
    }

    // ;P - Will copy palette to all sectors & sprites within highlighted with R-Alt key
    if (keystatus[KEYSC_SEMI] && keystatus[KEYSC_P])   // ; P
    {
        short w, start_wall, end_wall, currsector;
        unsigned char pal;

        keystatus[KEYSC_P] = 0;

        if (highlightsectorcnt == -1)
        {
            message("You didn't select any sectors!");
            return;
        }

        pal = (unsigned char)getnumber256("Global palette: ",0,MAXPALOOKUPS,0);
        if (AskIfSure()) return;

        for (i = 0; i < highlightsectorcnt; i++)
        {
            currsector = highlightsector[i];
            sector[currsector].ceilingpal = pal;
            sector[currsector].floorpal = pal;
            // Do all the walls in the sector
            start_wall = sector[currsector].wallptr;
            end_wall = start_wall + sector[currsector].wallnum;
            for (w = start_wall; w < end_wall; w++)
            {
                wall[w].pal = pal;
            }
            for (k=0;k<highlightsectorcnt;k++)
            {
                w = headspritesect[highlightsector[k]];
                while (w >= 0)
                {
                    j = nextspritesect[w];
                    sprite[w].pal = pal;
                    w = j;
                }
            }
        }
        message("Palettes changed");
    }

    if (keystatus[0xd3] > 0)
    {
        if (searchstat == 3)
        {
            deletesprite(searchwall);
            updatenumsprites();
            Bsprintf(tempbuf,"Sprite %d deleted",searchwall);
            message(tempbuf);
            asksave = 1;
        }
        keystatus[0xd3] = 0;
    }

    if (keystatus[KEYSC_F6] > 0)  //F6
    {
        keystatus[KEYSC_F6] = 0;
        autospritehelp=!autospritehelp;
        Bsprintf(tempbuf,"Automatic SECTOREFFECTOR help: %d",autospritehelp);
        message(tempbuf);
    }
    if (keystatus[KEYSC_F7] > 0)  //F7
    {
        keystatus[KEYSC_F7] = 0;
        autosecthelp=!autosecthelp;
        Bsprintf(tempbuf,"Automatic sector tag help: %d",autosecthelp);
        message(tempbuf);

    }

    if ((searchstat == 3) && (sprite[searchwall].picnum==SECTOREFFECTOR))
        if (autospritehelp && helpon==0) Show3dText("sehelp.hlp");

    if (searchstat == 1 || searchstat == 2)
        if (autosecthelp && helpon==0) Show3dText("sthelp.hlp");



    if (keystatus[0x33] > 0) // , Search & fix panning to the left (3D)
    {
        if (searchstat == 3)
        {
            i = searchwall;
            if ((keystatus[0x2a]|keystatus[0x36]) > 0)
                sprite[i].ang = ((sprite[i].ang+2048-1)&2047);
            else
            {
                sprite[i].ang = ((sprite[i].ang+2048-128)&2047);
                keystatus[0x33] = 0;
            }
            Bsprintf(tempbuf,"Sprite %d angle: %d",i,sprite[i].ang);
            message(tempbuf);
        }
    }
    if (keystatus[0x34] > 0) // . Search & fix panning to the right (3D)
    {
        if ((searchstat == 0) || (searchstat == 4))
        {
            AutoAlignWalls((int)searchwall,0L);
            keystatus[0x34] = 0;
        }
        if (searchstat == 3)
        {
            i = searchwall;
            if ((keystatus[0x2a]|keystatus[0x36]) > 0)
                sprite[i].ang = ((sprite[i].ang+2048+1)&2047);
            else
            {
                sprite[i].ang = ((sprite[i].ang+2048+128)&2047);
                keystatus[0x34] = 0;
            }
            Bsprintf(tempbuf,"Sprite %d angle: %d",i,sprite[i].ang);
            message(tempbuf);
        }
    }

    if ((keystatus[KEYSC_L] > 0) && (keystatus[KEYSC_QUOTE] > 0)) // ' L
    {
        switch (searchstat)
        {
        case 1:
            strcpy(tempbuf,"Sector ceilingz: ");
            sector[searchsector].ceilingz = getnumber256(tempbuf,sector[searchsector].ceilingz,8388608,1);
            if (!(sector[searchsector].ceilingstat&2))
                sector[searchsector].ceilingheinum = 0;

            strcpy(tempbuf,"Sector ceiling slope: ");
            sector[searchsector].ceilingheinum = getnumber256(tempbuf,sector[searchsector].ceilingheinum,65536,1);
            break;
        case 2:
            strcpy(tempbuf,"Sector floorz: ");
            sector[searchsector].floorz = getnumber256(tempbuf,sector[searchsector].floorz,8388608,1);
            if (!(sector[searchsector].floorstat&2))
                sector[searchsector].floorheinum = 0;
            strcpy(tempbuf,"Sector floor slope: ");
            sector[searchsector].floorheinum = getnumber256(tempbuf,sector[searchsector].floorheinum,65536,1);
            break;
        case 3:
            strcpy(tempbuf,"Sprite x: ");
            sprite[searchwall].x = getnumber256(tempbuf,sprite[searchwall].x,131072,1);
            strcpy(tempbuf,"Sprite y: ");
            sprite[searchwall].y = getnumber256(tempbuf,sprite[searchwall].y,131072,1);
            strcpy(tempbuf,"Sprite z: ");
            sprite[searchwall].z = getnumber256(tempbuf,sprite[searchwall].z,8388608,1);
            strcpy(tempbuf,"Sprite angle: ");
            sprite[searchwall].ang = getnumber256(tempbuf,sprite[searchwall].ang,2048L,0);
            break;
        }
        if (sector[searchsector].ceilingheinum == 0)
            sector[searchsector].ceilingstat &= ~2;
        else
            sector[searchsector].ceilingstat |= 2;

        if (sector[searchsector].floorheinum == 0)
            sector[searchsector].floorstat &= ~2;
        else
            sector[searchsector].floorstat |= 2;
        asksave = 1;
        keystatus[KEYSC_L] = 0;
    }

    getzrange(posx,posy,posz,cursectnum,&hiz,&hihit,&loz,&lohit,128L,CLIPMASK0);

    if (keystatus[KEYSC_CAPS] > 0 || ((keystatus[KEYSC_Z] > 0) && (keystatus[KEYSC_QUOTE] > 0)))
    {
        zmode++;
        if (zmode == 3) zmode = 0;
        else if (zmode == 1) zlock = (loz-posz)&0xfffffc00;
        if (zmode == 0) message("Zmode = Gravity");
        else if (zmode == 1) message("Zmode = Locked/Sector");
        else if (zmode == 2) message("Zmode = Locked/Free");
        keystatus[KEYSC_CAPS] = keystatus[KEYSC_Z] = 0;
    }

    if (keystatus[KEYSC_M] > 0 && (keystatus[KEYSC_QUOTE]) > 0)  // M
    {
        switch (searchstat)
        {
        case 0:
        case 4:
            strcpy(buffer,"Wall extra: ");
            wall[searchwall].extra = getnumber256(buffer,(int)wall[searchwall].extra,65536L,1);
            break;
        case 1:
            strcpy(buffer,"Sector extra: ");
            sector[searchsector].extra = getnumber256(buffer,(int)sector[searchsector].extra,65536L,1);
            break;
        case 2:
            strcpy(buffer,"Sector extra: ");
            sector[searchsector].extra = getnumber256(buffer,(int)sector[searchsector].extra,65536L,1);
            break;
        case 3:
            strcpy(buffer,"Sprite extra: ");
            sprite[searchwall].extra = getnumber256(buffer,(int)sprite[searchwall].extra,65536L,1);
            break;
        }
        asksave = 1;
        keystatus[KEYSC_M] = 0;
    }

    if (keystatus[KEYSC_1] > 0)  // 1 (make 1-way wall)
    {
        if (searchstat != 3)
        {
            wall[searchwall].cstat ^= 32;
            sprintf(getmessage,"Wall %d 1 side masking %s",searchwall,wall[searchwall].cstat&32?"ON":"OFF");
            message(getmessage);
            asksave = 1;
        }
        else
        {
            sprite[searchwall].cstat ^= 64;
            i = sprite[searchwall].cstat;
            if ((i&48) == 32)
            {
                sprite[searchwall].cstat &= ~8;
                if ((i&64) > 0)
                    if (posz > sprite[searchwall].z)
                        sprite[searchwall].cstat |= 8;
            }
            asksave = 1;
            sprintf(getmessage,"Sprite %d 1 sided %s",searchwall,sprite[searchwall].cstat&64?"ON":"OFF");
            message(getmessage);

        }
        keystatus[KEYSC_1] = 0;
    }
    if (keystatus[KEYSC_2] > 0)  // 2 (bottom wall swapping)
    {
        if (searchstat != 3)
        {
            wall[searchwall].cstat ^= 2;
            sprintf(getmessage,"Wall %d bottom texture swap %s",searchwall,wall[searchwall].cstat&2?"ON":"OFF");
            message(getmessage);
            asksave = 1;
        }
        keystatus[KEYSC_2] = 0;
    }
    if (keystatus[KEYSC_O] > 0)  // O (top/bottom orientation - for doors)
    {
        if ((searchstat == 0) || (searchstat == 4))
        {
            wall[searchwall].cstat ^= 4;
            asksave = 1;
        }
        if (searchstat == 3)   // O (ornament onto wall) (2D)
        {
            asksave = 1;
            i = searchwall;

            hitscan(sprite[i].x,sprite[i].y,sprite[i].z,sprite[i].sectnum,
                    sintable[(sprite[i].ang+2560+1024)&2047],
                    sintable[(sprite[i].ang+2048+1024)&2047],
                    0,
                    &hitsect,&hitwall,&hitsprite,&hitx,&hity,&hitz,CLIPMASK1);

            sprite[i].x = hitx;
            sprite[i].y = hity;
            sprite[i].z = hitz;
            changespritesect(i,hitsect);
            if (hitwall >= 0)
                sprite[i].ang = ((getangle(wall[wall[hitwall].point2].x-wall[hitwall].x,wall[wall[hitwall].point2].y-wall[hitwall].y)+512)&2047);

            //Make sure sprite's in right sector
            if (inside(sprite[i].x,sprite[i].y,sprite[i].sectnum) == 0)
            {
                j = wall[hitwall].point2;
                sprite[i].x -= ksgn(wall[j].y-wall[hitwall].y);
                sprite[i].y += ksgn(wall[j].x-wall[hitwall].x);
            }
        }
        keystatus[KEYSC_O] = 0;
    }
    if (keystatus[KEYSC_M] > 0)  // M (masking walls)
    {
        if (searchstat != 3)
        {
            i = wall[searchwall].nextwall;
            tempint = (keystatus[0x2a]|keystatus[0x36]);
            if (i >= 0)
            {
                wall[searchwall].cstat ^= 16;
                sprintf(getmessage,"Wall %d masking %s",searchwall,wall[searchwall].cstat&16?"ON":"OFF");
                message(getmessage);
                if ((wall[searchwall].cstat&16) > 0)
                {
                    wall[searchwall].cstat &= ~8;
                    if (tempint == 0)
                    {
                        wall[i].cstat |= 8;           //auto other-side flip
                        wall[i].cstat |= 16;
                        wall[i].overpicnum = wall[searchwall].overpicnum;
                    }
                }
                else
                {
                    wall[searchwall].cstat &= ~8;
                    if (tempint == 0)
                    {
                        wall[i].cstat &= ~8;         //auto other-side unflip
                        wall[i].cstat &= ~16;
                    }
                }
                wall[searchwall].cstat &= ~32;
                if (tempint == 0) wall[i].cstat &= ~32;
                asksave = 1;
            }
        }
        keystatus[KEYSC_M] = 0;
    }

    if (keystatus[KEYSC_H] > 0)  // H (hitscan sensitivity)
    {
        if ((keystatus[KEYSC_QUOTE]) > 0)
        {
            switch (searchstat)
            {
            case 0:
            case 4:
                strcpy(buffer,"Wall hitag: ");
                wall[searchwall].hitag = getnumber256(buffer,(int)wall[searchwall].hitag,65536L,0);
                break;
            case 1:
                strcpy(buffer,"Sector hitag: ");
                sector[searchsector].hitag = getnumber256(buffer,(int)sector[searchsector].hitag,65536L,0);
                break;
            case 2:
                strcpy(buffer,"Sector hitag: ");
                sector[searchsector].hitag = getnumber256(buffer,(int)sector[searchsector].hitag,65536L,0);
                break;
            case 3:
                strcpy(buffer,"Sprite hitag: ");
                sprite[searchwall].hitag = getnumber256(buffer,(int)sprite[searchwall].hitag,65536L,0);
                break;
            }
        }

        else
        {

            if (searchstat == 3)
            {
                sprite[searchwall].cstat ^= 256;
                sprintf(getmessage,"Sprite %d hitscan sensitivity %s",searchwall,sprite[searchwall].cstat&256?"ON":"OFF");
                message(getmessage);
                asksave = 1;
            }
            else
            {
                wall[searchwall].cstat ^= 64;

                if ((wall[searchwall].nextwall >= 0) && ((keystatus[0x2a]|keystatus[0x36]) == 0))
                {
                    wall[wall[searchwall].nextwall].cstat &= ~64;
                    wall[wall[searchwall].nextwall].cstat |= (wall[searchwall].cstat&64);
                }
                sprintf(getmessage,"Wall %d hitscan sensitivity %s",searchwall,wall[searchwall].cstat&64?"ON":"OFF");
                message(getmessage);

                asksave = 1;
            }
        }
        keystatus[KEYSC_H] = 0;
    }

    smooshyalign = keystatus[0x4c];
    repeatpanalign = (keystatus[0x2a]|keystatus[0x36]|(bstatus&2));

    if (mlook == 2)
        mlook = 0;

    if (!unrealedlook && (bstatus&4)) mlook = 2;

//    if (bstatus&4)
    if (bstatus&(16|32) && !(bstatus&(1|2|4)))  // PK: no btn: wheel changes shade
    {
//        if (bstatus&1)
//        {
//            mlook = 2;
//        }
        if (bstatus&32)  // -
        {
            mouseb &= ~32;
            bstatus &= ~32;
            if ((keystatus[0x38]|keystatus[0xb8]) > 0)  //ALT
            {
                if ((keystatus[0x1d]|keystatus[0x9d]) > 0)  //CTRL
                {
                    if (visibility < 16384) visibility += visibility;
                }
                else
                {
                    if ((keystatus[0x2a]|keystatus[0x36]) == 0)  //not SHIFT
                        k = 16;
                    else k = 1;

                    if (highlightsectorcnt >= 0)
                        for (i=0;i<highlightsectorcnt;i++)
                            if (highlightsector[i] == searchsector)
                            {
                                while (k > 0)
                                {
                                    for (i=0;i<highlightsectorcnt;i++)
                                    {
                                        sector[highlightsector[i]].visibility++;
                                        if (sector[highlightsector[i]].visibility == 240)
                                            sector[highlightsector[i]].visibility = 239;
                                    }
                                    k--;
                                }
                                break;
                            }
                    while (k > 0)
                    {
                        sector[searchsector].visibility++;
                        if (sector[searchsector].visibility == 240)
                            sector[searchsector].visibility = 239;
                        k--;
                    }
                    asksave = 1;
                }
            }
            else
            {
                k = 0;
                if (highlightsectorcnt >= 0)
                {
                    for (i=0;i<highlightsectorcnt;i++)
                        if (highlightsector[i] == searchsector)
                        {
                            k = 1;
                            break;
                        }
                }

                if (k == 0)
                {
                    if (searchstat == 0) wall[searchwall].shade++;
                    if (searchstat == 1) sector[searchsector].ceilingshade++;
                    if (searchstat == 2) sector[searchsector].floorshade++;
                    if (searchstat == 3) sprite[searchwall].shade++;
                    if (searchstat == 4) wall[searchwall].shade++;
                }
                else
                {
                    for (i=0;i<highlightsectorcnt;i++)
                    {
                        dasector = highlightsector[i];

                        sector[dasector].ceilingshade++;        //sector shade
                        sector[dasector].floorshade++;

                        startwall = sector[dasector].wallptr;   //wall shade
                        endwall = startwall + sector[dasector].wallnum - 1;
                        for (j=startwall;j<=endwall;j++)
                            wall[j].shade++;

                        j = headspritesect[dasector];           //sprite shade
                        while (j != -1)
                        {
                            sprite[j].shade++;
                            j = nextspritesect[j];
                        }
                    }
                }
                asksave = 1;
            }
        }
        if (bstatus&16)  // +
        {
            mouseb &= ~16;
            bstatus &= ~16;
            if ((keystatus[0x38]|keystatus[0xb8]) > 0)  //ALT
            {
                if ((keystatus[0x1d]|keystatus[0x9d]) > 0)  //CTRL
                {
                    if (visibility > 32) visibility >>= 1;
                }
                else
                {
                    if ((keystatus[0x2a]|keystatus[0x36]) == 0)
                        k = 16;
                    else k = 1;

                    if (highlightsectorcnt >= 0)
                        for (i=0;i<highlightsectorcnt;i++)
                            if (highlightsector[i] == searchsector)
                            {
                                while (k > 0)
                                {
                                    for (i=0;i<highlightsectorcnt;i++)
                                    {
                                        sector[highlightsector[i]].visibility--;
                                        if (sector[highlightsector[i]].visibility == 239)
                                            sector[highlightsector[i]].visibility = 240;
                                    }
                                    k--;
                                }
                                break;
                            }
                    while (k > 0)
                    {
                        sector[searchsector].visibility--;
                        if (sector[searchsector].visibility == 239)
                            sector[searchsector].visibility = 240;
                        k--;
                    }
                    asksave = 1;
                }
            }
            else
            {
                k = 0;
                if (highlightsectorcnt >= 0)
                {
                    for (i=0;i<highlightsectorcnt;i++)
                        if (highlightsector[i] == searchsector)
                        {
                            k = 1;
                            break;
                        }
                }

                if (k == 0)
                {
                    if (searchstat == 0) wall[searchwall].shade--;
                    if (searchstat == 1) sector[searchsector].ceilingshade--;
                    if (searchstat == 2) sector[searchsector].floorshade--;
                    if (searchstat == 3) sprite[searchwall].shade--;
                    if (searchstat == 4) wall[searchwall].shade--;
                }
                else
                {
                    for (i=0;i<highlightsectorcnt;i++)
                    {
                        dasector = highlightsector[i];

                        sector[dasector].ceilingshade--;        //sector shade
                        sector[dasector].floorshade--;

                        startwall = sector[dasector].wallptr;   //wall shade
                        endwall = startwall + sector[dasector].wallnum - 1;
                        for (j=startwall;j<=endwall;j++)
                            wall[j].shade--;

                        j = headspritesect[dasector];           //sprite shade
                        while (j != -1)
                        {
                            sprite[j].shade--;
                            j = nextspritesect[j];
                        }
                    }
                }
                asksave = 1;
            }
        }
    }

//    if ((keystatus[KEYSC_DASH]|keystatus[KEYSC_EQUAL]|((bstatus&(16|32)) && !(bstatus&2))) > 0) // mousewheel, -, and +, cycle picnum
    if (keystatus[KEYSC_DASH] | keystatus[KEYSC_EQUAL] | (bstatus&(16|32) && (bstatus&1) && !(bstatus&2)))  // PK: lmb only & mousewheel, -, and +, cycle picnum

    {
        j = i = (keystatus[KEYSC_EQUAL]|(bstatus&16))?1:-1;
        switch (searchstat)
        {
        case 0:
            while (!tilesizx[wall[searchwall].picnum]||!tilesizy[wall[searchwall].picnum]||j)
            {
                if (wall[searchwall].picnum+i >= MAXTILES) wall[searchwall].picnum = 0;
                else if (wall[searchwall].picnum+i < 0) wall[searchwall].picnum = MAXTILES-1;
                else wall[searchwall].picnum += i;
                j = 0;
            }
            break;
        case 1:
            while (!tilesizx[sector[searchsector].ceilingpicnum]||!tilesizy[sector[searchsector].ceilingpicnum]||j)
            {
                if (sector[searchsector].ceilingpicnum+i >= MAXTILES) sector[searchsector].ceilingpicnum = 0;
                else if (sector[searchsector].ceilingpicnum+i < 0) sector[searchsector].ceilingpicnum = MAXTILES-1;
                else sector[searchsector].ceilingpicnum += i;
                j = 0;
            }
            break;
        case 2:
            while (!tilesizx[sector[searchsector].floorpicnum]||!tilesizy[sector[searchsector].floorpicnum]||j)
            {
                if (sector[searchsector].floorpicnum+i >= MAXTILES) sector[searchsector].floorpicnum = 0;
                else if (sector[searchsector].floorpicnum+i < 0) sector[searchsector].floorpicnum = MAXTILES-1;
                else sector[searchsector].floorpicnum += i;
                j = 0;
            }
            break;
        case 3:
            while (!tilesizx[sprite[searchwall].picnum]||!tilesizy[sprite[searchwall].picnum]||j)
            {
                if (sprite[searchwall].picnum+i >= MAXTILES) sprite[searchwall].picnum = 0;
                else if (sprite[searchwall].picnum+i < 0) sprite[searchwall].picnum = MAXTILES-1;
                else sprite[searchwall].picnum += i;
                j = 0;
            }
            break;
        case 4:
            while (!tilesizx[wall[searchwall].overpicnum]||!tilesizy[wall[searchwall].overpicnum]||j)
            {
                if (wall[searchwall].overpicnum+i >= MAXTILES) wall[searchwall].overpicnum = 0;
                else if (wall[searchwall].overpicnum+i < 0) wall[searchwall].overpicnum = MAXTILES-1;
                else wall[searchwall].overpicnum += i;
                j = 0;
            }
            break;
        }
        asksave = 1;
        keystatus[KEYSC_DASH] = keystatus[KEYSC_EQUAL] = 0;
        mouseb &= ~(16|32);
    }

    if (keystatus[KEYSC_E] > 0)  // E (expand)
    {
        if (searchstat == 1)
        {
            sector[searchsector].ceilingstat ^= 8;
            sprintf(getmessage,"Sector %d ceiling texture expansion %s",searchsector,sector[searchsector].ceilingstat&8?"ON":"OFF");
            message(getmessage);
            asksave = 1;
        }
        if (searchstat == 2)
        {
            sector[searchsector].floorstat ^= 8;
            sprintf(getmessage,"Sector %d floor texture expansion %s",searchsector,sector[searchsector].floorstat&8?"ON":"OFF");
            message(getmessage);
            asksave = 1;
        }
        keystatus[KEYSC_E] = 0;
    }
    if (keystatus[KEYSC_R] > 0)  // R (relative alignment, rotation)
    {

        if (keystatus[KEYSC_QUOTE] > 0) // FRAMERATE TOGGLE
        {

            framerateon = !framerateon;
            if (framerateon) message("Show framerate ON");
            else message("Show framerate OFF");

        }

        else

        {
            if (searchstat == 1)
            {
                sector[searchsector].ceilingstat ^= 64;
                sprintf(getmessage,"Sector %d ceiling texture relativity %s",searchsector,sector[searchsector].ceilingstat&64?"ON":"OFF");
                message(getmessage);
                asksave = 1;
            }
            if (searchstat == 2)
            {
                sector[searchsector].floorstat ^= 64;
                sprintf(getmessage,"Sector %d floor texture relativity %s",searchsector,sector[searchsector].floorstat&64?"ON":"OFF"); //PK (was ceiling in string)
                message(getmessage);
                asksave = 1;
            }
            if (searchstat == 3)
            {
                i = sprite[searchwall].cstat;
                if ((i&48) < 32) i += 16;

                else i &= ~48;
                sprite[searchwall].cstat = i;

                if (sprite[searchwall].cstat&16)
                    sprintf(getmessage,"Sprite %d now wall aligned",searchwall);
                else if (sprite[searchwall].cstat&32)
                    sprintf(getmessage,"Sprite %d now floor aligned",searchwall);
                else
                    sprintf(getmessage,"Sprite %d now view aligned",searchwall);
                message(getmessage);
                asksave = 1;
            }
        }
        keystatus[KEYSC_R] = 0;
    }
    if (keystatus[KEYSC_F] > 0)  //F (Flip)
    {
        keystatus[KEYSC_F] = 0;
        if ((keystatus[0x38]|keystatus[0xb8]) > 0)  //ALT-F (relative alignmment flip)
        {
            if (searchstat != 3)
            {
                setfirstwall(searchsector,searchwall);
                asksave = 1;
            }
        }
        else
        {
            if ((searchstat == 0) || (searchstat == 4))
            {
                i = wall[searchwall].cstat;
                i = ((i>>3)&1)+((i>>7)&2);    //3-x,8-y
                switch (i)
                {
                case 0:
                    i = 1;
                    break;
                case 1:
                    i = 3;
                    break;
                case 2:
                    i = 0;
                    break;
                case 3:
                    i = 2;
                    break;
                }
                i = ((i&1)<<3)+((i&2)<<7);
                wall[searchwall].cstat &= ~0x0108;
                wall[searchwall].cstat |= i;
                asksave = 1;
            }
            if (searchstat == 1)         //8-way ceiling flipping (bits 2,4,5)
            {
                i = sector[searchsector].ceilingstat;
                i = (i&0x4)+((i>>4)&3);
                switch (i)
                {
                case 0:
                    i = 6;
                    break;
                case 6:
                    i = 3;
                    break;
                case 3:
                    i = 5;
                    break;
                case 5:
                    i = 1;
                    break;
                case 1:
                    i = 7;
                    break;
                case 7:
                    i = 2;
                    break;
                case 2:
                    i = 4;
                    break;
                case 4:
                    i = 0;
                    break;
                }
                i = (i&0x4)+((i&3)<<4);
                sector[searchsector].ceilingstat &= ~0x34;
                sector[searchsector].ceilingstat |= i;
                asksave = 1;
            }
            if (searchstat == 2)         //8-way floor flipping (bits 2,4,5)
            {
                i = sector[searchsector].floorstat;
                i = (i&0x4)+((i>>4)&3);
                switch (i)
                {
                case 0:
                    i = 6;
                    break;
                case 6:
                    i = 3;
                    break;
                case 3:
                    i = 5;
                    break;
                case 5:
                    i = 1;
                    break;
                case 1:
                    i = 7;
                    break;
                case 7:
                    i = 2;
                    break;
                case 2:
                    i = 4;
                    break;
                case 4:
                    i = 0;
                    break;
                }
                i = (i&0x4)+((i&3)<<4);
                sector[searchsector].floorstat &= ~0x34;
                sector[searchsector].floorstat |= i;
                asksave = 1;
            }
            if (searchstat == 3)
            {
                i = sprite[searchwall].cstat;
                if (((i&48) == 32) && ((i&64) == 0))
                {
                    sprite[searchwall].cstat &= ~0xc;
                    sprite[searchwall].cstat |= ((i&4)^4);
                }
                else
                {
                    i = ((i>>2)&3);
                    switch (i)
                    {
                    case 0:
                        i = 1;
                        break;
                    case 1:
                        i = 3;
                        break;
                    case 2:
                        i = 0;
                        break;
                    case 3:
                        i = 2;
                        break;
                    }
                    i <<= 2;
                    sprite[searchwall].cstat &= ~0xc;
                    sprite[searchwall].cstat |= i;
                }
                asksave = 1;
            }
        }
    }

    if (keystatus[0xc7] > 0) // HOME
        updownunits = 256;
    else if (keystatus[0xcf] > 0) // END
        updownunits = 512;
    else
        updownunits = 1024;

//    if ((keystatus[0xc9] > 0) || ((bstatus&2) && (bstatus&16))) // PGUP
    if ((keystatus[0xc9] > 0) || ((bstatus&2) && (bstatus&16) && !(bstatus&1))) // PK: PGUP, rmb only & mwheel
    {
        k = 0;
        if (highlightsectorcnt >= 0)
        {
            for (i=0;i<highlightsectorcnt;i++)
                if (highlightsector[i] == searchsector)
                {
                    k = 1;
                    break;
                }
        }

        if ((searchstat == 0) || (searchstat == 1))
        {
            if (k == 0)
            {
                i = headspritesect[searchsector];
                while (i != -1)
                {
                    tempint = getceilzofslope(searchsector,sprite[i].x,sprite[i].y);
                    tempint += ((tilesizy[sprite[i].picnum]*sprite[i].yrepeat)<<2);
                    if (sprite[i].cstat&128) tempint += ((tilesizy[sprite[i].picnum]*sprite[i].yrepeat)<<1);
                    if (sprite[i].z == tempint)
                        sprite[i].z -= updownunits << ((keystatus[0x1d]|keystatus[0x9d])<<1);   // JBF 20031128
                    i = nextspritesect[i];
                }
                sector[searchsector].ceilingz -= updownunits << ((keystatus[0x1d]|keystatus[0x9d])<<1); // JBF 20031128
                sprintf(getmessage,"Sector %d ceilingz = %d",searchsector,sector[searchsector].ceilingz);
                message(getmessage);

            }
            else
            {
                for (j=0;j<highlightsectorcnt;j++)
                {
                    i = headspritesect[highlightsector[j]];
                    while (i != -1)
                    {
                        tempint = getceilzofslope(highlightsector[j],sprite[i].x,sprite[i].y);
                        tempint += ((tilesizy[sprite[i].picnum]*sprite[i].yrepeat)<<2);
                        if (sprite[i].cstat&128) tempint += ((tilesizy[sprite[i].picnum]*sprite[i].yrepeat)<<1);
                        if (sprite[i].z == tempint)
                            sprite[i].z -= updownunits << ((keystatus[0x1d]|keystatus[0x9d])<<1);   // JBF 20031128
                        i = nextspritesect[i];
                    }
                    sector[highlightsector[j]].ceilingz -= updownunits << ((keystatus[0x1d]|keystatus[0x9d])<<1);   // JBF 20031128
                    sprintf(getmessage,"Sector %d ceilingz = %d",*highlightsector,sector[highlightsector[j]].ceilingz);
                    message(getmessage);

                }
            }
        }
        if (searchstat == 2)
        {
            if (k == 0)
            {
                i = headspritesect[searchsector];
                while (i != -1)
                {
                    tempint = getflorzofslope(searchsector,sprite[i].x,sprite[i].y);
                    if (sprite[i].cstat&128) tempint += ((tilesizy[sprite[i].picnum]*sprite[i].yrepeat)<<1);
                    if (sprite[i].z == tempint)
                        sprite[i].z -= updownunits << ((keystatus[0x1d]|keystatus[0x9d])<<1);   // JBF 20031128
                    i = nextspritesect[i];
                }
                sector[searchsector].floorz -= updownunits << ((keystatus[0x1d]|keystatus[0x9d])<<1);   // JBF 20031128
                sprintf(getmessage,"Sector %d floorz = %d",searchsector,sector[searchsector].floorz);
                message(getmessage);

            }
            else
            {
                for (j=0;j<highlightsectorcnt;j++)
                {
                    i = headspritesect[highlightsector[j]];
                    while (i != -1)
                    {
                        tempint = getflorzofslope(highlightsector[j],sprite[i].x,sprite[i].y);
                        if (sprite[i].cstat&128) tempint += ((tilesizy[sprite[i].picnum]*sprite[i].yrepeat)<<1);
                        if (sprite[i].z == tempint)
                            sprite[i].z -= updownunits << ((keystatus[0x1d]|keystatus[0x9d])<<1);   // JBF 20031128
                        i = nextspritesect[i];
                    }
                    sector[highlightsector[j]].floorz -= updownunits << ((keystatus[0x1d]|keystatus[0x9d])<<1); // JBF 20031128
                    sprintf(getmessage,"Sector %d floorz = %d",*highlightsector,sector[highlightsector[j]].floorz);
                    message(getmessage);

                }
            }

        }
        if (sector[searchsector].floorz < sector[searchsector].ceilingz)
            sector[searchsector].floorz = sector[searchsector].ceilingz;
        if (searchstat == 3)
        {
            if ((keystatus[0x1d]|keystatus[0x9d]) > 0)  //CTRL - put sprite on ceiling
            {
                sprite[searchwall].z = getceilzofslope(searchsector,sprite[searchwall].x,sprite[searchwall].y);
                if (sprite[searchwall].cstat&128) sprite[searchwall].z -= ((tilesizy[sprite[searchwall].picnum]*sprite[searchwall].yrepeat)<<1);
                if ((sprite[searchwall].cstat&48) != 32)
                    sprite[searchwall].z += ((tilesizy[sprite[searchwall].picnum]*sprite[searchwall].yrepeat)<<2);
            }
            else
            {
                k = 0;
                if (highlightcnt >= 0)
                    for (i=0;i<highlightcnt;i++)
                        if (highlight[i] == searchwall+16384)
                        {
                            k = 1;
                            break;
                        }

                if (k == 0)
                {
                    sprite[searchwall].z -= updownunits;
                    sprintf(getmessage,"Sprite %d z = %d",searchwall,sprite[searchwall].z);
                    message(getmessage);

                }
                else
                {
                    for (i=0;i<highlightcnt;i++)
                        if ((highlight[i]&0xc000) == 16384)
                            sprite[highlight[i]&16383].z -= updownunits;
                    sprintf(getmessage,"Sprite %d z = %d",highlight[i]&16383,sprite[highlight[i]&16383].z);
                    message(getmessage);

                }
            }
        }
        asksave = 1;
        keystatus[0xc9] = 0;
        mouseb &= ~16;
    }
//    if ((keystatus[0xd1] > 0) || ((bstatus&2) && (bstatus&32))) // PGDN
    if ((keystatus[0xd1] > 0) || ((bstatus&2) && (bstatus&32) && !(bstatus&1))) // PK: PGDN, rmb only & mwheel
    {
        k = 0;
        if (highlightsectorcnt >= 0)
        {
            for (i=0;i<highlightsectorcnt;i++)
                if (highlightsector[i] == searchsector)
                {
                    k = 1;
                    break;
                }
        }

        if ((searchstat == 0) || (searchstat == 1))
        {
            if (k == 0)
            {
                i = headspritesect[searchsector];
                while (i != -1)
                {
                    tempint = getceilzofslope(searchsector,sprite[i].x,sprite[i].y);
                    if (sprite[i].cstat&128) tempint += ((tilesizy[sprite[i].picnum]*sprite[i].yrepeat)<<1);
                    tempint += ((tilesizy[sprite[i].picnum]*sprite[i].yrepeat)<<2);
                    if (sprite[i].z == tempint)
                        sprite[i].z += updownunits << ((keystatus[0x1d]|keystatus[0x9d])<<1);   // JBF 20031128
                    i = nextspritesect[i];
                }
                sector[searchsector].ceilingz += updownunits << ((keystatus[0x1d]|keystatus[0x9d])<<1); // JBF 20031128
                sprintf(getmessage,"Sector %d ceilingz = %d",searchsector,sector[searchsector].ceilingz);
                message(getmessage);

            }
            else
            {
                for (j=0;j<highlightsectorcnt;j++)
                {
                    i = headspritesect[highlightsector[j]];
                    while (i != -1)
                    {
                        tempint = getceilzofslope(highlightsector[j],sprite[i].x,sprite[i].y);
                        if (sprite[i].cstat&128) tempint += ((tilesizy[sprite[i].picnum]*sprite[i].yrepeat)<<1);
                        tempint += ((tilesizy[sprite[i].picnum]*sprite[i].yrepeat)<<2);
                        if (sprite[i].z == tempint)
                            sprite[i].z += updownunits << ((keystatus[0x1d]|keystatus[0x9d])<<1);   // JBF 20031128
                        i = nextspritesect[i];
                    }
                    sector[highlightsector[j]].ceilingz += updownunits << ((keystatus[0x1d]|keystatus[0x9d])<<1);   // JBF 20031128
                    sprintf(getmessage,"Sector %d ceilingz = %d",*highlightsector,sector[highlightsector[j]].ceilingz);
                    message(getmessage);

                }
            }
        }
        if (searchstat == 2)
        {
            if (k == 0)
            {
                i = headspritesect[searchsector];
                while (i != -1)
                {
                    tempint = getflorzofslope(searchsector,sprite[i].x,sprite[i].y);
                    if (sprite[i].cstat&128) tempint += ((tilesizy[sprite[i].picnum]*sprite[i].yrepeat)<<1);
                    if (sprite[i].z == tempint)
                        sprite[i].z += updownunits << ((keystatus[0x1d]|keystatus[0x9d])<<1);   // JBF 20031128
                    i = nextspritesect[i];
                }
                sector[searchsector].floorz += updownunits << ((keystatus[0x1d]|keystatus[0x9d])<<1);   // JBF 20031128
                sprintf(getmessage,"Sector %d floorz = %d",searchsector,sector[searchsector].floorz);
                message(getmessage);

            }
            else
            {
                for (j=0;j<highlightsectorcnt;j++)
                {
                    i = headspritesect[highlightsector[j]];
                    while (i != -1)
                    {
                        tempint = getflorzofslope(highlightsector[j],sprite[i].x,sprite[i].y);
                        if (sprite[i].cstat&128) tempint += ((tilesizy[sprite[i].picnum]*sprite[i].yrepeat)<<1);
                        if (sprite[i].z == tempint)
                            sprite[i].z += updownunits << ((keystatus[0x1d]|keystatus[0x9d])<<1);   // JBF 20031128
                        i = nextspritesect[i];
                    }
                    sector[highlightsector[j]].floorz += updownunits << ((keystatus[0x1d]|keystatus[0x9d])<<1); // JBF 20031128
                    sprintf(getmessage,"Sector %d floorz = %d",*highlightsector,sector[highlightsector[j]].floorz);
                    message(getmessage);

                }
            }
        }
        if (sector[searchsector].ceilingz > sector[searchsector].floorz)
            sector[searchsector].ceilingz = sector[searchsector].floorz;
        if (searchstat == 3)
        {
            if ((keystatus[0x1d]|keystatus[0x9d]) > 0)  //CTRL - put sprite on ground
            {
                sprite[searchwall].z = getflorzofslope(searchsector,sprite[searchwall].x,sprite[searchwall].y);
                if (sprite[searchwall].cstat&128) sprite[searchwall].z -= ((tilesizy[sprite[searchwall].picnum]*sprite[searchwall].yrepeat)<<1);
            }
            else
            {
                k = 0;
                if (highlightcnt >= 0)
                    for (i=0;i<highlightcnt;i++)
                        if (highlight[i] == searchwall+16384)
                        {
                            k = 1;
                            break;
                        }

                if (k == 0)
                {
                    sprite[searchwall].z += updownunits;
                    sprintf(getmessage,"Sprite %d z = %d",searchwall,sprite[searchwall].z);
                    message(getmessage);

                }
                else
                {
                    for (i=0;i<highlightcnt;i++)
                        if ((highlight[i]&0xc000) == 16384)
                            sprite[highlight[i]&16383].z += updownunits;
                    sprintf(getmessage,"Sprite %d z = %d",highlight[i]&16383,sprite[highlight[i]&16383].z);
                    message(getmessage);

                }
            }
        }
        asksave = 1;
        keystatus[0xd1] = 0;
        mouseb &= ~32;
    }

    /* end Mapster32 */

    //  DoWater(horiz);

    i = totalclock;
    if (i != clockval[clockcnt])
    {
        rate=(120<<4)/(i-clockval[clockcnt])-1;
        if (framerateon)
        {
            int p = 8;

            Bsprintf(tempbuf,"%d",rate);
            if (rate > 9) p += 8;
            if (rate > 99) p += 8;
            if (rate > 999) p += 8;
            if (xdimgame <= 640) p >>= 1;

            begindrawing();
            printext256(xdimgame-p-1,2,0,-1,tempbuf,!(xdimgame > 640));
            printext256(xdimgame-p-2,1,rate < 40?248:whitecol,-1,tempbuf,!(xdimgame > 640));
            enddrawing();
        }
    }
    clockval[clockcnt] = i;
    clockcnt = ((clockcnt+1)&15);

    tempbuf[0] = 0;
    if (bstatus&4 && !(bstatus&(1|2)) && !unrealedlook)  //PK
    {
        Bsprintf(tempbuf,"VIEW");
//        else Bsprintf(tempbuf,"SHADE");
    }
    else if (bstatus&2 && !(bstatus&1))
        Bsprintf(tempbuf,"Z");
    else if (bstatus&1 && !(bstatus&2))
        Bsprintf(tempbuf,"LOCK");

    if (tempbuf[0] != 0)
    {
        i = (Bstrlen(tempbuf)<<3)+6;
        if ((searchx+i) < (xdim-1))
            i = 0;
        else i = (searchx+i)-(xdim-1);
        if ((searchy+16) < (ydim-1))
            j = 0;
        else j = (searchy+16)-(ydim-1);
        //            printext16(searchx+6-i,searchy+6-j,11,-1,tempbuf,0);
        printext256(searchx+4+2-i,searchy+4+2-j,0,-1,tempbuf,!(xdimgame > 640));
        printext256(searchx+4-i,searchy+4-j,whitecol,-1,tempbuf,!(xdimgame > 640));

        //        printext256(searchx+4+2,searchy+4+2,0,-1,tempbuf,!(xdimgame > 640));
        //        printext256(searchx+4,searchy+4,whitecol,-1,tempbuf,!(xdimgame > 640));
    }
    if (helpon==1)
    {
        for (i=0;i<MAXHELP3D;i++)
        {
            begindrawing();
            printext256(0*8+2,8+(i*(8+(xdimgame > 640)))+2,0,-1,Help3d[i],!(xdimgame > 640));
            printext256(0*8,8+(i*(8+(xdimgame > 640))),whitecol,-1,Help3d[i],!(xdimgame > 640));
            enddrawing();
            switch (i)
            {
            case 8:
                Bsprintf(tempbuf,"%d",autosave);
                break;
            case 9:
                Bsprintf(tempbuf,"%s",SKILLMODE[skill]);
                break;
            case 10:
                Bsprintf(tempbuf,"%d",tabgraphic);
                break;
            case 11:
                Bsprintf(tempbuf,"%d",framerateon);
                break;
            case 12:
                Bsprintf(tempbuf,"%s",SPRDSPMODE[nosprites]);
                break;
            case 13:
                Bsprintf(tempbuf,"%d",shadepreview);
                break;
            case 14:
                Bsprintf(tempbuf,"%d",purpleon);
                break;
            default :
                sprintf(tempbuf," ");
                break;
            }
            begindrawing();
            if (!strcmp(tempbuf,"0"))
                Bsprintf(tempbuf,"OFF");
            else if (!strcmp(tempbuf,"1"))
                Bsprintf(tempbuf,"ON");
            else if (!strcmp(tempbuf,"2"))
                Bsprintf(tempbuf,"ON (2)");

            printext256((20+((xdimgame > 640) * 20))*8+2,8+(i*8+(xdimgame > 640))+2,0,-1,tempbuf,!(xdimgame > 640));
            printext256((20+((xdimgame > 640) * 20))*8,8+(i*8+(xdimgame > 640)),whitecol,-1,tempbuf,!(xdimgame > 640));
            enddrawing();
        }
    }

    /* if(purpleon) {
                begindrawing();
    //          printext256(1*4,1*8,whitecol,-1,"Purple ON",0);
                    sprintf(getmessage,"Purple ON");
                    message(getmessage);
                enddrawing();
                }
    */
    if (sector[cursectnum].lotag==2)
    {
        if (sector[cursectnum].floorpal==8) SetBOSS1Palette();
        else SetWATERPalette();
    }
    else SetGAMEPalette();


    //Stick this in 3D part of ExtCheckKeys
    //Also choose your own key scan codes



    if (keystatus[KEYSC_QUOTE]==1 && keystatus[0x20]==1) // ' d
        /*
            {
                ShowHelpText("SectorEffector");
            } */

    {
        keystatus[0x20] = 0;
        skill++;
        if (skill>MAXSKILL-1) skill=0;
        sprintf(tempbuf,"%s",SKILLMODE[skill]);
        //        printext256(1*4,1*8,11,-1,tempbuf,0);
        message(tempbuf);
    }

    begindrawing();
    if (keystatus[KEYSC_QUOTE]==1 && keystatus[0x22]==1) // ' g
    {
        keystatus[0x22] = 0;
        tabgraphic++;
        if (tabgraphic > 2) tabgraphic = 0;
        if (tabgraphic) message("Graphics ON");
        else message("Graphics OFF");
    }

    if (keystatus[KEYSC_QUOTE]==1 && keystatus[0x2d]==1) // ' x
    {
        keystatus[0x2d] = 0;
        shadepreview=!shadepreview;
        if (shadepreview) message("Sprite shade preview ON");
        else message("Sprite shade preview OFF");
    }


    if (keystatus[KEYSC_QUOTE]==1 && keystatus[0x13]==1) // ' r
    {
        keystatus[0x13] = 0;
        framerateon=!framerateon;
        if (framerateon) message("Framerate ON");
        else message("Framerate OFF");
    }

    if (keystatus[KEYSC_QUOTE]==1 && keystatus[0x11]==1) // ' w
    {
        keystatus[0x11] = 0;
        nosprites++;
        if (nosprites>3) nosprites=0;
        Bsprintf(tempbuf,"%s",SPRDSPMODE[nosprites]);
        //        printext256(1*4,1*8,whitecol,-1,tempbuf,0);
        message(tempbuf);
    }

    if (keystatus[KEYSC_QUOTE]==1 && keystatus[0x15]==1) // ' y
    {
        keystatus[0x15] = 0;
        purpleon=!purpleon;
        if (nosprites>3) nosprites=0;
        if (purpleon) message("Purple ON");
        else message("Purple OFF");
    }
    enddrawing();
    if (keystatus[KEYSC_QUOTE]==1 && keystatus[0x2e]==1) // ' C
    {
        keystatus[0x2e] = 0;
        switch (searchstat)
        {
        case 0:
        case 4:
            for (i=0;i<MAXWALLS;i++)
            {
                if (wall[i].picnum==temppicnum)
                    wall[i].shade=tempshade;
            }
            break;
        case 1:
        case 2:
            for (i=0;i<MAXSECTORS;i++)
            {
                if (searchstat==1)
                    if (sector[i].ceilingpicnum==temppicnum)
                    {
                        sector[i].ceilingshade=tempshade;
                    }
                if (searchstat==2)
                    if (sector[i].floorpicnum==temppicnum)
                    {
                        sector[i].floorshade=tempshade;
                    }
            }
            break;
        case 3:
            for (i=0;i<MAXSPRITES;i++)
            {
                if (sprite[i].picnum==temppicnum)
                {
                    sprite[i].shade=tempshade;
                }
            }
            break;
        }
    }

    if (keystatus[KEYSC_QUOTE]==1 && keystatus[0x14]==1) // ' T
    {
        keystatus[0x14] = 0;
        switch (searchstat)
        {
        case 0:
        case 4:
            Bstrcpy(tempbuf,"Wall lotag: ");
            wall[searchwall].lotag =
                getnumber256(tempbuf,wall[searchwall].lotag,65536L,0);
            break;
        case 1:
        case 2:
            Bstrcpy(tempbuf,"Sector lotag: ");
            sector[searchsector].lotag =
                getnumber256(tempbuf,sector[searchsector].lotag,65536L,0);
            break;
        case 3:
            Bstrcpy(tempbuf,"Sprite lotag: ");
            sprite[searchwall].lotag =
                getnumber256(tempbuf,sprite[searchwall].lotag,65536L,0);
            break;
        }
    }

    if (keystatus[KEYSC_QUOTE]==1 && keystatus[0x23]==1) // ' H
    {
        keystatus[0x23] = 0;
        switch (searchstat)
        {
        case 0:
        case 4:
            Bstrcpy(tempbuf,"Wall hitag: ");
            wall[searchwall].hitag =
                getnumber256(tempbuf,wall[searchwall].hitag,65536L,0);
            break;
        case 1:
        case 2:
            Bstrcpy(tempbuf,"Sector hitag: ");
            sector[searchsector].hitag =
                getnumber256(tempbuf,sector[searchsector].hitag,65536L,0);
            break;
        case 3:
            Bstrcpy(tempbuf,"Sprite hitag: ");
            sprite[searchwall].hitag =
                getnumber256(tempbuf,sprite[searchwall].hitag,65536L,0);
            break;
        }
    }

    if (keystatus[KEYSC_QUOTE]==1 && keystatus[0x1f]==1) // ' S
    {
        keystatus[0x1f] = 0;
        switch (searchstat)
        {
        case 0:
        case 4:
            Bstrcpy(tempbuf,"Wall shade: ");
            wall[searchwall].shade =
                getnumber256(tempbuf,wall[searchwall].shade,128L,1);
            break;
        case 1:
        case 2:
            Bstrcpy(tempbuf,"Sector shade: ");
            if (searchstat==1)
                sector[searchsector].ceilingshade =
                    getnumber256(tempbuf,sector[searchsector].ceilingshade,128L,1);
            if (searchstat==2)
                sector[searchsector].floorshade =
                    getnumber256(tempbuf,sector[searchsector].floorshade,128L,1);
            break;
        case 3:
            Bstrcpy(tempbuf,"Sprite shade: ");
            sprite[searchwall].shade =
                getnumber256(tempbuf,sprite[searchwall].shade,128L,1);
            break;
        }
    }

    if (keystatus[0x3c]>0) // F2
    {
        usedcount=!usedcount;
        keystatus[0x3c] = 0;
    }
    if (keystatus[0x0f]>0) // TAB : USED
    {
        //        usedcount=!usedcount;

        count=0;
        for (i=0;i<numwalls;i++)
        {
            if (wall[i].picnum == temppicnum) count++;
            if (wall[i].overpicnum == temppicnum) count++;
        }
        for (i=0;i<numsectors;i++)  // JBF 20040307: was numwalls, thanks Semicharm
        {
            if (sector[i].ceilingpicnum == temppicnum) count++;
            if (sector[i].floorpicnum == temppicnum) count++;
        }
        statnum = 0;        //status 1
        i = headspritestat[statnum];
        while (i != -1)
        {
            nexti = nextspritestat[i];

            //your code goes here
            //ex: printf("Sprite %d has a status of 1 (active)\n",i,statnum);

            if (sprite[i].picnum == temppicnum) count++;
            i = nexti;
        }

    }


    if (keystatus[0x3b]==1) // F1
    {
        helpon=!helpon;
        keystatus[0x23]=0;
        keystatus[0x3b]=0;
    }

    if ((keystatus[KEYSC_G] > 0)) // G
    {
        switch (searchstat)
        {
        case 0:
            strcpy(tempbuf,"Wall picnum: ");
            i = getnumber256(tempbuf,wall[searchwall].picnum,MAXTILES-1,0);
            if (tilesizx[i] != 0)
                wall[searchwall].picnum = i;
            break;
        case 1:
            strcpy(tempbuf,"Sector ceiling picnum: ");
            i = getnumber256(tempbuf,sector[searchsector].ceilingpicnum,MAXTILES-1,0);
            if (tilesizx[i] != 0)
                sector[searchsector].ceilingpicnum = i;
            break;
        case 2:
            strcpy(tempbuf,"Sector floor picnum: ");
            i = getnumber256(tempbuf,sector[searchsector].floorpicnum,MAXTILES-1,0);
            if (tilesizx[i] != 0)
                sector[searchsector].floorpicnum = i;
            break;
        case 3:
            strcpy(tempbuf,"Sprite picnum: ");
            i = getnumber256(tempbuf,sprite[searchwall].picnum,MAXTILES-1,0);
            if (tilesizx[i] != 0)
                sprite[searchwall].picnum = i;
            break;
        case 4:
            strcpy(tempbuf,"Masked wall picnum: ");
            i = getnumber256(tempbuf,wall[searchwall].overpicnum,MAXTILES-1,0);
            if (tilesizx[i] != 0)
                wall[searchwall].overpicnum = i;
            break;
        }
        asksave = 1;
        keystatus[KEYSC_G] = 0;
    }

    if (keystatus[KEYSC_B] > 0)  // B (clip Blocking xor) (3D)
    {
        if (searchstat == 3)
        {
            sprite[searchwall].cstat ^= 1;
            //                                sprite[searchwall].cstat &= ~256;
            //                                sprite[searchwall].cstat |= ((sprite[searchwall].cstat&1)<<8);
            sprintf(getmessage,"Sprite %d blocking %s",searchwall,sprite[searchwall].cstat&1?"ON":"OFF");
            message(getmessage);
            asksave = 1;
        }
        else
        {
            wall[searchwall].cstat ^= 1;
            //                                wall[searchwall].cstat &= ~64;
            if ((wall[searchwall].nextwall >= 0) && ((keystatus[0x2a]|keystatus[0x36]) == 0))
            {
                wall[wall[searchwall].nextwall].cstat &= ~(1+64);
                wall[wall[searchwall].nextwall].cstat |= (wall[searchwall].cstat&1);
            }
            sprintf(getmessage,"Wall %d blocking %s",searchwall,wall[searchwall].cstat&1?"ON":"OFF");
            message(getmessage);
            asksave = 1;
        }
        keystatus[KEYSC_B] = 0;
    }

    if (keystatus[KEYSC_T] > 0)  // T (transluscence for sprites/masked walls)
    {
        if (searchstat == 1)   //Set masked/transluscent ceilings/floors
        {
            i = (sector[searchsector].ceilingstat&(128+256));
            sector[searchsector].ceilingstat &= ~(128+256);
            switch (i)
            {
            case 0:
                sector[searchsector].ceilingstat |= 128;
                break;
            case 128:
                sector[searchsector].ceilingstat |= 256;
                break;
            case 256:
                sector[searchsector].ceilingstat |= 384;
                break;
            case 384:
                sector[searchsector].ceilingstat |= 0;
                break;
            }
            asksave = 1;
        }
        if (searchstat == 2)
        {
            i = (sector[searchsector].floorstat&(128+256));
            sector[searchsector].floorstat &= ~(128+256);
            switch (i)
            {
            case 0:
                sector[searchsector].floorstat |= 128;
                break;
            case 128:
                sector[searchsector].floorstat |= 256;
                break;
            case 256:
                sector[searchsector].floorstat |= 384;
                break;
            case 384:
                sector[searchsector].floorstat |= 0;
                break;
            }
            asksave = 1;
        }

        if ((keystatus[KEYSC_QUOTE]) > 0)
        {
            switch (searchstat)
            {
            case 0:
            case 4:
                strcpy(buffer,"Wall lotag: ");
                wall[searchwall].lotag = getnumber256(buffer,(int)wall[searchwall].lotag,65536L,0);
                break;
            case 1:
                strcpy(buffer,"Sector lotag: ");
                sector[searchsector].lotag = getnumber256(buffer,(int)sector[searchsector].lotag,65536L,0);
                break;
            case 2:
                strcpy(buffer,"Sector lotag: ");
                sector[searchsector].lotag = getnumber256(buffer,(int)sector[searchsector].lotag,65536L,0);
                break;
            case 3:
                strcpy(buffer,"Sprite lotag: ");
                sprite[searchwall].lotag = getnumber256(buffer,(int)sprite[searchwall].lotag,65536L,0);
                break;
            }
        }
        else
        {
            if (searchstat == 3)
            {
                if ((sprite[searchwall].cstat&2) == 0)
                    sprite[searchwall].cstat |= 2;
                else if ((sprite[searchwall].cstat&512) == 0)
                    sprite[searchwall].cstat |= 512;
                else
                    sprite[searchwall].cstat &= ~(2+512);
                asksave = 1;
            }
            if (searchstat == 4)
            {
                if ((wall[searchwall].cstat&128) == 0)
                    wall[searchwall].cstat |= 128;
                else if ((wall[searchwall].cstat&512) == 0)
                    wall[searchwall].cstat |= 512;
                else
                    wall[searchwall].cstat &= ~(128+512);

                if (wall[searchwall].nextwall >= 0)
                {
                    wall[wall[searchwall].nextwall].cstat &= ~(128+512);
                    wall[wall[searchwall].nextwall].cstat |= (wall[searchwall].cstat&(128+512));
                }
                asksave = 1;
            }
        }
        keystatus[KEYSC_T] = 0;
    }

    if (keystatus[KEYSC_QUOTE]==1 && keystatus[0x1c]==1) // ' ENTER
    {
        begindrawing();
        message("Pasted graphic only");
        enddrawing();
        switch (searchstat)
        {
        case 0 :
            wall[searchwall].picnum = temppicnum;
            break;
        case 1 :
            sector[searchsector].ceilingpicnum = temppicnum;
            break;
        case 2 :
            sector[searchsector].floorpicnum = temppicnum;
            break;
        case 3 :
            sprite[searchwall].picnum = temppicnum;
            break;
        case 4 :
            wall[searchwall].overpicnum = temppicnum;
            break;
        }
        keystatus[0x1c]=0;
    }



}// end 3d

static void Keys2d(void)
{
    short temp=0;
    int i=0, j,k;
    int radius, xp1, yp1;
    char col;

    int repeatcountx=0,repeatcounty=0,smooshyalign,changedir;
    /*
       for(i=0;i<0x50;i++)
       {if(keystatus[i]==1) {Bsprintf(tempbuf,"key %d",i); printmessage16(tempbuf);
    }}
    */

    Bsprintf(tempbuf, "Mapster32"VERSION"");
    printext16(9L,ydim2d-STATUS2DSIZ+9L,4,-1,tempbuf,0);
    printext16(8L,ydim2d-STATUS2DSIZ+8L,12,-1,tempbuf,0);

    if ((totalclock > getmessagetimeoff) && (totalclock > (lastpm16time + 120*3)))
    {
        updatesector(mousxplc,mousyplc,&cursectornum);
        if (pointhighlight >= 16384)
        {
            char tmpbuf[2048];
            i = pointhighlight-16384;
            if (strlen(names[sprite[i].picnum]) > 0)
            {
                if (sprite[i].picnum==SECTOREFFECTOR)
                    Bsprintf(tmpbuf,"Sprite %d %s: lo:%d hi:%d ex:%d",i,SectorEffectorText(i),sprite[i].lotag,sprite[i].hitag,sprite[i].extra);
                else Bsprintf(tmpbuf,"Sprite %d %s: lo:%d hi:%d ex:%d",i,names[sprite[i].picnum],sprite[i].lotag,sprite[i].hitag,sprite[i].extra);
            }
            else Bsprintf(tmpbuf,"Sprite %d picnum %d: lo:%d hi:%d ex:%d",i,sprite[i].picnum,sprite[i].lotag,sprite[i].hitag,sprite[i].extra);
            _printmessage16(tmpbuf);
        }
        else if ((linehighlight >= 0) && (sectorofwall(linehighlight) == cursectornum))
        {
            int dax, day, dist;
            dax = wall[linehighlight].x-wall[wall[linehighlight].point2].x;
            day = wall[linehighlight].y-wall[wall[linehighlight].point2].y;
            dist = ksqrt(dax*dax+day*day);
            Bsprintf(tempbuf,"Wall %d: length:%d lo:%d hi:%d ex:%d",linehighlight,dist,wall[linehighlight].lotag,wall[linehighlight].hitag,wall[linehighlight].extra);
            _printmessage16(tempbuf);
        }
        else if (cursectornum >= 0)
        {
            Bsprintf(tempbuf,"Sector %d: lo:%d hi:%d ex:%d",cursectornum,sector[cursectornum].lotag,sector[cursectornum].hitag,sector[cursectornum].extra);
            _printmessage16(tempbuf);
        }
        else _printmessage16("");
    }

    begindrawing();
    for (i=0;i<numsprites;i++) // Game specific 2D sprite stuff goes here.  Drawn on top of everything else.
    {
        xp1 = mulscale14(sprite[i].x-posx,zoom);
        yp1 = mulscale14(sprite[i].y-posy,zoom);

        if (sprite[i].picnum == 5 /*&& zoom >= 256*/ && sprite[i].sectnum != MAXSECTORS)
        {
            radius = mulscale15(sprite[i].hitag,zoom);
            col = 6;
            if (i+16384 == pointhighlight)
                if (totalclock & 32) col += (2<<2);
            drawlinepat = 0xf0f0f0f0;
            drawcircle16(halfxdim16+xp1, midydim16+yp1, radius, col);
            drawlinepat = 0xffffffff;
        }
    }
    enddrawing();

    if (keystatus[0x3b]==1 || (keystatus[KEYSC_QUOTE]==1 && keystatus[0x29]==1)) //F1 or ' ~
    {
        keystatus[0x3b]=0;
        clearmidstatbar16();
        begindrawing();
        for (i=0;i<MAXHELP2D;i++)
        {
            k = 0;
            j = 0;
            if (i > 9)
            {
                j = 256;
                k = 90;
            }
            printext16(j,ydim16+32+(i*9)-k,11,-1,Help2d[i],0);
        }
        enddrawing();
    }

    getpoint(searchx,searchy,&mousxplc,&mousyplc);
    ppointhighlight = getpointhighlight(mousxplc,mousyplc);

    if ((ppointhighlight&0xc000) == 16384)
    {
        //   sprite[ppointhighlight&16383].cstat ^= 1;
        cursprite=(ppointhighlight&16383);
    }

    if (keystatus[67]==1) // F9 f1=3b
    {
        Show2dText("sthelp.hlp");
    }

    /* start Mapster32 */

    if (keystatus[0x32] > 0)  // M (tag)
    {
        keystatus[0x32] = 0;
        if ((keystatus[0x38]|keystatus[0xb8]) > 0)  //ALT
        {
            if (pointhighlight >= 16384)
            {
                i = pointhighlight-16384;
                Bsprintf(tempbuf,"Sprite %d Extra: ",i);
                sprite[i].extra = getnumber16(tempbuf,sprite[i].extra,65536L,1);
                clearmidstatbar16();
                showspritedata((short)i);
            }
            else if (linehighlight >= 0)
            {
                i = linehighlight;
                Bsprintf(tempbuf,"Wall %d Extra: ",i);
                wall[i].extra = getnumber16(tempbuf,wall[i].extra,65536L,1);
                clearmidstatbar16();
                showwalldata((short)i);
            }
            printmessage16("");
        }
        else
        {
            for (i=0;i<numsectors;i++)
                if (inside(mousxplc,mousyplc,i) == 1)
                {
                    Bsprintf(tempbuf,"Sector %d Extra: ",i);
                    sector[i].extra = getnumber16(tempbuf,sector[i].extra,65536L,1);
                    clearmidstatbar16();
                    showsectordata((short)i);
                    break;
                }
            printmessage16("");
        }
    }

    if (keystatus[0x35] > 0)  // /?     Reset panning&repeat to 0
    {
        if ((ppointhighlight&0xc000) == 16384)
        {
            if ((keystatus[0x2a]|keystatus[0x36]) > 0)
            {
                sprite[cursprite].xrepeat = sprite[cursprite].yrepeat;
            }
            else
            {
                sprite[cursprite].xrepeat = 64;
                sprite[cursprite].yrepeat = 64;
            }
        }
        keystatus[0x35] = 0;
        asksave = 1;
    }

    if ((keystatus[0x4b]|keystatus[0x4d]) > 0)  // 4 & 6 (keypad)
    {
        smooshyalign = keystatus[0x4c];
        if ((repeatcountx == 0) || (repeatcountx > 16))
        {
            changedir = 0;
            if (keystatus[0x4b] > 0) changedir = -1;
            if (keystatus[0x4d] > 0) changedir = 1;

            if ((ppointhighlight&0xc000) == 16384)
            {
                sprite[cursprite].xrepeat = changechar(sprite[cursprite].xrepeat,changedir,smooshyalign,1);
                if (sprite[cursprite].xrepeat < 4)
                    sprite[cursprite].xrepeat = 4;
            }
            asksave = 1;
            repeatcountx = max(1,repeatcountx);
        }
        repeatcountx += synctics;
    }
    else
        repeatcountx = 0;

    if ((keystatus[0x48]|keystatus[0x50]) > 0)  // 2 & 8 (keypad)
    {
        smooshyalign = keystatus[0x4c];
        if ((repeatcounty == 0) || (repeatcounty > 16))
        {
            changedir = 0;
            if (keystatus[0x48] > 0) changedir = -1;
            if (keystatus[0x50] > 0) changedir = 1;

            if ((ppointhighlight&0xc000) == 16384)
            {
                sprite[cursprite].yrepeat = changechar(sprite[cursprite].yrepeat,changedir,smooshyalign,1);
                if (sprite[cursprite].yrepeat < 4)
                    sprite[cursprite].yrepeat = 4;
            }
            asksave = 1;
            repeatcounty = max(1,repeatcounty);
        }
        repeatcounty += synctics;
        //}
    }
    else
        repeatcounty = 0;

    if (keystatus[0x13] > 0)  // R (relative alignment, rotation)
    {
        if (pointhighlight >= 16384)
        {
            i = sprite[cursprite].cstat;
            if ((i&48) < 32) i += 16;

            else i &= ~48;
            sprite[cursprite].cstat = i;

            if (sprite[cursprite].cstat&16)
                sprintf(getmessage,"Sprite %d now wall aligned",cursprite);
            else if (sprite[cursprite].cstat&32)
                sprintf(getmessage,"Sprite %d now floor aligned",cursprite);
            else
                sprintf(getmessage,"Sprite %d now view aligned",cursprite);
            message(getmessage);
            asksave = 1;

        }
        keystatus[0x13] = 0;
    }


    if (keystatus[KEYSC_QUOTE] && keystatus[KEYSC_S]) // ' S
    {
        if (pointhighlight >= 16384)
        {
            keystatus[0x1f] = 0;
            Bsprintf(tempbuf,"Sprite %d xrepeat: ",cursprite);
            sprite[cursprite].xrepeat=getnumber16(tempbuf, sprite[cursprite].xrepeat, 256,0);
            Bsprintf(tempbuf,"Sprite %d yrepeat: ",cursprite);
            sprite[cursprite].yrepeat=getnumber16(tempbuf, sprite[cursprite].yrepeat, 256,0);
            Bsprintf(tempbuf,"Sprite %d updated",i);
            printmessage16(tempbuf);
        }
    }

    if (keystatus[KEYSC_QUOTE] && keystatus[KEYSC_F]) // ' F
    {
        keystatus[KEYSC_F] = 0;
        FuncMenu();
    }

    if (keystatus[0x1a]>0) // [     search backward
    {
        keystatus[0x1a]=0;
        if (wallsprite==0)
        {
            SearchSectorsBackward();
        }
        else

            if (wallsprite==1)
            {
                if (curwallnum>0) curwallnum--;
                for (i=curwallnum;i>=0;i--)
                {
                    if (
                        (wall[i].picnum==wall[curwall].picnum)
                        &&((search_lotag==0)||
                           (search_lotag!=0 && search_lotag==wall[i].lotag))
                        &&((search_hitag==0)||
                           (search_hitag!=0 && search_hitag==wall[i].hitag))
                    )
                    {
                        posx=(wall[i].x)-(((wall[i].x)-(wall[wall[i].point2].x))/2);
                        posy=(wall[i].y)-(((wall[i].y)-(wall[wall[i].point2].y))/2);
                        printmessage16("< Wall search: found");
                        //                    curwallnum--;
                        keystatus[0x1a]=0;
                        return;
                    }
                    curwallnum--;
                }
                printmessage16("< Wall search: none found");
            }
            else

                if (wallsprite==2)
                {
                    if (cursearchspritenum>0) cursearchspritenum--;
                    for (i=cursearchspritenum;i>=0;i--)
                    {

                        if (
                            (sprite[i].picnum==sprite[cursearchsprite].picnum &&
                             sprite[i].statnum==0)
                            &&((search_lotag==0)||
                               (search_lotag!=0 && search_lotag==sprite[i].lotag))
                            &&((search_hitag==0)||
                               (search_hitag!=0 && search_hitag==sprite[i].hitag))
                        )
                        {
                            posx=sprite[i].x;
                            posy=sprite[i].y;
                            ang= sprite[i].ang;
                            printmessage16("< Sprite search: found");
                            //                    curspritenum--;
                            keystatus[0x1a]=0;
                            return;
                        }
                        cursearchspritenum--;
                    }
                    printmessage16("< Sprite search: none found");
                }
    }


    if (keystatus[0x1b]>0) // ]     search forward
    {
        keystatus[0x1b]=0;
        if (wallsprite==0)
        {
            SearchSectorsForward();
        }
        else

            if (wallsprite==1)
            {
                if (curwallnum<MAXWALLS) curwallnum++;
                for (i=curwallnum;i<=MAXWALLS;i++)
                {
                    if (
                        (wall[i].picnum==wall[curwall].picnum)
                        &&((search_lotag==0)||
                           (search_lotag!=0 && search_lotag==wall[i].lotag))
                        &&((search_hitag==0)||
                           (search_hitag!=0 && search_hitag==wall[i].hitag))
                    )
                    {
                        posx=(wall[i].x)-(((wall[i].x)-(wall[wall[i].point2].x))/2);
                        posy=(wall[i].y)-(((wall[i].y)-(wall[wall[i].point2].y))/2);
                        printmessage16("> Wall search: found");
                        //                    curwallnum++;
                        keystatus[0x1b]=0;
                        return;
                    }
                    curwallnum++;
                }
                printmessage16("> Wall search: none found");
            }
            else

                if (wallsprite==2)
                {
                    if (cursearchspritenum<MAXSPRITES) cursearchspritenum++;
                    for (i=cursearchspritenum;i<=MAXSPRITES;i++)
                    {
                        if (
                            (sprite[i].picnum==sprite[cursearchsprite].picnum &&
                             sprite[i].statnum==0)
                            &&((search_lotag==0)||
                               (search_lotag!=0 && search_lotag==sprite[i].lotag))
                            &&((search_hitag==0)||
                               (search_hitag!=0 && search_hitag==sprite[i].hitag))
                        )
                        {
                            posx=sprite[i].x;
                            posy=sprite[i].y;
                            ang= sprite[i].ang;
                            printmessage16("> Sprite search: found");
                            //                    curspritenum++;
                            keystatus[0x1b]=0;
                            return;
                        }
                        cursearchspritenum++;
                    }
                    printmessage16("> Sprite search: none found");
                }
    }

    if (keystatus[0x22] > 0)  // G (grid on/off)
    {
        grid += ((keystatus[0x2a]|keystatus[0x36]) > 0?-1:1);
        if (grid == -1 || grid == 9)
        {
            switch (grid)
            {
            case -1:
                grid = 8;
                break;
            case 9:
                grid = 0;
                break;
            }
        }
        if (!grid) sprintf(tempbuf,"Grid off");
        else sprintf(tempbuf,"Grid size: %d (%d units)",grid,2048>>grid);
        printmessage16(tempbuf);
        keystatus[0x22] = 0;
    }

    if ((keystatus[0x26] > 0) && (keystatus[KEYSC_QUOTE] > 0)) // ' L
    {
        if (pointhighlight >= 16384)
        {
            i = pointhighlight - 16384;
            Bsprintf(tempbuf,"Sprite %d x: ",i);
            sprite[i].x = getnumber16(tempbuf,sprite[i].x,131072,1);
            Bsprintf(tempbuf,"Sprite %d y: ",i);
            sprite[i].y = getnumber16(tempbuf,sprite[i].y,131072,1);
            Bsprintf(tempbuf,"Sprite %d z: ",i);
            sprite[i].z = getnumber16(tempbuf,sprite[i].z,8388608,1);
            Bsprintf(tempbuf,"Sprite %d angle: ",i);
            sprite[i].ang = getnumber16(tempbuf,sprite[i].ang,2048L,0);
            Bsprintf(tempbuf,"Sprite %d updated",i);
            printmessage16(tempbuf);
        }

        else if (pointhighlight <= 16383)
        {
            i = linehighlight;
            Bsprintf(tempbuf,"Wall %d x: ",i);
            wall[i].x = getnumber16(tempbuf,wall[i].x,131072,1);
            Bsprintf(tempbuf,"Wall %d y: ",i);
            wall[i].y = getnumber16(tempbuf,wall[i].y,131072,1);
            Bsprintf(tempbuf,"Wall %d updated",i);
            printmessage16(tempbuf);
        }

        keystatus[0x26] = 0;
    }


    if (keystatus[KEYSC_QUOTE]==1 && keystatus[0x04]==1) // ' 3
    {
        onnames++;
        if (onnames>8) onnames=0;
        keystatus[0x04]=0;
        Bsprintf(tempbuf,"Mode %d %s",onnames,SpriteMode[onnames]);
        printmessage16(tempbuf);
        //   clearmidstatbar16();
        //   for(i=0;i<MAXMODE32D;i++) {printext16(0*8,ydim16+32+(i*8),15,-1,SpriteMode[i],0);
    }
    //   Ver();

    /*
     if(keystatus[KEYSC_QUOTE]==1 && keystatus[0x06]==1) // ' 5
     {
        keystatus[0x06]=0;
       sprintf(tempbuf,"Power-Up Ammo now equals Normal");
       printmessage16(tempbuf);
        for(i=0;i<MAXSPRITES;i++)
            {
         if(sprite[i].picnum>=20 && sprite[i].picnum<=59)
         {
            sprite[i].xrepeat = 32;
            sprite[i].yrepeat = 32;
         }
        }

     }
    */

    //  What the fuck is this supposed to do?

    /* Motorcycle ha ha ha
    if(keystatus[KEYSC_QUOTE]==1 && keystatus[0x06]==1) // ' 5
    {
        keystatus[0x06]=0;
            sidemode++; if (sidemode > 2) sidemode = 0;
            if (sidemode == 1)
            {
                    editstatus = 0;
                    zmode = 2;
                    posz = ((sector[cursectnum].ceilingz+sector[cursectnum].floorz)>>1);
            }
            else
            {
                    editstatus = 1;
                    zmode = 1;
            }
    }
    */

    if (keystatus[KEYSC_QUOTE]==1 && keystatus[0x08]==1) // ' 7 : swap hilo
    {
        keystatus[0x08]=0;

        if (pointhighlight >= 16384)
        {
            temp=sprite[cursprite].lotag;
            sprite[cursprite].lotag=sprite[cursprite].hitag;
            sprite[cursprite].hitag=temp;
            Bsprintf(tempbuf,"Sprite %d tags swapped",cursprite);
            printmessage16(tempbuf);
        }
        else if (linehighlight >= 0)
        {
            temp=wall[linehighlight].lotag;
            wall[linehighlight].lotag=wall[linehighlight].hitag;
            wall[linehighlight].hitag=temp;
            Bsprintf(tempbuf,"Wall %d tags swapped",linehighlight);
            printmessage16(tempbuf);
        }
    }

    if (keystatus[KEYSC_QUOTE]==1 && keystatus[0x24]==1) // ' J
    {
        posx=getnumber16("X-coordinate:    ",posx,131072L,1);
        posy=getnumber16("Y-coordinate:    ",posy,131072L,1);
        Bsprintf(tempbuf,"Current pos now (%d, %d)",posx,posy);
        printmessage16(tempbuf);
        keystatus[0x24]=0;
    }

}// end key2d

void ExtSetupSpecialSpriteCols(void)
{
    int i;
    for (i=0;i<MAXTILES;i++)

        switch (i)
        {
        case SECTOREFFECTOR:
        case ACTIVATOR:
        case TOUCHPLATE:
        case ACTIVATORLOCKED:
        case MUSICANDSFX:
        case LOCATORS:
        case CYCLER:
        case MASTERSWITCH:
        case RESPAWN:
        case GPSPEED:
            spritecol2d[i][0] = 15;
            spritecol2d[i][1] = 15;
            break;
        case APLAYER:
            spritecol2d[i][0] = 2;
            spritecol2d[i][1] = 2;
            break;
        case LIZTROOP :
        case LIZTROOPRUNNING :
        case LIZTROOPSTAYPUT :
        case LIZTROOPSHOOT :
        case LIZTROOPJETPACK :
        case LIZTROOPONTOILET :
        case LIZTROOPDUCKING :
        case PIGCOP:
        case PIGCOPSTAYPUT:
        case PIGCOPDIVE:
        case LIZMAN:
        case LIZMANSTAYPUT:
        case LIZMANSPITTING:
        case LIZMANFEEDING:
        case LIZMANJUMP:
        case BOSS1:
        case BOSS1STAYPUT:
        case BOSS1SHOOT:
        case BOSS1LOB:
        case BOSSTOP:
        case COMMANDER:
        case COMMANDERSTAYPUT:
        case OCTABRAIN:
        case OCTABRAINSTAYPUT:
        case RECON:
        case DRONE:
        case ROTATEGUN:
        case EGG:
        case ORGANTIC:
        case GREENSLIME:
        case BOSS2:
        case BOSS3:
        case TANK:
        case NEWBEAST:
        case BOSS4:
            spritecol2d[i][0] = 31;
            spritecol2d[i][1] = 31;
            break;
        case FIRSTGUNSPRITE:
        case CHAINGUNSPRITE :
        case RPGSPRITE:
        case FREEZESPRITE:
        case SHRINKERSPRITE:
        case HEAVYHBOMB:
        case TRIPBOMBSPRITE:
        case SHOTGUNSPRITE:
        case DEVISTATORSPRITE:
        case FREEZEAMMO:
        case AMMO:
        case BATTERYAMMO:
        case DEVISTATORAMMO:
        case RPGAMMO:
        case GROWAMMO:
        case CRYSTALAMMO:
        case HBOMBAMMO:
        case AMMOLOTS:
        case SHOTGUNAMMO:
        case COLA:
        case SIXPAK:
        case FIRSTAID:
        case SHIELD:
        case STEROIDS:
        case AIRTANK:
        case JETPACK:
        case HEATSENSOR:
        case ACCESSCARD:
        case BOOTS:
            spritecol2d[i][0] = 24;
            spritecol2d[i][1] = 24;
            break;
        default:
            break;
        }
}

static void InitCustomColors(void)
{
    /* blue */
    /*    vgapal16[9*4+0] = 63;
        vgapal16[9*4+1] = 31;
        vgapal16[9*4+2] = 7; */

    /* orange */
    vgapal16[31*4+0] = 20; // blue
    vgapal16[31*4+1] = 45; // green
    vgapal16[31*4+2] = 60; // red

    vgapal16[39*4+0] = 36;
    vgapal16[39*4+1] = 53;
    vgapal16[39*4+2] = 63;


    /* light yellow */
    vgapal16[22*4+0] = 51;
    vgapal16[22*4+1] = 63;
    vgapal16[22*4+2] = 63;

    /* grey */
    vgapal16[23*4+0] = 45;
    vgapal16[23*4+1] = 45;
    vgapal16[23*4+2] = 45;

    /* blue */
    vgapal16[24*4+0] = 51;
    vgapal16[24*4+1] = 41;
    vgapal16[24*4+2] = 12;

    vgapal16[32*4+0] = 60;
    vgapal16[32*4+1] = 50;
    vgapal16[32*4+2] = 21;
}

void ExtPreSaveMap(void)
{
    if (fixmapbeforesaving)
    {
        int i, startwall, j, endwall;

        for (i=0;i<numsectors;i++)
        {
            startwall = sector[i].wallptr;
            for (j=startwall;j<numwalls;j++)
                if (wall[j].point2 < startwall) startwall = wall[j].point2;
            sector[i].wallptr = startwall;
        }
        for (i=numsectors-2;i>=0;i--)
            sector[i].wallnum = sector[i+1].wallptr-sector[i].wallptr;
        sector[numsectors-1].wallnum = numwalls-sector[numsectors-1].wallptr;

        for (i=0;i<numwalls;i++)
        {
            wall[i].nextsector = -1;
            wall[i].nextwall = -1;
        }
        for (i=0;i<numsectors;i++)
        {
            startwall = sector[i].wallptr;
            endwall = startwall + sector[i].wallnum;
            for (j=startwall;j<endwall;j++)
                checksectorpointer((short)j,(short)i);
        }
    }
}

void ExtPreLoadMap(void)
{}

static void comlinehelp(void)
{
    char *s = "Usage: mapster32 [OPTIONS] [FILE]\n\n"
              "-gFILE, -grp FILE\tUse extra group file FILE\n"
              "-hFILE\t\tUse definitions file FILE\n"
              "-jDIR, -game_dir DIR\n\t\tAdds DIR to the file path stack\n"
              "-nocheck\t\tDisables map pointer checking when saving\n"
#if defined RENDERTYPEWIN || (defined RENDERTYPESDL && !defined __APPLE__ && defined HAVE_GTK2)
              "-setup\t\tDisplays the configuration dialog\n"
#endif
#if !defined(_WIN32)
              "-usecwd\t\tRead game data and configuration file from working directory\n"
#endif
              "\n-?, -help, --help\tDisplay this help message and exit"
              ;
    wm_msgbox("Mapster32"VERSION,s);
}

static void addgamepath(const char *buffer)
{
    struct strllist *s;
    s = (struct strllist *)calloc(1,sizeof(struct strllist));
    s->str = strdup(buffer);

    if (CommandPaths)
    {
        struct strllist *t;
        for (t = CommandPaths; t->next; t=t->next) ;
        t->next = s;
        return;
    }
    CommandPaths = s;
}

static void addgroup(const char *buffer)
{
    struct strllist *s;
    s = (struct strllist *)calloc(1,sizeof(struct strllist));
    s->str = Bstrdup(buffer);
    if (Bstrchr(s->str,'.') == 0)
        Bstrcat(s->str,".grp");

    if (CommandGrps)
    {
        struct strllist *t;
        for (t = CommandGrps; t->next; t=t->next) ;
        t->next = s;
        return;
    }
    CommandGrps = s;
}

static void checkcommandline(int argc, const char **argv)
{
    int i = 1;
    char *c;

    if (argc > 1)
    {
        while (i < argc)
        {
            c = (char *)argv[i];
            if (((*c == '/') || (*c == '-')))
            {
                if (!Bstrcasecmp(c+1,"?") || !Bstrcasecmp(c+1,"help") || !Bstrcasecmp(c+1,"-help"))
                {
                    comlinehelp();
                    exit(0);
                }

                if (!Bstrcasecmp(c+1, "g") || !Bstrcasecmp(c+1,  "grp"))
                {
                    if (argc > i+1)
                    {
                        addgroup(argv[i+1]);
                        i++;
                    }
                    i++;
                    continue;
                }

                if (!Bstrcasecmp(c+1,"game_dir"))
                {
                    if (argc > i+1)
                    {
                        addgamepath(argv[i+1]);
                        i++;
                    }
                    i++;
                    continue;
                }
                if (!Bstrcasecmp(c+1,"cfg"))
                {
                    if (argc > i+1)
                    {
                        Bstrcpy(setupfilename,argv[i+1]);
                        i++;
                    }
                    i++;
                    continue;
                }
                if (!Bstrcasecmp(c+1,"gamegrp"))
                {
                    if (argc > i+1)
                    {
                        Bstrcpy(defaultduke3dgrp,argv[i+1]);
                        i++;
                    }
                    i++;
                    continue;
                }
                if (!Bstrcasecmp(c+1,"nam"))
                {
                    strcpy(duke3dgrp, "nam.grp");
                    i++;
                    continue;
                }
                if (!Bstrcasecmp(c+1,"ww2gi"))
                {
                    strcpy(duke3dgrp, "ww2gi.grp");
                    i++;
                    continue;
                }
                if (!Bstrcasecmp(c+1,"nocheck"))
                {
                    initprintf("Map pointer checking disabled\n");
                    fixmapbeforesaving = 0;
                    i++;
                    continue;
                }
                if (!Bstrcasecmp(c+1,"noautoload"))
                {
                    initprintf("Autoload disabled\n");
                    NoAutoLoad = 1;
                    i++;
                    continue;
                }
#if !defined(_WIN32)
                if (!Bstrcasecmp(c+1,"usecwd"))
                {
                    usecwd = 1;
                    i++;
                    continue;
                }
#endif
            }

            if ((*c == '/') || (*c == '-'))
            {
                c++;
                switch (*c)
                {
                case 'h':
                case 'H':
                    c++;
                    if (*c)
                    {
                        defsfilename = c;
                        initprintf("Using DEF file: %s.\n",defsfilename);
                    }
                    break;
                case 'j':
                case 'J':
                    c++;
                    if (!*c) break;
                    addgamepath(c);
                    break;
                case 'g':
                case 'G':
                    c++;
                    if (!*c) break;
                    addgroup(c);
                    break;
                }
            }
            i++;
        }
    }
}

int ExtPreInit(int argc,const char **argv)
{
    wm_setapptitle("Mapster32"VERSION);

    OSD_SetLogFile("mapster32.log");
    OSD_SetVersionString("Mapster32"VERSION,0,2);
    initprintf("Mapster32"VERSION" ("__DATE__" "__TIME__")\n");
    initprintf("Copyright (c) 2008 EDuke32 team\n");

    checkcommandline(argc,argv);

    return 0;
}

static int osdcmd_quit(const osdfuncparm_t *parm)
{
    UNREFERENCED_PARAMETER(parm);
    clearfilenames();
    ExtUnInit();
    uninitengine();

    exit(0);

    return OSDCMD_OK;
}

static int osdcmd_editorgridextent(const osdfuncparm_t *parm)
{
    int i;
    extern int editorgridextent;

    if (parm->numparms == 0)
    {
        OSD_Printf("\"editorgridextent\" is \"%d\"\n", editorgridextent);
        return OSDCMD_SHOWHELP;
    }
    else if (parm->numparms != 1) return OSDCMD_SHOWHELP;

    i = Batol(parm->parms[0]);

    if (i >= 65536 && i <= 524288)
    {
        editorgridextent = i;
        OSD_Printf("editorgridextent %d\n", editorgridextent);
    }
    else OSD_Printf("editorgridextent: value out of range\n");
    return OSDCMD_OK;
}

static int osdcmd_addpath(const osdfuncparm_t *parm)
{
    char pathname[BMAX_PATH];

    if (parm->numparms != 1) return OSDCMD_SHOWHELP;

    strcpy(pathname,parm->parms[0]);
    addsearchpath(pathname);
    return OSDCMD_OK;
}

static int osdcmd_initgroupfile(const osdfuncparm_t *parm)
{
    char file[BMAX_PATH];

    if (parm->numparms != 1) return OSDCMD_SHOWHELP;

    strcpy(file,parm->parms[0]);
    initgroupfile(file);
    return OSDCMD_OK;
}

static int osdcmd_echo(const osdfuncparm_t *parm)
{
    int i;
    for (i = 0; i < parm->numparms; i++)
    {
        if (i > 0) OSD_Printf(" ");
        OSD_Printf("%s", parm->parms[i]);
    }
    OSD_Printf("\n");

    return OSDCMD_OK;
}

static int osdcmd_fileinfo(const osdfuncparm_t *parm)
{
    unsigned int crc, length;
    int i,j;
    char buf[256];

    if (parm->numparms != 1) return OSDCMD_SHOWHELP;

    if ((i = kopen4load((char *)parm->parms[0],0)) < 0)
    {
        OSD_Printf("fileinfo: File \"%s\" not found.\n", parm->parms[0]);
        return OSDCMD_OK;
    }

    length = kfilelength(i);

    crc32init(&crc);
    do
    {
        j = kread(i,buf,256);
        crc32block(&crc,(unsigned char *)buf,j);
    }
    while (j == 256);
    crc32finish(&crc);

    kclose(i);

    OSD_Printf("fileinfo: %s\n"
               "  File size: %d\n"
               "  CRC-32:    %08X\n",
               parm->parms[0], length, crc);

    return OSDCMD_OK;
}

static int osdcmd_sensitivity(const osdfuncparm_t *parm)
{
    if (parm->numparms != 1)
    {
        OSD_Printf("\"sensitivity\" is \"%.2f\"\n",msens);
        return OSDCMD_SHOWHELP;
    }
    msens = atof(parm->parms[0]);
    OSD_Printf("sensitivity %.2f\n",msens);
    return OSDCMD_OK;
}

static int osdcmd_gamma(const osdfuncparm_t *parm)
{
    extern short brightness;

    if (parm->numparms != 1)
    {
        OSD_Printf("\"gamma\" \"%d\"\n",brightness);
        return OSDCMD_SHOWHELP;
    }
    brightness = atoi(parm->parms[0]);
    setbrightness(brightness,palette,0);
    OSD_Printf("gamma %d\n",brightness);
    return OSDCMD_OK;
}

static int load_script(const char *szScript)
{
    FILE* fp = fopenfrompath(szScript, "r");

    if (fp != NULL)
    {
        char line[255];

        OSD_Printf("Executing \"%s\"\n", szScript);
        while (fgets(line ,sizeof(line)-1, fp) != NULL)
            OSD_Dispatch(strtok(line,"\r\n"));
        fclose(fp);
        return 0;
    }
    return 1;
}

static int osdcmd_exec(const osdfuncparm_t *parm)
{
    char fn[BMAX_PATH];

    if (parm->numparms != 1) return OSDCMD_SHOWHELP;
    Bstrcpy(fn,parm->parms[0]);

    if (load_script(fn))
    {
        OSD_Printf("exec: file \"%s\" not found.\n", fn);
        return OSDCMD_OK;
    }
    return OSDCMD_OK;
}

static int osdcmd_noclip(const osdfuncparm_t *parm)
{
    UNREFERENCED_PARAMETER(parm);
    noclip = !noclip;

    return OSDCMD_OK;
}

//PK vvv ------------
extern int pk_turnaccel, pk_turndecel, pk_uedaccel;
extern char quickmapcycling;

static int osdcmd_vars_pk(const osdfuncparm_t *parm)
{
    int showval = (parm->numparms < 1);

    // this is something of a misnomer, since it's actually accel+decel
    if (!Bstrcasecmp(parm->name, "pk_turnaccel"))
    {
        if (showval) { OSD_Printf("Turning acceleration+declaration is %d\n", pk_turnaccel); }
        else
        {
            pk_turnaccel = atoi(parm->parms[0]);
            pk_turnaccel = pk_turnaccel<=pk_turndecel ? (pk_turndecel+1):pk_turnaccel;
            pk_turnaccel = pk_turnaccel>256 ? 256:pk_turnaccel;
        }
    }
    else if (!Bstrcasecmp(parm->name, "pk_turndecel"))
    {
        if (showval) { OSD_Printf("Turning deceleration is %d\n", pk_turndecel); }
        else
        {
            pk_turndecel = atoi(parm->parms[0]);
            pk_turndecel = pk_turndecel<=0 ? 1:pk_turndecel;
            pk_turndecel = pk_turndecel>=pk_turnaccel ? (pk_turnaccel-1):pk_turndecel;
            pk_turndecel = pk_turndecel>128 ? 128:pk_turndecel;
        }
    }
    else if (!Bstrcasecmp(parm->name, "pk_quickmapcycling"))
    {
        OSD_Printf("Quick map cycling ((LShift-)Ctrl-X): %s\n",
                   (quickmapcycling = !quickmapcycling) ? "on":"off");
    }
    else if (!Bstrcasecmp(parm->name, "pk_uedaccel"))
    {
        if (showval) { OSD_Printf("UnrealEd mouse navigation acceleration is %d\n", pk_uedaccel); }
        else
        {
            pk_uedaccel = atoi(parm->parms[0]);
            pk_uedaccel = pk_uedaccel<0 ? 0:pk_uedaccel;
            pk_uedaccel = pk_uedaccel>5 ? 5:pk_uedaccel;
        }
    }
    return OSDCMD_OK;
}

static int registerosdcommands(void)
{
    OSD_RegisterFunction("addpath","addpath <path>: adds path to game filesystem", osdcmd_addpath);

    OSD_RegisterFunction("echo","echo [text]: echoes text to the console", osdcmd_echo);
    OSD_RegisterFunction("editorgridextent","editorgridextent: sets the size of the 2D mode editing grid",osdcmd_editorgridextent);
    OSD_RegisterFunction("exec","exec <scriptfile>: executes a script", osdcmd_exec);

    OSD_RegisterFunction("fileinfo","fileinfo <file>: gets a file's information", osdcmd_fileinfo);

    OSD_RegisterFunction("gamma","gamma <value>: changes brightness", osdcmd_gamma);

    OSD_RegisterFunction("initgroupfile","initgroupfile <path>: adds a grp file into the game filesystem", osdcmd_initgroupfile);

    OSD_RegisterFunction("noclip","noclip: toggles clipping mode", osdcmd_noclip);

    OSD_RegisterFunction("quit","quit: exits the game immediately", osdcmd_quit);

    OSD_RegisterFunction("sensitivity","sensitivity <value>: changes the mouse sensitivity", osdcmd_sensitivity);

    //PK
    OSD_RegisterFunction("pk_turnaccel", "pk_turnaccel: sets turning acceleration", osdcmd_vars_pk);
    OSD_RegisterFunction("pk_turndecel", "pk_turndecel: sets turning deceleration", osdcmd_vars_pk);
    OSD_RegisterFunction("pk_uedaccel", "pk_uedaccel: sets UnrealEd movement speed factor (0-5, exponentially)", osdcmd_vars_pk);
    OSD_RegisterFunction("pk_quickmapcycling", "pk_quickmapcycling: allows cycling of maps with (Shift-)Ctrl-X", osdcmd_vars_pk);
    return 0;
}

#ifdef DUKEOSD
void GAME_drawosdchar(int x, int y, char ch, int shade, int pal)
{
    int ac;

    if (ch == 32) return;
    ac = ch-'!'+STARTALPHANUM;
    if (ac < STARTALPHANUM || ac > ENDALPHANUM) return;

    rotatesprite(((x<<3)+x)<<16, (y<<3)<<16, 65536l, 0, ac, shade, pal, 8|16, 0, 0, xdim-1, ydim-1);
}

void GAME_drawosdstr(int x, int y, char *ch, int len, int shade, int pal)
{
    int ac;

    for (x = (x<<3)+x; len>0; len--, ch++, x++)
    {
        if (*ch == 32)
        {
            x+=5;
            continue;
        }
        ac = *ch-'!'+STARTALPHANUM;
        if (ac < STARTALPHANUM || ac > ENDALPHANUM) return;

        rotatesprite(x<<16, (y<<3)<<16, 65536l, 0, ac, shade, pal, 8|16, 0, 0, xdim-1, ydim-1);
        if (*ch >= '0' && *ch <= '9') x+=8;
        else x += tilesizx[ac];
    }
}

static int GetTime(void)
{
    return totalclock;
}

void GAME_drawosdcursor(int x, int y, int type, int lastkeypress)
{
    int ac;

    if (type) ac = SMALLFNTCURSOR;
    else ac = '_'-'!'+STARTALPHANUM;

    if (!((GetTime()-lastkeypress) & 0x40l))
        rotatesprite(((x<<3)+x)<<16, ((y<<3)+(type?-1:2))<<16, 65536l, 0, ac, 0, 8, 8|16, 0, 0, xdim-1, ydim-1);
}

int GAME_getcolumnwidth(int w)
{
    return w/9;
}

int GAME_getrowheight(int w)
{
    return w>>3;
}

//#define BGTILE 311
//#define BGTILE 1156
#define BGTILE 1141	// BIGHOLE
#define BORDTILE 3250	// VIEWBORDER
#define BITSTH 1+32+8+16	// high translucency
#define BITSTL 1+8+16	// low translucency
#define BITS 8+16+64		// solid
#define SHADE 16
#define PALETTE 4
void GAME_clearbackground(int c, int r)
{
    int x, y, xsiz, ysiz, tx2, ty2;
    int daydim, bits;

#ifdef _WIN32
    if (qsetmode != 200)
    {
        OSD_SetFunctions(
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            (int(*)(void))GetTime,
            NULL
        );
        return;
    }
#endif

    if (getrendermode() < 3) bits = BITS;
    else bits = BITSTL;

    daydim = r<<3;

    xsiz = tilesizx[BGTILE];
    tx2 = xdim/xsiz;
    ysiz = tilesizy[BGTILE];
    ty2 = daydim/ysiz;

    for (x=0;x<=tx2;x++)
        for (y=0;y<=ty2;y++)
            rotatesprite(x*xsiz<<16,y*ysiz<<16,65536L,0,BGTILE,SHADE,PALETTE,bits,0,0,xdim,daydim);

    xsiz = tilesizy[BORDTILE];
    tx2 = xdim/xsiz;
    ysiz = tilesizx[BORDTILE];

    for (x=0;x<=tx2;x++)
        rotatesprite(x*xsiz<<16,(daydim+ysiz+1)<<16,65536L,1536,BORDTILE,SHADE-12,PALETTE,BITS,0,0,xdim,daydim+ysiz+1);
}
#endif

enum
{
    T_EOF = -2,
    T_ERROR = -1,
    T_INCLUDE = 0,
    T_DEFINE = 1,
    T_LOADGRP,
    T_TILEGROUP,
    T_TILE,
    T_TILERANGE,
    T_HOTKEY,
    T_TILES,
    T_NOAUTOLOAD
};

typedef struct
{
    char *text;
    int tokenid;
}
tokenlist;

static int getatoken(scriptfile *sf, tokenlist *tl, int ntokens)
{
    char *tok;
    int i;

    if (!sf) return T_ERROR;
    tok = scriptfile_gettoken(sf);
    if (!tok) return T_EOF;

    for (i=0;i<ntokens;i++)
    {
        if (!Bstrcasecmp(tok, tl[i].text))
            return tl[i].tokenid;
    }
    return T_ERROR;
}

static void autoloadgrps(const char *fn)
{
    Bsprintf(tempbuf,"autoload/%s",fn);
    getfilenames(tempbuf,"*.grp");
    while (findfiles) { Bsprintf(tempbuf,"autoload/%s/%s",fn,findfiles->name); initprintf("Using group file '%s'.\n",tempbuf); initgroupfile(tempbuf); findfiles = findfiles->next; }
    Bsprintf(tempbuf,"autoload/%s",fn);
    getfilenames(tempbuf,"*.zip");
    while (findfiles) { Bsprintf(tempbuf,"autoload/%s/%s",fn,findfiles->name); initprintf("Using group file '%s'.\n",tempbuf); initgroupfile(tempbuf); findfiles = findfiles->next; }
    Bsprintf(tempbuf,"autoload/%s",fn);
    getfilenames(tempbuf,"*.pk3");
    while (findfiles) { Bsprintf(tempbuf,"autoload/%s/%s",fn,findfiles->name); initprintf("Using group file '%s'.\n",tempbuf); initgroupfile(tempbuf); findfiles = findfiles->next; }
}

int parsegroupfiles(scriptfile *script)
{
    int tokn;
    char *cmdtokptr;

    tokenlist grptokens[] =
    {
        { "include",         T_INCLUDE },
        { "#include",        T_INCLUDE },
        { "loadgrp",         T_LOADGRP },
        { "noautoload",      T_NOAUTOLOAD }
    };

    while (1)
    {
        tokn = getatoken(script,grptokens,sizeof(grptokens)/sizeof(tokenlist));
        cmdtokptr = script->ltextptr;
        switch (tokn)
        {
        case T_LOADGRP:
        {
            char *fn;

            pathsearchmode = 1;
            if (!scriptfile_getstring(script,&fn))
            {
                int j = initgroupfile(fn);

                if (j == -1)
                    initprintf("Could not find group file '%s'.\n",fn);
                else
                {
                    initprintf("Using group file '%s'.\n",fn);
                    if (!NoAutoLoad)
                        autoloadgrps(fn);
                }

            }
            pathsearchmode = 0;
        }
        break;
        case T_INCLUDE:
        {
            char *fn;
            if (!scriptfile_getstring(script,&fn))
            {
                scriptfile *included;

                included = scriptfile_fromfile(fn);
                if (!included)
                {
                    initprintf("Warning: Failed including %s on line %s:%d\n",
                               fn, script->filename,scriptfile_getlinum(script,cmdtokptr));
                }
                else
                {
                    parsegroupfiles(included);
                    scriptfile_close(included);
                }
            }
            break;
        }
        break;
        case T_NOAUTOLOAD:
            NoAutoLoad = 1;
            break;
        case T_EOF:
            return(0);
        default:
            break;
        }
    }
    return 0;
}

int loadgroupfiles(char *fn)
{
    scriptfile *script;

    script = scriptfile_fromfile(fn);
    if (!script) return -1;

    parsegroupfiles(script);

    scriptfile_close(script);
    scriptfile_clearsymbols();

    return 0;
}

int parsetilegroups(scriptfile *script)
{
    int tokn;
    char *cmdtokptr;

    tokenlist tgtokens[] =
    {
        { "include",         T_INCLUDE          },
        { "#include",        T_INCLUDE          },
        { "define",          T_DEFINE           },
        { "#define",         T_DEFINE           },
        { "tilegroup",       T_TILEGROUP        },
    };

    while (1)
    {
        tokn = getatoken(script,tgtokens,sizeof(tgtokens)/sizeof(tokenlist));
        cmdtokptr = script->ltextptr;
        switch (tokn)
        {
        case T_INCLUDE:
        {
            char *fn;
            if (!scriptfile_getstring(script,&fn))
            {
                scriptfile *included;

                included = scriptfile_fromfile(fn);
                if (!included)
                {
                    initprintf("Warning: Failed including %s on line %s:%d\n",
                               fn, script->filename,scriptfile_getlinum(script,cmdtokptr));
                }
                else
                {
                    parsetilegroups(included);
                    scriptfile_close(included);
                }
            }
            break;
        }
        case T_DEFINE:
        {
            char *name;
            int number;

            if (scriptfile_getstring(script,&name)) break;
            if (scriptfile_getsymbol(script,&number)) break;
            if (scriptfile_addsymbolvalue(name,number) < 0)
                initprintf("Warning: Symbol %s was NOT redefined to %d on line %s:%d\n",
                           name,number,script->filename,scriptfile_getlinum(script,cmdtokptr));
            break;
        }
        case T_TILEGROUP:
        {
            char *end, *name;
            int i;

            if (tile_groups >= MAX_TILE_GROUPS) break;
            if (scriptfile_getstring(script,&name)) break;
            if (scriptfile_getbraces(script,&end)) break;

            s_TileGroups[tile_groups].pIds = Bcalloc(MAX_TILE_GROUP_ENTRIES,sizeof(int));
            s_TileGroups[tile_groups].szText = Bstrdup(name);

            while (script->textptr < end)
            {
                tokenlist tgtokens2[] =
                {
                    { "tilegroup",  T_TILEGROUP   },
                    { "tile",       T_TILE        },
                    { "tilerange",  T_TILERANGE   },
                    { "hotkey",     T_HOTKEY      },
                    { "tiles",      T_TILES       },
                };

                int token = getatoken(script,tgtokens2,sizeof(tgtokens2)/sizeof(tokenlist));
                switch (token)
                {
                case T_TILE:
                {
                    if (scriptfile_getsymbol(script,&i)) break;
                    if (i >= 0 && i < MAXTILES && s_TileGroups[tile_groups].nIds < MAX_TILE_GROUP_ENTRIES)
                        s_TileGroups[tile_groups].pIds[s_TileGroups[tile_groups].nIds++] = i;
//                    OSD_Printf("added tile %d to group %d\n",i,g);
                    break;
                }
                case T_TILERANGE:
                {
                    int j;
                    if (scriptfile_getsymbol(script,&i)) break;
                    if (scriptfile_getsymbol(script,&j)) break;
                    if (i < 0 || i >= MAXTILES || j < 0 || j >= MAXTILES) break;
                    while (s_TileGroups[tile_groups].nIds < MAX_TILE_GROUP_ENTRIES && i <= j)
                    {
                        s_TileGroups[tile_groups].pIds[s_TileGroups[tile_groups].nIds++] = i++;
//                        OSD_Printf("added tile %d to group %d\n",i,g);
                    }
                    break;
                }
                case T_HOTKEY:
                {
                    char *c;
                    if (scriptfile_getstring(script,&c)) break;
                    s_TileGroups[tile_groups].key1 = Btoupper(c[0]);
                    s_TileGroups[tile_groups].key2 = Btolower(c[0]);
                    break;
                }
                case T_TILES:
                {
                    char *end2;
                    if (scriptfile_getbraces(script,&end2)) break;
                    while (script->textptr < end2-1)
                    {
                        if (!scriptfile_getsymbol(script,&i))
                        {
                            if (i >= 0 && i < MAXTILES && s_TileGroups[tile_groups].nIds < MAX_TILE_GROUP_ENTRIES)
                                s_TileGroups[tile_groups].pIds[s_TileGroups[tile_groups].nIds++] = i;
                            //                    OSD_Printf("added tile %d to group %d\n",i,g);
                        }
                    }
                    break;
                }
                }
            }
            s_TileGroups[tile_groups].pIds = Brealloc(s_TileGroups[tile_groups].pIds,s_TileGroups[tile_groups].nIds*sizeof(int));
            tile_groups++;
            break;
        }
        case T_EOF:
            return(0);
        default:
            break;
        }
    }
    return 0;
}

int loadtilegroups(char *fn)
{
    int i;
    scriptfile *script;
    TileGroup blank = { NULL,     0,  NULL,     0, 0 };

    script = scriptfile_fromfile(fn);
    if (!script) return -1;

    for (i = 0; i < MAX_TILE_GROUPS; i++)
    {
        Bmemcpy(&s_TileGroups[i],&blank,sizeof(blank));
    }

    parsetilegroups(script);

    scriptfile_close(script);
    scriptfile_clearsymbols();

    return 0;
}

int ExtInit(void)
{
    int rv = 0;
    int i;
    char cwd[BMAX_PATH];

#if defined(__linux__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
    addsearchpath("/usr/share/games/jfduke3d");
    addsearchpath("/usr/local/share/games/jfduke3d");
    addsearchpath("/usr/share/games/eduke32");
    addsearchpath("/usr/local/share/games/eduke32");
#elif defined(__APPLE__)
    addsearchpath("/Library/Application Support/JFDuke3D");
    addsearchpath("/Library/Application Support/EDuke32");
#endif

    if (getcwd(cwd,BMAX_PATH)) addsearchpath(cwd);

    if (CommandPaths)
    {
        struct strllist *s;
        while (CommandPaths)
        {
            s = CommandPaths->next;
            addsearchpath(CommandPaths->str);

            free(CommandPaths->str);
            free(CommandPaths);
            CommandPaths = s;
        }
    }

#if defined(_WIN32)
    if (!access("user_profiles_enabled", F_OK))
#else
    if (usecwd == 0 && access("user_profiles_disabled", F_OK))
#endif
    {
        char cwd[BMAX_PATH];
        char *homedir;
        int asperr;

        if ((homedir = Bgethomedir()))
        {
            Bsnprintf(cwd,sizeof(cwd),"%s/"
#if defined(_WIN32)
                      "EDuke32 Settings"
#elif defined(__APPLE__)
                      "Library/Application Support/EDuke32"
#else
                      ".eduke32"
#endif
                      ,homedir);
            asperr = addsearchpath(cwd);
            if (asperr == -2)
            {
                if (Bmkdir(cwd,S_IRWXU) == 0) asperr = addsearchpath(cwd);
                else asperr = -1;
            }
            if (asperr == 0)
                chdir(cwd);
            Bfree(homedir);
        }
    }

    // JBF 20031220: Because it's annoying renaming GRP files whenever I want to test different game data
    if (getenv("DUKE3DGRP"))
    {
        duke3dgrp = getenv("DUKE3DGRP");
        initprintf("Using %s as main GRP file\n", duke3dgrp);
    }

    i = initgroupfile(duke3dgrp);

    if (!NoAutoLoad)
    {
        getfilenames("autoload","*.grp");
        while (findfiles) { Bsprintf(tempbuf,"autoload/%s",findfiles->name); initprintf("Using group file '%s'.\n",tempbuf); initgroupfile(tempbuf); findfiles = findfiles->next; }
        getfilenames("autoload","*.zip");
        while (findfiles) { Bsprintf(tempbuf,"autoload/%s",findfiles->name); initprintf("Using group file '%s'.\n",tempbuf); initgroupfile(tempbuf); findfiles = findfiles->next; }
        getfilenames("autoload","*.pk3");
        while (findfiles) { Bsprintf(tempbuf,"autoload/%s",findfiles->name); initprintf("Using group file '%s'.\n",tempbuf); initgroupfile(tempbuf); findfiles = findfiles->next; }

        if (i != -1)
            autoloadgrps(duke3dgrp);
    }

    if (getenv("DUKE3DDEF"))
    {
        defsfilename = getenv("DUKE3DDEF");
        initprintf("Using '%s' as definitions file\n", defsfilename);
    }
    loadgroupfiles(defsfilename);

    {
        struct strllist *s;
        int j;

        pathsearchmode = 1;
        while (CommandGrps)
        {
            s = CommandGrps->next;
            j = initgroupfile(CommandGrps->str);
            if (j == -1) initprintf("Could not find group file '%s'.\n",CommandGrps->str);
            else
            {
                initprintf("Using group file '%s'.\n",CommandGrps->str);
                if (!NoAutoLoad)
                    autoloadgrps(CommandGrps->str);
            }

            free(CommandGrps->str);
            free(CommandGrps);
            CommandGrps = s;
        }
        pathsearchmode = 0;
    }

    bpp = 32;

#if defined(POLYMOST) && defined(USE_OPENGL)
    glusetexcache = glusetexcachecompression = -1;

    initprintf("Using config file '%s'.\n",setupfilename);
    if (loadsetup(setupfilename) < 0) initprintf("Configuration file not found, using defaults.\n"), rv = 1;

    if (glusetexcache == -1 || glusetexcachecompression == -1)
    {
        int i;
#if 1
        i=wm_ynbox("Texture Caching",
                   "Would you like to enable the on-disk texture cache? This feature will use an undetermined amount of space "
                   "on your hard disk to store textures in your video card's native format, enabling them to load dramatically "
                   "faster after the first time they are loaded.\n\n"
                   "You will generally want to say 'yes' here, especially if using the HRP.");
#else
        i = 1;
#endif
        if (i)
            glusetexcompr = glusetexcache = glusetexcachecompression = 1;
        else glusetexcache = glusetexcachecompression = 0;
    }
#endif

    Bmemcpy((void *)buildkeys,(void *)keys,NUMBUILDKEYS);   //Trick to make build use setup.dat keys

    if (initengine())
    {
        initprintf("There was a problem initializing the engine.\n");
        return -1;
    }

    kensplayerheight = 40; //32
    zmode = 2;
    zlock = kensplayerheight<<8;
    defaultspritecstat = 0;

    ReadGamePalette();
    //  InitWater();

    InitCustomColors();

    getmessageleng = 0;
    getmessagetimeoff = 0;

    Bstrcpy(apptitle, "Mapster32"VERSION"");
    autosavetimer = totalclock+120*180;

#if defined(_WIN32) && defined(DUKEOSD)
    OSD_SetFunctions(
        GAME_drawosdchar,
        GAME_drawosdstr,
        GAME_drawosdcursor,
        GAME_getcolumnwidth,
        GAME_getrowheight,
        GAME_clearbackground,
        (int(*)(void))GetTime,
        NULL
    );
#endif

    OSD_SetParameters(0,2, 0,0, 4,0);
    registerosdcommands();

    loadtilegroups("tiles.cfg");

    return rv;
}

#ifdef RENDERTYPEWIN
void app_crashhandler(void)
{
    if (levelname[0])
    {
        char *f;
        fixspritesectors();   //Do this before saving!
        updatesector(startposx,startposy,&startsectnum);
        if (pathsearchmode) f = levelname;
        else
        {
            // virtual filesystem mode can't save to directories so drop the file into
            // the current directory
            f = strrchr(levelname, '/');
            if (!f) f = levelname; else f++;
        }
        ExtPreSaveMap();
        saveboard(f,&startposx,&startposy,&startposz,&startang,&startsectnum);
        ExtSaveMap(f);
        message("Board saved");
        asksave = 0;
        keystatus[0x1f] = 0;
    }
}
#endif

void ExtUnInit(void)
{
    int i;
    // setvmode(0x03);
    uninitgroupfile();
    writesetup(setupfilename);

    for (i = 0; (unsigned)i < MAX_TILE_GROUPS; i++)
    {
        if (s_TileGroups[i].pIds != NULL)
            Bfree(s_TileGroups[i].pIds);
        if (s_TileGroups[i].szText != NULL)
            Bfree(s_TileGroups[i].szText);
    }
}

void ExtPreCheckKeys(void) // just before drawrooms
{
    if (qsetmode == 200)    //In 3D mode
    {
        if (floor_over_floor) SE40Code(posx,posy,posz,ang,horiz);
        if (purpleon) clearview(255);
        if (sidemode != 0)
        {
            lockbyte4094 = 1;
            if (waloff[4094] == 0)
                allocache(&waloff[4094],320L*200L,&lockbyte4094);
            setviewtotile(4094,320L,200L);
            searchx ^= searchy;
            searchy ^= searchx;
            searchx ^= searchy;
            searchx = ydim-1-searchx;
        }
    }
}

void ExtAnalyzeSprites(void)
{
    int i, k;
    spritetype *tspr;
    int frames=0, l;

    for (i=0,tspr=&tsprite[0];i<spritesortcnt;i++,tspr++)
    {
        frames=0;

        if ((nosprites==1||nosprites==3)&&tspr->picnum<11) tspr->xrepeat=0;

        if (nosprites==1||nosprites==3)
            switch (tspr->picnum)
            {
            case SEENINE :
                tspr->xrepeat=0;
            }

        if (shadepreview && !(tspr->cstat & 16))
        {
            if (sector[tspr->sectnum].ceilingstat&1)
                l = sector[tspr->sectnum].ceilingshade;
            else
            {
                l = sector[tspr->sectnum].floorshade;
                if (sector[tspr->sectnum].floorpal != 0 && sector[tspr->sectnum].floorpal < num_tables)
                    tspr->pal=sector[tspr->sectnum].floorpal;
            }
            if (l < -127) l = -127;
            if (l > 126) l =  127;

            tspr->shade = l;

        }

        switch (tspr->picnum)
        {
            // 5-frame walk
        case 1550 :             // Shark
            frames=5;
            // 2-frame walk
        case 1445 :             // duke kick
        case LIZTROOPDUCKING :
        case 2030 :            // pig shot
        case OCTABRAIN :
        case PIGCOPDIVE :
        case 2190 :            // liz capt shot
        case BOSS1SHOOT :
        case BOSS1LOB :
        case LIZTROOPSHOOT :
            if (frames==0) frames=2;

            // 4-frame walk
        case 1491 :             // duke crawl
        case LIZTROOP :
        case LIZTROOPRUNNING :
        case PIGCOP :
        case LIZMAN :
        case BOSS1 :
        case BOSS2 :
        case BOSS3 :
        case BOSS4 :
        case NEWBEAST:
            if (frames==0) frames=4;
        case LIZTROOPJETPACK :
        case DRONE :
        case COMMANDER :
        case TANK :
        case RECON :
            if (frames==0) frames = 10;
        case CAMERA1:
        case APLAYER :
            if (frames==0) frames=1;
        case GREENSLIME :
        case EGG :
        case PIGCOPSTAYPUT :
        case LIZMANSTAYPUT:
        case LIZTROOPSTAYPUT :
        case LIZMANSPITTING :
        case LIZMANFEEDING :
        case LIZMANJUMP :
            if (skill!=4)
            {
                if (tspr->lotag>skill+1)
                {
                    tspr->xrepeat=0;
                    tspr->cstat=32768;
                    break;
                }
            }
            if (nosprites==2||nosprites==3)
            {
                tspr->xrepeat=0;
                tspr->cstat=32768;
            }
            //                else tspr->cstat&=32767;

#if defined(USE_OPENGL) && defined(POLYMOST)
            if (!usemodels || md_tilehasmodel(tspr->picnum,tspr->pal) < 0)
#endif
            {
                if (frames!=0)
                {
                    if (frames==10) frames=0;
                    k = getangle(tspr->x-posx,tspr->y-posy);
                    k = (((tspr->ang+3072+128-k)&2047)>>8)&7;
                    //This guy has only 5 pictures for 8 angles (3 are x-flipped)
                    if (k <= 4)
                    {
                        tspr->picnum += k;
                        tspr->cstat &= ~4;   //clear x-flipping bit
                    }
                    else
                    {
                        tspr->picnum += 8-k;
                        tspr->cstat |= 4;    //set x-flipping bit
                    }
                }

                if (frames==2) tspr->picnum+=((((4-(totalclock>>5)))&1)*5);
                if (frames==4) tspr->picnum+=((((4-(totalclock>>5)))&3)*5);
                if (frames==5) tspr->picnum+=(((totalclock>>5)%5))*5;

                if (tilesizx[tspr->picnum] == 0)
                    tspr->picnum -= 5;       //Hack, for actors
            }
            break;
        default:
            break;

        }
    }
}

static void Keys2d3d(void)
{
    int i, j;
    if (keystatus[KEYSC_QUOTE]==1 && keystatus[0x1e]==1) // ' a
    {
        keystatus[0x1e] = 0;
        autosave=!autosave;
        if (autosave) message("Autosave ON");
        else message("Autosave OFF");
    }

    if (keystatus[KEYSC_QUOTE]==1 && keystatus[KEYSC_N]==1) // ' n
    {
        keystatus[KEYSC_N] = 0;
        noclip=!noclip;
        if (noclip) message("Clipping disabled");
        else message("Clipping enabled");
    }

    if ((totalclock > autosavetimer) && (autosave))
    {
        if (asksave)
        {
            fixspritesectors();   //Do this before saving!
            //             updatesector(startposx,startposy,&startsectnum);
            ExtPreSaveMap();
            saveboard("autosave.map",&startposx,&startposy,&startposz,&startang,&startsectnum);
            ExtSaveMap("autosave.map");
            message("Board autosaved to AUTOSAVE.MAP");
        }
        autosavetimer = totalclock+120*180;
    }

    if ((keystatus[0x1d]|keystatus[0x9d]) > 0)  //CTRL
        if (keystatus[0x1f] > 0) // S
        {
            if (levelname[0])
            {
                char *f;
                fixspritesectors();   //Do this before saving!
                updatesector(startposx,startposy,&startsectnum);
                if (pathsearchmode) f = levelname;
                else
                {
                    // virtual filesystem mode can't save to directories so drop the file into
                    // the current directory
                    f = strrchr(levelname, '/');
                    if (!f) f = levelname; else f++;
                }
                ExtPreSaveMap();
                saveboard(f,&startposx,&startposy,&startposz,&startang,&startsectnum);
                ExtSaveMap(f);
                message("Board saved");
                asksave = 0;
                keystatus[0x1f] = 0;
            }
        }

    if (keystatus[buildkeys[14]] > 0)  // Enter
    {
        getmessageleng = 0;
        getmessagetimeoff = 0;
#if defined(_WIN32) && defined(DUKEOSD)
        if (qsetmode == 200)
        {
            OSD_SetFunctions(
                NULL,
                NULL,
                NULL,
                NULL,
                NULL,
                NULL,
                (int(*)(void))GetTime,
                NULL
            );
        }
        else
        {
            OSD_SetFunctions(
                GAME_drawosdchar,
                GAME_drawosdstr,
                GAME_drawosdcursor,
                GAME_getcolumnwidth,
                GAME_getrowheight,
                GAME_clearbackground,
                (int(*)(void))GetTime,
                NULL
            );
        }
#endif
    }

    if (getmessageleng > 0)
    {
        charsperline = 64;
        //if (dimensionmode[snum] == 2) charsperline = 80;
        if (qsetmode == 200)
        {
            for (i=0;i<=getmessageleng;i+=charsperline)
            {
                for (j=0;j<charsperline;j++)
                    tempbuf[j] = getmessage[i+j];
                if (getmessageleng < i+charsperline)
                    tempbuf[(getmessageleng-i)] = 0;
                else
                    tempbuf[charsperline] = 0;
                begindrawing();
                if (tempbuf[charsperline] != 0)
                {
                    printext256((xdimgame>>6)+2,((i/charsperline)<<3)+(ydimgame-(ydimgame>>3))-(((getmessageleng-1)/charsperline)<<3)+2,0,-1,tempbuf,xdimgame>640?0:1);
                    printext256((xdimgame>>6),((i/charsperline)<<3)+(ydimgame-(ydimgame>>3))-(((getmessageleng-1)/charsperline)<<3),whitecol,-1,tempbuf,xdimgame>640?0:1);
                }
                enddrawing();
            }
        }
        else printmessage16(getmessage);
        if (totalclock > getmessagetimeoff)
            getmessageleng = 0;
    }

}

void ExtCheckKeys(void)
{
    readmousebstatus(&bstatus);
    Keys2d3d();
    if (qsetmode == 200)    //In 3D mode
    {
        Keys3d();
        if (sidemode != 1)
        {
            editinput();
            m32_showmouse();
        }
        return;
    }
    Keys2d();
}

void faketimerhandler(void)
{
    int i, dist;
    int hiz, hihit, loz, lohit, oposx, oposy;
    short hitwall, daang;

    counter++;
    if (counter>=5) counter=0;

    sampletimer();
    if (totalclock < ototalclock+TICSPERFRAME || qsetmode != 200 || sidemode != 1)
        return;
    ototalclock = totalclock;

    oposx = posx;
    oposy = posy;
    hitwall = clipmove(&posx,&posy,&posz,&cursectnum,xvel,yvel,128L,4L<<8,4L<<8,0);
    xvel = ((posx-oposx)<<14);
    yvel = ((posy-oposy)<<14);

    yvel += 80000;
    if ((hitwall&0xc000) == 32768)
    {
        hitwall &= (MAXWALLS-1);
        i = wall[hitwall].point2;
        daang = getangle(wall[i].x-wall[hitwall].x,wall[i].y-wall[hitwall].y);

        xvel -= (xvel>>4);
        if (xvel < 0) xvel++;
        if (xvel > 0) xvel--;

        yvel -= (yvel>>4);
        if (yvel < 0) yvel++;
        if (yvel > 0) yvel--;

        i = 4-keystatus[buildkeys[4]];
        xvel += mulscale(vel,(int)sintable[(ang+512)&2047],i);
        yvel += mulscale(vel,(int)sintable[ang&2047],i);

        if (((daang-ang)&2047) < 1024)
            ang = ((ang+((((daang-ang)&2047)+24)>>4))&2047);
        else
            ang = ((ang-((((ang-daang)&2047)+24)>>4))&2047);

        timoff = ototalclock;
    }
    else
    {
        if (ototalclock > timoff+32)
            ang = ((ang+((timoff+32-ototalclock)>>4))&2047);
    }

    getzrange(posx,posy,posz,cursectnum,&hiz,&hihit,&loz,&lohit,128L,0);

    oposx -= posx;
    oposy -= posy;

    dist = ksqrt(oposx*oposx+oposy*oposy);
    if (ototalclock > timoff+32) dist = 0;

    daang = mulscale(dist,angvel,9);
    posz += (daang<<6);
    if (posz > loz-(4<<8)) posz = loz-(4<<8), hvel = 0;
    if (posz < hiz+(4<<8)) posz = hiz+(4<<8), hvel = 0;

    horiz = ((horiz*7+(100-(daang>>1)))>>3);
    if (horiz < 100) horiz++;
    if (horiz > 100) horiz--;

    if (keystatus[KEYSC_QUOTE]==1 && keystatus[0x06]==1) // ' 5
    {
        keystatus[0x06]=0;
        editstatus = 1;
        sidemode = 2;
    }
}

static void SetBOSS1Palette()
{
    if (acurpalette==3) return;
    acurpalette=3;
    kensetpalette(BOSS1palette);
}

/*
static void SetSLIMEPalette()
{
    if (acurpalette==2) return;
    acurpalette=2;
    kensetpalette(SLIMEpalette);
}
*/

static void SetWATERPalette()
{
    if (acurpalette==1) return;
    acurpalette=1;
    kensetpalette(WATERpalette);
}


static void SetGAMEPalette()
{
    if (acurpalette==0) return;
    acurpalette=0;
    kensetpalette(GAMEpalette);
}

static void kensetpalette(char *vgapal)
{
    int i;
    char vesapal[1024];

    for (i=0;i<256;i++)
    {
        vesapal[i*4+0] = vgapal[i*3+2];
        vesapal[i*4+1] = vgapal[i*3+1];
        vesapal[i*4+2] = vgapal[i*3+0];
        vesapal[i*4+3] = 0;
    }
    setpalette(0L,256L,vesapal);
}

static void SearchSectorsForward()
{
    int ii=0;
    if (cursector_lotag!=0)
    {
        if (cursectornum<MAXSECTORS) cursectornum++;
        for (ii=cursectornum;ii<=MAXSECTORS;ii++)
        {
            if (sector[ii].lotag==cursector_lotag)
            {
                posx=wall[sector[ii].wallptr].x;
                posy=wall[sector[ii].wallptr].y;
                printmessage16("> Sector search: found");
                //            cursectornum++;
                keystatus[0x1b]=0; // ]
                return;
            }
            cursectornum++;
        }
    }
    printmessage16("> Sector search: none found");
}

static void SearchSectorsBackward()
{
    int ii=0;
    if (cursector_lotag!=0)
    {
        if (cursectornum>0) cursectornum--;
        for (ii=cursectornum;ii>=0;ii--)
        {
            if (sector[ii].lotag==cursector_lotag)
            {
                posx=wall[sector[ii].wallptr].x;
                posy=wall[sector[ii].wallptr].y;
                printmessage16("< Sector search: found");
                //            cursectornum--;
                keystatus[0x1a]=0; // [
                return;
            }
            cursectornum--;
        }
    }
    printmessage16("< Sector search: none found");
}

// Build edit originally by Ed Coolidge <semicharm@earthlink.net>
static void EditSectorData(short sectnum)
{
    char disptext[80];
    char edittext[80];
    int col=1, row=0, rowmax = 6, dispwidth = 24, editval = 0, i = -1;
    int xpos = 200, ypos = ydim-STATUS2DSIZ+48;

    disptext[dispwidth] = 0;
    clearmidstatbar16();
    showsectordata(sectnum);

    begindrawing();
    while (keystatus[1] == 0)
    {
        if (handleevents())
        {
            if (quitevent) quitevent = 0;
        }
        idle();
        printmessage16("Edit mode, press <Esc> to exit");
        if (keystatus[0xd0] > 0)
        {
            if (row < rowmax)
            {
                printext16(xpos,ypos+row*8,11,0,disptext,0);
                row++;
            }
            keystatus[0xd0] = 0;
        }
        if (keystatus[0xc8] > 0)
        {
            if (row > 0)
            {
                printext16(xpos,ypos+row*8,11,0,disptext,0);
                row--;
            }
            keystatus[0xc8] = 0;
        }
        if (keystatus[0xcb] > 0)
        {
            if (col == 2)
            {
                printext16(xpos,ypos+row*8,11,0,disptext,0);
                col = 1;
                xpos = 200;
                rowmax = 6;
                dispwidth = 24;
                disptext[dispwidth] = 0;
                if (row > rowmax) row = rowmax;
            }
            keystatus[0xcb] = 0;
        }
        if (keystatus[0xcd] > 0)
        {
            if (col == 1)
            {
                printext16(xpos,ypos+row*8,11,0,disptext,0);
                col = 2;
                xpos = 400;
                rowmax = 6;
                dispwidth = 24;
                disptext[dispwidth] = 0;
                if (row > rowmax) row = rowmax;
            }
            keystatus[0xcd] = 0;
        }
        if (keystatus[0x1c] > 0)
        {
            keystatus[0x1c] = 0;
            editval = 1;
        }

        if (col == 1)
        {
            switch (row)
            {
            case 0:
                for (i=Bsprintf(disptext,"Flags (hex): %x",sector[sectnum].ceilingstat); i < dispwidth; i++) disptext[i] = ' ';
                Bsprintf(edittext,"Sector %d Ceiling Flags: ",sectnum);
                if (editval)
                {
                    printmessage16(edittext);
                    sector[sectnum].ceilingstat = (short)getnumber16(edittext,(int)sector[sectnum].ceilingstat,65536L,0);
                }
                break;
            case 1:
                for (i=Bsprintf(disptext,"(X,Y)pan: %d, %d",sector[sectnum].ceilingxpanning,sector[sectnum].ceilingypanning); i < dispwidth; i++) disptext[i] = ' ';
                if (editval)
                {
                    Bsprintf(edittext,"Sector %d Ceiling X Pan: ",sectnum);
                    printmessage16(edittext);
                    sector[sectnum].ceilingxpanning = (char)getnumber16(edittext,(int)sector[sectnum].ceilingxpanning,256L,0);
                    Bsprintf(edittext,"Sector %d Ceiling Y Pan: ",sectnum);
                    printmessage16(edittext);
                    sector[sectnum].ceilingypanning = (char)getnumber16(edittext,(int)sector[sectnum].ceilingypanning,256L,0);
                }
                break;
            case 2:
                for (i=Bsprintf(disptext,"Shade byte: %d",sector[sectnum].ceilingshade); i < dispwidth; i++) disptext[i] = ' ';
                Bsprintf(edittext,"Sector %d Ceiling Shade: ",sectnum);
                if (editval)
                {
                    printmessage16(edittext);
                    sector[sectnum].ceilingshade = (char)getnumber16(edittext,(int)sector[sectnum].ceilingshade,128L,1);
                }
                break;

            case 3:
                for (i=Bsprintf(disptext,"Z-coordinate: %d",sector[sectnum].ceilingz); i < dispwidth; i++) disptext[i] = ' ';
                Bsprintf(edittext,"Sector %d Ceiling Z-coordinate: ",sectnum);
                if (editval)
                {
                    printmessage16(edittext);
                    sector[sectnum].ceilingz = getnumber16(edittext,sector[sectnum].ceilingz,8388608,1); //2147483647L,-2147483648L
                }
                break;

            case 4:
                for (i=Bsprintf(disptext,"Tile number: %d",sector[sectnum].ceilingpicnum); i < dispwidth; i++) disptext[i] = ' ';
                Bsprintf(edittext,"Sector %d Ceiling Tile Number: ",sectnum);
                if (editval)
                {
                    printmessage16(edittext);
                    sector[sectnum].ceilingpicnum = (short)getnumber16(edittext,(int)sector[sectnum].ceilingpicnum,MAXTILES,0);
                }
                break;

            case 5:
                for (i=Bsprintf(disptext,"Ceiling heinum: %d",sector[sectnum].ceilingheinum); i < dispwidth; i++) disptext[i] = ' ';
                Bsprintf(edittext,"Sector %d Ceiling Heinum: ",sectnum);
                if (editval)
                {
                    printmessage16(edittext);
                    sector[sectnum].ceilingheinum = (short)getnumber16(edittext,(int)sector[sectnum].ceilingheinum,65536L,1);
                }
                break;

            case 6:
                for (i=Bsprintf(disptext,"Palookup number: %d",sector[sectnum].ceilingpal); i < dispwidth; i++) disptext[i] = ' ';
                Bsprintf(edittext,"Sector %d Ceiling Palookup Number: ",sectnum);
                if (editval)
                {
                    printmessage16(edittext);
                    sector[sectnum].ceilingpal = (char)getnumber16(edittext,(int)sector[sectnum].ceilingpal,MAXPALOOKUPS,0);
                }
                break;
            }
        }
        if (col == 2)
        {
            switch (row)
            {
            case 0:
                for (i=Bsprintf(disptext,"Flags (hex): %x",sector[sectnum].floorstat); i < dispwidth; i++) disptext[i] = ' ';
                Bsprintf(edittext,"Sector %d Floor Flags: ",sectnum);
                if (editval)
                {
                    printmessage16(edittext);
                    sector[sectnum].floorstat = (short)getnumber16(edittext,(int)sector[sectnum].floorstat,65536L,0);
                }
                break;

            case 1:
                for (i=Bsprintf(disptext,"(X,Y)pan: %d, %d",sector[sectnum].floorxpanning,sector[sectnum].floorypanning); i < dispwidth; i++) disptext[i] = ' ';
                if (editval)
                {
                    Bsprintf(edittext,"Sector %d Floor X Pan: ",sectnum);
                    printmessage16(edittext);
                    sector[sectnum].floorxpanning = (char)getnumber16(edittext,(int)sector[sectnum].floorxpanning,256L,0);
                    Bsprintf(edittext,"Sector %d Floor Y Pan: ",sectnum);
                    printmessage16(edittext);
                    sector[sectnum].floorypanning = (char)getnumber16(edittext,(int)sector[sectnum].floorypanning,256L,0);
                }
                break;

            case 2:
                for (i=Bsprintf(disptext,"Shade byte: %d",sector[sectnum].floorshade); i < dispwidth; i++) disptext[i] = ' ';
                Bsprintf(edittext,"Sector %d Floor Shade: ",sectnum);
                if (editval)
                {
                    printmessage16(edittext);
                    sector[sectnum].floorshade = (char)getnumber16(edittext,(int)sector[sectnum].floorshade,128L,1L);
                }
                break;

            case 3:
                for (i=Bsprintf(disptext,"Z-coordinate: %d",sector[sectnum].floorz); i < dispwidth; i++) disptext[i] = ' ';
                Bsprintf(edittext,"Sector %d Floor Z-coordinate: ",sectnum);
                if (editval)
                {
                    printmessage16(edittext);
                    sector[sectnum].floorz = getnumber16(edittext,sector[sectnum].floorz,8388608L,1); //2147483647L,-2147483648L
                }
                break;

            case 4:
                for (i=Bsprintf(disptext,"Tile number: %d",sector[sectnum].floorpicnum); i < dispwidth; i++) disptext[i] = ' ';
                Bsprintf(edittext,"Sector %d Floor Tile Number: ",sectnum);
                if (editval)
                {
                    printmessage16(edittext);
                    sector[sectnum].floorpicnum = (short)getnumber16(edittext,(int)sector[sectnum].floorpicnum,MAXTILES,0);
                }
                break;
            case 5:
                for (i=Bsprintf(disptext,"Floor heinum: %d",sector[sectnum].floorheinum); i < dispwidth; i++) disptext[i] = ' ';
                Bsprintf(edittext,"Sector %d Flooring Heinum: ",sectnum);
                if (editval)
                {
                    printmessage16(edittext);
                    sector[sectnum].floorheinum = (short)getnumber16(edittext,(int)sector[sectnum].floorheinum,65536L,1);
                }
                break;
            case 6:
                for (i=Bsprintf(disptext,"Palookup number: %d",sector[sectnum].floorpal); i < dispwidth; i++) disptext[i] = ' ';
                Bsprintf(edittext,"Sector %d Floor Palookup Number: ",sectnum);
                if (editval)
                {
                    printmessage16(edittext);
                    sector[sectnum].floorpal = (char)getnumber16(edittext,(int)sector[sectnum].floorpal,MAXPALOOKUPS,0);
                }
                break;
            }
        }
        printext16(xpos,ypos+row*8,11,1,disptext,0);
        if (editval)
        {
            editval = 0;
        }
        showframe(1);
    }
    printext16(xpos,ypos+row*8,11,0,disptext,0);
    printmessage16("");
    enddrawing();
    showframe(1);
    keystatus[1] = 0;
}

static void EditWallData(short wallnum)
{
    char disptext[80];
    char edittext[80];
    int row=0, dispwidth = 24, editval = 0, i = -1;
    int xpos = 200, ypos = ydim-STATUS2DSIZ+48;

    disptext[dispwidth] = 0;
    clearmidstatbar16();
    showwalldata(wallnum);
    begindrawing();
    while (keystatus[1] == 0)
    {
        if (handleevents())
        {
            if (quitevent) quitevent = 0;
        }
        idle();
        printmessage16("Edit mode, press <Esc> to exit");
        if (keystatus[0xd0] > 0)
        {
            if (row < 6)
            {
                printext16(xpos,ypos+row*8,11,0,disptext,0);
                row++;
            }
            keystatus[0xd0] = 0;
        }
        if (keystatus[0xc8] > 0)
        {
            if (row > 0)
            {
                printext16(xpos,ypos+row*8,11,0,disptext,0);
                row--;
            }
            keystatus[0xc8] = 0;
        }
        if (keystatus[0x1c] > 0)
        {
            keystatus[0x1c] = 0;
            editval = 1;
        }
        switch (row)
        {
        case 0:
            for (i=Bsprintf(disptext,"Flags (hex): %x",wall[wallnum].cstat); i < dispwidth; i++) disptext[i] = ' ';
            Bsprintf(edittext,"Wall %d Flags: ",wallnum);
            if (editval)
            {
                printmessage16(edittext);
                wall[wallnum].cstat = (short)getnumber16(edittext,(int)wall[wallnum].cstat,65536L,0);
            }
            break;
        case 1:
            for (i=Bsprintf(disptext,"Shade: %d",wall[wallnum].shade); i < dispwidth; i++) disptext[i] = ' ';
            Bsprintf(edittext,"Wall %d Shade: ",wallnum);
            if (editval)
            {
                printmessage16(edittext);
                wall[wallnum].shade = (char)getnumber16(edittext,(int)wall[wallnum].shade,128,1);
            }
            break;
        case 2:
            for (i=Bsprintf(disptext,"Pal: %d",wall[wallnum].pal); i < dispwidth; i++) disptext[i] = ' ';
            Bsprintf(edittext,"Wall %d Pal: ",wallnum);
            if (editval)
            {
                printmessage16(edittext);
                wall[wallnum].pal = (char)getnumber16(edittext,(int)wall[wallnum].pal,MAXPALOOKUPS,0);
            }
            break;
        case 3:
            for (i=Bsprintf(disptext,"(X,Y)repeat: %d, %d",wall[wallnum].xrepeat,wall[wallnum].yrepeat); i < dispwidth; i++) disptext[i] = ' ';
            if (editval)
            {
                Bsprintf(edittext,"Wall %d X Repeat: ",wallnum);
                printmessage16(edittext);
                wall[wallnum].xrepeat = (char)getnumber16(edittext,(int)wall[wallnum].xrepeat,256L,0);
                Bsprintf(edittext,"Wall %d Y Repeat: ",wallnum);
                printmessage16(edittext);
                wall[wallnum].yrepeat = (char)getnumber16(edittext,(int)wall[wallnum].yrepeat,256L,0);
            }
            break;
        case 4:
            for (i=Bsprintf(disptext,"(X,Y)pan: %d, %d",wall[wallnum].xpanning,wall[wallnum].ypanning); i < dispwidth; i++) disptext[i] = ' ';
            if (editval)
            {
                Bsprintf(edittext,"Wall %d X Pan: ",wallnum);
                printmessage16(edittext);
                wall[wallnum].xpanning = (char)getnumber16(edittext,(int)wall[wallnum].xpanning,256L,0);
                Bsprintf(edittext,"Wall %d Y Pan: ",wallnum);
                printmessage16(edittext);
                wall[wallnum].ypanning = (char)getnumber16(edittext,(int)wall[wallnum].ypanning,256L,0);
            }
            break;
        case 5:
            for (i=Bsprintf(disptext,"Tile number: %d",wall[wallnum].picnum); i < dispwidth; i++) disptext[i] = ' ';
            Bsprintf(edittext,"Wall %d Tile number: ",wallnum);
            if (editval)
            {
                printmessage16(edittext);
                wall[wallnum].picnum = (short)getnumber16(edittext,(int)wall[wallnum].picnum,MAXTILES,0);
            }
            break;

        case 6:
            for (i=Bsprintf(disptext,"OverTile number: %d",wall[wallnum].overpicnum); i < dispwidth; i++) disptext[i] = ' ';
            Bsprintf(edittext,"Wall %d OverTile number: ",wallnum);
            if (editval)
            {
                printmessage16(edittext);
                wall[wallnum].overpicnum = (short)getnumber16(edittext,(int)wall[wallnum].overpicnum,MAXTILES,0);
            }
            break;
        }
        printext16(xpos,ypos+row*8,11,1,disptext,0);
        if (editval)
        {
            editval = 0;
            //showwalldata(wallnum);
            //printmessage16("");
        }
        //enddrawing();
        showframe(1);
    }
    //begindrawing();
    printext16(xpos,ypos+row*8,11,0,disptext,0);
    printmessage16("");
    enddrawing();
    showframe(1);
    keystatus[1] = 0;
}

static void EditSpriteData(short spritenum)
{
    char disptext[80];
    char edittext[80];
    int col=0, row=0, rowmax=4, dispwidth = 24, editval = 0, i = -1;
    int xpos = 8, ypos = ydim-STATUS2DSIZ+48;

    disptext[dispwidth] = 0;
    clearmidstatbar16();
    showspritedata(spritenum);

    while (keystatus[1] == 0)
    {
        begindrawing();
        if (handleevents())
        {
            if (quitevent) quitevent = 0;
        }
        idle();
        printmessage16("Edit mode, press <Esc> to exit");
        if (keystatus[0xd0] > 0)
        {
            if (row < rowmax)
            {
                printext16(xpos,ypos+row*8,11,0,disptext,0);
                row++;
            }
            keystatus[0xd0] = 0;
        }
        if (keystatus[0xc8] > 0)
        {
            if (row > 0)
            {
                printext16(xpos,ypos+row*8,11,0,disptext,0);
                row--;
            }
            keystatus[0xc8] = 0;
        }
        if (keystatus[0xcb] > 0)
        {
            switch (col)
            {
            case 1:
            {
                printext16(xpos,ypos+row*8,11,0,disptext,0);
                col = 0;
                xpos = 8;
                rowmax = 4;
                dispwidth = 23;
                disptext[dispwidth] = 0;
                if (row > rowmax) row = rowmax;
            }
            break;
            case 2:
            {
                printext16(xpos,ypos+row*8,11,0,disptext,0);
                col = 1;
                xpos = 200;
                rowmax = 5;
                dispwidth = 24;
                disptext[dispwidth] = 0;
                if (row > rowmax) row = rowmax;
            }
            break;
            }
            keystatus[0xcb] = 0;
        }
        if (keystatus[0xcd] > 0)
        {
            switch (col)
            {
            case 0:
            {
                printext16(xpos,ypos+row*8,11,0,disptext,0);
                col = 1;
                xpos = 200;
                rowmax = 5;
                dispwidth = 24;
                disptext[dispwidth] = 0;
                if (row > rowmax) row = rowmax;
            }
            break;
            case 1:
            {
                printext16(xpos,ypos+row*8,11,0,disptext,0);
                col = 2;
                xpos = 400;
                rowmax = 6;
                dispwidth = 26;
                disptext[dispwidth] = 0;
                if (row > rowmax) row = rowmax;
            }
            break;
            }
            keystatus[0xcd] = 0;
        }
        if (keystatus[0x1c] > 0)
        {
            keystatus[0x1c] = 0;
            editval = 1;
        }
        switch (col)
        {
        case 0:
        {
            switch (row)
            {
            case 0:
            {
                for (i=Bsprintf(disptext,"X-coordinate: %d",sprite[spritenum].x); i < dispwidth; i++) disptext[i] = ' ';
                Bsprintf(edittext,"Sprite %d X-coordinate: ",spritenum);
                if (editval)
                {
                    printmessage16(edittext);
                    sprite[spritenum].x = getnumber16(edittext,sprite[spritenum].x,131072,1);
                }
            }
            break;
            case 1:
            {
                for (i=Bsprintf(disptext,"Y-coordinate: %d",sprite[spritenum].y); i < dispwidth; i++) disptext[i] = ' ';
                Bsprintf(edittext,"Sprite %d Y-coordinate: ",spritenum);
                if (editval)
                {
                    printmessage16(edittext);
                    sprite[spritenum].y = getnumber16(edittext,sprite[spritenum].y,131072,1);
                }
            }
            break;
            case 2:
            {
                for (i=Bsprintf(disptext,"Z-coordinate: %d",sprite[spritenum].z); i < dispwidth; i++) disptext[i] = ' ';
                Bsprintf(edittext,"Sprite %d Z-coordinate: ",spritenum);
                if (editval)
                {
                    printmessage16(edittext);
                    sprite[spritenum].z = getnumber16(edittext,sprite[spritenum].z,8388608,1); //2147483647L,-2147483648L
                }
            }
            break;
            case 3:
            {
                for (i=Bsprintf(disptext,"Sectnum: %d",sprite[spritenum].sectnum); i < dispwidth; i++) disptext[i] = ' ';
                Bsprintf(edittext,"Sprite %d Sectnum: ",spritenum);
                if (editval)
                {
                    printmessage16(edittext);
                    i = getnumber16(edittext,sprite[spritenum].sectnum,MAXSECTORS-1,0);
                    if (i != sprite[spritenum].sectnum)
                        changespritesect(spritenum,i);
                }
            }
            break;
            case 4:
            {
                for (i=Bsprintf(disptext,"Statnum: %d",sprite[spritenum].statnum); i < dispwidth; i++) disptext[i] = ' ';
                Bsprintf(edittext,"Sprite %d Statnum: ",spritenum);
                if (editval)
                {
                    printmessage16(edittext);
                    i = getnumber16(edittext,sprite[spritenum].statnum,MAXSTATUS-1,0);
                    if (i != sprite[spritenum].statnum)
                        changespritestat(spritenum,i);
                }
            }
            break;
            }
        }
        break;
        case 1:
        {
            switch (row)
            {
            case 0:
            {
                for (i=Bsprintf(disptext,"Flags (hex): %x",sprite[spritenum].cstat); i < dispwidth; i++) disptext[i] = ' ';
                Bsprintf(edittext,"Sprite %d Flags: ",spritenum);
                if (editval)
                {
                    printmessage16(edittext);
                    sprite[spritenum].cstat = (short)getnumber16(edittext,(int)sprite[spritenum].cstat,65536L,0);
                }
            }
            break;
            case 1:
            {
                for (i=Bsprintf(disptext,"Shade: %d",sprite[spritenum].shade); i < dispwidth; i++) disptext[i] = ' ';
                Bsprintf(edittext,"Sprite %d Shade: ",spritenum);
                if (editval)
                {
                    printmessage16(edittext);
                    sprite[spritenum].shade = (char)getnumber16(edittext,(int)sprite[spritenum].shade,128,1);
                }
            }
            break;
            case 2:
            {
                for (i=Bsprintf(disptext,"Pal: %d",sprite[spritenum].pal); i < dispwidth; i++) disptext[i] = ' ';
                Bsprintf(edittext,"Sprite %d Pal: ",spritenum);
                if (editval)
                {
                    printmessage16(edittext);
                    sprite[spritenum].pal = (char)getnumber16(edittext,(int)sprite[spritenum].pal,MAXPALOOKUPS,0);
                }
            }
            break;
            case 3:
            {
                for (i=Bsprintf(disptext,"(X,Y)repeat: %d, %d",sprite[spritenum].xrepeat,sprite[spritenum].yrepeat); i < dispwidth; i++) disptext[i] = ' ';
                if (editval)
                {
                    Bsprintf(edittext,"Sprite %d X Repeat: ",spritenum);
                    printmessage16(edittext);
                    sprite[spritenum].xrepeat = (char)getnumber16(edittext,(int)sprite[spritenum].xrepeat,256L,0);
                    Bsprintf(edittext,"Sprite %d Y Repeat: ",spritenum);
                    printmessage16(edittext);
                    sprite[spritenum].yrepeat = (char)getnumber16(edittext,(int)sprite[spritenum].yrepeat,256L,0);
                }
            }
            break;
            case 4:
            {
                for (i=Bsprintf(disptext,"(X,Y)offset: %d, %d",sprite[spritenum].xoffset,sprite[spritenum].yoffset); i < dispwidth; i++) disptext[i] = ' ';
                if (editval)
                {
                    Bsprintf(edittext,"Sprite %d X Offset: ",spritenum);
                    printmessage16(edittext);
                    sprite[spritenum].xoffset = (char)getnumber16(edittext,(int)sprite[spritenum].xoffset,128L,1);
                    Bsprintf(edittext,"Sprite %d Y Offset: ",spritenum);
                    printmessage16(edittext);
                    sprite[spritenum].yoffset = (char)getnumber16(edittext,(int)sprite[spritenum].yoffset,128L,1);
                }
            }
            break;
            case 5:
            {
                for (i=Bsprintf(disptext,"Tile number: %d",sprite[spritenum].picnum); i < dispwidth; i++) disptext[i] = ' ';
                Bsprintf(edittext,"Sprite %d Tile number: ",spritenum);
                if (editval)
                {
                    printmessage16(edittext);
                    sprite[spritenum].picnum = (short)getnumber16(edittext,(int)sprite[spritenum].picnum,MAXTILES,0);
                }
            }
            break;
            }
        }
        break;
        case 2:
        {
            switch (row)
            {
            case 0:
            {
                for (i=Bsprintf(disptext,"Angle (2048 degrees): %d",sprite[spritenum].ang); i < dispwidth; i++) disptext[i] = ' ';
                Bsprintf(edittext,"Sprite %d Angle: ",spritenum);
                if (editval)
                {
                    printmessage16(edittext);
                    sprite[spritenum].ang = (short)getnumber16(edittext,(int)sprite[spritenum].ang,2048L,0);
                }
            }
            break;
            case 1:
            {
                for (i=Bsprintf(disptext,"X-Velocity: %d",sprite[spritenum].xvel); i < dispwidth; i++) disptext[i] = ' ';
                Bsprintf(edittext,"Sprite %d X-Velocity: ",spritenum);
                if (editval)
                {
                    printmessage16(edittext);
                    sprite[spritenum].xvel = getnumber16(edittext,(int)sprite[spritenum].xvel,65536,1);
                }
            }
            break;
            case 2:
            {
                for (i=Bsprintf(disptext,"Y-Velocity: %d",sprite[spritenum].yvel); i < dispwidth; i++) disptext[i] = ' ';
                Bsprintf(edittext,"Sprite %d Y-Velocity: ",spritenum);
                if (editval)
                {
                    printmessage16(edittext);
                    sprite[spritenum].yvel = getnumber16(edittext,(int)sprite[spritenum].yvel,65536,1);
                }
            }
            break;
            case 3:
            {
                for (i=Bsprintf(disptext,"Z-Velocity: %d",sprite[spritenum].zvel); i < dispwidth; i++) disptext[i] = ' ';
                Bsprintf(edittext,"Sprite %d Z-Velocity: ",spritenum);
                if (editval)
                {
                    printmessage16(edittext);
                    sprite[spritenum].zvel = getnumber16(edittext,(int)sprite[spritenum].zvel,65536,1);
                }
            }
            break;
            case 4:
            {
                for (i=Bsprintf(disptext,"Owner: %d",sprite[spritenum].owner); i < dispwidth; i++) disptext[i] = ' ';
                Bsprintf(edittext,"Sprite %d Owner: ",spritenum);
                if (editval)
                {
                    printmessage16(edittext);
                    sprite[spritenum].owner = getnumber16(edittext,(int)sprite[spritenum].owner,MAXSPRITES,0);
                }
            }
            break;
            case 5:
            {
                for (i=Bsprintf(disptext,"Clipdist: %d",sprite[spritenum].clipdist); i < dispwidth; i++) disptext[i] = ' ';
                Bsprintf(edittext,"Sprite %d Clipdist: ",spritenum);
                if (editval)
                {
                    printmessage16(edittext);
                    sprite[spritenum].clipdist = (char)getnumber16(edittext,(int)sprite[spritenum].clipdist,256,0);
                }
            }
            break;
            case 6:
            {
                for (i=Bsprintf(disptext,"Extra: %d",sprite[spritenum].extra); i < dispwidth; i++) disptext[i] = ' ';
                Bsprintf(edittext,"Sprite %d Extra: ",spritenum);
                if (editval)
                {
                    printmessage16(edittext);
                    sprite[spritenum].extra = getnumber16(edittext,(int)sprite[spritenum].extra,65536,1);
                }
            }
            break;
            }
        }
        break;
        }

        printext16(xpos,ypos+row*8,11,1,disptext,0);
        if (editval)
        {
            editval = 0;
        }
        enddrawing();
        showframe(1);
    }
    begindrawing();
    printext16(xpos,ypos+row*8,11,0,disptext,0);
    printmessage16("");
    enddrawing();
    showframe(1);
    keystatus[1] = 0;
}

// Build edit

static void FuncMenuOpts(void)
{
    char snotbuf[80];

    Bsprintf(snotbuf,"Special functions");
    printext16(8,ydim-STATUS2DSIZ+32,11,-1,snotbuf,0);
    Bsprintf(snotbuf,"Replace invalid tiles");
    printext16(8,ydim-STATUS2DSIZ+48,11,-1,snotbuf,0);
    Bsprintf(snotbuf,"Delete all spr of tile #");
    printext16(8,ydim-STATUS2DSIZ+56,11,-1,snotbuf,0);
    Bsprintf(snotbuf,"Global sky shade");
    printext16(8,ydim-STATUS2DSIZ+64,11,-1,snotbuf,0);
    Bsprintf(snotbuf,"Global sky height");
    printext16(8,ydim-STATUS2DSIZ+72,11,-1,snotbuf,0);
    Bsprintf(snotbuf,"Global Z coord shift");
    printext16(8,ydim-STATUS2DSIZ+80,11,-1,snotbuf,0);
    Bsprintf(snotbuf,"Up-size selected sectors");
    printext16(8,ydim-STATUS2DSIZ+88,11,-1,snotbuf,0);
    Bsprintf(snotbuf,"Down-size selected sects");
    printext16(8,ydim-STATUS2DSIZ+96,11,-1,snotbuf,0);
    Bsprintf(snotbuf,"Global shade divide");
    printext16(8,ydim-STATUS2DSIZ+104,11,-1,snotbuf,0);

    Bsprintf(snotbuf,"Global visibility divide");
    printext16(200,ydim-STATUS2DSIZ+48,11,-1,snotbuf,0);
    /*
        Bsprintf(snotbuf,"     (0x%x), (0x%x)",sprite[spritenum].hitag,sprite[spritenum].lotag);
        printext16(8,ydim-STATUS2DSIZ+104,11,-1,snotbuf,0);

        printext16(200,ydim-STATUS2DSIZ+32,11,-1,names[sprite[spritenum].picnum],0);
        Bsprintf(snotbuf,"Flags (hex): %x",sprite[spritenum].cstat);
        printext16(200,ydim-STATUS2DSIZ+48,11,-1,snotbuf,0);
        Bsprintf(snotbuf,"Shade: %d",sprite[spritenum].shade);
        printext16(200,ydim-STATUS2DSIZ+56,11,-1,snotbuf,0);
        Bsprintf(snotbuf,"Pal: %d",sprite[spritenum].pal);
        printext16(200,ydim-STATUS2DSIZ+64,11,-1,snotbuf,0);
        Bsprintf(snotbuf,"(X,Y)repeat: %d, %d",sprite[spritenum].xrepeat,sprite[spritenum].yrepeat);
        printext16(200,ydim-STATUS2DSIZ+72,11,-1,snotbuf,0);
        Bsprintf(snotbuf,"(X,Y)offset: %d, %d",sprite[spritenum].xoffset,sprite[spritenum].yoffset);
        printext16(200,ydim-STATUS2DSIZ+80,11,-1,snotbuf,0);
        Bsprintf(snotbuf,"Tile number: %d",sprite[spritenum].picnum);
        printext16(200,ydim-STATUS2DSIZ+88,11,-1,snotbuf,0);

        Bsprintf(snotbuf,"Angle (2048 degrees): %d",sprite[spritenum].ang);
        printext16(400,ydim-STATUS2DSIZ+48,11,-1,snotbuf,0);
        Bsprintf(snotbuf,"X-Velocity: %d",sprite[spritenum].xvel);
        printext16(400,ydim-STATUS2DSIZ+56,11,-1,snotbuf,0);
        Bsprintf(snotbuf,"Y-Velocity: %d",sprite[spritenum].yvel);
        printext16(400,ydim-STATUS2DSIZ+64,11,-1,snotbuf,0);
        Bsprintf(snotbuf,"Z-Velocity: %d",sprite[spritenum].zvel);
        printext16(400,ydim-STATUS2DSIZ+72,11,-1,snotbuf,0);
        Bsprintf(snotbuf,"Owner: %d",sprite[spritenum].owner);
        printext16(400,ydim-STATUS2DSIZ+80,11,-1,snotbuf,0);
        Bsprintf(snotbuf,"Clipdist: %d",sprite[spritenum].clipdist);
        printext16(400,ydim-STATUS2DSIZ+88,11,-1,snotbuf,0);
        Bsprintf(snotbuf,"Extra: %d",sprite[spritenum].extra);
        printext16(400,ydim-STATUS2DSIZ+96,11,-1,snotbuf,0); */
}

static void FuncMenu(void)
{
    char disptext[80];
    int col=0, row=0, rowmax=7, dispwidth = 24, editval = 0, i = -1, j;
    int xpos = 8, ypos = ydim-STATUS2DSIZ+48;

    disptext[dispwidth] = 0;
    clearmidstatbar16();

    FuncMenuOpts();

    while (!editval && keystatus[1] == 0)
    {
        begindrawing();
        if (handleevents())
        {
            if (quitevent) quitevent = 0;
        }
        idle();
        printmessage16("Select an option, press <Esc> to exit");
        if (keystatus[0xd0] > 0)
        {
            if (row < rowmax)
            {
                printext16(xpos,ypos+row*8,11,0,disptext,0);
                row++;
            }
            keystatus[0xd0] = 0;
        }
        if (keystatus[0xc8] > 0)
        {
            if (row > 0)
            {
                printext16(xpos,ypos+row*8,11,0,disptext,0);
                row--;
            }
            keystatus[0xc8] = 0;
        }
        if (keystatus[0xcb] > 0)
        {
            /*            if (col == 2)
                        {
                            printext16(xpos,ypos+row*8,11,0,disptext,0);
                            col = 1;
                            xpos = 200;
                            rowmax = 6;
                            dispwidth = 24;
                            disptext[dispwidth] = 0;
                            if (row > rowmax) row = rowmax;
                        }
                        else */
            if (col == 1)
            {
                printext16(xpos,ypos+row*8,11,0,disptext,0);
                col = 0;
                xpos = 8;
                rowmax = 7;
                dispwidth = 23;
                disptext[dispwidth] = 0;
                if (row > rowmax) row = rowmax;
            }
            keystatus[0xcb] = 0;
        }
        if (keystatus[0xcd] > 0)
        {
            if (col == 0)
            {
                printext16(xpos,ypos+row*8,11,0,disptext,0);
                col = 1;
                xpos = 200;
                rowmax = 0;
                dispwidth = 24;
                disptext[dispwidth] = 0;
                if (row > rowmax) row = rowmax;
            }
            /*            else if (col == 1)
                        {
                            printext16(xpos,ypos+row*8,11,0,disptext,0);
                            col = 2;
                            xpos = 400;
                            rowmax = 6;
                            dispwidth = 26;
                            disptext[dispwidth] = 0;
                            if (row > rowmax) row = rowmax;
                        } */
            keystatus[0xcd] = 0;
        }
        if (keystatus[0x1c] > 0)
        {
            keystatus[0x1c] = 0;
            editval = 1;
        }
        switch (col)
        {
        case 0:
            switch (row)
            {
            case 0:
            {
                for (i=Bsprintf(disptext,"Replace invalid tiles"); i < dispwidth; i++) disptext[i] = ' ';
                if (editval)
                {
                    j = 0;
                    for (i=0;i<MAXSECTORS;i++)
                    {
                        if (tilesizx[sector[i].ceilingpicnum] <= 0)
                            sector[i].ceilingpicnum = 0,j++;
                        if (tilesizx[sector[i].floorpicnum] <= 0)
                            sector[i].floorpicnum = 0,j++;
                    }
                    for (i=0;i<MAXWALLS;i++)
                    {
                        if (tilesizx[wall[i].picnum] <= 0)
                            wall[i].picnum = 0,j++;
                        if (tilesizx[wall[i].overpicnum] <= 0)
                            wall[i].overpicnum = 0,j++;
                    }
                    for (i=0;i<MAXSPRITES;i++)
                    {
                        if (tilesizx[sprite[i].picnum] <= 0)
                            sprite[i].picnum = 0,j++;
                    }
                    Bsprintf(tempbuf,"Replaced %d invalid tiles",j);
                    printmessage16(tempbuf);
                }
            }
            break;
            case 1:
            {
                for (i=Bsprintf(disptext,"Delete all spr of tile #"); i < dispwidth; i++) disptext[i] = ' ';
                if (editval)
                {
                    Bsprintf(tempbuf,"Delete all sprites of tile #: ");
                    i = getnumber16(tempbuf,-1,MAXSPRITES-1,1);
                    if (i >= 0)
                    {
                        int k = 0;
                        for (j=0;j<MAXSPRITES-1;j++)
                            if (sprite[j].picnum == i)
                                deletesprite(j), k++;
                        Bsprintf(tempbuf,"%d sprite(s) deleted",k);
                        printmessage16(tempbuf);
                    }
                    else printmessage16("Aborted");
                }
            }
            break;
            case 2:
            {
                for (i=Bsprintf(disptext,"Global sky shade"); i < dispwidth; i++) disptext[i] = ' ';
                if (editval)
                {
                    j=getnumber16("Global sky shade:    ",0,128,1);

                    for (i=0;i<numsectors;i++)
                    {
                        if (sector[i].ceilingstat&1)
                            sector[i].ceilingshade = j;
                    }
                    printmessage16("Parallaxed skies adjusted");
                }
            }
            break;
            case 3:
            {
                for (i=Bsprintf(disptext,"Global sky height"); i < dispwidth; i++) disptext[i] = ' ';
                if (editval)
                {
                    j=getnumber16("Global sky height:    ",0,16777216,1);
                    if (j != 0)
                    {
                        for (i=0;i<numsectors;i++)
                        {
                            if (sector[i].ceilingstat&1)
                                sector[i].ceilingz = j;
                        }
                        printmessage16("Parallaxed skies adjusted");
                    }
                    else printmessage16("Aborted");
                }
            }
            break;
            case 4:
            {
                for (i=Bsprintf(disptext,"Global Z coord shift"); i < dispwidth; i++) disptext[i] = ' ';
                if (editval)
                {
                    j=getnumber16("Z offset:    ",0,16777216,1);
                    if (j!=0)
                    {
                        for (i=0;i<numsectors;i++)
                        {
                            sector[i].ceilingz += j;
                            sector[i].floorz += j;
                        }
                        for (i=0;i<MAXSPRITES;i++)
                            sprite[i].z += j;
                        printmessage16("Map adjusted");
                    }
                    else printmessage16("Aborted");
                }
            }
            break;
            case 5:
            {
                for (i=Bsprintf(disptext,"Up-size selected sectors"); i < dispwidth; i++) disptext[i] = ' ';
                if (editval)
                {
                    j=getnumber16("Size multiplier:    ",1,8,0);
                    if (j!=1)
                    {
                        int w, currsector, start_wall, end_wall;
                        for (i = 0; i < highlightsectorcnt; i++)
                        {
                            currsector = highlightsector[i];
                            sector[currsector].ceilingz *= j;
                            sector[currsector].floorz *= j;
                            // Do all the walls in the sector
                            start_wall = sector[currsector].wallptr;
                            end_wall = start_wall + sector[currsector].wallnum;
                            for (w = start_wall; w < end_wall; w++)
                            {
                                wall[w].x *= j;
                                wall[w].y *= j;
                                wall[w].yrepeat = min(wall[w].yrepeat/j,255);
                            }
                            w = headspritesect[highlightsector[i]];
                            while (w >= 0)
                            {
                                sprite[w].x *= j;
                                sprite[w].y *= j;
                                sprite[w].z *= j;
                                sprite[w].xrepeat = min(max(sprite[w].xrepeat*j,1),255);
                                sprite[w].yrepeat = min(max(sprite[w].yrepeat*j,1),255);
                                w = nextspritesect[w];
                            }
                        }
                        printmessage16("Map scaled");
                    }
                    else printmessage16("Aborted");
                }
            }
            break;
            case 6:
            {
                for (i=Bsprintf(disptext,"Down-size selected sects"); i < dispwidth; i++) disptext[i] = ' ';
                if (editval)
                {
                    j=getnumber16("Size divisor:    ",1,8,0);
                    if (j!=1)
                    {
                        int w, currsector, start_wall, end_wall;
                        for (i = 0; i < highlightsectorcnt; i++)
                        {
                            currsector = highlightsector[i];
                            sector[currsector].ceilingz /= j;
                            sector[currsector].floorz /= j;
                            // Do all the walls in the sector
                            start_wall = sector[currsector].wallptr;
                            end_wall = start_wall + sector[currsector].wallnum;
                            for (w = start_wall; w < end_wall; w++)
                            {
                                wall[w].x /= j;
                                wall[w].y /= j;
                                wall[w].yrepeat = min(wall[w].yrepeat*j,255);
                            }
                            w = headspritesect[highlightsector[i]];
                            while (w >= 0)
                            {
                                sprite[w].x /= j;
                                sprite[w].y /= j;
                                sprite[w].z /= j;
                                sprite[w].xrepeat = min(max(sprite[w].xrepeat/j,1),255);
                                sprite[w].yrepeat = min(max(sprite[w].yrepeat/j,1),255);
                                w = nextspritesect[w];
                            }
                        }
                        printmessage16("Map scaled");
                    }
                    else printmessage16("Aborted");
                }
            }
            break;
            case 7:
            {
                for (i=Bsprintf(disptext,"Global shade divide"); i < dispwidth; i++) disptext[i] = ' ';
                if (editval)
                {
                    j=getnumber16("Shade divisor:    ",1,128,1);
                    if (j!=1)
                    {
                        for (i=0;i<numsectors;i++)
                        {
                            sector[i].ceilingshade /= j;
                            sector[i].floorshade /= j;
                        }
                        for (i=0;i<numwalls;i++)
                            wall[i].shade /= j;
                        for (i=0;i<MAXSPRITES;i++)
                            sprite[i].shade /= j;
                        printmessage16("Shades adjusted");
                    }
                    else printmessage16("Aborted");
                }
            }
            break;
            }
            break;
        case 1:
            switch (row)
            {
            case 0:
            {
                for (i=Bsprintf(disptext,"Global visibility divide"); i < dispwidth; i++) disptext[i] = ' ';
                if (editval)
                {
                    j=getnumber16("Visibility divisor:    ",1,128,0);
                    if (j!=1)
                    {
                        for (i=0;i<numsectors;i++)
                        {
                            if (sector[i].visibility < 240)
                                sector[i].visibility /= j;
                            else sector[i].visibility = 240 + (sector[i].visibility>>4)/j;
                        }
                        printmessage16("Visibility adjusted");
                    }
                    else printmessage16("Aborted");
                }
            }
            break;
            }
            break;
        }
        printext16(xpos,ypos+row*8,11,1,disptext,0);
        enddrawing();
        showframe(1);
    }
    begindrawing();
    printext16(xpos,ypos+row*8,11,0,disptext,0);
    enddrawing();
    clearmidstatbar16();
    showframe(1);
    keystatus[1] = 0;
}
