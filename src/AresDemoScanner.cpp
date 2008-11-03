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

/* This is a hack that runs *WITHIN* a development version of Ares, for making DEMO levels.

The object is to remove all data not relevent to demos to discourage hacking.

Running it should scan through the hand-coded levels, mark the data it wants to keep, wipe out
the data it doesn't (where applicable) and then copy it all over to the prefs file. Developer
(me) then takes all that data from the prefs file and copies into new data files.

MAKE SURE YOU BACK UP **ALL** DATA FILES 1ST, JUST IN CASE.

other things you have to change:
remember that the chapters start at 2, and that the apparent chapter # is -1
scn 0 = dch 2/rch 3 -> cond 000 Counter -> action 279 declare winner 0 next:3 <-> next:4
scn 5 = dch 3/rch 4 -> cond 057 Owner -> action 289 declare winner 0 next:4 <-> next:5
scn12 = dch 4/rch13 -> cond 63 Countr >= -> action 458 declare winner 0 next:-1 <-> next:14
*/

#include "AresDemoScanner.h"

#include <QDOffscreen.h>

#include "AresGlobalType.h"
#include "AresPreferences.h"
#include "ConditionalMacros.h"
#include "Debug.h"
#include "Error.h"
#include "OffscreenGWorld.h"
#include "PlayerInterface.h"
#include "Resources.h"
#include "SpaceObjectHandling.h"
#include "ScenarioMaker.h"

extern aresGlobalType *gAresGlobal;

extern scenarioType                 *gThisScenario;
//extern long                           gAresGlobal->gThisScenarioNumber;
extern Handle                       /*gAresGlobal->gScenarioData,*/
                                    gObjectActionData,
                                    //gAresGlobal->gScenarioInitialData,
                                    //gAresGlobal->gScenarioBriefData,
                                    //gAresGlobal->gScenarioConditionData,
                                    gSpaceObjectData, gBaseObjectData;
extern pixTableType                 gPixTable[];
//extern smartSoundHandle               gAresGlobal->gSound[];
extern GWorldPtr                    gOffWorld;

/* MAKE SURE YOU COMMENT OUT THE CONTENTS OF THE CorrectAllBaseObjectColor ROUTINE
*/

void MakeDemoDataHack( void)

{
    Boolean *baseObjectKeepList = (Boolean *)NewPtr( sizeof( baseObjectType) * (long)kMaxBaseObject),
            *boolPtr = nil;
    long    count = 0, c2;
    scenarioType    *aScenario;

    boolPtr = baseObjectKeepList;
    for ( count = 0; count < kMaxBaseObject; count++)
    {
        *boolPtr = false;
        boolPtr++;
    }

    SetAllSoundsNoKeep();
    SetAllPixTablesNoKeep();
    RemoveAllUnusedSounds();
    RemoveAllUnusedPixTables();

    ScanLevel( 0, baseObjectKeepList);
    CopyAllBriefingData( 0); // = 1; really, 13 = 1

    ScanLevel( 5, baseObjectKeepList);
    CopyAllBriefingData( 5); // = 2; really, 11 = 2

    ScanLevel( 12, baseObjectKeepList);
    CopyAllBriefingData( 12); // = 3; really, 0 = 3

    ScanLevel( 21, baseObjectKeepList);
    CopyAllBriefingData( 21);

    ScanLevel( 23, baseObjectKeepList); // can't be played
    CopyAllBriefingData( 23);

    ScanLevel( 13, baseObjectKeepList); // can't be played
    CopyAllBriefingData( 13);

    ClearAndCopyAllUnusedBaseObjects( baseObjectKeepList);
    CopyAllUsedPixTables();
    CopyAllUsedSounds();

    DisposePtr( (Ptr)baseObjectKeepList);

    aScenario = (scenarioType *)*gAresGlobal->gScenarioData;
    for ( count = 0; count < GetScenarioNumber(); count++)
    {
        if ( ( count !=
                            0
            ) && ( count !=
                            5
            ) && ( count !=
                            12
            ) && ( count !=
                            21
            ) && ( count !=
                            23
            ) && ( count !=
                            13
            ))
        {
            aScenario->netRaceFlags = 0;
            aScenario->playerNum = 0;
            aScenario->initialFirst = -1;
            aScenario->prologueID = -1;
            aScenario->initialNum = -1;
            aScenario->songID = -1;
            aScenario->conditionFirst = -1;
            aScenario->epilogueID = -1;
            aScenario->conditionNum = -1;
            aScenario->starMapH = -1;
            aScenario->briefPointFirst = -1;
            aScenario->starMapV = -1;
            aScenario->briefPointNum = -1;
            aScenario->parTime = -1;
            aScenario->movieNameStrNum = -1;
            aScenario->parKills = -1;
            aScenario->levelNameStrNum = -1;
            aScenario->parKillRatio = -1;
            aScenario->parLosses = -1;
            aScenario->startTime = 0;
            for ( c2 = 0; c2 < kScenarioPlayerNum; c2++)
            {
                aScenario->player[c2].playerType = -1;
                aScenario->player[c2].playerRace = -1;
                aScenario->player[c2].nameResID = -1;
                aScenario->player[c2].nameStrNum = -1;
                aScenario->player[c2].admiralNumber = -1;
                aScenario->player[c2].earningPower = -1;
                aScenario->player[c2].reserved1 = -1;
            }
        }
        aScenario++;
    }
    SaveAnyResourceInPreferences( 'snro', 500, nil, gAresGlobal->gScenarioData, true);
}

