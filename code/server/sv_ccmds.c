/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#include "server.h"

/*
===============================================================================

OPERATOR CONSOLE ONLY COMMANDS

These commands can only be entered from stdin or by a remote operator datagram
===============================================================================
*/


/*
==================
SV_GetPlayerByHandle

Returns the player with player id or name from Cmd_Argv(1)
==================
*/
static client_t *SV_GetPlayerByHandle( void ) {
    client_t    *cl;
    int         i;
    char        *s;
    char        cleanName[64];

    // make sure server is running
    if ( !com_sv_running->integer ) {
        return NULL;
    }

    if ( Cmd_Argc() < 2 ) {
        Com_Printf( "No player specified.\n" );
        return NULL;
    }

    s = Cmd_Argv(1);

    // Check whether this is a numeric player handle
    for(i = 0; s[i] >= '0' && s[i] <= '9'; i++);

    if(!s[i])
    {
        int plid = atoi(s);

        // Check for numeric playerid match
        if(plid >= 0 && plid < sv_maxclients->integer)
        {
            cl = &svs.clients[plid];

            if(cl->state)
                return cl;
        }
    }

    // check for a name match
    for ( i=0, cl=svs.clients ; i < sv_maxclients->integer ; i++,cl++ ) {
        if ( !cl->state ) {
            continue;
        }
        if ( !Q_stricmp( cl->name, s ) ) {
            return cl;
        }

        Q_strncpyz( cleanName, cl->name, sizeof(cleanName) );
        Q_CleanStr( cleanName );
        if ( !Q_stricmp( cleanName, s ) ) {
            return cl;
        }
    }

    Com_Printf( "Player %s is not on the server\n", s );

    return NULL;
}

/*
==================
SV_GetPlayerByNum

Returns the player with idnum from Cmd_Argv(1)
==================
*/
static client_t *SV_GetPlayerByNum( void ) {
    client_t    *cl;
    int         i;
    int         idnum;
    char        *s;

    // make sure server is running
    if ( !com_sv_running->integer ) {
        return NULL;
    }

    if ( Cmd_Argc() < 2 ) {
        Com_Printf( "No player specified.\n" );
        return NULL;
    }

    s = Cmd_Argv(1);

    for (i = 0; s[i]; i++) {
        if (s[i] < '0' || s[i] > '9') {
            Com_Printf( "Bad slot number: %s\n", s);
            return NULL;
        }
    }
    idnum = atoi( s );
    if ( idnum < 0 || idnum >= sv_maxclients->integer ) {
        Com_Printf( "Bad client slot: %i\n", idnum );
        return NULL;
    }

    cl = &svs.clients[idnum];
    if ( !cl->state ) {
        Com_Printf( "Client %i is not active\n", idnum );
        return NULL;
    }
    return cl;
}

//=========================================================


/*
==================
SV_Map_f

Restart the server on a different map
==================
*/
static void SV_Map_f( void ) {
    char        *cmd;
    char        *map;
    qboolean    killBots, cheat, altmap = qfalse;
    char        expanded[MAX_QPATH];
    char        mapname[MAX_QPATH];

    map = Cmd_Argv(1);
    if ( !map ) {
        return;
    }

    // make sure the level exists before trying to change, so that
    // a typo at the server console won't end the game
    Com_sprintf (expanded, sizeof(expanded), "maps/%s.bsp", map);
    if ( FS_ReadFile (expanded, NULL) == -1 ) {
        Com_Printf ("Can't find map %s\n", expanded);
        return;
    }

    // force latched values to get set
    Cvar_Get ("g_gametype", "dm", CVAR_SERVERINFO | CVAR_USERINFO | CVAR_LATCH );

    cmd = Cmd_Argv(0);
    if (!Q_stricmpn(cmd, "devmap", 6) || !Q_stricmp(cmd, "spdevmap")) {
        cheat = qtrue;
        killBots = qtrue;
        altmap = qfalse;
    } else if (!Q_stricmp(cmd, "altmap")) {
        cheat = qfalse;
        killBots = qfalse;
        altmap = qtrue;
        Cvar_Set("sv_altmap", "1");
    } else {
        cheat = qfalse;
        killBots = qfalse;
        altmap = qfalse;
    }

    // save the map name here cause on a map restart we reload the q3config.cfg
    // and thus nuke the arguments of the map command
    Q_strncpyz(mapname, map, sizeof(mapname));

    // start up the map
    SV_SpawnServer( mapname, killBots );

    // set the cheat value
    // if the level was started with "map <levelname>", then
    // cheats will not be allowed.  If started with "devmap <levelname>"
    // then cheats will be allowed
    if ( cheat ) {
        Cvar_Set( "sv_cheats", "1" );
    } else {
        Cvar_Set( "sv_cheats", "0" );
    }
}

