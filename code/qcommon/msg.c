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
#include "q_shared.h"
#include "qcommon.h"
#include "../game/bg_public.h"

static huffman_t        msgHuff;

static qboolean         msgInit = qfalse;

int pcount[256];


meansOfDeathDiff_t meansOfDeathTranslations[] = {
    { MOD_UNKNOWN, L_MOD_UNKNOWN },
    { MOD_KNIFE, L_MOD_KNIFE },
    { MOD_M1911A1_PISTOL, L_MOD_M1911A1_PISTOL },
    { MOD_USSOCOM_PISTOL, L_MOD_USSOCOM_PISTOL },
    { MOD_SILVER_TALON, L_MOD_M1911A1_PISTOL },
    { MOD_M590_SHOTGUN, L_MOD_M590_SHOTGUN },
    { MOD_MICRO_UZI_SUBMACHINEGUN, L_MOD_MICRO_UZI_SUBMACHINEGUN },
    { MOD_M3A1_SUBMACHINEGUN, L_MOD_M3A1_SUBMACHINEGUN },
    { MOD_MP5, L_MOD_M4_ASSAULT_RIFLE },
    { MOD_USAS_12_SHOTGUN, L_MOD_USAS_12_SHOTGUN },
    { MOD_M4_ASSAULT_RIFLE, L_MOD_M4_ASSAULT_RIFLE },
    { MOD_AK74_ASSAULT_RIFLE, L_MOD_AK74_ASSAULT_RIFLE },
    { MOD_SIG551, L_MOD_M4_ASSAULT_RIFLE },
    { MOD_MSG90A1_SNIPER_RIFLE, L_MOD_MSG90A1_SNIPER_RIFLE },
    { MOD_M60_MACHINEGUN, L_MOD_M60_MACHINEGUN },
    { MOD_MM1_GRENADE_LAUNCHER, L_MOD_MM1_GRENADE_LAUNCHER },
    { MOD_RPG7_LAUNCHER, L_MOD_RPG7_LAUNCHER },
    { MOD_M84_GRENADE, L_MOD_M84_GRENADE },
    { MOD_SMOHG92_GRENADE, L_MOD_SMOHG92_GRENADE },
    { MOD_ANM14_GRENADE, L_MOD_ANM14_GRENADE },
    { MOD_M15_GRENADE, L_MOD_M15_GRENADE },
    { MOD_WATER, L_MOD_WATER },
    { MOD_CRUSH, L_MOD_CRUSH },
    { MOD_TELEFRAG, L_MOD_TELEFRAG },
    { MOD_FALLING, L_MOD_FALLING },
    { MOD_SUICIDE, L_MOD_SUICIDE },
    { MOD_TEAMCHANGE, L_MOD_TEAMCHANGE },
    { MOD_TARGET_LASER, L_MOD_TARGET_LASER },
    { MOD_TRIGGER_HURT, L_MOD_TRIGGER_HURT },
    { MOD_TRIGGER_HURT_NOSUICIDE, L_MOD_TRIGGER_HURT_NOSUICIDE }
};

weaponDiff_t weaponTranslations[] = {
    { WP_NONE, L_WP_NONE },
    { WP_KNIFE, L_WP_KNIFE },
    { WP_M1911A1_PISTOL, L_WP_M1911A1_PISTOL },
    { WP_USSOCOM_PISTOL, L_WP_USSOCOM_PISTOL },
    { WP_SILVER_TALON, L_WP_NONE },
    { WP_M590_SHOTGUN, L_WP_M590_SHOTGUN },
    { WP_MICRO_UZI_SUBMACHINEGUN, L_WP_MICRO_UZI_SUBMACHINEGUN },
    { WP_M3A1_SUBMACHINEGUN, L_WP_M3A1_SUBMACHINEGUN },
    { WP_MP5, L_WP_NONE },
    { WP_USAS_12_SHOTGUN, L_WP_USAS_12_SHOTGUN },
    { WP_M4_ASSAULT_RIFLE, L_WP_M4_ASSAULT_RIFLE },
    { WP_AK74_ASSAULT_RIFLE, L_WP_AK74_ASSAULT_RIFLE },
    { WP_SIG551, L_WP_NONE },
    { WP_MSG90A1, L_WP_MSG90A1 },
    { WP_M60_MACHINEGUN, L_WP_M60_MACHINEGUN },
    { WP_MM1_GRENADE_LAUNCHER, L_WP_MM1_GRENADE_LAUNCHER },
    { WP_RPG7_LAUNCHER, L_WP_RPG7_LAUNCHER },
    { WP_M84_GRENADE, L_WP_M84_GRENADE },
    { WP_SMOHG92_GRENADE, L_WP_SMOHG92_GRENADE },
    { WP_ANM14_GRENADE, L_WP_ANM14_GRENADE },
    { WP_M15_GRENADE, L_WP_M15_GRENADE },
    //{ WP_M67_GRENADE, L_WP_M67_GRENADE },
    //{ WP_F1_GRENADE, L_WP_F1_GRENADE },
    //{ WP_L2A2_GRENADE, L_WP_L2A2_GRENADE },
    //{ WP_MDN11_GRENADE, L_WP_MDN11_GRENADE }
};

weaponDiff_t weaponTranslationReversed[] = {
    { L_WP_NONE, WP_NONE },
    { L_WP_KNIFE, WP_KNIFE },
    { L_WP_M1911A1_PISTOL, WP_M1911A1_PISTOL },
    { L_WP_USSOCOM_PISTOL, WP_USSOCOM_PISTOL },
    { L_WP_M590_SHOTGUN, WP_M590_SHOTGUN },
    { L_WP_MICRO_UZI_SUBMACHINEGUN, WP_MICRO_UZI_SUBMACHINEGUN },
    { L_WP_M3A1_SUBMACHINEGUN, WP_M3A1_SUBMACHINEGUN },
    { L_WP_USAS_12_SHOTGUN, WP_USAS_12_SHOTGUN },
    { L_WP_M4_ASSAULT_RIFLE, WP_M4_ASSAULT_RIFLE },
    { L_WP_AK74_ASSAULT_RIFLE, WP_AK74_ASSAULT_RIFLE },
    { L_WP_MSG90A1, WP_MSG90A1 },
    { L_WP_M60_MACHINEGUN, WP_M60_MACHINEGUN },
    { L_WP_MM1_GRENADE_LAUNCHER, WP_MM1_GRENADE_LAUNCHER },
    { L_WP_RPG7_LAUNCHER, WP_RPG7_LAUNCHER },
    { L_WP_M67_GRENADE, WP_NONE }, // JANFIXME - translations for additional nades from Client Additions
    { L_WP_M84_GRENADE, WP_M84_GRENADE },
    { L_WP_F1_GRENADE, WP_NONE },
    { L_WP_L2A2_GRENADE, WP_NONE },
    { L_WP_MDN11_GRENADE, WP_NONE },
    { L_WP_SMOHG92_GRENADE, WP_SMOHG92_GRENADE },
    { L_WP_ANM14_GRENADE, WP_ANM14_GRENADE },
    { L_WP_M15_GRENADE, WP_M15_GRENADE }
};

ammoDiff_t ammoTranslations[] = {
    { AMMO_KNIFE, L_AMMO_KNIFE},
    { AMMO_045, L_AMMO_045 },
    { AMMO_556, L_AMMO_556 },
    { AMMO_9, L_AMMO_9 },
    { AMMO_12, L_AMMO_12 },
    { AMMO_762, L_AMMO_762 },
    { AMMO_40, L_AMMO_40 },
    { AMMO_RPG7, L_AMMO_RPG7 },
    { AMMO_M15, L_AMMO_M15 },
    { AMMO_M84, L_AMMO_M84 },
    { AMMO_SMOHG92, L_AMMO_SMOHG92 },
    { AMMO_ANM14, L_AMMO_ANM14 },
    { AMMO_762_BELT, L_AMMO_762 },
    { AMMO_MP5_9, L_AMMO_NONE },
    { AMMO_MAX, L_AMMO_NONE },
    { AMMO_NONE, L_AMMO_NONE }
};

ammoDiff_t ammoTranslationsReversed[] = {
    { L_AMMO_KNIFE, AMMO_KNIFE},
    { L_AMMO_045, AMMO_045 },
    { L_AMMO_556, AMMO_556 },
    { L_AMMO_9, AMMO_9 },
    { L_AMMO_12, AMMO_12 },
    { L_AMMO_762, AMMO_762 },
    { L_AMMO_40, AMMO_40 },
    { L_AMMO_RPG7, AMMO_RPG7 },
    { L_AMMO_M15, AMMO_M15 },
    { L_AMMO_M84, AMMO_M84 },
    { L_AMMO_SMOHG92, AMMO_SMOHG92 },
    { L_AMMO_ANM14, AMMO_ANM14 },
    { L_AMMO_762, AMMO_762_BELT },
    { L_AMMO_NONE, AMMO_NONE }
};

int translateSilverAmmoToGoldAmmo(int input) {

    if (input < 0 || input >= sizeof(ammoTranslationsReversed) / sizeof(ammoTranslationsReversed[0])) {
        return input;
    }

    return ammoTranslationsReversed[input].translatedAmmo;

}

modelIndexDiff_t modelIndexTranslations[] = {
    {MODELINDEX_NONE, L_MODELINDEX_NONE},
    {MODELINDEX_ARMOR_BIG, L_MODELINDEX_ARMOR_BIG},
    {MODELINDEX_ARMOR_MEDIUM, L_MODELINDEX_ARMOR_MEDIUM},
    {MODELINDEX_ARMOR_SMALL, L_MODELINDEX_ARMOR_SMALL},
    {MODELINDEX_HEALTH_BIG, L_MODELINDEX_HEALTH_BIG},
    {MODELINDEX_HEALTH_SMALL, L_MODELINDEX_HEALTH_BIG},
    {MODELINDEX_WEAPON_KNIFE, L_MODELINDEX_WEAPON_KNIFE},
    {MODELINDEX_WEAPON_SOCOM, L_MODELINDEX_WEAPON_SOCOM},
    {MODELINDEX_WEAPON_M19, L_MODELINDEX_WEAPON_M19},
    {MODELINDEX_WEAPON_SILVERTALON, L_MODELINDEX_NONE}, // whilst M19 would be fine as for displaying purposes, Silver Talon should be removed.
    {MODELINDEX_WEAPON_MICROUZI, L_MODELINDEX_WEAPON_MICROUZI},
    {MODELINDEX_WEAPON_M3A1, L_MODELINDEX_WEAPON_M3A1},
    {MODELINDEX_WEAPON_MP5, L_MODELINDEX_WEAPON_MICROUZI},
    {MODELINDEX_WEAPON_USAS12, L_MODELINDEX_WEAPON_USAS12},
    {MODELINDEX_WEAPON_M590, L_MODELINDEX_WEAPON_M590},
    {MODELINDEX_WEAPON_MSG90A1, L_MODELINDEX_WEAPON_MSG90A1},
    {MODELINDEX_WEAPON_M4, L_MODELINDEX_WEAPON_M4},
    {MODELINDEX_WEAPON_AK74, L_MODELINDEX_WEAPON_AK74},
    {MODELINDEX_WEAPON_SIG551, L_MODELINDEX_NONE}, // SIG551 should be disabled altogether due to scope.
    {MODELINDEX_WEAPON_M60, L_MODELINDEX_WEAPON_M60},
    {MODELINDEX_WEAPON_RPG7, L_MODELINDEX_WEAPON_RPG7},
    {MODELINDEX_WEAPON_MM1, L_MODELINDEX_WEAPON_MM1},
    {MODELINDEX_WEAPON_M84, L_MODELINDEX_WEAPON_M84},
    {MODELINDEX_WEAPON_SMOHG92, L_MODELINDEX_WEAPON_SMOHG92},
    {MODELINDEX_WEAPON_ANM14, L_MODELINDEX_WEAPON_ANM14},
    {MODELINDEX_WEAPON_M15, L_MODELINDEX_WEAPON_M15},
    {MODELINDEX_AMMO_045, L_MODELINDEX_AMMO_045},
    {MODELINDEX_AMMO_9MM, L_MODELINDEX_AMMO_9MM},
    {MODELINDEX_AMMO_12GAUGE, L_MODELINDEX_AMMO_12GAUGE},
    {MODELINDEX_AMMO_762, L_MODELINDEX_AMMO_762},
    {MODELINDEX_AMMO_556, L_MODELINDEX_AMMO_556},
    {MODELINDEX_AMMO_40MM, L_MODELINDEX_AMMO_40MM},
    {MODELINDEX_AMMO_RPG7, L_MODELINDEX_AMMO_RPG7},
    {MODELINDEX_BACKPACK, L_MODELINDEX_BACKPACK},
    {MODELINDEX_GAMETYPE_ITEM, L_MODELINDEX_GAMETYPE_ITEM},
    {MODELINDEX_GAMETYPE_ITEM_2, L_MODELINDEX_GAMETYPE_ITEM_2},
    {MODELINDEX_GAMETYPE_ITEM_3, L_MODELINDEX_GAMETYPE_ITEM_3},
    {MODELINDEX_GAMETYPE_ITEM_4, L_MODELINDEX_GAMETYPE_ITEM_4},
    {MODELINDEX_GAMETYPE_ITEM_5, L_MODELINDEX_GAMETYPE_ITEM_5},
    {MODELINDEX_ARMOR, L_MODELINDEX_ARMOR},
    {MODELINDEX_NIGHTVISION, L_MODELINDEX_NIGHTVISION},
    {MODELINDEX_THERMAL, L_MODELINDEX_THERMAL},
    //{MODELINDEX_WEAPON_M67, L_MODELINDEX_WEAPON_M67},
    //{MODELINDEX_WEAPON_F1, L_MODELINDEX_WEAPON_F1},
    //{MODELINDEX_WEAPON_L2A2, L_MODELINDEX_WEAPON_L2A2},
    //{MODELINDEX_WEAPON_MDN11, L_MODELINDEX_WEAPON_MDN11}
};

