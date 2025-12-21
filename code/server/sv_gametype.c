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
// sv_gametype.c - Interface to the gametype DLL.

#include "server.h"
#include "../gametype/gt_public.h"

#define GT_TO_G_MAX_ARGS 5 // Max args passed from gtvm to gvm. Not accounting syscall num

// Local variable definitions.
static vm_t         *gtvm                   = NULL;   // Gametype virtual machine.
static intptr_t     gameVMAllocatedPointers[GT_TO_G_MAX_ARGS];

//==============================================

/*
====================
SV_GametypeSystemCalls

The module is making
a system call.
====================
*/

static int  FloatAsInt(float f) {
    floatint_t fi;
    fi.f = f;
    return fi.i;
}

intptr_t SV_GametypeSystemCalls(qboolean runningQVM, intptr_t *args)
{
    if (sv_gameModernABI->integer) {
        switch (args[0]) {
        case GT_PRINT:
            Com_Printf("%s", (const char*)VMA(1));
            return 0;
        case GT_ERROR:
            Com_Error(ERR_DROP, "%s", (const char*)VMA(1));
            return 0;
        case GT_MILLISECONDS:
            return Sys_Milliseconds();
        case GT_CVAR_REGISTER:
            Cvar_Register(VMA(1), VMA(2), VMA(3), args[4], VMF(5), VMF(6));
            return 0;
        case GT_CVAR_UPDATE:
            Cvar_Update(VMA(1));
            return 0;
        case GT_CVAR_SET:
            Cvar_SetSafe((const char*)VMA(1), (const char*)VMA(2));
            return 0;
        case GT_CVAR_VARIABLE_INTEGER_VALUE:
            return Cvar_VariableIntegerValue((const char*)VMA(1));
        case GT_CVAR_VARIABLE_STRING_BUFFER:
            Cvar_VariableStringBuffer(VMA(1), VMA(2), args[3]);
            return 0;

        default:
            break;
        }

        // The game module handles the remaining gametype syscalls.
        return VM_Call(gvm, GAME_GAMETYPE_COMMAND, args[0], args[1], args[2], args[3], args[4], args[5]);
    }
    else {
        switch (args[0]) {
            case LEGACY_GT_PRINT:
                Com_Printf("%s", (const char*)VMA(1));
                return 0;
            case LEGACY_GT_ERROR:
                Com_Error(ERR_DROP, "%s", (const char*)VMA(1));
                return 0;
            case LEGACY_GT_MILLISECONDS:
                return Sys_Milliseconds();
            case LEGACY_GT_CVAR_REGISTER:
                Cvar_Register(VMA(1), VMA(2), VMA(3), args[4], VMF(5), VMF(6));
                return 0;
            case LEGACY_GT_CVAR_UPDATE:
                Cvar_Update(VMA(1));
                return 0;
            case LEGACY_GT_CVAR_SET:
                Cvar_SetSafe((const char*)VMA(1), (const char*)VMA(2));
                return 0;
            case LEGACY_GT_CVAR_VARIABLE_INTEGER_VALUE:
                return Cvar_VariableIntegerValue((const char*)VMA(1));
                return 0;
            case LEGACY_GT_CVAR_VARIABLE_STRING_BUFFER:
                Cvar_VariableStringBuffer(VMA(1), VMA(2), args[3]);
                return 0;
            case LEGACY_GT_MEMSET:
                Com_Memset(VMA(1), args[2], args[3]);
                return 0;
            case LEGACY_GT_MEMCPY:
                Com_Memcpy(VMA(1), VMA(2), args[3]);
                return 0;
            case LEGACY_GT_STRNCPY:
                strncpy(VMA(1), VMA(2), args[3]);
                return 0;
            case LEGACY_GT_SIN:
                return FloatAsInt(sin(VMF(1)));
            case LEGACY_GT_COS:
                return FloatAsInt(cos(VMF(1)));
            case LEGACY_GT_ATAN2:
                return FloatAsInt(atan2(VMF(1), VMF(2)));
            case LEGACY_GT_SQRT:
                return FloatAsInt(sqrt(VMF(1)));
            case LEGACY_GT_ANGLEVECTORS:
                AngleVectors(VMA(1), VMA(2), VMA(3), VMA(4));
                return 0;
            case LEGACY_GT_PERPENDICULARVECTOR:
                PerpendicularVector(VMA(1), VMA(2));
                return 0;
            case LEGACY_GT_FLOOR:
                return FloatAsInt(floor(VMF(1)));
            case LEGACY_GT_CEIL:
                return FloatAsInt(ceil(VMF(1)));
            case LEGACY_GT_TESTPRINTINT:
                Com_Printf("GT_TESTPRINTINT: %d\r\n", args[1]);
                return 0;
            case LEGACY_GT_TESTPRINTFLOAT:
                Com_Printf("GT_TESTPRINTFLOAT: %f\r\n", VMF(1));
                return 0;
            case LEGACY_GT_ACOS:
                return FloatAsInt(acos(VMF(1)));
            case LEGACY_GT_ASIN:
                return FloatAsInt(asin(VMF(1)));
            case LEGACY_GT_MATRIXMULTIPLY:
                MatrixMultiply(VMA(1), VMA(2), VMA(3));
                return 0;

            // Rest follow the VM_Call lower, just that the case is used to set up args in the calls.

            case LEGACY_GT_TEXTMESSAGE: {
                // void trap_Cmd_TextMessage ( int client, const char* message )
                intptr_t ptr = VM_ExplicitArgPtr(gvm, gameVMAllocatedPointers[0]);
                strcpy(ptr, VMA(2));
                args[2] = gameVMAllocatedPointers[0];
                break;
            }

            case LEGACY_GT_RESETITEM: {
                // void trap_Cmd_ResetItem ( int itemid )
                // Ints work fine, so we do nothing here.
                break;
            }

            case LEGACY_GT_GETCLIENTNAME: {
                // void trap_Cmd_GetClientName ( int clientid, const char* buffer, int buffersize )
                // Write buf to the game module allocated pointer, then read it back and write to GT
                intptr_t retval = VM_Call(gvm, GAME_GAMETYPE_COMMAND, GTCMD_GETCLIENTNAME, args[1], gameVMAllocatedPointers[0], args[3]);
                strncpy(VMA(2), VM_ExplicitArgPtr(gvm, gameVMAllocatedPointers[0]), args[3]);
                return retval;
            }

            case LEGACY_GT_REGISTERGLOBALSOUND: {
                // int trap_Cmd_RegisterGlobalSound ( const char* sound )
                intptr_t ptr = VM_ExplicitArgPtr(gvm, gameVMAllocatedPointers[0]);
                strcpy(ptr, VMA(1));
                args[1] = gameVMAllocatedPointers[0];
                break;
            }

            case LEGACY_GT_STARTGLOBALSOUND: {
                // void trap_Cmd_StartGlobalSound ( int sound )
                // Int index only, so we do nothing here.
                break;
            }

            case LEGACY_GT_REGISTERITEM: {
                // qboolean trap_Cmd_RegisterItem ( int itemid, const char* name, gtItemDef_t* def )
                // Arg2 is strcpy
                strcpy(VM_ExplicitArgPtr(gvm, gameVMAllocatedPointers[0]), VMA(2));

                // Now I wonder whether I need the struct here to actually copy it...
                // It's different in sizes when in gold vs when in silver.
                // The client can also define it however they see fit...

                if (!net_runningLegacy->integer) {
                    gtItemDef_t* itemDefPtr = VM_ExplicitArgPtr(gvm, gameVMAllocatedPointers[1]);
                    gtItemDef_t* gtItemDef = VMA(3);
                    itemDefPtr->size = gtItemDef->size;
                    itemDefPtr->use = gtItemDef->use;
                    itemDefPtr->useIcon = gtItemDef->useIcon;
                    itemDefPtr->useSound = gtItemDef->useSound;
                    itemDefPtr->useTime = gtItemDef->useTime;
                }
                else {
                    gtItemDefSilver_t* itemDefPtr = VM_ExplicitArgPtr(gvm, gameVMAllocatedPointers[1]);
                    gtItemDefSilver_t* gtItemDef = VMA(3);

                    // This truly takes the assumption that the struct has not been changed.
                    
                    itemDefPtr->use = gtItemDef->use;
                    itemDefPtr->useTime = gtItemDef->useTime;
                }

                args[2] = gameVMAllocatedPointers[0];
                args[3] = gameVMAllocatedPointers[1];

                // Game module does nothing more with them, so we can just break here.
                break;
            }

            case LEGACY_GT_RADIOMESSAGE: {
                // void trap_Cmd_RadioMessage ( int client, const char* message )
                strcpy(VM_ExplicitArgPtr(gvm, gameVMAllocatedPointers[0]), VMA(2));
                args[2] = gameVMAllocatedPointers[0];
                break;
            }

            case LEGACY_GT_REGISTERTRIGGER: {
                // qboolean trap_Cmd_RegisterTrigger ( int trigid, const char* name, gtTriggerDef_t* def )
                strcpy(VM_ExplicitArgPtr(gvm, gameVMAllocatedPointers[0]), VMA(2));

                if (!net_runningLegacy->integer) {
                    gtTriggerDef_t* triggerDefPtr = VM_ExplicitArgPtr(gvm, gameVMAllocatedPointers[1]);
                    gtTriggerDef_t* gtTriggerDef = VMA(3);
                    triggerDefPtr->size = gtTriggerDef->size;
                    triggerDefPtr->use = gtTriggerDef->use;
                    triggerDefPtr->useIcon = gtTriggerDef->useIcon;
                    triggerDefPtr->useSound = gtTriggerDef->useSound;
                    triggerDefPtr->useTime = gtTriggerDef->useTime;
                }
                else {
                    gtTriggerDefSilver_t* triggerDefPtr = VM_ExplicitArgPtr(gvm, gameVMAllocatedPointers[1]);
                    gtTriggerDefSilver_t* gtTriggerDef = VMA(3);

                    // This truly takes the assumption that the struct has not been changed.

                    triggerDefPtr->use = gtTriggerDef->use;
                    triggerDefPtr->useTime = gtTriggerDef->useTime;
                }

                args[2] = gameVMAllocatedPointers[0];
                args[3] = gameVMAllocatedPointers[1];

                // Game module does nothing more with them, so we can just break here.
                break;
            }

            case LEGACY_GT_GETCLIENTITEMS: {
                // void trap_Cmd_GetClientItems ( int clientid, int* buffer, int buffersize )
                // This actually misses an implementation even in the original engine, both silver and gold.
                Com_DPrintf("SV_GametypeSystemCalls: LEGACY_GT_GETCLIENTITEMS not implemented.\n");
                return 0;
            }

            case LEGACY_GT_DOESCLIENTHAVEITEM: {
                // qboolean trap_Cmd_DoesClientHaveItem ( int clientid, int itemid )
                // Int values, don't need to do anything.
                break;
            }

            case LEGACY_GT_ADDTEAMSCORE: {
                // void trap_Cmd_AddTeamScore ( team_t team, int score )
                // Int values, don't need to do anything.
                break;
            }

            case LEGACY_GT_ADDCLIENTSCORE: {
                // void trap_Cmd_AddClientScore ( int clientid, int score )
                // Int values, don't need to do anything.
                break;
            }

            case LEGACY_GT_RESTART: {
                // void trap_Cmd_Restart ( int delay )
                // Int values, don't need to do anything.
                break;
            }

            case LEGACY_GT_REGISTEREFFECT: {
                // int trap_Cmd_RegisterEffect ( const char* effect )

                strcpy(VM_ExplicitArgPtr(gvm, gameVMAllocatedPointers[0]), VMA(1));
                args[1] = gameVMAllocatedPointers[0];

                break;
            }

            case LEGACY_GT_PLAYEFFECT: {
                // void trap_Cmd_PlayEffect ( int effect, vec3_t origin, vec3_t angles )

                // vec3_t's need to be handled.

                vec3_t* origin = VMA(2);
                vec3_t* angles = VMA(3);

                vec3_t* gvmOrigin = VM_ExplicitArgPtr(gvm, gameVMAllocatedPointers[0]);
                vec3_t* gvmAngles = VM_ExplicitArgPtr(gvm, gameVMAllocatedPointers[1]);

                VectorCopy(*origin, *gvmOrigin);
                VectorCopy(*angles, *gvmAngles);

                args[2] = gameVMAllocatedPointers[0];
                args[3] = gameVMAllocatedPointers[1];

                break;
            }

            // Gold extras.

            case LEGACY_GT_REGISTERICON: {
                // int trap_Cmd_RegisterIcon ( const char* icon )
                strcpy(VM_ExplicitArgPtr(gvm, gameVMAllocatedPointers[0]), VMA(1));
                args[1] = gameVMAllocatedPointers[0];
                break;
            }

            case LEGACY_GT_USETARGETS: {
                // void trap_Cmd_UseTargets ( const char* targetname )
                strcpy(VM_ExplicitArgPtr(gvm, gameVMAllocatedPointers[0]), VMA(1));
                args[1] = gameVMAllocatedPointers[0];
                break;
            }

            case LEGACY_GT_GETCLIENTORIGIN: {
                // void trap_Cmd_GetClientOrigin ( int clientid, vec3_t origin )
                vec3_t* gvmOrigin = VM_ExplicitArgPtr(gvm, gameVMAllocatedPointers[0]);
                vec3_t* gtvmOrigin = VMA(2);
                args[2] = gameVMAllocatedPointers[0];

                intptr_t retval = VM_Call(gvm, GAME_GAMETYPE_COMMAND, GTCMD_GETCLIENTORIGIN, args[1], args[2]);

                VectorCopy(*gvmOrigin, *gtvmOrigin);

                return retval;
            }

            case LEGACY_GT_GIVECLIENTITEM: {
                // void trap_Cmd_GiveClientItem ( int clientid, int itemid )
                // Int values, don't need to do anything.
                break;
            }

            case LEGACY_GT_TAKECLIENTITEM: {
                // void trap_Cmd_TakeClientItem ( int clientid, int itemid )
                // Int values, don't need to do anything.
                break;
            }

            case LEGACY_GT_SPAWNITEM: {
                // void trap_Cmd_SpawnItem ( int clientid, vec3_t origin, vec3_t angles )
                vec3_t* origin = VMA(2);
                vec3_t* angles = VMA(3);

                vec3_t* gvmOrigin = VM_ExplicitArgPtr(gvm, gameVMAllocatedPointers[0]);
                vec3_t* gvmAngles = VM_ExplicitArgPtr(gvm, gameVMAllocatedPointers[1]);

                VectorCopy(*origin, *gvmOrigin);
                VectorCopy(*angles, *gvmAngles);

                args[2] = gameVMAllocatedPointers[0];
                args[3] = gameVMAllocatedPointers[1];

                break;
            }

            case LEGACY_GT_STARTSOUND: {
                // void trap_Cmd_StartSound ( int sound, vec3_t origin )
                vec3_t* origin = VMA(2);
                vec3_t* gvmOrigin = VM_ExplicitArgPtr(gvm, gameVMAllocatedPointers[0]);
                VectorCopy(*origin, *gvmOrigin);
                args[2] = gameVMAllocatedPointers[0];
                break;
            }

            case LEGACY_GT_GETTRIGGERTARGET: {
                // void trap_Cmd_GetTriggerTarget ( int triggerid, const char* buffer, int buffersize )
                intptr_t retval = VM_Call(gvm, GAME_GAMETYPE_COMMAND, GTCMD_GETTRIGGERTARGET, args[1], gameVMAllocatedPointers[0], args[3]);
                strncpy(VMA(2), VM_ExplicitArgPtr(gvm, gameVMAllocatedPointers[0]), args[3]);
                return retval;
            }

            case LEGACY_GT_GETCLIENTLIST: {
                // int trap_Cmd_GetClientList ( team_t team, int* clients, int clientcount )
                intptr_t retval = VM_Call(gvm, GAME_GAMETYPE_COMMAND, GTCMD_GETCLIENTLIST, args[1], gameVMAllocatedPointers[0], args[3]);
                Com_Memcpy(VMA(2), VM_ExplicitArgPtr(gvm, gameVMAllocatedPointers[0]), args[3] * sizeof(int));
                return retval;
            }

            case LEGACY_GT_SETHUDICON: {
                // void trap_Cmd_SetHUDIcon ( int index, int icon )
                // Int values, don't need to do anything.
                break;
            }

            default:
                break;

        }

        return VM_Call(gvm, GAME_GAMETYPE_COMMAND, (args[0] > LEGACY_GT_GETCLIENTITEMS ? (args[0] - LEGACY_GT_TEXTMESSAGE - 1) : (args[0] - LEGACY_GT_TEXTMESSAGE)), args[1], args[2], args[3], args[4], args[5]); 
        // Adjustment is because for whatever reason, the GT syscalls inside the game module are GTCMD's which start from 0 - GTCMD_TEXTMESSAGE.
        // And, GT_GETCLIENTITEMS doesn't exist in GTCMD.
    }
}

