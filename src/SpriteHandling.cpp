/*
Ares, a tactical space combat game.
Copyright (C) 1997, 1999-2001, 2008 Nathan Lamont

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

/* SpriteHandling */

#include "SpriteHandling.h"

#include <QDOffscreen.h>

#include "AresResFile.h"
#include "AresGlobalType.h"
#include "ColorTranslation.h"
#include "ConditionalMacros.h"
#include "Debug.h"
#include "DirectText.h"
#include "Error.h"
#include "HandleHandling.h"
#include "MacroColors.h"
#include "MathMacros.h"
#include "NateDraw.h"
#include "NatePixTable.h"
#include "OffscreenGWorld.h"
#include "Processor.h"
#include "Randomize.h" // for static table
#include "Resources.h"
#include "Rotation.h"
#include "SingleDataFile.h"

#define kMaxSpriteNum           500//300

#define kDefaultBoxSize         3

#define kSpriteHandleError      "\pSPHD"

#define kSpriteResFileName      "\p:Ares Data Folder:Ares Sprites"

#define kSpriteBlipThreshhold   kOneQuarterScale
//#define   kBlipSpriteID           1000                // table we need for blips
//#define   kBlipRotDiv             45                  // divide real angle by this to get blip
//#define   kBlipShapeNum           9                   // each color blip has this many shapes
//#define   kBlipRotOffset          1                   // add this to color * blip shape num for 1st rot shape

#define kMinVolatilePixTable    1                   // sound 0 is always there; 1+ is volatile

//#define   kDrawOverride
//#define   kByteLevelTesting

// assembly struct offsets

#define kSpriteTypeWidth        0x0004
#define kSpriteTypeHeight       0x0008
#define kLongRectTop            0x0004
#define kLongRectRight          0x0008
#define kLongRectBottom         0x000c
#define kPixMapRowBytes         0x0004

#define kSolidSquareBlip        0x00000000
#define kTriangeUpBlip          0x00000010
#define kDiamondBlip            0x00000020
#define kPlusBlip               0x00000030
#define kFramedSquareBlip       0x00000040

#define kBlipSizeMask           0x0000000f
#define kBlipTypeMask           0x000000f0

#define kStaticTableSize        2000

extern  WindowPtr       gTheWindow;
extern  PixMapHandle    thePixMapHandle;
extern  GDHandle        theDevice;
extern  GWorldPtr       gOffWorld, gRealWorld, gSaveWorld;
extern  long            gNatePortLeft, gNatePortTop;
extern aresGlobalType   *gAresGlobal;

long                    *gScaleHMap = nil, *gScaleVMap = nil, gAbsoluteScale = MIN_SCALE;
pixTableType            gPixTable[ kMaxPixTableEntry];
Handle                  gSpriteTable = nil, gBothScaleMaps = nil/*, gBlipTable = nil*/, gStaticTable = nil;
short                   gSpriteFileRefID = 0;

Boolean PixelInSprite_IsOutside( spritePix *sprite, long x, long y, long *hmap,
long *vamp);

void ResolveSpriteData( Handle);

void SpriteHandlingInit ( void)

{
    int             i, j;
    unsigned char   *staticValue = nil;
    short           error;

    gBothScaleMaps = NewHandle( sizeof( long) * (long)MAX_PIX_SIZE * 2);
    if ( gBothScaleMaps == nil)
    {
        ShowErrorAny( eExitToShellErr, kErrorStrID, nil, nil, nil, nil, MEMORY_ERROR, -1, -1, -1, __FILE__, 1);
    }
    /*
    MoveHHi( gBothScaleMaps);
    HLock( gBothScaleMaps);
    */
    mHandleLockAndRegister( gBothScaleMaps, nil, nil, ResolveScaleMapData, "\pgBothScaleMaps")

    gScaleHMap = (long *)*gBothScaleMaps;
    gScaleVMap = (long *)*gBothScaleMaps + MAX_PIX_SIZE;

/*
    gSpriteFileRefID = ARF_OpenResFile( kSpriteResFileName);
    error = ResError();

    if ( error != noErr)
    {
        ShowErrorOfTypeOccurred( eContinueOnlyErr, kErrorStrID, kDataFileResourceError, error, __FILE__, 1);
    }
    if ( gSpriteFileRefID == -1)
    {
        ShowErrorAny( eExitToShellErr, kErrorStrID, nil, nil, nil, nil, kSpritesFileError, kDataFolderError, -1, -1, __FILE__, 8);
    }
*/

//  if ( gAresGlobal->externalFileRefNum > 0)
//      UseResFile( gAresGlobal->externalFileRefNum);

    ResetAllPixTables();

    gSpriteTable = NewHandle( sizeof( spriteType) * (long)kMaxSpriteNum);
    if ( gSpriteTable == nil)
    {
        ShowErrorAny( eExitToShellErr, kErrorStrID, nil, nil, nil, nil, MEMORY_ERROR, -1, -1, -1, __FILE__, 2);
    }
    ResetAllSprites();
    /*
    MoveHHi( gSpriteTable);
    HLock( gSpriteTable);
    */
    mDataHandleLockAndRegister( gSpriteTable, nil, ResolveSpriteData, nil, "\pgSpriteTable")

/*
    SetRect( &tRect, 0, 0, WORLD_WIDTH, WORLD_HEIGHT);
    DrawInOffWorld();
    CopySaveWorldToOffWorld( &tRect);
    DrawInRealWorld();
*/

/*  AddPixTable( kBlipSpriteID);
    gBlipTable = GetPixTable( kBlipSpriteID);
    if ( gBlipTable == nil)
        ShowErrorNoRecover( SPRITE_CREATE_ERROR, kSpriteHandleError, 18);
*/
    gStaticTable = NewHandle( sizeof( unsigned char) * (kStaticTableSize * 2));
    if ( gStaticTable == nil)
    {
    }

    mHandleLockAndRegister( gStaticTable, nil, nil, nil, "\pgStaticTable")

    staticValue = (unsigned char *)*gStaticTable;
    for ( i = 0; i < (kStaticTableSize * 2); i++)
    {
        j = Randomize( 256);
        *staticValue = j;
        staticValue++;
    }
}

void ResetAllSprites( void)

{
    spriteType  *aSprite;
    short       i;

    aSprite = ( spriteType *)*gSpriteTable;
    for ( i = 0; i < kMaxSpriteNum; i++)
    {
        aSprite->table = nil;
        aSprite->resID = -1;
        aSprite->killMe = FALSE;
        aSprite->thisRect.left = aSprite->thisRect.top = aSprite->thisRect.right =
                aSprite->thisRect.bottom = aSprite->lastRect.left = aSprite->lastRect.top =
                aSprite->lastRect.right = aSprite->lastRect.bottom = 0;
        aSprite->whichLayer = kNoSpriteLayer;
        aSprite->style = spriteNormal;
        aSprite->styleColor = 0x00;
        aSprite->styleData = 0;
        aSprite++;
    }

}

void ResetAllPixTables( void)

{
    short   i;

    for ( i = 0; i < kMaxPixTableEntry; i++)
    {
        if ( gPixTable[i].resource != nil)
        {
            mHandleDisposeAndDeregister( gPixTable[i].resource)
            gPixTable[i].resource = nil;
        }
        gPixTable[i].keepMe = FALSE;
        gPixTable[i].resID = -1;
    }
}

void CleanupSpriteHandling( void)

{
    int i;

    if ( gBothScaleMaps != nil)
        DisposeHandle( gBothScaleMaps);
//  CloseResFile( gSpriteFileRefID);
    for ( i = 0; i < kMaxPixTableEntry; i++)
    {
        if ( gPixTable[i].resource != nil)
        {
//          mHandleDisposeAndDeregister( gPixTable[i].resource)
            DisposeHandle( gPixTable[i].resource);
        }
    }
    if ( gSpriteTable != nil)
        DisposeHandle( gSpriteTable);
}

void SetAllPixTablesNoKeep( void)

{
    short   i;

    for ( i = kMinVolatilePixTable; i < kMaxPixTableEntry; i++)
        gPixTable[i].keepMe = FALSE;
}

void KeepPixTable( short resID)

{
    short   i = 0;

    while (( gPixTable[i].resID != resID) && ( i < kMaxPixTableEntry)) i++;
    if ( i != kMaxPixTableEntry) gPixTable[i].keepMe = TRUE;
}

void RemoveAllUnusedPixTables( void)

{
    short   i;

    for ( i = kMinVolatilePixTable; i < kMaxPixTableEntry; i++)
    {
        if (( gPixTable[i].keepMe == FALSE) && ( gPixTable[i].resource != nil))
        {
            mHandleDisposeAndDeregister( gPixTable[i].resource)
            gPixTable[i].resource = nil;
            gPixTable[i].keepMe = FALSE;
            gPixTable[i].resID = -1;
        }
    }

}

Handle AddPixTable( short resID)

{
    short           i = 0, realResID = resID;
    short           color = 0;

    mWriteDebugString("\pADDPIX < HANDLE");
    while ((!(( gPixTable[i].resource != nil) && ( gPixTable[i].resID == resID))) && ( i < kMaxPixTableEntry)) i++;
    if ( i == kMaxPixTableEntry)
    {
        i = 0;
        while (( gPixTable[i].resource != nil) && ( i < kMaxPixTableEntry)) i++;
        if ( i == kMaxPixTableEntry)
        {
//          Debugger();
            ShowErrorAny( eExitToShellErr, kErrorStrID, nil, nil, nil, nil, kNoMoreSpriteTablesError, -1, -1, -1, __FILE__, 111);
        }

        if ( realResID & kSpriteTableColorIDMask)
        {
            realResID &= ~kSpriteTableColorIDMask;
            color = ( resID & kSpriteTableColorIDMask) >> kSpriteTableColorShift;
            mWriteDebugString("\pAdd COLORIZED Pix");
            WriteDebugLong( realResID);
            WriteDebugLong( resID);
            WriteDebugLong( color);
        }

        gPixTable[i].resource = HHGetResource( kPixResType, realResID);

        if ( gPixTable[i].resource == nil)
        {
//          Debugger();
            ShowErrorAny( eContinueOnlyErr, kErrorStrID, nil, nil, nil, nil, kLoadSpriteError, -1, -1, -1, __FILE__, resID);
            return( nil);
        }

        DetachResource( gPixTable[i].resource);
        /*
        MoveHHi( gPixTable[i].resource);
        HLock( gPixTable[i].resource);
        */
        mDataHandleLockAndRegister( gPixTable[i].resource, nil, nil, nil, "\pgPixTable[i].resource")

//      WriteDebugLine((char *)"\pADDPIX");
//      WriteDebugLong( resID);

        if ( color == 0)
        {
            RemapNatePixTableColor( gPixTable[i].resource);
        } else
        {
            ColorizeNatePixTableColor( gPixTable[i].resource, color);
        }

        gPixTable[i].resID = resID;
        return( gPixTable[i].resource);
    } return( GetPixTable( resID));
}

Handle GetPixTable( short resID)

{
    short       i = 0;

//  mWriteDebugString("\pGETpix < HANDLE");
    while (( gPixTable[i].resID != resID) && ( i < kMaxPixTableEntry)) i++;
    if ( i == kMaxPixTableEntry) return( nil);
    return ( gPixTable[i].resource);
}

spriteType *AddSprite( Point where, Handle table, short resID, short whichShape, long scale, long size,
                    short layer, unsigned char color, long *whichSprite)

{
    int         i = 0;
    spriteType  *aSprite;

    aSprite = ( spriteType *)*gSpriteTable;
    while (( aSprite->table != nil) && ( i < kMaxSpriteNum)) { i++; aSprite++;}
    if ( i == kMaxSpriteNum)
    {
        *whichSprite = kNoSprite;
        DebugStr("\pNo Free Sprites!");
        return ( nil);
    } else *whichSprite = i;

    aSprite->where = where;
    aSprite->table = table;
    aSprite->resID = resID;
    aSprite->whichShape = whichShape;
    aSprite->scale = scale;
    aSprite->thisRect.left = aSprite->thisRect.top = 0;
    aSprite->thisRect.right = aSprite->thisRect.bottom = -1;
    aSprite->whichLayer = layer;
    aSprite->tinySize = size;
    aSprite->tinyColor = color;
    aSprite->killMe = FALSE;
    aSprite->style = spriteNormal;
    aSprite->styleColor = 0x00;
    aSprite->styleData = 0;

    return ( aSprite);
}
void RemoveSprite( spriteType *aSprite)

{
    aSprite->killMe = FALSE;
    aSprite->table = nil;
    aSprite->resID = -1;
}

void CreateSpritePixFromPixMap( spritePix *sprite, int type, PixMapHandle pixMap, Rect *bounds)