modelIndexDiff_t modelIndexTranslationsReversed[] = {
    { L_MODELINDEX_NONE, MODELINDEX_NONE },
    { L_MODELINDEX_ARMOR_BIG, MODELINDEX_ARMOR_BIG },
    { L_MODELINDEX_ARMOR_MEDIUM, MODELINDEX_ARMOR_MEDIUM },
    { L_MODELINDEX_ARMOR_SMALL, MODELINDEX_ARMOR_SMALL },
    { L_MODELINDEX_HEALTH_BIG, MODELINDEX_HEALTH_BIG },
    { L_MODELINDEX_HEALTH_SMALL, MODELINDEX_HEALTH_SMALL },
    { L_MODELINDEX_WEAPON_KNIFE, MODELINDEX_WEAPON_KNIFE },
    { L_MODELINDEX_WEAPON_SOCOM, MODELINDEX_WEAPON_SOCOM },
    { L_MODELINDEX_WEAPON_M19, MODELINDEX_WEAPON_M19 },
    { L_MODELINDEX_WEAPON_MICROUZI, MODELINDEX_WEAPON_MICROUZI },
    { L_MODELINDEX_WEAPON_M3A1, MODELINDEX_WEAPON_M3A1 },
    { L_MODELINDEX_WEAPON_USAS12, MODELINDEX_WEAPON_USAS12 },
    { L_MODELINDEX_WEAPON_M590, MODELINDEX_WEAPON_M590 },
    { L_MODELINDEX_WEAPON_MSG90A1, MODELINDEX_WEAPON_MSG90A1 },
    { L_MODELINDEX_WEAPON_M4, MODELINDEX_WEAPON_M4 },
    { L_MODELINDEX_WEAPON_AK74, MODELINDEX_WEAPON_AK74 },
    { L_MODELINDEX_WEAPON_M60, MODELINDEX_WEAPON_M60 },
    { L_MODELINDEX_WEAPON_RPG7, MODELINDEX_WEAPON_RPG7 },
    { L_MODELINDEX_WEAPON_MM1, MODELINDEX_WEAPON_MM1 },
    //{ L_MODELINDEX_WEAPON_M67, MODELINDEX_WEAPON_M67 },
    { L_MODELINDEX_WEAPON_M84, MODELINDEX_WEAPON_M84 },
    //{ L_MODELINDEX_WEAPON_F1, MODELINDEX_WEAPON_F1 },
    //{ L_MODELINDEX_WEAPON_L2A2, MODELINDEX_WEAPON_L2A2 },
    //{ L_MODELINDEX_WEAPON_MDN11, MODELINDEX_WEAPON_MDN11 },
    { L_MODELINDEX_WEAPON_SMOHG92, MODELINDEX_WEAPON_SMOHG92 },
    { L_MODELINDEX_WEAPON_ANM14, MODELINDEX_WEAPON_ANM14 },
    { L_MODELINDEX_WEAPON_M15, MODELINDEX_WEAPON_M15 },
    { L_MODELINDEX_AMMO_045, MODELINDEX_AMMO_045 },
    { L_MODELINDEX_AMMO_9MM, MODELINDEX_AMMO_9MM },
    { L_MODELINDEX_AMMO_12GAUGE, MODELINDEX_AMMO_12GAUGE },
    { L_MODELINDEX_AMMO_762, MODELINDEX_AMMO_762 },
    { L_MODELINDEX_AMMO_556, MODELINDEX_AMMO_556 },
    { L_MODELINDEX_AMMO_40MM, MODELINDEX_AMMO_40MM },
    { L_MODELINDEX_AMMO_RPG7, MODELINDEX_AMMO_RPG7 },
    { L_MODELINDEX_BACKPACK, MODELINDEX_BACKPACK },
    { L_MODELINDEX_GAMETYPE_ITEM, MODELINDEX_GAMETYPE_ITEM },
    { L_MODELINDEX_GAMETYPE_ITEM_2, MODELINDEX_GAMETYPE_ITEM_2 },
    { L_MODELINDEX_GAMETYPE_ITEM_3, MODELINDEX_GAMETYPE_ITEM_3 },
    { L_MODELINDEX_GAMETYPE_ITEM_4, MODELINDEX_GAMETYPE_ITEM_4 },
    { L_MODELINDEX_GAMETYPE_ITEM_5, MODELINDEX_GAMETYPE_ITEM_5 },
    { L_MODELINDEX_ARMOR, MODELINDEX_ARMOR },
    { L_MODELINDEX_NIGHTVISION, MODELINDEX_NIGHTVISION },
    { L_MODELINDEX_THERMAL, MODELINDEX_THERMAL  }
};


static int translateGoldModelIdxToSilverModelIdx(int input) {

    if (input < 0 || input >= sizeof(modelIndexTranslations) / sizeof(modelIndexTranslations[0])) {
        return input;
    }

    return modelIndexTranslations[input].translatedIndex;
}

static int translateGoldWeaponToSilverWeapon(int input) {

    if (input < 0 || input >= sizeof(weaponTranslations) / sizeof(weaponTranslations[0])) {
        return input;
    }

    return weaponTranslations[input].translatedWeapon;
}

int translateSilverWeaponToGoldWeapon(int input) {
    if (input < 0 || input >= sizeof(weaponTranslationReversed) / sizeof(weaponTranslationReversed[0])) {
        return input;
    }

    return weaponTranslationReversed[input].translatedWeapon;
}

int translateSilverModelIdxToGoldModelIdx(int input) {

    if (input < 0 || input >= sizeof(modelIndexTranslationsReversed) / sizeof(modelIndexTranslationsReversed[0])) {
        return input;
    }

    return modelIndexTranslationsReversed[input].translatedIndex;

}

static int translateGoldStatWpnsToSilver(int input) {

    // assume valid input.
    int newStats = 0;

    for (int i = 0; i < sizeof(weaponTranslations) / sizeof(weaponTranslations[0]); i++) {
        weaponDiff_t dif = weaponTranslations[i];

        if (input & (1 << dif.weapon)) {
            newStats |= 1 << dif.translatedWeapon;
        }
    }

    return newStats;

}

/*
==============================================================================

            MESSAGE IO FUNCTIONS

Handles byte ordering and avoids alignment errors
==============================================================================
*/

int oldsize = 0;

void MSG_initHuffman( void );

void MSG_Init( msg_t *buf, byte *data, int length ) {
    if (!msgInit) {
        MSG_initHuffman();
    }
    Com_Memset (buf, 0, sizeof(*buf));
    buf->data = data;
    buf->maxsize = length;
}

void MSG_InitOOB( msg_t *buf, byte *data, int length ) {
    if (!msgInit) {
        MSG_initHuffman();
    }
    Com_Memset (buf, 0, sizeof(*buf));
    buf->data = data;
    buf->maxsize = length;
    buf->oob = qtrue;
}

void MSG_Clear( msg_t *buf ) {
    buf->cursize = 0;
    buf->overflowed = qfalse;
    buf->bit = 0;                   //<- in bits
}


void MSG_Bitstream( msg_t *buf ) {
    buf->oob = qfalse;
}

void MSG_BeginReading( msg_t *msg ) {
    msg->readcount = 0;
    msg->bit = 0;
    msg->oob = qfalse;
}

void MSG_BeginReadingOOB( msg_t *msg ) {
    msg->readcount = 0;
    msg->bit = 0;
    msg->oob = qtrue;
}

void MSG_Copy(msg_t *buf, byte *data, int length, msg_t *src)
{
    if (length<src->cursize) {
        Com_Error( ERR_DROP, "MSG_Copy: can't copy into a smaller msg_t buffer");
    }
    Com_Memcpy(buf, src, sizeof(msg_t));
    buf->data = data;
    Com_Memcpy(buf->data, src->data, src->cursize);
}

/*
=============================================================================

bit functions

=============================================================================
*/

// negative bit values include signs
void MSG_WriteBits( msg_t *msg, int value, int bits ) {
    int i;

    oldsize += bits;

    if ( msg->overflowed ) {
        return;
    }

    if ( bits == 0 || bits < -31 || bits > 32 ) {
        Com_Error( ERR_DROP, "MSG_WriteBits: bad bits %i", bits );
    }

    if ( bits < 0 ) {
        bits = -bits;
    }

    if ( msg->oob ) {
        if ( msg->cursize + ( bits >> 3 ) > msg->maxsize ) {
            msg->overflowed = qtrue;
            return;
        }

        if ( bits == 8 ) {
            msg->data[msg->cursize] = value;
            msg->cursize += 1;
            msg->bit += 8;
        } else if ( bits == 16 ) {
            short temp = value;

            CopyLittleShort( &msg->data[msg->cursize], &temp );
            msg->cursize += 2;
            msg->bit += 16;
        } else if ( bits==32 ) {
            CopyLittleLong( &msg->data[msg->cursize], &value );
            msg->cursize += 4;
            msg->bit += 32;
        } else {
            Com_Error( ERR_DROP, "can't write %d bits", bits );
        }
    } else {
        value &= (0xffffffff >> (32 - bits));
        if ( bits&7 ) {
            int nbits;
            nbits = bits&7;
            if ( msg->bit + nbits > msg->maxsize << 3 ) {
                msg->overflowed = qtrue;
                return;
            }
            for( i = 0; i < nbits; i++ ) {
                Huff_putBit( (value & 1), msg->data, &msg->bit );
                value = (value >> 1);
            }
            bits = bits - nbits;
        }
        if ( bits ) {
            for( i = 0; i < bits; i += 8 ) {
                Huff_offsetTransmit( &msgHuff.compressor, (value & 0xff), msg->data, &msg->bit, msg->maxsize << 3 );
                value = (value >> 8);

                if ( msg->bit > msg->maxsize << 3 ) {
                    msg->overflowed = qtrue;
                    return;
                }
            }
        }
        msg->cursize = (msg->bit >> 3) + 1;
    }
}

