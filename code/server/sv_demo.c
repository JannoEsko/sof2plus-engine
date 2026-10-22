/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) 2008 Amanieu d'Antras
Copyright (C) 2012-2017 Stephen Larroque <lrq3000@gmail.com>

This file is part of SoF2Plus engine (based on Quake III Arena source code).

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2 of the License, or (at your
option) any later version.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

// sv_demo.c -- Server-side demo recording and playback (SoF2Plus)

/*
 * A server-side demo records the game and replays it on the server so connected players can
 * spectate the reconstructed match.
 *
 *   - Plain little-endian byte stream: no Huffman, no bit packing, no MSG or bcs chunking. Every
 *     record is a u32 length, a u8 type and a flat payload.
 *   - Entity/player state is stored as the RAW structs (entityState_t / entityShared_t /
 *     playerState_t). 
 *
 * Independent of the game being played (demo, multiprotocol)
 */

#include "server.h"

#define SVDEMO_VERSION  4               
#define SVDEMO_MAGIC    "SVDM"
#define SVDEMO_BUFSIZE  (0x400000)      // 4 MB scratch buffer (one record)
#define SVDEMO_EXTSTR   SVDEMOEXT SVDEMO_VERSIONSTR     // "svdm_4", for file masks/completion
#define SVDEMO_FILEEXT  "." SVDEMO_EXTSTR               // e.g. ".svdm_4"
#define SVDEMO_MAXREC   (256 * 1024)    // sanity limit for the metadata record

// record types: first byte of every record payload
typedef enum {
    REC_ENDDEMO = 0,        // end of the demo stream
    REC_META,               // metadata key/value pairs
    REC_CONFIGSTRING,       // u16 index + string
    REC_CLIENTCONFIGSTRING, // u8 clientNum + string
    REC_CLIENTUSERINFO,     // u8 clientNum + string
    REC_CLIENTCOMMAND,      // u8 clientNum + string
    REC_SERVERCOMMAND,      // string
    REC_GAMECOMMAND,        // u8 clientNum + string
    REC_ENTITIES,           // { u16 entNum, u8 flags, [entityState], [entityShared] } ... 0xFFFF
    REC_PLAYERSTATES,       // { u8 clientNum, playerState_t } ... 0xFF
    REC_USERCMD,            // u8 clientNum + usercmd_t, written when the command arrives
    REC_ENDFRAME            // u32 server time
} demo_rec_e;

// entity record flags
#define ENT_FLAG_STATE   1
#define ENT_FLAG_SHARED  2

/*
 * File-local state.
 */
static byte     svDemoWrBuf[SVDEMO_BUFSIZE];
static int      svDemoWrLen;

static byte     svDemoRdBuf[SVDEMO_BUFSIZE];
static int      svDemoRdLen;
static int      svDemoRdPos;
static qboolean svDemoRdOverflow;
static char     svDemoRdStr[MAX_STRING_CHARS];

// saved server configuration, restored when playback ends
static int      svDemoSavedFPS = -1;
static int      svDemoSavedDoWarmup = -1;
static int      svDemoSavedBotMinPlayers = -1;
static int      svDemoSavedAllowVote = -1;
static char     svDemoSavedFsGame[MAX_QPATH] = "";
static char     svDemoSavedGametype[MAX_QPATH] = "";
static char     svDemoSavedMap[MAX_QPATH] = "";
static qboolean svDemoHaveSaved = qfalse;

static char     svDemoPlaybackName[MAX_QPATH] = "";  // demo file being played back
static int      svDemoRecordedClients = 0;           // client/player count stored in the demo
static int      svDemoFrameCounter = -1;             // used for timescale compensation
static int      svDemoLastTime = 0;                  // last replayed server time
static int      svDemoRecordAnonLevel = 0;            // effective anonymity level for the current recording

/*
====================
Little-endian flat write helpers (record builder)
====================
*/
static void DemoW_Data( const void *p, int n ) {
    if ( n <= 0 ) {
        return;
    }
    if ( svDemoWrLen + n > (int)sizeof( svDemoWrBuf ) ) {
        Com_Error( ERR_DROP, "SV_DemoWrite: record overflow (%i bytes)", svDemoWrLen + n );
        return;
    }
    Com_Memcpy( svDemoWrBuf + svDemoWrLen, p, n );
    svDemoWrLen += n;
}

static void DemoW_Byte( int v ) {
    byte b = (byte)v;
    DemoW_Data( &b, 1 );
}

static void DemoW_Short( int v ) {
    unsigned short s = (unsigned short)v;
    DemoW_Data( &s, 2 );
}

static void DemoW_Long( int v ) {
    unsigned int l = (unsigned int)v;
    DemoW_Data( &l, 4 );
}

static void DemoW_String( const char *s ) {
    int n;

    if ( !s ) {
        s = "";
    }
    n = (int)strlen( s );
    if ( n > 65535 ) {
        n = 65535;
    }
    DemoW_Short( n );
    DemoW_Data( s, n );
}

// begin a new record (type is the first payload byte)
static void DemoW_Begin( int type ) {
    svDemoWrLen = 0;
    DemoW_Byte( type );
}

// write the finished record to the demo file: u32 length + payload
static void DemoW_Commit( void ) {
    int len;

    if ( !sv.demoFile ) {
        return;
    }
    len = LittleLong( svDemoWrLen );
    FS_Write( &len, 4, sv.demoFile );
    FS_Write( svDemoWrBuf, svDemoWrLen, sv.demoFile );
}

/*
====================
Flat read helpers (operate on the current record)
====================
*/
static void DemoR_Data( void *p, int n ) {
    if ( n <= 0 ) {
        return;
    }
    if ( svDemoRdPos + n > svDemoRdLen ) {
        svDemoRdOverflow = qtrue;
        return;
    }
    Com_Memcpy( p, svDemoRdBuf + svDemoRdPos, n );
    svDemoRdPos += n;
}

static int DemoR_Byte( void ) {
    byte b = 0;
    DemoR_Data( &b, 1 );
    return b;
}

static int DemoR_Short( void ) {
    unsigned short s = 0;
    DemoR_Data( &s, 2 );
    return s;
}

static int DemoR_Long( void ) {
    unsigned int l = 0;
    DemoR_Data( &l, 4 );
    return (int)l;
}

static char *DemoR_String( void ) {
    int n = DemoR_Short();

    if ( n < 0 ) {
        n = 0;
    }
    if ( n > MAX_STRING_CHARS - 1 ) {
        n = MAX_STRING_CHARS - 1;
    }
    if ( svDemoRdPos + n > svDemoRdLen ) {
        svDemoRdOverflow = qtrue;
        svDemoRdStr[0] = 0;
        return svDemoRdStr;
    }
    Com_Memcpy( svDemoRdStr, svDemoRdBuf + svDemoRdPos, n );
    svDemoRdStr[n] = 0;
    svDemoRdPos += n;
    return svDemoRdStr;
}

/*
====================
Word-wise delta helpers
====================
*/
#define SVDEMO_WORD_BYTES   (((int)(sizeof( playerState_t ) / 4) + 7) / 8)

static byte     svDemoMask[SVDEMO_WORD_BYTES];
static byte     svDemoEntityUsed[MAX_GENTITIES];   // entity slots that are part of the recording

static void DemoW_Delta( const void *from, const void *to, int words ) {
    const unsigned int *a = (const unsigned int *)from;
    const unsigned int *b = (const unsigned int *)to;
    int maskBytes = ( words + 7 ) / 8;
    int i;

    if ( maskBytes > (int)sizeof( svDemoMask ) ) {
        Com_Error( ERR_DROP, "SV_DemoWrite: delta too large (%i words)", words );
        return;
    }

    Com_Memset( svDemoMask, 0, maskBytes );
    for ( i = 0; i < words; i++ ) {
        if ( a[i] != b[i] ) {
            svDemoMask[i >> 3] |= (byte)( 1 << ( i & 7 ) );
        }
    }

    DemoW_Data( svDemoMask, maskBytes );
    for ( i = 0; i < words; i++ ) {
        if ( svDemoMask[i >> 3] & ( 1 << ( i & 7 ) ) ) {
            DemoW_Data( &b[i], 4 );
        }
    }
}

static void DemoR_Delta( const void *from, void *to, int words ) {
    unsigned int *b = (unsigned int *)to;
    int maskBytes = ( words + 7 ) / 8;
    int i;

    if ( maskBytes > (int)sizeof( svDemoMask ) ) {
        svDemoRdOverflow = qtrue;
        return;
    }

    // inherit everything from the previous frame, then patch the words that changed
    if ( from != to ) {
        Com_Memcpy( to, from, words * 4 );
    }
    DemoR_Data( svDemoMask, maskBytes );
    if ( svDemoRdOverflow ) {
        return;
    }
    for ( i = 0; i < words; i++ ) {
        if ( svDemoMask[i >> 3] & ( 1 << ( i & 7 ) ) ) {
            DemoR_Data( &b[i], 4 );
        }
    }
}

/*
====================
SV_DemoStoreConfigstring / SV_DemoReassertConfigstrings / SV_DemoFreeConfigstrings

The recording's configstrings must stay authoritative while replaying. The live game module keeps
running and allocates/reuses configstring slots (models, sounds, effects, ...) for its own content,
which would leave the demo's entities pointing at the wrong - or an empty - name on the spectators'
clients
====================
*/
static char *svDemoCs[MAX_CONFIGSTRINGS];
static int   svDemoCsBytes = 0;

static void SV_DemoFreeConfigstrings( void ) {
    int i;

    for ( i = 0; i < MAX_CONFIGSTRINGS; i++ ) {
        if ( svDemoCs[i] ) {
            Z_Free( svDemoCs[i] );
            svDemoCs[i] = NULL;
        }
    }
    svDemoCsBytes = 0;
}