{
    Rect    fixBounds;
    long    rowBytes;
    char    *source, *dest, *mask;
    int     i, j, *rowlength, *count, width;

    fixBounds = *bounds;
    if ( fixBounds.left % 4) fixBounds.left += 4 - ( fixBounds.left % 4);
    if ( fixBounds.right % 4) fixBounds.right += 4 - ( fixBounds.right % 4);
    sprite->type = type;
    sprite->width = fixBounds.right - fixBounds.left + 1;
    sprite->height = fixBounds.bottom - fixBounds.top + 1;
    sprite->center.h = sprite->width / 2;
    sprite->center.v = sprite->height / 2;
    rowBytes = 0x0000ffff & (long)((*pixMap)->rowBytes ^ ROW_BYTES_MASK);
    source = (*pixMap)->baseAddr + (long)fixBounds.top * rowBytes + (long)fixBounds.left;
    switch ( type)
    {
        case BLOCK_TYPE:
            sprite->data = NewHandle( (long)sprite->width * (long)sprite->height);
            if ( sprite->data == nil)
                ShowErrorNoRecover( SPRITE_CREATE_ERROR, kSpriteHandleError, 3);
            HLock( sprite->data);
            dest = *(sprite->data);
            for ( j = 0; j < sprite->height; j++)
            {
                for ( i = 0 ; i < sprite->width; i++)
                    *(dest++) = *(source++);
                source += rowBytes - (long)sprite->width;
            }
            break;
        case MASK_TYPE:
            sprite->data = NewHandle( (long)sprite->width * (long)sprite->height * 2L);
            if ( sprite->data == nil)
                ShowErrorNoRecover( SPRITE_CREATE_ERROR, kSpriteHandleError, 4);
            HLock( sprite->data);
            dest = *(sprite->data);
            mask = dest + (long)sprite->width * (long)sprite->height;
            for ( j = 0; j < sprite->height; j++)
            {
                for ( i = 0 ; i < sprite->width; i++)
                {
                    if ( *source)
                        *mask++ = 0xff;
                    else *mask++ = 0x00;
                    *(dest++) = *(source++);
                }
                source += rowBytes - (long)sprite->width;
            }
            break;
        case COMP_PIX_TYPE:
            sprite->data = NewHandle( (long)sprite->width * (long)sprite->height);
            if ( sprite->data == nil)
                ShowErrorNoRecover( SPRITE_CREATE_ERROR, kSpriteHandleError, 5);
            HLock( sprite->data);
            dest = *(sprite->data);
            for ( j = 0; j < sprite->height; j++)
            {
                for ( i = 0 ; i < sprite->width; i++)
                    *(dest++) = *(source++);
                source += rowBytes - (long)sprite->width;
            }
            break;
        case RUN_LENGTH_TYPE:
        case ASM_LENGTH_TYPE:
            i = sizeof( int);
            sprite->data = NewHandle( (long)sprite->width * (long)sprite->height * 4L + (long)i);
            if ( sprite->data == nil)
                ShowErrorNoRecover( SPRITE_CREATE_ERROR, kSpriteHandleError, 6);
            HLock( sprite->data);
            dest = *(sprite->data);
            for ( j = 0; j < sprite->height; j++)
            {
                rowlength = (int *)dest;
                dest += sizeof(int);
                *rowlength = sizeof(int);
                width = 0;
                while ( width < sprite->width)
                {
                    count = (int *)dest;
                    dest += sizeof(int);
                    *rowlength += sizeof(int);
                    *count = 0;
                    while ( ( *source == 0x00) && ( width < sprite->width))
                    {
                        source++;
                        (*count)++;
                        width++;
                    }
                    count = (int *)dest;
                    dest += sizeof(int);
                    *rowlength += sizeof(int);
                    *count = 0;
                    while ( ( *source != 0x00) && ( width < sprite->width))
                    {
                        (*count)++;
                        *(dest++) = *(source++);
                        *rowlength += 1;
                        width++;
                    }
                    if (( *count > 0) && (width == sprite->width))
                    {
                        count = (int *)dest;
                        dest += sizeof(int);
                        *rowlength += sizeof(int);
                        *count = 0;
                        count = (int *)dest;
                        dest += sizeof(int);
                        *rowlength += sizeof(int);
                        *count = 0;
                    }
                    WriteDebugInt( width);
                }
                source += rowBytes - (long)sprite->width;
            }
            dest = *(sprite->data);
            for ( j = 0; j < 2; j++)
            {
                rowlength = ( int *)dest;
                dest += sizeof(int);
                for ( i = 0; i < *rowlength - sizeof(int); i++)
                {
                    WriteDebug2Int( i, (int)*(dest++));
                }
            }
            break;
        case OLD_SCALE_TYPE:
        case ASM_SCALE_TYPE:
        case EXPERIMENT_TYPE:
            sprite->data = NewHandle( (long)sprite->width * (long)sprite->height);
            if ( sprite->data == nil)
                ShowErrorNoRecover( SPRITE_CREATE_ERROR, kSpriteHandleError, 7);
            HLock( sprite->data);
            dest = *(sprite->data);
            for ( j = 0; j < sprite->height; j++)
            {
                for ( i = 0 ; i < sprite->width; i++)
                    *(dest++) = *(source++);
                source += rowBytes - (long)sprite->width;
            }
            break;

    }
}

/* DrawSpriteInPixMap:
    WARNING: DOES NOT CLIP.  WILL CRASH IF DESTINATION RECT IS NOT CONTAINED IN
    DESTINATION PIX MAP.
*/


void RunLengthSpritePixInPixMap( spritePix *sprite, Point where, PixMapHandle pixMap)

{
    int     width, height, runlen, pixlen, *sword;
    char    *source, *dest;
    long    rowBytes, sRowPlus, dRowPlus, *slong, *dlong;

    rowBytes = 0x0000ffff & (long)((*pixMap)->rowBytes ^ ROW_BYTES_MASK);
    dest = (*pixMap)->baseAddr + (long)where.v * rowBytes + (long)where.h;
    sword = (int *)*(sprite->data);
    sRowPlus = 0;
    dRowPlus = rowBytes - (long)sprite->width;
    width = sprite->width;
    height = sprite->height - 1;

    startrow:
        sword++;
    addnil:
        dest += (long)*sword;
        sword++;
        if ( *sword == 0)
            goto endrow;
        runlen = *sword;
        sword++;
        if ( runlen < 4)
            goto pixbyte;
        pixlen = runlen >> 2;
        runlen %= 4;
        pixlen--;
        dlong = (long *)dest;
        slong = (long *)sword;
    longloop:
        *dlong++ = *slong++;
        if ( --pixlen >= 0)
            goto longloop;
        dest = (char *)dlong;
        sword = (int *)slong;
        if ( runlen == 0)
            goto addnil;

    pixbyte:
        runlen--;
        source = (char *)sword;
    byteloop:
        *dest++ = *source++;
        if ( --runlen >= 0)
            goto byteloop;
        sword = (int *)source;
        goto addnil;

    endrow:
        dest += dRowPlus;
        sword++;
        if ( --height >= 0)
            goto startrow;
}


#ifdef kDontDoLong68KAssem  //powercc

void OptScaleSpritePixInPixMap( spritePix *sprite, Point where, long scale, longRect *dRect,
        longRect *clipRect, PixMapHandle pixMap)

{
    long        mapWidth, mapHeight, x, y, i, h, v, d, last;
    long        shapeRowPlus, destRowPlus, rowbytes, *hmap, *vmap, *hmapoffset, *lhend, scaleCalc;
    char        *destByte, *shapeByte, *hend, *vend, *chunkByte, *chunkend;
    longRect    mapRect, sourceRect;
    Boolean     clipped = FALSE;

    scaleCalc = ((long)sprite->width * scale);
    scaleCalc >>= SHIFT_SCALE;
    mapWidth = scaleCalc;
    scaleCalc = ((long)sprite->height * scale);
    scaleCalc >>= SHIFT_SCALE;
    mapHeight = scaleCalc;

    if ((( where.h + (int)mapWidth) > clipRect->left) && (( where.h -(int) mapWidth) <
            clipRect->right) && (( where.v + (int)mapHeight) > clipRect->top) &&
            (( where.v - (int)mapHeight < clipRect->bottom)) && ( mapHeight > 0) && ( mapWidth > 0))
    {
        scaleCalc = (long)sprite->center.h * scale;
        scaleCalc >>= SHIFT_SCALE;
        dRect->left = where.h - (int)scaleCalc;
        scaleCalc = (long)sprite->center.v * scale;
        scaleCalc >>= SHIFT_SCALE;
        dRect->top = where.v - (int)scaleCalc;

        mapRect.left = mapRect.top = 0;
        mapRect.right = mapWidth;
        mapRect.bottom = mapHeight;
        dRect->right = dRect->left + mapWidth;
        dRect->bottom = dRect->top + mapHeight;

        sourceRect.left = sourceRect.top = 0;
        sourceRect.right = sprite->width;
        sourceRect.bottom = sprite->height;

        if ( dRect->left < clipRect->left)
        {
            mapRect.left += clipRect->left - dRect->left;
            dRect->left = clipRect->left;
            clipped = TRUE;
        }
        if ( dRect->right > clipRect->right)
        {
            mapRect.right -= dRect->right - clipRect->right;// + 1;
            dRect->right = clipRect->right;// - 1;
            clipped = TRUE;
        }
        if ( dRect->top < clipRect->top)
        {
            mapRect.top += clipRect->top - dRect->top;
            dRect->top = clipRect->top;
            clipped = TRUE;
        }
        if ( dRect->bottom > clipRect->bottom)
        {
            mapRect.bottom -= dRect->bottom - clipRect->bottom;// + 1;
            dRect->bottom = clipRect->bottom;// - 1;
            clipped = TRUE;
        }

        if (( (dRect->left + 1) < clipRect->right) && ( dRect->right > clipRect->left) &&
                ( dRect->top < clipRect->bottom) && ( dRect->bottom > clipRect->top))
        {
            if ( scale <= SCALE_SCALE)
            {

                h = sprite->width - 1;
                v = ( mapWidth - 1) << 1;
                d = v - h;
                h = v - ( h << 1);
                x = y = i = last = 0;
                hmap = gScaleHMap;
                if ( v == 0) v = 1;

                while (( x < sprite->width) || ( y < mapWidth))
                {
                    x++;
                    i++;
                    if ( d > 0)
                    {
                        *hmap = ( x - (i >> 1L)) - last;
                        last += *hmap;
                        i = 0;
                        y++;
                        hmap++;
                        d += h;
                    } else d += v;
                }
                *hmap = sprite->width - last;

                h = sprite->height - 1;
                v = ( mapHeight - 1) << 1;
                d = v - h;
                h = v - ( h << 1);
                x = y = i = last = 0;
                vmap = gScaleVMap;
                if ( v == 0) v = 1;

                while (( x < sprite->height) || ( y < mapHeight))
                {
                    x++;
                    i++;
                    if ( d > 0)
                    {
                        *vmap = ( x - ( i >> 1L)) - last;
                        last += *vmap;
                        i = 0;
                        y++;
                        vmap++;
                        d += h;
                    } else d += v;
                }

                *vmap = sprite->height - last;

                if ( clipped)
                {
                    sourceRect.left = 0;
                    hmap = gScaleHMap;
                    d = mapRect.left;
                    while ( d > 0) { sourceRect.left += *hmap++; d--;}

                    sourceRect.right = sourceRect.left;
                    d = mapRect.right - mapRect.left + 1;
                    while ( d > 0) { sourceRect.right += *hmap++; d--;}

                    sourceRect.top = 0;
                    vmap = gScaleVMap;
                    d = mapRect.top;
                    while ( d > 0) { sourceRect.top += *vmap++; d--;}

                    sourceRect.bottom = sourceRect.top;
                    d = mapRect.bottom - mapRect.top + 1;
                    while ( d > 0) { sourceRect.bottom += *vmap++; d--;}


                } // otherwise sourceRect is set

                scaleCalc = (dRect->right - dRect->left);
//              rowbytes = 0x0000ffff & (long)((*pixMap)->rowBytes ^ ROW_BYTES_MASK);

                rowbytes = (long)(*pixMap)->rowBytes;
                rowbytes &= 0x0000ffff;
                rowbytes |= 0x00008000;
                rowbytes ^= 0x00008000;

                destRowPlus = rowbytes - scaleCalc;
                shapeRowPlus = (long)sprite->width - (sourceRect.right - sourceRect.left);                                              //KLUDGE ALERT
                destByte = (*pixMap)->baseAddr + dRect->top * rowbytes + dRect->left;
                shapeByte = *(sprite->data) + sourceRect.top * sprite->width + sourceRect.left;

                vmap = gScaleVMap + mapRect.top;
                hmapoffset = gScaleHMap + mapRect.left;
                vend = destByte + rowbytes * (dRect->bottom - dRect->top);
                y = dRect->bottom - dRect->top;

                shapeRowPlus += *(hmapoffset + scaleCalc);
                mapWidth = (long)sprite->width;
                chunkByte = (*pixMap)->baseAddr + (long)((*pixMap)->bounds.bottom) * rowbytes;

                // for debugging
//              x = dRect->left;
//              y = dRect->top;

                do
                {
                    hmap = hmapoffset;
                    hend = destByte + scaleCalc;

                    // for debugging
//                  x = dRect->left;
//                  TestByte( destByte);

                    do
                    {
                        if ( *shapeByte)
                            *destByte = *shapeByte;

//#ifdef kByteLevelTesting
//                      TestByte( (char *)destByte, *pixMap, "\pSMALLSP");
//#endif

//                      // debugging

                        shapeByte += *hmap++;
                        destByte++;

//                      // debugging
//                      if ( x > clipRect->right)
//                      {
//                          WriteDebugLine( (char *)"\pX:");
//                          WriteDebugLong( hend - destByte);
//                      }
//                      x++;

                    } while ( destByte < hend);
                    destByte += destRowPlus;

                    // debugging
//                  y++;
//                  if ( y > clipRect->bottom)
//                  {
//                      WriteDebugLine( (char *)"\pY:");
//                      WriteDebugLong( y);
//                  }

                    shapeByte += (*vmap++ - 1) * mapWidth + shapeRowPlus;
                } while ( destByte < vend);
//              } while ( --y > 0);
            } else if ( scale <= MAX_SCALE)
            {
                h = mapWidth - 1;
                v = ( sprite->width - 1) << 1;
                d = v - h;
                h = v - ( h << 1);
                x = y = i = last = 0;
                hmap = gScaleHMap;
                vmap = hmapoffset = nil;
                while (( x < mapWidth - 1) || ( y < sprite->width - 1))
                {
                    x++;
                    i++;
                    if ( d > 0)
                    {
                        *hmap = i;

                        i = 0;
                        y++;
                        hmap++;
                        d += h;
                    } else d += v;
                }
                *hmap = i + 1;

                h = mapHeight - 1;
                v = ( sprite->height - 1) << 1;
                d = v - h;
                h = v - ( h << 1);
                x = y = i = last = 0;
                vmap = gScaleVMap;
                hmapoffset = hmap = nil;
                while (( x < mapHeight - 1) || ( y < sprite->height - 1))
                {
                    x++;
                    i++;
                    if ( d > 0)
                    {
                        *vmap = i;

                        i = 0;
                        y++;
                        vmap++;
                        d += h;
                    } else d += v;
                }
                *vmap = i + 1;

                if ( clipped)
                {
                    sourceRect.left = h = 0;
                    hmap = gScaleHMap;
                    while ( ( h + *hmap) < mapRect.left) { h += *hmap++; sourceRect.left++;}
                    x = *hmap;
                    *hmap -= mapRect.left - h;
                    h += x - *hmap;
                    sourceRect.right = sourceRect.left;
                    while ( ( h + *hmap) < mapRect.right) { h += *hmap++; sourceRect.right++;}
                    *hmap = mapRect.right - h;
                    sourceRect.right++;

                    sourceRect.top = h = 0;
                    vmap = gScaleVMap;
                    while ( ( h + *vmap) < mapRect.top) { h += *vmap++; sourceRect.top++;}
                    x = *vmap;
                    *vmap -= mapRect.top - h;
                    h += x - *vmap;
                    sourceRect.bottom = sourceRect.top;
                    while ( ( h + *vmap) < mapRect.bottom) { h += *vmap++; sourceRect.bottom++;}
                    *vmap = mapRect.bottom - h;
                    if ( sourceRect.bottom < sprite->height) sourceRect.bottom++;
                } // otherwise sourceRect is already set

                scaleCalc = (dRect->right - dRect->left);
                rowbytes = 0x0000ffff & (long)((*pixMap)->rowBytes ^ ROW_BYTES_MASK);
                destRowPlus = rowbytes - scaleCalc;
                destByte = (*pixMap)->baseAddr + dRect->top * rowbytes + dRect->left;
                shapeByte = *(sprite->data) + sourceRect.top * (long)sprite->width + sourceRect.left;

                vmap = gScaleVMap + sourceRect.top;
                hmapoffset = gScaleHMap + sourceRect.left;
                shapeRowPlus = sprite->width;
                mapWidth = (long)sprite->width;
                vend = *(sprite->data) + sourceRect.bottom * (long)sprite->width + sourceRect.left;
                lhend = gScaleHMap + sourceRect.right;

                while ( shapeByte < vend)
                {
                    v = *vmap;
                    while ( v > 0)
                    {
                        hmap = hmapoffset;
                        chunkByte = shapeByte;
                        while ( hmap < lhend)
                        {
                            if (( *chunkByte) && ( *hmap))
                            {
                                for ( h = *hmap; h > 0; h--)
                                    *destByte++ = *chunkByte;
                            } else destByte += *hmap;
                            hmap++;
                            chunkByte++;
                        }
                        destByte += destRowPlus;
                        v--;
                    }
                    vmap++;
                    shapeByte += shapeRowPlus;
                }

            } else dRect->left = dRect->right = dRect->top = dRect->bottom = 0;
        } else dRect->left = dRect->right = dRect->top = dRect->bottom = 0;
    } else dRect->left = dRect->right = dRect->top = dRect->bottom = 0;
}