int MSG_ReadBits( msg_t *msg, int bits ) {
    int         value;
    int         get;
    qboolean    sgn;
    int         i, nbits;
//  FILE*   fp;

    if ( msg->readcount > msg->cursize ) {
        return 0;
    }

    value = 0;

    if ( bits < 0 ) {
        bits = -bits;
        sgn = qtrue;
    } else {
        sgn = qfalse;
    }

    if (msg->oob) {
        if (msg->readcount + (bits>>3) > msg->cursize) {
            msg->readcount = msg->cursize + 1;
            return 0;
        }

        if(bits==8)
        {
            value = msg->data[msg->readcount];
            msg->readcount += 1;
            msg->bit += 8;
        }
        else if(bits==16)
        {
            short temp;

            CopyLittleShort(&temp, &msg->data[msg->readcount]);
            value = temp;
            msg->readcount += 2;
            msg->bit += 16;
        }
        else if(bits==32)
        {
            CopyLittleLong(&value, &msg->data[msg->readcount]);
            msg->readcount += 4;
            msg->bit += 32;
        }
        else
            Com_Error(ERR_DROP, "can't read %d bits", bits);
    } else {
        nbits = 0;
        if (bits&7) {
            nbits = bits&7;
            if (msg->bit + nbits > msg->cursize << 3) {
                msg->readcount = msg->cursize + 1;
                return 0;
            }
            for(i=0;i<nbits;i++) {
                value |= (Huff_getBit(msg->data, &msg->bit)<<i);
            }
            bits = bits - nbits;
        }
        if (bits) {
//          fp = fopen("c:\\netchan.bin", "a");
            for(i=0;i<bits;i+=8) {
                Huff_offsetReceive (msgHuff.decompressor.tree, &get, msg->data, &msg->bit, msg->cursize<<3);
//              fwrite(&get, 1, 1, fp);
                value = (unsigned int)value | ((unsigned int)get<<(i+nbits));

                if (msg->bit > msg->cursize<<3) {
                    msg->readcount = msg->cursize + 1;
                    return 0;
                }
            }
//          fclose(fp);
        }
        msg->readcount = (msg->bit>>3)+1;
    }
    if ( sgn && bits > 0 && bits < 32 ) {
        if ( value & ( 1 << ( bits - 1 ) ) ) {
            value |= -1 ^ ( ( 1 << bits ) - 1 );
        }
    }

    return value;
}



//================================================================================

//
// writing functions
//

void MSG_WriteChar( msg_t *sb, int c ) {
#ifdef PARANOID
    if (c < -128 || c > 127)
        Com_Error (ERR_FATAL, "MSG_WriteChar: range error");
#endif

    MSG_WriteBits( sb, c, 8 );
}

void MSG_WriteByte( msg_t *sb, int c ) {
#ifdef PARANOID
    if (c < 0 || c > 255)
        Com_Error (ERR_FATAL, "MSG_WriteByte: range error");
#endif

    MSG_WriteBits( sb, c, 8 );
}

void MSG_WriteData( msg_t *buf, const void *data, int length ) {
    int i;
    for(i=0;i<length;i++) {
        MSG_WriteByte(buf, ((byte *)data)[i]);
    }
}

void MSG_WriteShort( msg_t *sb, int c ) {
#ifdef PARANOID
    if (c < ((short)0x8000) || c > (short)0x7fff)
        Com_Error (ERR_FATAL, "MSG_WriteShort: range error");
#endif

    MSG_WriteBits( sb, c, 16 );
}

void MSG_WriteLong( msg_t *sb, int c ) {
    MSG_WriteBits( sb, c, 32 );
}

void MSG_WriteFloat( msg_t *sb, float f ) {
    floatint_t dat;
    dat.f = f;
    MSG_WriteBits( sb, dat.i, 32 );
}

void MSG_WriteString( msg_t *sb, const char *s ) {
    if ( !s ) {
        MSG_WriteData (sb, "", 1);
    } else {
        int     l,i;
        char    string[MAX_STRING_CHARS];

        l = strlen( s );
        if ( l >= MAX_STRING_CHARS ) {
            Com_Printf( "MSG_WriteString: MAX_STRING_CHARS" );
            MSG_WriteData (sb, "", 1);
            return;
        }
        Q_strncpyz( string, s, sizeof( string ) );

        // get rid of 0x80+ and '%' chars, because old clients don't like them
        for ( i = 0 ; i < l ; i++ ) {
            if ( ((byte *)string)[i] > 127 || string[i] == '%' ) {
                string[i] = '.';
            }
        }

        MSG_WriteData (sb, string, l+1);
    }
}

void MSG_WriteBigString( msg_t *sb, const char *s ) {
    if ( !s ) {
        MSG_WriteData (sb, "", 1);
    } else {
        int     l,i;
        char    string[BIG_INFO_STRING];

        l = strlen( s );
        if ( l >= BIG_INFO_STRING ) {
            Com_Printf( "MSG_WriteString: BIG_INFO_STRING" );
            MSG_WriteData (sb, "", 1);
            return;
        }
        Q_strncpyz( string, s, sizeof( string ) );

        // get rid of 0x80+ and '%' chars, because old clients don't like them
        for ( i = 0 ; i < l ; i++ ) {
            if ( ((byte *)string)[i] > 127 || string[i] == '%' ) {
                string[i] = '.';
            }
        }

        MSG_WriteData (sb, string, l+1);
    }
}

void MSG_WriteAngle( msg_t *sb, float f ) {
    MSG_WriteByte (sb, (int)(f*256/360) & 255);
}

void MSG_WriteAngle16( msg_t *sb, float f ) {
    MSG_WriteShort (sb, ANGLE2SHORT(f));
}


//============================================================

//
// reading functions
//

// returns -1 if no more characters are available
int MSG_ReadChar (msg_t *msg ) {
    int c;

    c = (signed char)MSG_ReadBits( msg, 8 );
    if ( msg->readcount > msg->cursize ) {
        c = -1;
    }

    return c;
}

int MSG_ReadByte( msg_t *msg ) {
    int c;

    c = (unsigned char)MSG_ReadBits( msg, 8 );
    if ( msg->readcount > msg->cursize ) {
        c = -1;
    }
    return c;
}

int MSG_LookaheadByte( msg_t *msg ) {
    const int bloc = Huff_getBloc();
    const int readcount = msg->readcount;
    const int bit = msg->bit;
    int c = MSG_ReadByte(msg);
    Huff_setBloc(bloc);
    msg->readcount = readcount;
    msg->bit = bit;
    return c;
}

int MSG_ReadShort( msg_t *msg ) {
    int c;

    c = (short)MSG_ReadBits( msg, 16 );
    if ( msg->readcount > msg->cursize ) {
        c = -1;
    }

    return c;
}

int MSG_ReadLong( msg_t *msg ) {
    int c;

    c = MSG_ReadBits( msg, 32 );
    if ( msg->readcount > msg->cursize ) {
        c = -1;
    }

    return c;
}

float MSG_ReadFloat( msg_t *msg ) {
    floatint_t dat;

    dat.i = MSG_ReadBits( msg, 32 );
    if ( msg->readcount > msg->cursize ) {
        dat.f = -1;
    }

    return dat.f;
}

char *MSG_ReadString( msg_t *msg ) {
    static char string[MAX_STRING_CHARS];
    int     l,c;

    l = 0;
    do {
        c = MSG_ReadByte(msg);      // use ReadByte so -1 is out of bounds
        if ( c == -1 || c == 0 ) {
            break;
        }
        // translate all fmt spec to avoid crash bugs
        if ( c == '%' ) {
            c = '.';
        }
        // don't allow higher ascii values
        if ( c > 127 ) {
            c = '.';
        }
        // break only after reading all expected data from bitstream
        if ( l >= sizeof(string)-1 ) {
            break;
        }
        string[l++] = c;
    } while (1);

    string[l] = '\0';

    return string;
}

char *MSG_ReadBigString( msg_t *msg ) {
    static char string[BIG_INFO_STRING];
    int     l,c;

    l = 0;
    do {
        c = MSG_ReadByte(msg);      // use ReadByte so -1 is out of bounds
        if ( c == -1 || c == 0 ) {
            break;
        }
        // translate all fmt spec to avoid crash bugs
        if ( c == '%' ) {
            c = '.';
        }
        // don't allow higher ascii values
        if ( c > 127 ) {
            c = '.';
        }
        // break only after reading all expected data from bitstream
        if ( l >= sizeof(string)-1 ) {
            break;
        }
        string[l++] = c;
    } while (1);

    string[l] = '\0';

    return string;
}

char *MSG_ReadStringLine( msg_t *msg ) {
    static char string[MAX_STRING_CHARS];
    int     l,c;

    l = 0;
    do {
        c = MSG_ReadByte(msg);      // use ReadByte so -1 is out of bounds
        if (c == -1 || c == 0 || c == '\n') {
            break;
        }
        // translate all fmt spec to avoid crash bugs
        if ( c == '%' ) {
            c = '.';
        }
        // don't allow higher ascii values
        if ( c > 127 ) {
            c = '.';
        }
        // break only after reading all expected data from bitstream
        if ( l >= sizeof(string)-1 ) {
            break;
        }
        string[l++] = c;
    } while (1);

    string[l] = '\0';

    return string;
}

float MSG_ReadAngle16( msg_t *msg ) {
    return SHORT2ANGLE(MSG_ReadShort(msg));
}

void MSG_ReadData( msg_t *msg, void *data, int len ) {
    int     i;

    for (i=0 ; i<len ; i++) {
        ((byte *)data)[i] = MSG_ReadByte (msg);
    }
}

// a string hasher which gives the same hash value even if the
// string is later modified via the legacy MSG read/write code
int MSG_HashKey(const char *string, int maxlen) {
    int hash, i;

    hash = 0;
    for (i = 0; i < maxlen && string[i] != '\0'; i++) {
        if (string[i] & 0x80 || string[i] == '%')
            hash += '.' * (119 + i);
        else
            hash += string[i] * (119 + i);
    }
    hash = (hash ^ (hash >> 10) ^ (hash >> 20));
    return hash;
}

extern cvar_t *cl_shownet;

#define LOG(x) if( cl_shownet && cl_shownet->integer == 4 ) { Com_Printf("%s ", x ); };

/*
=============================================================================

delta functions with keys

=============================================================================
*/

int kbitmask[32] = {
    0x00000001, 0x00000003, 0x00000007, 0x0000000F,
    0x0000001F, 0x0000003F, 0x0000007F, 0x000000FF,
    0x000001FF, 0x000003FF, 0x000007FF, 0x00000FFF,
    0x00001FFF, 0x00003FFF, 0x00007FFF, 0x0000FFFF,
    0x0001FFFF, 0x0003FFFF, 0x0007FFFF, 0x000FFFFF,
    0x001FFFFf, 0x003FFFFF, 0x007FFFFF, 0x00FFFFFF,
    0x01FFFFFF, 0x03FFFFFF, 0x07FFFFFF, 0x0FFFFFFF,
    0x1FFFFFFF, 0x3FFFFFFF, 0x7FFFFFFF, 0xFFFFFFFF,
};

void MSG_WriteDeltaKey( msg_t *msg, int key, int oldV, int newV, int bits ) {
    if ( oldV == newV ) {
        MSG_WriteBits( msg, 0, 1 );
        return;
    }
    MSG_WriteBits( msg, 1, 1 );
    MSG_WriteBits( msg, newV ^ key, bits );
}

