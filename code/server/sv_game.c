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
// sv_game.c -- interface to the game dll

#include "server.h"

#include "../botlib/botlib.h"

botlib_export_t *botlib_export;

// these functions must be used instead of pointer arithmetic, because
// the game allocates gentities with private information after the server shared part
int SV_NumForGentity( sharedEntity_t *ent ) {
    int     num;

    num = ( (byte *)ent - (byte *)sv.gentities ) / sv.gentitySize;

    return num;
}

sharedEntity_t *SV_GentityNum( int num ) {
    sharedEntity_t *ent;

    ent = (sharedEntity_t *)((byte *)sv.gentities + sv.gentitySize*(num));

    return ent;
}

playerState_t *SV_GameClientNum( int num ) {
    playerState_t   *ps;

    ps = (playerState_t *)((byte *)sv.gameClients + sv.gameClientSize*(num));

    return ps;
}

svEntity_t  *SV_SvEntityForGentity( sharedEntity_t *gEnt ) {
    if ( !gEnt || gEnt->s.number < 0 || gEnt->s.number >= MAX_GENTITIES ) {
        Com_Error( ERR_DROP, "SV_SvEntityForGentity: bad gEnt" );
    }
    return &sv.svEntities[ gEnt->s.number ];
}

sharedEntity_t *SV_GEntityForSvEntity( svEntity_t *svEnt ) {
    int     num;

    num = svEnt - sv.svEntities;
    return SV_GentityNum( num );
}

/*
===============
SV_GameSendServerCommand

Sends a command string to a client
===============
*/
void SV_GameSendServerCommand( int clientNum, const char *text ) {
    if ( clientNum == -1 ) {
        SV_SendServerCommand( NULL, "%s", text );
    } else {
        if ( clientNum < 0 || clientNum >= sv_maxclients->integer ) {
            return;
        }
        SV_SendServerCommand( svs.clients + clientNum, "%s", text );
    }
}


/*
===============
SV_GameDropClient

Disconnects the client with a message
===============
*/
void SV_GameDropClient( int clientNum, const char *reason ) {
    if ( clientNum < 0 || clientNum >= sv_maxclients->integer ) {
        return;
    }
    SV_DropClient( svs.clients + clientNum, reason );
}


/*
=================
SV_SetBrushModel

sets mins and maxs for inline bmodels
=================
*/
void SV_SetBrushModel( sharedEntity_t *ent, const char *name ) {
    vec3_t  mins, maxs;

    if (!name) {
        Com_Error( ERR_DROP, "SV_SetBrushModel: NULL" );
    }

    if (!Q_stricmp(name, "none")) {
        return;
    }

    // Regular map model.
    if(name[0] == '*'){
        clipHandle_t    h;

        ent->s.modelindex = atoi(name + 1);

        if(sv.subBSPIndex){
            ent->s.modelindex += sv.subBSPModelIndex;
        }

        h = CM_InlineModel( ent->s.modelindex );
        CM_ModelBounds( h, mins, maxs );
        VectorCopy (mins, ent->r.mins);
        VectorCopy (maxs, ent->r.maxs);
        ent->r.bmodel = qtrue;

        ent->r.contents = -1;       // we don't know exactly what is in the brushes
    }
    // Sub-BSP.
    else if (name[0] == '#' && name[1]){
        ent->s.modelindex = CM_LoadSubBSP(va("maps/%s.bsp", name + 1));

        CM_ModelBounds( ent->s.modelindex, mins, maxs );
        VectorCopy (mins, ent->r.mins);
        VectorCopy (maxs, ent->r.maxs);
        ent->r.bmodel = qtrue;

        ent->r.contents = -1;       // we don't know exactly what is in the brushes
    }else{
        Com_Error( ERR_DROP, "SV_SetBrushModel: %s isn't a brush model", name );
    }
}



/*
=================
SV_inPVS

Also checks portalareas so that doors block sight
=================
*/
qboolean SV_inPVS (const vec3_t p1, const vec3_t p2)
{
    int     leafnum;
    int     cluster;
    int     area1, area2;
    byte    *mask;

    leafnum = CM_PointLeafnum (p1);
    cluster = CM_LeafCluster (leafnum);
    area1 = CM_LeafArea (leafnum);
    mask = CM_ClusterPVS (cluster);

    leafnum = CM_PointLeafnum (p2);
    cluster = CM_LeafCluster (leafnum);
    area2 = CM_LeafArea (leafnum);
    if ( mask && (!(mask[cluster>>3] & (1<<(cluster&7)) ) ) )
        return qfalse;
    if (!CM_AreasConnected (area1, area2))
        return qfalse;      // a door blocks sight
    return qtrue;
}


/*
=================
SV_inPVSIgnorePortals

Does NOT check portalareas
=================
*/
qboolean SV_inPVSIgnorePortals( const vec3_t p1, const vec3_t p2)
{
    int     leafnum;
    int     cluster;
    byte    *mask;

    leafnum = CM_PointLeafnum (p1);
    cluster = CM_LeafCluster (leafnum);
    mask = CM_ClusterPVS (cluster);

    leafnum = CM_PointLeafnum (p2);
    cluster = CM_LeafCluster (leafnum);

    if ( mask && (!(mask[cluster>>3] & (1<<(cluster&7)) ) ) )
        return qfalse;

    return qtrue;
}


/*
========================
SV_AdjustAreaPortalState
========================
*/
void SV_AdjustAreaPortalState( sharedEntity_t *ent, qboolean open ) {
    svEntity_t  *svEnt;

    svEnt = SV_SvEntityForGentity( ent );
    if ( svEnt->areanum2 == -1 ) {
        return;
    }
    CM_AdjustAreaPortalState( svEnt->areanum, svEnt->areanum2, open );
}


/*
==================
SV_EntityContact
==================
*/
qboolean    SV_EntityContact( vec3_t mins, vec3_t maxs, const sharedEntity_t *gEnt, int capsule ) {
    const float *origin, *angles;
    clipHandle_t    ch;
    trace_t         trace;

    // check for exact collision
    origin = gEnt->r.currentOrigin;
    angles = gEnt->r.currentAngles;

    ch = SV_ClipHandleForEntity( gEnt );
    CM_TransformedBoxTrace ( &trace, vec3_origin, vec3_origin, mins, maxs,
        ch, -1, origin, angles, capsule );

    return trace.startsolid;
}


/*
===============
SV_GetServerinfo

===============
*/
void SV_GetServerinfo( char *buffer, int bufferSize ) {
    if ( bufferSize < 1 ) {
        Com_Error( ERR_DROP, "SV_GetServerinfo: bufferSize == %i", bufferSize );
    }
    Q_strncpyz( buffer, Cvar_InfoString( CVAR_SERVERINFO ), bufferSize );
}

/*
===============
SV_LocateGameData

===============
*/
void SV_LocateGameData( sharedEntity_t *gEnts, int numGEntities, int sizeofGEntity_t,
                       playerState_t *clients, int sizeofGameClient ) {
    sv.gentities = gEnts;
    sv.gentitySize = sizeofGEntity_t;
    sv.num_entities = numGEntities;

    sv.gameClients = clients;
    sv.gameClientSize = sizeofGameClient;
}


/*
===============
SV_GetUsercmd

===============
*/
void SV_GetUsercmd( int clientNum, usercmd_t *cmd ) {
    if ( clientNum < 0 || clientNum >= sv_maxclients->integer ) {
        Com_Error( ERR_DROP, "SV_GetUsercmd: bad clientNum:%i", clientNum );
    }
    *cmd = svs.clients[clientNum].lastUsercmd;
}

/*
==================
SV_SetActiveSubBSP

Set server sub-BSP based on model
index, or resets the active
sub-BSP if requested (-1) or
out of range.
==================
*/

static void SV_SetActiveSubBSP(int modelIndex)
{
    // Set sub-BSP index based on the provided model index.
    sv.subBSPIndex      = CM_FindSubBSP(modelIndex);

    // Save the passed model index.
    sv.subBSPModelIndex = modelIndex;

    // Set the entity parse point to the beginning of the active BSP.
    sv.subBSPParsePoint = CM_EntityString(sv.subBSPIndex);
    if (modelIndex > 0) {
        sv.inSubBSP = qtrue;
    } else {
        sv.inSubBSP = qfalse;
    }
}

/*
==================
SV_ValidateMapName

Validates whether the given mapname
is a known map to the server.
Returns 0 on no matches,
Returns 1 on EXACT match,
Returns 2 on multiple matches.
==================
*/
static int SV_ValidateMapName(const char* mapName, char* output, int outputSize) {

    int nfiles;
    char** bspNames = FS_ListFilteredFiles("maps", "bsp", va("maps/%s", mapName), &nfiles, qtrue); // Filter accepts maps/%s as a filter,
    char tmpFilename[MAX_STRING_CHARS];
    qboolean exactMatch = qfalse;

    // Clear the output buffer first.
    Com_Memset(output, 0, outputSize);

    // We need to figure out whether the list contains an exact match as well.
    // If not, shoot back the names we got.

    for (int i = 0; i < nfiles; i++) {
        FS_ConvertPath(bspNames[i]);
        
        const char* slash = strchr(bspNames[i], '/');
        if (slash && !Q_strncmp(bspNames[i], "maps/", 5)) {
            Q_strncpyz(tmpFilename, slash + 1, sizeof(tmpFilename));
        }
        else {
            continue; // We're querying for maps but it is not a map? Shouldn't happen.
        }

        COM_StripExtension(tmpFilename, tmpFilename, sizeof(tmpFilename));

        if (!Q_stricmp(mapName, tmpFilename)) {
            exactMatch = qtrue;
        }

        int spaceRemaining = outputSize - strlen(output) - 1; // Account for null terminator
        int nameLength = strlen(tmpFilename);

        if (nameLength + 1 <= spaceRemaining) {
            Q_strcat(output, outputSize, tmpFilename);
            Q_strcat(output, outputSize, "\n");
        }

    }

    FS_FreeFileList(bspNames);

    if (exactMatch == 1) {
        return 1; // Exact match.
    }
    else if (nfiles > 0) {
        return 2;
    }
    else {
        return 0;
    }
}

//==============================================

static int  FloatAsInt( float f ) {
    floatint_t fi;
    fi.f = f;
    return fi.i;
}

static qboolean qvmPointerMarshallingInitialized = qfalse;