/*
==================
SV_Mapcycle_f

Switch the server to the next map
in the specified map cycle file.
==================
*/
static char     lastMapGroup[1024] = { 0 };
static void SV_Mapcycle_f(void)
{
    TGenericParser2 GP2;
    TGPGroup        mapcycleGroup;
    TGPGroup        cvarGroup;
    TGPValue        cvarPairs;
    TGPGroup        subGroups;
    TGPGroup        nextMap;
    qboolean        fileValid;
    char            key[64];
    char            value[64];

    //
    // Validate the specified map cycle file.
    //

    // Try to open the GP2 file.
    fileValid = qfalse;
    GP2 = GP_ParseFile(sv_mapcycle->string);

    if(!GP2){
        Com_Printf(S_COLOR_RED "ERROR: map cycle file \"%s\" not found!\n", sv_mapcycle->string);
    }else{
        // Check if the "mapcycle" subgroup is present.
        mapcycleGroup = GPG_FindSubGroup(GP_GetBaseParseGroup(GP2), "mapcycle");
        subGroups = GPG_GetSubGroups(mapcycleGroup);

        if(mapcycleGroup == NULL){
            Com_Printf(S_COLOR_RED "ERROR: map cycle file \"%s\" is invalid: \"mapcycle\" group not found!\n", sv_mapcycle->string);
            GP_Delete(&GP2);
        }else if(subGroups == NULL){
            Com_Printf(S_COLOR_RED "ERROR: map cycle file \"%s\" is invalid: no map groups are found!\n", sv_mapcycle->string);
            GP_Delete(&GP2);
        }else{
            fileValid = qtrue;
        }
    }

    // If the map cycle file is invalid, restart the current map.
    if(!fileValid){
        // FIXME BOE
        //if(RMG->integer > 0){
        //    Com_Printf(S_COLOR_RED "Restarting RMG map due to error.\n");
        //    Cbuf_AddText("rmgmap 0");
        //}else{
            // If the server is not yet started, we cannot restart the current map.
            // In this case, the server will not start.
            if(!com_sv_running->integer){
                Com_Printf(S_COLOR_RED "Server not starting due to error.\n");
            }else{
                Com_Printf(S_COLOR_RED "Restarting current map due to error.\n");
                Cbuf_AddText("map_restart 0");
            }
        //}

        return;
    }

    //
    // Find the next map group.
    //

    // Are we continuing from somewhere within the cycle?
    nextMap = NULL;
    if(strlen(lastMapGroup) > 0){
        // Find the next sub group.
        nextMap = GPG_GetNext(GPG_FindSubGroup(mapcycleGroup, lastMapGroup));
    }

    // If we have no next map, we are either just
    // starting or reached the end of the cycle.
    if(!nextMap){
        nextMap = subGroups;
    }

    //
    // Parse the next map cycle group.
    //

    // Iterate through CVARs to set, if any.
    cvarGroup = GPG_FindSubGroup(nextMap, "cvars");
    cvarPairs = GPG_GetPairs(cvarGroup);

    while(cvarPairs != NULL){
        // Get the CVAR name and value.
        GPV_GetName(cvarPairs, key, sizeof(key));
        GPV_GetTopValue(cvarPairs, value, sizeof(value));

        // Set the CVAR.
        Cvar_Set(key, value);

        // Advance to the next CVAR pair.
        cvarPairs = GPV_GetNext(cvarPairs);
    }

    // Save the current position in the map cycle file.
    GPG_GetName(nextMap, lastMapGroup, sizeof(lastMapGroup));

    // Execute the command.
    GPG_FindPairValue(nextMap, "command", "", value, sizeof(value));
    Cbuf_AddText(value);

    GP_Delete(&GP2);
}

int SV_MapcycleList(char* output, int sizeofOutput) {

    TGenericParser2 GP2;
    TGPGroup        mapcycleGroup;
    TGPGroup        cvarGroup;
    TGPValue        cvarPairs;
    TGPGroup        subGroups;
    TGPGroup        nextMap;
    qboolean        fileValid;
    char            key[64];
    char            value[64];

    Com_Memset(output, 0, sizeofOutput);

    fileValid = qfalse;
    GP2 = GP_ParseFile(sv_mapcycle->string);
    if (!GP2) {
        return 0;
    }
    else {
        // Check if the "mapcycle" subgroup is present.
        mapcycleGroup = GPG_FindSubGroup(GP_GetBaseParseGroup(GP2), "mapcycle");
        subGroups = GPG_GetSubGroups(mapcycleGroup);

        if (mapcycleGroup == NULL) {
            
            GP_Delete(&GP2);
            return 0;
        }
        else if (subGroups != NULL) {
            fileValid = qtrue;
        }
    }

    if (!fileValid) {
        GP_Delete(&GP2);
        return 0;
    }

    nextMap = subGroups;

    while (nextMap != NULL) {
        // We care about the command, map name, gametype. Nothing more.
        char curMapGroup[MAX_QPATH], mapCommand[MAX_QPATH], mapType[MAX_QPATH], mapName[MAX_QPATH], mapGametype[MAX_QPATH];
        GPG_GetName(nextMap, curMapGroup, sizeof(curMapGroup));
        qboolean isCurrentMap = qfalse, foundGt = qfalse;

        if (!Q_stricmp(curMapGroup, lastMapGroup)) {
            isCurrentMap = qtrue;
        }

        GPG_FindPairValue(nextMap, "command", "", mapCommand, sizeof(mapCommand));

        char* mapPointer = strchr(mapCommand, ' ');

        if (mapPointer) {
            Q_strncpyz(mapType, mapCommand, mapPointer - mapCommand + 1);
        }
        else {
            // shouldn't happen ever.
            Q_strncpyz(mapType, "map", sizeof(mapType));
        }

        qboolean isDevmap = qfalse, isAltmap = qfalse, isMap = qtrue;

        if (!Q_stricmp(mapCommand, "altmap")) {
            isAltmap = qtrue;
            isMap = qfalse;
        }
        else if (!Q_stricmp(mapCommand, "devmap")) {
            isDevmap = qtrue;
            isMap = qfalse;
        }

        mapPointer++;
        Q_strncpyz(mapName, mapPointer, sizeof(mapName));

        cvarGroup = GPG_FindSubGroup(nextMap, "cvars");
        cvarPairs = GPG_GetPairs(cvarGroup);

        while (cvarPairs != NULL) {
            GPV_GetName(cvarPairs, key, sizeof(key));
            GPV_GetTopValue(cvarPairs, value, sizeof(value));
            if (!Q_stricmp(key, "g_gametype")) {
                Q_strncpyz(mapGametype, value, sizeof(mapGametype));
                foundGt = qtrue;
                break;
            }
            cvarPairs = GPV_GetNext(cvarPairs);
        }

        if (!foundGt) {
            Q_strncpyz(mapGametype, Cvar_VariableString("g_gametype"), sizeof(mapGametype));
        }

        Q_strcat(output, sizeofOutput, va("%s %-6.6s%-20.20s%-9.9s%-4.4s%-4.4s%-4.4s\n", isCurrentMap ? "*" : " ",curMapGroup, mapName, mapGametype, isMap ? "X" : "", isAltmap ? "X" : "", isDevmap ? "X" : ""));

        nextMap = GPG_GetNext(GPG_FindSubGroup(mapcycleGroup, curMapGroup));
    }
    GP_Delete(&GP2);

    return 1;
}