static void SV_DemoStoreConfigstring( int index, const char *val ) {
    int len;

    if ( index < 0 || index >= MAX_CONFIGSTRINGS || !val ) {
        return;
    }
    len = (int)strlen( val ) + 1;
    if ( svDemoCsBytes + len > 512 * 1024 ) {
        return;     // safety budget; configstrings are small
    }
    if ( svDemoCs[index] ) {
        svDemoCsBytes -= (int)strlen( svDemoCs[index] ) + 1;
        Z_Free( svDemoCs[index] );
    }
    svDemoCs[index] = CopyString( val );
    svDemoCsBytes += len;
}

/*
====================
SV_DemoEnforceTeam

The recorded team lives in the game's session data, and a mod re-reads that session data whenever it
touches the client - which still says "spectator" on this server, so ClientUserinfoChanged
overwrites the player's CS_PLAYERS entry and the player is no longer spectatable/followed.
The standard "team <colour>" client command is the public way to set sess.team without utilizing NA_BOT, so whenever the game
has overwritten a recorded player's CS_PLAYERS entry we push the recorded team back. Throttled to
once a second per player so a mod's join broadcast cannot spam.
====================
*/
static int svDemoTeamTime[MAX_CLIENTS];

static void SV_DemoEnforceTeam( int num ) {
    const char *cs, *t;

    if ( !svs.clients || num < 0 || num >= sv_democlients->integer || num >= MAX_CLIENTS ) {
        return;
    }
    if ( svs.clients[num].state < CS_ACTIVE ) {
        return;
    }
    cs = svDemoCs[CS_PLAYERS + num];
    if ( !cs || !cs[0] ) {
        return;
    }
    t = Info_ValueForKey( cs, "t" );
    if ( !t[0] || ( svDemoTeamTime[num] && sv.time - svDemoTeamTime[num] < 1000 ) ) {
        return;
    }
    svDemoTeamTime[num] = sv.time;

    if ( atoi( t ) == 1 ) {             // TEAM_RED
        SV_ExecuteClientCommand( &svs.clients[num], "team red", qtrue );
    } else if ( atoi( t ) == 2 ) {      // TEAM_BLUE
        SV_ExecuteClientCommand( &svs.clients[num], "team blue", qtrue );
    }
}

static void SV_DemoReassertConfigstrings( void ) {
    int i;

    for ( i = 0; i < MAX_CONFIGSTRINGS; i++ ) {
        if ( svDemoCs[i] && Q_stricmp( sv.configstrings[i], svDemoCs[i] ) ) {
            SV_SetConfigstring( i, svDemoCs[i] );
            // A player entry only changes behind our back when the game re-read its session data,
            // which also resets that player's team - so put the team back too.
            if ( i >= CS_PLAYERS && i < CS_PLAYERS + MAX_CLIENTS ) {
                SV_DemoEnforceTeam( i - CS_PLAYERS );
            }
        }
    }
}

static qboolean svDemoUserCmdSeen[MAX_CLIENTS];   // slots already fed by recorded commands

static void SV_DemoThinkClients( void ) {
    int i;

    if ( !svs.clients ) {
        return;
    }
    for ( i = 0; i < sv_democlients->integer && i < MAX_CLIENTS; i++ ) {
        client_t *cl = &svs.clients[i];
        usercmd_t cmd;

        if ( cl->state < CS_ACTIVE ) {
            continue;
        }
        if ( svDemoUserCmdSeen[i] ) {
            continue;   // the recording supplies this slot's commands now
        }

        Com_Memset( &cmd, 0, sizeof( cmd ) );
        cmd.serverTime = sv.time;   // the value the game already has as level.time this frame
        SV_ClientThink( cl, &cmd );
    }
}


/*
====================
SV_DemoCleanFilename

Strip anything that is not [A-Za-z0-9-._] so a string can safely be used as a filename.
====================
*/
static char *SV_DemoCleanFilename( char *string ) {
    char *d = string;
    char *s = string;

    while ( *s ) {
        if ( Q_IsColorString( s ) ) {
            s++;
        } else if ( *s == '-' || *s == '.' || *s == '_' ||
                    ( *s >= '0' && *s <= '9' ) ||
                    ( *s >= 'A' && *s <= 'Z' ) ||
                    ( *s >= 'a' && *s <= 'z' ) ) {
            *d++ = *s;
        }
        s++;
    }
    *d = '\0';
    return string;
}

/*
====================
SV_DemoFileExists

Cheap existence test used to give a useful error when a demo of an older format is requested.
====================
*/
static qboolean SV_DemoFileExists( const char *filename ) {
    fileHandle_t f;

    FS_FOpenFileRead( filename, &f, qtrue );
    if ( f ) {
        FS_FCloseFile( f );
        return qtrue;
    }
    return qfalse;
}

/*
====================
SV_DemoFilterUserinfo

Apply the anonymity policy (sv_demoRecordAnonymous) to a copy of a client userinfo:
   0 = keep everything, 1 = strip network-identifying keys, 2 = also pseudonymise the name.
====================
*/
static void SV_DemoFilterUserinfo( char *userinfo, int clientNum ) {
    int level = svDemoRecordAnonLevel;

    if ( level <= 0 || !userinfo[0] ) {
        return;
    }

    Info_RemoveKey( userinfo, "ip" );
    Info_RemoveKey( userinfo, "cl_guid" );
    Info_RemoveKey( userinfo, "guid" );
    Info_RemoveKey( userinfo, "sof2guid" );
    Info_RemoveKey_Big( userinfo, "cl_voip" );

    if ( level >= 2 ) {
        Info_SetValueForKey( userinfo, "name", va( "player%d", clientNum ) );
    }
}

static char *SV_DemoGenerateDateTime( void ) {
    qtime_t now;
    time_t  utcnow;
    struct tm tnow;
    char    utcBuff[128];

    Com_RealTime( &now );

    utcnow = time( NULL );
    tnow = *gmtime( &utcnow );
    strftime( utcBuff, sizeof( utcBuff ), "UTC %Y-%m-%d %H:%M:%S", &tnow );

    return va( "%04d-%02d-%02d %02d:%02d:%02d (%s)",
               1900 + now.tm_year, 1 + now.tm_mon, now.tm_mday,
               now.tm_hour, now.tm_min, now.tm_sec, utcBuff );
}

/*
====================
SV_DemoCleanStrCmd / SV_DemoCheckLastCmd

Duplicate suppression for recorded commands (the engine sometimes emits the same command once
per client).
====================
*/
static void SV_DemoCleanStrCmd( char *string ) {
    char *d = string;
    char *s = string;

    while ( *s ) {
        if ( Q_IsColorString( s ) || ( s[0] == Q_COLOR_ESCAPE && s[1] == '\0' ) ) {
            s++;
        } else if ( *s >= 0x20 && *s <= 0x7E ) {
            *d++ = *s;
        }
        s++;
    }
    *d = '\0';
}

static qboolean SV_DemoCheckLastCmd( const char *cmd, qboolean onlyStore ) {
    static char prevCmd[MAX_STRING_CHARS] = "";
    char cleanedPrev[MAX_STRING_CHARS];
    char cleanedCur[MAX_STRING_CHARS];
    qboolean result = qtrue;

    Q_strncpyz( cleanedPrev, prevCmd, sizeof( cleanedPrev ) );
    SV_DemoCleanStrCmd( cleanedPrev );
    Q_strncpyz( cleanedCur, cmd, sizeof( cleanedCur ) );
    SV_DemoCleanStrCmd( cleanedCur );

    if ( !onlyStore && prevCmd[0] && !Q_stricmp( cleanedPrev, cleanedCur ) ) {
        result = qfalse;
    } else {
        Q_strncpyz( prevCmd, cmd, sizeof( prevCmd ) );
    }

    return result;
}

/*
====================
Write-side filters (same policy as the reference patch)
====================
*/
static qboolean SV_DemoCheckClientCommand( client_t *client, const char *cmd ) {
    if ( !Q_strncmp( cmd, "userinfo", 8 ) && strlen( cmd ) > 9 ) {
        SV_DemoWriteClientUserinfo( client, cmd + 9 );
        return qfalse;
    }
    if ( !Q_strncmp( cmd, "tell", 4 ) || !Q_strncmp( cmd, "say_team", 8 ) ) {
        return qfalse;      // private communication
    }
    return qtrue;
}

static qboolean SV_DemoCheckServerCommand( const char *cmd ) {
    if ( !Q_strncmp( cmd, "print", 5 ) || !Q_strncmp( cmd, "cp", 2 ) ||
         !Q_strncmp( cmd, "disconnect", 10 ) || !Q_strncmp( cmd, "cs ", 3 ) ) {
        return qfalse;
    }
    return qtrue;
}

static qboolean SV_DemoCheckGameCommand( const char *cmd ) {
    if ( !SV_DemoCheckLastCmd( cmd, qfalse ) ) {
        return qfalse;
    }
    if ( !Q_strncmp( cmd, "chat", 4 ) || !Q_strncmp( cmd, "tchat", 5 ) || !Q_strncmp( cmd, "vtchat", 6 ) ) {
        return qfalse;
    }
    if ( !Q_strncmp( cmd, "cs", 2 ) ) {
        Cmd_SaveCmdContext();
        Cmd_TokenizeString( cmd );
        SV_DemoWriteConfigString( atoi( Cmd_Argv( 1 ) ), Cmd_Argv( 2 ) );
        Cmd_RestoreCmdContext();
        return qfalse;
    }
    return qtrue;
}