int MSG_ReadDeltaKey( msg_t *msg, int key, int oldV, int bits ) {
    if ( MSG_ReadBits( msg, 1 ) ) {
        return MSG_ReadBits( msg, bits ) ^ (key & kbitmask[ bits - 1 ]);
    }
    return oldV;
}

void MSG_WriteDeltaKeyFloat( msg_t *msg, int key, float oldV, float newV ) {
    floatint_t fi;
    if ( oldV == newV ) {
        MSG_WriteBits( msg, 0, 1 );
        return;
    }
    fi.f = newV;
    MSG_WriteBits( msg, 1, 1 );
    MSG_WriteBits( msg, fi.i ^ key, 32 );
}

float MSG_ReadDeltaKeyFloat( msg_t *msg, int key, float oldV ) {
    if ( MSG_ReadBits( msg, 1 ) ) {
        floatint_t fi;

        fi.i = MSG_ReadBits( msg, 32 ) ^ key;
        return fi.f;
    }
    return oldV;
}


/*
============================================================================

usercmd_t communication

============================================================================
*/

/*
=====================
MSG_WriteDeltaUsercmdKey

JANFIXME - wpn might need a change here too
=====================
*/
void MSG_WriteDeltaUsercmdKey( msg_t *msg, int key, usercmd_t *from, usercmd_t *to ) {
    if ( to->serverTime - from->serverTime < 256 ) {
        MSG_WriteBits( msg, 1, 1 );
        MSG_WriteBits( msg, to->serverTime - from->serverTime, 8 );
    } else {
        MSG_WriteBits( msg, 0, 1 );
        MSG_WriteBits( msg, to->serverTime, 32 );
    }
    if (from->angles[0] == to->angles[0] &&
        from->angles[1] == to->angles[1] &&
        from->angles[2] == to->angles[2] &&
        from->forwardmove == to->forwardmove &&
        from->rightmove == to->rightmove &&
        from->upmove == to->upmove &&
        from->buttons == to->buttons &&
        from->weapon == to->weapon) {
            MSG_WriteBits( msg, 0, 1 );             // no change
            oldsize += 7;
            return;
    }
    key ^= to->serverTime;
    MSG_WriteBits( msg, 1, 1 );
    MSG_WriteDeltaKey( msg, key, from->angles[0], to->angles[0], 16 );
    MSG_WriteDeltaKey( msg, key, from->angles[1], to->angles[1], 16 );
    MSG_WriteDeltaKey( msg, key, from->angles[2], to->angles[2], 16 );
    MSG_WriteDeltaKey( msg, key, from->forwardmove, to->forwardmove, 8 );
    MSG_WriteDeltaKey( msg, key, from->rightmove, to->rightmove, 8 );
    MSG_WriteDeltaKey( msg, key, from->upmove, to->upmove, 8 );
    MSG_WriteDeltaKey( msg, key, from->buttons, to->buttons, 16 );
    MSG_WriteDeltaKey( msg, key, from->weapon, to->weapon, 8 );
}


/*
=====================
MSG_ReadDeltaUsercmdKey
=====================
*/
void MSG_ReadDeltaUsercmdKey( msg_t *msg, int key, usercmd_t *from, usercmd_t *to, qboolean legacyProtocol ) {
    if ( MSG_ReadBits( msg, 1 ) ) {
        to->serverTime = from->serverTime + MSG_ReadBits( msg, 8 );
    } else {
        to->serverTime = MSG_ReadBits( msg, 32 );
    }
    if ( MSG_ReadBits( msg, 1 ) ) {
        key ^= to->serverTime;
        to->angles[0] = MSG_ReadDeltaKey( msg, key, from->angles[0], 16);
        to->angles[1] = MSG_ReadDeltaKey( msg, key, from->angles[1], 16);
        to->angles[2] = MSG_ReadDeltaKey( msg, key, from->angles[2], 16);
        to->forwardmove = MSG_ReadDeltaKey( msg, key, from->forwardmove, 8);
        if( to->forwardmove == -128 )
            to->forwardmove = -127;
        to->rightmove = MSG_ReadDeltaKey( msg, key, from->rightmove, 8);
        if( to->rightmove == -128 )
            to->rightmove = -127;
        to->upmove = MSG_ReadDeltaKey( msg, key, from->upmove, 8);
        if( to->upmove == -128 )
            to->upmove = -127;
        to->buttons = MSG_ReadDeltaKey( msg, key, from->buttons, 16);
        to->weapon = MSG_ReadDeltaKey( msg, key, from->weapon, 8);

        /*if (legacyProtocol) {
            if (to->weapon & WP_DELAYED_CHANGE_BIT) {
                to->weapon = translateSilverWeaponToGoldWeapon(to->weapon & ~WP_DELAYED_CHANGE_BIT) | WP_DELAYED_CHANGE_BIT;
            }
            else {
                to->weapon = translateSilverWeaponToGoldWeapon(to->weapon);
            }
        }
        */
    } else {
        to->angles[0] = from->angles[0];
        to->angles[1] = from->angles[1];
        to->angles[2] = from->angles[2];
        to->forwardmove = from->forwardmove;
        to->rightmove = from->rightmove;
        to->upmove = from->upmove;
        to->buttons = from->buttons;
        to->weapon = from->weapon;
    }

    
}

/*
=============================================================================

entityState_t communication

=============================================================================
*/

/*
=================
MSG_ReportChangeVectors_f

Prints out a table from the current statistics for copying to code
=================
*/
void MSG_ReportChangeVectors_f( void ) {
    int i;
    for(i=0;i<256;i++) {
        if (pcount[i]) {
            Com_Printf("%d used %d\n", i, pcount[i]);
        }
    }
}

typedef struct {
    char    *name;
    int     offset;
    int     bits;       // 0 = float
} netField_t;

// using the stringizing operator to save typing...
#define NETF(x) #x,(size_t)&((entityState_t*)0)->x

netField_t  legacyEntityStateFields[] =
{
    {NETF(pos.trTime), 32},
    {NETF(pos.trBase[0]), 0},
    {NETF(pos.trBase[1]), 0},
    {NETF(pos.trDelta[0]), 0},
    {NETF(pos.trDelta[1]), 0},
    {NETF(pos.trBase[2]), 0},
    {NETF(apos.trBase[1]), 0},
    {NETF(pos.trDelta[2]), 0},
    {NETF(apos.trBase[0]), 0},
    {NETF(event), 10},
    {NETF(angles2[1]), 0},
    {NETF(eType), 8},
    {NETF(torsoAnim), 12},
    {NETF(torsoTimer), 12},
    {NETF(eventParm), 0},
    {NETF(legsAnim), 12},
    {NETF(groundEntityNum), GENTITYNUM_BITS},
    {NETF(pos.trType), 8},
    {NETF(eFlags), 32},
    {NETF(otherEntityNum), GENTITYNUM_BITS},
    {NETF(weapon), 8},
    {NETF(clientNum), 8},
    {NETF(angles[1]), 0},
    {NETF(pos.trDuration), 32},
    {NETF(apos.trType), 8},
    {NETF(origin[0]), 0},
    {NETF(origin[1]), 0},
    {NETF(origin[2]), 0},
    {NETF(solid), 24},
    {NETF(gametypeitems), 8},
    {NETF(modelindex), 8},
    {NETF(otherEntityNum2), GENTITYNUM_BITS},
    {NETF(loopSound), 8},
    {NETF(generic1), 8},
    {NETF(mSoundSet), 6},
    {NETF(origin2[2]), 0},
    {NETF(origin2[0]), 0},
    {NETF(origin2[1]), 0},
    {NETF(modelindex2), 8},
    {NETF(angles[0]), 0},
    {NETF(time), 32},
    {NETF(apos.trTime), 32},
    {NETF(apos.trDuration), 32},
    {NETF(apos.trBase[2]), 0},
    {NETF(apos.trDelta[0]), 0},
    {NETF(apos.trDelta[1]), 0},
    {NETF(apos.trDelta[2]), 0},
    {NETF(time2), 32},
    {NETF(angles[2]), 0},
    {NETF(angles2[0]), 0},
    {NETF(angles2[2]), 0},
    {NETF(frame), 16},
    {NETF(leanOffset), 6},
};

netField_t  entityStateFields[] =
{
    {NETF(pos.trTime), 32},
    {NETF(pos.trBase[0]), 0},
    {NETF(pos.trBase[1]), 0},
    {NETF(pos.trDelta[0]), 0},
    {NETF(pos.trDelta[1]), 0},
    {NETF(pos.trBase[2]), 0},
    {NETF(apos.trBase[1]), 0},
    {NETF(pos.trDelta[2]), 0},
    {NETF(apos.trBase[0]), 0},
    {NETF(event), 10},
    {NETF(angles2[1]), 0},
    {NETF(eType), 8},
    {NETF(torsoAnim), 12},
    {NETF(torsoTimer), 13},
    {NETF(eventParm), 0},
    {NETF(legsAnim), 12},
    {NETF(groundEntityNum), GENTITYNUM_BITS},
    {NETF(pos.trType), 8},
    {NETF(eFlags), 32},
    {NETF(otherEntityNum), GENTITYNUM_BITS},
    {NETF(weapon), 8},
    {NETF(clientNum), 8},
    {NETF(angles[1]), 0},
    {NETF(pos.trDuration), 32},
    {NETF(apos.trType), 8},
    {NETF(origin[0]), 0},
    {NETF(origin[1]), 0},
    {NETF(origin[2]), 0},
    {NETF(solid), 24},
    {NETF(gametypeitems), 8},
    {NETF(modelindex), 8},
    {NETF(otherEntityNum2), GENTITYNUM_BITS},
    {NETF(loopSound), 8},
    {NETF(generic1), 8},
    {NETF(mSoundSet), 6},
    {NETF(origin2[2]), 0},
    {NETF(origin2[0]), 0},
    {NETF(origin2[1]), 0},
    {NETF(modelindex2), 8},
    {NETF(angles[0]), 0},
    {NETF(time), 32},
    {NETF(apos.trTime), 32},
    {NETF(apos.trDuration), 32},
    {NETF(apos.trBase[2]), 0},
    {NETF(apos.trDelta[0]), 0},
    {NETF(apos.trDelta[1]), 0},
    {NETF(apos.trDelta[2]), 0},
    {NETF(time2), 32},
    {NETF(angles[2]), 0},
    {NETF(angles2[0]), 0},
    {NETF(angles2[2]), 0},
    {NETF(frame), 16},
    {NETF(leanOffset), 6},
};


// if (int)f == f and (int)f + ( 1<<(FLOAT_INT_BITS-1) ) < ( 1 << FLOAT_INT_BITS )
// the float will be sent with FLOAT_INT_BITS, otherwise all 32 bits will be sent
#define FLOAT_INT_BITS  13
#define FLOAT_INT_BIAS  (1<<(FLOAT_INT_BITS-1))