void SV_SkipToMap(int mapId) {
    mapId = Com_Clamp(0, 99, --mapId);
    if (mapId == 0) {
        Com_Memset(lastMapGroup, 0, sizeof(lastMapGroup));
    }
    else {
        Q_strncpyz(lastMapGroup, va("map%d", mapId), sizeof(lastMapGroup));
    }
    
    SV_Mapcycle_f();
}

/*
================
SV_MapRestart_f

Completely restarts a level, but doesn't send a new gamestate to the clients.
This allows fair starts with variable load times.
================
*/
static void SV_MapRestart_f( void ) {
    int         i;
    client_t    *client;
    char        *denied;
    qboolean    isBot;
    int         delay;

    // make sure we aren't restarting twice in the same frame
    if ( com_frameTime == sv.serverId ) {
        return;
    }

    // make sure server is running
    if ( !com_sv_running->integer ) {
        Com_Printf( "Server is not running.\n" );
        return;
    }

    if ( sv.restartTime ) {
        return;
    }

    if (Cmd_Argc() > 1 ) {
        delay = atoi( Cmd_Argv(1) );
    }
    else {
        delay = 5;
    }
    if( delay && !Cvar_VariableValue("g_doWarmup") ) {
        sv.restartTime = sv.time + delay * 1000;
        SV_SetConfigstring( CS_WARMUP, va("%i", sv.restartTime) );
        return;
    }

    // check for changes in variables that can't just be restarted
    // check for maxclients change
    if ( sv_maxclients->modified || sv_gametype->modified ) {
        char    mapname[MAX_QPATH];

        Com_Printf( "variable change -- restarting.\n" );
        // restart the map the slow way
        Q_strncpyz( mapname, Cvar_VariableString( "mapname" ), sizeof( mapname ) );

        SV_SpawnServer( mapname, qfalse );
        return;
    }

    // toggle the server bit so clients can detect that a
    // map_restart has happened
    svs.snapFlagServerBit ^= SNAPFLAG_SERVERCOUNT;

    // generate a new serverid
    // TTimo - don't update restartedserverId there, otherwise we won't deal correctly with multiple map_restart
    sv.serverId = com_frameTime;
    Cvar_Set( "sv_serverid", va("%i", sv.serverId ) );

    // if a map_restart occurs while a client is changing maps, we need
    // to give them the correct time so that when they finish loading
    // they don't violate the backwards time check in cl_cgame.c
    for (i=0 ; i<sv_maxclients->integer ; i++) {
        if (svs.clients[i].state == CS_PRIMED) {
            svs.clients[i].oldServerTime = sv.restartTime;
        }
    }

    // reset all the vm data in place without changing memory allocation
    // note that we do NOT set sv.state = SS_LOADING, so configstrings that
    // had been changed from their default values will generate broadcast updates
    sv.state = SS_LOADING;
    sv.restarting = qtrue;

    SV_RestartGameProgs();

    // run a few frames to allow everything to settle
    for (i = 0; i < 3; i++)
    {
        VM_Call (gvm, GAME_RUN_FRAME, sv.time);
        sv.time += 100;
        svs.time += 100;
    }

    sv.state = SS_GAME;
    sv.restarting = qfalse;

    // connect and begin all the clients
    for (i=0 ; i<sv_maxclients->integer ; i++) {
        client = &svs.clients[i];

        // send the new gamestate to all connected clients
        if ( client->state < CS_CONNECTED) {
            continue;
        }

        if ( client->netchan.remoteAddress.type == NA_BOT ) {
            isBot = qtrue;
        } else {
            isBot = qfalse;
        }

        // add the map_restart command
        SV_AddServerCommand( client, "map_restart\n" );

        // connect the client again, without the firstTime flag
        denied = VM_ExplicitArgPtr( gvm, VM_Call( gvm, GAME_CLIENT_CONNECT, i, qfalse, isBot ) );
        if ( denied ) {
            // this generally shouldn't happen, because the client
            // was connected before the level change
            SV_DropClient( client, denied );
            Com_Printf( "SV_MapRestart_f(%d): dropped client %i - denied!\n", delay, i );
            continue;
        }

        if(client->state == CS_ACTIVE)
            SV_ClientEnterWorld(client, &client->lastUsercmd);
        else
        {
            // If we don't reset client->lastUsercmd and are restarting during map load,
            // the client will hang because we'll use the last Usercmd from the previous map,
            // which is wrong obviously.
            SV_ClientEnterWorld(client, NULL);
        }
    }

    // run another frame to allow things to look at all the players
    VM_Call (gvm, GAME_RUN_FRAME, sv.time);
    sv.time += 100;
    svs.time += 100;

    Com_Printf ("-----------------------------------\n");
}

//===============================================================

