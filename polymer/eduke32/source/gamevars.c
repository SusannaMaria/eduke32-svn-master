//-------------------------------------------------------------------------
/*
Copyright (C) 1996, 2003 - 3D Realms Entertainment
Copyright (C) 2000, 2003 - Matt Saettler (EDuke Enhancements)
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

#include "duke3d.h"
#include "gamedef.h"
#include "osd.h"

extern int g_i,g_p;
extern int OSD_errors;

void Gv_RefreshPointers(void);
extern void G_FreeMapState(int mapnum);
static void Gv_Free(void) /* called from Gv_ReadSave() and Gv_ResetVars() */
{
    // call this function as many times as needed.
    int i=(MAXGAMEVARS-1);
    //  AddLog("Gv_Free");
    for (;i>=0;i--)
    {
        if (aGameVars[i].plValues)
            Bfree(aGameVars[i].plValues);
        aGameVars[i].plValues=NULL;
        aGameVars[i].bReset=1;
        if (i >= MAXGAMEARRAYS)
            continue;
        if (aGameArrays[i].plValues)
            Bfree(aGameArrays[i].plValues);
        aGameArrays[i].plValues=NULL;
        aGameArrays[i].bReset=1;
    }
    g_gameVarCount=g_gameArrayCount=0;
    HASH_init(&gamevarH);
    HASH_init(&arrayH);
    return;
}

static void Gv_Clear(void)
{
    // only call this function ONCE...
    int i=(MAXGAMEVARS-1);

    //AddLog("Gv_Clear");

    for (;i>=0;i--)
    {
        aGameVars[i].lValue=0;
        if (aGameVars[i].szLabel)
            Bfree(aGameVars[i].szLabel);
        aGameVars[i].szLabel=NULL;
        aGameVars[i].dwFlags=0;

        if (aGameVars[i].plValues)
            Bfree(aGameVars[i].plValues);
        aGameVars[i].plValues=NULL;
        aGameVars[i].bReset=1;
        if (i >= MAXGAMEARRAYS)
            continue;
        if (aGameArrays[i].szLabel)
            Bfree(aGameArrays[i].szLabel);
        aGameArrays[i].szLabel=NULL;

        if (aGameArrays[i].plValues)
            Bfree(aGameArrays[i].plValues);
        aGameArrays[i].plValues=NULL;
        aGameArrays[i].bReset=1;
    }
    g_gameVarCount=g_gameArrayCount=0;
    HASH_init(&gamevarH);
    HASH_init(&arrayH);
    return;
}

int Gv_ReadSave(int fil)
{
    int i, j;
    intptr_t l;
    char savedstate[MAXVOLUMES*MAXLEVELS];

    Bmemset(&savedstate,0,sizeof(savedstate));

    //     AddLog("Reading gamevars from savegame");

    Gv_Free(); // nuke 'em from orbit, it's the only way to be sure...

    //  Bsprintf(g_szBuf,"CP:%s %d",__FILE__,__LINE__);
    //  AddLog(g_szBuf);

    if (kdfread(&g_gameVarCount,sizeof(g_gameVarCount),1,fil) != 1) goto corrupt;
    for (i=0;i<g_gameVarCount;i++)
    {
        if (kdfread(&(aGameVars[i]),sizeof(gamevar_t),1,fil) != 1) goto corrupt;
        aGameVars[i].szLabel=Bcalloc(MAXVARLABEL,sizeof(char));
        if (kdfread(aGameVars[i].szLabel,sizeof(char) * MAXVARLABEL, 1, fil) != 1) goto corrupt;
        HASH_replace(&gamevarH,aGameVars[i].szLabel,i);
    }
    //  Bsprintf(g_szBuf,"CP:%s %d",__FILE__,__LINE__);
    //  AddLog(g_szBuf);
    for (i=0;i<g_gameVarCount;i++)
    {
        if (aGameVars[i].dwFlags & GAMEVAR_PERPLAYER)
            aGameVars[i].plValues=Bcalloc(MAXPLAYERS,sizeof(intptr_t));
        else if (aGameVars[i].dwFlags & GAMEVAR_PERACTOR)
            aGameVars[i].plValues=Bcalloc(MAXSPRITES,sizeof(intptr_t));
        else
            // else nothing 'extra...'
            aGameVars[i].plValues=NULL;
    }

    Gv_InitWeaponPointers();

    //  Bsprintf(g_szBuf,"CP:%s %d",__FILE__,__LINE__);
    //  AddLog(g_szBuf);
    for (i=0;i<g_gameVarCount;i++)
    {
        if (aGameVars[i].dwFlags & GAMEVAR_PERPLAYER)
        {
            //Bsprintf(g_szBuf,"Reading value array for %s (%d)",aGameVars[i].szLabel,sizeof(int) * MAXPLAYERS);
            //AddLog(g_szBuf);
            if (kdfread(aGameVars[i].plValues,sizeof(intptr_t) * MAXPLAYERS, 1, fil) != 1) goto corrupt;
        }
        else if (aGameVars[i].dwFlags & GAMEVAR_PERACTOR)
        {
            //Bsprintf(g_szBuf,"Reading value array for %s (%d)",aGameVars[i].szLabel,sizeof(int) * MAXSPRITES);
            //AddLog(g_szBuf);
            if (kdfread(&aGameVars[i].plValues[0],sizeof(intptr_t), MAXSPRITES, fil) != MAXSPRITES) goto corrupt;
        }
        // else nothing 'extra...'
    }

    //  Bsprintf(g_szBuf,"CP:%s %d",__FILE__,__LINE__);
    //  AddLog(g_szBuf);
    Gv_RefreshPointers();

    if (kdfread(&g_gameArrayCount,sizeof(g_gameArrayCount),1,fil) != 1) goto corrupt;
    for (i=0;i<g_gameArrayCount;i++)
    {
        if (kdfread(&(aGameArrays[i]),sizeof(gamearray_t),1,fil) != 1) goto corrupt;
        aGameArrays[i].szLabel=Bcalloc(MAXARRAYLABEL,sizeof(char));
        if (kdfread(aGameArrays[i].szLabel,sizeof(char) * MAXARRAYLABEL, 1, fil) != 1) goto corrupt;
        HASH_replace(&arrayH,aGameArrays[i].szLabel,i);
    }
    //  Bsprintf(g_szBuf,"CP:%s %d",__FILE__,__LINE__);
    //  AddLog(g_szBuf);
    for (i=0;i<g_gameArrayCount;i++)
    {
        aGameArrays[i].plValues=Bcalloc(aGameArrays[i].size,sizeof(intptr_t));
    }

    //  Bsprintf(g_szBuf,"CP:%s %d",__FILE__,__LINE__);
    //  AddLog(g_szBuf);
    for (i=0;i<g_gameArrayCount;i++)
    {
        //Bsprintf(g_szBuf,"Reading value array for %s (%d)",aGameVars[i].szLabel,sizeof(int) * MAXPLAYERS);
        //AddLog(g_szBuf);
        if (kdfread(aGameArrays[i].plValues,sizeof(intptr_t) * aGameArrays[i].size, 1, fil) < 1) goto corrupt;
    }

    //  Bsprintf(g_szBuf,"CP:%s %d",__FILE__,__LINE__);
    //  AddLog(g_szBuf);
    if (kdfread(apScriptGameEvent,sizeof(apScriptGameEvent),1,fil) != 1) goto corrupt;
    for (i=0;i<MAXGAMEEVENTS;i++)
        if (apScriptGameEvent[i])
        {
            l = (intptr_t)apScriptGameEvent[i]+(intptr_t)&script[0];
            apScriptGameEvent[i] = (intptr_t *)l;
        }

    //  Bsprintf(g_szBuf,"CP:%s %d",__FILE__,__LINE__);
    //  AddLog(g_szBuf);

    if (kdfread(&savedstate[0],sizeof(savedstate),1,fil) != 1) goto corrupt;

    for (i=0;i<(MAXVOLUMES*MAXLEVELS);i++)
    {
        if (savedstate[i])
        {
            if (MapInfo[i].savedstate == NULL)
                MapInfo[i].savedstate = Bcalloc(1,sizeof(mapstate_t));
            if (kdfread(MapInfo[i].savedstate,sizeof(mapstate_t),1,fil) != sizeof(mapstate_t)) goto corrupt;
            for (j=0;j<g_gameVarCount;j++)
            {
                if (aGameVars[j].dwFlags & GAMEVAR_NORESET) continue;
                if (aGameVars[j].dwFlags & GAMEVAR_PERPLAYER)
                {
                    if (!MapInfo[i].savedstate->vars[j])
                        MapInfo[i].savedstate->vars[j] = Bcalloc(MAXPLAYERS,sizeof(intptr_t));
                    if (kdfread(&MapInfo[i].savedstate->vars[j][0],sizeof(intptr_t) * MAXPLAYERS, 1, fil) != 1) goto corrupt;
                }
                else if (aGameVars[j].dwFlags & GAMEVAR_PERACTOR)
                {
                    if (!MapInfo[i].savedstate->vars[j])
                        MapInfo[i].savedstate->vars[j] = Bcalloc(MAXSPRITES,sizeof(intptr_t));
                    if (kdfread(&MapInfo[i].savedstate->vars[j][0],sizeof(intptr_t), MAXSPRITES, fil) != MAXSPRITES) goto corrupt;
                }
            }
        }
        else if (MapInfo[i].savedstate)
        {
            G_FreeMapState(i);
        }
    }

    if (kdfread(&l,sizeof(l),1,fil) != 1) goto corrupt;
    if (kdfread(g_szBuf,l,1,fil) != 1) goto corrupt;
    g_szBuf[l]=0;
    OSD_Printf("%s\n",g_szBuf);

#if 0
    {
        FILE *fp;
        AddLog("Dumping Vars...");
        fp=fopen("xxx.txt","w");
        if (fp)
        {
            Gv_DumpValues(fp);
            fclose(fp);
        }
        AddLog("Done Dumping...");
    }
#endif
    return(0);
corrupt:
    return(1);
}

