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

// Ares Movie Player.c

#include "AresMoviePlayer.hpp"

#include "AresGlobalType.hpp"
#include "Debug.hpp"
#include "Error.hpp"
#include "FakeDrawing.hpp"
#include "KeyMapTranslation.hpp"
#include "Options.hpp"
#include "Randomize.hpp"

#define kSoundVolumeMultiplier  32  // = 256 (1.0) / 8 (kMaxSoundVolumePreference)

#define kUseMovies

void InitMoviePlayer( void)
{
#ifdef kUseMovies
    OSErr                       err;

    if ( globals()->gOptions & kOptionQuicktime)
    {
        err = EnterMovies();
    }
#endif
}

void CleanupMoviePlayer( void)
{
#ifdef kUseMovies
    if ( globals()->gOptions & kOptionQuicktime)
    {
        ExitMovies();
    }
#endif
}


void PlayMovieByName(const unsigned char* filePath, WindowPtr aWindow, bool doubleIt)

{
#ifdef kUseMovies
    OSErr                       err;
    FSSpec                      fileSpec;
    Movie                       aMovie = nil;
    short                       movieResFile;
    Rect                        movieBox;
    bool                     done = false;
    Fixed                       movieRate;
    TimeValue               movieTime;

#pragma unused ( device)
    if ( globals()->gOptions & kOptionQuicktime)
    {

        err = FSMakeFSSpec( 0, 0L, filePath, &fileSpec);
        if ( err == noErr)
        {
            err = OpenMovieFile ( &fileSpec, &movieResFile, fsRdPerm);
            if (err == noErr)
            {
                short               movieResID = 0;                     // want first movie
                Str255              movieName;
                bool             wasChanged;

                err = NewMovieFromFile (&aMovie, movieResFile, &movieResID, movieName,  newMovieActive,
                                        &wasChanged);
                CloseMovieFile (movieResFile);
            } else
            {
                WriteDebugLine("\pCouldn't Open Movie");
                WriteDebugLine(filePath);
            }
        } else WriteDebugLine("\pCouldn't Find Movie");
        if ( aMovie != nil)
        {
/*          if ( HasDepth( device, 16, 1, 1))
            {
                depthSet = true;
                SetDepth( device, 16, 1, 1);
            }
*/
            GetMovieBox (aMovie, &movieBox);

    /*
            // make a new window centered in the device of choice
            windowRect = movieBox;
            OffsetRect (&windowRect,
                (((*device)->gdRect.right - (*device)->gdRect.left) / 2) -
                ((windowRect.right - windowRect.left) / 2),
                (((*device)->gdRect.bottom - (*device)->gdRect.top) / 2) -
                ((windowRect.bottom - windowRect.top) / 2));
            movieWindow = (CWindowPtr)NewCWindow (nil, &windowRect, "\p", false, plainDBox,
                            (WindowPtr)-1, false, 703);
            ShowWindow( (WindowPtr)movieWindow);
    */

            // if we have a custom color table for this movie
            ClearScreen();

            if ( (( movieBox.right - movieBox.left) <= (( aWindow->portRect.right -
                aWindow->portRect.left) / 2)) && (( movieBox.bottom - movieBox.top) <= (( aWindow->portRect.bottom -
                aWindow->portRect.top) / 2)) && ( doubleIt))
            {
                movieBox.right *= 2;
                movieBox.bottom *= 2;
            }
            movieBox.offset(
                ((aWindow->portRect.right - aWindow->portRect.left) / 2) -
                ((movieBox.right - movieBox.left) / 2),
                ((aWindow->portRect.bottom - aWindow->portRect.top) / 2) -
                ((movieBox.bottom - movieBox.top) / 2));
            SetMovieBox( aMovie, &movieBox);
            SetMovieGWorld( aMovie, aWindow, nil);
            SetMovieVolume( aMovie, kSoundVolumeMultiplier * globals()->gSoundVolume);

            HideCursor();
            movieRate = GetMovieRate( aMovie);
            movieTime = 0;//GetMovieDuration( aMovie);
            if ( PrerollMovie ( aMovie, movieTime, movieRate) != noErr) SysBeep( 20);
            StartMovie (aMovie);
            while ( !IsMovieDone(aMovie) && !done )
            {
                MoviesTask (aMovie, DoTheRightThing);
                if ( !ShiftKey())
                    done = AnyEvent();
            }
            MacShowCursor();
            DisposeMovie (aMovie);
            ClearScreen();

    //      DisposeWindow( (WindowPtr)movieWindow);

/*          if ( depthSet)
            {
                SetDepth( device, 8, 1, 1);
            }
*/      }
    }
#endif
}