void ScanLevel( long whatLevel, Boolean *baseObjectKeepList)

{
    scenarioType            *scenario = (scenarioType *)*gAresGlobal->gScenarioData + whatLevel;
    long                    count, c2, c3, newShipNum;
    baseObjectType          *baseObject;
    scenarioConditionType   *condition;
    scenarioInitialType     *initial;
    objectActionType        *action;

    SetAllBaseObjectsUnchecked();

    gAresGlobal->gThisScenarioNumber = whatLevel;
    gThisScenario = (scenarioType *)*gAresGlobal->gScenarioData + whatLevel;
    ///// FIRST SELECT WHAT MEDIA WE NEED TO USE:
    // uncheck all base objects
    // uncheck all sounds

    WriteDebugLine((char *)"\p- C H E C K -");

    // for each initial object

    baseObject = mGetBaseObjectPtr( gAresGlobal->scenarioFileInfo.energyBlobID);
    if ( baseObject != nil)
        CheckBaseObjectMedia( baseObject, 0);
    baseObject = mGetBaseObjectPtr( gAresGlobal->scenarioFileInfo.warpInFlareID);
    if ( baseObject != nil)
        CheckBaseObjectMedia( baseObject, 0);
    baseObject = mGetBaseObjectPtr( gAresGlobal->scenarioFileInfo.warpOutFlareID);
    if ( baseObject != nil)
        CheckBaseObjectMedia( baseObject, 0);
    baseObject = mGetBaseObjectPtr( gAresGlobal->scenarioFileInfo.playerBodyID);
    if ( baseObject != nil)
        CheckBaseObjectMedia( baseObject, 0);

    initial = mGetScenarioInitial( gThisScenario, 0);
    for ( count = 0; count < gThisScenario->initialNum; count++)
    {

        // get the base object equiv
        baseObject = mGetBaseObjectPtr( initial->type);
        // check the media for this object

        CheckBaseObjectMedia( baseObject, 0);

        // check any objects this object can build
        for ( c2 = 0; c2 < kMaxTypeBaseCanBuild; c2++)
        {
            if ( initial->canBuild[c2] != kNoClass)
            {
                // check for each player
                for ( c3 = 0; c3 < gThisScenario->playerNum; c3++)
                {
                    mGetBaseObjectFromClassRace( baseObject, newShipNum, initial->canBuild[c2], gThisScenario->player[c3].playerRace)

                    if ( baseObject != nil)
                        CheckBaseObjectMedia( baseObject, 0);
                }
            }
        }
        initial++;
    }

    // check media for all condition actions
    condition = mGetScenarioCondition( gThisScenario, 0);
    for ( count = 0; count < gThisScenario->conditionNum; count++)
    {
        CheckActionMedia( condition->startVerb, condition->verbNum, 0);
        condition = mGetScenarioCondition( gThisScenario, count);
    }

    baseObject = (baseObjectType *)*gBaseObjectData;

    for ( count = 0; count < kMaxBaseObject; count++)
    {
        if ( baseObject->internalFlags & 0x0000ffff)
        {
            *baseObjectKeepList = true;
        }
        baseObjectKeepList++;
        baseObject++;
    }

    SetAllBaseObjectsUnchecked();

    // **************************
    // *** add media          ***
    // **************************

    baseObject = mGetBaseObjectPtr( gAresGlobal->scenarioFileInfo.energyBlobID);
    if ( baseObject != nil)
    {
        AddBaseObjectMedia( gAresGlobal->scenarioFileInfo.energyBlobID, 0);
    }
    baseObject = mGetBaseObjectPtr( gAresGlobal->scenarioFileInfo.warpInFlareID);
    if ( baseObject != nil)
    {
        AddBaseObjectMedia( gAresGlobal->scenarioFileInfo.warpInFlareID, 0);
    }
    baseObject = mGetBaseObjectPtr( gAresGlobal->scenarioFileInfo.warpOutFlareID);
    if ( baseObject != nil)
    {
        AddBaseObjectMedia( gAresGlobal->scenarioFileInfo.warpOutFlareID, 0);
    }
    baseObject = mGetBaseObjectPtr( gAresGlobal->scenarioFileInfo.playerBodyID);
    if ( baseObject != nil)
    {
        AddBaseObjectMedia( gAresGlobal->scenarioFileInfo.playerBodyID, 0);
    }

    for ( count = 0; count < gThisScenario->initialNum; count++)
    {
        initial = mGetScenarioInitial( gThisScenario, count);

        // get the base object equiv
        baseObject = mGetBaseObjectPtr( initial->type);
        // check the media for this object
        AddBaseObjectMedia( initial->type, 0);

        // we may have just moved memory, so let's make sure our ptrs are correct
        initial = mGetScenarioInitial( gThisScenario, count);

        // make sure we're not overriding the sprite
        if ( initial->spriteIDOverride >= 0)
        {
            AddPixTable( initial->spriteIDOverride);
        }

        // check any objects this object can build
        for ( c2 = 0; c2 < kMaxTypeBaseCanBuild; c2++)
        {
            initial = mGetScenarioInitial( gThisScenario, count);
            if ( initial->canBuild[c2] != kNoClass)
            {
                // check for each player
                for ( c3 = 0; c3 < gThisScenario->playerNum; c3++)
                {
                    initial = mGetScenarioInitial( gThisScenario, count);
                    baseObject = mGetBaseObjectPtr( initial->type);
                    mGetBaseObjectFromClassRace( baseObject, newShipNum, initial->canBuild[c2], gThisScenario->player[c3].playerRace)
                    if ( baseObject != nil)
                    {
                        AddBaseObjectMedia( newShipNum, 0);
                    }
                }
            }
        }
    }

    // add media for all condition actions
    condition = mGetScenarioCondition( gThisScenario, 0);
    for ( count = 0; count < gThisScenario->conditionNum; count++)
    {
        condition = mGetScenarioCondition( gThisScenario, count);
        action = (objectActionType *)*gObjectActionData + condition->startVerb;
        for ( c2 = 0; c2 < condition->verbNum; c2++)
        {
            condition = mGetScenarioCondition( gThisScenario, count);
            action = (objectActionType *)*gObjectActionData + condition->startVerb + c2;
            AddActionMedia( action, 0);
        }
    }
    SetAllBaseObjectsUnchecked();
}