#else


void asm OptScaleSpritePixInPixMap( spritePix *sprite, Point where, long scale, longRect *dRect,
        longRect *clipRect, PixMapHandle pixMap)

{
#ifdef kAllowAssem

    register long   dr3, dr4, dr5, dr6, dr7;
    register long   *ar2, *ar3, *ar4;

    long            clipped, mapWidth, mapHeight, h, y, last, *vmap, *hmap, *shapeByte;
    longRect        mapRect, sourceRect;

        fralloc +

        clr.l   clipped;                                // clipped = false


        //// **** First, calculate the scaled size of the spirte

        // calculate mapWidth = (sprite->width * scale) / SCALE_SCALE
        movea.l sprite, ar2;                            // ar2 = sprite
        move.l  struct(spritePixStruct.width)(ar2), dr6;// dr6 = sprite->width(ar2)
        move.l  scale, dr5;                             // dr5 = scale
        muls.w  dr5, dr6;                               // we're doing short mul
        asr.l   #ASM_SHIFT_SCALE_1, dr6;                    // dr6 is long from mul; we're dividing again to short
        asr.l   #ASM_SHIFT_SCALE_2, dr6;                    // (hack since we want to asr > 8)
        move.l  dr6, mapWidth;                          // mapWidth = dr6

        // calculate mapHeight = (sprite->height * scale) / SCALE_SCALE
        move.l  struct(spritePixStruct.height)(ar2), dr4;   // dr4 = sprite->width(ar2)
        muls.w  dr5, dr4;                               // we're doing short mul
        asr.l   #ASM_SHIFT_SCALE_1, dr4;                    // dr4 is long from mul; we're dividing again to short
        asr.l   #ASM_SHIFT_SCALE_2, dr4;                    // (hack since we want to asr > 8)
        move.l  dr4, mapHeight;                         // mapHeight = dr4

// -> ar2 = sprite, dr4 = mapHeight, dr5 = scale, dr6 = mapWidth

        //// **** now, we'll roughly check to see if we're within the clipRect

        // check the left edge of the clipRect
        // if where.h + mapWidth < clipRect->left then don't draw
        move.w  where.h, dr3;                           // dr3 = where.h
        ext.l   dr3;
        add.l   dr6, dr3;                               // dr3 += dr6(mapWidth)
        movea.l clipRect, ar3;                          // ar3 = clipRect
        cmp.l   struct(longRectStruct.left)(ar3),dr3;   // if dr3 <= ar3->left(clipRect)
        ble     offScreen;                              // then not visable

        // check the right edge
        // if where.h - mapWidth > clipRect->right then don't draw
        move.w  where.h, dr3;                           // dr3 = where.h
        ext.l   dr3;
        sub.l   dr6, dr3;                               // dr3 -= dr6(mapWidth)
        cmp.l   struct(longRectStruct.right)(ar3), dr3; // if dr3 > ar3->right(clipRect)
        bge     offScreen;                              // then not visable

        // check the top edge
        // if where.v + mapHeight < clipRect.top then don't draw
        move.w  where.v, dr3;                           // dr3 = where.v
        ext.l   dr3;
        add.l   dr4, dr3;                               // dr3 += dr4(mapHeight)
        cmp.l   struct(longRectStruct.top)(ar3), dr3;   // if dr3 <= ar3->top(clipRect)
        ble     offScreen;                              // then not visable

        // check the bottom edge
        // if where.v - mapHeight > clipRect.bottom then don't draw
        move.w  where.v, dr3;                           // dr3 = where.v
        ext.l   dr3;
        sub.l   dr4, dr3;                               // dr3 -= dr4(mapHeight)
        cmp.l   struct(longRectStruct.bottom)(ar3), dr3;// if dr3 >= ar3->bottom(clipRect)
        bge     offScreen;

// -> ar3 = clipRect, dr3 = scrap

        //// **** Calculate the destination rect using scaled "center" of sprite
        //          dRect is where the scaled sprite will be drawn in dest pixMap coords

        // calc the top and bottom edges
        // dRect->top = where.v - (sprite->center.v * scale) / SCALE_SCALE

        move.w  struct(spritePixStruct.center.v)(ar2), dr7; // dr7 = ar2->center.v(sprite)

        muls.w  dr5, dr7;                               // dr7 *= scale
        asr.l   #ASM_SHIFT_SCALE_1, dr7;                // dr7 is long from mul; we're dividing again to short
        asr.l   #ASM_SHIFT_SCALE_2, dr7;                // (hack since we want to asr > 8)
        move.w  where.v, dr3;                           // dr3 = where.v
        ext.l   dr3;
        sub.l   dr7, dr3;                               // dr3 -= dr7
        movea.l dRect, ar4;                             // ar4 = dRect
        move.l  dr3, struct(longRectStruct.top)(ar4);   // ar4->top(dRect) = dr3

        // drect->bottom = dRect.top + mapHeight

        add.l   dr4, dr3;                               // dr3 += dr4(mapHeight)
        move.l  dr3, struct(longRectStruct.bottom)(ar4);// ar4->bottom(dRect) = dr3

        // calc the left and right edges
        // dRect->left = where.h - (sprite->center.h * scale) / SCALE_SCALE

        move.w  struct(spritePixStruct.center.h)(ar2), dr7; // dr7 = ar2->center.h(sprite)
        muls.w  dr5, dr7;                               // dr7 *= scale
        asr.l   #ASM_SHIFT_SCALE_1, dr7;                // dr7 is long from mul; we're dividing again to short
        asr.l   #ASM_SHIFT_SCALE_2, dr7;                // (hack since we want to asr > 8)
        move.w  where.h, dr3;                           // dr3 = where.h
        ext.l   dr3;
        sub.l   dr7, dr3;                               // dr3 -= dr7
        move.l  dr3, struct(longRectStruct.left)(ar4);  // ar4->left(dRect) = dr3

        // dRect->right = dRect->left + mapWidth

        add.l   dr6, dr3;                               // dr3 += dr6(mapWidth)
        move.l  dr3, struct(longRectStruct.right)(ar4); // ar4->right(dRect) = dr3

// -> ar4 = dRect, dr3 = scrap, dr7 = scrap

        //// **** set the mapRect & the sourceRect

        // set the mapRect, the bounds of the scaled sprite in absolute coords

        clr.l   mapRect.left;                           // mapRect.left = 0
        clr.l   mapRect.top;                            // mapRect.top = 0
        move.l  dr6, mapRect.right;                     // mapRect.right = mapWidth
        move.l  dr4, mapRect.bottom;                    // mapRect.bottom = mapHeight

        // set the sourceRect, the bounds of the unscaled sprite in absolute coords

        clr.l   sourceRect.left;                        // sourceRect.left = 0
        clr.l   sourceRect.top;                         // sourceRect.top = 0
        move.l  struct(spritePixStruct.width)(ar2), sourceRect.right;   // sourceRect.right = ar2->width(sprite)
        move.l  struct(spritePixStruct.height)(ar2), sourceRect.bottom; // sourceRect.bottom = ar2-height(sprite)

        //// **** Clip the dest and map Rects to the clipRect if needed

        // if dRect->left < clipRect->left { mapRect.left += clipRect->left - dRect->left; dRect->left = clipRect->left;}
        move.l  struct(longRectStruct.left)(ar4), dr3;  // dr3 = ar4->left(dRect)
        cmp.l   struct(longRectStruct.left)(ar3), dr3;  // if dr3 >= ar3->left(clipRect)
        bge     skipClipLeft;                           // then don't clip

        move.l  struct(longRectStruct.left)(ar3), dr3;  // dr3 = ar3->left(clipRect)
        sub.l   struct(longRectStruct.left)(ar4), dr3;  // dr3 -= ar4->left(dRect)
        add.l   dr3, mapRect.left;                      // mapRect.left += dr3
        move.l  struct(longRectStruct.left)(ar3), struct(longRectStruct.left)(ar4); // dRect->left(ar4) = clipRect->left(ar3)
        move.l  #0x01, clipped;                         // clipped = true

        // if dRect->right > clipRect->right { mapRect.right -= dRect->right - clipRect->right; dRect->right = clipRect->right;}
    skipClipLeft:
        move.l  struct(longRectStruct.right)(ar4), dr3; // dr3 = ar4->right(dRect)
        cmp.l   struct(longRectStruct.right)(ar3), dr3; // if dr3 < ar3->right(clipRect)
        ble     skipClipRight;                          // then don't clip

        sub.l   struct(longRectStruct.right)(ar3), dr3; // dr3 -= ar3->right(clipRect)
        sub.l   dr3, mapRect.right;                     // mapRect.right -= dr3
        move.l  struct(longRectStruct.right)(ar3), struct(longRectStruct.right)(ar4);   // ar4->right(dRect) = ar3->right(clipRect)
        move.l  #0x01, clipped

        // if dRect->top < clipRect.top { mapRect.bottom += clipRect->top - dRect->top; dRect->top = clipRect->top;}
    skipClipRight:
        move.l  struct(longRectStruct.top)(ar4), dr3;   // dr3 = ar4->top(dRect)
        cmp.l   struct(longRectStruct.top)(ar3), dr3;   // if dr3 >= ar3->top(clipRect)
        bge     skipClipTop;                            // then don't clip

        move.l  struct(longRectStruct.top)(ar3), dr3;   // else dr3 = ar3->top(clipRect)
        sub.l   struct(longRectStruct.top)(ar4), dr3;   // dr3 -= ar4->top(dRect)
        add.l   dr3, mapRect.top;                       // mapRect.top += dr3
        move.l  struct(longRectStruct.top)(ar3), struct(longRectStruct.top)(ar4);   // ar4->top(dRect) = ar3->top(clipRect)
        move.l  #0x01, clipped

        // if dRect->bottom > clipRect->bottom { mapRect.bottom -= dRect->bottom - clipRect->bottom; dRect->bottom = clipRect->bottom;}
    skipClipTop:
        move.l  struct(longRectStruct.bottom)(ar4), dr3;    // dr3 = ar4->bottom(dRect)
        cmp.l   struct(longRectStruct.bottom)(ar3), dr3;    // if dr3 < ar3->bottom(clipRect)
        ble     skipClipBottom;                             // then don't clip

        sub.l   struct(longRectStruct.bottom)(ar3), dr3;    // dr3 -= ar3->bottom(clipRect)
        sub.l   dr3, mapRect.bottom;                    // mapRect.bottom -= dr3
        move.l  struct(longRectStruct.bottom)(ar3), struct(longRectStruct.bottom)(ar4); // ar4->bottom(dRect) = ar3->bottom(clipRect)
        move.l  #0x01, clipped

// -> dr3 = scrap

        //// **** Check to see if the clipped dest Rect is, in face, within the clipRect

        // if dRect->left > clipRect->right then skip it
    skipClipBottom:
        // if dRect->right <= dRect->left then skip it

        move.l  struct(longRectStruct.right)(ar4), dr3; // dr3 = ar4->left(dRect)
        cmp.l   struct(longRectStruct.left)(ar4), dr3;  // if ( dr3 <= ar4->left(clipRect)
        ble     offScreen

        // if dRect->bottom <= dRect->top then skip it

        move.l  struct(longRectStruct.bottom)(ar4), dr3;    // dr3 = ar4->bottom(dRect)
        cmp.l   struct(longRectStruct.top)(ar4), dr3;   // if ( dr3 <= ar4->top(clipRect)
        ble     offScreen

        // if scale > SCALE_SCALE then scale up

        cmpi.l  #SCALE_SCALE, dr5;                      // if dr5(scale) > SCALE_SCALE
        bgt     scaleSpriteUp;                          // then scale up

        // if scale < MIN_SCALE

        cmpi.l  #MIN_SCALE, dr5;                        // if dr5(scale) < MIN_SCALE
        blt     offScreen;                              // then skip it

// -> dr3 = scrap

        //// ********************************* ////
        //// **** SCALE DOWN
        //// ********************************* ////

        //// **** Prepare for the horizontal scale down map

//      jmp     offScreen;

        move.l  struct(spritePixStruct.width)(ar2), dr3;    // dr3 = ar2->width(sprite)
        subq.l  #0x1, dr3;                              // dr3 -= 1
        subq.l  #0x1, dr6;                              // dr6(mapWidth) -= 1
        add.l   dr6, dr6;                               // dr6(v) *= 2 >> v = (mapWidth -1) * 2
        move.l  dr6, dr5;                               // dr5(d) = dr6(v)
        sub.l   dr3, dr5;                               // dr5(d) -= dr3(h) >> d = v - h
        add.l   dr3, dr3;                               // dr3 *= 2
        move.l  dr6, h;                                 // h = v
        sub.l   dr3, h;                                 // h -= dr3 >> h = v - (h * 2)
        clr.l   dr7;                                    // dr7(x) = 0
        clr.l   dr4;                                    // dr4(i) = 0
        clr.l   y;                                      // y = 0
        clr.l   last;                                   // last = 0
        movea.l gScaleHMap, ar4;                        // ar4(hmap) = gScaleHMap

        tst.l   dr6;
        bne     makeDownHMapCheck;                      // if dr6(v) != 0 then go ahead

        move.l  #1, dr6;                                    // else dr6(v) = 1 (don't want to scale to 0)
        jmp     makeDownHMapCheck;

    makeDownHMapLoop:
        addq.l  #1, dr7;                                // dr7(x) += 1
        addq.l  #1, dr4;                                // dr4(i) += 1
        tst.l   dr5;                                    // if dr5(d) <= 0
        ble     downHMapIsNegative;                     // then handle d <= 0 case

        move.l  dr7, (ar4);                             // *ar4(hmap) = dr7(x)
        move.l  dr4, dr3;                               // dr3 = dr4(i)
        asr.l   #0x1, dr3;                              // dr3 *= 2
        sub.l   dr3, (ar4);                             // *ar4(hmap) -= dr3

        move.l  last, dr3;                              // dr3 = last
        sub.l   dr3, (ar4);                             // ar4 -= dr3

        add.l   (ar4), dr3;                             // dr3 += *ar4(hmap)
        move.l  dr3, last;                              // last = ar3;

        addq.l  #1, y;                                  // y++
        adda.l  #0x4, ar4;                              // ar4(hmap) += 4
        add.l   h, dr5;                                 // dr5(d) += h
        clr.l   dr4;                                    // dr4(i) = 0
        jmp     makeDownHMapCheck;

    downHMapIsNegative:
        add.l   dr6, dr5;                               // dr5(d) += dr6(v)

    makeDownHMapCheck:
        cmp.l   struct(spritePixStruct.width)(ar2), dr7;    // if dr7(x) <= ar2->width(sprite)
        blt     makeDownHMapLoop;                       // then continue loop

        move.l  mapWidth, dr3;                          // dr3 = mapWidth
        cmp.l   y, dr3;                                 // if dr3 > y
        bgt     makeDownHMapLoop;                       // then continue loop

        move.l  struct(spritePixStruct.width)(ar2), dr3;    // dr3 = ar2->width(sprite)
        sub.l   last, dr3;                              // dr3 -= last
        move.l  dr3, (ar4);                             // ar4(hmap) = dr3

// -> dr3, dr4, dr5, dr6, dr7 = scrap, ar4 = scrap

        //// **** Prepare for the vertical scale down map

        move.l  struct(spritePixStruct.height)(ar2), dr3;   // dr3 = ar2->height(sprite)
        subq.l  #0x1, dr3;                              // dr3 -= 1
        move.l  mapHeight, dr6;                         // dr6 = mapHeight
        subq.l  #0x1, dr6;                              // dr6(mapHeight) -= 1
        add.l   dr6, dr6;                               // dr6(v) *= 2 >> v = (mapHeight -1) * 2
        move.l  dr6, dr5;                               // dr5(d) = dr6(v)
        sub.l   dr3, dr5;                               // dr5(d) -= dr3(h) >> d = v - h
        add.l   dr3, dr3;                               // dr3 *= 2
        move.l  dr6, h;                                 // h = v
        sub.l   dr3, h;                                 // h -= dr3 >> h = v - (h * 2)
        clr.l   dr7;                                    // dr7(x) = 0
        clr.l   dr4;                                    // dr4(i) = 0
        clr.l   y;                                      // y = 0
        clr.l   last;                                   // last = 0
        movea.l gScaleVMap, ar4;                        // ar4(vmap) = gScaleVMap

        tst.l   dr6;
        bne     makeDownVMapCheck;                      // if dr6(v) != 0 then go ahead

        move.l  #1, dr6;                                    // else dr6(v) = 1 (don't want to scale to 0)
        jmp     makeDownVMapCheck;

    makeDownVMapLoop:
        addq.l  #1, dr7;                                // dr7(x) += 1
        addq.l  #1, dr4;                                // dr4(i) += 1
        tst.l   dr5;                                    // if dr5(d) <= 0
        ble     downVMapIsNegative;                     // then handle d <= 0 case

        move.l  dr7, (ar4);                             // *ar4(vmap) = dr7(x)
        move.l  dr4, dr3;                               // dr3 = dr4(i)
        asr.l   #0x1, dr3;                              // dr3 *= 2
        sub.l   dr3, (ar4);                             // *ar4(vmap) -= dr3

        move.l  last, dr3;                              // dr3 = last
        sub.l   dr3, (ar4);                             // ar4 -= dr3

        add.l   (ar4), dr3;                             // dr3 += *ar4(vmap)
        move.l  dr3, last;                              // last = ar3;

        addq.l  #1, y;                                  // y++
        adda.l  #0x4, ar4;                              // ar4(vmap) += 4
        add.l   h, dr5;                                 // dr5(d) += h
        clr.l   dr4;                                    // dr4(i) = 0
        jmp     makeDownVMapCheck;

    downVMapIsNegative:
        add.l   dr6, dr5;                               // dr5(d) += dr6(v)

    makeDownVMapCheck:
        cmp.l   struct(spritePixStruct.height)(ar2), dr7;   // if dr7(x) <= ar2->height(sprite)
        blt     makeDownVMapLoop;                       // then continue loop

        move.l  mapHeight, dr3;                         // dr3 = mapWidth
        cmp.l   y, dr3;                                 // if dr3 > y
        bgt     makeDownVMapLoop;                       // then continue loop

        move.l  struct(spritePixStruct.height)(ar2), dr3;   // dr3 = ar2->height(sprite)
        sub.l   last, dr3;                              // dr3 -= last
        move.l  dr3, (ar4);                             // ar4(hmap) = dr3

        tst.l   clipped;
        beq     scaleDownPredraw;

// -> dr3, dr4, dr5, dr6, dr7 = scrap, ar4 = scrap

        //// **** Clip the source rect

        // left edge
        clr.l   dr5;                                    // dr5(left) = 0
        movea.l gScaleHMap, ar2;                        // ar2(hmap) = gScaleHMap
        move.l  mapRect.left, dr3;                      // dr3 = mapRect.left
        bra     scaleDownClipLeftSourceCheck;

    scaleDownClipLeftSourceLoop:
        add.l   (ar2)+, dr5;                            // dr5(left) += *ar2++(hmap)

    scaleDownClipLeftSourceCheck:
        dbra    dr3, scaleDownClipLeftSourceLoop;

        move.l  dr5, sourceRect.left;

        // right edge
        move.l  mapRect.right, dr3;
        sub.l   mapRect.left, dr3;
        addq.l  #0x1, dr3;

        bra     scaleDownClipRightSourceCheck;

    scaleDownClipRightSourceLoop:
        add.l   (ar2)+, dr5;

    scaleDownClipRightSourceCheck:
        dbra    dr3, scaleDownClipRightSourceLoop;

        move.l  dr5, sourceRect.right;

        // top edge

        clr.l   dr5;
        movea.l gScaleVMap, ar2;
        move.l  mapRect.top, dr3;
        bra     scaleDownClipTopSourceCheck;

    scaleDownClipTopSourceLoop:
        add.l   (ar2)+, dr5;

    scaleDownClipTopSourceCheck:
        dbra    dr3, scaleDownClipTopSourceLoop;

        move.l  dr5, sourceRect.top;

        // bottom edge

        move.l  mapRect.bottom, dr3;
        sub.l   mapRect.top, dr3;

        bra     scaleDownClipBottomSourceCheck;

    scaleDownClipBottomSourceLoop:
        add.l   (ar2)+, dr5;

    scaleDownClipBottomSourceCheck:
        dbra    dr3, scaleDownClipBottomSourceLoop;

        move.l  dr5, sourceRect.bottom;

// -> dr3, dr5 = scrap, ar2 = scrap

        //// **** Draw the Scaled-down, clipped sprite

    scaleDownPredraw:

//      _Debugger;

        movea.l pixMap, ar2;                            // ar2 = pixMap
        movea.l (ar2), ar2;                             // ar2 = *ar2(pixMap)
        move.w  struct(PixMap.rowBytes)(ar2), dr3;      // dr3 = ar2->rowBytes(pixMap)
        eori.l  #ROW_BYTES_MASK, dr3;                   // dr3 &= ~(ROW_BYTES_MASK)
        ext.l   dr3;
        move.l  dr3, dr5;                               // dr5(destRowPlus) = dr3(rowBytes)

        movea.l dRect, ar4;                             // ar4 = dRect
        move.l  struct(longRectStruct.right)(ar4), dr7; // dr7 = ar4->right(dRect)
        sub.l   struct(longRectStruct.left)(ar4), dr7;  // dr7 -= ar4->left(dRect)
        sub.l   dr7, dr5;                               // dr5 -= dr7
        move.l  struct(longRectStruct.top)(ar4), dr4;   // dr4 = ar4->top(dRect)
        mulu.w  dr4, dr3;                               // dr3(rowBytes) *= dr4;
        move.l  dr3, h;                                 // h = dr3;
        movea.l struct(PixMap.baseAddr)(ar2), ar2;      // ar2 = ar2->rowBytes(pixMap)
        adda.l  dr3, ar2;                               // ar2(destByte) += dr3(destRowPlus)
        adda.l  struct(longRectStruct.left)(ar4), ar2;  // ar2(destByte) += ar4->left(dRect)

        move.l  sourceRect.top, dr3;                    // dr3 = sourceRect.top
        movea.l sprite, ar3;                            // ar3 = sprite
        move.l  struct(spritePixStruct.width)(ar3), dr4;        // dr4 = ar3->width(sprite)
        move.l  dr4, mapWidth;                          // mapWidth = dr4;
        mulu.w  dr4, dr3;                               // dr3 *= dr4;
        movea.l struct(spritePixStruct.data)(ar3), ar4; // ar4 = ar3->data(sprite)
        movea.l (ar4), ar4;                             // ar4 = *ar4(sprite->data)
        adda.l  dr3, ar4;                               // ar4 += dr3;
        adda.l  sourceRect.left, ar4;                   // ar4(shapeByte) += sourceRect.left

        move.l  sourceRect.right, dr3;                  // dr3 = sourceRect.right
        sub.l   sourceRect.left, dr3;                   // dr3 -= sourceRect.left
        sub.l   dr3, dr4;                               // dr4 (shapeRowPlus) -= dr3;

        move.l  gScaleVMap, vmap;                       // vmap = gScaleVMap;
        move.l  mapRect.top, dr3;                       // dr3 = mapRect.top
        lsl.l   #0x2, dr3;                              // dr3 *= 4 (long ptr)
        add.l   dr3, vmap;                              // vmap += dr3

        move.l  mapRect.bottom, dr6;                    // dr6 = mapRect.bottom;
        sub.l   mapRect.top, dr6;                       // dr6 -= mapRect.top

        move.l  gScaleHMap, hmap;                       // hmap = gScaleVMap;
        move.l  mapRect.left, dr3;                      // dr3 = mapRect.left
        lsl.l   #0x2, dr3;                              // dr3 *= 4
        add.l   dr3, hmap;                              // hmap += dr3

        movea.l hmap, ar3;
        move.l  dr7, dr3;
        lsl.l   #0x2, dr3;
        adda.l  dr3, ar3;

        add.l   (ar3), dr4;                             // dr4 (shapeRowPlus) += *ar3(hmap)
        move.l  dr7, h;
        move.l  mapWidth, dr7;

        move.l  h, dr3;
        cmp.l   #0x1, dr3;
        bgt     scaleDownVerticalCheck;
        jmp     offScreen;

    scaleDownVerticalLoop:
        move.l  hmap, ar3;                              // ar3 = hmap
        move.l  h, dr3;                                 // dr3 = h;
        bra     scaleDownHorizontalCheck;

    scaleDownHorizontalLoop:
        tst.b   (ar4);                                  // if !*ar4
        beq     scaleDownNilPixel;                      // then don't draw

        move.b  (ar4), (ar2)                            // else ar2 = ar4

    scaleDownNilPixel:
        adda.l  (ar3)+, ar4;                            // ar4 += *ar3++
        adda.l  #0x1, ar2;                              // ar2 += 1

    scaleDownHorizontalCheck:
        dbra    dr3, scaleDownHorizontalLoop            // if dr3 > 0 then continue row

        adda.l  dr5, ar2;                               // ar2 += dr5
        movea.l vmap, ar3;
        move.l  (ar3)+, dr3;                            // dr3 = ar3+
        move.l  ar3, vmap;
        subq.l  #0x1, dr3;
        muls.w  dr7, dr3;
        adda.l  dr3, ar4;
        adda.l  dr4, ar4;

    scaleDownVerticalCheck:
        dbra    dr6, scaleDownVerticalLoop

        jmp     scaleSpriteReturn;

        //// ********************************* ////
        //// **** SCALE UP
        //// ********************************* ////

// -> current ar2 = sprite, ar3 = clipRect, ar4 = dRect, dr4 = mapHeight, dr5 = scale, dr6 = mapWidth
// -> dr3, dr7 = scrap

    scaleSpriteUp:
        cmpi.l  #MAX_SCALE, dr5;
        bgt     offScreen;

        //// **** Prepare for the horizontal scale up map

//      _Debugger;
//      jmp     offScreen;

        move.l  dr6, h;                                 // h = mapWidth
        sub.l   #0x1, h;
        move.l  struct(spritePixStruct.width)(ar2), dr3;    // dr3(v) = ar2->width(sprite)
        sub.l   #0x1, dr3;                              // dr3 -= 1
        add.l   dr3, dr3;                               // dr3 *= 2
        move.l  dr3, last;                              // last(v) = dr3
        move.l  dr3, dr7;                               // dr7(d) = dr3
        sub.l   h, dr7;                                 // dr7(d) -= h
        move.l  h, dr3;                                 // dr3 = h
        add.l   dr3, dr3;                               // dr3 *= 2
        move.l  last, h;                                // h = last
        sub.l   dr3, h;                                 // h -= dr3
        clr.l   dr5;
        clr.l   y;
        clr.l   dr4;
        movea.l gScaleHMap, ar4;
        subq.l  #1, dr6;                                // dr6(mapWidth) -= 1

        jmp     makeUpHMapCheck;

    makeUpHMapLoop:
        addq.l  #0x1, dr5;                              // dr5(x) += 1
        addq.l  #0x1, dr4;                              // dr4(i) += 1
        tst.l   dr7;                                    // if d <= 0
        ble     upHMapDIsNegative;                      // then handle

        move.l  dr4, (ar4)+;                            // *ar4++(hmap) = dr4(i)
        clr.l   dr4;                                    // dr4(i) = 0
        addq.l  #0x1, y;                                // y += 1
        add.l   h, dr7;                                 // dr7(d) += h
        jmp     makeUpHMapCheck;

    upHMapDIsNegative:
        add.l   last, dr7;                              // dr7(d) += last(v)

    makeUpHMapCheck:
        cmp.l   dr5, dr6;                               // if dr6 > x
        bgt     makeUpHMapLoop;

        move.l  struct(spritePixStruct.width)(ar2), dr3;    // dr3 = ar2->width(sprite)
        sub.l   #0x1, dr3;                              // dr3 -= 1
        cmp.l   y, dr3;                                 // if dr3 > y
        bgt     makeUpHMapLoop;

        move.l  dr4, (ar4);                             // *ar4 = dr4(i)
        addq.l  #0x1, (ar4);                            // *ar4 += 1

        //// **** Prepare for the vertical scale up map

        move.l  mapHeight, dr6;
        move.l  dr6, h;                                 // h = mapHeight
        sub.l   #0x1, h;
        move.l  struct(spritePixStruct.height)(ar2), dr3;   // dr3(v) = ar2->height(sprite)
        sub.l   #0x1, dr3;                              // dr3 -= 1
        add.l   dr3, dr3;                               // dr3 *= 2
        move.l  dr3, last;                              // last(v) = dr3
        move.l  dr3, dr7;                               // dr7(d) = dr3
        sub.l   h, dr7;                                 // dr7(d) -= h
        move.l  h, dr3;                                 // dr3 = h
        add.l   dr3, dr3;                               // dr3 *= 2
        move.l  last, h;                                // h = last
        sub.l   dr3, h;                                 // h -= dr3
        clr.l   dr5;
        clr.l   y;
        clr.l   dr4;
        movea.l gScaleVMap, ar4;
        subq.l  #1, dr6;                                // dr6(mapHeight) -= 1

        jmp     makeUpVMapCheck;

    makeUpVMapLoop:
        addq.l  #0x1, dr5;                              // dr5(x) += 1
        addq.l  #0x1, dr4;                              // dr4(i) += 1
        tst.l   dr7;                                    // if d <= 0
        ble     upVMapDIsNegative;                      // then handle

        move.l  dr4, (ar4)+;                            // *ar4++(vmap) = dr4(i)
        clr.l   dr4;                                    // dr4(i) = 0
        addq.l  #0x1, y;                                // y += 1
        add.l   h, dr7;                                 // dr7(d) += h
        jmp     makeUpVMapCheck;

    upVMapDIsNegative:
        add.l   last, dr7;                              // dr7(d) += last(v)

    makeUpVMapCheck:
        cmp.l   dr5, dr6;                               // if dr6 > x
        bgt     makeUpVMapLoop;

        move.l  struct(spritePixStruct.height)(ar2), dr3;// dr3 = ar2->height(sprite)
        sub.l   #0x1, dr3;                              // dr3 -= 1
        cmp.l   y, dr3;                                 // if dr3 > y
        bgt     makeUpVMapLoop;

        move.l  dr4, (ar4);                             // *ar4 = dr4(i)
        addq.l  #0x1, (ar4);                            // *ar4 += 1

        tst.l   clipped;
        beq     scaleUpPredraw;

        // clip the sourceRect and scale maps for scaling up

        // clip the left edge

        clr.l   dr7;                                    // dr7(h) = 0
        clr.l   dr4;                                    // dr4(sourceRect.left) = 0
        move.l  mapRect.left, dr5;                      // dr5 = mapRect.left
        movea.l gScaleHMap, ar4;                        // ar4(hmap) = gScaleHMap
        bra     scaleUpClipLeftMap;

    scaleUpClipLeftMapLoop:
        add.l   (ar4)+, dr7;                            // dr7(h) += (ar4)+ (*hmap++)
        addq.l  #1, dr4;                                // dr4(sourceRect.left) += 1

    scaleUpClipLeftMap:
        move.l  dr7, dr3;                               // dr3 = dr7(h)
        add.l   (ar4), dr3;                             // dr3 += (ar4)(*hmap)
        cmp.l   dr5, dr3;                               // if dr3 < dr5 then
        blt     scaleUpClipLeftMapLoop;                 // repeat loop

        move.l  dr4, sourceRect.left;                   // sourceRect.left = dr4
        move.l  (ar4), dr6;                             // dr6(x) = (ar4)
        move.l  dr5, dr3;                               // dr3 = dr5(mapRect.left)
        sub.l   dr7, dr3;                               // dr3 -= dr7(h)
        sub.l   dr3, (ar4);                             // (ar4)(*hmap) -= dr3
        sub.l   (ar4), dr6;                             // dr6(x) -= (ar4)(*hmap)
        add.l   dr6, dr7;                               // dr7(h) += dr6(x)
        move.l  mapRect.right, dr5;                     // dr5 = mapRect.right

        // clip the scale up right edge

        bra     scaleUpClipRightMap;

    scaleUpClipRightMapLoop:
        add.l   (ar4)+, dr7;                            // dr7(h) += (ar4)(*hmap)
        addq.l  #1, dr4;                                // dr4(sourceRect.right) += 1;

    scaleUpClipRightMap:
        move.l  dr7, dr3;                               // dr3 = dr7(h)
        add.l   (ar4), dr3;                             // dr3 += (ar4)(*hmap)
        cmp.l   dr5, dr3;                               // if dr3 < dr5(mapRect.right)
        blt     scaleUpClipRightMapLoop;                // then repeat loop

        addq.l  #1, dr4;                                // dr4(sourceRect.right) += 1;
        move.l  dr4, sourceRect.right;                  // sourceRect.right = dr4
        sub.l   dr7, dr5;                               // dr5(mapRect.right) -= dr7(h)
        move.l  dr5, (ar4);                             // (ar4)(*hmap) = dr5(mapRect.right - h)

        // clip the top edge

        clr.l   dr7;                                    // dr7(h) = 0
        clr.l   dr4;                                    // dr4(sourceRect.left) = 0
        move.l  mapRect.top, dr5;                       // dr5 = mapRect.left
        movea.l gScaleVMap, ar4;                        // ar4(hmap) = gScaleHMap
        bra     scaleUpClipTopMap;

    scaleUpClipTopMapLoop:
        add.l   (ar4)+, dr7;                            // dr7(h) += (ar4)+ (*hmap++)
        addq.l  #1, dr4;                                // dr4(sourceRect.left) += 1

    scaleUpClipTopMap:
        move.l  dr7, dr3;                               // dr3 = dr7(h)
        add.l   (ar4), dr3;                             // dr3 += (ar4)(*hmap)
        cmp.l   dr5, dr3;                               // if dr3 < dr5 then
        blt     scaleUpClipTopMapLoop;                  // repeat loop

        move.l  dr4, sourceRect.top;                    // sourceRect.left = dr4
        move.l  (ar4), dr6;                             // dr6(x) = (ar4)
        move.l  dr5, dr3;                               // dr3 = dr5(mapRect.left)
        sub.l   dr7, dr3;                               // dr3 -= dr7(h)
        sub.l   dr3, (ar4);                             // (ar4)(*hmap) -= dr3
        sub.l   (ar4), dr6;                             // dr6(x) -= (ar4)(*hmap)
        add.l   dr6, dr7;                               // dr7(h) += dr6(x)
        move.l  mapRect.bottom, dr5;                        // dr5 = mapRect.right

        // clip the scale up right edge

        bra     scaleUpClipBottomMap;

    scaleUpClipBottomMapLoop:
        add.l   (ar4)+, dr7;                            // dr7(h) += (ar4)(*hmap)
        addq.l  #1, dr4;                                // dr4(sourceRect.right) += 1;

    scaleUpClipBottomMap:
        move.l  dr7, dr3;                               // dr3 = dr7(h)
        add.l   (ar4), dr3;                             // dr3 += (ar4)(*hmap)
        cmp.l   dr5, dr3;                               // if dr3 < dr5(mapRect.right)
        blt     scaleUpClipBottomMapLoop;               // then repeat loop

        move.l  dr4, sourceRect.bottom;                 // sourceRect.right = dr4
        sub.l   dr7, dr5;                               // dr5 -= dr7(h);
        move.l  dr5, (ar4);                             // (ar4)(*vmap) = dr5;
        move.l  struct(spritePixStruct.height)(ar2), dr3;   // dr3 = ar2->height(sprite)
        cmp.l   dr3, dr4;                               // if dr3(sprite->height) > dr4(sourceRect.bottom)
        bgt     scaleUpPredraw;                         // then draw

        addq.l  #1, sourceRect.bottom;                  // else sourceRect.bottom += 1, then draw

        //// **** Draw the scaled up, clipped sprite

    scaleUpPredraw:
        movea.l pixMap, ar2;                            // ar2 = pixmap
        movea.l (ar2), ar2;
        move.w  struct(PixMap.rowBytes)(ar2), dr4;      // dr4 = ar2->rowBytes(pixMap)
        eori.l  #ROW_BYTES_MASK, dr4;                   // dr4 ^= ~ROW_BYTES_MASK
        ext.l   dr4;
        move.l  dr4, dr5;

        movea.l dRect, ar3;                             // ar3 = dRect
        move.l  struct(longRectStruct.right)(ar3), dr3; // dr3 = ar3->right(dRect)
        sub.l   struct(longRectStruct.left)(ar3), dr3;  // dr3 -= ar3->left
        sub.l   dr3, dr5;                               // dr5(rowBytes) -= dr3;
        move.l  struct(longRectStruct.top)(ar3), dr3;   // dr3 = ar3->top(dRect)
        mulu.w  dr3, dr4;                               // dr4(rowBytes) *= dr3;
        movea.l struct(PixMap.baseAddr)(ar2), ar2;      // ar2 = ar2->baseAddr(pixmap)
        adda.l  dr4, ar2;                               // ar2(destByte) += dr4;
        adda.l  struct(longRectStruct.left)(ar3), ar2;  // ar2(destByte) += ar3->left(dRect)

        movea.l sprite, ar3;
        move.l  sourceRect.top, dr3;                    // dr3 = sourceRect.top
        move.l  struct(spritePixStruct.width)(ar3), dr6;    // dr6 = ar3->width(sprite)
        mulu.w  dr6, dr3;                               // dr3 *= dr6;
        movea.l struct(spritePixStruct.data)(ar3), ar4; // ar4 = ar3->data(sprite)
        movea.l (ar4), ar4;
        adda.l  dr3, ar4;                               // ar4(shapeByte) += dr3;
        adda.l  sourceRect.left, ar4;                   // ar4 += sourceRect.left

        move.l  struct(spritePixStruct.width)(ar3), h;  // h = ar3->width(sprite)
        move.l  sourceRect.right, dr3;                  // dr3 = sourceRect.right
        sub.l   sourceRect.left, dr3;                   // dr3 -= sourceRect.left
        move.l  dr3, mapWidth;                          // mapWidth = dr3;
        movea.l gScaleVMap, ar3;                        // ar3 = gScaleVMap;
        move.l  sourceRect.top, dr3;                    // dr3 = sourceRect.top
        lsl.l   #0x2, dr3;                              // dr3 *= 4
        adda.l  dr3, ar3;                               // ar3 += dr3
        move.l  ar3, vmap;                              // vmap = ar3;

        move.l  sourceRect.bottom, dr4;                 // dr4 = sourceRect.bottom
        sub.l   sourceRect.top, dr4;                    // dr4 -= sourcerect.top
        move.l  sourceRect.left, dr3;                   // dr3 = sourceRect.left
        lsl.l   #0x02, dr3;
        move.l  gScaleHMap, hmap;
        add.l   dr3, hmap;

        jmp     scaleUpVerticalCheck;

    scaleUpVerticalLoop:
        movea.l vmap, ar3;
        move.l  (ar3), dr6;
        jmp     scaleUpVerticalPixelCheck;

    scaleUpVerticalPixelLoop:
        movea.l hmap, ar3;
        move.l  ar4, shapeByte;
        move.l  mapWidth, dr7;
        jmp     scaleUpHorizontalCheck;

    scaleUpHorizontalLoop:
        tst.b   (ar4)
        beq     scaleUpSkipHPixel;

        move.l  #MAX_SCALE_PIX, dr3;
        sub.l   (ar3), dr3;
        add.l   dr3, dr3;
        jmp     scaleUpPutHPix(dr3);

    scaleUpPutHPix:
        move.b  (ar4), (ar2)+;
        move.b  (ar4), (ar2)+;
        move.b  (ar4), (ar2)+;
        move.b  (ar4), (ar2)+;
        move.b  (ar4), (ar2)+;
        move.b  (ar4), (ar2)+;
        move.b  (ar4), (ar2)+;
        move.b  (ar4), (ar2)+;

        move.b  (ar4), (ar2)+;
        move.b  (ar4), (ar2)+;
        move.b  (ar4), (ar2)+;
        move.b  (ar4), (ar2)+;
        move.b  (ar4), (ar2)+;
        move.b  (ar4), (ar2)+;
        move.b  (ar4), (ar2)+;
        move.b  (ar4), (ar2)+;

        move.b  (ar4), (ar2)+;
        move.b  (ar4), (ar2)+;
        move.b  (ar4), (ar2)+;
        move.b  (ar4), (ar2)+;
        move.b  (ar4), (ar2)+;
        move.b  (ar4), (ar2)+;
        move.b  (ar4), (ar2)+;
        move.b  (ar4), (ar2)+;

        move.b  (ar4), (ar2)+;
        move.b  (ar4), (ar2)+;
        move.b  (ar4), (ar2)+;
        move.b  (ar4), (ar2)+;
        move.b  (ar4), (ar2)+;
        move.b  (ar4), (ar2)+;
        move.b  (ar4), (ar2)+;
        move.b  (ar4), (ar2)+;

        jmp     scaleUpDoneHPixel;

    scaleUpSkipHPixel:
        adda.l  (ar3), ar2;

    scaleUpDoneHPixel:
        addq.l  #0x4, ar3;
        addq.l  #0x1, ar4;

    scaleUpHorizontalCheck:
        dbra    dr7, scaleUpHorizontalLoop;

        adda.l  dr5, ar2;
        movea.l shapeByte, ar4;

    scaleUpVerticalPixelCheck:
        dbra    dr6, scaleUpVerticalPixelLoop;

        movea.l vmap, ar3;
        adda.l  #0x4, ar3;
        adda.l  h, ar4;
        move.l  ar3, vmap;

    scaleUpVerticalCheck:
        dbra    dr4, scaleUpVerticalLoop;

        jmp     scaleSpriteReturn;

    offScreen:
        movea.l dRect, ar2;
        clr.l   struct(longRectStruct.left)(ar2);
        clr.l   struct(longRectStruct.top)(ar2);
        clr.l   struct(longRectStruct.right)(ar2);
        clr.l   struct(longRectStruct.bottom)(ar2);

    scaleSpriteReturn:
        frfree

#endif // kAllowAssem
    rts
}