/*
====================
SV_GameSystemCalls

The module is making a system call
====================
*/
intptr_t SV_GameSystemCalls(qboolean runningQVM, intptr_t *args ) {
    // TEMPORARY
#if !defined(_WIN64) && !defined(__x86_64__) && !defined(__amd64__) && !defined(__LP64__)


        switch (args[0]) {
        case LEGACY_G_PRINT:
            Com_Printf("%s", (const char*)VMA(1));
            return 0;
        case LEGACY_G_ERROR:
            Com_Error(ERR_DROP, "%s", (const char*)VMA(1));
            return 0;
        case LEGACY_G_MILLISECONDS:
            return Sys_Milliseconds();
        case LEGACY_G_CVAR_REGISTER:
            Cvar_Register(VMA(1), VMA(2), VMA(3), args[4], VMF(5), VMF(6));
            return 0;
        case LEGACY_G_CVAR_UPDATE:
            Cvar_Update(VMA(1));
            return 0;
        case LEGACY_G_CVAR_SET:
            Cvar_SetSafe((const char*)VMA(1), (const char*)VMA(2));
            return 0;
        case LEGACY_G_CVAR_VARIABLE_INTEGER_VALUE:
            return Cvar_VariableIntegerValue((const char*)VMA(1));
        case LEGACY_G_CVAR_VARIABLE_STRING_BUFFER:
            Cvar_VariableStringBuffer(VMA(1), VMA(2), args[3]);
            return 0;
        case LEGACY_G_ARGC:
            return Cmd_Argc();
        case LEGACY_G_ARGV:
            Cmd_ArgvBuffer(args[1], VMA(2), args[3]);
            return 0;
        case LEGACY_G_FS_FOPEN_FILE:
        {
            fsMode_t mode = 0;
			legacyFsMode_t legacyMode = (legacyFsMode_t)args[3];

            switch (legacyMode) {

            case LEGACY_FS_READ:
                mode = FS_READ;
                break;
            case LEGACY_FS_WRITE:
                mode = FS_WRITE;
                break;
            case LEGACY_FS_APPEND:
                mode = FS_APPEND;
                break;
            case LEGACY_FS_APPEND_SYNC:
                mode = FS_APPEND_SYNC;
                break;
            case LEGACY_FS_READ_TEXT:
                mode = FS_READ;
                break;
            case LEGACY_FS_WRITE_TEXT:
                mode = FS_WRITE;
                break;
            case LEGACY_FS_APPEND_TEXT:
                mode = FS_APPEND;
                break;
            case LEGACY_FS_APPEND_SYNC_TEXT:
                mode = FS_APPEND_SYNC;
                break;
            default:
                Com_Error(ERR_DROP, "SV_GameSystemCalls: LEGACY_G_FS_FOPEN_FILE: bad mode %d", legacyMode);

                
            }
            if (mode != legacyMode) {
                Com_Printf("Translated lmode %d to mode %d\r\n", legacyMode, mode);
            }
            return FS_FOpenFileByMode(VMA(1), VMA(2), mode);
        }
            
        case LEGACY_G_FS_READ:
            FS_Read(VMA(1), args[2], args[3]);
            return 0;
        case LEGACY_G_FS_WRITE:
            FS_Write(VMA(1), args[2], args[3]);
            return 0;
        case LEGACY_G_FS_FCLOSE_FILE:
            FS_FCloseFile(args[1]);
            return 0;
        case LEGACY_G_SEND_CONSOLE_COMMAND:
            Cbuf_ExecuteText(args[1], VMA(2));
            return 0;
        case LEGACY_G_LOCATE_GAME_DATA:
            SV_LocateGameData(VMA(1), args[2], args[3], VMA(4), args[5]);
            return 0;
        case LEGACY_G_GET_WORLD_BOUNDS:
            CM_ModelBounds(0, VMA(1), VMA(2));
            return 0;
        case LEGACY_G_RMG_INIT:
			// SoF2Plus currently lacks RMG support.
            return 0;
        case LEGACY_G_DROP_CLIENT:
            SV_GameDropClient(args[1], VMA(2));
            return 0;
        case LEGACY_G_SEND_SERVER_COMMAND:
            SV_GameSendServerCommand(args[1], VMA(2));
            return 0;
        case LEGACY_G_SET_CONFIGSTRING:
            SV_SetConfigstring(args[1], VMA(2));
            return 0;
        case LEGACY_G_GET_CONFIGSTRING:
            SV_GetConfigstring(args[1], VMA(2), args[3]);
            return 0;
        case LEGACY_G_GET_USERINFO:
            SV_GetUserinfo(args[1], VMA(2), args[3]);
            return 0;
        case LEGACY_G_SET_USERINFO:
            SV_SetUserinfo(args[1], VMA(2));
            return 0;
        case LEGACY_G_GET_SERVERINFO:
            SV_GetServerinfo(VMA(1), args[2]);
            return 0;
        case LEGACY_G_SET_BRUSH_MODEL:
            SV_SetBrushModel(VMA(1), VMA(2));
            return 0;
        case LEGACY_G_SET_ACTIVE_SUBBSP:
            SV_SetActiveSubBSP(args[1]);
            return 0;
        case LEGACY_G_TRACE:
            SV_Trace(VMA(1), VMA(2), VMA(3), VMA(4), VMA(5), args[6], args[7], /*int capsule*/ qfalse);
            return 0;
        case LEGACY_G_POINT_CONTENTS:
            return SV_PointContents(VMA(1), args[2]);
        case LEGACY_G_IN_PVS:
            return SV_inPVS(VMA(1), VMA(2));
        case LEGACY_G_IN_PVS_IGNORE_PORTALS:
            return SV_inPVSIgnorePortals(VMA(1), VMA(2));
        case LEGACY_G_ADJUST_AREA_PORTAL_STATE:
            SV_AdjustAreaPortalState(VMA(1), args[2]);
            return 0;
        case LEGACY_G_AREAS_CONNECTED:
            return CM_AreasConnected(args[1], args[2]);
        case LEGACY_G_LINKENTITY:
            SV_LinkEntity(VMA(1));
            return 0;
        case LEGACY_G_UNLINKENTITY:
            SV_UnlinkEntity(VMA(1));
            return 0;
        case LEGACY_G_ENTITIES_IN_BOX:
            return SV_AreaEntities(VMA(1), VMA(2), VMA(3), args[4]);
        case LEGACY_G_ENTITY_CONTACT:
            return SV_EntityContact(VMA(1), VMA(2), VMA(3), /*int capsule*/ qfalse);
        case LEGACY_G_BOT_ALLOCATE_CLIENT:
            return SV_BotAllocateClient();
        case LEGACY_G_BOT_FREE_CLIENT:
            SV_BotFreeClient(args[1]);
            return 0;
        case LEGACY_G_GET_USERCMD:
            SV_GetUsercmd(args[1], VMA(2));
            return 0;
        case LEGACY_G_GET_ENTITY_TOKEN: // JANFIXME - inSubBSP is not covered here at the moment, needs mod support or another workaround.
        {
            const char* s;
            qboolean inSubBSP = sv.inSubBSP;

            if (inSubBSP) {
                s = COM_Parse(&sv.subBSPParsePoint);
                Q_strncpyz(VMA(1), s, args[2]);
                if (!sv.subBSPParsePoint && !s[0]) {
                    return qfalse;
                }
                else {
                    return qtrue;
                }
            }
            else {
                s = COM_Parse(&sv.entityParsePoint);
                Q_strncpyz(VMA(1), s, args[2]);
                if (!sv.entityParsePoint && !s[0]) {
                    return qfalse;
                }
                else {
                    return qtrue;
                }
            }
        }
        case LEGACY_G_FS_GETFILELIST:
            return FS_GetFileList(VMA(1), VMA(2), VMA(3), args[4]);
        case LEGACY_G_BOT_GET_MEMORY:
            return (intptr_t)Z_TagMalloc(TAG_BOTLIB, args[1]);
        case LEGACY_G_BOT_FREE_MEMORY:
			Z_Free((intptr_t*)args[1]);
            return 0;
        case LEGACY_G_DEBUG_POLYGON_CREATE:
            return BotImport_DebugPolygonCreate(args[1], args[2], VMA(3));
        case LEGACY_G_DEBUG_POLYGON_DELETE:
            BotImport_DebugPolygonDelete(args[1]);
            return 0;
        case LEGACY_G_REAL_TIME:
            return Com_RealTime(VMA(1));
        case LEGACY_G_SNAPVECTOR:
            Q_SnapVector(VMA(1));
            return 0;
        case LEGACY_G_TRACECAPSULE:
            SV_Trace(VMA(1), VMA(2), VMA(3), VMA(4), VMA(5), args[6], args[7], /*int capsule*/ qtrue);
            return 0;
        case LEGACY_G_ENTITY_CONTACTCAPSULE:
            return SV_EntityContact(VMA(1), VMA(2), VMA(3), /*int capsule*/ qtrue);
            return 0;
        case LEGACY_G_MEMSET:
            Com_Memset(VMA(1), args[2], args[3]);
            return 0;
        case LEGACY_G_MEMCPY:
            Com_Memcpy(VMA(1), VMA(2), args[3]);
            return 0;
        case LEGACY_G_STRNCPY:
            strncpy(VMA(1), VMA(2), args[3]);
            return args[1];
        case LEGACY_G_SIN:
            return FloatAsInt(sin(VMF(1)));
        case LEGACY_G_COS:
            return FloatAsInt(cos(VMF(1)));
        case LEGACY_G_ATAN2:
            return FloatAsInt(atan2(VMF(1), VMF(2)));
        case LEGACY_G_SQRT:
            return FloatAsInt(sqrt(VMF(1)));
        case LEGACY_G_ANGLEVECTORS:
            AngleVectors(VMA(1), VMA(2), VMA(3), VMA(4));
            return 0;
        case LEGACY_G_PERPENDICULARVECTOR:
            PerpendicularVector(VMA(1), VMA(2));
            return 0;
        case LEGACY_G_FLOOR:
            return FloatAsInt(floor(VMF(1)));
        case LEGACY_G_CEIL:
            return FloatAsInt(ceil(VMF(1)));
        case LEGACY_G_TESTPRINTINT:
            Com_DPrintf("Testprintint - not implemented");
            return 0;
        case LEGACY_G_TESTPRINTFLOAT:
            Com_DPrintf("Testprintfloat - not implemented");
            return 0;
        case LEGACY_G_ACOS:
            return FloatAsInt(acos(VMF(1)));
        case LEGACY_G_ASIN:
            return FloatAsInt(asin(VMF(1)));
        case LEGACY_G_MATRIXMULTIPLY:
            MatrixMultiply(VMA(1), VMA(2), VMA(3));
            return 0;


		// Botlib calls
        case LEGACY_BOTLIB_SETUP:
                return SV_BotLibSetup();
        case LEGACY_BOTLIB_SHUTDOWN:
                return SV_BotLibShutdown();
        case LEGACY_BOTLIB_LIBVAR_SET:
                return botlib_export->BotLibVarSet( VMA(1), VMA(2) );
        case LEGACY_BOTLIB_LIBVAR_GET:
                return botlib_export->BotLibVarGet( VMA(1), VMA(2), args[3] );
        case LEGACY_BOTLIB_PC_ADD_GLOBAL_DEFINE:
                return botlib_export->PC_AddGlobalDefine( VMA(1) );
        case LEGACY_BOTLIB_START_FRAME:
                return botlib_export->BotLibStartFrame( VMF(1) );
        case LEGACY_BOTLIB_LOAD_MAP:
                return botlib_export->BotLibLoadMap( VMA(1) );
        case LEGACY_BOTLIB_UPDATENTITY:
                return botlib_export->BotLibUpdateEntity( args[1], VMA(2) );
        case LEGACY_BOTLIB_TEST:
            return 0;
        case LEGACY_BOTLIB_GET_SNAPSHOT_ENTITY:
                return SV_BotGetSnapshotEntity( args[1], args[2] );
        case LEGACY_BOTLIB_GET_CONSOLE_MESSAGE:
            return 0;
        case LEGACY_BOTLIB_USER_COMMAND:
            return 0;
        case LEGACY_BOTLIB_AAS_ENABLE_ROUTING_AREA:
            return 0;
        case LEGACY_BOTLIB_AAS_BBOX_AREAS:
            return 0;
        case LEGACY_BOTLIB_AAS_AREA_INFO:
            return 0;
        case LEGACY_BOTLIB_AAS_ENTITY_INFO:
            return 0;
        case LEGACY_BOTLIB_AAS_INITIALIZED:
            return 0;
        case LEGACY_BOTLIB_AAS_PRESENCE_TYPE_BOUNDING_BOX:
            return 0;
        case LEGACY_BOTLIB_AAS_TIME:
            return 0;
        case LEGACY_BOTLIB_AAS_POINT_AREA_NUM:
            return 0;
        case LEGACY_BOTLIB_AAS_TRACE_AREAS:
            return 0;
        case LEGACY_BOTLIB_AAS_POINT_CONTENTS:
            return 0;
        case LEGACY_BOTLIB_AAS_NEXT_BSP_ENTITY:
            return 0;
        case LEGACY_BOTLIB_AAS_VALUE_FOR_BSP_EPAIR_KEY:
            return 0;
        case LEGACY_BOTLIB_AAS_VECTOR_FOR_BSP_EPAIR_KEY:
            return 0;
        case LEGACY_BOTLIB_AAS_FLOAT_FOR_BSP_EPAIR_KEY:
            return 0;
        case LEGACY_BOTLIB_AAS_INT_FOR_BSP_EPAIR_KEY:
            return 0;
        case LEGACY_BOTLIB_AAS_AREA_REACHABILITY:
            return 0;
        case LEGACY_BOTLIB_AAS_AREA_TRAVEL_TIME_TO_GOAL_AREA:
            return 0;
        case LEGACY_BOTLIB_AAS_SWIMMING:
            return 0;
        case LEGACY_BOTLIB_AAS_PREDICT_CLIENT_MOVEMENT:
            return 0;
        case LEGACY_BOTLIB_EA_SAY:
            return 0;
        case LEGACY_BOTLIB_EA_SAY_TEAM:
            return 0;
        case LEGACY_BOTLIB_EA_COMMAND:
            return 0;
        case LEGACY_BOTLIB_EA_ACTION:
            return 0;
        case LEGACY_BOTLIB_EA_GESTURE:
            return 0;
        case LEGACY_BOTLIB_EA_TALK:
            return 0;
        case LEGACY_BOTLIB_EA_ATTACK:
            return 0;
        case LEGACY_BOTLIB_EA_ALT_ATTACK:
            return 0;
        case LEGACY_BOTLIB_EA_FORCEPOWER:
            return 0;
        case LEGACY_BOTLIB_EA_USE:
            return 0;
        case LEGACY_BOTLIB_EA_RESPAWN:
            return 0;
        case LEGACY_BOTLIB_EA_CROUCH:
            return 0;
        case LEGACY_BOTLIB_EA_MOVE_UP:
            return 0;
        case LEGACY_BOTLIB_EA_MOVE_DOWN:
            return 0;
        case LEGACY_BOTLIB_EA_MOVE_FORWARD:
            return 0;
        case LEGACY_BOTLIB_EA_MOVE_BACK:
            return 0;
        case LEGACY_BOTLIB_EA_MOVE_LEFT:
            return 0;
        case LEGACY_BOTLIB_EA_MOVE_RIGHT:
            return 0;
        case LEGACY_BOTLIB_EA_SELECT_WEAPON:
            return 0;
        case LEGACY_BOTLIB_EA_JUMP:
            return 0;
        case LEGACY_BOTLIB_EA_DELAYED_JUMP:
            return 0;
        case LEGACY_BOTLIB_EA_MOVE:
            return 0;
        case LEGACY_BOTLIB_EA_VIEW:
            return 0;
        case LEGACY_BOTLIB_EA_END_REGULAR:
            return 0;
        case LEGACY_BOTLIB_EA_GET_INPUT:
            return 0;
        case LEGACY_BOTLIB_EA_RESET_INPUT:
            return 0;
        case LEGACY_BOTLIB_AI_LOAD_CHARACTER:
            return 0;
        case LEGACY_BOTLIB_AI_FREE_CHARACTER:
            return 0;
        case LEGACY_BOTLIB_AI_CHARACTERISTIC_FLOAT:
            return 0;
        case LEGACY_BOTLIB_AI_CHARACTERISTIC_BFLOAT:
            return 0;
        case LEGACY_BOTLIB_AI_CHARACTERISTIC_INTEGER:
            return 0;
        case LEGACY_BOTLIB_AI_CHARACTERISTIC_BINTEGER:
            return 0;
        case LEGACY_BOTLIB_AI_CHARACTERISTIC_STRING:
            return 0;
        case LEGACY_BOTLIB_AI_ALLOC_CHAT_STATE:
            return 0;
        case LEGACY_BOTLIB_AI_FREE_CHAT_STATE:
            return 0;
        case LEGACY_BOTLIB_AI_QUEUE_CONSOLE_MESSAGE:
            return 0;
        case LEGACY_BOTLIB_AI_REMOVE_CONSOLE_MESSAGE:
            return 0;
        case LEGACY_BOTLIB_AI_NEXT_CONSOLE_MESSAGE:
            return 0;
        case LEGACY_BOTLIB_AI_NUM_CONSOLE_MESSAGE:
            return 0;
        case LEGACY_BOTLIB_AI_INITIAL_CHAT:
            return 0;
        case LEGACY_BOTLIB_AI_REPLY_CHAT:
            return 0;
        case LEGACY_BOTLIB_AI_CHAT_LENGTH:
            return 0;
        case LEGACY_BOTLIB_AI_ENTER_CHAT:
            return 0;
        case LEGACY_BOTLIB_AI_STRING_CONTAINS:
            return 0;
        case LEGACY_BOTLIB_AI_FIND_MATCH:
            return 0;
        case LEGACY_BOTLIB_AI_MATCH_VARIABLE:
            return 0;
        case LEGACY_BOTLIB_AI_UNIFY_WHITE_SPACES:
            return 0;
        case LEGACY_BOTLIB_AI_REPLACE_SYNONYMS:
            return 0;
        case LEGACY_BOTLIB_AI_LOAD_CHAT_FILE:
            return 0;
        case LEGACY_BOTLIB_AI_SET_CHAT_GENDER:
            return 0;
        case LEGACY_BOTLIB_AI_SET_CHAT_NAME:
            return 0;
        case LEGACY_BOTLIB_AI_RESET_GOAL_STATE:
            return 0;
        case LEGACY_BOTLIB_AI_RESET_AVOID_GOALS:
            return 0;
        case LEGACY_BOTLIB_AI_PUSH_GOAL:
            return 0;
        case LEGACY_BOTLIB_AI_POP_GOAL:
            return 0;
        case LEGACY_BOTLIB_AI_EMPTY_GOAL_STACK:
            return 0;
        case LEGACY_BOTLIB_AI_DUMP_AVOID_GOALS:
            return 0;
        case LEGACY_BOTLIB_AI_DUMP_GOAL_STACK:
            return 0;
        case LEGACY_BOTLIB_AI_GOAL_NAME:
            return 0;
        case LEGACY_BOTLIB_AI_GET_TOP_GOAL:
            return 0;
        case LEGACY_BOTLIB_AI_GET_SECOND_GOAL:
            return 0;
        case LEGACY_BOTLIB_AI_CHOOSE_LTG_ITEM:
            return 0;
        case LEGACY_BOTLIB_AI_CHOOSE_NBG_ITEM:
            return 0;
        case LEGACY_BOTLIB_AI_TOUCHING_GOAL:
            return 0;
        case LEGACY_BOTLIB_AI_ITEM_GOAL_IN_VIS_BUT_NOT_VISIBLE:
            return 0;
        case LEGACY_BOTLIB_AI_GET_LEVEL_ITEM_GOAL:
            return 0;
        case LEGACY_BOTLIB_AI_AVOID_GOAL_TIME:
            return 0;
        case LEGACY_BOTLIB_AI_INIT_LEVEL_ITEMS:
            return 0;
        case LEGACY_BOTLIB_AI_UPDATE_ENTITY_ITEMS:
            return 0;
        case LEGACY_BOTLIB_AI_LOAD_ITEM_WEIGHTS:
            return 0;
        case LEGACY_BOTLIB_AI_FREE_ITEM_WEIGHTS:
            return 0;
        case LEGACY_BOTLIB_AI_SAVE_GOAL_FUZZY_LOGIC:
            return 0;
        case LEGACY_BOTLIB_AI_ALLOC_GOAL_STATE:
            return 0;
        case LEGACY_BOTLIB_AI_FREE_GOAL_STATE:
            return 0;
        case LEGACY_BOTLIB_AI_RESET_MOVE_STATE:
            return 0;
        case LEGACY_BOTLIB_AI_MOVE_TO_GOAL:
            return 0;
        case LEGACY_BOTLIB_AI_MOVE_IN_DIRECTION:
            return 0;
        case LEGACY_BOTLIB_AI_RESET_AVOID_REACH:
            return 0;
        case LEGACY_BOTLIB_AI_RESET_LAST_AVOID_REACH:
            return 0;
        case LEGACY_BOTLIB_AI_REACHABILITY_AREA:
            return 0;
        case LEGACY_BOTLIB_AI_MOVEMENT_VIEW_TARGET:
            return 0;
        case LEGACY_BOTLIB_AI_ALLOC_MOVE_STATE:
            return 0;
        case LEGACY_BOTLIB_AI_FREE_MOVE_STATE:
            return 0;
        case LEGACY_BOTLIB_AI_INIT_MOVE_STATE:
            return 0;
        case LEGACY_BOTLIB_AI_CHOOSE_BEST_FIGHT_WEAPON:
            return 0;
        case LEGACY_BOTLIB_AI_GET_WEAPON_INFO:
            return 0;
        case LEGACY_BOTLIB_AI_LOAD_WEAPON_WEIGHTS:
            return 0;
        case LEGACY_BOTLIB_AI_ALLOC_WEAPON_STATE:
            return 0;
        case LEGACY_BOTLIB_AI_FREE_WEAPON_STATE:
            return 0;
        case LEGACY_BOTLIB_AI_RESET_WEAPON_STATE:
            return 0;
        case LEGACY_BOTLIB_AI_GENETIC_PARENTS_AND_CHILD_SELECTION:
            return 0;
        case LEGACY_BOTLIB_AI_INTERBREED_GOAL_FUZZY_LOGIC:
            return 0;
        case LEGACY_BOTLIB_AI_MUTATE_GOAL_FUZZY_LOGIC:
            return 0;
        case LEGACY_BOTLIB_AI_GET_NEXT_CAMP_SPOT_GOAL:
            return 0;
        case LEGACY_BOTLIB_AI_GET_MAP_LOCATION_GOAL:
            return 0;
        case LEGACY_BOTLIB_AI_NUM_INITIAL_CHATS:
            return 0;
        case LEGACY_BOTLIB_AI_GET_CHAT_MESSAGE:
            return 0;
        case LEGACY_BOTLIB_AI_REMOVE_FROM_AVOID_GOALS:
            return 0;
        case LEGACY_BOTLIB_AI_PREDICT_VISIBLE_POSITION:
            return 0;
        case LEGACY_BOTLIB_AI_SET_AVOID_GOAL_TIME:
            return 0;
        case LEGACY_BOTLIB_AI_ADD_AVOID_SPOT:
            return 0;
        case LEGACY_BOTLIB_AAS_ALTERNATIVE_ROUTE_GOAL:
            return 0;
        case LEGACY_BOTLIB_AAS_PREDICT_ROUTE:
            return 0;
        case LEGACY_BOTLIB_AAS_POINT_REACHABILITY_AREA_INDEX:
                return botlib_export->aas.AAS_PointReachabilityAreaIndex( VMA(1) );
        case LEGACY_BOTLIB_PC_LOAD_SOURCE:
                return botlib_export->PC_LoadSourceHandle( VMA(1) );
        case LEGACY_BOTLIB_PC_FREE_SOURCE:
                return botlib_export->PC_FreeSourceHandle( args[1] );
        case LEGACY_BOTLIB_PC_READ_TOKEN:
                return botlib_export->PC_ReadTokenHandle( args[1], VMA(2) );
        case LEGACY_BOTLIB_PC_SOURCE_FILE_AND_LINE:
                return botlib_export->PC_SourceFileAndLine( args[1], VMA(2), VMA(3) );
        case LEGACY_BOTLIB_PC_LOAD_GLOBAL_DEFINES:
            return 0;
        case LEGACY_BOTLIB_PC_REMOVE_ALL_GLOBAL_DEFINES:
            return 0;


		// Ghoul2 Insert Start
        // NB - Ghoul2 calls are most likely different to vanilla SoF2.
        case LEGACY_G_G2_LISTBONES:
            //G2API_ListBones(VMA(1)); // arg2 frame ignored, but also NB - vanilla SDK does not call this function.
                Com_Printf("G_G2_LISTBONES: Not implemented!\r\n");
            return 0;
        case LEGACY_G_G2_LISTSURFACES:
                Com_Printf("G_G2_LISTSURFACES: Not implemented!\r\n");
            //G2API_ListSurfaces(VMA(1));
            return 0;
        case LEGACY_G_G2_HAVEWEGHOULMODELS:
			// CGame syscall, not used in the server.
            return 0;
        case LEGACY_G_G2_SETMODELS:
			// CGame syscall, not used in the server.
            return 0;
        case LEGACY_G_G2_GETBOLT:
			// CGame syscall, not used in the server.
            return 0;
        case LEGACY_G_G2_INITGHOUL2MODEL: {
            CGhoul2Model_t** test = VMA(1);
            G2API_InitGhoul2Model(test, (const char *)VMA(2), args[4], args[7]);
            Com_Printf("break");
        }
        case LEGACY_G_G2_ADDBOLT:
			// CGame / UI syscall, not used in the server.
            return 0;
        case LEGACY_G_G2_SETBOLTINFO:
			// Cgame / UI syscall, not used in the server.
            return 0;
        case LEGACY_G_G2_ANGLEOVERRIDE:
			return G2API_SetBoneAngles(VMA(1), VMA(2), VMA(3), args[4], args[5], args[6], args[7]); // has 3 more args which are ignored.
        case LEGACY_G_G2_PLAYANIM:
			    return G2API_SetBoneAnim(VMA(1), VMA(3), args[4], args[5], args[6], VMF(7), VMF(9)); // arg2 and arg8 are ignored.
        case LEGACY_G_G2_GETGLANAME:
            // Not used.
            return 0;
        case LEGACY_G_G2_COPYGHOUL2INSTANCE:
            // Not used.
            return 0;
        case LEGACY_G_G2_COPYSPECIFICGHOUL2MODEL:
			// CGame syscall, not used in the server.
            return 0;
        case LEGACY_G_G2_DUPLICATEGHOUL2INSTANCE:
			// CGame syscall, not used in the server.
            return 0;
        case LEGACY_G_G2_REMOVEGHOUL2MODEL:
        case LEGACY_G_G2_CLEANMODELS:
                Com_Printf("Syscall %d cleanmodel %p\r\n", args[0], VMA(1));
            return G2API_RemoveGhoul2Model(VMA(1));
        case LEGACY_G_GP_PARSE: {
            return (intptr_t)GP_Parse(VMA(1));
        }
            
        case LEGACY_G_GP_PARSE_FILE:
            return (intptr_t)GP_ParseFile(VMA(1));
        case LEGACY_G_GP_CLEAN:
            GP_Clean(VMA(1));
            return 0;
        case LEGACY_G_GP_DELETE:
            GP_Delete(VMA(1));
            return 0;

        case LEGACY_G_GP_GET_BASE_PARSE_GROUP:
            return (intptr_t)GP_GetBaseParseGroup((TGenericParser2)args[1]);
        case LEGACY_G_GPG_GET_NAME:
            return (intptr_t)GPG_GetName((TGPGroup)(args[1]), VMA(2), -1);
        case LEGACY_G_GPG_GET_NEXT:
            return ((intptr_t)GPG_GetNext((TGPGroup)(args[1])));
        case LEGACY_G_GPG_GET_INORDER_NEXT:
            return ((intptr_t)GPG_GetInOrderNext((TGPGroup)(args[1])));
        case LEGACY_G_GPG_GET_INORDER_PREVIOUS:
            return ((intptr_t)GPG_GetInOrderPrevious((TGPGroup)(args[1])));
        case LEGACY_G_GPG_GET_PAIRS:
            return ((intptr_t)GPG_GetPairs((TGPGroup)(args[1])));
        case LEGACY_G_GPG_GET_INORDER_PAIRS:
            return ((intptr_t)GPG_GetInOrderPairs((TGPGroup)(args[1])));
        case LEGACY_G_GPG_GET_SUBGROUPS:
            return ((intptr_t)GPG_GetSubGroups((TGPGroup)(args[1])));
        case LEGACY_G_GPG_GET_INORDER_SUBGROUPS:
            return ((intptr_t)GPG_GetInOrderSubGroups((TGPGroup)(args[1])));
        case LEGACY_G_GPG_FIND_SUBGROUP:
            return ((intptr_t)GPG_FindSubGroup((TGPGroup)(args[1]), VMA(2)));
        case LEGACY_G_GPG_FIND_PAIR:
            return ((intptr_t)GPG_FindPair((TGPGroup)(args[1]), VMA(2)));
        case LEGACY_G_GPG_FIND_PAIRVALUE:
            GPG_FindPairValue((TGPGroup)(args[1]), VMA(2), VMA(3), VMA(4), -1);
            return 0;

            // CGPValue (void *) routines
        case LEGACY_G_GPV_GET_NAME:
            return GPV_GetName((TGPValue)(args[1]), VMA(2), -1);
        case LEGACY_G_GPV_GET_NEXT:
            return ((intptr_t)GPV_GetNext((TGPValue)(args[1])));
        case LEGACY_G_GPV_GET_INORDER_NEXT:
            return ((intptr_t)GPV_GetInOrderNext((TGPValue)(args[1])));
        case LEGACY_G_GPV_GET_INORDER_PREVIOUS:
            return ((intptr_t)GPV_GetInOrderPrevious((TGPValue)(args[1])));
        case LEGACY_G_GPV_IS_LIST:
            return GPV_IsList((TGPValue)(args[1]));
        case LEGACY_G_GPV_GET_TOP_VALUE:
            return GPV_GetTopValue((TGPValue)(args[1]), VMA(2), -1);
        case LEGACY_G_GPV_GET_LIST:
            return ((intptr_t)GPV_GetList((TGPValue)(args[1])));

        case LEGACY_G_CM_REGISTER_TERRAIN:
            return CM_RegisterTerrain((const char*)VMA(1));

        case LEGACY_G_GET_MODEL_FORMALNAME:
            // Doesn't seem to be used.
            return 0;

        // Memory management
        // In SoF2Plus, memory management is actually handled by the game module, but QVM expects them to be managed by the engine module.
        case LEGACY_G_VM_LOCALALLOC:
        {
            /*if (!qvmMemoryInitialized) {
                SV_InitQvmMemory();
            }

            return (intptr_t)SV_QVM_Alloc(args[1]);*/

            return (intptr_t)QVM_Local_Alloc(args[1]);
        }
        case LEGACY_G_VM_LOCALALLOCUNALIGNED:
        {
            /*if (!qvmMemoryInitialized) {
                SV_InitQvmMemory();
            }

			return (intptr_t)SV_QVM_AllocUnaligned(args[1]);*/
            return (intptr_t)QVM_Local_AllocUnaligned(args[1]);
        }
        case LEGACY_G_VM_LOCALTEMPALLOC:
        {
            /*if (!qvmMemoryInitialized) {
                SV_InitQvmMemory();
            }

			return (intptr_t)SV_QVM_TempAlloc(args[1]);*/
            return (intptr_t)QVM_Local_TempAlloc(args[1]);
        }
        case LEGACY_G_VM_LOCALTEMPFREE:
        {
            /*if (!qvmMemoryInitialized) {
                SV_InitQvmMemory();
            }

			SV_QVM_TempFree(args[1]);*/
            QVM_Local_TempFree(args[1]);
            return 0;
        }
        case LEGACY_G_VM_LOCALSTRINGALLOC:
        {
            /*if (!qvmMemoryInitialized) {
                SV_InitQvmMemory();
            }

			return SV_QVM_StringAlloc((const char*)VMA(1));*/
            return QVM_Local_StringAlloc((const char*)VMA(1));
        }

		// End memory management

        case LEGACY_G_G2_COLLISIONDETECT:
            G2API_CollisionDetect(VMA(1), VMA(2), (const float*)VMA(3), (const float*)VMA(4), args[5], args[6],
                (float*)VMA(7), (float*)VMA(8), (float*)VMA(9), args[10], args[11]);
            return 0;
        case LEGACY_G_G2_REGISTERSKIN:
            return G2API_RegisterSkin((const char*)VMA(1), args[2], (const char*)VMA(3));
        case LEGACY_G_G2_SETSKIN: {
            CGhoul2Model_t* test = args[1];
            CGhoul2Model_t* test2 = VMA(1);
            return (intptr_t)G2API_SetSkin(args[1], args[3]);
        }

        case LEGACY_G_G2_GETANIMFILENAMEINDEX:
            qboolean ok = G2API_GetAnimFileName(args[1], VMA(3), -1);
            char* test = VMA(3);
                //L;OL
                return ok;
        case LEGACY_G_GT_INIT:
            SV_GT_Init((const char*)VMA(1), args[2]);
            return 0;
        case LEGACY_G_GT_RUNFRAME:
            SV_GT_RunFrame(args[1]);
            return 0;
        case LEGACY_G_GT_START:
            SV_GT_Start(args[1]);
            return 0;
        case LEGACY_G_GT_SENDEVENT:
            return SV_GT_SendEvent(args[1], args[2], args[3], args[4], args[5], args[6], args[7]);

        default:
            Com_Error(ERR_DROP, "Bad game system trap: %ld", (long int)args[0]);
        }
#else
        switch (args[0]) {
        case G_PRINT:
            Com_Printf("%s", (const char*)VMA(1));
            return 0;
        case G_ERROR:
            Com_Error(ERR_DROP, "%s", (const char*)VMA(1));
            return 0;
        case G_MILLISECONDS:
            return Sys_Milliseconds();
        case G_CVAR_REGISTER:
            Cvar_Register(VMA(1), VMA(2), VMA(3), args[4], VMF(5), VMF(6));
            return 0;
        case G_CVAR_UPDATE:
            Cvar_Update(VMA(1));
            return 0;
        case G_CVAR_SET:
            Cvar_SetSafe((const char*)VMA(1), (const char*)VMA(2));
            return 0;
        case G_CVAR_VARIABLE_INTEGER_VALUE:
            return Cvar_VariableIntegerValue((const char*)VMA(1));
        case G_CVAR_VARIABLE_STRING_BUFFER:
            Cvar_VariableStringBuffer(VMA(1), VMA(2), args[3]);
            return 0;
        case G_ARGC:
            return Cmd_Argc();
        case G_ARGV:
            Cmd_ArgvBuffer(args[1], VMA(2), args[3]);
            return 0;
        case G_SEND_CONSOLE_COMMAND:
            Cbuf_ExecuteText(args[1], VMA(2));
            return 0;

        case G_FS_FOPEN_FILE:
            return FS_FOpenFileByMode(VMA(1), VMA(2), args[3]);
        case G_FS_READ:
            FS_Read(VMA(1), args[2], args[3]);
            return 0;
        case G_FS_WRITE:
            FS_Write(VMA(1), args[2], args[3]);
            return 0;
        case G_FS_FCLOSE_FILE:
            FS_FCloseFile(args[1]);
            return 0;
        case G_FS_GETFILELIST:
            return FS_GetFileList(VMA(1), VMA(2), VMA(3), args[4]);

        case G_LOCATE_GAME_DATA:
            SV_LocateGameData(VMA(1), args[2], args[3], VMA(4), args[5]);
            return 0;

        case G_GET_WORLD_BOUNDS:
            CM_ModelBounds(0, VMA(1), VMA(2));
            return 0;

        case G_DROP_CLIENT:
            SV_GameDropClient(args[1], VMA(2));
            return 0;
        case G_SEND_SERVER_COMMAND:
            SV_GameSendServerCommand(args[1], VMA(2));
            return 0;
        case G_LINKENTITY:
            SV_LinkEntity(VMA(1));
            return 0;
        case G_UNLINKENTITY:
            SV_UnlinkEntity(VMA(1));
            return 0;
        case G_ENTITIES_IN_BOX:
            return SV_AreaEntities(VMA(1), VMA(2), VMA(3), args[4]);
        case G_ENTITY_CONTACT:
            return SV_EntityContact(VMA(1), VMA(2), VMA(3), /*int capsule*/ qfalse);
        case G_ENTITY_CONTACTCAPSULE:
            return SV_EntityContact(VMA(1), VMA(2), VMA(3), /*int capsule*/ qtrue);
        case G_TRACE:
            SV_Trace(VMA(1), VMA(2), VMA(3), VMA(4), VMA(5), args[6], args[7], /*int capsule*/ qfalse);
            return 0;
        case G_TRACECAPSULE:
            SV_Trace(VMA(1), VMA(2), VMA(3), VMA(4), VMA(5), args[6], args[7], /*int capsule*/ qtrue);
            return 0;
        case G_POINT_CONTENTS:
            return SV_PointContents(VMA(1), args[2]);
        case G_SET_BRUSH_MODEL:
            SV_SetBrushModel(VMA(1), VMA(2));
            return 0;
        case G_SET_ACTIVE_SUBBSP:
            SV_SetActiveSubBSP(args[1]);
            return 0;
        case G_IN_PVS:
            return SV_inPVS(VMA(1), VMA(2));
        case G_IN_PVS_IGNORE_PORTALS:
            return SV_inPVSIgnorePortals(VMA(1), VMA(2));

        case G_SET_CONFIGSTRING:
            SV_SetConfigstring(args[1], VMA(2));
            return 0;
        case G_GET_CONFIGSTRING:
            SV_GetConfigstring(args[1], VMA(2), args[3]);
            return 0;
        case G_SET_USERINFO:
            SV_SetUserinfo(args[1], VMA(2));
            return 0;
        case G_GET_USERINFO:
            SV_GetUserinfo(args[1], VMA(2), args[3]);
            return 0;
        case G_GET_SERVERINFO:
            SV_GetServerinfo(VMA(1), args[2]);
            return 0;
        case G_ADJUST_AREA_PORTAL_STATE:
            SV_AdjustAreaPortalState(VMA(1), args[2]);
            return 0;
        case G_AREAS_CONNECTED:
            return CM_AreasConnected(args[1], args[2]);

        case G_BOT_ALLOCATE_CLIENT:
            return SV_BotAllocateClient();
        case G_BOT_FREE_CLIENT:
            SV_BotFreeClient(args[1]);
            return 0;

        case G_GET_USERCMD:
            SV_GetUsercmd(args[1], VMA(2));
            return 0;
        case G_GET_ENTITY_TOKEN:
        {
            const char* s;
            qboolean inSubBSP = (qboolean)args[3];

            if (inSubBSP) {
                s = COM_Parse(&sv.subBSPParsePoint);
                Q_strncpyz(VMA(1), s, args[2]);
                if (!sv.subBSPParsePoint && !s[0]) {
                    return qfalse;
                }
                else {
                    return qtrue;
                }
            }
            else {
                s = COM_Parse(&sv.entityParsePoint);
                Q_strncpyz(VMA(1), s, args[2]);
                if (!sv.entityParsePoint && !s[0]) {
                    return qfalse;
                }
                else {
                    return qtrue;
                }
            }
        }

        case G_DEBUG_POLYGON_CREATE:
            return BotImport_DebugPolygonCreate(args[1], args[2], VMA(3));
        case G_DEBUG_POLYGON_DELETE:
            BotImport_DebugPolygonDelete(args[1]);
            return 0;
        case G_REAL_TIME:
            return Com_RealTime(VMA(1));
        case G_SNAPVECTOR:
            Q_SnapVector(VMA(1));
            return 0;

            //====================================

        case BOTLIB_SETUP:
            return SV_BotLibSetup();
        case BOTLIB_SHUTDOWN:
            return SV_BotLibShutdown();
        case BOTLIB_LIBVAR_SET:
            return botlib_export->BotLibVarSet(VMA(1), VMA(2));
        case BOTLIB_LIBVAR_GET:
            return botlib_export->BotLibVarGet(VMA(1), VMA(2), args[3]);

        case BOTLIB_PC_ADD_GLOBAL_DEFINE:
            return botlib_export->PC_AddGlobalDefine(VMA(1));
        case BOTLIB_PC_LOAD_SOURCE:
            return botlib_export->PC_LoadSourceHandle(VMA(1));
        case BOTLIB_PC_FREE_SOURCE:
            return botlib_export->PC_FreeSourceHandle(args[1]);
        case BOTLIB_PC_READ_TOKEN:
            return botlib_export->PC_ReadTokenHandle(args[1], VMA(2));
        case BOTLIB_PC_SOURCE_FILE_AND_LINE:
            return botlib_export->PC_SourceFileAndLine(args[1], VMA(2), VMA(3));

        case BOTLIB_START_FRAME:
            return botlib_export->BotLibStartFrame(VMF(1));
        case BOTLIB_LOAD_MAP:
            return botlib_export->BotLibLoadMap(VMA(1));
        case BOTLIB_UPDATENTITY:
            return botlib_export->BotLibUpdateEntity(args[1], VMA(2));
        case BOTLIB_TEST:
            return botlib_export->Test(args[1], VMA(2), VMA(3), VMA(4));

        case BOTLIB_GET_SNAPSHOT_ENTITY:
            return SV_BotGetSnapshotEntity(args[1], args[2]);
        case BOTLIB_GET_CONSOLE_MESSAGE:
            return SV_BotGetConsoleMessage(args[1], VMA(2), args[3]);
        case BOTLIB_USER_COMMAND:
        {
            int clientNum = args[1];

            if (clientNum >= 0 && clientNum < sv_maxclients->integer) {
                SV_ClientThink(&svs.clients[clientNum], VMA(2));
            }
        }
        return 0;

        case BOTLIB_AAS_BBOX_AREAS:
            return botlib_export->aas.AAS_BBoxAreas(VMA(1), VMA(2), VMA(3), args[4]);
        case BOTLIB_AAS_AREA_INFO:
            return botlib_export->aas.AAS_AreaInfo(args[1], VMA(2));
        case BOTLIB_AAS_ALTERNATIVE_ROUTE_GOAL:
            return botlib_export->aas.AAS_AlternativeRouteGoals(VMA(1), args[2], VMA(3), args[4], args[5], VMA(6), args[7], args[8]);
        case BOTLIB_AAS_ENTITY_INFO:
            botlib_export->aas.AAS_EntityInfo(args[1], VMA(2));
            return 0;

        case BOTLIB_AAS_INITIALIZED:
            return botlib_export->aas.AAS_Initialized();
        case BOTLIB_AAS_PRESENCE_TYPE_BOUNDING_BOX:
            botlib_export->aas.AAS_PresenceTypeBoundingBox(args[1], VMA(2), VMA(3));
            return 0;
        case BOTLIB_AAS_TIME:
            return FloatAsInt(botlib_export->aas.AAS_Time());

        case BOTLIB_AAS_POINT_AREA_NUM:
            return botlib_export->aas.AAS_PointAreaNum(VMA(1));
        case BOTLIB_AAS_POINT_REACHABILITY_AREA_INDEX:
            return botlib_export->aas.AAS_PointReachabilityAreaIndex(VMA(1));
        case BOTLIB_AAS_TRACE_AREAS:
            return botlib_export->aas.AAS_TraceAreas(VMA(1), VMA(2), VMA(3), VMA(4), args[5]);

        case BOTLIB_AAS_POINT_CONTENTS:
            return botlib_export->aas.AAS_PointContents(VMA(1));
        case BOTLIB_AAS_NEXT_BSP_ENTITY:
            return botlib_export->aas.AAS_NextBSPEntity(args[1]);
        case BOTLIB_AAS_VALUE_FOR_BSP_EPAIR_KEY:
            return botlib_export->aas.AAS_ValueForBSPEpairKey(args[1], VMA(2), VMA(3), args[4]);
        case BOTLIB_AAS_VECTOR_FOR_BSP_EPAIR_KEY:
            return botlib_export->aas.AAS_VectorForBSPEpairKey(args[1], VMA(2), VMA(3));
        case BOTLIB_AAS_FLOAT_FOR_BSP_EPAIR_KEY:
            return botlib_export->aas.AAS_FloatForBSPEpairKey(args[1], VMA(2), VMA(3));
        case BOTLIB_AAS_INT_FOR_BSP_EPAIR_KEY:
            return botlib_export->aas.AAS_IntForBSPEpairKey(args[1], VMA(2), VMA(3));

        case BOTLIB_AAS_AREA_REACHABILITY:
            return botlib_export->aas.AAS_AreaReachability(args[1]);

        case BOTLIB_AAS_AREA_TRAVEL_TIME_TO_GOAL_AREA:
            return botlib_export->aas.AAS_AreaTravelTimeToGoalArea(args[1], VMA(2), args[3], args[4]);
        case BOTLIB_AAS_ENABLE_ROUTING_AREA:
            return botlib_export->aas.AAS_EnableRoutingArea(args[1], args[2]);
        case BOTLIB_AAS_PREDICT_ROUTE:
            return botlib_export->aas.AAS_PredictRoute(VMA(1), args[2], VMA(3), args[4], args[5], args[6], args[7], args[8], args[9], args[10], args[11]);

        case BOTLIB_AAS_SWIMMING:
            return botlib_export->aas.AAS_Swimming(VMA(1));
        case BOTLIB_AAS_PREDICT_CLIENT_MOVEMENT:
            return botlib_export->aas.AAS_PredictClientMovement(VMA(1), args[2], VMA(3), args[4], args[5],
                VMA(6), VMA(7), args[8], args[9], VMF(10), args[11], args[12], args[13]);

        case BOTLIB_EA_SAY:
            botlib_export->ea.EA_Say(args[1], VMA(2));
            return 0;
        case BOTLIB_EA_SAY_TEAM:
            botlib_export->ea.EA_SayTeam(args[1], VMA(2));
            return 0;
        case BOTLIB_EA_COMMAND:
            botlib_export->ea.EA_Command(args[1], VMA(2));
            return 0;

        case BOTLIB_EA_ACTION:
            botlib_export->ea.EA_Action(args[1], args[2]);
            return 0;
        case BOTLIB_EA_GESTURE:
            botlib_export->ea.EA_Gesture(args[1]);
            return 0;
        case BOTLIB_EA_TALK:
            botlib_export->ea.EA_Talk(args[1]);
            return 0;
        case BOTLIB_EA_ATTACK:
            botlib_export->ea.EA_Attack(args[1]);
            return 0;
        case BOTLIB_EA_USE:
            botlib_export->ea.EA_Use(args[1]);
            return 0;
        case BOTLIB_EA_RESPAWN:
            botlib_export->ea.EA_Respawn(args[1]);
            return 0;
        case BOTLIB_EA_CROUCH:
            botlib_export->ea.EA_Crouch(args[1]);
            return 0;
        case BOTLIB_EA_MOVE_UP:
            botlib_export->ea.EA_MoveUp(args[1]);
            return 0;
        case BOTLIB_EA_MOVE_DOWN:
            botlib_export->ea.EA_MoveDown(args[1]);
            return 0;
        case BOTLIB_EA_MOVE_FORWARD:
            botlib_export->ea.EA_MoveForward(args[1]);
            return 0;
        case BOTLIB_EA_MOVE_BACK:
            botlib_export->ea.EA_MoveBack(args[1]);
            return 0;
        case BOTLIB_EA_MOVE_LEFT:
            botlib_export->ea.EA_MoveLeft(args[1]);
            return 0;
        case BOTLIB_EA_MOVE_RIGHT:
            botlib_export->ea.EA_MoveRight(args[1]);
            return 0;

        case BOTLIB_EA_SELECT_WEAPON:
            botlib_export->ea.EA_SelectWeapon(args[1], args[2]);
            return 0;
        case BOTLIB_EA_JUMP:
            botlib_export->ea.EA_Jump(args[1]);
            return 0;
        case BOTLIB_EA_DELAYED_JUMP:
            botlib_export->ea.EA_DelayedJump(args[1]);
            return 0;
        case BOTLIB_EA_MOVE:
            botlib_export->ea.EA_Move(args[1], VMA(2), VMF(3));
            return 0;
        case BOTLIB_EA_VIEW:
            botlib_export->ea.EA_View(args[1], VMA(2));
            return 0;

        case BOTLIB_EA_END_REGULAR:
            botlib_export->ea.EA_EndRegular(args[1], VMF(2));
            return 0;
        case BOTLIB_EA_GET_INPUT:
            botlib_export->ea.EA_GetInput(args[1], VMF(2), VMA(3));
            return 0;
        case BOTLIB_EA_RESET_INPUT:
            botlib_export->ea.EA_ResetInput(args[1]);
            return 0;

        case BOTLIB_AI_LOAD_CHARACTER:
            return botlib_export->ai.BotLoadCharacter(VMA(1), VMF(2));
        case BOTLIB_AI_FREE_CHARACTER:
            botlib_export->ai.BotFreeCharacter(args[1]);
            return 0;
        case BOTLIB_AI_CHARACTERISTIC_FLOAT:
            return FloatAsInt(botlib_export->ai.Characteristic_Float(args[1], args[2]));
        case BOTLIB_AI_CHARACTERISTIC_BFLOAT:
            return FloatAsInt(botlib_export->ai.Characteristic_BFloat(args[1], args[2], VMF(3), VMF(4)));
        case BOTLIB_AI_CHARACTERISTIC_INTEGER:
            return botlib_export->ai.Characteristic_Integer(args[1], args[2]);
        case BOTLIB_AI_CHARACTERISTIC_BINTEGER:
            return botlib_export->ai.Characteristic_BInteger(args[1], args[2], args[3], args[4]);
        case BOTLIB_AI_CHARACTERISTIC_STRING:
            botlib_export->ai.Characteristic_String(args[1], args[2], VMA(3), args[4]);
            return 0;

        case BOTLIB_AI_ALLOC_CHAT_STATE:
            return botlib_export->ai.BotAllocChatState();
        case BOTLIB_AI_FREE_CHAT_STATE:
            botlib_export->ai.BotFreeChatState(args[1]);
            return 0;
        case BOTLIB_AI_QUEUE_CONSOLE_MESSAGE:
            botlib_export->ai.BotQueueConsoleMessage(args[1], args[2], VMA(3));
            return 0;
        case BOTLIB_AI_REMOVE_CONSOLE_MESSAGE:
            botlib_export->ai.BotRemoveConsoleMessage(args[1], args[2]);
            return 0;
        case BOTLIB_AI_NEXT_CONSOLE_MESSAGE:
            return botlib_export->ai.BotNextConsoleMessage(args[1], VMA(2));
        case BOTLIB_AI_NUM_CONSOLE_MESSAGE:
            return botlib_export->ai.BotNumConsoleMessages(args[1]);
        case BOTLIB_AI_INITIAL_CHAT:
            botlib_export->ai.BotInitialChat(args[1], VMA(2), args[3], VMA(4), VMA(5), VMA(6), VMA(7), VMA(8), VMA(9), VMA(10), VMA(11));
            return 0;
        case BOTLIB_AI_NUM_INITIAL_CHATS:
            return botlib_export->ai.BotNumInitialChats(args[1], VMA(2));
        case BOTLIB_AI_REPLY_CHAT:
            return botlib_export->ai.BotReplyChat(args[1], VMA(2), args[3], args[4], VMA(5), VMA(6), VMA(7), VMA(8), VMA(9), VMA(10), VMA(11), VMA(12));
        case BOTLIB_AI_CHAT_LENGTH:
            return botlib_export->ai.BotChatLength(args[1]);
        case BOTLIB_AI_ENTER_CHAT:
            botlib_export->ai.BotEnterChat(args[1], args[2], args[3]);
            return 0;
        case BOTLIB_AI_GET_CHAT_MESSAGE:
            botlib_export->ai.BotGetChatMessage(args[1], VMA(2), args[3]);
            return 0;
        case BOTLIB_AI_STRING_CONTAINS:
            return botlib_export->ai.StringContains(VMA(1), VMA(2), args[3]);
        case BOTLIB_AI_FIND_MATCH:
            return botlib_export->ai.BotFindMatch(VMA(1), VMA(2), args[3]);
        case BOTLIB_AI_MATCH_VARIABLE:
            botlib_export->ai.BotMatchVariable(VMA(1), args[2], VMA(3), args[4]);
            return 0;
        case BOTLIB_AI_UNIFY_WHITE_SPACES:
            botlib_export->ai.UnifyWhiteSpaces(VMA(1));
            return 0;
        case BOTLIB_AI_REPLACE_SYNONYMS:
            botlib_export->ai.BotReplaceSynonyms(VMA(1), args[2]);
            return 0;
        case BOTLIB_AI_LOAD_CHAT_FILE:
            return botlib_export->ai.BotLoadChatFile(args[1], VMA(2), VMA(3));
        case BOTLIB_AI_SET_CHAT_GENDER:
            botlib_export->ai.BotSetChatGender(args[1], args[2]);
            return 0;
        case BOTLIB_AI_SET_CHAT_NAME:
            botlib_export->ai.BotSetChatName(args[1], VMA(2), args[3]);
            return 0;

        case BOTLIB_AI_RESET_GOAL_STATE:
            botlib_export->ai.BotResetGoalState(args[1]);
            return 0;
        case BOTLIB_AI_RESET_AVOID_GOALS:
            botlib_export->ai.BotResetAvoidGoals(args[1]);
            return 0;
        case BOTLIB_AI_REMOVE_FROM_AVOID_GOALS:
            botlib_export->ai.BotRemoveFromAvoidGoals(args[1], args[2]);
            return 0;
        case BOTLIB_AI_PUSH_GOAL:
            botlib_export->ai.BotPushGoal(args[1], VMA(2));
            return 0;
        case BOTLIB_AI_POP_GOAL:
            botlib_export->ai.BotPopGoal(args[1]);
            return 0;
        case BOTLIB_AI_EMPTY_GOAL_STACK:
            botlib_export->ai.BotEmptyGoalStack(args[1]);
            return 0;
        case BOTLIB_AI_DUMP_AVOID_GOALS:
            botlib_export->ai.BotDumpAvoidGoals(args[1]);
            return 0;
        case BOTLIB_AI_DUMP_GOAL_STACK:
            botlib_export->ai.BotDumpGoalStack(args[1]);
            return 0;
        case BOTLIB_AI_GOAL_NAME:
            botlib_export->ai.BotGoalName(args[1], VMA(2), args[3]);
            return 0;
        case BOTLIB_AI_GET_TOP_GOAL:
            return botlib_export->ai.BotGetTopGoal(args[1], VMA(2));
        case BOTLIB_AI_GET_SECOND_GOAL:
            return botlib_export->ai.BotGetSecondGoal(args[1], VMA(2));
        case BOTLIB_AI_CHOOSE_LTG_ITEM:
            return botlib_export->ai.BotChooseLTGItem(args[1], VMA(2), VMA(3), args[4]);
        case BOTLIB_AI_CHOOSE_NBG_ITEM:
            return botlib_export->ai.BotChooseNBGItem(args[1], VMA(2), VMA(3), args[4], VMA(5), VMF(6));
        case BOTLIB_AI_TOUCHING_GOAL:
            return botlib_export->ai.BotTouchingGoal(VMA(1), VMA(2));
        case BOTLIB_AI_ITEM_GOAL_IN_VIS_BUT_NOT_VISIBLE:
            return botlib_export->ai.BotItemGoalInVisButNotVisible(args[1], VMA(2), VMA(3), VMA(4));
        case BOTLIB_AI_GET_LEVEL_ITEM_GOAL:
            return botlib_export->ai.BotGetLevelItemGoal(args[1], VMA(2), VMA(3));
        case BOTLIB_AI_GET_NEXT_CAMP_SPOT_GOAL:
            return botlib_export->ai.BotGetNextCampSpotGoal(args[1], VMA(2));
        case BOTLIB_AI_GET_MAP_LOCATION_GOAL:
            return botlib_export->ai.BotGetMapLocationGoal(VMA(1), VMA(2));
        case BOTLIB_AI_AVOID_GOAL_TIME:
            return FloatAsInt(botlib_export->ai.BotAvoidGoalTime(args[1], args[2]));
        case BOTLIB_AI_SET_AVOID_GOAL_TIME:
            botlib_export->ai.BotSetAvoidGoalTime(args[1], args[2], VMF(3));
            return 0;
        case BOTLIB_AI_INIT_LEVEL_ITEMS:
            botlib_export->ai.BotInitLevelItems();
            return 0;
        case BOTLIB_AI_UPDATE_ENTITY_ITEMS:
            botlib_export->ai.BotUpdateEntityItems();
            return 0;
        case BOTLIB_AI_LOAD_ITEM_WEIGHTS:
            return botlib_export->ai.BotLoadItemWeights(args[1], VMA(2));
        case BOTLIB_AI_FREE_ITEM_WEIGHTS:
            botlib_export->ai.BotFreeItemWeights(args[1]);
            return 0;
        case BOTLIB_AI_INTERBREED_GOAL_FUZZY_LOGIC:
            botlib_export->ai.BotInterbreedGoalFuzzyLogic(args[1], args[2], args[3]);
            return 0;
        case BOTLIB_AI_SAVE_GOAL_FUZZY_LOGIC:
            botlib_export->ai.BotSaveGoalFuzzyLogic(args[1], VMA(2));
            return 0;
        case BOTLIB_AI_MUTATE_GOAL_FUZZY_LOGIC:
            botlib_export->ai.BotMutateGoalFuzzyLogic(args[1], VMF(2));
            return 0;
        case BOTLIB_AI_ALLOC_GOAL_STATE:
            return botlib_export->ai.BotAllocGoalState(args[1]);
        case BOTLIB_AI_FREE_GOAL_STATE:
            botlib_export->ai.BotFreeGoalState(args[1]);
            return 0;

        case BOTLIB_AI_RESET_MOVE_STATE:
            botlib_export->ai.BotResetMoveState(args[1]);
            return 0;
        case BOTLIB_AI_ADD_AVOID_SPOT:
            botlib_export->ai.BotAddAvoidSpot(args[1], VMA(2), VMF(3), args[4]);
            return 0;
        case BOTLIB_AI_MOVE_TO_GOAL:
            botlib_export->ai.BotMoveToGoal(VMA(1), args[2], VMA(3), args[4]);
            return 0;
        case BOTLIB_AI_MOVE_IN_DIRECTION:
            return botlib_export->ai.BotMoveInDirection(args[1], VMA(2), VMF(3), args[4]);
        case BOTLIB_AI_RESET_AVOID_REACH:
            botlib_export->ai.BotResetAvoidReach(args[1]);
            return 0;
        case BOTLIB_AI_RESET_LAST_AVOID_REACH:
            botlib_export->ai.BotResetLastAvoidReach(args[1]);
            return 0;
        case BOTLIB_AI_REACHABILITY_AREA:
            return botlib_export->ai.BotReachabilityArea(VMA(1), args[2]);
        case BOTLIB_AI_MOVEMENT_VIEW_TARGET:
            return botlib_export->ai.BotMovementViewTarget(args[1], VMA(2), args[3], VMF(4), VMA(5));
        case BOTLIB_AI_PREDICT_VISIBLE_POSITION:
            return botlib_export->ai.BotPredictVisiblePosition(VMA(1), args[2], VMA(3), args[4], VMA(5));
        case BOTLIB_AI_ALLOC_MOVE_STATE:
            return botlib_export->ai.BotAllocMoveState();
        case BOTLIB_AI_FREE_MOVE_STATE:
            botlib_export->ai.BotFreeMoveState(args[1]);
            return 0;
        case BOTLIB_AI_INIT_MOVE_STATE:
            botlib_export->ai.BotInitMoveState(args[1], VMA(2));
            return 0;

        case BOTLIB_AI_CHOOSE_BEST_FIGHT_WEAPON:
            return botlib_export->ai.BotChooseBestFightWeapon(args[1], VMA(2));
        case BOTLIB_AI_GET_WEAPON_INFO:
            botlib_export->ai.BotGetWeaponInfo(args[1], args[2], VMA(3));
            return 0;
        case BOTLIB_AI_LOAD_WEAPON_WEIGHTS:
            return botlib_export->ai.BotLoadWeaponWeights(args[1], VMA(2));
        case BOTLIB_AI_ALLOC_WEAPON_STATE:
            return botlib_export->ai.BotAllocWeaponState();
        case BOTLIB_AI_FREE_WEAPON_STATE:
            botlib_export->ai.BotFreeWeaponState(args[1]);
            return 0;
        case BOTLIB_AI_RESET_WEAPON_STATE:
            botlib_export->ai.BotResetWeaponState(args[1]);
            return 0;

        case BOTLIB_AI_GENETIC_PARENTS_AND_CHILD_SELECTION:
            return botlib_export->ai.GeneticParentsAndChildSelection(args[1], VMA(2), VMA(3), VMA(4), VMA(5));

            //=============== Ghoul II functionality ================
        case G_G2_LISTBONES:
            G2API_ListBones(VMA(1));
            return 0;
        case G_G2_LISTSURFACES:
            G2API_ListSurfaces(VMA(1));
            return 0;
        case G_G2_INITGHOUL2MODEL:
            return G2API_InitGhoul2Model(VMA(1), (const char*)VMA(2), args[3], args[4]);
        case G_G2_ANGLEOVERRIDE:
            return G2API_SetBoneAngles(VMA(1), (const char*)VMA(2), (float*)VMA(3), args[4],
                (const Eorientations)args[5], (const Eorientations)args[6], (const Eorientations)args[7]);
        case G_G2_PLAYANIM:
            return G2API_SetBoneAnim(VMA(1), (const char*)VMA(2), args[3], args[4], args[5], VMF(6), VMF(7));
        case G_G2_GETANIMFILENAME:
            return (intptr_t)G2API_GetAnimFileName(VMA(1), VMA(2), args[3]);
        case G_G2_REMOVEGHOUL2MODEL:
            return G2API_RemoveGhoul2Model(VMA(1));

        case G_G2_COLLISIONDETECT:
            G2API_CollisionDetect(VMA(1), VMA(2), (const float*)VMA(3), (const float*)VMA(4), args[5], args[6],
                (float*)VMA(7), (float*)VMA(8), (float*)VMA(9), args[10], args[11]);
            return 0;
        case G_G2_REGISTERSKIN:
            return G2API_RegisterSkin((const char*)VMA(1), args[2], (const char*)VMA(3));
        case G_G2_SETSKIN:
            return (intptr_t)G2API_SetSkin(VMA(1), args[2]);

            //======== Generic Parser 2 (GP2) functionality =========
            // CGenericParser2 (void *) routines
        case G_GP_PARSE:
            return (intptr_t)GP_Parse(VMA(1));
        case G_GP_PARSE_FILE:
            return (intptr_t)GP_ParseFile((const char*)VMA(1));
        case G_GP_CLEAN:
            GP_Clean((TGenericParser2)args[1]);
            return 0;
        case G_GP_DELETE:
            GP_Delete((TGenericParser2)args[1]);
            return 0;
        case G_GP_GET_BASE_PARSE_GROUP:
            return (intptr_t)GP_GetBaseParseGroup((TGenericParser2)args[1]);

            // CGPGroup (void *) routines
        case G_GPG_GET_NAME:
            return (intptr_t)GPG_GetName((TGPGroup)args[1], VMA(2), args[3]);
        case G_GPG_GET_NEXT:
            return (intptr_t)GPG_GetNext((TGPGroup)args[1]);
        case G_GPG_GET_INORDER_NEXT:
            return (intptr_t)GPG_GetInOrderNext((TGPGroup)args[1]);
        case G_GPG_GET_INORDER_PREVIOUS:
            return (intptr_t)GPG_GetInOrderPrevious((TGPGroup)args[1]);
        case G_GPG_GET_PAIRS:
            return (intptr_t)GPG_GetPairs((TGPGroup)args[1]);
        case G_GPG_GET_INORDER_PAIRS:
            return (intptr_t)GPG_GetInOrderPairs((TGPGroup)args[1]);
        case G_GPG_GET_SUBGROUPS:
            return (intptr_t)GPG_GetSubGroups((TGPGroup)args[1]);
        case G_GPG_GET_INORDER_SUBGROUPS:
            return (intptr_t)GPG_GetInOrderSubGroups((TGPGroup)args[1]);
        case G_GPG_FIND_SUBGROUP:
            return (intptr_t)GPG_FindSubGroup((TGPGroup)args[1], VMA(2));
        case G_GPG_FIND_PAIR:
            return (intptr_t)GPG_FindPair((TGPGroup)args[1], VMA(2));
        case G_GPG_FIND_PAIRVALUE:
            GPG_FindPairValue((TGPGroup)args[1], VMA(2), VMA(3), VMA(4), args[5]);
            return 0;

            // CGPValue (void *) routines
        case G_GPV_GET_NAME:
            return GPV_GetName((TGPValue)args[1], VMA(2), args[3]);
        case G_GPV_GET_NEXT:
            return (intptr_t)GPV_GetNext((TGPValue)args[1]);
        case G_GPV_GET_INORDER_NEXT:
            return (intptr_t)GPV_GetInOrderNext((TGPValue)args[1]);
        case G_GPV_GET_INORDER_PREVIOUS:
            return (intptr_t)GPV_GetInOrderPrevious((TGPValue)args[1]);
        case G_GPV_IS_LIST:
            return GPV_IsList((TGPValue)args[1]);
        case G_GPV_GET_TOP_VALUE:
            return GPV_GetTopValue((TGPValue)args[1], VMA(2), args[3]);
        case G_GPV_GET_LIST:
            return (intptr_t)GPV_GetList((TGPValue)args[1]);

        case G_CM_REGISTER_TERRAIN:
            return CM_RegisterTerrain((const char*)VMA(1));

        case G_MEM_INIT:
        {
            void* gameMemory;

            // Free any memory previously allocated by the game module.
            // This should happen once per map.
            Z_FreeTags(TAG_GAMEMEM);

            // Allocate memory for the game module memory management system.
            gameMemory = Z_TagMalloc(args[1], TAG_GAMEMEM);
            Com_Memset(gameMemory, 0, args[1]);

            return (intptr_t)gameMemory;
        }

        //================= Gametype interface ==================
        case G_GT_INIT:
            SV_GT_Init((const char*)VMA(1), args[2]);
            return 0;
        case G_GT_RUNFRAME:
            SV_GT_RunFrame(args[1]);
            return 0;
        case G_GT_START:
            SV_GT_Start(args[1]);
            return 0;
        case G_GT_SENDEVENT:
            return SV_GT_SendEvent(args[1], args[2], args[3], args[4], args[5], args[6], args[7]);
        case G_GT_SHUTDOWN:
            SV_GT_Shutdown();
            return 0;

        case G_CLIENT_ISLEGACYPROTOCOL:
            if (args[1] < 0 || args[1] >= sv_maxclients->integer) {
                Com_Error(ERR_DROP, "Syscall IsLegacyProtocol: bad clientNum %i", args[1]);
            }
            return svs.clients[args[1]].legacyProtocol;

        case G_TRANSLATE_SILVER_WPN_TO_GOLD:
            return translateSilverWeaponToGoldWeapon(args[1]);

        case G_TRANSLATE_GOLD_WPN_TO_SILVER:
            return translateGoldWeaponToSilverWeapon(args[1]);

        case G_VALIDATE_MAP_NAME:
            return SV_ValidateMapName(VMA(1), VMA(2), args[3]);

        case G_GET_MAPCYCLE_LIST:
            return SV_MapcycleList(VMA(1), args[2]);

        case G_SKIP_TO_MAP:
            SV_SkipToMap(args[1]);
            return 0;

        //=======================================================
        default:
            Com_Error(ERR_DROP, "Bad game system trap: %ld", (long int)args[0]);
        }
#endif

    
    return 0;
}

