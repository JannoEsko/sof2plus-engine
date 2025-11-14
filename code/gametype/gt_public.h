// Copyright (C) 2001-2002 Raven Software.
//
// gt_public.h -- game type module

#define GAMETYPE_API_VERSION    1

typedef enum
{
    //=================== Engine calls ======================
    GT_PRINT,                       // ( const char *string );
    GT_ERROR,                       // ( const char *string );
    GT_MILLISECONDS,                // ( void );

    GT_CVAR_REGISTER,               // ( vmCvar_t *vmCvar, const char *varName, const char *defaultValue, int flags );
    GT_CVAR_UPDATE,                 // ( vmCvar_t *vmCvar );
    GT_CVAR_SET,                    // ( const char *var_name, const char *value );
    GT_CVAR_VARIABLE_INTEGER_VALUE, // ( const char *var_name );
    GT_CVAR_VARIABLE_STRING_BUFFER, // ( const char *var_name, char *buffer, int bufsize );

    //================== Game module calls ==================
    GT_TEXTMESSAGE,                 // void ( int clientid, const char* message );
    GT_RESETITEM,                   // void ( int itemid );
    GT_GETCLIENTNAME,               // void ( int clientid, const char* buffer, int buffersize );

    GT_REGISTERSOUND,               // int  ( const char* filename );
    GT_STARTGLOBALSOUND,            // void ( int soundid );

    GT_REGISTERITEM,                // bool ( int itemid, const char* name, gtItemDef_t* def );
    GT_RADIOMESSAGE,                // void ( int clientid, const char* message );
    GT_REGISTERTRIGGER,             // bool ( int trigid, const char* name, gtTriggerDef_t* def );

    GT_DOESCLIENTHAVEITEM,          // bool ( int clientid, int itemid );

    GT_ADDTEAMSCORE,                // void ( team_t team, int score );
    GT_ADDCLIENTSCORE,              // void ( int clientid, int score );

    GT_RESTART,                     // void ( int delay );

    GT_REGISTEREFFECT,              // int  ( const char* name );
    GT_PLAYEFFECT,                  // void ( int effect, vec3_t origin, vec3_t angles );

    GT_REGISTERICON,                // int  ( const char* icon );

    GT_USETARGETS,                  // void ( const char* targetname );

    GT_GETCLIENTORIGIN,             // void ( int clientid, vec3_t origin );
    GT_GIVECLIENTITEM,              // void ( int clientid, int itemid );
    GT_TAKECLIENTITEM,              // void ( int clientid, int itemid );

    GT_SPAWNITEM,                   // void ( int itemid, vec3_t origin, vec3_t angles );

    GT_STARTSOUND,                  // void ( int soundid, vec3_t origin );
    GT_GETTRIGGERTARGET,            // void ( int triggerid, char* buffer, int buffersize );

    GT_GETCLIENTLIST,               // int  ( team_t team, int* clients, int clientcount );

    GT_SETHUDICON,                  // void ( int index, int icon );

} gametypeImport_t;