void Gv_WriteSave(FILE *fil)
{
    int i, j;
    intptr_t l;
    char savedstate[MAXVOLUMES*MAXLEVELS];

    Bmemset(&savedstate,0,sizeof(savedstate));

    //   AddLog("Saving Game Vars to File");
    dfwrite(&g_gameVarCount,sizeof(g_gameVarCount),1,fil);

    for (i=0;i<g_gameVarCount;i++)
    {
        dfwrite(&(aGameVars[i]),sizeof(gamevar_t),1,fil);
        dfwrite(aGameVars[i].szLabel,sizeof(char) * MAXVARLABEL, 1, fil);
    }

    //     dfwrite(&aGameVars,sizeof(aGameVars),1,fil);

    for (i=0;i<g_gameVarCount;i++)
    {
        if (aGameVars[i].dwFlags & GAMEVAR_PERPLAYER)
        {
            //Bsprintf(g_szBuf,"Writing value array for %s (%d)",aGameVars[i].szLabel,sizeof(int) * MAXPLAYERS);
            //AddLog(g_szBuf);
            dfwrite(aGameVars[i].plValues,sizeof(intptr_t) * MAXPLAYERS, 1, fil);
        }
        else if (aGameVars[i].dwFlags & GAMEVAR_PERACTOR)
        {
            //Bsprintf(g_szBuf,"Writing value array for %s (%d)",aGameVars[i].szLabel,sizeof(int) * MAXSPRITES);
            //AddLog(g_szBuf);
            dfwrite(&aGameVars[i].plValues[0],sizeof(intptr_t), MAXSPRITES, fil);
        }
        // else nothing 'extra...'
    }

    dfwrite(&g_gameArrayCount,sizeof(g_gameArrayCount),1,fil);

    for (i=0;i<g_gameArrayCount;i++)
    {
        dfwrite(&(aGameArrays[i]),sizeof(gamearray_t),1,fil);
        dfwrite(aGameArrays[i].szLabel,sizeof(char) * MAXARRAYLABEL, 1, fil);
    }

    //     dfwrite(&aGameVars,sizeof(aGameVars),1,fil);

    for (i=0;i<g_gameArrayCount;i++)
    {
        dfwrite(aGameArrays[i].plValues,sizeof(intptr_t) * aGameArrays[i].size, 1, fil);
    }

    for (i=0;i<MAXGAMEEVENTS;i++)
        if (apScriptGameEvent[i])
        {
            l = (intptr_t)apScriptGameEvent[i]-(intptr_t)&script[0];
            apScriptGameEvent[i] = (intptr_t *)l;
        }
    dfwrite(apScriptGameEvent,sizeof(apScriptGameEvent),1,fil);
    for (i=0;i<MAXGAMEEVENTS;i++)
        if (apScriptGameEvent[i])
        {
            l = (intptr_t)apScriptGameEvent[i]+(intptr_t)&script[0];
            apScriptGameEvent[i] = (intptr_t *)l;
        }

    for (i=0;i<(MAXVOLUMES*MAXLEVELS);i++)
        if (MapInfo[i].savedstate != NULL)
            savedstate[i] = 1;

    dfwrite(&savedstate[0],sizeof(savedstate),1,fil);

    for (i=0;i<(MAXVOLUMES*MAXLEVELS);i++)
        if (MapInfo[i].savedstate)
        {
            dfwrite(MapInfo[i].savedstate,sizeof(mapstate_t),1,fil);
            for (j=0;j<g_gameVarCount;j++)
            {
                if (aGameVars[j].dwFlags & GAMEVAR_NORESET) continue;
                if (aGameVars[j].dwFlags & GAMEVAR_PERPLAYER)
                {
                    dfwrite(&MapInfo[i].savedstate->vars[j][0],sizeof(intptr_t) * MAXPLAYERS, 1, fil);
                }
                else if (aGameVars[j].dwFlags & GAMEVAR_PERACTOR)
                {
                    dfwrite(&MapInfo[i].savedstate->vars[j][0],sizeof(intptr_t) * MAXSPRITES, 1, fil);
                }
            }
        }

    Bsprintf(g_szBuf,"EOF: EDuke32");
    l=strlen(g_szBuf);
    dfwrite(&l,sizeof(l),1,fil);
    dfwrite(g_szBuf,l,1,fil);
}

void Gv_DumpValues(FILE *fp)
{
    int i;
    if (!fp)
    {
        return;
    }
    fprintf(fp,"// Current Game Definitions\n\n");
    for (i=0;i<g_gameVarCount;i++)
    {
        if (aGameVars[i].dwFlags & (GAMEVAR_SECRET))
        {
            continue; // do nothing...
        }
        else
        {
            fprintf(fp,"gamevar %s ",aGameVars[i].szLabel);

            if (aGameVars[i].dwFlags & (GAMEVAR_INTPTR))
                fprintf(fp,"%d",*((int*)aGameVars[i].lValue));
            else if (aGameVars[i].dwFlags & (GAMEVAR_SHORTPTR))
                fprintf(fp,"%d",*((short*)aGameVars[i].lValue));
            else if (aGameVars[i].dwFlags & (GAMEVAR_CHARPTR))
                fprintf(fp,"%d",*((char*)aGameVars[i].lValue));
            else
                fprintf(fp,"%" PRIdPTR "",aGameVars[i].lValue);
            if (aGameVars[i].dwFlags & (GAMEVAR_PERPLAYER))
                fprintf(fp," GAMEVAR_PERPLAYER");
            else if (aGameVars[i].dwFlags & (GAMEVAR_PERACTOR))
                fprintf(fp," GAMEVAR_PERACTOR");
            else
                fprintf(fp," %d",aGameVars[i].dwFlags & (GAMEVAR_USER_MASK));
            fprintf(fp," // ");
            if (aGameVars[i].dwFlags & (GAMEVAR_SYSTEM))
                fprintf(fp," (system)");
            if (aGameVars[i].dwFlags & (GAMEVAR_INTPTR|GAMEVAR_SHORTPTR|GAMEVAR_CHARPTR))
                fprintf(fp," (pointer)");
            if (aGameVars[i].dwFlags & (GAMEVAR_READONLY))
                fprintf(fp," (read only)");
            fprintf(fp,"\n");
        }
    }
    fprintf(fp,"\n// end of game definitions\n");
}

void Gv_ResetVars(void) /* this is called during a new game and nowhere else */
{
    int i;

    //AddLog("Reset Game Vars");
    Gv_Free();
    OSD_errors=0;

    for (i=0;i<MAXGAMEVARS;i++)
    {
        //Bsprintf(g_szBuf,"Resetting %d: '%s' to %d",i,aDefaultGameVars[i].szLabel,
        //  	aDefaultGameVars[i].lValue
        //     );
        //AddLog(g_szBuf);
        if (aGameVars[i].szLabel != NULL && aGameVars[i].bReset)
            Gv_NewVar(aGameVars[i].szLabel,aGameVars[i].lDefault,aGameVars[i].dwFlags);
    }

    for (i=0;i<MAXGAMEARRAYS;i++)
    {
        //Bsprintf(g_szBuf,"Resetting %d: '%s' to %d",i,aDefaultGameVars[i].szLabel,
        //  	aDefaultGameVars[i].lValue
        //     );
        //AddLog(g_szBuf);
        if (aGameArrays[i].szLabel != NULL && aGameArrays[i].bReset)
            Gv_NewArray(aGameArrays[i].szLabel,aGameArrays[i].size);
    }
}

int Gv_NewArray(const char *pszLabel, int asize)
{
    int i;

    if (g_gameArrayCount >= MAXGAMEARRAYS)
    {
        g_numCompilerErrors++;
        C_ReportError(-1);
        initprintf("%s:%d: error: too many arrays!\n",g_szScriptFileName,g_lineNumber);
        return 0;
    }

    if (Bstrlen(pszLabel) > (MAXARRAYLABEL-1))
    {
        g_numCompilerErrors++;
        C_ReportError(-1);
        initprintf("%s:%d: error: array name `%s' exceeds limit of %d characters.\n",g_szScriptFileName,g_lineNumber,pszLabel, MAXARRAYLABEL);
        return 0;
    }
    i = HASH_find(&arrayH,pszLabel);
    if (i >=0 && !aGameArrays[i].bReset)
    {
        // found it it's a duplicate in error
        g_numCompilerWarnings++;
        C_ReportError(WARNING_DUPLICATEDEFINITION);
        return 0;
    }

    i = g_gameArrayCount;

    if (aGameArrays[i].szLabel == NULL)
        aGameArrays[i].szLabel=Bcalloc(MAXVARLABEL,sizeof(char));
    if (aGameArrays[i].szLabel != pszLabel)
        Bstrcpy(aGameArrays[i].szLabel,pszLabel);
    aGameArrays[i].plValues=Bcalloc(asize,sizeof(intptr_t));
    aGameArrays[i].size=asize;
    aGameVars[i].bReset=0;
    g_gameArrayCount++;
    HASH_replace(&arrayH,aGameArrays[i].szLabel,i);
    return 1;
}

int Gv_NewVar(const char *pszLabel, int lValue, unsigned int dwFlags)
{
    int i, j;

    //Bsprintf(g_szBuf,"Gv_NewVar(%s, %d, %X)",pszLabel, lValue, dwFlags);
    //AddLog(g_szBuf);

    if (g_gameVarCount >= MAXGAMEVARS)
    {
        g_numCompilerErrors++;
        C_ReportError(-1);
        initprintf("%s:%d: error: too many gamevars!\n",g_szScriptFileName,g_lineNumber);
        return 0;
    }

    if (Bstrlen(pszLabel) > (MAXVARLABEL-1))
    {
        g_numCompilerErrors++;
        C_ReportError(-1);
        initprintf("%s:%d: error: variable name `%s' exceeds limit of %d characters.\n",g_szScriptFileName,g_lineNumber,pszLabel, MAXVARLABEL);
        return 0;
    }

    i = HASH_find(&gamevarH,pszLabel);

    if (i >= 0 && !aGameVars[i].bReset)
    {
        // found it...
        if (aGameVars[i].dwFlags & (GAMEVAR_INTPTR|GAMEVAR_SHORTPTR|GAMEVAR_CHARPTR))
        {
            //  			   warning++;
            //  			   initprintf("%s:%d: warning: Internal gamevar '%s' cannot be redefined.\n",g_szScriptFileName,g_lineNumber,label+(g_numLabels<<6));
            C_ReportError(-1);
            initprintf("%s:%d: warning: cannot redefine internal gamevar `%s'.\n",g_szScriptFileName,g_lineNumber,label+(g_numLabels<<6));
            return 0;
        }
        else if (!(aGameVars[i].dwFlags & GAMEVAR_DEFAULT) && !(aGameVars[i].dwFlags & GAMEVAR_SYSTEM))
        {
            // it's a duplicate in error
            g_numCompilerWarnings++;
            C_ReportError(WARNING_DUPLICATEDEFINITION);
            return 0;
        }
    }

    if (i == -1)
        i = g_gameVarCount;

    // Set values
    if ((aGameVars[i].dwFlags & GAMEVAR_SYSTEM) == 0)
    {
        if (aGameVars[i].szLabel == NULL)
            aGameVars[i].szLabel=Bcalloc(MAXVARLABEL,sizeof(char));
        if (aGameVars[i].szLabel != pszLabel)
            Bstrcpy(aGameVars[i].szLabel,pszLabel);
        aGameVars[i].dwFlags=dwFlags;

        if (aGameVars[i].plValues)
        {
            // only free if not system
            Bfree(aGameVars[i].plValues);
            aGameVars[i].plValues=NULL;
        }
    }

    // if existing is system, they only get to change default value....
    aGameVars[i].lValue = aGameVars[i].lDefault = lValue;
    aGameVars[i].bReset = 0;

    if (i == g_gameVarCount)
    {
        // we're adding a new one.
        HASH_add(&gamevarH, aGameVars[i].szLabel, g_gameVarCount++);
    }

    if (aGameVars[i].dwFlags & GAMEVAR_PERPLAYER)
    {
        if (!aGameVars[i].plValues)
            aGameVars[i].plValues=Bcalloc(MAXPLAYERS,sizeof(intptr_t));
        for (j=MAXPLAYERS-1;j>=0;j--)
            aGameVars[i].plValues[j]=lValue;
    }
    else if (aGameVars[i].dwFlags & GAMEVAR_PERACTOR)
    {
        if (!aGameVars[i].plValues)
            aGameVars[i].plValues=Bcalloc(MAXSPRITES,sizeof(intptr_t));
        for (j=MAXSPRITES-1;j>=0;j--)
            aGameVars[i].plValues[j]=lValue;
    }
    return 1;
}