static qboolean SV_DemoCheckConfigString( int cs_index, const char *cs_string ) {
    if ( cs_index >= CS_PLAYERS && cs_index < CS_PLAYERS + sv_maxclients->integer ) {
        // Only non-empty player configstrings are relayed: empty ones are just unused slots, and
        // leaving them out lets the replay side infer how many client slots the demo really needs.
        if ( cs_string && cs_string[0] ) {
            SV_DemoWriteClientConfigString( cs_index - CS_PLAYERS, cs_string );
        }
        return qfalse;
    }
    if ( cs_index < CS_PLAYERS ) {
        return qfalse;      // serverinfo/systeminfo are regenerated by the engine on playback
    }
    return qtrue;
}

/*
====================
SV_DemoWriteMetaData

Metadata needed to replay the demo faithfully (map/mod/gametype/limits/...).
====================
*/
/*
====================
Game-relevant cvars

These decide how the game behaves - which gametype logic runs, movement, and the mod's own gametype
reporting - and they MUST be in place *before* the replay's map is loaded, because the gametype
module and the game read them at map spawn. 
====================
*/
static const char *svDemoGameCvarNames[] = {
    "g_gametype", "g_speed", "g_gravity", "g_publicgametype", "g_spoofgametype"
};
#define SVDEMO_GAMECVAR_COUNT   ( (int)( sizeof( svDemoGameCvarNames ) / sizeof( svDemoGameCvarNames[0] ) ) )

static char     svDemoGameCvarValues[SVDEMO_GAMECVAR_COUNT][MAX_CVAR_VALUE_STRING];
static char     svDemoSavedGameCvars[SVDEMO_GAMECVAR_COUNT][MAX_CVAR_VALUE_STRING];
static qboolean svDemoHaveGameCvars = qfalse;

static void SV_DemoWriteGameCvars( void ) {
    int i;

    for ( i = 0; i < SVDEMO_GAMECVAR_COUNT; i++ ) {
        DemoW_String( svDemoGameCvarNames[i] );
        DemoW_String( Cvar_VariableString( svDemoGameCvarNames[i] ) );
    }
}

// Apply the recording's game cvars. 
static void SV_DemoApplyGameCvars( void ) {
    int i;

    for ( i = 0; i < SVDEMO_GAMECVAR_COUNT; i++ ) {
        const char *cur = Cvar_VariableString( svDemoGameCvarNames[i] );

        if ( !svDemoHaveGameCvars ) {
            Q_strncpyz( svDemoSavedGameCvars[i], cur, sizeof( svDemoSavedGameCvars[i] ) );
        }
        if ( svDemoGameCvarValues[i][0] && Q_stricmp( cur, svDemoGameCvarValues[i] ) ) {
            Com_Printf( "DEMO: setting %s \"%s\" (was \"%s\") for the replay.\n",
                        svDemoGameCvarNames[i], svDemoGameCvarValues[i],
                        svDemoHaveGameCvars ? svDemoSavedGameCvars[i] : cur );
            Cvar_Set( svDemoGameCvarNames[i], svDemoGameCvarValues[i] );
        }
    }
    svDemoHaveGameCvars = qtrue;
}

static void SV_DemoRestoreGameCvars( void ) {
    int i;

    if ( !svDemoHaveGameCvars ) {
        return;
    }
    for ( i = 0; i < SVDEMO_GAMECVAR_COUNT; i++ ) {
        if ( svDemoSavedGameCvars[i][0] ) {
            Cvar_Set( svDemoGameCvarNames[i], svDemoSavedGameCvars[i] );
        }
    }
    svDemoHaveGameCvars = qfalse;
}

static void SV_DemoWriteMetaData( void ) {
    DemoW_Begin( REC_META );

    DemoW_String( "map" );
    DemoW_String( sv_mapname->string );
    DemoW_String( "gametype" );
    DemoW_String( sv_gametype->string );
    DemoW_String( "fs_game" );
    DemoW_String( Cvar_VariableString( "fs_game" ) );
    DemoW_String( "clients" );
    DemoW_Long( sv_maxclients->integer );
    DemoW_String( "sv_fps" );
    DemoW_Long( sv_fps->integer );
    DemoW_String( "servertime" );
    DemoW_Long( sv.time );
    // the demo stores raw structs: record their sizes so a build that changes them is rejected
    DemoW_String( "entsize" );
    DemoW_Long( sizeof( entityState_t ) );
    DemoW_String( "shrdsize" );
    DemoW_Long( sizeof( entityShared_t ) );
    DemoW_String( "pssize" );
    DemoW_Long( sizeof( playerState_t ) );
    DemoW_String( "usercmdsize" );
    DemoW_Long( sizeof( usercmd_t ) );
    DemoW_String( "hostname" );
    DemoW_String( sv_hostname->string );
    DemoW_String( "datetime" );
    DemoW_String( SV_DemoGenerateDateTime() );
    // the game settings the replay has to run with (applied before the map is loaded on playback)
    SV_DemoWriteGameCvars();

    DemoW_String( "endMeta" );

    DemoW_Commit();
}

/*
====================
Record writers. These are called from engine hooks while recording.
====================
*/
void SV_DemoWriteConfigString( int cs_index, const char *cs_string ) {
    if ( sv.demoState != DS_RECORDING || !sv.demoFile ) {
        return;
    }
    if ( !SV_DemoCheckConfigString( cs_index, cs_string ) ) {
        return;
    }

    DemoW_Begin( REC_CONFIGSTRING );
    DemoW_Short( cs_index );
    DemoW_String( cs_string );
    DemoW_Commit();
}

void SV_DemoWriteClientConfigString( int clientNum, const char *cs_string ) {
    if ( sv.demoState != DS_RECORDING || !sv.demoFile ) {
        return;
    }
    if ( clientNum < 0 || clientNum >= MAX_CLIENTS ) {
        return;
    }

    DemoW_Begin( REC_CLIENTCONFIGSTRING );
    DemoW_Byte( clientNum );
    DemoW_String( cs_string );
    DemoW_Commit();
}

void SV_DemoWriteClientUserinfo( client_t *client, const char *userinfo ) {
    static char filtered[MAX_STRING_CHARS];
    int clientNum;

    if ( sv.demoState != DS_RECORDING || !sv.demoFile ) {
        return;
    }
    if ( !userinfo ) {
        userinfo = "";
    }

    clientNum = client - svs.clients;
    Q_strncpyz( filtered, userinfo, sizeof( filtered ) );
    SV_DemoFilterUserinfo( filtered, clientNum );

    DemoW_Begin( REC_CLIENTUSERINFO );
    DemoW_Byte( clientNum );
    DemoW_String( filtered );
    DemoW_Commit();
}

void SV_DemoWriteClientCommand( client_t *client, const char *cmd ) {
    if ( sv.demoState != DS_RECORDING || !sv.demoFile ) {
        return;
    }
    if ( !SV_DemoCheckClientCommand( client, cmd ) ) {
        return;
    }

    DemoW_Begin( REC_CLIENTCOMMAND );
    DemoW_Byte( client - svs.clients );
    DemoW_String( cmd );
    DemoW_Commit();
}

/*
====================
SV_DemoWriteUserCmd

Called from SV_ClientThink - i.e. for exactly the commands the game module is about to consume, after
SV_UserMove has already dropped the cl_packetdup duplicates. Recording them here (rather than
sampling the player each frame) is what lets a replay hand the game the same input the live game got:
movement, antilag, triggers and clock checks such as g_timeouttospec all see real data. Bots arrive
through this same hook.
====================
*/
void SV_DemoWriteUserCmd( client_t *client, usercmd_t *cmd ) {
    int num;

    if ( sv.demoState != DS_RECORDING || !sv.demoFile || !cmd ) {
        return;
    }
    num = client - svs.clients;
    if ( num < 0 || num >= MAX_CLIENTS ) {
        return;
    }

    DemoW_Begin( REC_USERCMD );
    DemoW_Byte( num );
    DemoW_Delta( &sv.demoUserCmds[num], cmd, sizeof( usercmd_t ) / 4 );
    sv.demoUserCmds[num] = *cmd;        // remember for the next delta
    DemoW_Commit();
}

void SV_DemoWriteServerCommand( const char *cmd ) {
    if ( sv.demoState != DS_RECORDING || !sv.demoFile ) {
        return;
    }
    if ( !SV_DemoCheckServerCommand( cmd ) ) {
        return;
    }
    // avoid double-recording commands the game module already emitted (SV_GameSendServerCommand)
    if ( !SV_DemoCheckLastCmd( cmd, qfalse ) ) {
        return;
    }

    DemoW_Begin( REC_SERVERCOMMAND );
    DemoW_String( cmd );
    DemoW_Commit();
}

void SV_DemoWriteGameCommand( int clientNum, const char *cmd ) {
    if ( sv.demoState != DS_RECORDING || !sv.demoFile ) {
        return;
    }
    if ( !SV_DemoCheckGameCommand( cmd ) ) {
        return;
    }

    DemoW_Begin( REC_GAMECOMMAND );
    DemoW_Byte( clientNum );
    DemoW_String( cmd );
    DemoW_Commit();
}

/*
====================
SV_DemoWriteEntities

Writes the raw entityState_t / entityShared_t of every entity that changed since the previous
frame (whole-struct memcmp), terminated by a 0xFFFF entity number. Unused client slots
([sv_maxclients, MAX_CLIENTS)) are skipped, like the snapshot code does.
====================
*/
static void SV_DemoWriteEntities( void ) {
    sharedEntity_t *entity;
    int i;

    DemoW_Begin( REC_ENTITIES );

    for ( i = 0; i < sv.num_entities; i++ ) {
        if ( i >= sv_maxclients->integer && i < MAX_CLIENTS ) {
            continue;
        }
        entity = SV_GentityNum( i );
        entity->s.number = i;

        {
            qboolean stateChanged = ( memcmp( &sv.demoEntities[i].s, &entity->s, sizeof( entityState_t ) ) != 0 );
            qboolean sharedChanged = ( memcmp( &sv.demoEntities[i].r, &entity->r, sizeof( entityShared_t ) ) != 0 );

            if ( stateChanged || sharedChanged ) {
                DemoW_Short( i );
                DemoW_Byte( ( stateChanged ? ENT_FLAG_STATE : 0 ) | ( sharedChanged ? ENT_FLAG_SHARED : 0 ) );
                if ( stateChanged ) {
                    DemoW_Delta( &sv.demoEntities[i].s, &entity->s, sizeof( entityState_t ) / 4 );
                }
                if ( sharedChanged ) {
                    DemoW_Delta( &sv.demoEntities[i].r, &entity->r, sizeof( entityShared_t ) / 4 );
                }
                sv.demoEntities[i] = *entity;   // remember for the next frame's delta
            }
        }
    }

    DemoW_Short( 0xFFFF );      // end marker
    DemoW_Commit();
}

