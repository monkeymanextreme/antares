// Copyright (C) 1997, 1999-2001, 2008 Nathan Lamont
// Copyright (C) 2008-2017 The Antares Authors
//
// This file is part of Antares, a tactical space combat game.
//
// Antares is free software: you can redistribute it and/or modify it
// under the terms of the Lesser GNU General Public License as published
// by the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Antares is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with Antares.  If not, see http://www.gnu.org/licenses/

#include "game/level.hpp"

#include <set>
#include <sfz/sfz.hpp>

#include "data/plugin.hpp"
#include "data/races.hpp"
#include "drawing/pix-table.hpp"
#include "game/action.hpp"
#include "game/admiral.hpp"
#include "game/condition.hpp"
#include "game/globals.hpp"
#include "game/initial.hpp"
#include "game/instruments.hpp"
#include "game/labels.hpp"
#include "game/messages.hpp"
#include "game/minicomputer.hpp"
#include "game/motion.hpp"
#include "game/non-player-ship.hpp"
#include "game/player-ship.hpp"
#include "game/starfield.hpp"
#include "game/sys.hpp"
#include "game/vector.hpp"
#include "lang/defines.hpp"
#include "math/macros.hpp"
#include "math/random.hpp"
#include "math/rotation.hpp"
#include "math/units.hpp"

using sfz::range;
using std::set;