inline void A_ResetVars(int iActor)
{
    int i=(MAXGAMEVARS-1);
    //    OSD_Printf("resetting vars for actor %d\n",iActor);
    for (;i>=0;i--)
        if ((aGameVars[i].dwFlags & GAMEVAR_PERACTOR) && !(aGameVars[i].dwFlags & GAMEVAR_NODEFAULT))
        {
            //  		  OSD_Printf("reset %s (%d) to %s (%d)\n",aGameVars[i].szLabel,aGameVars[i].plValues[iActor],aDefaultGameVars[i].szLabel,aDefaultGameVars[i].lValue);
            aGameVars[i].plValues[iActor]=aGameVars[i].lDefault;
        }
}

static inline int Gv_GetVarIndex(const char *szGameLabel)
{
    int i = HASH_find(&gamevarH,szGameLabel);
    if (i == -1)
    {
        OSD_Printf(OSD_ERROR "Gv_GetVarDataPtr(): INTERNAL ERROR: couldn't find gamevar %s!\n",szGameLabel);
        return 0;
    }
    return i;
}

int __fastcall Gv_GetVar(int id, int iActor, int iPlayer)
{
    if (id == MAXGAMEVARS)
        return(*insptr++);

    if (id == g_iThisActorID)
        return iActor;

    {
        int neg = 0;

        if (id >= g_gameVarCount || id < 0)
        {
            if (id&(MAXGAMEVARS<<2)) // array
            {
                int index=Gv_GetVar(*insptr++,iActor,iPlayer);

                if (id&(MAXGAMEVARS<<1)) // negative array access
                    neg = 1;

                id &= ~((MAXGAMEVARS<<2)|(MAXGAMEVARS<<1));

                if (index >= aGameArrays[id].size || index < 0)
                {
                    OSD_Printf(CON_ERROR "Gv_GetVar(): invalid array index (%s[%d])\n",g_errorLineNum,keyw[g_tw],aGameArrays[id].szLabel,index);
                    return -1;
                }
                return(neg?-aGameArrays[id].plValues[index]:aGameArrays[id].plValues[index]);
            }

            if ((id&(MAXGAMEVARS<<1)) == 0)
            {
                OSD_Printf(CON_ERROR "Gv_GetVar(): invalid gamevar ID (%d)\n",g_errorLineNum,keyw[g_tw],id);
                return -1;
            }

            neg = 1;
            id &= ~(MAXGAMEVARS<<1);
        }

        if (aGameVars[id].dwFlags & GAMEVAR_PERPLAYER)
        {
            // for the current player
            if (iPlayer < 0 || iPlayer >= MAXPLAYERS)
            {
                OSD_Printf(CON_ERROR "Gv_GetVar(): invalid player ID (%d)\n",g_errorLineNum,keyw[g_tw],iPlayer);
                return -1;
            }
            return(neg?-aGameVars[id].plValues[iPlayer]:aGameVars[id].plValues[iPlayer]);
        }

        if (aGameVars[id].dwFlags & GAMEVAR_PERACTOR)
        {
            // for the current actor
            if (iActor < 0 || iActor >= MAXSPRITES)
            {
                OSD_Printf(CON_ERROR "Gv_GetVar(): invalid sprite ID (%d)\n",g_errorLineNum,keyw[g_tw],iActor);
                return -1;
            }
            return(neg?-aGameVars[id].plValues[iActor]:aGameVars[id].plValues[iActor]);
        }

        if (!(aGameVars[id].dwFlags & (GAMEVAR_INTPTR|GAMEVAR_SHORTPTR|GAMEVAR_CHARPTR)))
            return(neg?-aGameVars[id].lValue:aGameVars[id].lValue);

        if (aGameVars[id].dwFlags & GAMEVAR_INTPTR)
            return(neg?-(*((int*)aGameVars[id].lValue)):(*((int*)aGameVars[id].lValue)));

        if (aGameVars[id].dwFlags & GAMEVAR_SHORTPTR)
            return(neg?-(*((short*)aGameVars[id].lValue)):(*((short*)aGameVars[id].lValue)));

        if (aGameVars[id].dwFlags & GAMEVAR_CHARPTR)
            return(neg?-(*((char*)aGameVars[id].lValue)):(*((char*)aGameVars[id].lValue)));

        // this should be impossible
        OSD_Printf(CON_ERROR "Gv_GetVar(): unknown variable type (%d)\n",g_errorLineNum,keyw[g_tw],aGameVars[id].dwFlags);
        return -1;
    }
}

void __fastcall Gv_SetVar(int id, int lValue, int iActor, int iPlayer)
{
    if (id<0 || id >= g_gameVarCount)
    {
        OSD_Printf(CON_ERROR "Gv_SetVar(): tried to set invalid gamevar ID (%d) from sprite %d (%d), player %d\n",g_errorLineNum,keyw[g_tw],id,g_i,sprite[g_i].picnum,g_p);
        return;
    }
    //Bsprintf(g_szBuf,"SGVI: %d ('%s') to %d for %d %d",id,aGameVars[id].szLabel,lValue,iActor,iPlayer);
    //AddLog(g_szBuf);
    if (aGameVars[id].dwFlags & GAMEVAR_PERPLAYER)
    {
        if (iPlayer < 0 || iPlayer > MAXPLAYERS-1)
        {
            OSD_Printf(CON_ERROR "Gv_SetVar(): invalid player (%d) for per-player gamevar %s from sprite %d, player %d\n",g_errorLineNum,keyw[g_tw],iPlayer,aGameVars[id].szLabel,g_i,g_p);
            return;
        }
        // for the current player
        aGameVars[id].plValues[iPlayer]=lValue;
        return;
    }

    if (aGameVars[id].dwFlags & GAMEVAR_PERACTOR)
    {
        if (iActor < 0 || iActor > MAXSPRITES-1)
        {
            OSD_Printf(CON_ERROR "Gv_SetVar(): invalid sprite (%d) for per-actor gamevar %s from sprite %d (%d), player %d\n",g_errorLineNum,keyw[g_tw],iActor,aGameVars[id].szLabel,g_i,sprite[g_i].picnum,g_p);
            return;
        }
        // for the current actor
        aGameVars[id].plValues[iActor]=lValue;
        return;
    }

    if (!(aGameVars[id].dwFlags & (GAMEVAR_INTPTR|GAMEVAR_SHORTPTR|GAMEVAR_CHARPTR)))
    {
        aGameVars[id].lValue=lValue;
        return;
    }

    if (aGameVars[id].dwFlags & GAMEVAR_INTPTR)
    {
        // set the value at pointer
        *((int*)aGameVars[id].lValue)=(int)lValue;
        return;
    }

    if (aGameVars[id].dwFlags & GAMEVAR_SHORTPTR)
    {
        // set the value at pointer
        *((short*)aGameVars[id].lValue)=(short)lValue;
        return;
    }

    if (aGameVars[id].dwFlags & GAMEVAR_CHARPTR)
    {
        // set the value at pointer
        *((char*)aGameVars[id].lValue)=(char)lValue;
        return;
    }
}

int Gv_GetVarByLabel(const char *szGameLabel, int lDefault, int iActor, int iPlayer)
{
    int i = HASH_find(&gamevarH,szGameLabel);

    if (i < 0)
        return lDefault;

    return Gv_GetVar(i, iActor, iPlayer);
}

static intptr_t *Gv_GetVarDataPtr(const char *szGameLabel)
{
    int i = HASH_find(&gamevarH,szGameLabel);

    if (i < 0)
        return NULL;

    if (aGameVars[i].dwFlags & (GAMEVAR_PERACTOR | GAMEVAR_PERPLAYER))
    {
        if (!aGameVars[i].plValues)
            OSD_Printf(CON_ERROR "Gv_GetVarDataPtr(): INTERNAL ERROR: NULL array !!!\n",g_errorLineNum,keyw[g_tw]);
        return aGameVars[i].plValues;
    }

    return &(aGameVars[i].lValue);
}

void Gv_ResetSystemDefaults(void)
{
    // call many times...

    int i,j;
    char aszBuf[64];

    //AddLog("ResetWeaponDefaults");

    for (j=MAXPLAYERS-1;j>=0;j--)
    {
        for (i=MAX_WEAPONS-1;i>=0;i--)
        {
            Bsprintf(aszBuf,"WEAPON%d_CLIP",i);
            aplWeaponClip[i][j]=Gv_GetVarByLabel(aszBuf,0, -1, j);
            Bsprintf(aszBuf,"WEAPON%d_RELOAD",i);
            aplWeaponReload[i][j]=Gv_GetVarByLabel(aszBuf,0, -1, j);
            Bsprintf(aszBuf,"WEAPON%d_FIREDELAY",i);
            aplWeaponFireDelay[i][j]=Gv_GetVarByLabel(aszBuf,0, -1, j);
            Bsprintf(aszBuf,"WEAPON%d_TOTALTIME",i);
            aplWeaponTotalTime[i][j]=Gv_GetVarByLabel(aszBuf,0, -1, j);
            Bsprintf(aszBuf,"WEAPON%d_HOLDDELAY",i);
            aplWeaponHoldDelay[i][j]=Gv_GetVarByLabel(aszBuf,0, -1, j);
            Bsprintf(aszBuf,"WEAPON%d_FLAGS",i);
            aplWeaponFlags[i][j]=Gv_GetVarByLabel(aszBuf,0, -1, j);
            Bsprintf(aszBuf,"WEAPON%d_SHOOTS",i);
            aplWeaponShoots[i][j]=Gv_GetVarByLabel(aszBuf,0, -1, j);
            Bsprintf(aszBuf,"WEAPON%d_SPAWNTIME",i);
            aplWeaponSpawnTime[i][j]=Gv_GetVarByLabel(aszBuf,0, -1, j);
            Bsprintf(aszBuf,"WEAPON%d_SPAWN",i);
            aplWeaponSpawn[i][j]=Gv_GetVarByLabel(aszBuf,0, -1, j);
            Bsprintf(aszBuf,"WEAPON%d_SHOTSPERBURST",i);
            aplWeaponShotsPerBurst[i][j]=Gv_GetVarByLabel(aszBuf,0, -1, j);
            Bsprintf(aszBuf,"WEAPON%d_WORKSLIKE",i);
            aplWeaponWorksLike[i][j]=Gv_GetVarByLabel(aszBuf,0, -1, j);
            Bsprintf(aszBuf,"WEAPON%d_INITIALSOUND",i);
            aplWeaponInitialSound[i][j]=Gv_GetVarByLabel(aszBuf,0, -1, j);
            Bsprintf(aszBuf,"WEAPON%d_FIRESOUND",i);
            aplWeaponFireSound[i][j]=Gv_GetVarByLabel(aszBuf,0, -1, j);
            Bsprintf(aszBuf,"WEAPON%d_SOUND2TIME",i);
            aplWeaponSound2Time[i][j]=Gv_GetVarByLabel(aszBuf,0, -1, j);
            Bsprintf(aszBuf,"WEAPON%d_SOUND2SOUND",i);
            aplWeaponSound2Sound[i][j]=Gv_GetVarByLabel(aszBuf,0, -1, j);
            Bsprintf(aszBuf,"WEAPON%d_RELOADSOUND1",i);
            aplWeaponReloadSound1[i][j]=Gv_GetVarByLabel(aszBuf,0, -1, j);
            Bsprintf(aszBuf,"WEAPON%d_RELOADSOUND2",i);
            aplWeaponReloadSound2[i][j]=Gv_GetVarByLabel(aszBuf,0, -1, j);
            Bsprintf(aszBuf,"WEAPON%d_SELECTSOUND",i);
            aplWeaponSelectSound[i][j]=Gv_GetVarByLabel(aszBuf,0, -1, j);
        }
    }

    g_iReturnVarID=Gv_GetVarIndex("RETURN");
    g_iWeaponVarID=Gv_GetVarIndex("WEAPON");
    g_iWorksLikeVarID=Gv_GetVarIndex("WORKSLIKE");
    g_iZRangeVarID=Gv_GetVarIndex("ZRANGE");
    g_iAngRangeVarID=Gv_GetVarIndex("ANGRANGE");
    g_iAimAngleVarID=Gv_GetVarIndex("AUTOAIMANGLE");
    g_iLoTagID=Gv_GetVarIndex("LOTAG");
    g_iHiTagID=Gv_GetVarIndex("HITAG");
    g_iTextureID=Gv_GetVarIndex("TEXTURE");
    g_iThisActorID=Gv_GetVarIndex("THISACTOR");

    Bmemcpy(&ProjectileData,&DefaultProjectileData,sizeof(ProjectileData));

    //AddLog("EOF:ResetWeaponDefaults");
}