// the "mini movie" routines are for playing a movie within a rect without disturbing the rest of the display.
// The PlayMovieByName is for playing a movie in one step, automatically clearing the diplay and all.

OSErr LoadMiniMovie(unsigned char* filePath, Movie *aMovie, Rect *destRect, WindowPtr aWindow, bool doubleIt)

{
#ifdef kUseMovies
    OSErr                       err;
    FSSpec                      fileSpec;
    short                       movieResFile, offH, offV;
    Rect                        movieBox;
    Fixed                       movieRate;
    TimeValue                   movieTime;

    if ( globals()->gOptions & kOptionQuicktime)
    {

        err = FSMakeFSSpec( 0, 0L, filePath, &fileSpec);
        if ( err == noErr)
        {
            err = OpenMovieFile ( &fileSpec, &movieResFile, fsRdPerm);
            if (err == noErr)
            {
                short               movieResID = 0;                     // want first movie
                Str255              movieName;
                bool             wasChanged;

                err = NewMovieFromFile ( aMovie, movieResFile, &movieResID, movieName,  newMovieActive,
                                        &wasChanged);
                CloseMovieFile (movieResFile);
            } else
            {
                WriteDebugLine("\pCouldn't Open Movie");
                WriteDebugLine(filePath);
                return( err);
            }
        } else
        {
            WriteDebugLine("\pCouldn't Find Movie");
            return( err);
        }
        if ( *aMovie != nil)
        {
            MacSetPort( aWindow);

            GetMovieBox (*aMovie, &movieBox);

            if ( (( movieBox.right - movieBox.left) <= (( destRect->right -
                destRect->left) / 2)) && (( movieBox.bottom - movieBox.top) <= (( destRect->bottom -
                destRect->top) / 2)) && ( doubleIt))
            {
                movieBox.right *= 2;
                movieBox.bottom *= 2;
            }
            offH = ((destRect->right - destRect->left) / 2) -
                ((movieBox.right - movieBox.left) / 2) + destRect->left;
            offV = ((destRect->bottom - destRect->top) / 2) -
                ((movieBox.bottom - movieBox.top) / 2) + destRect->top;

            movieBox.offset(offH, offV);

            ClearScreen();

            SetMovieGWorld (*aMovie, aWindow, nil);
            SetMovieVolume( *aMovie, kSoundVolumeMultiplier * globals()->gSoundVolume);
            SetMovieBox (*aMovie, &movieBox);

            movieRate = GetMovieRate( *aMovie);
            movieTime = 0;//GetMovieDuration( *aMovie);

            err = PrerollMovie ( *aMovie, movieTime, movieRate);
            if ( err != noErr) return( err);
        } else
        {
            WriteDebugLine("\pMovie = nil");
            return ( -1);
        }
    } else
    {
        *aMovie = nil;
    }
#endif

    return( noErr);
}

OSErr StartMiniMovie( Movie aMovie)

{
#ifdef kUseMovies
    OSErr err;

    if ( aMovie != nil) StartMovie( aMovie);
    err = GetMoviesError();
//  if ( err != noErr) Debugger();

    return( err);
#else
    return( noErr);
#endif
}

// returns true if movie is done
bool DoMiniMovieTask( Movie aMovie)

{
#ifdef kUseMovies
    OSErr   err;
    bool done = false;

    if ( aMovie == nil) return( true);
    else
    {
        done = IsMovieDone(aMovie);
        err = GetMoviesError();
        if ( err != noErr) return true;//Debugger();
        if ( done) return( true);
        else
        {
            MoviesTask( aMovie, DoTheRightThing);
            err = GetMoviesError();
//          if ( err != noErr) Debugger();
            return( false);
        }
    }
#endif
    return( true);
}

OSErr CleanUpMiniMovie( Movie *aMovie)

{
#ifdef kUseMovies
    OSErr   err;
    Rect    movieBox;

    if ( *aMovie != nil)
    {
        GetMovieBox( *aMovie, &movieBox);
        err = GetMoviesError();
        if ( err != noErr) return err;//Debugger();
        ClearScreen();
        StopMovie (*aMovie);
        err = GetMoviesError();
        if ( err != noErr) return err;//Debugger();
        DisposeMovie( *aMovie);
        err = GetMoviesError();
        if ( err != noErr) return err;//Debugger();
        *aMovie = nil;
    }
#endif
    return ( noErr);
}