/*
====================
SV_DemoWritePlayerStates

Writes the playerState_t of every active client whose state changed, terminated by a 0xFF client
number. Unchanged states are simply left out (the reader keeps the previous one).
====================
*/
static void SV_DemoWritePlayerStates( void ) {
    playerState_t *ps;
    int i;

    DemoW_Begin( REC_PLAYERSTATES );

    for ( i = 0; i < sv_maxclients->integer; i++ ) {
        if ( svs.clients[i].state < CS_ACTIVE ) {
            continue;
        }
        ps = SV_GameClientNum( i );
        if ( memcmp( &sv.demoPlayerStates[i], ps, sizeof( playerState_t ) ) == 0 ) {
            continue;
        }
        DemoW_Byte( i );
        DemoW_Delta( &sv.demoPlayerStates[i], ps, sizeof( playerState_t ) / 4 );
        sv.demoPlayerStates[i] = *ps;   // remember for the next frame's delta
    }

    DemoW_Byte( 0xFF );         // end marker
    DemoW_Commit();
}

/*
====================
SV_DemoWriteFrame

Record one whole demo frame. Called once per server frame while recording.
====================
*/
void SV_DemoWriteFrame( void ) {
    if ( sv.demoState != DS_RECORDING || !sv.demoFile ) {
        return;
    }

    SV_DemoWriteEntities();
    SV_DemoWritePlayerStates();

    DemoW_Begin( REC_ENDFRAME );
    DemoW_Long( sv.time );
    DemoW_Commit();
}

/*
====================
Read-side replay helpers. Each reads/replays one record payload.
====================
*/
static void SV_DemoReadConfigString( void ) {
    int num = DemoR_Short();
    const char *cs = DemoR_String();

    if ( num < 0 || num >= MAX_CONFIGSTRINGS ) {
        return;
    }
    // don't overwrite the democlients' player configstrings (handled via client configstring)
    if ( num < CS_PLAYERS + sv_democlients->integer || num >= CS_PLAYERS + sv_maxclients->integer ) {
        SV_SetConfigstring( num, cs );
        SV_DemoStoreConfigstring( num, cs );     // keep the recording's value authoritative
    }
}

static void SV_DemoReadClientConfigString( void ) {
    const char *cs;
    int num = DemoR_Byte();

    cs = DemoR_String();

    if ( num < 0 || num >= sv_maxclients->integer ) {
        return;
    }

    if ( cs[0] && Q_stricmp( sv.configstrings[CS_PLAYERS + num], cs ) ) {
        SV_SetConfigstring( CS_PLAYERS + num, cs );
    }
    SV_DemoStoreConfigstring( CS_PLAYERS + num, cs );

    // The recorded player still has to be a real game client, because spectating a specific player
    // ("follow") works through the game's own level.clients[]/gentity.client - which only exist for
    // clients the game knows about. Keeping it harmless is done elsewhere: its CS_PLAYERS entry is
    // kept authoritative (above), its playerState_t is mirrored from the recording every frame so
    // the game never sees it as garbage/dead, and its entities are kept non-solid so the game's
    // traces never dereference the game-side pointers a demo cannot carry.
    if ( cs[0] ) {
        client_t *client = &svs.clients[num];

        client->demoClient = qtrue;     // simulated and relayed to others, never transmitted to

        // Build the player the same way a real connection does. Follow/spectate works through the
        // game's own level.clients[]/gentity.client, and the game only creates those in
        // GAME_CLIENT_CONNECT - ClientBegin alone is not enough (it also leaves the client's
        // pers.connected/sess state unset, which is what makes "follow" refuse a player). The
        // userinfo was replayed beforehand and CS_PLAYERS is set above, so the game builds the
        // recorded identity.
        if ( client->state < CS_CONNECTED ) {
            NET_StringToAdr( "localhost", &client->netchan.remoteAddress, NA_LOOPBACK );
            client->state = CS_CONNECTED;
            client->gentity = SV_GentityNum( num );
            VM_Call( gvm, GAME_CLIENT_CONNECT, num, qtrue, qfalse );
        }
        if ( client->state < CS_ACTIVE ) {
            client->state = CS_ACTIVE;
            client->deltaMessage = -1;
            client->lastSnapshotTime = 0;
            client->commProto = COMMPROTO_GOLD;
            VM_Call( gvm, GAME_CLIENT_BEGIN, num );
        }

        // Put the recorded player back on its recorded team (see SV_DemoEnforceTeam). This is the
        // first enforcement; the configstring re-assert re-applies it whenever the game's own
        // session data overwrites the player's CS_PLAYERS entry.
        svDemoTeamTime[num] = 0;
        SV_DemoEnforceTeam( num );
    }
}

static void SV_DemoReadClientUserinfo( void ) {
    const char *userinfo;
    int num;

    Cmd_SaveCmdContext();
    num = DemoR_Byte();
    userinfo = DemoR_String();

    // Recorded players' userinfo has to be in place as well: the game reads it through
    // trap_GetUserinfo() when it creates the client for the slot (see SV_DemoReadClientConfigString).
    if ( num >= 0 && num < sv_maxclients->integer && userinfo[0] ) {
        client_t *client = &svs.clients[num];

        if ( num < sv_democlients->integer ) {
            // emulated player: keep it "local" so nothing ever tries to talk to it
            NET_StringToAdr( "localhost", &client->netchan.remoteAddress, NA_LOOPBACK );
        }
        Q_strncpyz( client->userinfo, userinfo, sizeof( client->userinfo ) );
        SV_UserinfoChanged( client );
        if ( client->state >= CS_ACTIVE ) {
            VM_Call( gvm, GAME_CLIENT_USERINFO_CHANGED, num );
        }
    }

    Cmd_RestoreCmdContext();
}

static void SV_DemoReadClientCommand( void ) {
    char cmd[MAX_STRING_CHARS];
    int num;

    Cmd_SaveCmdContext();
    num = DemoR_Byte();
    Q_strncpyz( cmd, DemoR_String(), sizeof( cmd ) );

    // Recorded player commands must be replayed: this is where the recorded chat ("say ...") and
    // the players' own commands live, and it is what makes a replay look like the live game.
    if ( num >= 0 && num < sv_maxclients->integer ) {
        SV_ExecuteClientCommand( &svs.clients[num], cmd, qtrue );
    }

    Cmd_RestoreCmdContext();
}

static void SV_DemoReadServerCommand( void ) {
    char cmd[MAX_STRING_CHARS];

    Cmd_SaveCmdContext();
    Q_strncpyz( cmd, DemoR_String(), sizeof( cmd ) );
    SV_SendServerCommand( NULL, "%s", cmd );
    Cmd_RestoreCmdContext();
}

static void SV_DemoReadGameCommand( void ) {
    char cmd[MAX_STRING_CHARS];

    Cmd_SaveCmdContext();
    DemoR_Byte();   // clientNum, unused: game commands are always broadcast
    Q_strncpyz( cmd, DemoR_String(), sizeof( cmd ) );
    if ( SV_DemoCheckLastCmd( cmd, qfalse ) ) {
        SV_GameSendServerCommand( -1, cmd );
    }
    Cmd_RestoreCmdContext();
}

/*
====================
SV_DemoReadEntities

Restore the raw entity state read from the REC_ENTITIES record.
====================
*/
static void SV_DemoReadEntities( void ) {
    sharedEntity_t *entity;
    int num, flags;

    for ( ;; ) {
        num = DemoR_Short();
        if ( num == 0xFFFF || num < 0 || num >= MAX_GENTITIES ) {
            break;
        }
        flags = DemoR_Byte();
        entity = SV_GentityNum( num );

        if ( flags & ENT_FLAG_STATE ) {
            DemoR_Delta( &sv.demoEntities[num].s, &entity->s, sizeof( entityState_t ) / 4 );
            entity->s.number = num;

            // Avoid the "bad moverstate" error: never let a mover reach TR_LINEAR_STOP (the game
            // owns the mover state, which we cannot replay, so keep movers perpetually linear).
            if ( entity->s.eType == ET_MOVER && entity->s.pos.trType == TR_LINEAR_STOP ) {
                entity->s.pos.trType = TR_LINEAR;
            }
        } else {
            entity->s = sv.demoEntities[num].s;   // unchanged this frame: keep the recorded state
        }

        if ( flags & ENT_FLAG_SHARED ) {
            DemoR_Delta( &sv.demoEntities[num].r, &entity->r, sizeof( entityShared_t ) / 4 );
            entity->r.svFlags &= ~SVF_BOT;   // replay as a normal player, not a bot

            if ( entity->r.linked && ( !sv.demoEntities[num].r.linked ||
                 entity->r.linkcount != sv.demoEntities[num].r.linkcount ) ) {
                SV_LinkEntity( entity );
            } else if ( !entity->r.linked && sv.demoEntities[num].r.linked ) {
                SV_UnlinkEntity( entity );
            }

            if ( num >= sv.num_entities ) {
                sv.num_entities = num + 1;
            }
        } else {
            entity->r = sv.demoEntities[num].r;   // unchanged this frame: keep the recorded state
        }

        sv.demoEntities[num] = *entity;
        svDemoEntityUsed[num] = qtrue;

        if ( svDemoRdOverflow ) {
            break;
        }
    }
}

