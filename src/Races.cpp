// Ares, a tactical space combat game.
// Copyright (C) 1997, 1999-2001, 2008 Nathan Lamont
// 
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

// Races.c

#include "Races.hpp"

#include "AresGlobalType.hpp"
#include "BinaryStream.hpp"
#include "Debug.hpp"
#include "Error.hpp"
#include "Resource.hpp"
#include "Resources.h"

#define kRaceError          "\pRACE"

short InitRaces( void)
{
    if (globals()->gRaceData.get() == nil)
    {
        globals()->gRaceData.load_resource('race', kRaceResID);
        if (globals()->gRaceData.get() == nil)
        {
            ShowErrorAny( eQuitErr, kErrorStrID, nil, nil, nil, nil, kReadRaceDataError, -1, -1, -1, __FILE__, 1);
            return( RESOURCE_ERROR);
        }
    }

    return( kNoError);
}

void CleanupRaces( void)
{
    if (globals()->gRaceData.get() != nil) {
        globals()->gRaceData.destroy();
    }
}

// GetNextLegalRace: Gets the next legal race *after* raceNum in the scenario it
// is passed. Pass -1 in raceNum to get the first legal race. Returns -1 if no legal
// races left.

short GetNextLegalRace( short raceNum, short playerNum, scenarioType *scenario)
{
    if ( scenario == nil) return -1;
    if ( playerNum >= scenario->playerNum) return raceNum;

    raceNum++;

    while (( raceNum < kRaceNum) &&
            ( !(scenario->player[playerNum].netRaceFlags & ( 0x0001 << raceNum))))
    {
        raceNum++;
    }
    if ( raceNum >= kRaceNum) return ( -1);
    else return( raceNum);
}

// GetPreviousLegalRace: gets the previous legal race *before* raceNum in the
// scenario it is passed. Pass kRaceNum in raceNum to get the first legal race.
// Returns -1 if no legal races left.

short GetPreviousLegalRace( short raceNum, short playerNum, scenarioType *scenario)
{
    if ( scenario == nil) return -1;
    if ( playerNum >= scenario->playerNum) return raceNum;

    raceNum--;

    while (( raceNum >= 0) &&
            ( !(scenario->player[playerNum].netRaceFlags & ( 0x0001 << raceNum))))
    {
        raceNum--;
    }
    if ( raceNum < 0) return ( -1);
    else return( raceNum);
}

bool IsRaceLegal( short raceNum, short playerNum, scenarioType *scenario)
{
    if ( playerNum >= scenario->playerNum) return false;

    if (scenario->player[playerNum].netRaceFlags & ( 0x0001 << raceNum)) return ( true);
    else return( false);
}

// GetRaceString: returns requested string for specified race. Use the following
// constants for whatString: kRaceAdjective, kRacePlural, kRaceForce, kRaceWorld.

void GetRaceString(unsigned char* string, short whatString, short raceNum) {
    GetIndString( string, kRaceStrID, (raceNum * kRaceStringNum) + 1 + whatString);
}

// GetRaceAdvantage: returns race estimated advantage.

smallFixedType GetRaceAdvantage( short raceNum)
{
    raceType    *race = *globals()->gRaceData + raceNum;

    if ( raceNum >= 0)
    {
        return race->advantage;
    } else return 0;
}

// GetRaceNumFromID: the RaceID is used, for stupid historic reasons. This will
// get the race number based on an ID. Returns -1 if no race found

short GetRaceNumFromID( short raceID)
{
    raceType    *race = *globals()->gRaceData;
    short       raceNum = 0;

    while ( (race->id != raceID) && ( raceNum < kRaceNum))
    {
        raceNum++;
        race++;
    }

    if ( raceNum >= kRaceNum) return( -1);
    else return( raceNum);
}

// GetRaceIDFromNum: see GetRaceNumFromID. This does the reverse.

short GetRaceIDFromNum( short raceNum)
{
    raceType    *race = *globals()->gRaceData + raceNum;

    if (( raceNum >= 0) && ( raceNum < kRaceNum))
    {
        return( race->id);
    } else
    {
        return( -1);
    }
}

unsigned char GetApparentColorFromRace( short raceNum)
{
    raceType    *race = *globals()->gRaceData + raceNum;


    if (( raceNum >= 0) && ( raceNum < kRaceNum))
    {
        return( race->apparentColor);
    } else
    {
        return( 0);
    }
}

size_t raceType::load_data(const char* data, size_t len) {
    BufferBinaryReader bin(data, len);

    bin.read(&id);
    bin.read(&apparentColor);
    bin.discard(1);
    bin.read(&illegalColors);
    bin.read(&advantage);

    return bin.bytes_read();
}