#endif

void StaticScaleSpritePixInPixMap( spritePix *sprite, Point where, long scale, longRect *dRect,
        longRect *clipRect, PixMapHandle pixMap, short staticValue)

{
    long        mapWidth, mapHeight, x, y, i, h, v, d, last;
    long        shapeRowPlus, destRowPlus, rowbytes, *hmap, *vmap, *hmapoffset, *lhend, scaleCalc;
    char        *destByte, *shapeByte, *hend, *vend, *chunkByte;
    unsigned char   *staticByte;
    longRect    mapRect, sourceRect;
    Boolean     clipped = FALSE;

    scaleCalc = ((long)sprite->width * scale);
    scaleCalc >>= SHIFT_SCALE;
    mapWidth = scaleCalc;
    scaleCalc = ((long)sprite->height * scale);
    scaleCalc >>= SHIFT_SCALE;
    mapHeight = scaleCalc;

    if ((( where.h + (int)mapWidth) > clipRect->left) && (( where.h -(int) mapWidth) <
            clipRect->right) && (( where.v + (int)mapHeight) > clipRect->top) &&
            (( where.v - (int)mapHeight < clipRect->bottom)) && ( mapHeight > 0) && ( mapWidth > 0))
    {
        scaleCalc = (long)sprite->center.h * scale;
        scaleCalc >>= SHIFT_SCALE;
        dRect->left = where.h - (int)scaleCalc;
        scaleCalc = (long)sprite->center.v * scale;
        scaleCalc >>= SHIFT_SCALE;
        dRect->top = where.v - (int)scaleCalc;

        mapRect.left = mapRect.top = 0;
        mapRect.right = mapWidth;
        mapRect.bottom = mapHeight;
        dRect->right = dRect->left + mapWidth;
        dRect->bottom = dRect->top + mapHeight;

        sourceRect.left = sourceRect.top = 0;
        sourceRect.right = sprite->width;
        sourceRect.bottom = sprite->height;

        if ( dRect->left < clipRect->left)
        {
            mapRect.left += clipRect->left - dRect->left;
            dRect->left = clipRect->left;
            clipped = TRUE;
        }
        if ( dRect->right > clipRect->right)
        {
            mapRect.right -= dRect->right - clipRect->right;// + 1;
            dRect->right = clipRect->right;// - 1;
            clipped = TRUE;
        }
        if ( dRect->top < clipRect->top)
        {
            mapRect.top += clipRect->top - dRect->top;
            dRect->top = clipRect->top;
            clipped = TRUE;
        }
        if ( dRect->bottom > clipRect->bottom)
        {
            mapRect.bottom -= dRect->bottom - clipRect->bottom;// + 1;
            dRect->bottom = clipRect->bottom;// - 1;
            clipped = TRUE;
        }

        if (( (dRect->left + 1) < clipRect->right) && ( dRect->right > clipRect->left) &&
                ( dRect->top < clipRect->bottom) && ( dRect->bottom > clipRect->top))
        {
            staticByte = (unsigned char *)*gStaticTable + (long)staticValue;
            if ( scale <= SCALE_SCALE)
            {

                h = sprite->width - 1;
                v = ( mapWidth - 1) << 1;
                d = v - h;
                h = v - ( h << 1);
                x = y = i = last = 0;
                hmap = gScaleHMap;
                if ( v == 0) v = 1;

                while (( x < sprite->width) || ( y < mapWidth))
                {
                    x++;
                    i++;
                    if ( d > 0)
                    {
                        *hmap = ( x - (i >> 1L)) - last;
                        last += *hmap;
                        i = 0;
                        y++;
                        hmap++;
                        d += h;
                    } else d += v;
                }
                *hmap = sprite->width - last;

                h = sprite->height - 1;
                v = ( mapHeight - 1) << 1;
                d = v - h;
                h = v - ( h << 1);
                x = y = i = last = 0;
                vmap = gScaleVMap;
                if ( v == 0) v = 1;

                while (( x < sprite->height) || ( y < mapHeight))
                {
                    x++;
                    i++;
                    if ( d > 0)
                    {
                        *vmap = ( x - ( i >> 1L)) - last;
                        last += *vmap;
                        i = 0;
                        y++;
                        vmap++;
                        d += h;
                    } else d += v;
                }

                *vmap = sprite->height - last;

                if ( clipped)
                {
                    sourceRect.left = 0;
                    hmap = gScaleHMap;
                    d = mapRect.left;
                    while ( d > 0) { sourceRect.left += *hmap++; d--;}

                    sourceRect.right = sourceRect.left;
                    d = mapRect.right - mapRect.left + 1;
                    while ( d > 0) { sourceRect.right += *hmap++; d--;}

                    sourceRect.top = 0;
                    vmap = gScaleVMap;
                    d = mapRect.top;
                    while ( d > 0) { sourceRect.top += *vmap++; d--;}

                    sourceRect.bottom = sourceRect.top;
                    d = mapRect.bottom - mapRect.top + 1;
                    while ( d > 0) { sourceRect.bottom += *vmap++; d--;}


                } // otherwise sourceRect is set

                scaleCalc = (dRect->right - dRect->left);

                rowbytes = (long)(*pixMap)->rowBytes;
                rowbytes &= 0x0000ffff;
                rowbytes |= 0x00008000;
                rowbytes ^= 0x00008000;

                destRowPlus = rowbytes - scaleCalc;
                shapeRowPlus = (long)sprite->width - (sourceRect.right - sourceRect.left);                                              //KLUDGE ALERT
                destByte = (*pixMap)->baseAddr + dRect->top * rowbytes + dRect->left;
                shapeByte = *(sprite->data) + sourceRect.top * sprite->width + sourceRect.left;

                vmap = gScaleVMap + mapRect.top;
                hmapoffset = gScaleHMap + mapRect.left;
                vend = destByte + rowbytes * (dRect->bottom - dRect->top);
                y = dRect->bottom - dRect->top;

                shapeRowPlus += *(hmapoffset + scaleCalc);
                mapWidth = (long)sprite->width;
                chunkByte = (*pixMap)->baseAddr + (long)((*pixMap)->bounds.bottom) * rowbytes;

                do
                {
                    hmap = hmapoffset;
                    hend = destByte + scaleCalc;
                    if ( (staticValue + scaleCalc) > ( kStaticTableSize))
                    {
                        staticValue += scaleCalc - kStaticTableSize;
                        staticByte = (unsigned char *)*gStaticTable + (long)staticValue;
                    } else staticValue += scaleCalc;

                    do
                    {
                        if ( *shapeByte)
                            *destByte = *staticByte;

#ifdef kByteLevelTesting
                        TestByte( (char *)destByte, *pixMap, "\pSMALLSP");
#endif

                        shapeByte += *hmap++;
                        destByte++;
                        staticByte++;


                    } while ( destByte < hend);
                    destByte += destRowPlus;


                    shapeByte += (*vmap++ - 1) * mapWidth + shapeRowPlus;
                } while ( destByte < vend);
            } else if ( scale <= MAX_SCALE)
            {
                h = mapWidth - 1;
                v = ( sprite->width - 1) << 1;
                d = v - h;
                h = v - ( h << 1);
                x = y = i = last = 0;
                hmap = gScaleHMap;
                vmap = hmapoffset = nil;
                while (( x < mapWidth - 1) || ( y < sprite->width - 1))
                {
                    x++;
                    i++;
                    if ( d > 0)
                    {
                        *hmap = i;

                        i = 0;
                        y++;
                        hmap++;
                        d += h;
                    } else d += v;
                }
                *hmap = i + 1;

                h = mapHeight - 1;
                v = ( sprite->height - 1) << 1;
                d = v - h;
                h = v - ( h << 1);
                x = y = i = last = 0;
                vmap = gScaleVMap;
                hmapoffset = hmap = nil;
                while (( x < mapHeight - 1) || ( y < sprite->height - 1))
                {
                    x++;
                    i++;
                    if ( d > 0)
                    {
                        *vmap = i;

                        i = 0;
                        y++;
                        vmap++;
                        d += h;
                    } else d += v;
                }
                *vmap = i + 1;

                if ( clipped)
                {
                    sourceRect.left = h = 0;
                    hmap = gScaleHMap;
                    while ( ( h + *hmap) < mapRect.left) { h += *hmap++; sourceRect.left++;}
                    x = *hmap;
                    *hmap -= mapRect.left - h;
                    h += x - *hmap;
                    sourceRect.right = sourceRect.left;
                    while ( ( h + *hmap) < mapRect.right) { h += *hmap++; sourceRect.right++;}
                    *hmap = mapRect.right - h;
                    sourceRect.right++;

                    sourceRect.top = h = 0;
                    vmap = gScaleVMap;
                    while ( ( h + *vmap) < mapRect.top) { h += *vmap++; sourceRect.top++;}
                    x = *vmap;
                    *vmap -= mapRect.top - h;
                    h += x - *vmap;
                    sourceRect.bottom = sourceRect.top;
                    while ( ( h + *vmap) < mapRect.bottom) { h += *vmap++; sourceRect.bottom++;}
                    *vmap = mapRect.bottom - h;
                    if ( sourceRect.bottom < sprite->height) sourceRect.bottom++;
                } // otherwise sourceRect is already set

                scaleCalc = (dRect->right - dRect->left);
                rowbytes = 0x0000ffff & (long)((*pixMap)->rowBytes ^ ROW_BYTES_MASK);
                destRowPlus = rowbytes - scaleCalc;
                destByte = (*pixMap)->baseAddr + dRect->top * rowbytes + dRect->left;
                shapeByte = *(sprite->data) + sourceRect.top * (long)sprite->width + sourceRect.left;

                vmap = gScaleVMap + sourceRect.top;
                hmapoffset = gScaleHMap + sourceRect.left;
                shapeRowPlus = sprite->width;
                mapWidth = (long)sprite->width;
                vend = *(sprite->data) + sourceRect.bottom * (long)sprite->width + sourceRect.left;
                lhend = gScaleHMap + sourceRect.right;

                while ( shapeByte < vend)
                {
                    v = *vmap;
                    while ( v > 0)
                    {
                        hmap = hmapoffset;
                        chunkByte = shapeByte;
                        if ( (staticValue + mapWidth) > ( kStaticTableSize))
                        {
                            staticValue += mapWidth - kStaticTableSize;
                            staticByte = (unsigned char *)*gStaticTable + (long)staticValue;
                        } else staticValue += mapWidth;
                        while ( hmap < lhend)
                        {
                            if (( *chunkByte) && ( *hmap))
                            {
                                for ( h = *hmap; h > 0; h--)
                                    *destByte++ = *staticByte;
                            } else destByte += *hmap;
                            hmap++;
                            chunkByte++;
                            staticByte++;
                        }
                        destByte += destRowPlus;
                        v--;
                    }
                    vmap++;
                    shapeByte += shapeRowPlus;
                }

            } else dRect->left = dRect->right = dRect->top = dRect->bottom = 0;
        } else dRect->left = dRect->right = dRect->top = dRect->bottom = 0;
    } else dRect->left = dRect->right = dRect->top = dRect->bottom = 0;
}