/*
====================
SV_DemoReadPlayerStates

Restore the raw player states read from the REC_PLAYERSTATES record.
====================
*/
static void SV_DemoReadPlayerStates( void ) {
    int num;

    for ( ;; ) {
        num = DemoR_Byte();
        if ( num == 0xFF || num < 0 || num >= MAX_CLIENTS ) {
            break;
        }
        // patch the stored state in place (from == to, so this reads a pure delta)
        DemoR_Delta( &sv.demoPlayerStates[num], &sv.demoPlayerStates[num], sizeof( playerState_t ) / 4 );
        if ( svDemoRdOverflow ) {
            break;
        }
    }
}

/*
====================
SV_DemoReadUserCmd

Hand the game the recorded input. SV_ClientThink is the same entry point the live server uses, so the
mod's pers.cmd/movement/antilag/triggers see exactly what they saw when the demo was recorded, at the
same point in the frame (commands are recorded between frame records, so the per-frame order is kept).
The recorded playerstate is restored every frame as well, so pmove always starts from the recorded
state and cannot accumulate noticeable drift, apart from minor discrepancies.
====================
*/
static void SV_DemoReadUserCmd( void ) {
    int num;

    Cmd_SaveCmdContext();
    num = DemoR_Byte();

    // patch the stored command in place (from == to, so this reads a pure delta)
    if ( num >= 0 && num < sv_maxclients->integer ) {
        DemoR_Delta( &sv.demoUserCmds[num], &sv.demoUserCmds[num], sizeof( usercmd_t ) / 4 );
        if ( !svDemoRdOverflow && svs.clients[num].state >= CS_ACTIVE ) {
            SV_ClientThink( &svs.clients[num], &sv.demoUserCmds[num] );
            svDemoUserCmdSeen[num] = qtrue;
        }
    }

    Cmd_RestoreCmdContext();
}

/*
====================
SV_DemoReadRefreshEntities

Overwrite whatever the game may have changed. Called at the end of every replayed frame (the game
runs right before us in SV_Frame, so we must restore the recorded state last).
====================
*/
static void SV_DemoReadRefreshEntities( void ) {
    int i;

    for ( i = 0; i < sv.num_entities && i < MAX_GENTITIES; i++ ) {
        // never touch an entity the recording does not contain (the game may have created its own)
        if ( !svDemoEntityUsed[i] ) {
            continue;
        }
        // slots reserved for real (spectating) clients belong to the live game, not to the demo
        if ( i >= sv_democlients->integer && i < MAX_CLIENTS ) {
            continue;
        }
        *SV_GentityNum( i ) = sv.demoEntities[i];
        // An item entity's game-side pointers (gitem_t *item, ...) cannot be recorded, so a mod that
        // dereferences them from a use-check would crash on a restored item. Keeping *only items*
        // non-solid makes the game's traces skip them while they stay linked - and therefore still
        // visible. 
        if ( SV_GentityNum( i )->s.eType == ET_ITEM ) {
            SV_GentityNum( i )->r.contents = 0;
        }
    }

    for ( i = 0; i < sv_democlients->integer && i < sv_maxclients->integer; i++ ) {
        playerState_t *ps = SV_GameClientNum( i );

        *ps = sv.demoPlayerStates[i];
        // What spectators see comes from the entity refresh above, not from this playerState_t, so it
        // is safe to keep the game from acting on a recorded death: otherwise the game's per-client
        // frame respawns that player every single frame (spamming kills and spawn events), and its
        // spectate "follow" code refuses to follow a client it believes is dead.
        ps->pm_type = PM_NORMAL;
        ps->stats[STAT_HEALTH] = 100;
        ps->eFlags &= ~EF_DEAD;
        svs.clients[i].lastPacketTime = svs.time;   // keep emulated clients from timing out
    }
}

/*
====================
SV_DemoReadFrame

Play back one demo frame per call. Reads records until REC_ENDFRAME. Called at the end of SV_Frame.
====================
*/
void SV_DemoReadFrame( void ) {
    int type, len;

    if ( sv.demoState != DS_PLAYBACK ) {
        return;
    }

read_next_demo_frame:

    if ( sv_demoFreeze && sv_demoFreeze->integer ) {
        sv.time = svDemoLastTime;   // hold the clock so clients don't get kicked for going backwards
        return;
    }

    svDemoFrameCounter++;

    // slow motion: only read a frame every 1/timescale server frames
    if ( com_timescale->value > 0.0f && com_timescale->value < 1.0f ) {
        if ( ( svDemoFrameCounter % (int)( 1.0f / com_timescale->value ) ) != 0 ) {
            return;
        }
    }

    for ( ;; ) {
        if ( FS_Read( &len, 4, sv.demoFile ) != 4 ) {
            Com_Printf( "DEMO: end of demo file reached.\n" );
            SV_DemoStopPlayback();
            return;
        }
        len = LittleLong( len );
        if ( len < 1 || len > (int)sizeof( svDemoRdBuf ) ) {
            Com_Printf( "DEMOERROR: bad demo record length (%i).\n", len );
            SV_DemoStopPlayback();
            return;
        }
        if ( FS_Read( svDemoRdBuf, len, sv.demoFile ) != len ) {
            Com_Printf( "DEMOERROR: demo file truncated.\n" );
            SV_DemoStopPlayback();
            return;
        }
        svDemoRdLen = len;
        svDemoRdPos = 0;
        svDemoRdOverflow = qfalse;

        type = DemoR_Byte();

        switch ( type ) {
        case REC_META:               // already parsed at the start of playback
            break;
        case REC_CONFIGSTRING:       SV_DemoReadConfigString();       break;
        case REC_CLIENTCONFIGSTRING: SV_DemoReadClientConfigString(); break;
        case REC_CLIENTUSERINFO:     SV_DemoReadClientUserinfo();     break;
        case REC_CLIENTCOMMAND:      SV_DemoReadClientCommand();      break;
        case REC_SERVERCOMMAND:      SV_DemoReadServerCommand();      break;
        case REC_GAMECOMMAND:        SV_DemoReadGameCommand();        break;
        case REC_ENTITIES:           SV_DemoReadEntities();           break;
        case REC_PLAYERSTATES:       SV_DemoReadPlayerStates();       break;
        case REC_USERCMD:            SV_DemoReadUserCmd();            break;
        case REC_ENDFRAME:
            // Keep each recorded player's command clock current before anything else: mods use it
            // (g_timeouttospec) to decide a client has gone away.
            SV_DemoThinkClients();
            // the recording's configstrings and game cvars win over anything the game module just
            // wrote (a mod re-registers g_gametype/g_publicgametype when it loads, and re-reads its
            // own session data, so both have to be pushed back every frame)
            SV_DemoReassertConfigstrings();
            SV_DemoApplyGameCvars();
            SV_DemoReadRefreshEntities();
            sv.time = DemoR_Long();
            svDemoLastTime = sv.time;
            // fast forward: skip ahead without broadcasting the skipped frames
            if ( com_timescale->value > 1.0f ) {
                if ( ( svDemoFrameCounter % (int)com_timescale->value ) != 0 ) {
                    goto read_next_demo_frame;
                }
            }
            return;
        case REC_ENDDEMO:
            Com_Printf( "DEMO: end of demo reached.\n" );
            SV_DemoStopPlayback();
            return;
        default:
            if ( sv_demoTolerant && sv_demoTolerant->integer ) {
                break;      // len framing lets us safely skip records we do not understand
            }
            Com_Printf( "DEMOERROR: unknown demo record type (%i).\n", type );
            SV_DemoStopPlayback();
            return;
        }

        if ( svDemoRdOverflow && ( !sv_demoTolerant || !sv_demoTolerant->integer ) ) {
            Com_Printf( "DEMOERROR: demo record %i is malformed.\n", type );
            SV_DemoStopPlayback();
            return;
        }
    }
}

/*
 * Playback metadata, read from the demo file before replaying.
 */
static char     svDemoRecMap[MAX_QPATH];
static char     svDemoRecGametype[MAX_QPATH];
static char     svDemoRecFsGame[MAX_QPATH];
static char     svDemoRecHostname[MAX_STRING_CHARS];
static char     svDemoRecDate[MAX_STRING_CHARS];
static int      svDemoRecFps = 20;
static int      svDemoRecTime = 0;
static qboolean svDemoPlaybackPending = qfalse;

/*
====================
SV_DemoSetState / SV_DemoIsRecording / SV_DemoIsPlaying / SV_DemoGetState
====================
*/
static void SV_DemoSetState( demoState_t state ) {
    sv.demoState = state;
    if ( sv_demoState ) {
        Cvar_Set( "sv_demoState", va( "%i", state ) );
    }
}

qboolean SV_DemoIsRecording( void ) {
    return ( sv.demoState == DS_RECORDING );
}

qboolean SV_DemoIsPlaying( void ) {
    return ( sv.demoState == DS_PLAYBACK );
}

demoState_t SV_DemoGetState( void ) {
    return sv.demoState;
}

/*
====================
SV_DemoReservedClients

Number of client slots that must stay free for the emulated players of a server-side demo. Zero
unless a replay is pending or running. Used by SV_DirectConnect() so a real (spectating) client
can never be given a slot that belongs to an emulated demo player.
====================
*/
int SV_DemoReservedClients( void ) {
    if ( svDemoPlaybackPending || sv.demoState == DS_PLAYBACK ) {
        return sv_democlients ? sv_democlients->integer : 0;
    }
    return 0;
}

