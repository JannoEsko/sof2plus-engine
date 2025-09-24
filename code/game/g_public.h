// Copyright (C) 2001-2002 Raven Software.
//
// g_public.h -- game module information visible to server

#define GAME_API_VERSION    8

// entity->svFlags
// the server does not know how to interpret most of the values
// in entityStates (level eType), so the game must explicitly flag
// special server behaviors
#define SVF_NOCLIENT            0x00000001  // don't send entity to clients, even if it has effects
#define SVF_BOT                 0x00000008  // set if the entity is a bot
#define SVF_BROADCAST           0x00000020  // send to all connected clients
#define SVF_PORTAL              0x00000040  // merge a second pvs at origin2 into snapshots
#define SVF_USE_CURRENT_ORIGIN  0x00000080  // entity->r.currentOrigin instead of entity->s.origin
                                            // for link position (missiles and movers)
#define SVF_SINGLECLIENT        0x00000100  // only send to a single client (entityShared_t->singleClient)
#define SVF_NOSERVERINFO        0x00000200  // don't send CS_SERVERINFO updates to this client
                                            // so that it can be updated for ping tools without
                                            // lagging clients
#define SVF_CAPSULE             0x00000400  // use capsule for collision detection instead of bbox
#define SVF_NOTSINGLECLIENT     0x00000800  // send entity to everyone but one client
                                            // (entityShared_t->singleClient)

#define SVF_GLASS_BRUSH         0x08000000  // Ent is a glass brush

#define SVF_INFLATED_BBOX       0x00001000  // Bounding box has been doubled
#define SVF_LINKHACK            0x10000000  // Hack to link an entity into extra clusters
#define SVF_DETAIL              0x20000000  // Entity is a detail entity and can be dropped from the snapshot
#define SVF_SKIP                0x80000000  // Dont include this entity in the current snapshot (internal use only)

//===============================================================


typedef struct {
    qboolean    linked;             // qfalse if not in any good cluster
    int         linkcount;

    int         svFlags;            // SVF_NOCLIENT, SVF_BROADCAST, etc
    int         singleClient;       // only send to this client when SVF_SINGLECLIENT is set

    qboolean    bmodel;             // if false, assume an explicit mins / maxs bounding box
                                    // only set by trap_SetBrushModel
    vec3_t      mins, maxs;
    int         contents;           // CONTENTS_TRIGGER, CONTENTS_SOLID, CONTENTS_BODY, etc
                                    // a non-solid entity should set to 0

    vec3_t      absmin, absmax;     // derived from mins/maxs and origin + rotation

    // currentOrigin will be used for all collision detection and world linking.
    // it will not necessarily be the same as the trajectory evaluation for the current
    // time, because each entity must be moved one at a time after time is advanced
    // to avoid simultanious collision issues
    vec3_t      currentOrigin;
    vec3_t      currentAngles;

    // when a trace call is made and passEntityNum != ENTITYNUM_NONE,
    // an ent will be excluded from testing if:
    // ent->s.number == passEntityNum   (don't interact with self)
    // ent->s.ownerNum = passEntityNum  (don't interact with your own missiles)
    // entity[ent->s.ownerNum].ownerNum = passEntityNum (don't interact with other missiles from owner)
    int         ownerNum;

    // mask of clients that this entity should be broadcast too.  The first 32 clients
    // are represented by the first array index and the latter 32 clients are represented
    // by the second array index.
    int         broadcastClients[2];

    int         detailTime;

} entityShared_t;



// the server looks at a sharedEntity, which is the start of the game's gentity_t structure
typedef struct {
    entityState_t   s;              // communicated by server to clients
    entityShared_t  r;              // shared by both the server system and game
} sharedEntity_t;



//===============================================================