/*
==================
SV_Kick_f

Kick a user off of the server
==================
*/
static void SV_Kick_f( void ) {
    client_t    *cl;
    int         i;

    // make sure server is running
    if ( !com_sv_running->integer ) {
        Com_Printf( "Server is not running.\n" );
        return;
    }

    if ( Cmd_Argc() != 2 ) {
        Com_Printf ("Usage: kick <player name>\nkick all = kick everyone\nkick allbots = kick all bots\n");
        return;
    }

    cl = SV_GetPlayerByHandle();
    if ( !cl ) {
        if ( !Q_stricmp(Cmd_Argv(1), "all") ) {
            for ( i=0, cl=svs.clients ; i < sv_maxclients->integer ; i++,cl++ ) {
                if ( !cl->state ) {
                    continue;
                }
                if( cl->netchan.remoteAddress.type == NA_LOOPBACK ) {
                    continue;
                }
                SV_DropClient( cl, "was kicked" );
                cl->lastPacketTime = svs.time;  // in case there is a funny zombie
            }
        }
        else if ( !Q_stricmp(Cmd_Argv(1), "allbots") ) {
            for ( i=0, cl=svs.clients ; i < sv_maxclients->integer ; i++,cl++ ) {
                if ( !cl->state ) {
                    continue;
                }
                if( cl->netchan.remoteAddress.type != NA_BOT ) {
                    continue;
                }
                SV_DropClient( cl, "was kicked" );
                cl->lastPacketTime = svs.time;  // in case there is a funny zombie
            }
        }
        return;
    }
    if( cl->netchan.remoteAddress.type == NA_LOOPBACK ) {
        Com_Printf("Cannot kick host player\n");
        return;
    }

    SV_DropClient( cl, "was kicked" );
    cl->lastPacketTime = svs.time;  // in case there is a funny zombie
}

/*
==================
SV_KickBots_f

Kick all bots off of the server
==================
*/
static void SV_KickBots_f( void ) {
    client_t    *cl;
    int         i;

    // make sure server is running
    if( !com_sv_running->integer ) {
        Com_Printf("Server is not running.\n");
        return;
    }

    for( i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++ ) {
        if( !cl->state ) {
            continue;
        }

        if( cl->netchan.remoteAddress.type != NA_BOT ) {
            continue;
        }

        SV_DropClient( cl, "was kicked" );
        cl->lastPacketTime = svs.time; // in case there is a funny zombie
    }
}
/*
==================
SV_KickAll_f

Kick all users off of the server
==================
*/
static void SV_KickAll_f( void ) {
    client_t *cl;
    int i;

    // make sure server is running
    if( !com_sv_running->integer ) {
        Com_Printf( "Server is not running.\n" );
        return;
    }

    for( i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++ ) {
        if( !cl->state ) {
            continue;
        }

        if( cl->netchan.remoteAddress.type == NA_LOOPBACK ) {
            continue;
        }

        SV_DropClient( cl, "was kicked" );
        cl->lastPacketTime = svs.time; // in case there is a funny zombie
    }
}

/*
==================
SV_KickNum_f

Kick a user off of the server
==================
*/
static void SV_KickNum_f( void ) {
    client_t    *cl;

    // make sure server is running
    if ( !com_sv_running->integer ) {
        Com_Printf( "Server is not running.\n" );
        return;
    }

    if ( Cmd_Argc() != 2 ) {
        Com_Printf ("Usage: %s <client number>\n", Cmd_Argv(0));
        return;
    }

    cl = SV_GetPlayerByNum();
    if ( !cl ) {
        return;
    }
    if( cl->netchan.remoteAddress.type == NA_LOOPBACK ) {
        Com_Printf("Cannot kick host player\n");
        return;
    }

    SV_DropClient( cl, "was kicked" );
    cl->lastPacketTime = svs.time;  // in case there is a funny zombie
}

/*
==================
SV_RehashBans_f

Load saved bans from file.
==================
*/
static void SV_RehashBans_f(void)
{
    int index, filelen;
    fileHandle_t readfrom;
    char *textbuf, *curpos, *maskpos, *newlinepos, *endpos;
    char filepath[MAX_QPATH];

    // make sure server is running
    if ( !com_sv_running->integer ) {
        return;
    }

    serverBansCount = 0;

    if(!sv_banFile->string || !*sv_banFile->string)
        return;

    Com_sprintf(filepath, sizeof(filepath), "%s/%s", FS_GetCurrentGameDir(), sv_banFile->string);

    if((filelen = FS_SV_FOpenFileRead(filepath, &readfrom)) >= 0)
    {
        if(filelen < 2)
        {
            // Don't bother if file is too short.
            FS_FCloseFile(readfrom);
            return;
        }

        curpos = textbuf = Z_Malloc(filelen);

        filelen = FS_Read(textbuf, filelen, readfrom);
        FS_FCloseFile(readfrom);

        endpos = textbuf + filelen;

        for(index = 0; index < SERVER_MAXBANS && curpos + 2 < endpos; index++)
        {
            // find the end of the address string
            for(maskpos = curpos + 2; maskpos < endpos && *maskpos != ' '; maskpos++);

            if(maskpos + 1 >= endpos)
                break;

            *maskpos = '\0';
            maskpos++;

            // find the end of the subnet specifier
            for(newlinepos = maskpos; newlinepos < endpos && *newlinepos != '\n'; newlinepos++);

            if(newlinepos >= endpos)
                break;

            *newlinepos = '\0';

            if(NET_StringToAdr(curpos + 2, &serverBans[index].ip, NA_UNSPEC))
            {
                serverBans[index].isexception = (curpos[0] != '0');
                serverBans[index].subnet = atoi(maskpos);

                if(serverBans[index].ip.type == NA_IP &&
                   (serverBans[index].subnet < 1 || serverBans[index].subnet > 32))
                {
                    serverBans[index].subnet = 32;
                }
                else if(serverBans[index].ip.type == NA_IP6 &&
                    (serverBans[index].subnet < 1 || serverBans[index].subnet > 128))
                {
                    serverBans[index].subnet = 128;
                }
            }

            curpos = newlinepos + 1;
        }

        serverBansCount = index;

        Z_Free(textbuf);
    }
}