/*
====================
SV_DemoRecordStart

Begin recording the current game to <name>.<demo extension> (or to "<map>_<datetime>.<demo extension>"
when name is empty). Returns qfalse on failure.
====================
*/
qboolean SV_DemoRecordStart( const char *name, qboolean anonymous ) {
    char filename[MAX_OSPATH];
    char base[MAX_QPATH];
    int  i;

    if ( sv.demoState != DS_NONE ) {
        Com_Printf( "DEMO: already recording or replaying a demo.\n" );
        return qfalse;
    }
    if ( sv.state != SS_GAME ) {
        Com_Printf( "DEMO: no game running.\n" );
        return qfalse;
    }

    if ( name && name[0] ) {
        Q_strncpyz( base, name, sizeof( base ) );
        Com_sprintf( filename, sizeof( filename ), "%s%s",
                     SV_DemoCleanFilename( base ), SVDEMO_FILEEXT );
    } else {
        qtime_t curtime;

        Com_RealTime( &curtime );
        Q_strncpyz( base, sv_mapname->string, sizeof( base ) );
        Com_sprintf( filename, sizeof( filename ), "%s_%04d-%02d-%02d_%02d-%02d-%02d%s",
                     SV_DemoCleanFilename( base ),
                     1900 + curtime.tm_year, 1 + curtime.tm_mon, curtime.tm_mday,
                     curtime.tm_hour, curtime.tm_min, curtime.tm_sec, SVDEMO_FILEEXT );
    }

    sv.demoFile = FS_FOpenFileWrite( filename );
    if ( !sv.demoFile ) {
        Com_Printf( "DEMOERROR: could not create %s.\n", filename );
        return qfalse;
    }

    Q_strncpyz( sv.demoName, filename, sizeof( sv.demoName ) );

    // effective anonymity for this recording (a call may force it over the cvar)
    svDemoRecordAnonLevel = anonymous ? 2
        : ( sv_demoRecordAnonymous ? sv_demoRecordAnonymous->integer : 0 );

    // file header: magic + format version
    FS_Write( SVDEMO_MAGIC, 4, sv.demoFile );
    {
        int version = LittleLong( SVDEMO_VERSION );
        FS_Write( &version, 4, sv.demoFile );
    }

    // from now on the engine hooks append records to the file
    SV_DemoSetState( DS_RECORDING );

    SV_DemoWriteMetaData();

    // userinfo first: the client configstrings, below, trigger ClientBegin on playback and that
    // must already see the recorded userinfo
    for ( i = 0; i < sv_maxclients->integer; i++ ) {
        if ( svs.clients[i].state ) {
            SV_DemoWriteClientUserinfo( &svs.clients[i], svs.clients[i].userinfo );
        }
    }
    for ( i = 0; i < MAX_CONFIGSTRINGS; i++ ) {
        if ( sv.configstrings[i][0] ) {
            SV_DemoWriteConfigString( i, sv.configstrings[i] );
        }
    }

    // baseline for the per-frame entity/playerstate/usercmd deltas
    Com_Memset( sv.demoEntities, 0, sizeof( sv.demoEntities ) );
    Com_Memset( sv.demoPlayerStates, 0, sizeof( sv.demoPlayerStates ) );
    Com_Memset( sv.demoUserCmds, 0, sizeof( sv.demoUserCmds ) );

    Com_Printf( "DEMO: recording %s (%s, up to %i clients)\n",
                filename, SV_DemoGenerateDateTime(), sv_maxclients->integer );
    return qtrue;
}

/*
====================
SV_DemoRecordStop
====================
*/
void SV_DemoRecordStop( void ) {
    if ( sv.demoState != DS_RECORDING ) {
        return;
    }

    DemoW_Begin( REC_ENDDEMO );     // end-of-demo marker
    DemoW_Commit();

    SV_DemoSetState( DS_NONE );

    if ( sv.demoFile ) {
        FS_FCloseFile( sv.demoFile );
        sv.demoFile = 0;
    }

    Com_Printf( "DEMO: stopped recording %s.\n", sv.demoName );
    sv.demoName[0] = 0;
}

// alias, kept for the engine/game facing API
void SV_DemoStopRecord( void ) {
    SV_DemoRecordStop();
}

/*
====================
SV_DemoReadMeta

Open a demo, validate its header and parse the metadata record, then close it again. Fills the
recorded map/mod/gametype/fps used to reconfigure the server before replaying.
====================
*/
static qboolean SV_DemoReadMeta( const char *filename ) {
    fileHandle_t f;
    char magic[4];
    int  version, reclen, type, used;
    int  entsize = 0, shrdsize = 0, pssize = 0, usercmdsize = 0;
    char *key;

    svDemoRecMap[0] = 0;
    svDemoRecGametype[0] = 0;
    svDemoRecFsGame[0] = 0;
    svDemoRecHostname[0] = 0;
    svDemoRecDate[0] = 0;
    svDemoRecFps = 20;
    svDemoRecTime = 0;
    svDemoRecordedClients = 0;

    FS_FOpenFileRead( filename, &f, qtrue );
    if ( !f ) {
        Com_Printf( "DEMOERROR: cannot open %s.\n", filename );
        return qfalse;
    }

    if ( FS_Read( magic, 4, f ) != 4 || memcmp( magic, SVDEMO_MAGIC, 4 ) ) {
        Com_Printf( "DEMOERROR: %s is not a server-side demo file.\n", filename );
        FS_FCloseFile( f );
        return qfalse;
    }
    FS_Read( &version, 4, f );
    version = LittleLong( version );
    if ( version != SVDEMO_VERSION ) {
        Com_Printf( "DEMOERROR: %s uses demo format %i, this engine writes/reads format %i.\n",
                    filename, version, SVDEMO_VERSION );
        FS_FCloseFile( f );
        return qfalse;
    }

    if ( FS_Read( &reclen, 4, f ) != 4 ) {
        Com_Printf( "DEMOERROR: %s is truncated.\n", filename );
        FS_FCloseFile( f );
        return qfalse;
    }
    reclen = LittleLong( reclen );
    if ( reclen < 1 || reclen > (int)sizeof( svDemoRdBuf ) ) {
        Com_Printf( "DEMOERROR: %s has a bad metadata record.\n", filename );
        FS_FCloseFile( f );
        return qfalse;
    }
    if ( FS_Read( svDemoRdBuf, reclen, f ) != reclen ) {
        Com_Printf( "DEMOERROR: %s is truncated.\n", filename );
        FS_FCloseFile( f );
        return qfalse;
    }
    svDemoRdLen = reclen;
    svDemoRdPos = 0;
    svDemoRdOverflow = qfalse;

    type = DemoR_Byte();
    if ( type != REC_META ) {
        Com_Printf( "DEMOERROR: %s does not start with metadata.\n", filename );
        FS_FCloseFile( f );
        return qfalse;
    }

    // key/value pairs until "endMeta"; unknown keys are stored as strings
    for ( ;; ) {
        key = DemoR_String();
        if ( !key[0] || !Q_stricmp( key, "endMeta" ) ) {
            break;
        }
        if ( !Q_stricmp( key, "map" ) ) {
            Q_strncpyz( svDemoRecMap, DemoR_String(), sizeof( svDemoRecMap ) );
        } else if ( !Q_stricmp( key, "gametype" ) ) {
            Q_strncpyz( svDemoRecGametype, DemoR_String(), sizeof( svDemoRecGametype ) );
        } else if ( !Q_stricmp( key, "fs_game" ) ) {
            Q_strncpyz( svDemoRecFsGame, DemoR_String(), sizeof( svDemoRecFsGame ) );
        } else if ( !Q_stricmp( key, "hostname" ) ) {
            Q_strncpyz( svDemoRecHostname, DemoR_String(), sizeof( svDemoRecHostname ) );
        } else if ( !Q_stricmp( key, "datetime" ) ) {
            Q_strncpyz( svDemoRecDate, DemoR_String(), sizeof( svDemoRecDate ) );
        } else if ( !Q_stricmp( key, "clients" ) ) {
            svDemoRecordedClients = DemoR_Long();
        } else if ( !Q_stricmp( key, "sv_fps" ) ) {
            svDemoRecFps = DemoR_Long();
        } else if ( !Q_stricmp( key, "servertime" ) ) {
            svDemoRecTime = DemoR_Long();
        } else if ( !Q_stricmp( key, "entsize" ) ) {
            entsize = DemoR_Long();
        } else if ( !Q_stricmp( key, "shrdsize" ) ) {
            shrdsize = DemoR_Long();
        } else if ( !Q_stricmp( key, "pssize" ) ) {
            pssize = DemoR_Long();
        } else if ( !Q_stricmp( key, "usercmdsize" ) ) {
            usercmdsize = DemoR_Long();
        } else {
            int g;

            // a recorded game cvar (see SV_DemoWriteGameCvars)
            for ( g = 0; g < SVDEMO_GAMECVAR_COUNT; g++ ) {
                if ( !Q_stricmp( key, svDemoGameCvarNames[g] ) ) {
                    Q_strncpyz( svDemoGameCvarValues[g], DemoR_String(), sizeof( svDemoGameCvarValues[g] ) );
                    break;
                }
            }
            if ( g == SVDEMO_GAMECVAR_COUNT ) {
                DemoR_String();     // unknown key: skip its (string) value
            }
        }
        if ( svDemoRdOverflow ) {
            break;
        }
    }

    // A recording made before the game-cvar block existed carries only the gametype: fall back to
    // it, so such a demo still replays under the right gametype logic instead of the server's own.
    if ( !svDemoGameCvarValues[0][0] && svDemoRecGametype[0] ) {
        Q_strncpyz( svDemoGameCvarValues[0], svDemoRecGametype, sizeof( svDemoGameCvarValues[0] ) );
        Q_strncpyz( svDemoGameCvarValues[3], svDemoRecGametype, sizeof( svDemoGameCvarValues[3] ) );
        Com_Printf( "DEMO: note: %s predates game-cvar recording, only the gametype (%s) can be restored - re-record for g_speed/g_gravity.\n",
                    filename, svDemoRecGametype );
    }

    // the file stores raw structs: refuse a demo written by a build whose structs differ
    if ( entsize != (int)sizeof( entityState_t ) ||
         shrdsize != (int)sizeof( entityShared_t ) ||
         pssize != (int)sizeof( playerState_t ) ||
         usercmdsize != (int)sizeof( usercmd_t ) ) {
        Com_Printf( "DEMOERROR: %s was recorded by an incompatible build (raw struct sizes differ).\n",
                    filename );
        FS_FCloseFile( f );
        return qfalse;
    }

    // Walk the record stream to find how many client slots the demo actually uses. The metadata
    // records the slot count the server was configured with, but a demo recorded on a large server
    // with a handful of players must not force all those unused slots to be reserved on replay
    // (the recorded players always occupy the first slots).
    used = 0;
    for ( ;; ) {
        int consumed = 1;   // the record type byte

        if ( FS_Read( &reclen, 4, f ) != 4 ) {
            break;
        }
        reclen = LittleLong( reclen );
        if ( reclen < 1 || reclen > (int)sizeof( svDemoRdBuf ) ) {
            break;
        }
        if ( FS_Read( svDemoRdBuf, 1, f ) != 1 ) {
            break;
        }
        type = svDemoRdBuf[0];

        if ( type == REC_CLIENTCONFIGSTRING || type == REC_CLIENTUSERINFO ||
             type == REC_CLIENTCOMMAND ) {
            if ( FS_Read( svDemoRdBuf, 1, f ) != 1 ) {
                break;
            }
            if ( svDemoRdBuf[0] + 1 > used ) {
                used = svDemoRdBuf[0] + 1;
            }
            consumed = 2;
        }

        if ( FS_Seek( f, reclen - consumed, FS_SEEK_CUR ) < 0 ) {
            break;
        }
        if ( type == REC_ENDDEMO ) {
            break;
        }
    }

    if ( used > 0 && used <= MAX_CLIENTS ) {
        svDemoRecordedClients = used;
    }

    FS_FCloseFile( f );

    if ( !svDemoRecMap[0] ) {
        Com_Printf( "DEMOERROR: %s has no map in its metadata.\n", filename );
        return qfalse;
    }
    return qtrue;
}