//
// system traps provided by the main engine
//
typedef enum {
    //============== general Quake services ==================

    G_PRINT,        // ( const char *string );
    // print message on the local console

    G_ERROR,        // ( const char *string );
    // abort the game

    G_MILLISECONDS, // ( void );
    // get current time for profiling reasons
    // this should NOT be used for any game related tasks,
    // because it is not journaled

    // console variable interaction
    G_CVAR_REGISTER,    // ( vmCvar_t *vmCvar, const char *varName, const char *defaultValue, int flags );
    G_CVAR_UPDATE,  // ( vmCvar_t *vmCvar );
    G_CVAR_SET,     // ( const char *var_name, const char *value );
    G_CVAR_VARIABLE_INTEGER_VALUE,  // ( const char *var_name );

    G_CVAR_VARIABLE_STRING_BUFFER,  // ( const char *var_name, char *buffer, int bufsize );

    G_ARGC,         // ( void );
    // ClientCommand and ServerCommand parameter access

    G_ARGV,         // ( int n, char *buffer, int bufferLength );

    G_FS_FOPEN_FILE,    // ( const char *qpath, fileHandle_t *file, fsMode_t mode );
    G_FS_READ,      // ( void *buffer, int len, fileHandle_t f );
    G_FS_WRITE,     // ( const void *buffer, int len, fileHandle_t f );
    G_FS_FCLOSE_FILE,       // ( fileHandle_t f );

    G_SEND_CONSOLE_COMMAND, // ( const char *text );
    // add commands to the console as if they were typed in
    // for map changing, etc


    //=========== server specific functionality =============

    G_LOCATE_GAME_DATA,     // ( gentity_t *gEnts, int numGEntities, int sizeofGEntity_t,
    //                          playerState_t *clients, int sizeofGameClient );
    // the game needs to let the server system know where and how big the gentities
    // are, so it can look at them directly without going through an interface

    G_GET_WORLD_BOUNDS,     // ( vec3_t mins, vec3_t maxs )
                            // Returns the mins and maxs of the world

    G_RMG_INIT,

    G_DROP_CLIENT,      // ( int clientNum, const char *reason );
    // kick a client off the server with a message

    G_SEND_SERVER_COMMAND,  // ( int clientNum, const char *fmt, ... );
    // reliably sends a command string to be interpreted by the given
    // client.  If clientNum is -1, it will be sent to all clients

    G_SET_CONFIGSTRING, // ( int num, const char *string );
    // config strings hold all the index strings, and various other information
    // that is reliably communicated to all clients
    // All of the current configstrings are sent to clients when
    // they connect, and changes are sent to all connected clients.
    // All confgstrings are cleared at each level start.

    G_GET_CONFIGSTRING, // ( int num, char *buffer, int bufferSize );

    G_GET_USERINFO,     // ( int num, char *buffer, int bufferSize );
    // userinfo strings are maintained by the server system, so they
    // are persistant across level loads, while all other game visible
    // data is completely reset

    G_SET_USERINFO,     // ( int num, const char *buffer );

    G_GET_SERVERINFO,   // ( char *buffer, int bufferSize );
    // the serverinfo info string has all the cvars visible to server browsers

    G_SET_BRUSH_MODEL,  // ( gentity_t *ent, const char *name );
    // sets mins and maxs based on the brushmodel name

    G_SET_ACTIVE_SUBBSP,    // int index

    G_TRACE,    // ( trace_t *results, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int passEntityNum, int contentmask );
    // collision detection against all linked entities

    G_POINT_CONTENTS,   // ( const vec3_t point, int passEntityNum );
    // point contents against all linked entities

    G_IN_PVS,           // ( const vec3_t p1, const vec3_t p2 );

    G_IN_PVS_IGNORE_PORTALS,    // ( const vec3_t p1, const vec3_t p2 );

    G_ADJUST_AREA_PORTAL_STATE, // ( gentity_t *ent, qboolean open );

    G_AREAS_CONNECTED,  // ( int area1, int area2 );

    G_LINKENTITY,       // ( gentity_t *ent );
    // an entity will never be sent to a client or used for collision
    // if it is not passed to linkentity.  If the size, position, or
    // solidity changes, it must be relinked.

    G_UNLINKENTITY,     // ( gentity_t *ent );
    // call before removing an interactive entity

    G_ENTITIES_IN_BOX,  // ( const vec3_t mins, const vec3_t maxs, gentity_t **list, int maxcount );
    // EntitiesInBox will return brush models based on their bounding box,
    // so exact determination must still be done with EntityContact

    G_ENTITY_CONTACT,   // ( const vec3_t mins, const vec3_t maxs, const gentity_t *ent );
    // perform an exact check against inline brush models of non-square shape

    // access for bots to get and free a server client (FIXME?)
    G_BOT_ALLOCATE_CLIENT,  // ( void );

    G_BOT_FREE_CLIENT,  // ( int clientNum );

    G_GET_USERCMD,  // ( int clientNum, usercmd_t *cmd )

    G_GET_ENTITY_TOKEN, // qboolean ( char *buffer, int bufferSize )
    // Retrieves the next string token from the entity spawn text, returning
    // false when all tokens have been parsed.
    // This should only be done at GAME_INIT time.

    G_FS_GETFILELIST,
    G_BOT_GET_MEMORY,
    G_BOT_FREE_MEMORY,
    G_DEBUG_POLYGON_CREATE,
    G_DEBUG_POLYGON_DELETE,
    G_REAL_TIME,
    G_SNAPVECTOR,

    G_TRACECAPSULE, // ( trace_t *results, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int passEntityNum, int contentmask );
    G_ENTITY_CONTACTCAPSULE,    // ( const vec3_t mins, const vec3_t maxs, const gentity_t *ent );

    BOTLIB_SETUP = 200,             // ( void );
    BOTLIB_SHUTDOWN,                // ( void );
    BOTLIB_LIBVAR_SET,
    BOTLIB_LIBVAR_GET,
    BOTLIB_PC_ADD_GLOBAL_DEFINE,
    BOTLIB_START_FRAME,
    BOTLIB_LOAD_MAP,
    BOTLIB_UPDATENTITY,
    BOTLIB_TEST,

    BOTLIB_GET_SNAPSHOT_ENTITY,     // ( int client, int ent );
    BOTLIB_GET_CONSOLE_MESSAGE,     // ( int client, char *message, int size );
    BOTLIB_USER_COMMAND,            // ( int client, usercmd_t *ucmd );

    BOTLIB_AAS_ENABLE_ROUTING_AREA = 300,
    BOTLIB_AAS_BBOX_AREAS,
    BOTLIB_AAS_AREA_INFO,
    BOTLIB_AAS_ENTITY_INFO,

    BOTLIB_AAS_INITIALIZED,
    BOTLIB_AAS_PRESENCE_TYPE_BOUNDING_BOX,
    BOTLIB_AAS_TIME,

    BOTLIB_AAS_POINT_AREA_NUM,
    BOTLIB_AAS_TRACE_AREAS,

    BOTLIB_AAS_POINT_CONTENTS,
    BOTLIB_AAS_NEXT_BSP_ENTITY,
    BOTLIB_AAS_VALUE_FOR_BSP_EPAIR_KEY,
    BOTLIB_AAS_VECTOR_FOR_BSP_EPAIR_KEY,
    BOTLIB_AAS_FLOAT_FOR_BSP_EPAIR_KEY,
    BOTLIB_AAS_INT_FOR_BSP_EPAIR_KEY,

    BOTLIB_AAS_AREA_REACHABILITY,

    BOTLIB_AAS_AREA_TRAVEL_TIME_TO_GOAL_AREA,

    BOTLIB_AAS_SWIMMING,
    BOTLIB_AAS_PREDICT_CLIENT_MOVEMENT,

    BOTLIB_EA_SAY = 400,
    BOTLIB_EA_SAY_TEAM,
    BOTLIB_EA_COMMAND,

    BOTLIB_EA_ACTION,
    BOTLIB_EA_GESTURE,
    BOTLIB_EA_TALK,
    BOTLIB_EA_ATTACK,
    BOTLIB_EA_ALT_ATTACK,
    BOTLIB_EA_FORCEPOWER,
    BOTLIB_EA_USE,
    BOTLIB_EA_RESPAWN,
    BOTLIB_EA_CROUCH,
    BOTLIB_EA_MOVE_UP,
    BOTLIB_EA_MOVE_DOWN,
    BOTLIB_EA_MOVE_FORWARD,
    BOTLIB_EA_MOVE_BACK,
    BOTLIB_EA_MOVE_LEFT,
    BOTLIB_EA_MOVE_RIGHT,

    BOTLIB_EA_SELECT_WEAPON,
    BOTLIB_EA_JUMP,
    BOTLIB_EA_DELAYED_JUMP,
    BOTLIB_EA_MOVE,
    BOTLIB_EA_VIEW,

    BOTLIB_EA_END_REGULAR,
    BOTLIB_EA_GET_INPUT,
    BOTLIB_EA_RESET_INPUT,


    BOTLIB_AI_LOAD_CHARACTER = 500,
    BOTLIB_AI_FREE_CHARACTER,
    BOTLIB_AI_CHARACTERISTIC_FLOAT,
    BOTLIB_AI_CHARACTERISTIC_BFLOAT,
    BOTLIB_AI_CHARACTERISTIC_INTEGER,
    BOTLIB_AI_CHARACTERISTIC_BINTEGER,
    BOTLIB_AI_CHARACTERISTIC_STRING,

    BOTLIB_AI_ALLOC_CHAT_STATE,
    BOTLIB_AI_FREE_CHAT_STATE,
    BOTLIB_AI_QUEUE_CONSOLE_MESSAGE,
    BOTLIB_AI_REMOVE_CONSOLE_MESSAGE,
    BOTLIB_AI_NEXT_CONSOLE_MESSAGE,
    BOTLIB_AI_NUM_CONSOLE_MESSAGE,
    BOTLIB_AI_INITIAL_CHAT,
    BOTLIB_AI_REPLY_CHAT,
    BOTLIB_AI_CHAT_LENGTH,
    BOTLIB_AI_ENTER_CHAT,
    BOTLIB_AI_STRING_CONTAINS,
    BOTLIB_AI_FIND_MATCH,
    BOTLIB_AI_MATCH_VARIABLE,
    BOTLIB_AI_UNIFY_WHITE_SPACES,
    BOTLIB_AI_REPLACE_SYNONYMS,
    BOTLIB_AI_LOAD_CHAT_FILE,
    BOTLIB_AI_SET_CHAT_GENDER,
    BOTLIB_AI_SET_CHAT_NAME,

    BOTLIB_AI_RESET_GOAL_STATE,
    BOTLIB_AI_RESET_AVOID_GOALS,
    BOTLIB_AI_PUSH_GOAL,
    BOTLIB_AI_POP_GOAL,
    BOTLIB_AI_EMPTY_GOAL_STACK,
    BOTLIB_AI_DUMP_AVOID_GOALS,
    BOTLIB_AI_DUMP_GOAL_STACK,
    BOTLIB_AI_GOAL_NAME,
    BOTLIB_AI_GET_TOP_GOAL,
    BOTLIB_AI_GET_SECOND_GOAL,
    BOTLIB_AI_CHOOSE_LTG_ITEM,
    BOTLIB_AI_CHOOSE_NBG_ITEM,
    BOTLIB_AI_TOUCHING_GOAL,
    BOTLIB_AI_ITEM_GOAL_IN_VIS_BUT_NOT_VISIBLE,
    BOTLIB_AI_GET_LEVEL_ITEM_GOAL,
    BOTLIB_AI_AVOID_GOAL_TIME,
    BOTLIB_AI_INIT_LEVEL_ITEMS,
    BOTLIB_AI_UPDATE_ENTITY_ITEMS,
    BOTLIB_AI_LOAD_ITEM_WEIGHTS,
    BOTLIB_AI_FREE_ITEM_WEIGHTS,
    BOTLIB_AI_SAVE_GOAL_FUZZY_LOGIC,
    BOTLIB_AI_ALLOC_GOAL_STATE,
    BOTLIB_AI_FREE_GOAL_STATE,

    BOTLIB_AI_RESET_MOVE_STATE,
    BOTLIB_AI_MOVE_TO_GOAL,
    BOTLIB_AI_MOVE_IN_DIRECTION,
    BOTLIB_AI_RESET_AVOID_REACH,
    BOTLIB_AI_RESET_LAST_AVOID_REACH,
    BOTLIB_AI_REACHABILITY_AREA,
    BOTLIB_AI_MOVEMENT_VIEW_TARGET,
    BOTLIB_AI_ALLOC_MOVE_STATE,
    BOTLIB_AI_FREE_MOVE_STATE,
    BOTLIB_AI_INIT_MOVE_STATE,

    BOTLIB_AI_CHOOSE_BEST_FIGHT_WEAPON,
    BOTLIB_AI_GET_WEAPON_INFO,
    BOTLIB_AI_LOAD_WEAPON_WEIGHTS,
    BOTLIB_AI_ALLOC_WEAPON_STATE,
    BOTLIB_AI_FREE_WEAPON_STATE,
    BOTLIB_AI_RESET_WEAPON_STATE,

    BOTLIB_AI_GENETIC_PARENTS_AND_CHILD_SELECTION,
    BOTLIB_AI_INTERBREED_GOAL_FUZZY_LOGIC,
    BOTLIB_AI_MUTATE_GOAL_FUZZY_LOGIC,
    BOTLIB_AI_GET_NEXT_CAMP_SPOT_GOAL,
    BOTLIB_AI_GET_MAP_LOCATION_GOAL,
    BOTLIB_AI_NUM_INITIAL_CHATS,
    BOTLIB_AI_GET_CHAT_MESSAGE,
    BOTLIB_AI_REMOVE_FROM_AVOID_GOALS,
    BOTLIB_AI_PREDICT_VISIBLE_POSITION,

    BOTLIB_AI_SET_AVOID_GOAL_TIME,
    BOTLIB_AI_ADD_AVOID_SPOT,
    BOTLIB_AAS_ALTERNATIVE_ROUTE_GOAL,
    BOTLIB_AAS_PREDICT_ROUTE,
    BOTLIB_AAS_POINT_REACHABILITY_AREA_INDEX,

    BOTLIB_PC_LOAD_SOURCE,
    BOTLIB_PC_FREE_SOURCE,
    BOTLIB_PC_READ_TOKEN,
    BOTLIB_PC_SOURCE_FILE_AND_LINE,
    BOTLIB_PC_LOAD_GLOBAL_DEFINES,
    BOTLIB_PC_REMOVE_ALL_GLOBAL_DEFINES,

    //=============== Ghoul II functionality ================
    G_G2_LISTBONES,
    G_G2_LISTSURFACES,
    G_G2_INITGHOUL2MODEL,
    G_G2_ANGLEOVERRIDE,
    G_G2_PLAYANIM,
    G_G2_GETANIMFILENAME,
    G_G2_REMOVEGHOUL2MODEL,

    G_G2_COLLISIONDETECT,
    G_G2_REGISTERSKIN,
    G_G2_SETSKIN,

    //======== Generic Parser 2 (GP2) functionality =========
    // CGenericParser2 (void *) routines
    G_GP_PARSE,
    G_GP_PARSE_FILE,
    G_GP_CLEAN,
    G_GP_DELETE,
    G_GP_GET_BASE_PARSE_GROUP,

    // CGPGroup (void *) routines
    G_GPG_GET_NAME,
    G_GPG_GET_NEXT,
    G_GPG_GET_INORDER_NEXT,
    G_GPG_GET_INORDER_PREVIOUS,
    G_GPG_GET_PAIRS,
    G_GPG_GET_INORDER_PAIRS,
    G_GPG_GET_SUBGROUPS,
    G_GPG_GET_INORDER_SUBGROUPS,
    G_GPG_FIND_SUBGROUP,
    G_GPG_FIND_PAIR,
    G_GPG_FIND_PAIRVALUE,

    // CGPValue (void *) routines
    G_GPV_GET_NAME,
    G_GPV_GET_NEXT,
    G_GPV_GET_INORDER_NEXT,
    G_GPV_GET_INORDER_PREVIOUS,
    G_GPV_IS_LIST,
    G_GPV_GET_TOP_VALUE,
    G_GPV_GET_LIST,

    G_CM_REGISTER_TERRAIN,
    G_GET_MODEL_FORMALNAME,

    G_MEM_INIT,

    //================= Gametype interface ==================
    G_GT_INIT,
    G_GT_RUNFRAME,
    G_GT_START,
    G_GT_SENDEVENT,
    G_GT_SHUTDOWN,
    // custom 
    G_CLIENT_ISLEGACYPROTOCOL = 1001,
    G_TRANSLATE_SILVER_WPN_TO_GOLD,
    G_TRANSLATE_GOLD_WPN_TO_SILVER,
    G_VALIDATE_MAP_NAME,
    G_GET_MAPCYCLE_LIST,
    G_SKIP_TO_MAP

} gameImport_t;