void ColorScaleSpritePixInPixMap( spritePix *sprite, Point where, long scale, longRect *dRect,
        longRect *clipRect, PixMapHandle pixMap, short staticValue, unsigned char color,
        unsigned char colorAmount)

{
    long        mapWidth, mapHeight, x, y, i, h, v, d, last;
    long        shapeRowPlus, destRowPlus, rowbytes, *hmap, *vmap, *hmapoffset, *lhend, scaleCalc;
    char        *destByte, *shapeByte, *hend, *vend, *chunkByte;
    unsigned char   *staticByte;
    longRect    mapRect, sourceRect;
    Boolean     clipped = FALSE;

    scaleCalc = ((long)sprite->width * scale);
    scaleCalc >>= SHIFT_SCALE;
    mapWidth = scaleCalc;
    scaleCalc = ((long)sprite->height * scale);
    scaleCalc >>= SHIFT_SCALE;
    mapHeight = scaleCalc;

    if ((( where.h + (int)mapWidth) > clipRect->left) && (( where.h -(int) mapWidth) <
            clipRect->right) && (( where.v + (int)mapHeight) > clipRect->top) &&
            (( where.v - (int)mapHeight < clipRect->bottom)) && ( mapHeight > 0) && ( mapWidth > 0))
    {
        scaleCalc = (long)sprite->center.h * scale;
        scaleCalc >>= SHIFT_SCALE;
        dRect->left = where.h - (int)scaleCalc;
        scaleCalc = (long)sprite->center.v * scale;
        scaleCalc >>= SHIFT_SCALE;
        dRect->top = where.v - (int)scaleCalc;

        mapRect.left = mapRect.top = 0;
        mapRect.right = mapWidth;
        mapRect.bottom = mapHeight;
        dRect->right = dRect->left + mapWidth;
        dRect->bottom = dRect->top + mapHeight;

        sourceRect.left = sourceRect.top = 0;
        sourceRect.right = sprite->width;
        sourceRect.bottom = sprite->height;

        if ( dRect->left < clipRect->left)
        {
            mapRect.left += clipRect->left - dRect->left;
            dRect->left = clipRect->left;
            clipped = TRUE;
        }
        if ( dRect->right > clipRect->right)
        {
            mapRect.right -= dRect->right - clipRect->right;// + 1;
            dRect->right = clipRect->right;// - 1;
            clipped = TRUE;
        }
        if ( dRect->top < clipRect->top)
        {
            mapRect.top += clipRect->top - dRect->top;
            dRect->top = clipRect->top;
            clipped = TRUE;
        }
        if ( dRect->bottom > clipRect->bottom)
        {
            mapRect.bottom -= dRect->bottom - clipRect->bottom;// + 1;
            dRect->bottom = clipRect->bottom;// - 1;
            clipped = TRUE;
        }

        if (( (dRect->left + 1) < clipRect->right) && ( dRect->right > clipRect->left) &&
                ( dRect->top < clipRect->bottom) && ( dRect->bottom > clipRect->top))
        {
            staticByte = (unsigned char *)*gStaticTable + (long)staticValue;
            if ( scale <= SCALE_SCALE)
            {

                h = sprite->width - 1;
                v = ( mapWidth - 1) << 1;
                d = v - h;
                h = v - ( h << 1);
                x = y = i = last = 0;
                hmap = gScaleHMap;
                if ( v == 0) v = 1;

                while (( x < sprite->width) || ( y < mapWidth))
                {
                    x++;
                    i++;
                    if ( d > 0)
                    {
                        *hmap = ( x - (i >> 1L)) - last;
                        last += *hmap;
                        i = 0;
                        y++;
                        hmap++;
                        d += h;
                    } else d += v;
                }
                *hmap = sprite->width - last;

                h = sprite->height - 1;
                v = ( mapHeight - 1) << 1;
                d = v - h;
                h = v - ( h << 1);
                x = y = i = last = 0;
                vmap = gScaleVMap;
                if ( v == 0) v = 1;

                while (( x < sprite->height) || ( y < mapHeight))
                {
                    x++;
                    i++;
                    if ( d > 0)
                    {
                        *vmap = ( x - ( i >> 1L)) - last;
                        last += *vmap;
                        i = 0;
                        y++;
                        vmap++;
                        d += h;
                    } else d += v;
                }

                *vmap = sprite->height - last;

                if ( clipped)
                {
                    sourceRect.left = 0;
                    hmap = gScaleHMap;
                    d = mapRect.left;
                    while ( d > 0) { sourceRect.left += *hmap++; d--;}

                    sourceRect.right = sourceRect.left;
                    d = mapRect.right - mapRect.left + 1;
                    while ( d > 0) { sourceRect.right += *hmap++; d--;}

                    sourceRect.top = 0;
                    vmap = gScaleVMap;
                    d = mapRect.top;
                    while ( d > 0) { sourceRect.top += *vmap++; d--;}

                    sourceRect.bottom = sourceRect.top;
                    d = mapRect.bottom - mapRect.top + 1;
                    while ( d > 0) { sourceRect.bottom += *vmap++; d--;}


                } // otherwise sourceRect is set

                scaleCalc = (dRect->right - dRect->left);

                rowbytes = (long)(*pixMap)->rowBytes;
                rowbytes &= 0x0000ffff;
                rowbytes |= 0x00008000;
                rowbytes ^= 0x00008000;

                destRowPlus = rowbytes - scaleCalc;
                shapeRowPlus = (long)sprite->width - (sourceRect.right - sourceRect.left);                                              //KLUDGE ALERT
                destByte = (*pixMap)->baseAddr + dRect->top * rowbytes + dRect->left;
                shapeByte = *(sprite->data) + sourceRect.top * sprite->width + sourceRect.left;

                vmap = gScaleVMap + mapRect.top;
                hmapoffset = gScaleHMap + mapRect.left;
                vend = destByte + rowbytes * (dRect->bottom - dRect->top);
                y = dRect->bottom - dRect->top;

                shapeRowPlus += *(hmapoffset + scaleCalc);
                mapWidth = (long)sprite->width;
                chunkByte = (*pixMap)->baseAddr + (long)((*pixMap)->bounds.bottom) * rowbytes;

                if ( color != 0xff)
                {
                    do
                    {
                        hmap = hmapoffset;
                        hend = destByte + scaleCalc;
                        if ( (staticValue + scaleCalc) > ( kStaticTableSize))
                        {
                            staticValue += scaleCalc - kStaticTableSize;
                            staticByte = (unsigned char *)*gStaticTable + (long)staticValue;
                        } else staticValue += scaleCalc;

                        do
                        {
                            if ( *shapeByte)
                            {
                                if ( *staticByte > colorAmount)
                                    *destByte = *shapeByte;
                                else *destByte = color;
                            }

                            shapeByte += *hmap++;
                            destByte++;
                            staticByte++;


                        } while ( destByte < hend);
                        destByte += destRowPlus;


                        shapeByte += (*vmap++ - 1) * mapWidth + shapeRowPlus;
                    } while ( destByte < vend);
                } else // black is a special case--we don't want to draw black color
                {
                    do
                    {
                        hmap = hmapoffset;
                        hend = destByte + scaleCalc;
                        if ( (staticValue + scaleCalc) > ( kStaticTableSize))
                        {
                            staticValue += scaleCalc - kStaticTableSize;
                            staticByte = (unsigned char *)*gStaticTable + (long)staticValue;
                        } else staticValue += scaleCalc;

                        do
                        {
                            if ( *shapeByte)
                            {
                                if ( *staticByte > colorAmount)
                                    *destByte = *shapeByte;
                            }

                            shapeByte += *hmap++;
                            destByte++;
                            staticByte++;


                        } while ( destByte < hend);
                        destByte += destRowPlus;


                        shapeByte += (*vmap++ - 1) * mapWidth + shapeRowPlus;
                    } while ( destByte < vend);
                }
            } else if ( scale <= MAX_SCALE)
            {
                h = mapWidth - 1;
                v = ( sprite->width - 1) << 1;
                d = v - h;
                h = v - ( h << 1);
                x = y = i = last = 0;
                hmap = gScaleHMap;
                vmap = hmapoffset = nil;
                while (( x < mapWidth - 1) || ( y < sprite->width - 1))
                {
                    x++;
                    i++;
                    if ( d > 0)
                    {
                        *hmap = i;

                        i = 0;
                        y++;
                        hmap++;
                        d += h;
                    } else d += v;
                }
                *hmap = i + 1;

                h = mapHeight - 1;
                v = ( sprite->height - 1) << 1;
                d = v - h;
                h = v - ( h << 1);
                x = y = i = last = 0;
                vmap = gScaleVMap;
                hmapoffset = hmap = nil;
                while (( x < mapHeight - 1) || ( y < sprite->height - 1))
                {
                    x++;
                    i++;
                    if ( d > 0)
                    {
                        *vmap = i;

                        i = 0;
                        y++;
                        vmap++;
                        d += h;
                    } else d += v;
                }
                *vmap = i + 1;

                if ( clipped)
                {
                    sourceRect.left = h = 0;
                    hmap = gScaleHMap;
                    while ( ( h + *hmap) < mapRect.left) { h += *hmap++; sourceRect.left++;}
                    x = *hmap;
                    *hmap -= mapRect.left - h;
                    h += x - *hmap;
                    sourceRect.right = sourceRect.left;
                    while ( ( h + *hmap) < mapRect.right) { h += *hmap++; sourceRect.right++;}
                    *hmap = mapRect.right - h;
                    sourceRect.right++;

                    sourceRect.top = h = 0;
                    vmap = gScaleVMap;
                    while ( ( h + *vmap) < mapRect.top) { h += *vmap++; sourceRect.top++;}
                    x = *vmap;
                    *vmap -= mapRect.top - h;
                    h += x - *vmap;
                    sourceRect.bottom = sourceRect.top;
                    while ( ( h + *vmap) < mapRect.bottom) { h += *vmap++; sourceRect.bottom++;}
                    *vmap = mapRect.bottom - h;
                    if ( sourceRect.bottom < sprite->height) sourceRect.bottom++;
                } // otherwise sourceRect is already set

                scaleCalc = (dRect->right - dRect->left);
                rowbytes = 0x0000ffff & (long)((*pixMap)->rowBytes ^ ROW_BYTES_MASK);
                destRowPlus = rowbytes - scaleCalc;
                destByte = (*pixMap)->baseAddr + dRect->top * rowbytes + dRect->left;
                shapeByte = *(sprite->data) + sourceRect.top * (long)sprite->width + sourceRect.left;

                vmap = gScaleVMap + sourceRect.top;
                hmapoffset = gScaleHMap + sourceRect.left;
                shapeRowPlus = sprite->width;
                mapWidth = (long)sprite->width;
                vend = *(sprite->data) + sourceRect.bottom * (long)sprite->width + sourceRect.left;
                lhend = gScaleHMap + sourceRect.right;

                while ( shapeByte < vend)
                {
                    v = *vmap;
                    while ( v > 0)
                    {
                        hmap = hmapoffset;
                        chunkByte = shapeByte;
                        if ( (staticValue + mapWidth) > ( kStaticTableSize))
                        {
                            staticValue += mapWidth - kStaticTableSize;
                            staticByte = (unsigned char *)*gStaticTable + (long)staticValue;
                        } else staticValue += mapWidth;
                        while ( hmap < lhend)
                        {
                            if (( *chunkByte) && ( *hmap))
                            {
                                if ( *staticByte > colorAmount)
                                {
                                    for ( h = *hmap; h > 0; h--)
                                        *destByte++ = *chunkByte;
                                } else
                                {
                                    for ( h = *hmap; h > 0; h--)
                                        *destByte++ = color;
                                }
                            } else destByte += *hmap;
                            hmap++;
                            chunkByte++;
                            staticByte++;
                        }
                        destByte += destRowPlus;
                        v--;
                    }
                    vmap++;
                    shapeByte += shapeRowPlus;
                }

            } else dRect->left = dRect->right = dRect->top = dRect->bottom = 0;
        } else dRect->left = dRect->right = dRect->top = dRect->bottom = 0;
    } else dRect->left = dRect->right = dRect->top = dRect->bottom = 0;
}