/*
===============
SV_ShutdownGameProgs

Called every time a map changes
===============
*/
void SV_ShutdownGameProgs( void ) {
    if ( !gvm ) {
        return;
    }
    VM_Call( gvm, GAME_GHOUL_SHUTDOWN, qfalse );
    VM_Call( gvm, GAME_SHUTDOWN, qfalse );

    VM_Free( gvm );
    gvm = NULL;
}

/*
==================
SV_InitGameVM

Called for both a full init and a restart
==================
*/
static void SV_InitGameVM( qboolean restart ) {
    int     i;

    // start the entity parsing at the beginning
    sv.entityParsePoint = CM_EntityString(0);

    // clear all gentity pointers that might still be set from
    // a previous level
    // https://zerowing.idsoftware.com/bugzilla/show_bug.cgi?id=522
    //   now done before GAME_INIT call
    for ( i = 0 ; i < sv_maxclients->integer ; i++ ) {
        svs.clients[i].gentity = NULL;
    }

    // use the current msec count for a random seed
    // init for this gamestate
    VM_Call( gvm, GAME_INIT, sv.time, Com_Milliseconds(), restart );
    VM_Call( gvm, GAME_GHOUL_INIT );
}



/*
===================
SV_RestartGameProgs

Called on a map_restart, but not on a normal map change
===================
*/
void SV_RestartGameProgs( void ) {
    if ( !gvm ) {
        return;
    }
    VM_Call( gvm, GAME_GHOUL_SHUTDOWN );
    VM_Call( gvm, GAME_SHUTDOWN, qtrue );

    // do a restart instead of a free
    gvm = VM_Restart(gvm, qtrue);
    if ( !gvm ) {
        Com_Error( ERR_FATAL, "VM_Restart on game failed" );
    }

    SV_InitGameVM( qtrue );
}


/*
===============
SV_InitGameProgs

Called on a normal map change, not on a map_restart
===============
*/
void SV_InitGameProgs( void ) {
    cvar_t  *var;
    //FIXME these are temp while I make bots run in vm
    extern int  bot_enable;

    var = Cvar_Get( "bot_enable", "1", CVAR_LATCH );
    if ( var ) {
        bot_enable = var->integer;
    }
    else {
        bot_enable = 0;
    }

    // load the dll or bytecode
    gvm = VM_Create( "sof2mp_game", SV_GameSystemCalls, Cvar_VariableIntegerValue("vm_game"));
    if ( !gvm ) {
        Com_Error( ERR_FATAL, "VM_Create on game failed" );
    }

    SV_InitGameVM( qfalse );
}


/*
====================
SV_GameCommand

See if the current console command is claimed by the game
====================
*/
qboolean SV_GameCommand( void ) {
    if ( sv.state != SS_GAME ) {
        return qfalse;
    }

    return VM_Call( gvm, GAME_CONSOLE_COMMAND );
}


qboolean SV_SendRconLog(const char* ip, const char* command) {
    if (sv.state != SS_GAME) {
        return qfalse;
    }
    return VM_Call(gvm, GAME_RCON_LOG, ip, command);
}