/*
==================
MSG_WriteDeltaEntity

Writes part of a packetentities message, including the entity number.
Can delta from either a baseline or a previous packet_entity
If to is NULL, a remove entity update will be sent
If force is not set, then nothing at all will be generated if the entity is
identical, under the assumption that the in-order delta code will catch it.
==================
*/
void MSG_WriteDeltaEntity( msg_t *msg, struct entityState_s *from, struct entityState_s *to,
                           qboolean force, qboolean legacyProtocol ) {
    int         i, lc;
    int         numFields;
    netField_t  *field;
    int         trunc;
    float       fullFloat;
    int         *fromF, *toF;

    int fromModelIdx = -1,
        toModelIdx = -1,
        fromEventParm = -1,
        toEventParm = -1,
        fromTime = -1,
        toTime = -1,
        fromEType = -1,
        toEType = -1,
        fromEvent = -1,
        toEvent = -1,
        fromWpn = -1,
        toWpn = -1

        ;



    netField_t* entityStateFields_Local = legacyProtocol ? legacyEntityStateFields : entityStateFields;

    numFields = ARRAY_LEN( entityStateFields );

    // all fields should be 32 bits to avoid any compiler packing issues
    // the "number" field is not part of the field list
    // if this assert fails, someone added a field to the entityState_t
    // struct without updating the message fields
    assert( numFields + 1 == sizeof( *from )/4 );

    // a NULL to is a delta remove message
    if ( to == NULL ) {
        if ( from == NULL ) {
            return;
        }
        MSG_WriteBits( msg, from->number, GENTITYNUM_BITS );
        MSG_WriteBits( msg, 1, 1 );
        return;
    }

    if ( to->number < 0 || to->number >= MAX_GENTITIES ) {
        Com_Error (ERR_FATAL, "MSG_WriteDeltaEntity: Bad entity number: %i", to->number );
    }


    if (legacyProtocol) {
        // change the eType on temporary entities.
        // Rest of the events should be catered in the game module.

        if ((from->eType == ET_EVENTS + EV_ITEM_PICKUP || from->eType == ET_EVENTS + EV_ITEM_PICKUP_QUIET) && from->eType != to->eType) {
            
            fromEventParm = from->eventParm;
            
            qboolean autoSwitch = (from->eventParm & ITEM_AUTOSWITCHBIT) ? qtrue : qfalse;
            from->eventParm = translateGoldWeaponToSilverWeapon(from->eventParm & ~ITEM_AUTOSWITCHBIT);

            if (autoSwitch) {
                from->eventParm |= ITEM_AUTOSWITCHBIT;
            }
        }

        if ((to->eType == ET_EVENTS + EV_ITEM_PICKUP || to->eType == ET_EVENTS + EV_ITEM_PICKUP_QUIET) && from->eType != to->eType) {
            
            toEventParm = to->eventParm;
            
            qboolean autoSwitch = (to->eventParm & ITEM_AUTOSWITCHBIT) ? qtrue : qfalse;
            to->eventParm = translateGoldWeaponToSilverWeapon(to->eventParm & ~ITEM_AUTOSWITCHBIT);

            if (autoSwitch) {
                to->eventParm |= ITEM_AUTOSWITCHBIT;
            }

        }

        if (to->eType == EV_OBITUARY && from->eType != to->eType) {
            fromEventParm = from->eventParm;
            toEventParm = to->eventParm;

            if (toEventParm >= 0 && toEventParm < sizeof(meansOfDeathTranslations) / sizeof(meansOfDeathTranslations[0])) {
                to->eventParm = meansOfDeathTranslations[to->eventParm].translatedMod;
            }
        }

        if ((to->eType == ET_EVENTS + EV_BULLET_HIT_FLESH || to->eType == ET_EVENTS + EV_BULLET_HIT_WALL || to->eType == ET_EVENTS + EV_BULLET || to->eType == ET_EVENTS + EV_EXPLOSION_HIT_FLESH) && from->time != to->time) {
            // I honestly question the sanity of the developers on this. Weapon + attack type is inside entity time...???? :)))))))))))
            toTime = to->time;
            //tent->s.time = weapon + ((attack&0xFF)<<8);
            int originalWeapon = to->time & 0xFF;
            int originalAttack = (to->time >> 8) & 0xFF;
            int originalYaw = (to->time >> 16) & 0xFFFF;

            int silverWpn = translateGoldWeaponToSilverWeapon(originalWeapon);

            to->time = (silverWpn & 0xFF)
                | ((originalAttack & 0xFF) << 8)
                | ((originalYaw & 0xFFFF) << 16);
            
        }

        if ((from->eType == ET_GAMETYPE_TRIGGER || from->eType == ET_WALL) && from->eType != to->eType) {
            fromEType = from->eType;
            from->eType = ET_GENERAL;
        }

        if ((to->eType == ET_GAMETYPE_TRIGGER || to->eType == ET_WALL) && from->eType != to->eType) {
            toEType = to->eType;
            to->eType = ET_GENERAL;
        }

        if (to->eType >= ET_EVENTS + EV_ITEM_PICKUP_QUIET - 2 && from->eType != to->eType) { // -2 because ET_ ENUM is also smaller in 1.00.
            toEType = to->eType;
            to->eType -= 3;
        }

        if ((to->event & ~EV_EVENT_BITS) > EV_ITEM_PICKUP_QUIET && from->event != to->event) {
            toEvent = to->event;
            to->event--;
        }

        if (to->modelindex > 0 && to->modelindex < sizeof(modelIndexTranslations) / sizeof(modelIndexTranslations[0]) && to->eType == ET_ITEM && from->modelindex != to->modelindex) {
            toModelIdx = to->modelindex;
            to->modelindex = translateGoldModelIdxToSilverModelIdx(to->modelindex);
        }





        if ((from->eType == ET_EVENTS + EV_BULLET_HIT_FLESH || from->eType == ET_EVENTS + EV_BULLET_HIT_WALL || from->eType == ET_EVENTS + EV_BULLET || from->eType == ET_EVENTS + EV_EXPLOSION_HIT_FLESH) && from->time != to->time) {
            // I honestly question the sanity of the developers on this. Weapon + attack type is inside entity time...???? :)))))))))))
            fromTime = from->time;
            //tent->s.time = weapon + ((attack&0xFF)<<8);
            int originalWeapon = from->time & 0xFF;
            int originalAttack = (from->time >> 8) & 0xFF;
            int originalYaw = (from->time >> 16) & 0xFFFF;

            int silverWpn = translateGoldWeaponToSilverWeapon(originalWeapon);

            from->time = (silverWpn & 0xFF)
                | ((originalAttack & 0xFF) << 8)
                | ((originalYaw & 0xFFFF) << 16);

        }

        if (from->eType >= ET_EVENTS + EV_ITEM_PICKUP_QUIET - 2 && from->eType != to->eType) { // -2 because ET_ ENUM is also smaller in 1.00.
            fromEType = from->eType;
            from->eType -= 3;
        }

        if ((from->event & ~EV_EVENT_BITS) > EV_ITEM_PICKUP_QUIET && from->event != to->event) {
            fromEvent = from->event;
            from->event--;
        }

        if (from->modelindex > 0 && from->modelindex < sizeof(modelIndexTranslations) / sizeof(modelIndexTranslations[0]) && from->eType == ET_ITEM && from->modelindex != to->modelindex) {
            fromModelIdx = from->modelindex;
            from->modelindex = translateGoldModelIdxToSilverModelIdx(from->modelindex);
        }

        if (from->weapon != to->weapon) {
            fromWpn = from->weapon;
            toWpn = to->weapon;

            from->weapon = translateGoldWeaponToSilverWeapon(from->weapon);
            to->weapon = translateGoldWeaponToSilverWeapon(to->weapon);
        }

        

        /*if (to->modelindex2 > 0 && to->modelindex2 < sizeof(modelIndexTranslations)) {
            originalModelIdx2 = to->modelindex2;
            to->modelindex2 = translateGoldModelIdxToSilverModelIdx(to->modelindex2);
        }*/
    }

    lc = 0;
    // build the change vector as bytes so it is endien independent
    for ( i = 0, field = entityStateFields_Local; i < numFields ; i++, field++ ) {
        fromF = (int *)( (byte *)from + field->offset );
        toF = (int *)( (byte *)to + field->offset );
        if ( *fromF != *toF ) {
            lc = i+1;
        }
    }

    if ( lc == 0 ) {
        // nothing at all changed
        if ( !force ) {
            return;     // nothing at all
        }
        // write two bits for no change
        MSG_WriteBits( msg, to->number, GENTITYNUM_BITS );
        MSG_WriteBits( msg, 0, 1 );     // not removed
        MSG_WriteBits( msg, 0, 1 );     // no delta
        return;
    }

    MSG_WriteBits( msg, to->number, GENTITYNUM_BITS );
    MSG_WriteBits( msg, 0, 1 );         // not removed
    MSG_WriteBits( msg, 1, 1 );         // we have a delta

    MSG_WriteByte( msg, lc );   // # of changes

    oldsize += numFields;

    for ( i = 0, field = entityStateFields_Local; i < lc ; i++, field++ ) {
        fromF = (int *)( (byte *)from + field->offset );
        toF = (int *)( (byte *)to + field->offset );

        if ( *fromF == *toF ) {
            MSG_WriteBits( msg, 0, 1 ); // no change
            continue;
        }

        MSG_WriteBits( msg, 1, 1 ); // changed

        if ( field->bits == 0 ) {
            // float
            fullFloat = *(float *)toF;
            trunc = (int)fullFloat;

            if (fullFloat == 0.0f) {
                    MSG_WriteBits( msg, 0, 1 );
                    oldsize += FLOAT_INT_BITS;
            } else {
                MSG_WriteBits( msg, 1, 1 );
                if ( trunc == fullFloat && trunc + FLOAT_INT_BIAS >= 0 &&
                    trunc + FLOAT_INT_BIAS < ( 1 << FLOAT_INT_BITS ) ) {
                    // send as small integer
                    MSG_WriteBits( msg, 0, 1 );
                    MSG_WriteBits( msg, trunc + FLOAT_INT_BIAS, FLOAT_INT_BITS );
                } else {
                    // send as full floating point value
                    MSG_WriteBits( msg, 1, 1 );
                    MSG_WriteBits( msg, *toF, 32 );
                }
            }
        } else {
            if (*toF == 0) {
                MSG_WriteBits( msg, 0, 1 );
            } else {
                MSG_WriteBits( msg, 1, 1 );
                // integer
                MSG_WriteBits( msg, *toF, field->bits );
            }
        }
    }


    if (legacyProtocol) {

        if (fromEventParm != -1) {
            from->eventParm = fromEventParm;
        }

        if (toEventParm != -1) {
            to->eventParm = toEventParm;
        }

        if (fromTime != -1) {
            from->time = fromTime;
        }

        if (toTime != -1) {
            to->time = toTime;
        }

        if (fromEType != -1) {
            from->eType = fromEType;
        }

        if (toEType != -1) {
            to->eType = toEType;
        }

        if (fromEvent != -1) {
            from->event = fromEvent;
        }

        if (toEvent != -1) {
            to->event = toEvent;
        }

        if (fromModelIdx != -1) {
            from->modelindex = fromModelIdx;
        }

        if (toModelIdx != -1) {
            to->modelindex = toModelIdx;
        }

        if (fromWpn != -1) {
            from->weapon = fromWpn;
        }

        if (toWpn != -1) {
            to->weapon = toWpn;
        }

    }
}