/* Clear all unused baseobjects, and copy changed data
*/

void ClearAndCopyAllUnusedBaseObjects( Boolean *baseObjectKeepList)

{
    long            count;
    baseObjectType  *anObject;
    unsigned char   *nilObject = (unsigned char *)NewPtr( sizeof( baseObjectType)), *c = nil;

    c = nilObject;
    for ( count = 0; count < sizeof( baseObjectType); count++)
    {
        *c = 0;
        c++;
    }

    for ( count = 0; count < kMaxBaseObject; count++)
    {
        anObject = mGetBaseObjectPtr( count);
        if ( *baseObjectKeepList != true)
        {
            BlockMove( (Ptr)nilObject, (Ptr)anObject, sizeof( baseObjectType));
        }
        baseObjectKeepList++;
    }

    DisposePtr( (Ptr)nilObject);

    SaveAnyResourceInPreferences( kBaseObjectResType, kBaseObjectResID, nil, gBaseObjectData, true);
}

/* Copy all loaded pix tables
*/

void CopyAllUsedPixTables( void)

{
    long        count;

    for ( count = 0; count < kMaxPixTableEntry; count++)
    {
        if ( gPixTable[count].resource != nil)
        {
            WriteDebugLine((char *)"\pSavePIX:");
            WriteDebugLong( gPixTable[count].resID);
            SaveAnyResourceInPreferences( kPixResType, gPixTable[count].resID, nil,
                gPixTable[count].resource, true);
        }
    }
}