/*
====================
SV_DemoRestoreConfig

Put back the configuration that was in place before playback and reload that map.
====================
*/
static void SV_DemoRestoreConfig( void ) {
    // restore the game cvars even if the rest of the config was never saved (e.g. demo_play aborted
    // after SV_DemoApplyGameCvars() had already changed them)
    SV_DemoRestoreGameCvars();

    if ( !svDemoHaveSaved ) {
        return;
    }
    svDemoHaveSaved = qfalse;

    Cvar_Set( "sv_fps", va( "%i", svDemoSavedFPS ) );
    Cvar_Set( "g_doWarmup", va( "%i", svDemoSavedDoWarmup ) );
    Cvar_Set( "bot_minplayers", va( "%i", svDemoSavedBotMinPlayers ) );
    Cvar_Set( "g_allowVote", va( "%i", svDemoSavedAllowVote ) );
    if ( svDemoSavedGametype[0] ) {
        Cvar_Set( "sv_gametype", svDemoSavedGametype );
    }
    if ( svDemoSavedFsGame[0] ) {
        Cvar_Set( "fs_game", svDemoSavedFsGame );
    }

    if ( svDemoSavedMap[0] ) {
        Com_Printf( "DEMO: restoring map %s.\n", svDemoSavedMap );
        Cbuf_ExecuteText( EXEC_APPEND, va( "map %s\n", svDemoSavedMap ) );
    }
}

/*
====================
SV_DemoTeardownPlayback

Close the demo file and drop all playback state, without touching cvars or the map.
====================
*/
static void SV_DemoTeardownPlayback( void ) {
    int i;

    if ( sv.demoFile ) {
        FS_FCloseFile( sv.demoFile );
        sv.demoFile = 0;
    }

    // Drop the demo's configstring copies. Do NOT clear the live configstrings here: that would
    // broadcast ~140 reliable "cs" commands in one frame and overflow every client's reliable
    // command buffer (MAX_RELIABLE_COMMANDS), dropping them with "Server command overflow". The map
    // reload below re-initializes all configstrings anyway, so the recorded players vanish with it.
    SV_DemoFreeConfigstrings();

    // and drop any client slot that is still flagged as an emulated demo client (only possible if
    // an older build left one behind, since demo players never become clients anymore)
    for ( i = 0; i < sv_maxclients->integer; i++ ) {
        client_t *cl = &svs.clients[i];

        if ( !cl->demoClient ) {
            continue;
        }
        Com_Printf( "DEMO: dropping demo client %i (%s).\n", i, cl->name );
        SV_DropClient( cl, "demo playback ended" );
        cl->demoClient = qfalse;
    }

    svDemoPlaybackPending = qfalse;
    svDemoPlaybackName[0] = 0;
    Cvar_Set( "sv_democlients", "0" );
    SV_DemoSetState( DS_NONE );
}

/*
====================
SV_DemoStartPlayback

Begin replaying the pending demo. Called after the server has restarted on the recorded
map/gametype. The recorded configstrings/clientinfo are the first records in the stream, so they
are replayed by the first SV_DemoReadFrame() call.
====================
*/
static void SV_DemoStartPlayback( void ) {
    char magic[4];
    int  version, reclen, i;
    fileHandle_t f;

    svDemoPlaybackPending = qfalse;
    sv.demoFile = 0;

    FS_FOpenFileRead( svDemoPlaybackName, &f, qtrue );
    if ( !f ) {
        Com_Printf( "DEMOERROR: cannot reopen %s.\n", svDemoPlaybackName );
        SV_DemoRestoreConfig();
        svDemoPlaybackName[0] = 0;
        return;
    }

    // skip the header (magic + version) and the metadata record
    FS_Read( magic, 4, f );
    FS_Read( &version, 4, f );
    FS_Read( &reclen, 4, f );
    reclen = LittleLong( reclen );
    if ( reclen < 0 || reclen > (int)sizeof( svDemoRdBuf ) ) {
        Com_Printf( "DEMOERROR: %s has a bad metadata record.\n", svDemoPlaybackName );
        FS_FCloseFile( f );
        SV_DemoRestoreConfig();
        svDemoPlaybackName[0] = 0;
        return;
    }
    FS_Seek( f, reclen, FS_SEEK_CUR );

    // never grow sv_maxclients: if this server cannot host the demo plus a spectator, give up
    if ( svDemoRecordedClients >= sv_maxclients->integer ) {
        Com_Printf( "DEMOERROR: %s needs %i client slots but this server only has %i.\n",
                    svDemoPlaybackName, svDemoRecordedClients, sv_maxclients->integer );
        FS_FCloseFile( f );
        SV_DemoRestoreConfig();
        svDemoPlaybackName[0] = 0;
        return;
    }

    sv.demoFile = f;

    // reserve the recorded client slots; the remaining slots stay free for spectators
    Cvar_Set( "sv_democlients", va( "%i", svDemoRecordedClients ) );

    Com_Memset( sv.demoEntities, 0, sizeof( sv.demoEntities ) );
    Com_Memset( sv.demoPlayerStates, 0, sizeof( sv.demoPlayerStates ) );
    Com_Memset( sv.demoUserCmds, 0, sizeof( sv.demoUserCmds ) );
    Com_Memset( svDemoUserCmdSeen, 0, sizeof( svDemoUserCmdSeen ) );
    Com_Memset( svDemoEntityUsed, 0, sizeof( svDemoEntityUsed ) );
    SV_DemoFreeConfigstrings();
    svDemoFrameCounter = -1;
    // Adopt the recording's clock for the whole replay. Every recorded entity/playerstate timestamp
    // (pos.trTime, time, ...) lives in the recording's time base, so if the server kept its own,
    // much larger time the first replayed frame would look like time running backwards and clients
    // would throw the snapshots away ("cl.snap.serverTime < oldServerTime"). Clients only ever see
    // this time line - they connect after the replay's map load - so adopting it is safe, and it is
    // also what makes interpolation of the recorded motion correct. We do not otherwise care about
    // the server time while replaying.
    if ( svDemoRecTime > 0 ) {
        sv.time = svDemoRecTime;
    }
    svDemoLastTime = sv.time;

    SV_DemoSetState( DS_PLAYBACK );

    // A real client squatting in a slot the demo needs would be hijacked by the emulated players,
    // so free those slots up: the client is dropped and can reconnect above the reserved range.
    for ( i = 0; i < svDemoRecordedClients && i < sv_maxclients->integer; i++ ) {
        client_t *cl = &svs.clients[i];

        if ( cl->state && !cl->demoClient ) {
            Com_Printf( "DEMO: dropping %s (slot %i is reserved for an emulated demo player).\n",
                        cl->name, i );
            SV_DropClient( cl, "slot reserved for the server-side demo. Please reconnect" );
        }
    }

    // Any client that connected while the replay's map was loading got a gamestate carrying the
    // server's own clock, which is not the recording's. Resend it now that sv.time is the recording's
    // clock: a fresh gamestate resets the client's frame history, so its first replayed snapshot no
    // longer looks like time running backwards (which otherwise drops the connection).
    for ( i = 0; i < sv_maxclients->integer; i++ ) {
        client_t *cl = &svs.clients[i];

        if ( cl->state && !cl->demoClient ) {
            cl->state = CS_CONNECTED;
            SV_SendClientGameState( cl );
        }
    }

    Com_Printf( "DEMO: playing %s (recorded %s, map %s, %i clients, recorded time %i)\n",
                svDemoPlaybackName, svDemoRecDate[0] ? svDemoRecDate : "unknown",
                svDemoRecMap, svDemoRecordedClients, svDemoRecTime );
    SV_SendServerCommand( NULL, "print \"Server-side demo replay started (%s)\\n\"", svDemoPlaybackName );
}