static void Gv_AddSystemVars(void)
{
    // only call ONCE
    char aszBuf[64];

    //AddLog("Gv_AddSystemVars");

    Bsprintf(aszBuf,"WEAPON%d_WORKSLIKE",KNEE_WEAPON);
    Gv_NewVar(aszBuf, KNEE_WEAPON, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_CLIP",KNEE_WEAPON);
    Gv_NewVar(aszBuf, 0, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_RELOAD",KNEE_WEAPON);
    Gv_NewVar(aszBuf, 0, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_FIREDELAY",KNEE_WEAPON);
    Gv_NewVar(aszBuf, 7, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_TOTALTIME",KNEE_WEAPON);
    Gv_NewVar(aszBuf, 14, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_HOLDDELAY",KNEE_WEAPON);
    Gv_NewVar(aszBuf, 0, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_FLAGS",KNEE_WEAPON);
    Gv_NewVar(aszBuf, WEAPON_NOVISIBLE | WEAPON_RANDOMRESTART | WEAPON_AUTOMATIC, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_SHOOTS",KNEE_WEAPON);
    Gv_NewVar(aszBuf, KNEE, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_SPAWNTIME",KNEE_WEAPON);
    Gv_NewVar(aszBuf, 0, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_SPAWN",KNEE_WEAPON);
    Gv_NewVar(aszBuf, 0, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_SHOTSPERBURST",KNEE_WEAPON);
    Gv_NewVar(aszBuf, 0, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_INITIALSOUND",KNEE_WEAPON);
    Gv_NewVar(aszBuf, 0, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_FIRESOUND",KNEE_WEAPON);
    Gv_NewVar(aszBuf, 0, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_SOUND2TIME",KNEE_WEAPON);
    Gv_NewVar(aszBuf, 0, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_SOUND2SOUND",KNEE_WEAPON);
    Gv_NewVar(aszBuf, 0, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_RELOADSOUND1",KNEE_WEAPON);
    Gv_NewVar(aszBuf, EJECT_CLIP, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_RELOADSOUND2",KNEE_WEAPON);
    Gv_NewVar(aszBuf, INSERT_CLIP, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_SELECTSOUND",KNEE_WEAPON);
    Gv_NewVar(aszBuf, 0, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);

    /////////////////////////////
    Bsprintf(aszBuf,"WEAPON%d_WORKSLIKE",PISTOL_WEAPON);
    Gv_NewVar(aszBuf, PISTOL_WEAPON, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_CLIP",PISTOL_WEAPON);
    Gv_NewVar(aszBuf, NAM?20:12, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_RELOAD",PISTOL_WEAPON);
    Gv_NewVar(aszBuf, NAM?50:27, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_FIREDELAY",PISTOL_WEAPON);
    Gv_NewVar(aszBuf, 2, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_TOTALTIME",PISTOL_WEAPON);
    Gv_NewVar(aszBuf, 5, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_HOLDDELAY",PISTOL_WEAPON);
    Gv_NewVar(aszBuf, 0, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_FLAGS",PISTOL_WEAPON);
    Gv_NewVar(aszBuf, NAM?WEAPON_HOLSTER_CLEARS_CLIP:0 | WEAPON_RELOAD_TIMING, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_SHOOTS",PISTOL_WEAPON);
    Gv_NewVar(aszBuf, SHOTSPARK1, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_SPAWNTIME",PISTOL_WEAPON);
    Gv_NewVar(aszBuf, 2, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_SPAWN",PISTOL_WEAPON);
    Gv_NewVar(aszBuf, SHELL, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_SHOTSPERBURST",PISTOL_WEAPON);
    Gv_NewVar(aszBuf, 0, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_INITIALSOUND",PISTOL_WEAPON);
    Gv_NewVar(aszBuf, 0, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_FIRESOUND",PISTOL_WEAPON);
    Gv_NewVar(aszBuf, PISTOL_FIRE, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_SOUND2TIME",PISTOL_WEAPON);
    Gv_NewVar(aszBuf, 0, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_SOUND2SOUND",PISTOL_WEAPON);
    Gv_NewVar(aszBuf, 0, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_RELOADSOUND1",PISTOL_WEAPON);
    Gv_NewVar(aszBuf, EJECT_CLIP, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_RELOADSOUND2",PISTOL_WEAPON);
    Gv_NewVar(aszBuf, INSERT_CLIP, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_SELECTSOUND",PISTOL_WEAPON);
    Gv_NewVar(aszBuf, INSERT_CLIP, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);

    /////////////////////////////
    Bsprintf(aszBuf,"WEAPON%d_WORKSLIKE",SHOTGUN_WEAPON);
    Gv_NewVar(aszBuf, SHOTGUN_WEAPON, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_CLIP",SHOTGUN_WEAPON);
    Gv_NewVar(aszBuf, 0, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_RELOAD",SHOTGUN_WEAPON);
    Gv_NewVar(aszBuf, 13, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_FIREDELAY",SHOTGUN_WEAPON);
    Gv_NewVar(aszBuf, 4, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_TOTALTIME",SHOTGUN_WEAPON);
    Gv_NewVar(aszBuf, 30, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_HOLDDELAY",SHOTGUN_WEAPON);
    Gv_NewVar(aszBuf, 0, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_FLAGS",SHOTGUN_WEAPON);
    Gv_NewVar(aszBuf, WEAPON_CHECKATRELOAD, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_SHOOTS",SHOTGUN_WEAPON);
    Gv_NewVar(aszBuf, SHOTGUN, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_SPAWNTIME",SHOTGUN_WEAPON);
    Gv_NewVar(aszBuf, 24, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_SPAWN",SHOTGUN_WEAPON);
    Gv_NewVar(aszBuf, SHOTGUNSHELL, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_SHOTSPERBURST",SHOTGUN_WEAPON);
    Gv_NewVar(aszBuf, 7, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_INITIALSOUND",SHOTGUN_WEAPON);
    Gv_NewVar(aszBuf, 0, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_FIRESOUND",SHOTGUN_WEAPON);
    Gv_NewVar(aszBuf, SHOTGUN_FIRE, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_SOUND2TIME",SHOTGUN_WEAPON);
    Gv_NewVar(aszBuf, 15, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_SOUND2SOUND",SHOTGUN_WEAPON);
    Gv_NewVar(aszBuf, SHOTGUN_COCK, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_RELOADSOUND1",SHOTGUN_WEAPON);
    Gv_NewVar(aszBuf, EJECT_CLIP, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_RELOADSOUND2",SHOTGUN_WEAPON);
    Gv_NewVar(aszBuf, INSERT_CLIP, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_SELECTSOUND",SHOTGUN_WEAPON);
    Gv_NewVar(aszBuf, SHOTGUN_COCK, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);

    /////////////////////////////
    Bsprintf(aszBuf,"WEAPON%d_WORKSLIKE",CHAINGUN_WEAPON);
    Gv_NewVar(aszBuf, CHAINGUN_WEAPON, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_CLIP",CHAINGUN_WEAPON);
    Gv_NewVar(aszBuf, 0, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_RELOAD",CHAINGUN_WEAPON);
    Gv_NewVar(aszBuf, 0, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_FIREDELAY",CHAINGUN_WEAPON);
    Gv_NewVar(aszBuf, 3, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_TOTALTIME",CHAINGUN_WEAPON);
    Gv_NewVar(aszBuf, 12, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_HOLDDELAY",CHAINGUN_WEAPON);
    Gv_NewVar(aszBuf, 3, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_FLAGS",CHAINGUN_WEAPON);
    Gv_NewVar(aszBuf, WEAPON_AUTOMATIC | WEAPON_FIREEVERYTHIRD | WEAPON_AMMOPERSHOT | WEAPON_SPAWNTYPE3 | WEAPON_RESET, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_SHOOTS",CHAINGUN_WEAPON);
    Gv_NewVar(aszBuf, CHAINGUN, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_SPAWNTIME",CHAINGUN_WEAPON);
    Gv_NewVar(aszBuf, 1, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_SPAWN",CHAINGUN_WEAPON);
    Gv_NewVar(aszBuf, SHELL, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_SHOTSPERBURST",CHAINGUN_WEAPON);
    Gv_NewVar(aszBuf, 0, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_INITIALSOUND",CHAINGUN_WEAPON);
    Gv_NewVar(aszBuf, 0, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_FIRESOUND",CHAINGUN_WEAPON);
    Gv_NewVar(aszBuf, CHAINGUN_FIRE, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_SOUND2TIME",CHAINGUN_WEAPON);
    Gv_NewVar(aszBuf, 0, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_SOUND2SOUND",CHAINGUN_WEAPON);
    Gv_NewVar(aszBuf, 0, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_RELOADSOUND1",CHAINGUN_WEAPON);
    Gv_NewVar(aszBuf, EJECT_CLIP, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_RELOADSOUND2",CHAINGUN_WEAPON);
    Gv_NewVar(aszBuf, INSERT_CLIP, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_SELECTSOUND",CHAINGUN_WEAPON);
    Gv_NewVar(aszBuf, SELECT_WEAPON, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);

    /////////////////////////////
    Bsprintf(aszBuf,"WEAPON%d_WORKSLIKE",RPG_WEAPON);
    Gv_NewVar(aszBuf, RPG_WEAPON, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_CLIP",RPG_WEAPON);
    Gv_NewVar(aszBuf, 0, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_RELOAD",RPG_WEAPON);
    Gv_NewVar(aszBuf, 0, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_FIREDELAY",RPG_WEAPON);
    Gv_NewVar(aszBuf, 4, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_TOTALTIME",RPG_WEAPON);
    Gv_NewVar(aszBuf, 20, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_HOLDDELAY",RPG_WEAPON);
    Gv_NewVar(aszBuf, 0, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_FLAGS",RPG_WEAPON);
    Gv_NewVar(aszBuf, 0, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_SHOOTS",RPG_WEAPON);
    Gv_NewVar(aszBuf, RPG, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_SPAWNTIME",RPG_WEAPON);
    Gv_NewVar(aszBuf, 0, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_SPAWN",RPG_WEAPON);
    Gv_NewVar(aszBuf, 0, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_SHOTSPERBURST",RPG_WEAPON);
    Gv_NewVar(aszBuf, 0, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_INITIALSOUND",RPG_WEAPON);
    Gv_NewVar(aszBuf, 0, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_FIRESOUND",RPG_WEAPON);
    Gv_NewVar(aszBuf, 0, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_SOUND2TIME",RPG_WEAPON);
    Gv_NewVar(aszBuf, 0, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_SOUND2SOUND",RPG_WEAPON);
    Gv_NewVar(aszBuf, 0, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_RELOADSOUND1",RPG_WEAPON);
    Gv_NewVar(aszBuf, EJECT_CLIP, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_RELOADSOUND2",RPG_WEAPON);
    Gv_NewVar(aszBuf, INSERT_CLIP, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_SELECTSOUND",RPG_WEAPON);
    Gv_NewVar(aszBuf, SELECT_WEAPON, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);

    /////////////////////////////
    Bsprintf(aszBuf,"WEAPON%d_WORKSLIKE",HANDBOMB_WEAPON);
    Gv_NewVar(aszBuf, HANDBOMB_WEAPON, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_CLIP",HANDBOMB_WEAPON);
    Gv_NewVar(aszBuf, 0, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_RELOAD",HANDBOMB_WEAPON);
    Gv_NewVar(aszBuf, 30, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_FIREDELAY",HANDBOMB_WEAPON);
    Gv_NewVar(aszBuf, 6, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_TOTALTIME",HANDBOMB_WEAPON);
    Gv_NewVar(aszBuf, 19, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_HOLDDELAY",HANDBOMB_WEAPON);
    Gv_NewVar(aszBuf, 12, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_FLAGS",HANDBOMB_WEAPON);
    Gv_NewVar(aszBuf, WEAPON_THROWIT, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_SHOOTS",HANDBOMB_WEAPON);
    Gv_NewVar(aszBuf, HEAVYHBOMB, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_SPAWNTIME",HANDBOMB_WEAPON);
    Gv_NewVar(aszBuf, 0, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_SPAWN",HANDBOMB_WEAPON);
    Gv_NewVar(aszBuf, 0, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_SHOTSPERBURST",HANDBOMB_WEAPON);
    Gv_NewVar(aszBuf, 0, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_INITIALSOUND",HANDBOMB_WEAPON);
    Gv_NewVar(aszBuf, 0, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_FIRESOUND",HANDBOMB_WEAPON);
    Gv_NewVar(aszBuf, 0, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_SOUND2TIME",HANDBOMB_WEAPON);
    Gv_NewVar(aszBuf, 0, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_SOUND2SOUND",HANDBOMB_WEAPON);
    Gv_NewVar(aszBuf, 0, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_RELOADSOUND1",HANDBOMB_WEAPON);
    Gv_NewVar(aszBuf, EJECT_CLIP, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_RELOADSOUND2",HANDBOMB_WEAPON);
    Gv_NewVar(aszBuf, INSERT_CLIP, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_SELECTSOUND",HANDBOMB_WEAPON);
    Gv_NewVar(aszBuf, 0, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);

    /////////////////////////////
    Bsprintf(aszBuf,"WEAPON%d_WORKSLIKE",SHRINKER_WEAPON);
    Gv_NewVar(aszBuf, SHRINKER_WEAPON, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_CLIP",SHRINKER_WEAPON);
    Gv_NewVar(aszBuf, 0, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_RELOAD",SHRINKER_WEAPON);
    Gv_NewVar(aszBuf, 0, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_FIREDELAY",SHRINKER_WEAPON);
    Gv_NewVar(aszBuf, 10, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_TOTALTIME",SHRINKER_WEAPON);
    Gv_NewVar(aszBuf, NAM?30:12, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_HOLDDELAY",SHRINKER_WEAPON);
    Gv_NewVar(aszBuf, 0, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_FLAGS",SHRINKER_WEAPON);
    Gv_NewVar(aszBuf, WEAPON_GLOWS, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_SHOOTS",SHRINKER_WEAPON);
    Gv_NewVar(aszBuf, SHRINKER, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_SPAWNTIME",SHRINKER_WEAPON);
    Gv_NewVar(aszBuf, 0, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_SPAWN",SHRINKER_WEAPON);
    Gv_NewVar(aszBuf, 0, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_SHOTSPERBURST",SHRINKER_WEAPON);
    Gv_NewVar(aszBuf, 0, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_INITIALSOUND",SHRINKER_WEAPON);
    Gv_NewVar(aszBuf, SHRINKER_FIRE, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_FIRESOUND",SHRINKER_WEAPON);
    Gv_NewVar(aszBuf, 0, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_SOUND2TIME",SHRINKER_WEAPON);
    Gv_NewVar(aszBuf, 0, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_SOUND2SOUND",SHRINKER_WEAPON);
    Gv_NewVar(aszBuf, 0, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_RELOADSOUND1",SHRINKER_WEAPON);
    Gv_NewVar(aszBuf, EJECT_CLIP, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_RELOADSOUND2",SHRINKER_WEAPON);
    Gv_NewVar(aszBuf, INSERT_CLIP, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_SELECTSOUND",SHRINKER_WEAPON);
    Gv_NewVar(aszBuf, SELECT_WEAPON, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);

    /////////////////////////////
    Bsprintf(aszBuf,"WEAPON%d_WORKSLIKE",DEVISTATOR_WEAPON);
    Gv_NewVar(aszBuf, DEVISTATOR_WEAPON, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_CLIP",DEVISTATOR_WEAPON);
    Gv_NewVar(aszBuf, 0, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_RELOAD",DEVISTATOR_WEAPON);
    Gv_NewVar(aszBuf, 0, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_FIREDELAY",DEVISTATOR_WEAPON);
    Gv_NewVar(aszBuf, 3, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_TOTALTIME",DEVISTATOR_WEAPON);
    Gv_NewVar(aszBuf, 6, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_HOLDDELAY",DEVISTATOR_WEAPON);
    Gv_NewVar(aszBuf, 5, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_FLAGS",DEVISTATOR_WEAPON);
    Gv_NewVar(aszBuf, WEAPON_FIREEVERYOTHER | WEAPON_AMMOPERSHOT, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_SHOOTS",DEVISTATOR_WEAPON);
    Gv_NewVar(aszBuf, RPG, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_SPAWNTIME",DEVISTATOR_WEAPON);
    Gv_NewVar(aszBuf, 0, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_SPAWN",DEVISTATOR_WEAPON);
    Gv_NewVar(aszBuf, 0, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_SHOTSPERBURST",DEVISTATOR_WEAPON);
    Gv_NewVar(aszBuf, 2, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_INITIALSOUND",DEVISTATOR_WEAPON);
    Gv_NewVar(aszBuf, CAT_FIRE, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_FIRESOUND",DEVISTATOR_WEAPON);
    Gv_NewVar(aszBuf, 0, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_SOUND2TIME",DEVISTATOR_WEAPON);
    Gv_NewVar(aszBuf, 0, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_SOUND2SOUND",DEVISTATOR_WEAPON);
    Gv_NewVar(aszBuf, 0, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_RELOADSOUND1",DEVISTATOR_WEAPON);
    Gv_NewVar(aszBuf, EJECT_CLIP, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_RELOADSOUND2",DEVISTATOR_WEAPON);
    Gv_NewVar(aszBuf, INSERT_CLIP, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_SELECTSOUND",DEVISTATOR_WEAPON);
    Gv_NewVar(aszBuf, SELECT_WEAPON, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);

    /////////////////////////////
    Bsprintf(aszBuf,"WEAPON%d_WORKSLIKE",TRIPBOMB_WEAPON);
    Gv_NewVar(aszBuf, TRIPBOMB_WEAPON, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_CLIP",TRIPBOMB_WEAPON);
    Gv_NewVar(aszBuf, 0, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_RELOAD",TRIPBOMB_WEAPON);
    Gv_NewVar(aszBuf, 16, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_FIREDELAY",TRIPBOMB_WEAPON);
    Gv_NewVar(aszBuf, 3, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_TOTALTIME",TRIPBOMB_WEAPON);
    Gv_NewVar(aszBuf, 16, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_HOLDDELAY",TRIPBOMB_WEAPON);
    Gv_NewVar(aszBuf, 7, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_FLAGS",TRIPBOMB_WEAPON);
    Gv_NewVar(aszBuf, WEAPON_STANDSTILL | WEAPON_CHECKATRELOAD, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_SHOOTS",TRIPBOMB_WEAPON);
    Gv_NewVar(aszBuf, HANDHOLDINGLASER, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_SPAWNTIME",TRIPBOMB_WEAPON);
    Gv_NewVar(aszBuf, 0, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_SPAWN",TRIPBOMB_WEAPON);
    Gv_NewVar(aszBuf, 0, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_SHOTSPERBURST",TRIPBOMB_WEAPON);
    Gv_NewVar(aszBuf, 0, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_INITIALSOUND",TRIPBOMB_WEAPON);
    Gv_NewVar(aszBuf, 0, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_FIRESOUND",TRIPBOMB_WEAPON);
    Gv_NewVar(aszBuf, 0, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_SOUND2TIME",TRIPBOMB_WEAPON);
    Gv_NewVar(aszBuf, 0, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_SOUND2SOUND",TRIPBOMB_WEAPON);
    Gv_NewVar(aszBuf, 0, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_RELOADSOUND1",TRIPBOMB_WEAPON);
    Gv_NewVar(aszBuf, EJECT_CLIP, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_RELOADSOUND2",TRIPBOMB_WEAPON);
    Gv_NewVar(aszBuf, INSERT_CLIP, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_SELECTSOUND",TRIPBOMB_WEAPON);
    Gv_NewVar(aszBuf, 0, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);

    /////////////////////////////
    Bsprintf(aszBuf,"WEAPON%d_WORKSLIKE",FREEZE_WEAPON);
    Gv_NewVar(aszBuf, FREEZE_WEAPON, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_CLIP",FREEZE_WEAPON);
    Gv_NewVar(aszBuf, 0, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_RELOAD",FREEZE_WEAPON);
    Gv_NewVar(aszBuf, 0, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_FIREDELAY",FREEZE_WEAPON);
    Gv_NewVar(aszBuf, 3, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_TOTALTIME",FREEZE_WEAPON);
    Gv_NewVar(aszBuf, 5, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_HOLDDELAY",FREEZE_WEAPON);
    Gv_NewVar(aszBuf, 0, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_FLAGS",FREEZE_WEAPON);
    Gv_NewVar(aszBuf, WEAPON_RESET, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_SHOOTS",FREEZE_WEAPON);
    Gv_NewVar(aszBuf, FREEZEBLAST, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_SPAWNTIME",FREEZE_WEAPON);
    Gv_NewVar(aszBuf, 0, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_SPAWN",FREEZE_WEAPON);
    Gv_NewVar(aszBuf, 0, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_SHOTSPERBURST",FREEZE_WEAPON);
    Gv_NewVar(aszBuf, 0, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_INITIALSOUND",FREEZE_WEAPON);
    Gv_NewVar(aszBuf, CAT_FIRE, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_FIRESOUND",FREEZE_WEAPON);
    Gv_NewVar(aszBuf, CAT_FIRE, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_SOUND2TIME",FREEZE_WEAPON);
    Gv_NewVar(aszBuf, 0, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_SOUND2SOUND",FREEZE_WEAPON);
    Gv_NewVar(aszBuf, 0, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_RELOADSOUND1",FREEZE_WEAPON);
    Gv_NewVar(aszBuf, EJECT_CLIP, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_RELOADSOUND2",FREEZE_WEAPON);
    Gv_NewVar(aszBuf, INSERT_CLIP, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_SELECTSOUND",FREEZE_WEAPON);
    Gv_NewVar(aszBuf, SELECT_WEAPON, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);

    /////////////////////////////
    Bsprintf(aszBuf,"WEAPON%d_WORKSLIKE",HANDREMOTE_WEAPON);
    Gv_NewVar(aszBuf, HANDREMOTE_WEAPON, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_CLIP",HANDREMOTE_WEAPON);
    Gv_NewVar(aszBuf, 0, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_RELOAD",HANDREMOTE_WEAPON);
    Gv_NewVar(aszBuf, 10, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_FIREDELAY",HANDREMOTE_WEAPON);
    Gv_NewVar(aszBuf, 2, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_TOTALTIME",HANDREMOTE_WEAPON);
    Gv_NewVar(aszBuf, 10, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_HOLDDELAY",HANDREMOTE_WEAPON);
    Gv_NewVar(aszBuf, 0, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_FLAGS",HANDREMOTE_WEAPON);
    Gv_NewVar(aszBuf, WEAPON_BOMB_TRIGGER | WEAPON_NOVISIBLE, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_SHOOTS",HANDREMOTE_WEAPON);
    Gv_NewVar(aszBuf, 0, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_SPAWNTIME",HANDREMOTE_WEAPON);
    Gv_NewVar(aszBuf, 0, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_SPAWN",HANDREMOTE_WEAPON);
    Gv_NewVar(aszBuf, 0, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_SHOTSPERBURST",HANDREMOTE_WEAPON);
    Gv_NewVar(aszBuf, 0, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_INITIALSOUND",HANDREMOTE_WEAPON);
    Gv_NewVar(aszBuf, 0, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_FIRESOUND",HANDREMOTE_WEAPON);
    Gv_NewVar(aszBuf, 0, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_SOUND2TIME",HANDREMOTE_WEAPON);
    Gv_NewVar(aszBuf, 0, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_SOUND2SOUND",HANDREMOTE_WEAPON);
    Gv_NewVar(aszBuf, 0, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_RELOADSOUND1",HANDREMOTE_WEAPON);
    Gv_NewVar(aszBuf, EJECT_CLIP, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_RELOADSOUND2",HANDREMOTE_WEAPON);
    Gv_NewVar(aszBuf, INSERT_CLIP, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_SELECTSOUND",HANDREMOTE_WEAPON);
    Gv_NewVar(aszBuf, 0, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);

    ///////////////////////////////////////////////////////
    Bsprintf(aszBuf,"WEAPON%d_WORKSLIKE",GROW_WEAPON);
    Gv_NewVar(aszBuf, GROW_WEAPON, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_CLIP",GROW_WEAPON);
    Gv_NewVar(aszBuf, 0, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_RELOAD",GROW_WEAPON);
    Gv_NewVar(aszBuf, 0, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_FIREDELAY",GROW_WEAPON);
    Gv_NewVar(aszBuf, 3, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_TOTALTIME",GROW_WEAPON);
    Gv_NewVar(aszBuf, NAM?30:5, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_HOLDDELAY",GROW_WEAPON);
    Gv_NewVar(aszBuf, 0, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_FLAGS",GROW_WEAPON);
    Gv_NewVar(aszBuf, WEAPON_GLOWS, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_SHOOTS",GROW_WEAPON);
    Gv_NewVar(aszBuf, GROWSPARK, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_SPAWNTIME",GROW_WEAPON);
    Gv_NewVar(aszBuf, NAM?2:0, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_SPAWN",GROW_WEAPON);
    Gv_NewVar(aszBuf, NAM?SHELL:0, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_SHOTSPERBURST",GROW_WEAPON);
    Gv_NewVar(aszBuf, 0, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_INITIALSOUND",GROW_WEAPON);
    Gv_NewVar(aszBuf, 0, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_FIRESOUND",GROW_WEAPON);
    Gv_NewVar(aszBuf, NAM?0:EXPANDERSHOOT, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_SOUND2TIME",GROW_WEAPON);
    Gv_NewVar(aszBuf, 0, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_SOUND2SOUND",GROW_WEAPON);
    Gv_NewVar(aszBuf, 0, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_RELOADSOUND1",GROW_WEAPON);
    Gv_NewVar(aszBuf, EJECT_CLIP, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_RELOADSOUND2",GROW_WEAPON);
    Gv_NewVar(aszBuf, INSERT_CLIP, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Bsprintf(aszBuf,"WEAPON%d_SELECTSOUND",GROW_WEAPON);
    Gv_NewVar(aszBuf, SELECT_WEAPON, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);

    Gv_NewVar("GRENADE_LIFETIME", NAM_GRENADE_LIFETIME, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Gv_NewVar("GRENADE_LIFETIME_VAR", NAM_GRENADE_LIFETIME_VAR, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);

    Gv_NewVar("STICKYBOMB_LIFETIME", NAM_GRENADE_LIFETIME, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Gv_NewVar("STICKYBOMB_LIFETIME_VAR", NAM_GRENADE_LIFETIME_VAR, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);

    Gv_NewVar("TRIPBOMB_CONTROL", TRIPBOMB_TRIPWIRE, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Gv_NewVar("PIPEBOMB_CONTROL", NAM?PIPEBOMB_TIMER:PIPEBOMB_REMOTE, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);

    Gv_NewVar("RESPAWN_MONSTERS", (intptr_t)&ud.respawn_monsters,GAMEVAR_SYSTEM | GAMEVAR_INTPTR);
    Gv_NewVar("RESPAWN_ITEMS",(intptr_t)&ud.respawn_items, GAMEVAR_SYSTEM | GAMEVAR_INTPTR);
    Gv_NewVar("RESPAWN_INVENTORY",(intptr_t)&ud.respawn_inventory, GAMEVAR_SYSTEM | GAMEVAR_INTPTR);
    Gv_NewVar("MONSTERS_OFF",(intptr_t)&ud.monsters_off, GAMEVAR_SYSTEM | GAMEVAR_INTPTR);
    Gv_NewVar("MARKER",(intptr_t)&ud.marker, GAMEVAR_SYSTEM | GAMEVAR_INTPTR);
    Gv_NewVar("FFIRE",(intptr_t)&ud.ffire, GAMEVAR_SYSTEM | GAMEVAR_INTPTR);
    Gv_NewVar("LEVEL",(intptr_t)&ud.level_number, GAMEVAR_SYSTEM | GAMEVAR_INTPTR | GAMEVAR_READONLY);
    Gv_NewVar("VOLUME",(intptr_t)&ud.volume_number, GAMEVAR_SYSTEM | GAMEVAR_INTPTR | GAMEVAR_READONLY);

    Gv_NewVar("COOP",(intptr_t)&ud.coop, GAMEVAR_SYSTEM | GAMEVAR_INTPTR);
    Gv_NewVar("MULTIMODE",(intptr_t)&ud.multimode, GAMEVAR_SYSTEM | GAMEVAR_INTPTR);

    Gv_NewVar("WEAPON", 0, GAMEVAR_PERPLAYER | GAMEVAR_READONLY | GAMEVAR_SYSTEM);
    Gv_NewVar("WORKSLIKE", 0, GAMEVAR_PERPLAYER | GAMEVAR_READONLY | GAMEVAR_SYSTEM);
    Gv_NewVar("RETURN", 0, GAMEVAR_SYSTEM);
    Gv_NewVar("ZRANGE", 4, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Gv_NewVar("ANGRANGE", 18, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Gv_NewVar("AUTOAIMANGLE", 0, GAMEVAR_PERPLAYER | GAMEVAR_SYSTEM);
    Gv_NewVar("LOTAG", 0, GAMEVAR_SYSTEM);
    Gv_NewVar("HITAG", 0, GAMEVAR_SYSTEM);
    Gv_NewVar("TEXTURE", 0, GAMEVAR_SYSTEM);
    Gv_NewVar("THISACTOR", 0, GAMEVAR_READONLY | GAMEVAR_SYSTEM);
    Gv_NewVar("myconnectindex", (intptr_t)&myconnectindex, GAMEVAR_READONLY | GAMEVAR_INTPTR | GAMEVAR_SYSTEM | GAMEVAR_SYNCCHECK);
    Gv_NewVar("screenpeek", (intptr_t)&screenpeek, GAMEVAR_READONLY | GAMEVAR_INTPTR | GAMEVAR_SYSTEM | GAMEVAR_SYNCCHECK);
    Gv_NewVar("currentweapon",(intptr_t)&g_currentweapon, GAMEVAR_INTPTR | GAMEVAR_SYSTEM | GAMEVAR_SYNCCHECK);
    Gv_NewVar("gs",(intptr_t)&g_gs, GAMEVAR_INTPTR | GAMEVAR_SYSTEM | GAMEVAR_SYNCCHECK);
    Gv_NewVar("looking_arc",(intptr_t)&g_looking_arc, GAMEVAR_INTPTR | GAMEVAR_SYSTEM | GAMEVAR_SYNCCHECK);
    Gv_NewVar("gun_pos",(intptr_t)&g_gun_pos, GAMEVAR_INTPTR | GAMEVAR_SYSTEM | GAMEVAR_SYNCCHECK);
    Gv_NewVar("weapon_xoffset",(intptr_t)&g_weapon_xoffset, GAMEVAR_INTPTR | GAMEVAR_SYSTEM | GAMEVAR_SYNCCHECK);
    Gv_NewVar("weaponcount",(intptr_t)&g_kb, GAMEVAR_INTPTR | GAMEVAR_SYSTEM | GAMEVAR_SYNCCHECK);
    Gv_NewVar("looking_angSR1",(intptr_t)&g_looking_angSR1, GAMEVAR_INTPTR | GAMEVAR_SYSTEM | GAMEVAR_SYNCCHECK);
    Gv_NewVar("xdim",(intptr_t)&xdim, GAMEVAR_INTPTR | GAMEVAR_SYSTEM | GAMEVAR_READONLY | GAMEVAR_SYNCCHECK);
    Gv_NewVar("ydim",(intptr_t)&ydim, GAMEVAR_INTPTR | GAMEVAR_SYSTEM | GAMEVAR_READONLY | GAMEVAR_SYNCCHECK);
    Gv_NewVar("windowx1",(intptr_t)&windowx1, GAMEVAR_INTPTR | GAMEVAR_SYSTEM | GAMEVAR_READONLY | GAMEVAR_SYNCCHECK);
    Gv_NewVar("windowx2",(intptr_t)&windowx2, GAMEVAR_INTPTR | GAMEVAR_SYSTEM | GAMEVAR_READONLY | GAMEVAR_SYNCCHECK);
    Gv_NewVar("windowy1",(intptr_t)&windowy1, GAMEVAR_INTPTR | GAMEVAR_SYSTEM | GAMEVAR_READONLY | GAMEVAR_SYNCCHECK);
    Gv_NewVar("windowy2",(intptr_t)&windowy2, GAMEVAR_INTPTR | GAMEVAR_SYSTEM | GAMEVAR_READONLY | GAMEVAR_SYNCCHECK);
    Gv_NewVar("totalclock",(intptr_t)&totalclock, GAMEVAR_INTPTR | GAMEVAR_SYSTEM | GAMEVAR_READONLY | GAMEVAR_SYNCCHECK);
    Gv_NewVar("lastvisinc",(intptr_t)&lastvisinc, GAMEVAR_SYSTEM | GAMEVAR_INTPTR | GAMEVAR_SYNCCHECK);
    Gv_NewVar("numsectors",(intptr_t)&numsectors, GAMEVAR_SYSTEM | GAMEVAR_SHORTPTR | GAMEVAR_READONLY);

    Gv_NewVar("current_menu",(intptr_t)&g_currentMenu, GAMEVAR_SYSTEM | GAMEVAR_INTPTR | GAMEVAR_READONLY);
    Gv_NewVar("numplayers",(intptr_t)&numplayers, GAMEVAR_SYSTEM | GAMEVAR_INTPTR | GAMEVAR_READONLY);
    Gv_NewVar("viewingrange",(intptr_t)&viewingrange, GAMEVAR_SYSTEM | GAMEVAR_INTPTR | GAMEVAR_READONLY | GAMEVAR_SYNCCHECK);
    Gv_NewVar("yxaspect",(intptr_t)&yxaspect, GAMEVAR_SYSTEM | GAMEVAR_INTPTR | GAMEVAR_READONLY | GAMEVAR_SYNCCHECK);
    Gv_NewVar("gravitationalconstant",(intptr_t)&g_spriteGravity, GAMEVAR_SYSTEM | GAMEVAR_INTPTR);
    Gv_NewVar("gametype_flags",(intptr_t)&GametypeFlags[ud.coop], GAMEVAR_SYSTEM | GAMEVAR_INTPTR);
    Gv_NewVar("framerate",(intptr_t)&g_currentFrameRate, GAMEVAR_SYSTEM | GAMEVAR_INTPTR | GAMEVAR_READONLY | GAMEVAR_SYNCCHECK);
    Gv_NewVar("CLIPMASK0", CLIPMASK0, GAMEVAR_SYSTEM|GAMEVAR_READONLY);
    Gv_NewVar("CLIPMASK1", CLIPMASK1, GAMEVAR_SYSTEM|GAMEVAR_READONLY);

    Gv_NewVar("camerax",(intptr_t)&ud.camerax, GAMEVAR_SYSTEM | GAMEVAR_INTPTR | GAMEVAR_SYNCCHECK);
    Gv_NewVar("cameray",(intptr_t)&ud.cameray, GAMEVAR_SYSTEM | GAMEVAR_INTPTR | GAMEVAR_SYNCCHECK);
    Gv_NewVar("cameraz",(intptr_t)&ud.cameraz, GAMEVAR_SYSTEM | GAMEVAR_INTPTR | GAMEVAR_SYNCCHECK);
    Gv_NewVar("cameraang",(intptr_t)&ud.cameraang, GAMEVAR_SYSTEM | GAMEVAR_SHORTPTR | GAMEVAR_SYNCCHECK);
    Gv_NewVar("camerahoriz",(intptr_t)&ud.camerahoriz, GAMEVAR_SYSTEM | GAMEVAR_SHORTPTR | GAMEVAR_SYNCCHECK);
    Gv_NewVar("camerasect",(intptr_t)&ud.camerasect, GAMEVAR_SYSTEM | GAMEVAR_SHORTPTR | GAMEVAR_SYNCCHECK);
    Gv_NewVar("cameradist",(intptr_t)&g_cameraDistance, GAMEVAR_SYSTEM | GAMEVAR_INTPTR | GAMEVAR_SYNCCHECK);
    Gv_NewVar("cameraclock",(intptr_t)&g_cameraClock, GAMEVAR_SYSTEM | GAMEVAR_INTPTR | GAMEVAR_SYNCCHECK);

    Gv_NewVar("myx",(intptr_t)&myx, GAMEVAR_SYSTEM | GAMEVAR_INTPTR | GAMEVAR_SYNCCHECK);
    Gv_NewVar("myy",(intptr_t)&myy, GAMEVAR_SYSTEM | GAMEVAR_INTPTR | GAMEVAR_SYNCCHECK);
    Gv_NewVar("myz",(intptr_t)&myz, GAMEVAR_SYSTEM | GAMEVAR_INTPTR | GAMEVAR_SYNCCHECK);
    Gv_NewVar("omyx",(intptr_t)&omyx, GAMEVAR_SYSTEM | GAMEVAR_INTPTR | GAMEVAR_SYNCCHECK);
    Gv_NewVar("omyy",(intptr_t)&omyy, GAMEVAR_SYSTEM | GAMEVAR_INTPTR | GAMEVAR_SYNCCHECK);
    Gv_NewVar("omyz",(intptr_t)&omyz, GAMEVAR_SYSTEM | GAMEVAR_INTPTR | GAMEVAR_SYNCCHECK);
    Gv_NewVar("myxvel",(intptr_t)&myxvel, GAMEVAR_SYSTEM | GAMEVAR_INTPTR | GAMEVAR_SYNCCHECK);
    Gv_NewVar("myyvel",(intptr_t)&myyvel, GAMEVAR_SYSTEM | GAMEVAR_INTPTR | GAMEVAR_SYNCCHECK);
    Gv_NewVar("myzvel",(intptr_t)&myzvel, GAMEVAR_SYSTEM | GAMEVAR_INTPTR | GAMEVAR_SYNCCHECK);

    Gv_NewVar("myhoriz",(intptr_t)&myhoriz, GAMEVAR_SYSTEM | GAMEVAR_SHORTPTR | GAMEVAR_SYNCCHECK);
    Gv_NewVar("myhorizoff",(intptr_t)&myhorizoff, GAMEVAR_SYSTEM | GAMEVAR_SHORTPTR | GAMEVAR_SYNCCHECK);
    Gv_NewVar("omyhoriz",(intptr_t)&omyhoriz, GAMEVAR_SYSTEM | GAMEVAR_SHORTPTR | GAMEVAR_SYNCCHECK);
    Gv_NewVar("omyhorizoff",(intptr_t)&omyhorizoff, GAMEVAR_SYSTEM | GAMEVAR_SHORTPTR | GAMEVAR_SYNCCHECK);
    Gv_NewVar("myang",(intptr_t)&myang, GAMEVAR_SYSTEM | GAMEVAR_SHORTPTR | GAMEVAR_SYNCCHECK);
    Gv_NewVar("omyang",(intptr_t)&omyang, GAMEVAR_SYSTEM | GAMEVAR_SHORTPTR | GAMEVAR_SYNCCHECK);
    Gv_NewVar("mycursectnum",(intptr_t)&mycursectnum, GAMEVAR_SYSTEM | GAMEVAR_SHORTPTR | GAMEVAR_SYNCCHECK);
    Gv_NewVar("myjumpingcounter",(intptr_t)&myjumpingcounter, GAMEVAR_SYSTEM | GAMEVAR_SHORTPTR | GAMEVAR_SYNCCHECK);

    Gv_NewVar("myjumpingtoggle",(intptr_t)&myjumpingtoggle, GAMEVAR_SYSTEM | GAMEVAR_CHARPTR | GAMEVAR_SYNCCHECK);
    Gv_NewVar("myonground",(intptr_t)&myonground, GAMEVAR_SYSTEM | GAMEVAR_CHARPTR | GAMEVAR_SYNCCHECK);
    Gv_NewVar("myhardlanding",(intptr_t)&myhardlanding, GAMEVAR_SYSTEM | GAMEVAR_CHARPTR | GAMEVAR_SYNCCHECK);
    Gv_NewVar("myreturntocenter",(intptr_t)&myreturntocenter, GAMEVAR_SYSTEM | GAMEVAR_CHARPTR | GAMEVAR_SYNCCHECK);

    Gv_NewVar("display_mirror",(intptr_t)&display_mirror, GAMEVAR_SYSTEM | GAMEVAR_CHARPTR | GAMEVAR_SYNCCHECK);
    Gv_NewVar("randomseed",(intptr_t)&randomseed, GAMEVAR_SYSTEM | GAMEVAR_INTPTR);

    Gv_NewVar("NUMWALLS",(intptr_t)&numwalls, GAMEVAR_SYSTEM | GAMEVAR_SHORTPTR | GAMEVAR_READONLY);
    Gv_NewVar("NUMSECTORS",(intptr_t)&numsectors, GAMEVAR_SYSTEM | GAMEVAR_SHORTPTR | GAMEVAR_READONLY);

    Gv_NewVar("lastsavepos",(intptr_t)&g_lastSaveSlot, GAMEVAR_SYSTEM | GAMEVAR_INTPTR);
}

void Gv_Init(void)
{
    // only call ONCE

    //  initprintf("Initializing game variables\n");
    //AddLog("Gv_Init");

    Gv_Clear();
    Gv_AddSystemVars();
    Gv_InitWeaponPointers();
    Gv_ResetSystemDefaults();
}

void Gv_InitWeaponPointers(void)
{
    int i;
    char aszBuf[64];
    // called from game Init AND when level is loaded...

    //AddLog("Gv_InitWeaponPointers");

    for (i=(MAX_WEAPONS-1);i>=0;i--)
    {
        Bsprintf(aszBuf,"WEAPON%d_CLIP",i);
        aplWeaponClip[i]=Gv_GetVarDataPtr(aszBuf);
        if (!aplWeaponClip[i])
        {
            initprintf("ERROR: NULL weapon!  WTF?!\n");
            // exit(0);
            G_Shutdown();
        }
        Bsprintf(aszBuf,"WEAPON%d_RELOAD",i);
        aplWeaponReload[i]=Gv_GetVarDataPtr(aszBuf);
        Bsprintf(aszBuf,"WEAPON%d_FIREDELAY",i);
        aplWeaponFireDelay[i]=Gv_GetVarDataPtr(aszBuf);
        Bsprintf(aszBuf,"WEAPON%d_TOTALTIME",i);
        aplWeaponTotalTime[i]=Gv_GetVarDataPtr(aszBuf);
        Bsprintf(aszBuf,"WEAPON%d_HOLDDELAY",i);
        aplWeaponHoldDelay[i]=Gv_GetVarDataPtr(aszBuf);
        Bsprintf(aszBuf,"WEAPON%d_FLAGS",i);
        aplWeaponFlags[i]=Gv_GetVarDataPtr(aszBuf);
        Bsprintf(aszBuf,"WEAPON%d_SHOOTS",i);
        aplWeaponShoots[i]=Gv_GetVarDataPtr(aszBuf);
        Bsprintf(aszBuf,"WEAPON%d_SPAWNTIME",i);
        aplWeaponSpawnTime[i]=Gv_GetVarDataPtr(aszBuf);
        Bsprintf(aszBuf,"WEAPON%d_SPAWN",i);
        aplWeaponSpawn[i]=Gv_GetVarDataPtr(aszBuf);
        Bsprintf(aszBuf,"WEAPON%d_SHOTSPERBURST",i);
        aplWeaponShotsPerBurst[i]=Gv_GetVarDataPtr(aszBuf);
        Bsprintf(aszBuf,"WEAPON%d_WORKSLIKE",i);
        aplWeaponWorksLike[i]=Gv_GetVarDataPtr(aszBuf);
        Bsprintf(aszBuf,"WEAPON%d_INITIALSOUND",i);
        aplWeaponInitialSound[i]=Gv_GetVarDataPtr(aszBuf);
        Bsprintf(aszBuf,"WEAPON%d_FIRESOUND",i);
        aplWeaponFireSound[i]=Gv_GetVarDataPtr(aszBuf);
        Bsprintf(aszBuf,"WEAPON%d_SOUND2TIME",i);
        aplWeaponSound2Time[i]=Gv_GetVarDataPtr(aszBuf);
        Bsprintf(aszBuf,"WEAPON%d_SOUND2SOUND",i);
        aplWeaponSound2Sound[i]=Gv_GetVarDataPtr(aszBuf);
        Bsprintf(aszBuf,"WEAPON%d_RELOADSOUND1",i);
        aplWeaponReloadSound1[i]=Gv_GetVarDataPtr(aszBuf);
        Bsprintf(aszBuf,"WEAPON%d_RELOADSOUND2",i);
        aplWeaponReloadSound2[i]=Gv_GetVarDataPtr(aszBuf);
        Bsprintf(aszBuf,"WEAPON%d_SELECTSOUND",i);
        aplWeaponSelectSound[i]=Gv_GetVarDataPtr(aszBuf);
    }
}

void Gv_RefreshPointers(void)
{
    aGameVars[Gv_GetVarIndex("RESPAWN_MONSTERS")].lValue = (intptr_t)&ud.respawn_monsters;
    aGameVars[Gv_GetVarIndex("RESPAWN_ITEMS")].lValue = (intptr_t)&ud.respawn_items;
    aGameVars[Gv_GetVarIndex("RESPAWN_INVENTORY")].lValue = (intptr_t)&ud.respawn_inventory;
    aGameVars[Gv_GetVarIndex("MONSTERS_OFF")].lValue = (intptr_t)&ud.monsters_off;
    aGameVars[Gv_GetVarIndex("MARKER")].lValue = (intptr_t)&ud.marker;
    aGameVars[Gv_GetVarIndex("FFIRE")].lValue = (intptr_t)&ud.ffire;
    aGameVars[Gv_GetVarIndex("LEVEL")].lValue = (intptr_t)&ud.level_number;
    aGameVars[Gv_GetVarIndex("VOLUME")].lValue = (intptr_t)&ud.volume_number;

    aGameVars[Gv_GetVarIndex("COOP")].lValue = (intptr_t)&ud.coop;
    aGameVars[Gv_GetVarIndex("MULTIMODE")].lValue = (intptr_t)&ud.multimode;

    aGameVars[Gv_GetVarIndex("myconnectindex")].lValue = (intptr_t)&myconnectindex;
    aGameVars[Gv_GetVarIndex("screenpeek")].lValue = (intptr_t)&screenpeek;
    aGameVars[Gv_GetVarIndex("currentweapon")].lValue = (intptr_t)&g_currentweapon;
    aGameVars[Gv_GetVarIndex("gs")].lValue = (intptr_t)&g_gs;
    aGameVars[Gv_GetVarIndex("looking_arc")].lValue = (intptr_t)&g_looking_arc;
    aGameVars[Gv_GetVarIndex("gun_pos")].lValue = (intptr_t)&g_gun_pos;
    aGameVars[Gv_GetVarIndex("weapon_xoffset")].lValue = (intptr_t)&g_weapon_xoffset;
    aGameVars[Gv_GetVarIndex("weaponcount")].lValue = (intptr_t)&g_kb;
    aGameVars[Gv_GetVarIndex("looking_angSR1")].lValue = (intptr_t)&g_looking_angSR1;
    aGameVars[Gv_GetVarIndex("xdim")].lValue = (intptr_t)&xdim;
    aGameVars[Gv_GetVarIndex("ydim")].lValue = (intptr_t)&ydim;
    aGameVars[Gv_GetVarIndex("windowx1")].lValue = (intptr_t)&windowx1;
    aGameVars[Gv_GetVarIndex("windowx2")].lValue = (intptr_t)&windowx2;
    aGameVars[Gv_GetVarIndex("windowy1")].lValue = (intptr_t)&windowy1;
    aGameVars[Gv_GetVarIndex("windowy2")].lValue = (intptr_t)&windowy2;
    aGameVars[Gv_GetVarIndex("totalclock")].lValue = (intptr_t)&totalclock;
    aGameVars[Gv_GetVarIndex("lastvisinc")].lValue = (intptr_t)&lastvisinc;
    aGameVars[Gv_GetVarIndex("numsectors")].lValue = (intptr_t)&numsectors;
    aGameVars[Gv_GetVarIndex("numplayers")].lValue = (intptr_t)&numplayers;
    aGameVars[Gv_GetVarIndex("current_menu")].lValue = (intptr_t)&g_currentMenu;
    aGameVars[Gv_GetVarIndex("viewingrange")].lValue = (intptr_t)&viewingrange;
    aGameVars[Gv_GetVarIndex("yxaspect")].lValue = (intptr_t)&yxaspect;
    aGameVars[Gv_GetVarIndex("gravitationalconstant")].lValue = (intptr_t)&g_spriteGravity;
    aGameVars[Gv_GetVarIndex("gametype_flags")].lValue = (intptr_t)&GametypeFlags[ud.coop];
    aGameVars[Gv_GetVarIndex("framerate")].lValue = (intptr_t)&g_currentFrameRate;

    aGameVars[Gv_GetVarIndex("camerax")].lValue = (intptr_t)&ud.camerax;
    aGameVars[Gv_GetVarIndex("cameray")].lValue = (intptr_t)&ud.cameray;
    aGameVars[Gv_GetVarIndex("cameraz")].lValue = (intptr_t)&ud.cameraz;
    aGameVars[Gv_GetVarIndex("cameraang")].lValue = (intptr_t)&ud.cameraang;
    aGameVars[Gv_GetVarIndex("camerahoriz")].lValue = (intptr_t)&ud.camerahoriz;
    aGameVars[Gv_GetVarIndex("camerasect")].lValue = (intptr_t)&ud.camerasect;
    aGameVars[Gv_GetVarIndex("cameradist")].lValue = (intptr_t)&g_cameraDistance;
    aGameVars[Gv_GetVarIndex("cameraclock")].lValue = (intptr_t)&g_cameraClock;

    aGameVars[Gv_GetVarIndex("myx")].lValue = (intptr_t)&myx;
    aGameVars[Gv_GetVarIndex("myy")].lValue = (intptr_t)&myy;
    aGameVars[Gv_GetVarIndex("myz")].lValue = (intptr_t)&myz;
    aGameVars[Gv_GetVarIndex("omyx")].lValue = (intptr_t)&omyx;
    aGameVars[Gv_GetVarIndex("omyy")].lValue = (intptr_t)&omyy;
    aGameVars[Gv_GetVarIndex("omyz")].lValue = (intptr_t)&omyz;
    aGameVars[Gv_GetVarIndex("myxvel")].lValue = (intptr_t)&myxvel;
    aGameVars[Gv_GetVarIndex("myyvel")].lValue = (intptr_t)&myyvel;
    aGameVars[Gv_GetVarIndex("myzvel")].lValue = (intptr_t)&myzvel;

    aGameVars[Gv_GetVarIndex("myhoriz")].lValue = (intptr_t)&myhoriz;
    aGameVars[Gv_GetVarIndex("myhorizoff")].lValue = (intptr_t)&myhorizoff;
    aGameVars[Gv_GetVarIndex("omyhoriz")].lValue = (intptr_t)&omyhoriz;
    aGameVars[Gv_GetVarIndex("omyhorizoff")].lValue = (intptr_t)&omyhorizoff;
    aGameVars[Gv_GetVarIndex("myang")].lValue = (intptr_t)&myang;
    aGameVars[Gv_GetVarIndex("omyang")].lValue = (intptr_t)&omyang;
    aGameVars[Gv_GetVarIndex("mycursectnum")].lValue = (intptr_t)&mycursectnum;
    aGameVars[Gv_GetVarIndex("myjumpingcounter")].lValue = (intptr_t)&myjumpingcounter;

    aGameVars[Gv_GetVarIndex("myjumpingtoggle")].lValue = (intptr_t)&myjumpingtoggle;
    aGameVars[Gv_GetVarIndex("myonground")].lValue = (intptr_t)&myonground;
    aGameVars[Gv_GetVarIndex("myhardlanding")].lValue = (intptr_t)&myhardlanding;
    aGameVars[Gv_GetVarIndex("myreturntocenter")].lValue = (intptr_t)&myreturntocenter;

    aGameVars[Gv_GetVarIndex("display_mirror")].lValue = (intptr_t)&display_mirror;
    aGameVars[Gv_GetVarIndex("randomseed")].lValue = (intptr_t)&randomseed;

    aGameVars[Gv_GetVarIndex("NUMWALLS")].lValue = (intptr_t)&numwalls;
    aGameVars[Gv_GetVarIndex("NUMSECTORS")].lValue = (intptr_t)&numsectors;

    aGameVars[Gv_GetVarIndex("lastsavepos")].lValue = (intptr_t)&g_lastSaveSlot;
}