//
// functions exported by the game subsystem
//
typedef enum {
    GAME_INIT,  // ( int levelTime, int randomSeed, int restart );
    // init and shutdown will be called every single level
    // The game should call G_GET_ENTITY_TOKEN to parse through all the
    // entity configuration text and spawn gentities.

    GAME_SHUTDOWN,  // (void);

    GAME_CLIENT_CONNECT,    // ( int clientNum, qboolean firstTime, qboolean isBot );
    // return NULL if the client is allowed to connect, otherwise return
    // a text string with the reason for denial

    GAME_CLIENT_BEGIN,              // ( int clientNum );

    GAME_CLIENT_USERINFO_CHANGED,   // ( int clientNum );

    GAME_CLIENT_DISCONNECT,         // ( int clientNum );

    GAME_CLIENT_COMMAND,            // ( int clientNum );

    GAME_CLIENT_THINK,              // ( int clientNum );

    GAME_RUN_FRAME,                 // ( int levelTime );

    GAME_GHOUL_INIT,

    GAME_GHOUL_SHUTDOWN,

    GAME_CONSOLE_COMMAND,           // ( void );
    // ConsoleCommand will be called when a command has been issued
    // that is not recognized as a builtin function.
    // The game can issue trap_argc() / trap_argv() commands to get the command
    // and parameters.  Return qfalse if the game doesn't recognize it as a command.

    BOTAI_START_FRAME,              // ( int time );

    GAME_SPAWN_RMG_ENTITY,

    GAME_GAMETYPE_COMMAND,          // ( int cmd, int arg0, int arg1, int arg2, int arg3, int arg4 );

    GAME_RCON_LOG,

} gameExport_t;