void CopyAllUsedSounds( void)

{
    long    count;

    for ( count = 0; count < kSoundNum; count++)
    {
        if ( gAresGlobal->gSound[count].soundHandle != nil)
        {
            WriteDebugLine((char *)"\pSaveSND:");
            WriteDebugLong( gAresGlobal->gSound[count].id);
            SaveAnyResourceInPreferences( 'snd ', gAresGlobal->gSound[count].id, nil,
                gAresGlobal->gSound[count].soundHandle, true);
        }
    }
}

void CopyAllBriefingData( long whatLevel)
{
    scenarioType            *scenario = (scenarioType *)*gAresGlobal->gScenarioData + whatLevel;
    Handle                  textData = nil;
    PixMapHandle            offMap = GetGWorldPixMap( gOffWorld);
    Rect                    tRect = {0, 0, 480, 480};
    inlinePictType          inlinePictList[kMaxInlinePictNum];
    long                    whichBriefNum, length;
    briefPointType          *brief = nil;

    for ( whichBriefNum = 0; whichBriefNum < GetBriefPointNumber( whatLevel); whichBriefNum++)
    {
        brief = mGetScenarioBrief( scenario, whichBriefNum);
        for ( length = 0; length < kMaxInlinePictNum; length++)
        {
            inlinePictList[length].id = -1;
        }

        textData = GetResource( 'TEXT', brief->contentResID);
        if ( textData != nil)
        {
            DetachResource( textData);

            SaveAnyResourceInPreferences( 'TEXT', brief->contentResID, nil,
                textData, true);

            HLockHi( textData);

            length = GetHandleSize( textData);

            DrawInterfaceTextInRect( &tRect, (anyCharType *)*textData, length,
                            kLarge, 3, *offMap, 0, 0, inlinePictList);

            scenario = (scenarioType *)*gAresGlobal->gScenarioData + whatLevel;
            brief = mGetScenarioBrief( scenario, whichBriefNum);
            HUnlock( textData);
            DisposeHandle( textData);

            for ( length = 0; length < kMaxInlinePictNum; length++)
            {
                if ( inlinePictList[length].id > 0)
                {
                    textData = GetResource( 'PICT', inlinePictList[length].id);
                    if ( textData != nil)
                    {
                        DetachResource( textData);

                        SaveAnyResourceInPreferences( 'PICT', inlinePictList[length].id, nil,
                            textData, true);

                        DisposeHandle( textData);
                    }
                }
            }
        }
    }
}