/*
===================
SV_RestartGametypeProgs

Called on a map_restart, but
not on a normal map change.
===================
*/

static void SV_RestartGametypeProgs(void)
{
    if(!gtvm){
        return;
    }

    // Do a restart instead of a free.
    gtvm = VM_Restart(gtvm, qtrue);
    if(!gtvm){
        Com_Error(ERR_FATAL, "VM_Restart on gametype failed");
    }
}

/*
===============
SV_InitGametypeProgs

Called on both a normal map change
and a map_restart.
===============
*/

static void SV_InitGametypeProgs(const char *gametype)
{
    // Load the specified gametype DLL.
    gtvm = VM_Create(va("gt_%s", gametype), SV_GametypeSystemCalls, Cvar_VariableIntegerValue("vm_gametype"));
    if(!gtvm){
        Com_Error(ERR_FATAL, "VM_Create on gametype failed");
    }

    // NB - SoF2MP allocates 5 pointers to the game module here.

    /*
    for ( i = 0; i < 5; ++i )
    dword_82BE8FC[i] = sub_80A0824((_DWORD *)gameModuleVM, 4096);
    
    */

    for (int i = 0; i < GT_TO_G_MAX_ARGS; ++i) {
        gameVMAllocatedPointers[i] = QVM_Local_Alloc(4096);
    }

}