/*
==================
SV_WriteBans

Save bans to file.
==================
*/
static void SV_WriteBans(void)
{
    int index;
    fileHandle_t writeto;
    char filepath[MAX_QPATH];

    if(!sv_banFile->string || !*sv_banFile->string)
        return;

    Com_sprintf(filepath, sizeof(filepath), "%s/%s", FS_GetCurrentGameDir(), sv_banFile->string);

    if((writeto = FS_SV_FOpenFileWrite(filepath)))
    {
        char writebuf[128];
        serverBan_t *curban;

        for(index = 0; index < serverBansCount; index++)
        {
            curban = &serverBans[index];

            Com_sprintf(writebuf, sizeof(writebuf), "%d %s %d\n",
                    curban->isexception, NET_AdrToString(curban->ip), curban->subnet);
            FS_Write(writebuf, strlen(writebuf), writeto);
        }

        FS_FCloseFile(writeto);
    }
}

/*
==================
SV_DelBanEntryFromList

Remove a ban or an exception from the list.
==================
*/

static qboolean SV_DelBanEntryFromList(int index)
{
    if(index == serverBansCount - 1)
        serverBansCount--;
    else if(index < ARRAY_LEN(serverBans) - 1)
    {
        memmove(serverBans + index, serverBans + index + 1, (serverBansCount - index - 1) * sizeof(*serverBans));
        serverBansCount--;
    }
    else
        return qtrue;

    return qfalse;
}

/*
==================
SV_ParseCIDRNotation

Parse a CIDR notation type string and return a netadr_t and suffix by reference
==================
*/

static qboolean SV_ParseCIDRNotation(netadr_t *dest, int *mask, char *adrstr)
{
    char *suffix;

    suffix = strchr(adrstr, '/');
    if(suffix)
    {
        *suffix = '\0';
        suffix++;
    }

    if(!NET_StringToAdr(adrstr, dest, NA_UNSPEC))
        return qtrue;

    if(suffix)
    {
        *mask = atoi(suffix);

        if(dest->type == NA_IP)
        {
            if(*mask < 1 || *mask > 32)
                *mask = 32;
        }
        else
        {
            if(*mask < 1 || *mask > 128)
                *mask = 128;
        }
    }
    else if(dest->type == NA_IP)
        *mask = 32;
    else
        *mask = 128;

    return qfalse;
}

/*
==================
SV_AddBanToList

Ban a user from being able to play on this server based on his ip address.
==================
*/

static void SV_AddBanToList(qboolean isexception)
{
    char *banstring;
    char addy2[NET_ADDRSTRMAXLEN];
    netadr_t ip;
    int index, argc, mask;
    serverBan_t *curban;

    // make sure server is running
    if ( !com_sv_running->integer ) {
        Com_Printf( "Server is not running.\n" );
        return;
    }

    argc = Cmd_Argc();

    if(argc < 2 || argc > 3)
    {
        Com_Printf ("Usage: %s (ip[/subnet] | clientnum [subnet])\n", Cmd_Argv(0));
        return;
    }

    if(serverBansCount >= ARRAY_LEN(serverBans))
    {
        Com_Printf ("Error: Maximum number of bans/exceptions exceeded.\n");
        return;
    }

    banstring = Cmd_Argv(1);

    if(strchr(banstring, '.') || strchr(banstring, ':'))
    {
        // This is an ip address, not a client num.

        if(SV_ParseCIDRNotation(&ip, &mask, banstring))
        {
            Com_Printf("Error: Invalid address %s\n", banstring);
            return;
        }
    }
    else
    {
        client_t *cl;

        // client num.

        cl = SV_GetPlayerByNum();

        if(!cl)
        {
            Com_Printf("Error: Playernum %s does not exist.\n", Cmd_Argv(1));
            return;
        }

        ip = cl->netchan.remoteAddress;

        if(argc == 3)
        {
            mask = atoi(Cmd_Argv(2));

            if(ip.type == NA_IP)
            {
                if(mask < 1 || mask > 32)
                    mask = 32;
            }
            else
            {
                if(mask < 1 || mask > 128)
                    mask = 128;
            }
        }
        else
            mask = (ip.type == NA_IP6) ? 128 : 32;
    }

    if(ip.type != NA_IP && ip.type != NA_IP6)
    {
        Com_Printf("Error: Can ban players connected via the internet only.\n");
        return;
    }

    // first check whether a conflicting ban exists that would supersede the new one.
    for(index = 0; index < serverBansCount; index++)
    {
        curban = &serverBans[index];

        if(curban->subnet <= mask)
        {
            if((curban->isexception || !isexception) && NET_CompareBaseAdrMask(curban->ip, ip, curban->subnet))
            {
                Q_strncpyz(addy2, NET_AdrToString(ip), sizeof(addy2));

                Com_Printf("Error: %s %s/%d supersedes %s %s/%d\n", curban->isexception ? "Exception" : "Ban",
                       NET_AdrToString(curban->ip), curban->subnet,
                       isexception ? "exception" : "ban", addy2, mask);
                return;
            }
        }
        if(curban->subnet >= mask)
        {
            if(!curban->isexception && isexception && NET_CompareBaseAdrMask(curban->ip, ip, mask))
            {
                Q_strncpyz(addy2, NET_AdrToString(curban->ip), sizeof(addy2));

                Com_Printf("Error: %s %s/%d supersedes already existing %s %s/%d\n", isexception ? "Exception" : "Ban",
                       NET_AdrToString(ip), mask,
                       curban->isexception ? "exception" : "ban", addy2, curban->subnet);
                return;
            }
        }
    }

    // now delete bans that are superseded by the new one
    index = 0;
    while(index < serverBansCount)
    {
        curban = &serverBans[index];

        if(curban->subnet > mask && (!curban->isexception || isexception) && NET_CompareBaseAdrMask(curban->ip, ip, mask))
            SV_DelBanEntryFromList(index);
        else
            index++;
    }

    serverBans[serverBansCount].ip = ip;
    serverBans[serverBansCount].subnet = mask;
    serverBans[serverBansCount].isexception = isexception;

    serverBansCount++;

    SV_WriteBans();

    Com_Printf("Added %s: %s/%d\n", isexception ? "ban exception" : "ban",
           NET_AdrToString(ip), mask);
}