/* a hack; not fast
*/

void OutlineScaleSpritePixInPixMap( spritePix *sprite, Point where, long scale, longRect *dRect,
        longRect *clipRect, PixMapHandle pixMap, unsigned char colorOut,
        unsigned char colorIn)

{
    long        mapWidth, mapHeight, x, y, i, h, v, d, last, sourceX, sourceY;
    long        shapeRowPlus, destRowPlus, rowbytes, *hmap, *vmap, *hmapoffset, *lhend, scaleCalc;
    char        *destByte, *shapeByte, *hend, *vend, *chunkByte, *chunkend;
    longRect    mapRect, sourceRect;
    Boolean     clipped = FALSE;

    scaleCalc = ((long)sprite->width * scale);
    scaleCalc >>= SHIFT_SCALE;
    mapWidth = scaleCalc;
    scaleCalc = ((long)sprite->height * scale);
    scaleCalc >>= SHIFT_SCALE;
    mapHeight = scaleCalc;

    if ((( where.h + (int)mapWidth) > clipRect->left) && (( where.h -(int) mapWidth) <
            clipRect->right) && (( where.v + (int)mapHeight) > clipRect->top) &&
            (( where.v - (int)mapHeight < clipRect->bottom)) && ( mapHeight > 0) && ( mapWidth > 0))
    {
        scaleCalc = (long)sprite->center.h * scale;
        scaleCalc >>= SHIFT_SCALE;
        dRect->left = where.h - (int)scaleCalc;
        scaleCalc = (long)sprite->center.v * scale;
        scaleCalc >>= SHIFT_SCALE;
        dRect->top = where.v - (int)scaleCalc;

        mapRect.left = mapRect.top = 0;
        mapRect.right = mapWidth;
        mapRect.bottom = mapHeight;
        dRect->right = dRect->left + mapWidth;
        dRect->bottom = dRect->top + mapHeight;

        sourceRect.left = sourceRect.top = 0;
        sourceRect.right = sprite->width;
        sourceRect.bottom = sprite->height;

        if ( dRect->left < clipRect->left)
        {
            mapRect.left += clipRect->left - dRect->left;
            dRect->left = clipRect->left;
            clipped = TRUE;
        }
        if ( dRect->right > clipRect->right)
        {
            mapRect.right -= dRect->right - clipRect->right;// + 1;
            dRect->right = clipRect->right;// - 1;
            clipped = TRUE;
        }
        if ( dRect->top < clipRect->top)
        {
            mapRect.top += clipRect->top - dRect->top;
            dRect->top = clipRect->top;
            clipped = TRUE;
        }
        if ( dRect->bottom > clipRect->bottom)
        {
            mapRect.bottom -= dRect->bottom - clipRect->bottom;// + 1;
            dRect->bottom = clipRect->bottom;// - 1;
            clipped = TRUE;
        }

        if (( (dRect->left + 1) < clipRect->right) && ( dRect->right > clipRect->left) &&
                ( dRect->top < clipRect->bottom) && ( dRect->bottom > clipRect->top))
        {
            if ( scale <= SCALE_SCALE)
            {

                h = sprite->width - 1;
                v = ( mapWidth - 1) << 1;
                d = v - h;
                h = v - ( h << 1);
                x = y = i = last = 0;
                hmap = gScaleHMap;
                if ( v == 0) v = 1;

                while (( x < sprite->width) || ( y < mapWidth))
                {
                    x++;
                    i++;
                    if ( d > 0)
                    {
                        *hmap = ( x - (i >> 1L)) - last;
                        last += *hmap;
                        i = 0;
                        y++;
                        hmap++;
                        d += h;
                    } else d += v;
                }
                *hmap = sprite->width - last;

                h = sprite->height - 1;
                v = ( mapHeight - 1) << 1;
                d = v - h;
                h = v - ( h << 1);
                x = y = i = last = 0;
                vmap = gScaleVMap;
                if ( v == 0) v = 1;

                while (( x < sprite->height) || ( y < mapHeight))
                {
                    x++;
                    i++;
                    if ( d > 0)
                    {
                        *vmap = ( x - ( i >> 1L)) - last;
                        last += *vmap;
                        i = 0;
                        y++;
                        vmap++;
                        d += h;
                    } else d += v;
                }

                *vmap = sprite->height - last;

                if ( clipped)
                {
                    sourceRect.left = 0;
                    hmap = gScaleHMap;
                    d = mapRect.left;
                    while ( d > 0) { sourceRect.left += *hmap++; d--;}

                    sourceRect.right = sourceRect.left;
                    d = mapRect.right - mapRect.left + 1;
                    while ( d > 0) { sourceRect.right += *hmap++; d--;}

                    sourceRect.top = 0;
                    vmap = gScaleVMap;
                    d = mapRect.top;
                    while ( d > 0) { sourceRect.top += *vmap++; d--;}

                    sourceRect.bottom = sourceRect.top;
                    d = mapRect.bottom - mapRect.top + 1;
                    while ( d > 0) { sourceRect.bottom += *vmap++; d--;}


                } // otherwise sourceRect is set

                scaleCalc = (dRect->right - dRect->left);
//              rowbytes = 0x0000ffff & (long)((*pixMap)->rowBytes ^ ROW_BYTES_MASK);

                rowbytes = (long)(*pixMap)->rowBytes;
                rowbytes &= 0x0000ffff;
                rowbytes |= 0x00008000;
                rowbytes ^= 0x00008000;

                destRowPlus = rowbytes - scaleCalc;
                shapeRowPlus = (long)sprite->width - (sourceRect.right - sourceRect.left);                                              //KLUDGE ALERT
                destByte = (*pixMap)->baseAddr + dRect->top * rowbytes + dRect->left;
                shapeByte = *(sprite->data) + sourceRect.top * sprite->width + sourceRect.left;

                vmap = gScaleVMap + mapRect.top;
                hmapoffset = gScaleHMap + mapRect.left;
                vend = destByte + rowbytes * (dRect->bottom - dRect->top);
                y = dRect->bottom - dRect->top;

                shapeRowPlus += *(hmapoffset + scaleCalc);
                mapWidth = (long)sprite->width;
                chunkByte = (*pixMap)->baseAddr + (long)((*pixMap)->bounds.bottom) * rowbytes;

                // for debugging
//              x = dRect->left;
//              y = dRect->top;

                sourceY = sourceRect.top;
                do
                {
                    sourceX = sourceRect.left;
                    hmap = hmapoffset;
                    hend = destByte + scaleCalc;

                    // for debugging
//                  x = dRect->left;
//                  TestByte( destByte);

                    do
                    {
                        if ( *shapeByte)
                        {
                            if ( PixelInSprite_IsOutside( sprite, sourceX, sourceY,
                                hmap, vmap))
                                *destByte = colorOut;// *shapeByte;
                            else
                                *destByte = colorIn;
                        }

//#ifdef kByteLevelTesting
//                      TestByte( (char *)destByte, *pixMap, "\pSMALLSP");
//#endif

//                      // debugging
                        sourceX += *hmap;
                        shapeByte += *hmap++;
                        destByte++;

//                      // debugging
//                      if ( x > clipRect->right)
//                      {
//                          WriteDebugLine( (char *)"\pX:");
//                          WriteDebugLong( hend - destByte);
//                      }
//                      x++;

                    } while ( destByte < hend);
                    destByte += destRowPlus;

                    // debugging
//                  y++;
//                  if ( y > clipRect->bottom)
//                  {
//                      WriteDebugLine( (char *)"\pY:");
//                      WriteDebugLong( y);
//                  }
                    sourceY += (*vmap);
                    shapeByte += (*vmap++ - 1) * mapWidth + shapeRowPlus;
                } while ( destByte < vend);
//              } while ( --y > 0);
            } else if ( scale <= MAX_SCALE)
            {
                h = mapWidth - 1;
                v = ( sprite->width - 1) << 1;
                d = v - h;
                h = v - ( h << 1);
                x = y = i = last = 0;
                hmap = gScaleHMap;
                vmap = hmapoffset = nil;
                while (( x < mapWidth - 1) || ( y < sprite->width - 1))
                {
                    x++;
                    i++;
                    if ( d > 0)
                    {
                        *hmap = i;

                        i = 0;
                        y++;
                        hmap++;
                        d += h;
                    } else d += v;
                }
                *hmap = i + 1;

                h = mapHeight - 1;
                v = ( sprite->height - 1) << 1;
                d = v - h;
                h = v - ( h << 1);
                x = y = i = last = 0;
                vmap = gScaleVMap;
                hmapoffset = hmap = nil;
                while (( x < mapHeight - 1) || ( y < sprite->height - 1))
                {
                    x++;
                    i++;
                    if ( d > 0)
                    {
                        *vmap = i;

                        i = 0;
                        y++;
                        vmap++;
                        d += h;
                    } else d += v;
                }
                *vmap = i + 1;

                if ( clipped)
                {
                    sourceRect.left = h = 0;
                    hmap = gScaleHMap;
                    while ( ( h + *hmap) < mapRect.left) { h += *hmap++; sourceRect.left++;}
                    x = *hmap;
                    *hmap -= mapRect.left - h;
                    h += x - *hmap;
                    sourceRect.right = sourceRect.left;
                    while ( ( h + *hmap) < mapRect.right) { h += *hmap++; sourceRect.right++;}
                    *hmap = mapRect.right - h;
                    sourceRect.right++;

                    sourceRect.top = h = 0;
                    vmap = gScaleVMap;
                    while ( ( h + *vmap) < mapRect.top) { h += *vmap++; sourceRect.top++;}
                    x = *vmap;
                    *vmap -= mapRect.top - h;
                    h += x - *vmap;
                    sourceRect.bottom = sourceRect.top;
                    while ( ( h + *vmap) < mapRect.bottom) { h += *vmap++; sourceRect.bottom++;}
                    *vmap = mapRect.bottom - h;
                    if ( sourceRect.bottom < sprite->height) sourceRect.bottom++;
                } // otherwise sourceRect is already set

                scaleCalc = (dRect->right - dRect->left);
                rowbytes = 0x0000ffff & (long)((*pixMap)->rowBytes ^ ROW_BYTES_MASK);
                destRowPlus = rowbytes - scaleCalc;
                destByte = (*pixMap)->baseAddr + dRect->top * rowbytes + dRect->left;
                shapeByte = *(sprite->data) + sourceRect.top * (long)sprite->width + sourceRect.left;

                vmap = gScaleVMap + sourceRect.top;
                hmapoffset = gScaleHMap + sourceRect.left;
                shapeRowPlus = sprite->width;
                mapWidth = (long)sprite->width;
                vend = *(sprite->data) + sourceRect.bottom * (long)sprite->width + sourceRect.left;
                lhend = gScaleHMap + sourceRect.right;

                while ( shapeByte < vend)
                {
                    v = *vmap;
                    while ( v > 0)
                    {
                        hmap = hmapoffset;
                        chunkByte = shapeByte;
                        while ( hmap < lhend)
                        {
                            if (( *chunkByte) && ( *hmap))
                            {
                                for ( h = *hmap; h > 0; h--)
                                    *destByte++ = *chunkByte;
                            } else destByte += *hmap;
                            hmap++;
                            chunkByte++;
                        }
                        destByte += destRowPlus;
                        v--;
                    }
                    vmap++;
                    shapeByte += shapeRowPlus;
                }

            } else dRect->left = dRect->right = dRect->top = dRect->bottom = 0;
        } else dRect->left = dRect->right = dRect->top = dRect->bottom = 0;
    } else dRect->left = dRect->right = dRect->top = dRect->bottom = 0;
}