// The game import structure on QVM's differs quite a bit from SoF2Plus SDK
// Intention is to support QVM's built for the original sof2 release, so instead of messing up the entire syscall chain, we will add one more import struct
// This contains the ORIGINAL QVM  gameImport_t structure as per sof2 SDK.
// Taken as-is from 1fxmod's g_public.h

typedef enum {
    //============== general Quake services ==================

    LEGACY_G_PRINT,        // ( const char *string );
    // print message on the local console

    LEGACY_G_ERROR,        // ( const char *string );
    // abort the game

    LEGACY_G_MILLISECONDS, // ( void );
    // get current time for profiling reasons
    // this should NOT be used for any game related tasks,
    // because it is not journaled

    // console variable interaction
    LEGACY_G_CVAR_REGISTER,    // ( vmCvar_t *vmCvar, const char *varName, const char *defaultValue, int flags );
    LEGACY_G_CVAR_UPDATE,  // ( vmCvar_t *vmCvar );
    LEGACY_G_CVAR_SET,     // ( const char *var_name, const char *value );
    LEGACY_G_CVAR_VARIABLE_INTEGER_VALUE,  // ( const char *var_name );

    LEGACY_G_CVAR_VARIABLE_STRING_BUFFER,  // ( const char *var_name, char *buffer, int bufsize );

    LEGACY_G_ARGC,         // ( void );
    // ClientCommand and ServerCommand parameter access

    LEGACY_G_ARGV,         // ( int n, char *buffer, int bufferLength );

    LEGACY_G_FS_FOPEN_FILE,    // ( const char *qpath, fileHandle_t *file, fsMode_t mode );
    LEGACY_G_FS_READ,      // ( void *buffer, int len, fileHandle_t f );
    LEGACY_G_FS_WRITE,     // ( const void *buffer, int len, fileHandle_t f );
    LEGACY_G_FS_FCLOSE_FILE,       // ( fileHandle_t f );

    LEGACY_G_SEND_CONSOLE_COMMAND, // ( const char *text );
    // add commands to the console as if they were typed in
    // for map changing, etc


    //=========== server specific functionality =============

    LEGACY_G_LOCATE_GAME_DATA,     // ( gentity_t *gEnts, int numGEntities, int sizeofGEntity_t,
    //                          playerState_t *clients, int sizeofGameClient );
    // the game needs to let the server system know where and how big the gentities
    // are, so it can look at them directly without going through an interface

    LEGACY_G_GET_WORLD_BOUNDS,     // ( vec3_t mins, vec3_t maxs )
    // Returns the mins and maxs of the world

    LEGACY_G_RMG_INIT,

    LEGACY_G_DROP_CLIENT,      // ( int clientNum, const char *reason );
    // kick a client off the server with a message

    LEGACY_G_SEND_SERVER_COMMAND,  // ( int clientNum, const char *fmt, ... );
    // reliably sends a command string to be interpreted by the given
    // client.  If clientNum is -1, it will be sent to all clients

    LEGACY_G_SET_CONFIGSTRING, // ( int num, const char *string );
    // config strings hold all the index strings, and various other information
    // that is reliably communicated to all clients
    // All of the current configstrings are sent to clients when
    // they connect, and changes are sent to all connected clients.
    // All confgstrings are cleared at each level start.

    LEGACY_G_GET_CONFIGSTRING, // ( int num, char *buffer, int bufferSize );

    LEGACY_G_GET_USERINFO,     // ( int num, char *buffer, int bufferSize );
    // userinfo strings are maintained by the server system, so they
    // are persistant across level loads, while all other game visible
    // data is completely reset

    LEGACY_G_SET_USERINFO,     // ( int num, const char *buffer );

    LEGACY_G_GET_SERVERINFO,   // ( char *buffer, int bufferSize );
    // the serverinfo info string has all the cvars visible to server browsers

    LEGACY_G_SET_BRUSH_MODEL,  // ( gentity_t *ent, const char *name );
    // sets mins and maxs based on the brushmodel name

    LEGACY_G_SET_ACTIVE_SUBBSP,    // int index

    LEGACY_G_TRACE,    // ( trace_t *results, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int passEntityNum, int contentmask );
    // collision detection against all linked entities

    LEGACY_G_POINT_CONTENTS,   // ( const vec3_t point, int passEntityNum );
    // point contents against all linked entities

    LEGACY_G_IN_PVS,           // ( const vec3_t p1, const vec3_t p2 );

    LEGACY_G_IN_PVS_IGNORE_PORTALS,    // ( const vec3_t p1, const vec3_t p2 );

    LEGACY_G_ADJUST_AREA_PORTAL_STATE, // ( gentity_t *ent, qboolean open );

    LEGACY_G_AREAS_CONNECTED,  // ( int area1, int area2 );

    LEGACY_G_LINKENTITY,       // ( gentity_t *ent );
    // an entity will never be sent to a client or used for collision
    // if it is not passed to linkentity.  If the size, position, or
    // solidity changes, it must be relinked.

    LEGACY_G_UNLINKENTITY,     // ( gentity_t *ent );      
    // call before removing an interactive entity

    LEGACY_G_ENTITIES_IN_BOX,  // ( const vec3_t mins, const vec3_t maxs, gentity_t **list, int maxcount );
    // EntitiesInBox will return brush models based on their bounding box,
    // so exact determination must still be done with EntityContact

    LEGACY_G_ENTITY_CONTACT,   // ( const vec3_t mins, const vec3_t maxs, const gentity_t *ent );
    // perform an exact check against inline brush models of non-square shape

    // access for bots to get and free a server client (FIXME?)
    LEGACY_G_BOT_ALLOCATE_CLIENT,  // ( void );

    LEGACY_G_BOT_FREE_CLIENT,  // ( int clientNum );

    LEGACY_G_GET_USERCMD,  // ( int clientNum, usercmd_t *cmd )

    LEGACY_G_GET_ENTITY_TOKEN, // qboolean ( char *buffer, int bufferSize )
    // Retrieves the next string token from the entity spawn text, returning
    // false when all tokens have been parsed.
    // This should only be done at GAME_INIT time.

    LEGACY_G_FS_GETFILELIST,
    LEGACY_G_BOT_GET_MEMORY,
    LEGACY_G_BOT_FREE_MEMORY,
    LEGACY_G_DEBUG_POLYGON_CREATE,
    LEGACY_G_DEBUG_POLYGON_DELETE,
    LEGACY_G_REAL_TIME,
    LEGACY_G_SNAPVECTOR,

    LEGACY_G_TRACECAPSULE, // ( trace_t *results, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int passEntityNum, int contentmask );
    LEGACY_G_ENTITY_CONTACTCAPSULE,    // ( const vec3_t mins, const vec3_t maxs, const gentity_t *ent );

    LEGACY_G_MEMSET = 100,
    LEGACY_G_MEMCPY,
    LEGACY_G_STRNCPY,
    LEGACY_G_SIN,
    LEGACY_G_COS,
    LEGACY_G_ATAN2,
    LEGACY_G_SQRT,
    LEGACY_G_ANGLEVECTORS,
    LEGACY_G_PERPENDICULARVECTOR,
    LEGACY_G_FLOOR,
    LEGACY_G_CEIL,

    LEGACY_G_TESTPRINTINT,
    LEGACY_G_TESTPRINTFLOAT,

    LEGACY_G_ACOS,
    LEGACY_G_ASIN,

    LEGACY_G_MATRIXMULTIPLY,

    LEGACY_BOTLIB_SETUP = 200,             // ( void );
    LEGACY_BOTLIB_SHUTDOWN,                // ( void );
    LEGACY_BOTLIB_LIBVAR_SET,
    LEGACY_BOTLIB_LIBVAR_GET,
    LEGACY_BOTLIB_PC_ADD_GLOBAL_DEFINE,
    LEGACY_BOTLIB_START_FRAME,
    LEGACY_BOTLIB_LOAD_MAP,
    LEGACY_BOTLIB_UPDATENTITY,
    LEGACY_BOTLIB_TEST,

    LEGACY_BOTLIB_GET_SNAPSHOT_ENTITY,     // ( int client, int ent );
    LEGACY_BOTLIB_GET_CONSOLE_MESSAGE,     // ( int client, char *message, int size );
    LEGACY_BOTLIB_USER_COMMAND,            // ( int client, usercmd_t *ucmd );

    LEGACY_BOTLIB_AAS_ENABLE_ROUTING_AREA = 300,
    LEGACY_BOTLIB_AAS_BBOX_AREAS,
    LEGACY_BOTLIB_AAS_AREA_INFO,
    LEGACY_BOTLIB_AAS_ENTITY_INFO,

    LEGACY_BOTLIB_AAS_INITIALIZED,
    LEGACY_BOTLIB_AAS_PRESENCE_TYPE_BOUNDING_BOX,
    LEGACY_BOTLIB_AAS_TIME,

    LEGACY_BOTLIB_AAS_POINT_AREA_NUM,
    LEGACY_BOTLIB_AAS_TRACE_AREAS,

    LEGACY_BOTLIB_AAS_POINT_CONTENTS,
    LEGACY_BOTLIB_AAS_NEXT_BSP_ENTITY,
    LEGACY_BOTLIB_AAS_VALUE_FOR_BSP_EPAIR_KEY,
    LEGACY_BOTLIB_AAS_VECTOR_FOR_BSP_EPAIR_KEY,
    LEGACY_BOTLIB_AAS_FLOAT_FOR_BSP_EPAIR_KEY,
    LEGACY_BOTLIB_AAS_INT_FOR_BSP_EPAIR_KEY,

    LEGACY_BOTLIB_AAS_AREA_REACHABILITY,

    LEGACY_BOTLIB_AAS_AREA_TRAVEL_TIME_TO_GOAL_AREA,

    LEGACY_BOTLIB_AAS_SWIMMING,
    LEGACY_BOTLIB_AAS_PREDICT_CLIENT_MOVEMENT,

    LEGACY_BOTLIB_EA_SAY = 400,
    LEGACY_BOTLIB_EA_SAY_TEAM,
    LEGACY_BOTLIB_EA_COMMAND,

    LEGACY_BOTLIB_EA_ACTION,
    LEGACY_BOTLIB_EA_GESTURE,
    LEGACY_BOTLIB_EA_TALK,
    LEGACY_BOTLIB_EA_ATTACK,
    LEGACY_BOTLIB_EA_ALT_ATTACK,
    LEGACY_BOTLIB_EA_FORCEPOWER,
    LEGACY_BOTLIB_EA_USE,
    LEGACY_BOTLIB_EA_RESPAWN,
    LEGACY_BOTLIB_EA_CROUCH,
    LEGACY_BOTLIB_EA_MOVE_UP,
    LEGACY_BOTLIB_EA_MOVE_DOWN,
    LEGACY_BOTLIB_EA_MOVE_FORWARD,
    LEGACY_BOTLIB_EA_MOVE_BACK,
    LEGACY_BOTLIB_EA_MOVE_LEFT,
    LEGACY_BOTLIB_EA_MOVE_RIGHT,

    LEGACY_BOTLIB_EA_SELECT_WEAPON,
    LEGACY_BOTLIB_EA_JUMP,
    LEGACY_BOTLIB_EA_DELAYED_JUMP,
    LEGACY_BOTLIB_EA_MOVE,
    LEGACY_BOTLIB_EA_VIEW,

    LEGACY_BOTLIB_EA_END_REGULAR,
    LEGACY_BOTLIB_EA_GET_INPUT,
    LEGACY_BOTLIB_EA_RESET_INPUT,


    LEGACY_BOTLIB_AI_LOAD_CHARACTER = 500,
    LEGACY_BOTLIB_AI_FREE_CHARACTER,
    LEGACY_BOTLIB_AI_CHARACTERISTIC_FLOAT,
    LEGACY_BOTLIB_AI_CHARACTERISTIC_BFLOAT,
    LEGACY_BOTLIB_AI_CHARACTERISTIC_INTEGER,
    LEGACY_BOTLIB_AI_CHARACTERISTIC_BINTEGER,
    LEGACY_BOTLIB_AI_CHARACTERISTIC_STRING,

    LEGACY_BOTLIB_AI_ALLOC_CHAT_STATE,
    LEGACY_BOTLIB_AI_FREE_CHAT_STATE,
    LEGACY_BOTLIB_AI_QUEUE_CONSOLE_MESSAGE,
    LEGACY_BOTLIB_AI_REMOVE_CONSOLE_MESSAGE,
    LEGACY_BOTLIB_AI_NEXT_CONSOLE_MESSAGE,
    LEGACY_BOTLIB_AI_NUM_CONSOLE_MESSAGE,
    LEGACY_BOTLIB_AI_INITIAL_CHAT,
    LEGACY_BOTLIB_AI_REPLY_CHAT,
    LEGACY_BOTLIB_AI_CHAT_LENGTH,
    LEGACY_BOTLIB_AI_ENTER_CHAT,
    LEGACY_BOTLIB_AI_STRING_CONTAINS,
    LEGACY_BOTLIB_AI_FIND_MATCH,
    LEGACY_BOTLIB_AI_MATCH_VARIABLE,
    LEGACY_BOTLIB_AI_UNIFY_WHITE_SPACES,
    LEGACY_BOTLIB_AI_REPLACE_SYNONYMS,
    LEGACY_BOTLIB_AI_LOAD_CHAT_FILE,
    LEGACY_BOTLIB_AI_SET_CHAT_GENDER,
    LEGACY_BOTLIB_AI_SET_CHAT_NAME,

    LEGACY_BOTLIB_AI_RESET_GOAL_STATE,
    LEGACY_BOTLIB_AI_RESET_AVOID_GOALS,
    LEGACY_BOTLIB_AI_PUSH_GOAL,
    LEGACY_BOTLIB_AI_POP_GOAL,
    LEGACY_BOTLIB_AI_EMPTY_GOAL_STACK,
    LEGACY_BOTLIB_AI_DUMP_AVOID_GOALS,
    LEGACY_BOTLIB_AI_DUMP_GOAL_STACK,
    LEGACY_BOTLIB_AI_GOAL_NAME,
    LEGACY_BOTLIB_AI_GET_TOP_GOAL,
    LEGACY_BOTLIB_AI_GET_SECOND_GOAL,
    LEGACY_BOTLIB_AI_CHOOSE_LTG_ITEM,
    LEGACY_BOTLIB_AI_CHOOSE_NBG_ITEM,
    LEGACY_BOTLIB_AI_TOUCHING_GOAL,
    LEGACY_BOTLIB_AI_ITEM_GOAL_IN_VIS_BUT_NOT_VISIBLE,
    LEGACY_BOTLIB_AI_GET_LEVEL_ITEM_GOAL,
    LEGACY_BOTLIB_AI_AVOID_GOAL_TIME,
    LEGACY_BOTLIB_AI_INIT_LEVEL_ITEMS,
    LEGACY_BOTLIB_AI_UPDATE_ENTITY_ITEMS,
    LEGACY_BOTLIB_AI_LOAD_ITEM_WEIGHTS,
    LEGACY_BOTLIB_AI_FREE_ITEM_WEIGHTS,
    LEGACY_BOTLIB_AI_SAVE_GOAL_FUZZY_LOGIC,
    LEGACY_BOTLIB_AI_ALLOC_GOAL_STATE,
    LEGACY_BOTLIB_AI_FREE_GOAL_STATE,

    LEGACY_BOTLIB_AI_RESET_MOVE_STATE,
    LEGACY_BOTLIB_AI_MOVE_TO_GOAL,
    LEGACY_BOTLIB_AI_MOVE_IN_DIRECTION,
    LEGACY_BOTLIB_AI_RESET_AVOID_REACH,
    LEGACY_BOTLIB_AI_RESET_LAST_AVOID_REACH,
    LEGACY_BOTLIB_AI_REACHABILITY_AREA,
    LEGACY_BOTLIB_AI_MOVEMENT_VIEW_TARGET,
    LEGACY_BOTLIB_AI_ALLOC_MOVE_STATE,
    LEGACY_BOTLIB_AI_FREE_MOVE_STATE,
    LEGACY_BOTLIB_AI_INIT_MOVE_STATE,

    LEGACY_BOTLIB_AI_CHOOSE_BEST_FIGHT_WEAPON,
    LEGACY_BOTLIB_AI_GET_WEAPON_INFO,
    LEGACY_BOTLIB_AI_LOAD_WEAPON_WEIGHTS,
    LEGACY_BOTLIB_AI_ALLOC_WEAPON_STATE,
    LEGACY_BOTLIB_AI_FREE_WEAPON_STATE,
    LEGACY_BOTLIB_AI_RESET_WEAPON_STATE,

    LEGACY_BOTLIB_AI_GENETIC_PARENTS_AND_CHILD_SELECTION,
    LEGACY_BOTLIB_AI_INTERBREED_GOAL_FUZZY_LOGIC,
    LEGACY_BOTLIB_AI_MUTATE_GOAL_FUZZY_LOGIC,
    LEGACY_BOTLIB_AI_GET_NEXT_CAMP_SPOT_GOAL,
    LEGACY_BOTLIB_AI_GET_MAP_LOCATION_GOAL,
    LEGACY_BOTLIB_AI_NUM_INITIAL_CHATS,
    LEGACY_BOTLIB_AI_GET_CHAT_MESSAGE,
    LEGACY_BOTLIB_AI_REMOVE_FROM_AVOID_GOALS,
    LEGACY_BOTLIB_AI_PREDICT_VISIBLE_POSITION,

    LEGACY_BOTLIB_AI_SET_AVOID_GOAL_TIME,
    LEGACY_BOTLIB_AI_ADD_AVOID_SPOT,
    LEGACY_BOTLIB_AAS_ALTERNATIVE_ROUTE_GOAL,
    LEGACY_BOTLIB_AAS_PREDICT_ROUTE,
    LEGACY_BOTLIB_AAS_POINT_REACHABILITY_AREA_INDEX,

    LEGACY_BOTLIB_PC_LOAD_SOURCE,
    LEGACY_BOTLIB_PC_FREE_SOURCE,
    LEGACY_BOTLIB_PC_READ_TOKEN,
    LEGACY_BOTLIB_PC_SOURCE_FILE_AND_LINE,
    LEGACY_BOTLIB_PC_LOAD_GLOBAL_DEFINES,
    LEGACY_BOTLIB_PC_REMOVE_ALL_GLOBAL_DEFINES,

    LEGACY_G_G2_LISTBONES,
    LEGACY_G_G2_LISTSURFACES,
    LEGACY_G_G2_HAVEWEGHOULMODELS,
    LEGACY_G_G2_SETMODELS,
    LEGACY_G_G2_GETBOLT,
    LEGACY_G_G2_INITGHOUL2MODEL,
    LEGACY_G_G2_ADDBOLT,
    LEGACY_G_G2_SETBOLTINFO,
    LEGACY_G_G2_ANGLEOVERRIDE,
    LEGACY_G_G2_PLAYANIM,
    LEGACY_G_G2_GETGLANAME,
    LEGACY_G_G2_COPYGHOUL2INSTANCE,
    LEGACY_G_G2_COPYSPECIFICGHOUL2MODEL,
    LEGACY_G_G2_DUPLICATEGHOUL2INSTANCE,
    LEGACY_G_G2_REMOVEGHOUL2MODEL,
    LEGACY_G_G2_CLEANMODELS,

    // CGenericParser2 (void *) routines
    LEGACY_G_GP_PARSE,
    LEGACY_G_GP_PARSE_FILE,
    LEGACY_G_GP_CLEAN,
    LEGACY_G_GP_DELETE,
    LEGACY_G_GP_GET_BASE_PARSE_GROUP,

    // CGPGroup (void *) routines
    LEGACY_G_GPG_GET_NAME,
    LEGACY_G_GPG_GET_NEXT,
    LEGACY_G_GPG_GET_INORDER_NEXT,
    LEGACY_G_GPG_GET_INORDER_PREVIOUS,
    LEGACY_G_GPG_GET_PAIRS,
    LEGACY_G_GPG_GET_INORDER_PAIRS,
    LEGACY_G_GPG_GET_SUBGROUPS,
    LEGACY_G_GPG_GET_INORDER_SUBGROUPS,
    LEGACY_G_GPG_FIND_SUBGROUP,
    LEGACY_G_GPG_FIND_PAIR,
    LEGACY_G_GPG_FIND_PAIRVALUE,

    // CGPValue (void *) routines
    LEGACY_G_GPV_GET_NAME,
    LEGACY_G_GPV_GET_NEXT,
    LEGACY_G_GPV_GET_INORDER_NEXT,
    LEGACY_G_GPV_GET_INORDER_PREVIOUS,
    LEGACY_G_GPV_IS_LIST,
    LEGACY_G_GPV_GET_TOP_VALUE,
    LEGACY_G_GPV_GET_LIST,

    LEGACY_G_CM_REGISTER_TERRAIN,
    LEGACY_G_GET_MODEL_FORMALNAME,

    // Boe!Man 6/3/13: Dyanmic vm memory allocation.
    LEGACY_G_VM_LOCALALLOC,
    LEGACY_G_VM_LOCALALLOCUNALIGNED,
    LEGACY_G_VM_LOCALTEMPALLOC,
    LEGACY_G_VM_LOCALTEMPFREE,
    LEGACY_G_VM_LOCALSTRINGALLOC,

    LEGACY_G_G2_COLLISIONDETECT,
    LEGACY_G_G2_REGISTERSKIN,
    LEGACY_G_G2_SETSKIN,
    LEGACY_G_G2_GETANIMFILENAMEINDEX,

    LEGACY_G_GT_INIT,
    LEGACY_G_GT_RUNFRAME,
    LEGACY_G_GT_START,
    LEGACY_G_GT_SENDEVENT,

} legacyGameImport_t;