/*
==================
SV_DelBanFromList

Remove a ban or an exception from the list.
==================
*/

static void SV_DelBanFromList(qboolean isexception)
{
    int index, count = 0, todel, mask;
    netadr_t ip;
    char *banstring;

    // make sure server is running
    if ( !com_sv_running->integer ) {
        Com_Printf( "Server is not running.\n" );
        return;
    }

    if(Cmd_Argc() != 2)
    {
        Com_Printf ("Usage: %s (ip[/subnet] | num)\n", Cmd_Argv(0));
        return;
    }

    banstring = Cmd_Argv(1);

    if(strchr(banstring, '.') || strchr(banstring, ':'))
    {
        serverBan_t *curban;

        if(SV_ParseCIDRNotation(&ip, &mask, banstring))
        {
            Com_Printf("Error: Invalid address %s\n", banstring);
            return;
        }

        index = 0;

        while(index < serverBansCount)
        {
            curban = &serverBans[index];

            if(curban->isexception == isexception       &&
               curban->subnet >= mask           &&
               NET_CompareBaseAdrMask(curban->ip, ip, mask))
            {
                Com_Printf("Deleting %s %s/%d\n",
                       isexception ? "exception" : "ban",
                       NET_AdrToString(curban->ip), curban->subnet);

                SV_DelBanEntryFromList(index);
            }
            else
                index++;
        }
    }
    else
    {
        todel = atoi(Cmd_Argv(1));

        if(todel < 1 || todel > serverBansCount)
        {
            Com_Printf("Error: Invalid ban number given\n");
            return;
        }

        for(index = 0; index < serverBansCount; index++)
        {
            if(serverBans[index].isexception == isexception)
            {
                count++;

                if(count == todel)
                {
                    Com_Printf("Deleting %s %s/%d\n",
                       isexception ? "exception" : "ban",
                       NET_AdrToString(serverBans[index].ip), serverBans[index].subnet);

                    SV_DelBanEntryFromList(index);

                    break;
                }
            }
        }
    }

    SV_WriteBans();
}


/*
==================
SV_ListBans_f

List all bans and exceptions on console
==================
*/

static void SV_ListBans_f(void)
{
    int index, count;
    serverBan_t *ban;

    // make sure server is running
    if ( !com_sv_running->integer ) {
        Com_Printf( "Server is not running.\n" );
        return;
    }

    // List all bans
    for(index = count = 0; index < serverBansCount; index++)
    {
        ban = &serverBans[index];
        if(!ban->isexception)
        {
            count++;

            Com_Printf("Ban #%d: %s/%d\n", count,
                    NET_AdrToString(ban->ip), ban->subnet);
        }
    }
    // List all exceptions
    for(index = count = 0; index < serverBansCount; index++)
    {
        ban = &serverBans[index];
        if(ban->isexception)
        {
            count++;

            Com_Printf("Except #%d: %s/%d\n", count,
                    NET_AdrToString(ban->ip), ban->subnet);
        }
    }
}

/*
==================
SV_FlushBans_f

Delete all bans and exceptions.
==================
*/

static void SV_FlushBans_f(void)
{
    // make sure server is running
    if ( !com_sv_running->integer ) {
        Com_Printf( "Server is not running.\n" );
        return;
    }

    serverBansCount = 0;

    // empty the ban file.
    SV_WriteBans();

    Com_Printf("All bans and exceptions have been deleted.\n");
}

static void SV_BanAddr_f(void)
{
    SV_AddBanToList(qfalse);
}

static void SV_ExceptAddr_f(void)
{
    SV_AddBanToList(qtrue);
}

static void SV_BanDel_f(void)
{
    SV_DelBanFromList(qfalse);
}

static void SV_ExceptDel_f(void)
{
    SV_DelBanFromList(qtrue);
}

/*
** SV_Strlen -- skips color escape codes
*/
static int SV_Strlen( const char *str ) {
    const char *s = str;
    int count = 0;

    while ( *s ) {
        if ( Q_IsColorString( s ) ) {
            s += 2;
        } else {
            count++;
            s++;
        }
    }

    return count;
}