/*
==================
MSG_ReadDeltaEntity

The entity number has already been read from the message, which
is how the from state is identified.

If the delta removes the entity, entityState_t->number will be set to MAX_GENTITIES-1

Can go from either a baseline or a previous packet_entity
==================
*/
void MSG_ReadDeltaEntity( msg_t *msg, entityState_t *from, entityState_t *to,
                         int number) {
    int         i, lc;
    int         numFields;
    netField_t  *field;
    int         *fromF, *toF;
    int         print;
    int         trunc;
    int         startBit, endBit;

    if ( number < 0 || number >= MAX_GENTITIES) {
        Com_Error( ERR_DROP, "Bad delta entity number: %i", number );
    }

    if ( msg->bit == 0 ) {
        startBit = msg->readcount * 8 - GENTITYNUM_BITS;
    } else {
        startBit = ( msg->readcount - 1 ) * 8 + msg->bit - GENTITYNUM_BITS;
    }

    // check for a remove
    if ( MSG_ReadBits( msg, 1 ) == 1 ) {
        Com_Memset( to, 0, sizeof( *to ) );
        to->number = MAX_GENTITIES - 1;
        if ( cl_shownet && ( cl_shownet->integer >= 2 || cl_shownet->integer == -1 ) ) {
            Com_Printf( "%3i: #%-3i remove\n", msg->readcount, number );
        }
        return;
    }

    // check for no delta
    if ( MSG_ReadBits( msg, 1 ) == 0 ) {
        *to = *from;
        to->number = number;
        return;
    }

    numFields = ARRAY_LEN( entityStateFields );
    lc = MSG_ReadByte(msg);

    if ( lc > numFields || lc < 0 ) {
        Com_Error( ERR_DROP, "invalid entityState field count" );
    }

    // shownet 2/3 will interleave with other printed info, -1 will
    // just print the delta records`
    if ( cl_shownet && ( cl_shownet->integer >= 2 || cl_shownet->integer == -1 ) ) {
        print = 1;
        Com_Printf( "%3i: #%-3i ", msg->readcount, to->number );
    } else {
        print = 0;
    }

    to->number = number;

    for ( i = 0, field = entityStateFields ; i < lc ; i++, field++ ) {
        fromF = (int *)( (byte *)from + field->offset );
        toF = (int *)( (byte *)to + field->offset );

        if ( ! MSG_ReadBits( msg, 1 ) ) {
            // no change
            *toF = *fromF;
        } else {
            if ( field->bits == 0 ) {
                // float
                if ( MSG_ReadBits( msg, 1 ) == 0 ) {
                        *(float *)toF = 0.0f;
                } else {
                    if ( MSG_ReadBits( msg, 1 ) == 0 ) {
                        // integral float
                        trunc = MSG_ReadBits( msg, FLOAT_INT_BITS );
                        // bias to allow equal parts positive and negative
                        trunc -= FLOAT_INT_BIAS;
                        *(float *)toF = trunc;
                        if ( print ) {
                            Com_Printf( "%s:%i ", field->name, trunc );
                        }
                    } else {
                        // full floating point value
                        *toF = MSG_ReadBits( msg, 32 );
                        if ( print ) {
                            Com_Printf( "%s:%f ", field->name, *(float *)toF );
                        }
                    }
                }
            } else {
                if ( MSG_ReadBits( msg, 1 ) == 0 ) {
                    *toF = 0;
                } else {
                    // integer
                    *toF = MSG_ReadBits( msg, field->bits );
                    if ( print ) {
                        Com_Printf( "%s:%i ", field->name, *toF );
                    }
                }
            }
//          pcount[i]++;
        }
    }
    for ( i = lc, field = &entityStateFields[lc] ; i < numFields ; i++, field++ ) {
        fromF = (int *)( (byte *)from + field->offset );
        toF = (int *)( (byte *)to + field->offset );
        // no change
        *toF = *fromF;
    }

    if ( print ) {
        if ( msg->bit == 0 ) {
            endBit = msg->readcount * 8 - GENTITYNUM_BITS;
        } else {
            endBit = ( msg->readcount - 1 ) * 8 + msg->bit - GENTITYNUM_BITS;
        }
        Com_Printf( " (%i bits)\n", endBit - startBit  );
    }
}


/*
============================================================================

plyer_state_t communication

============================================================================
*/

// using the stringizing operator to save typing...
#define PSF(x) #x,(size_t)&((playerState_t*)0)->x

netField_t  legacyPlayerStateFields[] =
{
    {PSF(commandTime), 32},
    {PSF(origin[0]), 0},
    {PSF(origin[1]), 0},
    {PSF(bobCycle), 8},
    {PSF(velocity[0]), 0},
    {PSF(velocity[1]), 0},
    {PSF(viewangles[1]), 0},
    {PSF(viewangles[0]), 0},
    {PSF(weaponTime), -16},
    {PSF(weaponAnimTime), -16},
    {PSF(weaponFireBurstCount), 3},
    {PSF(weaponAnimId), -16},
    {PSF(weaponAnimIdChoice), -16},
    {PSF(weaponCallbackTime), 16},
    {PSF(weaponCallbackStep), -8},
    {PSF(origin[2]), 0},
    {PSF(velocity[2]), 0},
    {PSF(pm_time), -16},
    {PSF(eventSequence), 16},
    {PSF(torsoAnim), 12},
    {PSF(movementDir), 4},
    {PSF(events[0]), 10},
    {PSF(events[1]), 10},
    {PSF(events[2]), 10},
    {PSF(events[3]), 10},
    {PSF(legsAnim), 12},
    {PSF(pm_flags), 32},
    {PSF(pm_debounce), 16},
    {PSF(groundEntityNum), GENTITYNUM_BITS},
    {PSF(weaponstate), 4},
    {PSF(eFlags), 32},
    {PSF(externalEvent), 10},
    {PSF(gravity), 16},
    {PSF(speed), 16},
    {PSF(delta_angles[1]), 16},
    {PSF(externalEventParm), 8},
    {PSF(viewheight), -8},
    {PSF(damageEvent), 8},
    {PSF(damageYaw), 8},
    {PSF(damagePitch), 8},
    {PSF(damageCount), 8},
    {PSF(inaccuracy), 32},
    {PSF(inaccuracyTime), 16},
    {PSF(kickPitch), 18},
    {PSF(generic1), 8},
    {PSF(pm_type), 8},
    {PSF(delta_angles[0]), 16},
    {PSF(delta_angles[2]), 16},
    {PSF(torsoTimer), 12},
    {PSF(eventParms[0]), 32},
    {PSF(eventParms[1]), 32},
    {PSF(eventParms[2]), 32},
    {PSF(eventParms[3]), 32},
    {PSF(clientNum), 8},
    {PSF(weapon), 5},
    {PSF(viewangles[2]), 0},
    {PSF(loopSound), 16},
    {PSF(zoomTime), 32},
    {PSF(zoomFov), 6},
    {PSF(ladder), 6},
    {PSF(leanTime), 16},
    {PSF(grenadeTimer), 13},
    {PSF(respawnTimer), 32},
};

netField_t  playerStateFields[] =
{
    {PSF(commandTime), 32},
    {PSF(origin[0]), 0},
    {PSF(origin[1]), 0},
    {PSF(bobCycle), 8},
    {PSF(velocity[0]), 0},
    {PSF(velocity[1]), 0},
    {PSF(viewangles[1]), 0},
    {PSF(viewangles[0]), 0},
    {PSF(weaponTime), -16},
    {PSF(weaponAnimTime), -16},
    {PSF(weaponFireBurstCount), 3},
    {PSF(weaponAnimId), -16},
    {PSF(weaponAnimIdChoice), -16},
    {PSF(weaponCallbackTime), 16},
    {PSF(weaponCallbackStep), -8},
    {PSF(origin[2]), 0},
    {PSF(velocity[2]), 0},
    {PSF(pm_time), -16},
    {PSF(eventSequence), 16},
    {PSF(torsoAnim), 12},
    {PSF(movementDir), 4},
    {PSF(events[0]), 10},
    {PSF(events[1]), 10},
    {PSF(events[2]), 10},
    {PSF(events[3]), 10},
    {PSF(legsAnim), 12},
    {PSF(pm_flags), 32},
    {PSF(pm_debounce), 16},
    {PSF(groundEntityNum), GENTITYNUM_BITS},
    {PSF(weaponstate), 4},
    {PSF(eFlags), 32},
    {PSF(externalEvent), 10},
    {PSF(gravity), 16},
    {PSF(speed), 16},
    {PSF(delta_angles[1]), 16},
    {PSF(externalEventParm), 8},
    {PSF(viewheight), -8},
    {PSF(damageEvent), 8},
    {PSF(damageYaw), 8},
    {PSF(damagePitch), 8},
    {PSF(damageCount), 8},
    {PSF(inaccuracy), 32},
    {PSF(inaccuracyTime), 16},
    {PSF(kickPitch), 18},
    {PSF(generic1), 8},
    {PSF(pm_type), 8},
    {PSF(delta_angles[0]), 16},
    {PSF(delta_angles[2]), 16},
    {PSF(torsoTimer), 13},
    {PSF(eventParms[0]), 32},
    {PSF(eventParms[1]), 32},
    {PSF(eventParms[2]), 32},
    {PSF(eventParms[3]), 32},
    {PSF(clientNum), 8},
    {PSF(weapon), 5},
    {PSF(viewangles[2]), 0},
    {PSF(loopSound), 16},
    {PSF(zoomTime), 32},
    {PSF(zoomFov), 6},
    {PSF(ladder), 6},
    {PSF(leanTime), 16},
    {PSF(grenadeTimer), 13},
    {PSF(respawnTimer), 32},
};