Boolean PixelInSprite_IsOutside( spritePix *sprite, long x, long y,
    long *hmap, long *vmap)
{
    char    *pixel;
    long    rowPlus = sprite->width, i, j, *hmapStart = hmap;

    if ( x == 0) return true;
    if ( x >= ( sprite->width - 1)) return true;
    if ( y == 0) return true;
    if ( y >= ( sprite->height - 1)) return true;

    vmap--;
    hmapStart--;
    rowPlus -= ( *hmapStart + ( *(hmapStart + 1)) + ( *(hmapStart + 2)));
    pixel = *(sprite->data) + ((y - *vmap) * ((long)sprite->width)) +
                (x - *hmapStart);
    for ( j = y - 1; j <= ( y + 1); j++)
    {
        hmap = hmapStart;
        for ( i = x - 1; i <= ( x + 1); i++)
        {
            if ((( j != y) || ( x != i)) && ( !(*pixel))) return true;
            pixel += *hmap++;
        }
        pixel += ((*vmap++ - 1) * (long)sprite->width) + rowPlus;
    }
    return false;
}

void EraseSpriteTable( void)

{
    PixMapHandle    savePixBase, offPixBase;
    long                i;
    spriteType          *aSprite;

    savePixBase = GetGWorldPixMap( gSaveWorld);
    offPixBase = GetGWorldPixMap( gOffWorld);
    aSprite = (spriteType *)*gSpriteTable;
    for ( i = 0; i < kMaxSpriteNum; i++)
    {
        if ( aSprite->table != nil)
        {
        #ifndef kDrawOverride
            if ( aSprite->thisRect.left < aSprite->thisRect.right)
            {
//              ChunkCopyPixMapToPixMap( *savePixBase, &(aSprite->thisRect), *offPixBase);
                ChunkErasePixMap( *offPixBase, &(aSprite->thisRect));
            }
        #endif
            if ( aSprite->killMe)
                aSprite->lastRect = aSprite->thisRect;
        }
//      CopySaveWorldToOffWorld( &(gSprite[i].thisRect));
        aSprite++;
    }
}