/*
================
SV_Status_f
================
*/
static void SV_Status_f( void ) {
    int         i, j, l;
    client_t    *cl;
    playerState_t   *ps;
    const char      *s;
    int         ping;

    // make sure server is running
    if ( !com_sv_running->integer ) {
        Com_Printf( "Server is not running.\n" );
        return;
    }

    Com_Printf ("map: %s\n", sv_mapname->string );

    Com_Printf ("cl score ping name            address                                 rate \n");
    Com_Printf ("-- ----- ---- --------------- --------------------------------------- -----\n");
    for (i=0,cl=svs.clients ; i < sv_maxclients->integer ; i++,cl++)
    {
        if (!cl->state)
            continue;
        Com_Printf ("%2i ", i);
        ps = SV_GameClientNum( i );
        Com_Printf ("%5i ", ps->persistant[PERS_SCORE]);

        if (cl->state == CS_CONNECTED)
            Com_Printf ("CON ");
        else if (cl->state == CS_ZOMBIE)
            Com_Printf ("ZMB ");
        else
        {
            ping = cl->ping < 9999 ? cl->ping : 9999;
            Com_Printf ("%4i ", ping);
        }

        Com_Printf ("%s", cl->name);

        l = 16 - SV_Strlen(cl->name);
        j = 0;

        do
        {
            Com_Printf (" ");
            j++;
        } while(j < l);


        // TTimo adding a ^7 to reset the color
        s = NET_AdrToString( cl->netchan.remoteAddress );
        Com_Printf ("^7%s", s);
        l = 39 - strlen(s);
        j = 0;

        do
        {
            Com_Printf(" ");
            j++;
        } while(j < l);

        Com_Printf (" %5i", cl->rate);

        Com_Printf ("\n");
    }
    Com_Printf ("\n");
}

/*
==================
SV_ConSay_f
==================
*/
static void SV_ConSay_f(void) {
    char    *p;
    char    text[1024];

    // make sure server is running
    if ( !com_sv_running->integer ) {
        Com_Printf( "Server is not running.\n" );
        return;
    }

    if ( Cmd_Argc () < 2 ) {
        return;
    }

    strcpy (text, "console: ");
    p = Cmd_Args();

    if ( *p == '"' ) {
        p++;
        p[strlen(p)-1] = 0;
    }

    strcat(text, p);

    Com_Printf("%s\n", text);
    SV_SendServerCommand(NULL, "chat -1 \"%s\"", text);
}

/*
==================
SV_ConTell_f
==================
*/
static void SV_ConTell_f(void) {
    char    *p;
    char    text[1024];
    client_t    *cl;

    // make sure server is running
    if ( !com_sv_running->integer ) {
        Com_Printf( "Server is not running.\n" );
        return;
    }

    if ( Cmd_Argc() < 3 ) {
        Com_Printf ("Usage: contell <client number> <text>\n");
        return;
    }

    cl = SV_GetPlayerByNum();
    if ( !cl ) {
        return;
    }

    strcpy (text, "console_tell: ");
    p = Cmd_ArgsFrom(2);

    if ( *p == '"' ) {
        p++;
        p[strlen(p)-1] = 0;
    }

    strcat(text, p);

    Com_Printf("%s\n", text);
    SV_SendServerCommand(cl, "chat -1 \"%s\"", text);
}


/*
==================
SV_ConSayto_f
==================
*/
static void SV_ConSayto_f(void) {
    char        *p;
    char        text[1024];
    client_t    *cl;
    char        *rawname;
    char        name[MAX_NAME_LENGTH];
    char        cleanName[MAX_NAME_LENGTH];
    client_t    *saytocl;
    int         i;

    // make sure server is running
    if ( !com_sv_running->integer ) {
        Com_Printf( "Server is not running.\n" );
        return;
    }

    if ( Cmd_Argc() < 3 ) {
        Com_Printf ("Usage: consayto <player name> <text>\n");
        return;
    }

    rawname = Cmd_Argv(1);

    //allowing special characters in the console
    //with hex strings for player names
    Com_FieldStringToPlayerName( name, MAX_NAME_LENGTH, rawname );

    saytocl = NULL;
    for ( i=0, cl=svs.clients ; i < sv_maxclients->integer ; i++,cl++ ) {
        if ( !cl->state ) {
            continue;
        }
        Q_strncpyz( cleanName, cl->name, sizeof(cleanName) );
        Q_CleanStr( cleanName );

        if ( !Q_stricmp( cleanName, name ) ) {
            saytocl = cl;
            break;
        }
    }
    if( !saytocl )
    {
        Com_Printf ("No such player name: %s.\n", name);
        return;
    }

    strcpy (text, "console_sayto: ");
    p = Cmd_ArgsFrom(2);

    if ( *p == '"' ) {
        p++;
        p[strlen(p)-1] = 0;
    }

    strcat(text, p);

    Com_Printf("%s\n", text);
    SV_SendServerCommand(saytocl, "chat -1 \"%s\"", text);
}


/*
==================
SV_Heartbeat_f

Also called by SV_DropClient, SV_DirectConnect, and SV_SpawnServer
==================
*/
void SV_Heartbeat_f( void ) {
    svs.nextHeartbeatTime = -9999999;
}


/*
===========
SV_Serverinfo_f

Examine the serverinfo string
===========
*/
static void SV_Serverinfo_f( void ) {
    // make sure server is running
    if ( !com_sv_running->integer ) {
        Com_Printf( "Server is not running.\n" );
        return;
    }

    Com_Printf ("Server info settings:\n");
    Info_Print ( Cvar_InfoString( CVAR_SERVERINFO ) );
}


/*
===========
SV_Systeminfo_f

Examine the systeminfo string
===========
*/
static void SV_Systeminfo_f( void ) {
    // make sure server is running
    if ( !com_sv_running->integer ) {
        Com_Printf( "Server is not running.\n" );
        return;
    }

    Com_Printf ("System info settings:\n");
    Info_Print ( Cvar_InfoString_Big( CVAR_SYSTEMINFO ) );
}


/*
===========
SV_DumpUser_f

Examine all a users info strings
===========
*/
static void SV_DumpUser_f( void ) {
    client_t    *cl;

    // make sure server is running
    if ( !com_sv_running->integer ) {
        Com_Printf( "Server is not running.\n" );
        return;
    }

    if ( Cmd_Argc() != 2 ) {
        Com_Printf ("Usage: dumpuser <userid>\n");
        return;
    }

    cl = SV_GetPlayerByHandle();
    if ( !cl ) {
        return;
    }

    Com_Printf( "userinfo\n" );
    Com_Printf( "--------\n" );
    Info_Print( cl->userinfo );
}