/*
=============
MSG_WriteDeltaPlayerstate

=============
*/
void MSG_WriteDeltaPlayerstate( msg_t *msg, struct playerState_s *from, struct playerState_s *to, qboolean legacyProtocol ) {
    int             i;
    playerState_t   dummy;
    int             statsbits;
    int             persistantbits;
    int             ammobits;
    int             clipbits;
    int             altclipbits;
    int             firemodebits;
    int             numFields;
    netField_t      *field;
    int             *fromF, *toF;
    float           fullFloat;
    int             trunc, lc;

    int
        fromEvents0 = -1,
        fromEvents1 = -1,
        fromEvents2 = -1,
        fromEvents3 = -1,
        fromExternalEvent = -1,
        fromExternalEventParm = -1,
        fromWpn = -1,
        fromZoomFov = -1,

        toEvents0 = -1,
        toEvents1 = -1,
        toEvents2 = -1,
        toEvents3 = -1,
        toExternalEvent = -1,
        toExternalEventParm = -1,
        toWpn = -1,
        toZoomFov = -1
        ;

    netField_t* playerStateFields_Local = legacyProtocol ? legacyPlayerStateFields : playerStateFields;

    if (!from) {
        from = &dummy;
        Com_Memset (&dummy, 0, sizeof(dummy));
    }

    numFields = ARRAY_LEN( playerStateFields );


    if (legacyProtocol) {

        if (from->events[0] > EV_ITEM_PICKUP_QUIET && from->events[0] != to->events[0]) {
            fromEvents0 = from->events[0];
            from->events[0]--;
        }


        if (from->events[1] > EV_ITEM_PICKUP_QUIET && from->events[1] != to->events[1]) {
            fromEvents1 = from->events[1];
            from->events[1]--;
        }


        if (from->events[2] > EV_ITEM_PICKUP_QUIET && from->events[2] != to->events[2]) {
            fromEvents2 = from->events[2];
            from->events[2]--;
        }


        if (from->events[3] > EV_ITEM_PICKUP_QUIET && from->events[3] != to->events[3]) {
            fromEvents3 = from->events[3];
            from->events[3]--;
        }

        if (((from->externalEvent & ~EV_EVENT_BITS) == EV_ITEM_PICKUP || (from->externalEvent & ~EV_EVENT_BITS) == EV_ITEM_PICKUP_QUIET) && from->externalEvent != to->externalEvent) {
            fromExternalEventParm = from->externalEventParm;
            from->externalEventParm = translateGoldModelIdxToSilverModelIdx(from->externalEventParm);
        }

        if ((from->externalEvent & ~EV_EVENT_BITS) > EV_ITEM_PICKUP_QUIET && from->externalEvent != to->externalEvent) {
            fromExternalEvent = from->externalEvent;
            from->externalEvent--;
        }


        if (to->events[0] > EV_ITEM_PICKUP_QUIET && from->events[0] != to->events[0]) {
            toEvents0 = to->events[0];
            to->events[0]--;
        }


        if (to->events[1] > EV_ITEM_PICKUP_QUIET && from->events[1] != to->events[1]) {
            toEvents1 = to->events[1];
            to->events[1]--;
        }


        if (to->events[2] > EV_ITEM_PICKUP_QUIET && from->events[2] != to->events[2]) {
            toEvents2 = to->events[2];
            to->events[2]--;
        }


        if (to->events[3] > EV_ITEM_PICKUP_QUIET && from->events[3] != to->events[3]) {
            toEvents3 = to->events[3];
            to->events[3]--;
        }

        if (((to->externalEvent & ~EV_EVENT_BITS) == EV_ITEM_PICKUP || (to->externalEvent & ~EV_EVENT_BITS) == EV_ITEM_PICKUP_QUIET) && from->externalEvent != to->externalEvent) {
            toExternalEventParm = to->externalEventParm;
            to->externalEventParm = translateGoldModelIdxToSilverModelIdx(to->externalEventParm);
        }

        if ((to->externalEvent & ~EV_EVENT_BITS) > EV_ITEM_PICKUP_QUIET && from->externalEvent != to->externalEvent) {
            toExternalEvent = to->externalEvent;
            to->externalEvent--;
        }

        if (from->weapon != to->weapon) {

            fromWpn = from->weapon;
            toWpn = to->weapon;

            from->weapon = translateGoldWeaponToSilverWeapon(from->weapon); // this should be the cause of M4 issue. M4 is the same as sniper, so there's no delta if this is not translated.
            to->weapon = translateGoldWeaponToSilverWeapon(to->weapon);
        }

        if ((!(from->pm_flags & PMF_ZOOMED) && to->pm_flags & PMF_ZOOMED) || (from->zoomFov != to->zoomFov)) {
            fromZoomFov = from->zoomFov;
            toZoomFov = to->zoomFov;

            if (!(from->pm_flags & PMF_ZOOMED) && to->pm_flags & PMF_ZOOMED) {
                from->zoomFov = 0;
                to->zoomFov = 20;
            }
            else {
                switch (fromZoomFov) {
                case 0:
                    from->zoomFov = 20;
                    break;
                case 1:
                    from->zoomFov = 10;
                    break;
                case 2:
                    from->zoomFov = 5;
                    break;

                }


                switch (toZoomFov) {
                case 0:
                    to->zoomFov = 20;
                    break;
                case 1:
                    to->zoomFov = 10;
                    break;
                case 2:
                    to->zoomFov = 5;
                    break;

                }

            }
        }

        
    }

    lc = 0;
    for ( i = 0, field = playerStateFields_Local; i < numFields ; i++, field++ ) {
        fromF = (int *)( (byte *)from + field->offset );
        toF = (int *)( (byte *)to + field->offset );
        if ( *fromF != *toF ) {
            lc = i+1;
        }
    }

    MSG_WriteByte( msg, lc );   // # of changes

    oldsize += numFields - lc;

    for ( i = 0, field = playerStateFields_Local; i < lc ; i++, field++ ) {
        fromF = (int *)( (byte *)from + field->offset );
        toF = (int *)( (byte *)to + field->offset );

        if ( *fromF == *toF ) {
            MSG_WriteBits( msg, 0, 1 ); // no change
            continue;
        }

        MSG_WriteBits( msg, 1, 1 ); // changed

        if ( field->bits == 0 ) {
            // float
            fullFloat = *(float *)toF;
            trunc = (int)fullFloat;

            if ( trunc == fullFloat && trunc + FLOAT_INT_BIAS >= 0 &&
                trunc + FLOAT_INT_BIAS < ( 1 << FLOAT_INT_BITS ) ) {
                // send as small integer
                MSG_WriteBits( msg, 0, 1 );
                MSG_WriteBits( msg, trunc + FLOAT_INT_BIAS, FLOAT_INT_BITS );
            } else {
                // send as full floating point value
                MSG_WriteBits( msg, 1, 1 );
                MSG_WriteBits( msg, *toF, 32 );
            }
        } else {
            // integer
            MSG_WriteBits( msg, *toF, field->bits );
        }
    }

    // undo the change.

    if (legacyProtocol) {

        if (fromEvents0 != -1) {
            from->events[0] = fromEvents0;
        }

        if (fromEvents1 != -1) {
            from->events[1] = fromEvents1;
        }

        if (fromEvents2 != -1) {
            from->events[2] = fromEvents2;
        }

        if (fromEvents3 != -1) {
            from->events[3] = fromEvents3;
        }

        if (fromExternalEventParm != -1) {
            from->externalEventParm = fromExternalEventParm;
        }

        if (fromExternalEvent != -1) {
            from->externalEvent = fromExternalEvent;
        }

        if (fromWpn != -1) {
            from->weapon = fromWpn;
        }


        if (toEvents0 != -1) {
            to->events[0] = toEvents0;
        }

        if (toEvents1 != -1) {
            to->events[1] = toEvents1;
        }

        if (toEvents2 != -1) {
            to->events[2] = toEvents2;
        }

        if (toEvents3 != -1) {
            to->events[3] = toEvents3;
        }

        if (toExternalEventParm != -1) {
            to->externalEventParm = toExternalEventParm;
        }

        if (toExternalEvent != -1) {
            to->externalEvent = toExternalEvent;
        }

        if (toWpn != -1) {
            to->weapon = toWpn;
        }

        if (toZoomFov != -1) {
            to->zoomFov = toZoomFov;
        }

        if (fromZoomFov != -1) {
            from->zoomFov = fromZoomFov;
        }
    }

    //
    // send the arrays
    //

    statsbits = 0;
    for (i=0 ; i<MAX_STATS ; i++) {
        if (to->stats[i] != from->stats[i]) {
            statsbits |= 1 << i;
        }
    }

    

    persistantbits = 0;
    for (i=0 ; i<MAX_PERSISTANT ; i++) {
        if (to->persistant[i] != from->persistant[i]) {
            persistantbits |= 1<<i;
        }
    }
    ammobits = 0;
    for (i=0 ; i<MAX_AMMO ; i++) {

        if (legacyProtocol) {
            int legacyInt = translateSilverAmmoToGoldAmmo(i);
            if (to->ammo[legacyInt] != from->ammo[legacyInt]) {
                ammobits |= 1 << i;
            }
        }
        else {


            if (to->ammo[i] != from->ammo[i]) {
                ammobits |= 1 << i;
            }
        }
    }



    clipbits = 0;
    for(i = 0; i<MAX_WEAPONS; i++) {

        if (legacyProtocol) {
            int legacyInt = translateSilverWeaponToGoldWeapon(i);
            if (to->clip[ATTACK_NORMAL][legacyInt] != from->clip[ATTACK_NORMAL][legacyInt]) {
                clipbits |= 1 << i;
            }
        }
        else {

            if (to->clip[ATTACK_NORMAL][i] != from->clip[ATTACK_NORMAL][i]) {
                clipbits |= 1 << i;


            }
        }
    }
    altclipbits = 0;
    for(i = 0; i<MAX_WEAPONS; i++) {

        if (legacyProtocol) {
            int legacyInt = translateSilverWeaponToGoldWeapon(i);
            if (to->clip[ATTACK_ALTERNATE][legacyInt] != from->clip[ATTACK_ALTERNATE][legacyInt]) {
                altclipbits |= 1 << i;
            }
        } else {

            if(to->clip[ATTACK_ALTERNATE][i] != from->clip[ATTACK_ALTERNATE][i]) {
                altclipbits |= 1 << i;
            }
        }
    }



    firemodebits = 0;
    for(i = 0; i<MAX_WEAPONS; i++) {
        
        if (legacyProtocol) {
            int legacyInt = translateSilverWeaponToGoldWeapon(i);
            if (to->firemode[legacyInt] != from->firemode[legacyInt]) {
                firemodebits |= 1 << i;
            }
        }
        else {
            if (to->firemode[i] != from->firemode[i]) {
                firemodebits |= 1 << i;
            }
        }

        
    }

    if (!statsbits && !persistantbits && !ammobits && !clipbits && !altclipbits && !firemodebits) {
        MSG_WriteBits( msg, 0, 1 ); // no change
        oldsize += 4;
        return;
    }
    MSG_WriteBits( msg, 1, 1 ); // changed

    if ( statsbits ) {
        MSG_WriteBits( msg, 1, 1 ); // changed
        MSG_WriteBits( msg, statsbits, MAX_STATS );
        for (i = 0; i < MAX_STATS; i++) {

            if (legacyProtocol && i == STAT_WEAPONS && statsbits & (1 << i)) {

                MSG_WriteLong(msg, translateGoldStatWpnsToSilver(to->stats[i]));

                //if (statsbits & (1 << translateGoldWeaponToSilverWeapon(i)))
                    //MSG_WriteLong(msg, to->stats[i]);
            }
            else {
                if (statsbits & (1 << i))
                    MSG_WriteLong(msg, to->stats[i]);
            }

        }
            
    } else {
        MSG_WriteBits( msg, 0, 1 ); // no change
    }


    if ( persistantbits ) {
        MSG_WriteBits( msg, 1, 1 ); // changed
        MSG_WriteBits( msg, persistantbits, MAX_PERSISTANT );
        for (i=0 ; i<MAX_PERSISTANT ; i++)
            if (persistantbits & (1<<i) )
                MSG_WriteShort (msg, to->persistant[i]);
    } else {
        MSG_WriteBits( msg, 0, 1 ); // no change
    }

    if ( ammobits ) {
        MSG_WriteBits( msg, 1, 1 ); // changed
        MSG_WriteBits( msg, ammobits, MAX_AMMO );
        for (i = 0; i < MAX_AMMO; i++) {
            
            if (legacyProtocol) {
                if (ammobits & (1 << i)) {
                    MSG_WriteShort(msg, to->ammo[translateSilverAmmoToGoldAmmo(i)]);
                }
            }
            else {
                if (ammobits & (1 << i)) {
                    MSG_WriteShort(msg, to->ammo[i]);
                }
            }

            
                
        }
            
    } else {
        MSG_WriteBits( msg, 0, 1 ); // no change
    }

    if ( clipbits ) {
        MSG_WriteBits( msg, 1, 1 );   // changed
        MSG_WriteBits( msg, clipbits, MAX_WEAPONS );
        for (i = 0; i < MAX_WEAPONS; i++) {

            if (legacyProtocol) {
                if (clipbits & (1 << i)) {
                    MSG_WriteByte(msg, to->clip[ATTACK_NORMAL][translateSilverWeaponToGoldWeapon(i)]);
                }
            }
            else {
                if (clipbits & (1 << i)) {
                    MSG_WriteByte(msg, to->clip[ATTACK_NORMAL][i]);
                }
            }

            
                
        }
            
    }
    else {
        MSG_WriteBits( msg, 0, 1 );   // no change
    }

    if ( altclipbits ) {
        MSG_WriteBits( msg, 1, 1 );   // changed
        MSG_WriteBits( msg, altclipbits, MAX_WEAPONS );
        for (i = 0; i < MAX_WEAPONS; i++) {

            if (legacyProtocol) {
                if (altclipbits & (1 << i)) {
                    MSG_WriteByte(msg, to->clip[ATTACK_ALTERNATE][translateSilverWeaponToGoldWeapon(i)]);
                }

            }
            else {
                if (altclipbits & (1 << i)) {
                    MSG_WriteByte(msg, to->clip[ATTACK_ALTERNATE][i]);
                }
            }

            
                
        }
            
    }
    else {
        MSG_WriteBits( msg, 0, 1 );   // no change
    }

    if ( firemodebits ) {
        MSG_WriteBits( msg, 1, 1 );   // changed
        MSG_WriteBits( msg, firemodebits, MAX_WEAPONS );
        for (i = 0; i < MAX_WEAPONS; i++) {

            if (legacyProtocol) {
                if (firemodebits & (1 << i)) {
                    MSG_WriteBits(msg, to->firemode[translateSilverWeaponToGoldWeapon(i)], WP_FIREMODE_MAX);
                }
            }
            else {
                if (firemodebits & (1 << i)) {
                    MSG_WriteBits(msg, to->firemode[i], WP_FIREMODE_MAX);
                }
            }
        }
            
    }
    else {
        MSG_WriteBits( msg, 0, 1 );   // no change
    }
    

}