/*
===============
SV_GT_Init

Load and initialize the requested
gametype module.
===============
*/

void SV_GT_Init(const char *gametype, qboolean restart)
{

    if (restart && gtvm) {
        SV_RestartGametypeProgs();
    }
    else {
        SV_InitGametypeProgs(gametype);
    }

    // Execute the initialization routine
    // of the loaded gametype module.
    VM_Call(gtvm, GAMETYPE_INIT);
}

/*
===============
SV_GT_RunFrame

Advance non-player objects
in the gametype.
===============
*/

void SV_GT_RunFrame(int time)
{
    if (gtvm) {
        VM_Call(gtvm, GAMETYPE_RUN_FRAME, time);
    }
}

/*
===============
SV_GT_Start

Start the gametype.
===============
*/

void SV_GT_Start(int time)
{
    if (gtvm) {
        VM_Call(gtvm, GAMETYPE_START, time);
    }
}

/*
===============
SV_GT_SendEvent

Send an event to the
gametype module.
===============
*/

int SV_GT_SendEvent(int event, int time, int arg0, int arg1, int arg2, int arg3, int arg4)
{
    if (gtvm) {
        return VM_Call(gtvm, GAMETYPE_EVENT, event, time, arg0, arg1, arg2, arg3, arg4);
    }

    return 0;
}

/*
===============
SV_GT_Shutdown

Called every time a map
changes.
===============
*/

void SV_GT_Shutdown(void)
{
    if(!gtvm){
        return;
    }

    VM_Call(gtvm, GAMETYPE_SHUTDOWN);
    VM_Free(gtvm);
    gtvm = NULL;
}