void DrawSpriteTableInOffWorld( longRect *clipRect)

{
    PixMapHandle    pixMap;
    long            i, trueScale, layer, tinySize;
    longRect        sRect;
    spritePix       aSpritePix;
    char            *pixData;
    Handle          pixTable;
    int             whichShape;
    spriteType      *aSprite;

    pixMap = GetGWorldPixMap( gOffWorld);
    aSprite = (spriteType *)*gSpriteTable;

//  WriteDebugLong( gAbsoluteScale);
    if ( gAbsoluteScale >= kSpriteBlipThreshhold)
    {
        for ( layer = kFirstSpriteLayer; layer <= kLastSpriteLayer; layer++)
        {
            aSprite = (spriteType *)*gSpriteTable;
            for ( i = 0; i < kMaxSpriteNum; i++)
            {
                if (( aSprite->table != nil) && ( !aSprite->killMe) && ( aSprite->whichLayer == layer))
                {

                    pixTable = aSprite->table;
                    whichShape = aSprite->whichShape;
                    pixData = GetNatePixTableNatePixData( pixTable, aSprite->whichShape);

    //      if (( whichShape < 0) || ( whichShape >= GetNatePixTablePixNum( pixTable)))
    //          WriteDebugLong( (long)whichShape);

                    aSpritePix.data = &pixData;
                    aSpritePix.center.h = GetNatePixTableNatePixHRef( pixTable, whichShape);
                    aSpritePix.center.v = GetNatePixTableNatePixVRef( pixTable, whichShape);
                    aSpritePix.width = GetNatePixTableNatePixWidth( pixTable, whichShape);
                    aSpritePix.height = GetNatePixTableNatePixHeight( pixTable, whichShape);

                    trueScale = (long)aSprite->scale * gAbsoluteScale;
                    trueScale >>= (long)SHIFT_SCALE;

                #ifndef kDrawOverride
                    switch( aSprite->style)
                    {
                        case spriteNormal:
                            OptScaleSpritePixInPixMap( &aSpritePix, aSprite->where, trueScale,
                                &sRect, clipRect, pixMap);
                            break;

                        case spriteColor:
                            ColorScaleSpritePixInPixMap( &aSpritePix, aSprite->where, trueScale,
                                    &sRect, clipRect, pixMap, Randomize( kStaticTableSize),
                                    aSprite->styleColor, aSprite->styleData);
                            break;

                        case spriteStatic:
                            StaticScaleSpritePixInPixMap( &aSpritePix, aSprite->where, trueScale,
                                    &sRect, clipRect, pixMap, Randomize( kStaticTableSize));
                            break;
                    }
//                  sRect.top = sRect.left = sRect.bottom = sRect.right = 0;
                    mCopyAnyRect( aSprite->thisRect, sRect)
    //              LongRectToRect( &sRect, &(aSprite->thisRect));
                #endif
                }
                aSprite++;
            }
        }
    } else
    {
        for ( layer = kFirstSpriteLayer; layer <= kLastSpriteLayer; layer++)
        {
            aSprite = (spriteType *)*gSpriteTable;
            for ( i = 0; i < kMaxSpriteNum; i++)
            {
                tinySize = aSprite->tinySize & kBlipSizeMask;
                if (( aSprite->table != nil) && ( !aSprite->killMe) &&
                    ( aSprite->tinyColor != kNoTinyColor) &&
                    ( tinySize)
                    && ( aSprite->whichLayer == layer))
                {
/*                  sRect.left = aSprite->where.h - (aSprite->tinySize >> 1L);
                    sRect.right = sRect.left + aSprite->tinySize;
                    sRect.top = aSprite->where.v - (aSprite->tinySize >> 1L);
                    sRect.bottom = sRect.top + aSprite->tinySize;
*/
                    sRect.left = aSprite->where.h - tinySize;
                    sRect.right = aSprite->where.h + tinySize;
                    sRect.top = aSprite->where.v - tinySize;
                    sRect.bottom = aSprite->where.v + tinySize;
//                  WriteDebugLong( aSprite->tinySize);
                #ifndef kDrawOverride
                    switch( aSprite->tinySize & kBlipTypeMask)
                    {
                        case kTriangeUpBlip:
                            DrawNateTriangleUpClipped( *pixMap, &sRect, clipRect, 0, 0, aSprite->tinyColor);
                            break;

                        case kSolidSquareBlip:
                            DrawNateRectClipped( *pixMap, &sRect, clipRect, 0, 0, aSprite->tinyColor);
                            break;

                        case kPlusBlip:
                            DrawNatePlusClipped( *pixMap, &sRect, clipRect, 0, 0, aSprite->tinyColor);
                            break;

                        case kDiamondBlip:
                            DrawNateDiamondClipped( *pixMap, &sRect, clipRect, 0, 0, aSprite->tinyColor);
                            break;

                        case kFramedSquareBlip:
                            DrawNateRectClipped( *pixMap, &sRect, clipRect, 0, 0, aSprite->tinyColor);
                            break;

                        default:
                            sRect.top = sRect.left = sRect.bottom = sRect.right = 0;
//                          NumToString( aSprite->resID, hack);
//                          DebugStr( hack);
                            break;
                    }
//                  DrawNateRectClipped( *pixMap, &sRect, clipRect, 0, 0, aSprite->tinyColor);

                    mCopyAnyRect( aSprite->thisRect, sRect)
                #endif
                }

                aSprite++;
            }
        }
    }
}

void GetOldSpritePixData( spriteType *sourceSprite, spritePix *oldData)

{
    short               whichShape;
    char                *pixData;
    Handle              pixTable;

    if ( sourceSprite->table != nil)
    {
        pixTable = sourceSprite->table;
        whichShape = sourceSprite->whichShape;

//      WriteDebugLong( (long)pixTable);
        if ( whichShape >= GetNatePixTablePixNum( pixTable))
        {
            Str255  resIDString, shapeNumString;

            NumToString( sourceSprite->resID, resIDString);
            NumToString( whichShape, shapeNumString);

            ShowErrorAny( eQuitErr, kErrorStrID, nil, shapeNumString, nil, resIDString,
                SPRITE_DATA_ERROR, -1, 78, -1, __FILE__, sourceSprite->resID);
//          Debugger();
            return;
        }
        pixData = GetNatePixTableNatePixData( pixTable, sourceSprite->whichShape);
        oldData->data = &pixData;
        oldData->center.h = GetNatePixTableNatePixHRef( pixTable, whichShape);
        oldData->center.v = GetNatePixTableNatePixVRef( pixTable, whichShape);
        oldData->width = GetNatePixTableNatePixWidth( pixTable, whichShape);
        oldData->height = GetNatePixTableNatePixHeight( pixTable, whichShape);
    }
}

void ShowSpriteTable( void)

{
    Rect            tRect;
    PixMapHandle    pixMap;
    long            i;
    spriteType      *aSprite;

    aSprite = (spriteType *)*gSpriteTable;
    pixMap = GetGWorldPixMap( gOffWorld);
    for ( i = 0; i < kMaxSpriteNum; i++)
    {
        if ( aSprite->table != nil)
        {
            // if thisRect is null
            if (( aSprite->thisRect.right <= aSprite->thisRect.left) ||
                ( aSprite->thisRect.bottom <= aSprite->thisRect.top))
            {
                // and lastRect isn't then
                if ( aSprite->lastRect.right > aSprite->lastRect.left)
                {
                    // show lastRect

                    ChunkCopyPixMapToScreenPixMap( *pixMap, &(aSprite->lastRect),
                            *thePixMapHandle);


                }
            // else if lastRect is null (we now know this rect isn't)
            } else if (( aSprite->lastRect.right <= aSprite->lastRect.left) ||
                ( aSprite->lastRect.bottom <= aSprite->lastRect.top))
            {
                // then show thisRect

                ChunkCopyPixMapToScreenPixMap( *pixMap, &(aSprite->thisRect),
                        *thePixMapHandle);

            // else if the rects don't intersect
            } else if ( ( aSprite->lastRect.right < ( aSprite->thisRect.left - 32)) ||
                        ( aSprite->lastRect.left > ( aSprite->thisRect.right + 32)) ||
                        ( aSprite->lastRect.bottom < ( aSprite->thisRect.top - 32)) ||
                        ( aSprite->lastRect.top > ( aSprite->thisRect.bottom + 32)))
            {
                // then draw them individually


                ChunkCopyPixMapToScreenPixMap( *pixMap, &(aSprite->lastRect),
                        *thePixMapHandle);
                ChunkCopyPixMapToScreenPixMap( *pixMap, &(aSprite->thisRect),
                        *thePixMapHandle);

            // else the rects do intersect (and we know are both non-null)
            } else
            {
                tRect = aSprite->thisRect;
                mBiggestRect( tRect, aSprite->lastRect)

                ChunkCopyPixMapToScreenPixMap( *pixMap, &tRect, *thePixMapHandle);

            }
            aSprite->lastRect = aSprite->thisRect;
            if ( aSprite->killMe)
                RemoveSprite( aSprite);
        }
        aSprite++;
    }
}

/* CullSprites: if you're keeping track of sprites, but not showing them, use this to remove
dead sprites. (Implemented for Asteroid level, where game is run to populate scenario with Asteroids
before the player actually starts.
*/

void CullSprites( void)
{
    long            i;
    spriteType      *aSprite;

    aSprite = (spriteType *)*gSpriteTable;
    for ( i = 0; i < kMaxSpriteNum; i++)
    {
        if ( aSprite->table != nil)
        {
            aSprite->lastRect = aSprite->thisRect;
            if ( aSprite->killMe)
                RemoveSprite( aSprite);
        }
        aSprite++;
    }
}

/*
void asm PixMapTest( spritePix *sprite, Point where, long scale, longRect *dRect,
        longRect *clipRect, PixMapHandle pixMap)

{
        register longRect   *tRect;

        fralloc +

    test1:
        movea.l     dRect, tRect
        clr.l       struct(longRectStruct.left)(tRect)
        clr.l       struct(longRectStruct.top)(tRect)
        clr.l       struct(longRectStruct.right)(tRect)
        clr.l       struct(longRectStruct.bottom)(tRect)
    test2:
        frfree

    rts
}
*/
void TestByte( char *dbyte, PixMap *pixMap, StringPtr name)

{
    long            rowbytes, rowplus;
    char            *lbyte;

    rowbytes = 0x0000ffff & (long)((pixMap->rowBytes | ROW_BYTES_MASK) ^ ROW_BYTES_MASK);
    rowplus = (long)(pixMap->bounds.bottom - pixMap->bounds.top + 1) * rowbytes;
    lbyte = pixMap->baseAddr + rowplus;
    if (( dbyte < pixMap->baseAddr) || ( dbyte >= lbyte))
    {
        DebugStr( name);
        WriteDebugLine( (char *)name);
        WriteDebugLine((char *)"\p<<BAD>>");
    }
}

void ResolveScaleMapData( Handle scaleData)
{
#pragma unused( scaleData)
    gScaleHMap = (long *)*gBothScaleMaps;
    gScaleVMap = (long *)*gBothScaleMaps + (long)MAX_PIX_SIZE;
}

void ResolveSpriteData( Handle dummy)
{
    spriteType  *aSprite;
    short       i;

#pragma unused( dummy)
    mWriteDebugString("\pResolving Sprite");
    HLock( gSpriteTable);
    aSprite = ( spriteType *)*gSpriteTable;
    for ( i = 0; i < kMaxSpriteNum; i++)
    {
        if ( aSprite->resID != -1)
        {
            aSprite->table = GetPixTable( aSprite->resID);
            WriteDebugLong( aSprite->resID);
            if ( aSprite->table == nil)
            {
                aSprite->resID = -1;
                mWriteDebugString("\pNO TABLE");
            }
        }
        aSprite++;
    }

}