/*
===================
MSG_ReadDeltaPlayerstate
===================
*/
void MSG_ReadDeltaPlayerstate (msg_t *msg, playerState_t *from, playerState_t *to ) {
    int         i, lc;
    int         bits;
    netField_t  *field;
    int         numFields;
    int         startBit, endBit;
    int         print;
    int         *fromF, *toF;
    int         trunc;
    playerState_t   dummy;

    if ( !from ) {
        from = &dummy;
        Com_Memset( &dummy, 0, sizeof( dummy ) );
    }
    *to = *from;

    if ( msg->bit == 0 ) {
        startBit = msg->readcount * 8 - GENTITYNUM_BITS;
    } else {
        startBit = ( msg->readcount - 1 ) * 8 + msg->bit - GENTITYNUM_BITS;
    }

    // shownet 2/3 will interleave with other printed info, -2 will
    // just print the delta records
    if ( cl_shownet && ( cl_shownet->integer >= 2 || cl_shownet->integer == -2 ) ) {
        print = 1;
        Com_Printf( "%3i: playerstate ", msg->readcount );
    } else {
        print = 0;
    }

    numFields = ARRAY_LEN( playerStateFields );
    lc = MSG_ReadByte(msg);

    if ( lc > numFields || lc < 0 ) {
        Com_Error( ERR_DROP, "invalid playerState field count" );
    }

    for ( i = 0, field = playerStateFields ; i < lc ; i++, field++ ) {
        fromF = (int *)( (byte *)from + field->offset );
        toF = (int *)( (byte *)to + field->offset );

        if ( ! MSG_ReadBits( msg, 1 ) ) {
            // no change
            *toF = *fromF;
        } else {
            if ( field->bits == 0 ) {
                // float
                if ( MSG_ReadBits( msg, 1 ) == 0 ) {
                    // integral float
                    trunc = MSG_ReadBits( msg, FLOAT_INT_BITS );
                    // bias to allow equal parts positive and negative
                    trunc -= FLOAT_INT_BIAS;
                    *(float *)toF = trunc;
                    if ( print ) {
                        Com_Printf( "%s:%i ", field->name, trunc );
                    }
                } else {
                    // full floating point value
                    *toF = MSG_ReadBits( msg, 32 );
                    if ( print ) {
                        Com_Printf( "%s:%f ", field->name, *(float *)toF );
                    }
                }
            } else {
                // integer
                *toF = MSG_ReadBits( msg, field->bits );
                if ( print ) {
                    Com_Printf( "%s:%i ", field->name, *toF );
                }
            }
        }
    }
    for ( i=lc,field = &playerStateFields[lc];i<numFields; i++, field++) {
        fromF = (int *)( (byte *)from + field->offset );
        toF = (int *)( (byte *)to + field->offset );
        // no change
        *toF = *fromF;
    }


    // read the arrays
    if (MSG_ReadBits( msg, 1 ) ) {
        // parse stats
        if ( MSG_ReadBits( msg, 1 ) ) {
            LOG("PS_STATS");
            bits = MSG_ReadBits (msg, MAX_STATS);
            for (i=0 ; i<MAX_STATS ; i++) {
                if (bits & (1<<i) ) {
                    to->stats[i] = MSG_ReadShort(msg);
                }
            }
        }

        // parse persistant stats
        if ( MSG_ReadBits( msg, 1 ) ) {
            LOG("PS_PERSISTANT");
            bits = MSG_ReadBits (msg, MAX_PERSISTANT);
            for (i=0 ; i<MAX_PERSISTANT ; i++) {
                if (bits & (1<<i) ) {
                    to->persistant[i] = MSG_ReadShort(msg);
                }
            }
        }

        // parse ammo
        if ( MSG_ReadBits( msg, 1 ) ) {
            LOG("PS_AMMO");
            bits = MSG_ReadBits (msg, MAX_AMMO);
            for (i=0 ; i<MAX_AMMO ; i++) {
                if (bits & (1<<i) ) {
                    to->ammo[i] = MSG_ReadShort(msg);
                }
            }
        }

        // parse clips
        if(MSG_ReadBits(msg, 1)) {
            LOG("PS_CLIP");
            bits = MSG_ReadBits(msg, MAX_WEAPONS);
            for(i = 0; i<MAX_WEAPONS; i++) {
                if(bits & (1 << i)) {
                    to->clip[ATTACK_NORMAL][i] = MSG_ReadByte(msg);
                }
            }
        }

        // parse alt clip
        if(MSG_ReadBits(msg, 1)) {
            LOG("PS_ALTCLIP");
            bits = MSG_ReadBits(msg, MAX_WEAPONS);
            for(i = 0; i<MAX_WEAPONS; i++) {
                if(bits & (1 << i)) {
                    to->clip[ATTACK_ALTERNATE][i] = MSG_ReadByte(msg);
                }
            }
        }

        // pase firemodes
        if(MSG_ReadBits(msg, 1)) {
            LOG("PS_FIREMODE");
            bits = MSG_ReadBits(msg, MAX_WEAPONS);
            for(i = 0; i<MAX_WEAPONS; i++) {
                if(bits & (1 << i)) {
                    to->firemode[i] = MSG_ReadBits(msg, WP_FIREMODE_MAX);
                }
            }
        }
    }

    if ( print ) {
        if ( msg->bit == 0 ) {
            endBit = msg->readcount * 8 - GENTITYNUM_BITS;
        } else {
            endBit = ( msg->readcount - 1 ) * 8 + msg->bit - GENTITYNUM_BITS;
        }
        Com_Printf( " (%i bits)\n", endBit - startBit  );
    }
}

int msg_hData[256] = {
250315,         // 0
41193,          // 1
6292,           // 2
7106,           // 3
3730,           // 4
3750,           // 5
6110,           // 6
23283,          // 7
33317,          // 8
6950,           // 9
7838,           // 10
9714,           // 11
9257,           // 12
17259,          // 13
3949,           // 14
1778,           // 15
8288,           // 16
1604,           // 17
1590,           // 18
1663,           // 19
1100,           // 20
1213,           // 21
1238,           // 22
1134,           // 23
1749,           // 24
1059,           // 25
1246,           // 26
1149,           // 27
1273,           // 28
4486,           // 29
2805,           // 30
3472,           // 31
21819,          // 32
1159,           // 33
1670,           // 34
1066,           // 35
1043,           // 36
1012,           // 37
1053,           // 38
1070,           // 39
1726,           // 40
888,            // 41
1180,           // 42
850,            // 43
960,            // 44
780,            // 45
1752,           // 46
3296,           // 47
10630,          // 48
4514,           // 49
5881,           // 50
2685,           // 51
4650,           // 52
3837,           // 53
2093,           // 54
1867,           // 55
2584,           // 56
1949,           // 57
1972,           // 58
940,            // 59
1134,           // 60
1788,           // 61
1670,           // 62
1206,           // 63
5719,           // 64
6128,           // 65
7222,           // 66
6654,           // 67
3710,           // 68
3795,           // 69
1492,           // 70
1524,           // 71
2215,           // 72
1140,           // 73
1355,           // 74
971,            // 75
2180,           // 76
1248,           // 77
1328,           // 78
1195,           // 79
1770,           // 80
1078,           // 81
1264,           // 82
1266,           // 83
1168,           // 84
965,            // 85
1155,           // 86
1186,           // 87
1347,           // 88
1228,           // 89
1529,           // 90
1600,           // 91
2617,           // 92
2048,           // 93
2546,           // 94
3275,           // 95
2410,           // 96
3585,           // 97
2504,           // 98
2800,           // 99
2675,           // 100
6146,           // 101
3663,           // 102
2840,           // 103
14253,          // 104
3164,           // 105
2221,           // 106
1687,           // 107
3208,           // 108
2739,           // 109
3512,           // 110
4796,           // 111
4091,           // 112
3515,           // 113
5288,           // 114
4016,           // 115
7937,           // 116
6031,           // 117
5360,           // 118
3924,           // 119
4892,           // 120
3743,           // 121
4566,           // 122
4807,           // 123
5852,           // 124
6400,           // 125
6225,           // 126
8291,           // 127
23243,          // 128
7838,           // 129
7073,           // 130
8935,           // 131
5437,           // 132
4483,           // 133
3641,           // 134
5256,           // 135
5312,           // 136
5328,           // 137
5370,           // 138
3492,           // 139
2458,           // 140
1694,           // 141
1821,           // 142
2121,           // 143
1916,           // 144
1149,           // 145
1516,           // 146
1367,           // 147
1236,           // 148
1029,           // 149
1258,           // 150
1104,           // 151
1245,           // 152
1006,           // 153
1149,           // 154
1025,           // 155
1241,           // 156
952,            // 157
1287,           // 158
997,            // 159
1713,           // 160
1009,           // 161
1187,           // 162
879,            // 163
1099,           // 164
929,            // 165
1078,           // 166
951,            // 167
1656,           // 168
930,            // 169
1153,           // 170
1030,           // 171
1262,           // 172
1062,           // 173
1214,           // 174
1060,           // 175
1621,           // 176
930,            // 177
1106,           // 178
912,            // 179
1034,           // 180
892,            // 181
1158,           // 182
990,            // 183
1175,           // 184
850,            // 185
1121,           // 186
903,            // 187
1087,           // 188
920,            // 189
1144,           // 190
1056,           // 191
3462,           // 192
2240,           // 193
4397,           // 194
12136,          // 195
7758,           // 196
1345,           // 197
1307,           // 198
3278,           // 199
1950,           // 200
886,            // 201
1023,           // 202
1112,           // 203
1077,           // 204
1042,           // 205
1061,           // 206
1071,           // 207
1484,           // 208
1001,           // 209
1096,           // 210
915,            // 211
1052,           // 212
995,            // 213
1070,           // 214
876,            // 215
1111,           // 216
851,            // 217
1059,           // 218
805,            // 219
1112,           // 220
923,            // 221
1103,           // 222
817,            // 223
1899,           // 224
1872,           // 225
976,            // 226
841,            // 227
1127,           // 228
956,            // 229
1159,           // 230
950,            // 231
7791,           // 232
954,            // 233
1289,           // 234
933,            // 235
1127,           // 236
3207,           // 237
1020,           // 238
927,            // 239
1355,           // 240
768,            // 241
1040,           // 242
745,            // 243
952,            // 244
805,            // 245
1073,           // 246
740,            // 247
1013,           // 248
805,            // 249
1008,           // 250
796,            // 251
996,            // 252
1057,           // 253
11457,          // 254
13504,          // 255
};

void MSG_initHuffman( void ) {
    int i,j;

    msgInit = qtrue;
    Huff_Init(&msgHuff);
    for(i=0;i<256;i++) {
        for (j=0;j<msg_hData[i];j++) {
            Huff_addRef(&msgHuff.compressor,    (byte)i);           // Do update
            Huff_addRef(&msgHuff.decompressor,  (byte)i);           // Do update
        }
    }
}

/*
void MSG_NUinitHuffman() {
    byte    *data;
    int     size, i, ch;
    int     array[256];

    msgInit = qtrue;

    Huff_Init(&msgHuff);
    // load it in
    size = FS_ReadFile( "netchan/netchan.bin", (void **)&data );

    for(i=0;i<256;i++) {
        array[i] = 0;
    }
    for(i=0;i<size;i++) {
        ch = data[i];
        Huff_addRef(&msgHuff.compressor,    ch);            // Do update
        Huff_addRef(&msgHuff.decompressor,  ch);            // Do update
        array[ch]++;
    }
    Com_Printf("msg_hData {\n");
    for(i=0;i<256;i++) {
        if (array[i] == 0) {
            Huff_addRef(&msgHuff.compressor,    i);         // Do update
            Huff_addRef(&msgHuff.decompressor,  i);         // Do update
        }
        Com_Printf("%d,         // %d\n", array[i], i);
    }
    Com_Printf("};\n");
    FS_FreeFile( data );
    Cbuf_AddText( "condump dump.txt\n" );
}
*/

//===========================================================================