namespace antares {

namespace {

#ifdef DATA_COVERAGE
ANTARES_GLOBAL set<int32_t> possible_objects;
ANTARES_GLOBAL set<int32_t> possible_actions;
#endif  // DATA_COVERAGE

void AddBaseObjectActionMedia(
        const std::vector<std::unique_ptr<const Action>>& actions, std::bitset<16> all_colors);
void AddActionMedia(const Action& action, std::bitset<16> all_colors);

void AddBaseObjectMedia(Handle<const BaseObject> base, std::bitset<16> all_colors) {
    if (base.get()) {
        return;
    }
    load_object(base);

#ifdef DATA_COVERAGE
    possible_objects.insert(base.number());
#endif  // DATA_COVERAGE

    // Load sprites in all possible colors.
    //
    // For non-thinking objects, just load the gray ones. For thinking
    // objects, load gray, plus all admiral colors.
    //
    // This actually loads more colors than are possible in practice;
    // there was older code that determined which objects were possible
    // per admiral, and which could change ownership, but that wasn’t
    // worth the extra complication.
    std::bitset<16> colors;
    if (base->attributes & kCanThink) {
        colors = all_colors;
    } else {
        colors[0] = true;
    }
    for (int i = 0; i < 16; ++i) {
        if (colors[i] && (sprite_resource(*base) != kNoSpriteTable)) {
            sys.pix.add(sprite_resource(*base) + (i << kSpriteTableColorShift));
        }
    }

    AddBaseObjectActionMedia(base->destroy, all_colors);
    AddBaseObjectActionMedia(base->expire, all_colors);
    AddBaseObjectActionMedia(base->create, all_colors);
    AddBaseObjectActionMedia(base->collide, all_colors);
    AddBaseObjectActionMedia(base->activate, all_colors);
    AddBaseObjectActionMedia(base->arrive, all_colors);

    for (Handle<const BaseObject> weapon :
         {base->pulse.base, base->beam.base, base->special.base}) {
        if (weapon.number() >= 0) {
            AddBaseObjectMedia(weapon, all_colors);
        }
    }
}

void AddBaseObjectActionMedia(
        const std::vector<std::unique_ptr<const Action>>& actions, std::bitset<16> all_colors) {
    for (const auto& action : actions) {
        AddActionMedia(*action, all_colors);
    }
}

void AddActionMedia(const Action& action, std::bitset<16> all_colors) {
#ifdef DATA_COVERAGE
    possible_actions.insert(action.number());
#endif  // DATA_COVERAGE

    auto base = action.created_base();
    if (base.number() >= 0) {
        AddBaseObjectMedia(action.created_base(), all_colors);
    }

    auto range = action.sound_range();
    for (int32_t count = range.begin; count < range.end; count++) {
        sys.sound.load(count);
    }
}

static coordPointType rotate_coords(int32_t h, int32_t v, int32_t rotation) {
    mAddAngle(rotation, 90);
    Fixed lcos, lsin;
    GetRotPoint(&lcos, &lsin, rotation);
    coordPointType coord;
    coord.h =
            (kUniversalCenter + (Fixed::from_val(h) * -lcos).val() -
             (Fixed::from_val(v) * -lsin).val());
    coord.v =
            (kUniversalCenter + (Fixed::from_val(h) * -lsin).val() +
             (Fixed::from_val(v) * -lcos).val());
    return coord;
}

void GetInitialCoord(
        Handle<const Level::Initial> initial, coordPointType* coord, int32_t rotation) {
    *coord = rotate_coords(initial->at.h, initial->at.v, rotation);
}

}  // namespace

Point Level::star_map_point() const { return starMap; }

int32_t Level::chapter_number() const { return chapter; }

LoadState start_construct_level(Handle<const Level> level) {
    ResetAllSpaceObjects();
    reset_action_queue();
    Vectors::reset();
    ResetAllSprites();
    Label::reset();
    ResetInstruments();
    Admiral::reset();
    ResetAllDestObjectData();
    ResetMotionGlobals();
    plug.races.clear();
    plug.objects.clear();
    gAbsoluteScale = kTimesTwoScale;
    g.sync         = 0;

    g.level = level;

    {
        int32_t angle = g.level->angle;
        if (angle < 0) {
            g.angle = g.random.next(ROT_POS);
        } else {
            g.angle = angle;
        }
    }

    g.victor       = Admiral::none();
    g.next_level   = Level::none();
    g.victory_text = "";

    SetMiniScreenStatusStrList(g.level->score_strings);

    int i = 0;
    for (const auto& player : g.level->players) {
        if (player.playerType == PlayerType::HUMAN) {
            auto admiral = Admiral::make(i++, kAIsHuman, player);
            admiral->pay(Fixed::from_long(5000));
            g.admiral = admiral;
        } else {
            auto admiral = Admiral::make(i++, kAIsComputer, player);
            admiral->pay(Fixed::from_long(5000));
        }
        load_race(player.playerRace);
    }

    // *** END INIT ADMIRALS ***

    g.initials.clear();
    g.initials.resize(Level::Initial::all().size());
    g.initial_ids.clear();
    g.initial_ids.resize(Level::Initial::all().size());
    g.condition_enabled.clear();
    g.condition_enabled.resize(g.level->conditions.size());

    ///// FIRST SELECT WHAT MEDIA WE NEED TO USE:

    sys.pix.reset();
    sys.sound.reset();

    LoadState s;
    s.max = Level::Initial::all().size() * 3L + 1 +
            g.level->startTime.count();  // for each run through the initial num

    return s;
}

static void load_blessed_objects(std::bitset<16> all_colors) {
    if (plug.info.energyBlobID.number() < 0) {
        throw std::runtime_error("No energy blob defined");
    }
    if (plug.info.warpInFlareID.number() < 0) {
        throw std::runtime_error("No warp in flare defined");
    }
    if (plug.info.warpOutFlareID.number() < 0) {
        throw std::runtime_error("No warp out flare defined");
    }
    if (plug.info.playerBodyID.number() < 0) {
        throw std::runtime_error("No player body defined");
    }

    // Load the four blessed objects.  The player's body is needed
    // in all colors; the other three are needed only as neutral
    // objects by default.
    const auto&              info      = plug.info;
    Handle<const BaseObject> blessed[] = {
            info.energyBlobID, info.warpInFlareID, info.warpOutFlareID, info.playerBodyID,
    };
    for (auto id : blessed) {
        AddBaseObjectMedia(id, all_colors);
    }
}

static void load_initial(Handle<const Level::Initial> initial, std::bitset<16> all_colors) {
    Handle<Admiral> owner      = initial->owner;
    auto            baseObject = initial->base;
    // TODO(sfiera): remap objects in networked games.

    // Load the media for this object
    AddBaseObjectMedia(baseObject, all_colors);

    // make sure we're not overriding the sprite
    if (initial->sprite_override >= 0) {
        if (baseObject->attributes & kCanThink) {
            sys.pix.add(
                    initial->sprite_override +
                    (static_cast<int>(GetAdmiralColor(owner)) << kSpriteTableColorShift));
        } else {
            sys.pix.add(initial->sprite_override);
        }
    }

    // check any objects this object can build
    for (int j = 0; j < initial->build.size(); j++) {
        // check for each player
        for (auto a : Admiral::all()) {
            if (a->active()) {
                auto baseObject =
                        get_base_object_handle_from_class_and_race(initial->build[j], a->race());
                if (baseObject.number() >= 0) {
                    AddBaseObjectMedia(baseObject, all_colors);
                }
            }
        }
    }
}

static void load_condition(Handle<const Level::Condition> condition, std::bitset<16> all_colors) {
    for (const auto& action : condition->action) {
        AddActionMedia(*action, all_colors);
    }
    g.condition_enabled[condition.number()] = condition->initially_enabled;
}

static void run_game_1s() {
    game_ticks start_time = game_ticks(-g.level->startTime);
    do {
        g.time += kMajorTick;
        MoveSpaceObjects(kMajorTick);
        NonplayerShipThink();
        AdmiralThink();
        execute_action_queue();
        CollideSpaceObjects();
        if (((g.time - start_time) % kConditionTick) == ticks(0)) {
            CheckLevelConditions();
        }
        CullSprites();
        Vectors::cull();
    } while ((g.time.time_since_epoch() % secs(1)) != ticks(0));
}

void construct_level(Handle<const Level> level, LoadState* state) {
    int32_t         step = state->step;
    std::bitset<16> all_colors;
    all_colors[0] = true;
    for (auto adm : Admiral::all()) {
        if (adm->active()) {
            all_colors[static_cast<int>(GetAdmiralColor(adm))] = true;
        }
    }

    if (step == 0) {
        load_blessed_objects(all_colors);
        load_initial(Handle<const Level::Initial>(step), all_colors);
    } else if (step < Level::Initial::all().size()) {
        load_initial(Handle<const Level::Initial>(step), all_colors);
    } else if (step == Level::Initial::all().size()) {
        // add media for all condition actions
        step -= Level::Initial::all().size();
        for (auto c : Level::Condition::all()) {
            load_condition(c, all_colors);
        }
        create_initial(Handle<const Level::Initial>(step));
    } else if (step < (2 * Level::Initial::all().size())) {
        step -= Level::Initial::all().size();
        create_initial(Handle<const Level::Initial>(step));
    } else if (step < (3 * Level::Initial::all().size())) {
        // double back and set up any defined initial destinations
        step -= (2 * Level::Initial::all().size());
        set_initial_destination(Handle<const Level::Initial>(step), false);
    } else if (step == (3 * Level::Initial::all().size())) {
        RecalcAllAdmiralBuildData();  // set up all the admiral's destination objects
        Messages::clear();
        g.time = game_ticks(-g.level->startTime);
    } else {
        run_game_1s();
    }
    ++state->step;
    if (state->step == state->max) {
        state->done = true;
    }
    return;
}

void DeclareWinner(
        Handle<Admiral> whichPlayer, Handle<const Level> nextLevel, pn::string_view text) {
    if (!whichPlayer.get()) {
        // if there's no winner, we want to exit immediately
        g.next_level   = nextLevel;
        g.victory_text = text.copy();
        g.game_over    = true;
        g.game_over_at = g.time;
    } else {
        if (!g.victor.get()) {
            g.victor       = whichPlayer;
            g.victory_text = text.copy();
            g.next_level   = nextLevel;
            if (!g.game_over) {
                g.game_over    = true;
                g.game_over_at = g.time + secs(3);
            }
        }
    }
}

// GetLevelFullScaleAndCorner:
//  This is really just for the mission briefing.  It calculates the best scale
//  at which to show the entire scenario.

void GetLevelFullScaleAndCorner(
        const Level* level, int32_t rotation, coordPointType* corner, int32_t* scale,
        Rect* bounds) {
    int32_t biggest, mustFit;
    Point   coord, otherCoord, tempCoord;

    mustFit = bounds->bottom - bounds->top;
    if ((bounds->right - bounds->left) < mustFit)
        mustFit = bounds->right - bounds->left;

    biggest = 0;
    for (const auto& initial : Level::Initial::all()) {
        if (!(initial->attributes.initially_hidden())) {
            GetInitialCoord(initial, reinterpret_cast<coordPointType*>(&coord), g.angle);

            for (const auto& other : Level::Initial::all()) {
                GetInitialCoord(other, reinterpret_cast<coordPointType*>(&otherCoord), g.angle);

                if (ABS(otherCoord.h - coord.h) > biggest) {
                    biggest = ABS(otherCoord.h - coord.h);
                }
                if (ABS(otherCoord.v - coord.v) > biggest) {
                    biggest = ABS(otherCoord.v - coord.v);
                }
            }
        }
    }

    biggest += biggest >> 2L;

    *scale = SCALE_SCALE * mustFit;
    *scale /= biggest;

    otherCoord.h = kUniversalCenter;
    otherCoord.v = kUniversalCenter;
    coord.h      = kUniversalCenter;
    coord.v      = kUniversalCenter;
    for (const auto& initial : Level::Initial::all()) {
        if (!(initial->attributes.initially_hidden())) {
            GetInitialCoord(initial, reinterpret_cast<coordPointType*>(&tempCoord), g.angle);

            if (tempCoord.h < coord.h) {
                coord.h = tempCoord.h;
            }
            if (tempCoord.v < coord.v) {
                coord.v = tempCoord.v;
            }

            if (tempCoord.h > otherCoord.h) {
                otherCoord.h = tempCoord.h;
            }
            if (tempCoord.v > otherCoord.v) {
                otherCoord.v = tempCoord.v;
            }
        }
    }

    biggest = bounds->right - bounds->left;
    biggest *= SCALE_SCALE;
    biggest /= *scale;
    biggest /= 2;
    corner->h = (coord.h + (otherCoord.h - coord.h) / 2) - biggest;
    biggest   = (bounds->bottom - bounds->top);
    biggest *= SCALE_SCALE;
    biggest /= *scale;
    biggest /= 2;
    corner->v = (coord.v + (otherCoord.v - coord.v) / 2) - biggest;
}

coordPointType Translate_Coord_To_Level_Rotation(int32_t h, int32_t v) {
    return rotate_coords(h, v, g.angle);
}

}  // namespace antares