/*
====================
SV_DemoStopPlayback

Stop replaying. When called while actually playing (console/RCon, a finished/failed demo) the
previous configuration is restored and the previous map reloaded. When the state was already
cleared (SV_Shutdown path) only the playback is torn down.
====================
*/
void SV_DemoStopPlayback( void ) {
    qboolean full = ( sv.demoState == DS_PLAYBACK );

    if ( !full && !sv.demoFile && !svDemoPlaybackName[0] ) {
        return;     // nothing was playing
    }

    if ( full ) {
        Com_Printf( "DEMO: stopped playback of %s.\n", svDemoPlaybackName );
    }

    SV_DemoTeardownPlayback();

    if ( full ) {
        SV_DemoRestoreConfig();
    }
}

/*
====================
SV_DemoPlayStop

demo_stop: stop whatever the server is doing (recording or replaying).
====================
*/
void SV_DemoPlayStop( void ) {
    if ( sv.demoState == DS_PLAYBACK ) {
        SV_DemoStopPlayback();
    } else if ( sv.demoState == DS_RECORDING ) {
        SV_DemoRecordStop();
    } else {
        Com_Printf( "DEMO: nothing to stop.\n" );
    }
}

/*
====================
SV_DemoPlayStart

demo_play: read the demo's metadata, reconfigure the server to match and restart the map. The
replay itself begins in SV_DemoPlaybackAfterRestart() once the new map is up.
====================
*/
qboolean SV_DemoPlayStart( const char *name ) {
    char filename[MAX_QPATH];
    char base[MAX_QPATH];
    char *dot;
    int  v;

    if ( sv.demoState != DS_NONE ) {
        Com_Printf( "DEMO: already recording or replaying a demo.\n" );
        return qfalse;
    }
    if ( !name || !name[0] ) {
        Com_Printf( "usage: demo_play <name>\n" );
        return qfalse;
    }

    // Accept "mydemo", "mydemo.svdm_1", "mydemo.svdm_3", ... : strip whatever demo extension the
    // caller supplied and apply the one this build writes/reads.
    Q_strncpyz( base, name, sizeof( base ) );
    SV_DemoCleanFilename( base );
    dot = strrchr( base, '.' );
    if ( dot && !Q_stricmpn( dot + 1, SVDEMOEXT, (int)strlen( SVDEMOEXT ) ) ) {
        *dot = 0;
    }
    Com_sprintf( filename, sizeof( filename ), "%s%s", base, SVDEMO_FILEEXT );

    if ( !SV_DemoFileExists( filename ) ) {
        // by far the most common reason: an older recording of the same name (different format)
        for ( v = 1; v <= 3; v++ ) {
            char alt[MAX_QPATH];

            Com_sprintf( alt, sizeof( alt ), "%s." SVDEMOEXT "%i", base, v );
            if ( !Q_stricmp( alt, filename ) ) {
                continue;
            }
            if ( SV_DemoFileExists( alt ) ) {
                Com_Printf( "DEMO: note: %s is an older recording (this engine writes and reads format %i) - re-record it with demo_record.\n",
                            alt, SVDEMO_VERSION );
                break;
            }
        }
        Com_Printf( "DEMOERROR: cannot open %s.\n", filename );
        return qfalse;
    }

    if ( !SV_DemoReadMeta( filename ) ) {
        return qfalse;
    }

    if ( svDemoRecordedClients >= sv_maxclients->integer ) {
        Com_Printf( "DEMOERROR: %s needs %i client slots and this server has %i; at least one slot must stay free for spectators (raise sv_maxclients).\n",
                    filename, svDemoRecordedClients, sv_maxclients->integer );
        return qfalse;
    }

    // remember the configuration we are about to change, so it can be restored afterwards
    if ( !svDemoHaveSaved ) {
        svDemoSavedFPS = sv_fps->integer;
        svDemoSavedDoWarmup = Cvar_VariableIntegerValue( "g_doWarmup" );
        svDemoSavedBotMinPlayers = Cvar_VariableIntegerValue( "bot_minplayers" );
        svDemoSavedAllowVote = Cvar_VariableIntegerValue( "g_allowVote" );
        Q_strncpyz( svDemoSavedFsGame, Cvar_VariableString( "fs_game" ), sizeof( svDemoSavedFsGame ) );
        Q_strncpyz( svDemoSavedGametype, sv_gametype->string, sizeof( svDemoSavedGametype ) );
        Q_strncpyz( svDemoSavedMap, sv_mapname->string, sizeof( svDemoSavedMap ) );
        svDemoHaveSaved = qtrue;
    }

    Q_strncpyz( svDemoPlaybackName, filename, sizeof( svDemoPlaybackName ) );
    svDemoPlaybackPending = qtrue;

    // Reserve the recorded client slots *before* the restart below: clients that reconnect while
    // the new map is loading must not take a slot that belongs to an emulated demo player.
    Cvar_Set( "sv_democlients", va( "%i", svDemoRecordedClients ) );

    Com_Printf( "DEMO: switching to map %s (mod %s) to replay %s (%i of %i client slots reserved for the demo)\n",
                svDemoRecMap, svDemoRecFsGame[0] ? svDemoRecFsGame : "base", filename,
                svDemoRecordedClients, sv_maxclients->integer );

    // reconfigure and restart the map; playback begins in SV_DemoPlaybackAfterRestart()
    Cvar_Set( "sv_fps", va( "%i", svDemoRecFps ) );
    Cvar_Set( "g_doWarmup", "0" );
    Cvar_Set( "bot_minplayers", "0" );
    Cvar_Set( "g_allowVote", "0" );
    if ( svDemoRecGametype[0] ) {
        Cvar_Set( "sv_gametype", svDemoRecGametype );
    }
    if ( svDemoRecFsGame[0] ) {
        Cvar_Set( "fs_game", svDemoRecFsGame );
    }

    // Apply the recorded game settings and empty the server, both *before* the map below is loaded:
    // the gametype module and the game read these cvars at map spawn (sv_gametype alone is not
    // enough - the game reads g_gametype, which is why a CTF recording replayed as INF), and the
    // recorded players own slots 0..N-1, which the engine cannot hand over to a live client (the
    // game's per-client state is indexed by client number). Existing clients are therefore asked to
    // reconnect, and their reconnect lands above the reserved range via SV_DemoReservedClients().
    SV_DemoApplyGameCvars();

    // svs.clients does not exist until the first map is loaded, and demo_play may be issued before
    // that (the map command below is what starts the server), so this loop has to be guarded.
    if ( svs.clients ) {
        SV_FinalMessage( "Server-side demo replay started - reconnect.\n");
        
    }
    Cbuf_ExecuteText( EXEC_APPEND, va( "map %s\n", svDemoRecMap ) );
    return qtrue;
}

/*
====================
SV_DemoPlaybackAfterRestart

Called at the very end of SV_SpawnServer(): start the pending playback, or (if sv_autoDemo is on)
begin a new recording, unless the game module just set up a demo recording itself.
====================
*/
void SV_DemoPlaybackAfterRestart( void ) {
    if ( sv.demoState != DS_NONE ) {
        return;     // a recording was already started for this map
    }

    if ( svDemoPlaybackPending && svDemoPlaybackName[0] ) {
        SV_DemoStartPlayback();
        return;
    }

    SV_DemoAutoRecord();
}

/*
====================
SV_DemoAutoRecord

Start recording when sv_autoDemo is enabled and nothing is running yet.
====================
*/
void SV_DemoAutoRecord( void ) {
    if ( sv.demoState != DS_NONE ) {
        return;
    }
    if ( !sv_autoDemo || !sv_autoDemo->integer ) {
        return;
    }
    if ( sv.state != SS_GAME ) {
        return;
    }

    SV_DemoRecordStart( NULL, qfalse );
}

/*
====================
SV_DemoCompleteDemoName

Console completion for demo_play: lists the server-side demo files in the base directory.
====================
*/
void SV_DemoCompleteDemoName( char *args, int argNum ) {
    if ( argNum == 2 ) {
        Field_CompleteFilename( "", SVDEMO_EXTSTR, NULL, qtrue, qtrue );
    }
}

/*
====================
demo_record [name] [anonymous]
====================
*/
static void SV_DemoRecord_f( void ) {
    const char *name = NULL;
    qboolean anonymous = qfalse;

    if ( Cmd_Argc() >= 2 ) {
        if ( !Q_stricmp( Cmd_Argv( 1 ), "anonymous" ) ) {
            anonymous = qtrue;
        } else {
            name = Cmd_Argv( 1 );
            if ( Cmd_Argc() >= 3 && atoi( Cmd_Argv( 2 ) ) ) {
                anonymous = qtrue;
            }
        }
    }

    SV_DemoRecordStart( name, anonymous );
}

/*
====================
demo_play <name>
====================
*/
static void SV_DemoPlay_f( void ) {
    if ( Cmd_Argc() < 2 ) {
        Com_Printf( "usage: demo_play <name>\n" );
        return;
    }

    SV_DemoPlayStart( Cmd_Argv( 1 ) );
}

/*
====================
demo_stop
====================
*/
static void SV_DemoStop_f( void ) {
    SV_DemoPlayStop();
}

/*
====================
SV_DemoAddCommands

Register the demo_* operator commands. Called from SV_AddOperatorCommands().
====================
*/
void SV_DemoAddCommands( void ) {
    Cmd_AddCommand( "demo_record", SV_DemoRecord_f );
    Cmd_AddCommand( "demo_play", SV_DemoPlay_f );
    Cmd_AddCommand( "demo_stop", SV_DemoStop_f );
    Cmd_SetCommandCompletionFunc( "demo_play", SV_DemoCompleteDemoName );
}