/*
=================
SV_KillServer
=================
*/
static void SV_KillServer_f( void ) {
    SV_Shutdown( "killserver" );
}

//===========================================================

/*
==================
SV_CompleteMapName
==================
*/
static void SV_CompleteMapName( char *args, int argNum ) {
    if( argNum == 2 ) {
        Field_CompleteFilename( "maps", "bsp", qtrue, qfalse );
    }
}

/*
==================
SV_CompletePlayerName
==================
*/
static void SV_CompletePlayerName( char *args, int argNum ) {
    if( argNum == 2 ) {
        char        names[MAX_CLIENTS][MAX_NAME_LENGTH];
        const char  *namesPtr[MAX_CLIENTS];
        client_t    *cl;
        int         i;
        int         nameCount;
        int         clientCount;

        nameCount = 0;
        clientCount = sv_maxclients->integer;

        for ( i=0, cl=svs.clients ; i < clientCount; i++,cl++ ) {
            if ( !cl->state ) {
                continue;
            }
            if( i >= MAX_CLIENTS ) {
                break;
            }
            Q_strncpyz( names[nameCount], cl->name, sizeof(names[nameCount]) );
            Q_CleanStr( names[nameCount] );

            namesPtr[nameCount] = names[nameCount];

            nameCount++;
        }
        qsort( (void*)namesPtr, nameCount, sizeof( namesPtr[0] ), Com_strCompare );

        Field_CompletePlayerName( namesPtr, nameCount );
    }
}

/*
==================
SV_AddOperatorCommands
==================
*/
void SV_AddOperatorCommands( void ) {
    static qboolean initialized;

    if ( initialized ) {
        return;
    }
    initialized = qtrue;

    Cmd_AddCommand ("heartbeat", SV_Heartbeat_f);
    Cmd_AddCommand ("kick", SV_Kick_f);
    Cmd_AddCommand ("kickbots", SV_KickBots_f);
    Cmd_AddCommand ("kickall", SV_KickAll_f);
    Cmd_AddCommand ("kicknum", SV_KickNum_f);
    Cmd_AddCommand ("clientkick", SV_KickNum_f); // Legacy command
    Cmd_AddCommand ("status", SV_Status_f);
    Cmd_AddCommand ("serverinfo", SV_Serverinfo_f);
    Cmd_AddCommand ("systeminfo", SV_Systeminfo_f);
    Cmd_AddCommand ("dumpuser", SV_DumpUser_f);
    Cmd_AddCommand ("map_restart", SV_MapRestart_f);
    Cmd_AddCommand ("sectorlist", SV_SectorList_f);
    Cmd_AddCommand ("map", SV_Map_f);
    Cmd_SetCommandCompletionFunc( "map", SV_CompleteMapName );
#ifndef PRE_RELEASE_DEMO
    Cmd_AddCommand ("devmap", SV_Map_f);
    Cmd_SetCommandCompletionFunc( "devmap", SV_CompleteMapName );
    Cmd_AddCommand ("spmap", SV_Map_f);
    Cmd_SetCommandCompletionFunc( "spmap", SV_CompleteMapName );
    Cmd_AddCommand ("spdevmap", SV_Map_f);
    Cmd_SetCommandCompletionFunc( "spdevmap", SV_CompleteMapName );
    Cmd_AddCommand("altmap", SV_Map_f);
    Cmd_SetCommandCompletionFunc("altmap", SV_CompleteMapName);
#endif
    Cmd_AddCommand ("mapcycle", SV_Mapcycle_f);
    Cmd_AddCommand ("killserver", SV_KillServer_f);
    if( com_dedicated->integer ) {
        Cmd_AddCommand ("consay", SV_ConSay_f);
        Cmd_AddCommand ("contell", SV_ConTell_f);
        Cmd_AddCommand ("consayto", SV_ConSayto_f);
        Cmd_SetCommandCompletionFunc( "consayto", SV_CompletePlayerName );
    }

    Cmd_AddCommand("rehashbans", SV_RehashBans_f);
    Cmd_AddCommand("listbans", SV_ListBans_f);
    Cmd_AddCommand("banaddr", SV_BanAddr_f);
    Cmd_AddCommand("exceptaddr", SV_ExceptAddr_f);
    Cmd_AddCommand("bandel", SV_BanDel_f);
    Cmd_AddCommand("exceptdel", SV_ExceptDel_f);
    Cmd_AddCommand("flushbans", SV_FlushBans_f);
}

/*
==================
SV_RemoveOperatorCommands
==================
*/
void SV_RemoveOperatorCommands( void ) {
#if 0
    // removing these won't let the server start again
    Cmd_RemoveCommand ("heartbeat");
    Cmd_RemoveCommand ("kick");
    Cmd_RemoveCommand ("kicknum");
    Cmd_RemoveCommand ("clientkick");
    Cmd_RemoveCommand ("kickall");
    Cmd_RemoveCommand ("kickbots");
    Cmd_RemoveCommand ("banUser");
    Cmd_RemoveCommand ("banClient");
    Cmd_RemoveCommand ("status");
    Cmd_RemoveCommand ("serverinfo");
    Cmd_RemoveCommand ("systeminfo");
    Cmd_RemoveCommand ("dumpuser");
    Cmd_RemoveCommand ("map_restart");
    Cmd_RemoveCommand ("sectorlist");
    Cmd_RemoveCommand ("say");
#endif
}