typedef enum
{
    //============== general Quake services ==================

    LEGACY_GT_PRINT,						// ( const char *string );
    LEGACY_GT_ERROR,						// ( const char *string );
    LEGACY_GT_MILLISECONDS,				// ( void );

    LEGACY_GT_CVAR_REGISTER,				// ( vmCvar_t *vmCvar, const char *varName, const char *defaultValue, int flags );
    LEGACY_GT_CVAR_UPDATE,					// ( vmCvar_t *vmCvar );
    LEGACY_GT_CVAR_SET,					// ( const char *var_name, const char *value );
    LEGACY_GT_CVAR_VARIABLE_INTEGER_VALUE,	// ( const char *var_name );
    LEGACY_GT_CVAR_VARIABLE_STRING_BUFFER,	// ( const char *var_name, char *buffer, int bufsize );

    LEGACY_GT_MEMSET = 100,
    LEGACY_GT_MEMCPY,
    LEGACY_GT_STRNCPY,
    LEGACY_GT_SIN,
    LEGACY_GT_COS,
    LEGACY_GT_ATAN2,
    LEGACY_GT_SQRT,
    LEGACY_GT_ANGLEVECTORS,
    LEGACY_GT_PERPENDICULARVECTOR,
    LEGACY_GT_FLOOR,
    LEGACY_GT_CEIL,
    LEGACY_GT_TESTPRINTINT,
    LEGACY_GT_TESTPRINTFLOAT,
    LEGACY_GT_ACOS,
    LEGACY_GT_ASIN,
    LEGACY_GT_MATRIXMULTIPLY,


    LEGACY_GT_TEXTMESSAGE,					// void ( int clientid, const char* message );
    LEGACY_GT_RESETITEM,					// void ( int itemid );
    LEGACY_GT_GETCLIENTNAME,				// void ( int clientid, const char* buffer, int buffersize );

    LEGACY_GT_REGISTERGLOBALSOUND,			// int  ( const char* filename );
    LEGACY_GT_STARTGLOBALSOUND,			// void ( int soundid );

    LEGACY_GT_REGISTERITEM,				// bool ( int itemid, const char* name, gtItemDef_t* def );
    LEGACY_GT_RADIOMESSAGE,				// void ( int clientid, const char* message );
    LEGACY_GT_REGISTERTRIGGER,				// bool ( int trigid, const char* name, gtTriggerDef_t* def );

    LEGACY_GT_GETCLIENTITEMS,				// void ( int clientid, int* buffer, int buffersize );
    LEGACY_GT_DOESCLIENTHAVEITEM,			// bool ( int clientid, int itemid );

    LEGACY_GT_ADDTEAMSCORE,				// void ( team_t team, int score );
    LEGACY_GT_ADDCLIENTSCORE,				// void ( int clientid, int score );

    LEGACY_GT_RESTART,						// void ( int delay );

    LEGACY_GT_REGISTEREFFECT,				// int	( const char* name );
    LEGACY_GT_PLAYEFFECT,					// void	( int effect, vec3_t origin, vec3_t angles );

    // Gold extras.
    LEGACY_GT_REGISTERICON,				// int	( const char* icon );
    LEGACY_GT_USETARGETS,					// void ( const char* targetname );
    LEGACY_GT_GETCLIENTORIGIN,				// void ( int clientid, vec3_t origin );
    LEGACY_GT_GIVECLIENTITEM,				// void ( int clientid, int itemid );
    LEGACY_GT_TAKECLIENTITEM,				// void ( int clientid, int itemid );
    LEGACY_GT_SPAWNITEM,					// void ( int itemid, vec3_t origin, vec3_t angles );
    LEGACY_GT_STARTSOUND,					// void ( int soundid, vec3_t origin );
    LEGACY_GT_GETTRIGGERTARGET,			// void ( int triggerid, char* buffer, int buffersize );
    LEGACY_GT_GETCLIENTLIST,				// int  ( team_t team, int* clients, int clientcount );
    LEGACY_GT_SETHUDICON,					// void	( int index, int icon );

} legacyGametypeImport_t;


typedef enum
{
    GAMETYPE_INIT,
    GAMETYPE_START,
    GAMETYPE_RUN_FRAME,
    GAMETYPE_EVENT,
    GAMETYPE_SHUTDOWN
} gametypeExport_t;

typedef enum
{
    GTEV_ITEM_DROPPED,          // void ( int itemID );
    GTEV_ITEM_TOUCHED,          // int  ( int itemID, int clientID, int clientTeam );

    GTEV_TRIGGER_TOUCHED,       // int  ( int trigID, int clientID, int clientTeam );

    GTEV_TEAM_ELIMINATED,       // void ( team_t team );
    GTEV_TIME_EXPIRED,          // void ( void );

    GTEV_ITEM_STUCK,            // void ( int itemID );

    GTEV_ITEM_DEFEND,           // void ( int itemID, int clientID, int clientTeam );

    GTEV_CLIENT_DEATH,          // void ( int clientID, int clientTeam, int killerID, int killerTeam );

    GTEV_TRIGGER_USED,          // int  ( int trigID, int clientID, int clientTeam );

    GTEV_TRIGGER_CANBEUSED,     // int  ( int trigID, int clientID, int clientTeam );

    GTEV_ITEM_CANBEUSED,        // int  ( int itemID, int clientID, int clientTeam );
    GTEV_ITEM_USED,             // int  ( int itemID, int clientID, int clientTeam );

    GTEV_MAX

} gametypeEvent_t;

typedef enum
{
    GTCMD_TEXTMESSAGE,				// void ( int client, const char* message );
    GTCMD_RESETITEM,				// void ( const char* itemName );
    GTCMD_GETCLIENTNAME,			// void ( int clientid, char* buffer, int buffersize );

    GTCMD_REGISTERSOUND,			// int  ( const char* soundFile );
    GTCMD_STARTGLOBALSOUND,			// void ( int soundid );

    GTCMD_REGISTERITEM,				// int  ( const char* name, gtItemDef_t* def );

    GTCMD_RADIOMESSAGE,				// void ( int clientid, const char* message );
    GTCMD_REGISTERTRIGGER,			// bool ( int triggerid, const char* message, gtTriggerDef_t* def );

    GTCMD_DOESCLIENTHAVEITEM,		// bool ( int clientid, int itemid );

    GTCMD_ADDTEAMSCORE,				// void ( team_t team, int score );
    GTCMD_ADDCLIENTSCORE,			// void ( int clientid, int score );

    GTCMD_RESTART,					// void ( int delay );

    GTCMD_REGISTEREFFECT,			// int	( const char* name );
    GTCMD_PLAYEFFECT,				// void ( int effect, vec3_t origin, vec3_t angles );

    GTCMD_REGISTERICON,				// int  ( const char* icon );

    GTCMD_USETARGETS,				// void ( const char* targetname );

    GTCMD_GETCLIENTORIGIN,			// void ( int clientid, vec3_t origin );
    GTCMD_GIVECLIENTITEM,			// void ( int clientid, int itemid );
    GTCMD_TAKECLIENTITEM,			// void ( int clientid, int itemid );

    GTCMD_SPAWNITEM,				// void ( int itemid, vec3_t origin, vec3_t angles );

    GTCMD_STARTSOUND,				// void ( int soundid, vec3_t origin );

    GTCMD_GETTRIGGERTARGET,			// void ( int triggerid, char* bufferr, int buffersize );

    GTCMD_GETCLIENTLIST,			// int  ( team_t team, int* clients, int clientcount );

    GTCMD_SETHUDICON,				// void ( int index, int icon );

} gametypeCommand_t;


typedef struct gtItemDef_s
{
    int         size;           // size of structure
    qboolean    use;            // whether or not the item needs to be used
    int         useTime;        // If the item needs to be used, this is the time it takes to use it
    int         useIcon;        // Icon to display on screen if the item requires using
    int         useSound;       // Sound to loop when using this item

} gtItemDef_t;

typedef struct gtTriggerDef_s
{
    int         size;           // size of structure
    qboolean    use;            // Whether or not the trigger needs to be used
    int         useTime;        // If the trigger needs to be used, this is the time it takes to use it
    int         useIcon;        // Icon to display on screen if the trigger requires using
    int         useSound;       // Sound to loop when using this trigger

} gtTriggerDef_t;

typedef struct gtItemDefSilver_s
{
    qboolean	use;			// whether or not the item needs to be used
    int			useTime;		// If the item needs to be used, this is the time it takes to use it

} gtItemDefSilver_t;

typedef struct gtTriggerDefSilver_s
{
    qboolean	use;			// Whether or not the trigger needs to be used
    int			useTime;		// If the trigger needs to be used, this is the time it takes to use it

} gtTriggerDefSilver_t;
