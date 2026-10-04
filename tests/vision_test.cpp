#include <cstdlib>
#include <functional>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "avatar.h"
#include "avatar_action.h"
#include "calendar.h"
#include "cata_catch.h"
#include "character.h"
#include "coordinates.h"
#include "creature.h"
#include "creature_tracker.h"
#include "current_map.h"
#include "enums.h"
#include "game.h"
#include "level_cache.h"
#include "lightmap.h"
#include "map.h"
#include "map_helpers.h"
#include "map_helpers_tests.h"
#include "map_iterator.h"
#include "map_scale_constants.h"
#include "map_test_case.h"
#include "mdarray.h"
#include "monster.h"
#include "mtype.h"
#include "options_helpers.h"
#include "overmap_ui.h"
#include "player_helpers.h"
#include "point.h"
#include "string_formatter.h"
#include "teleport.h"
#include "type_id.h"
#include "units.h"
#include "vehicle.h"
#include "vpart_position.h"
#include "vpart_range.h"
#include "weather_type.h"

static const efftype_id effect_narcosis( "narcosis" );

static const field_type_str_id field_fd_fire( "fd_fire" );
static const field_type_str_id field_fd_smoke( "fd_smoke" );

static const furn_str_id furn_test_f_translucent( "test_f_translucent" );

static const move_mode_id move_mode_crouch( "crouch" );
static const move_mode_id move_mode_walk( "walk" );

static const mtype_id mon_test_camera( "mon_test_camera" );
static const mtype_id mon_zombie( "mon_zombie" );
static const mtype_id mon_zombie_electric( "mon_zombie_electric" );

static const ter_str_id ter_t_brick_wall( "t_brick_wall" );
static const ter_str_id ter_t_curtains( "t_curtains" );
static const ter_str_id ter_t_door_c( "t_door_c" );
static const ter_str_id ter_t_door_glass_frosted_c( "t_door_glass_frosted_c" );
static const ter_str_id ter_t_door_o( "t_door_o" );
static const ter_str_id ter_t_flat_roof( "t_flat_roof" );
static const ter_str_id ter_t_floor( "t_floor" );
static const ter_str_id ter_t_grass( "t_grass" );
static const ter_str_id ter_t_utility_light( "t_utility_light" );
static const ter_str_id ter_t_window_domestic( "t_window_domestic" );
static const ter_str_id ter_t_window_frame( "t_window_frame" );
static const ter_str_id ter_t_window_stained_green( "t_window_stained_green" );

static const trait_id trait_MYOPIC( "MYOPIC" );

static const vpart_id vpart_door_opaque( "door_opaque" );
static const vpart_id vpart_frame( "frame" );
static const vpart_id vpart_inboard_mirror( "inboard_mirror" );

static const vproto_id vehicle_prototype_meth_lab( "meth_lab" );
static const vproto_id vehicle_prototype_none( "none" );
static const vproto_id vehicle_prototype_vehicle_camera_test( "vehicle_camera_test" );

static const weather_type_id weather_clear( "clear" );
static const weather_type_id weather_fog( "fog" );

static int get_actual_light_level( const map_test_case::tile &t )
{
    const map &here = get_map();
    const visibility_variables &vvcache = here.get_visibility_variables_cache();
    return static_cast<int>( here.apparent_light_at( t.p, vvcache ) );
}

static std::string vision_test_info( map_test_case &t )
{
    std::ostringstream out;
    map &here = get_map();

    using namespace map_test_case_common;

    out << "origin: " << t.get_origin() << '\n';
    out << "player: " << get_player_character().pos_bub() << '\n';
    out << "unimpaired_range: " << get_player_character().unimpaired_range()  << '\n';
    out << "vision_threshold: " << here.get_visibility_variables_cache().vision_threshold << '\n';

    out << "fields:\n" <<  printers::fields( t ) << '\n';
    out << "transparency:\n" <<  printers::transparency( t ) << '\n';

    out << "seen:\n" <<  printers::seen( t ) << '\n';
    out << "lm:\n" <<  printers::lm( t ) << '\n';
    out << "apparent_light:\n" <<  printers::apparent_light( t ) << '\n';
    out << "obstructed:\n" <<  printers::obstructed( t ) << '\n';
    out << "floor_above:\n" <<  printers::floor( t, 1 ) << '\n';

    out << "expected:\n" <<  printers::expected( t ) << '\n';
    out << "actual:\n" << printers::format_2d_array(
    t.map_tiles_str( [&]( map_test_case::tile t, std::ostringstream & os ) {
        os << get_actual_light_level( t );
    } ) ) << '\n';

    return out.str();
}

static void assert_tile_light_level( map_test_case::tile t )
{
    if( t.expect_c < '0' || t.expect_c > '9' ) {
        FAIL( "unexpected result char '" << t.expect_c << "'" );
    }
    const int expected_level = t.expect_c - '0';
    REQUIRE( expected_level == get_actual_light_level( t ) );
}

static const time_point midnight = calendar::turn_zero + 0_hours;
static const time_point day_time = calendar::turn_zero + 9_hours + 30_minutes;

using namespace map_test_case_common;
using namespace map_test_case_common::tiles;

static const tile_predicate ter_set_flat_roof_above = ter_set( ter_t_flat_roof, tripoint::above );

static bool spawn_moncam( map_test_case::tile tile )
{
    monster *const slime = g->place_critter_at( mon_test_camera, tile.p );
    REQUIRE( slime->type->vision_day == 6 );
    slime->friendly = -1;
    return true;
}

static const tile_predicate set_up_tiles_common =
    ifchar( ' ', noop ) ||
    ifchar( 'U', noop ) ||
    ifchar( 'C', noop ) ||
    ifchar( 'Z', noop ) ||
    ifchar( 'z', ter_set( ter_t_floor ) + ter_set_flat_roof_above ) ||
    ifchar( 'u', ter_set( ter_t_floor ) + ter_set_flat_roof_above ) ||
    ifchar( 'L', ter_set( ter_t_utility_light ) + ter_set_flat_roof_above ) ||
    ifchar( '#', ter_set( ter_t_brick_wall ) + ter_set_flat_roof_above ) ||
    ifchar( '=', ter_set( ter_t_window_frame ) + ter_set_flat_roof_above ) ||
    ifchar( '-', ter_set( ter_t_floor ) + ter_set_flat_roof_above ) ||
    ifchar( 'G', ter_set( ter_t_window_stained_green ) + ter_set_flat_roof_above ) ||
    fail;

namespace
{
struct vision_test_flags {
    bool crouching = false;
    bool headlamp = false;
    bool blindfold = false;
    bool moncam = false;
    bool myopic = false;
};
} // namespace

namespace
{
struct vision_test_case {

    std::vector<std::string> setup;
    std::vector<std::string> expected_results;
    time_point time = day_time;
    vision_test_flags flags;
    tile_predicate set_up_tiles = set_up_tiles_common;
    std::string section_prefix;
    char anchor_char = 0;
    std::function<void()> intermission;

    vision_test_case( const std::vector<std::string> &setup,
                      const std::vector<std::string> &expectedResults,
                      const time_point &time ) : setup( setup ), expected_results( expectedResults ), time( time ) {}

    void test_all() const {
        Character &player_character = get_player_character();
        g->place_player( { 60, 60, 0 } );
        player_character.clear_worn(); // Remove any light-emitting clothing
        player_character.clear_effects();
        player_character.clear_bionics();
        player_character.clear_mutations(); // remove mutations that potentially affect vision
        player_character.clear_moncams();
        clear_map_without_vision( -2,
                                  OVERMAP_HEIGHT ); // without_vision just skips updating map memory which we don't test here.
        g->reset_light_level();
        scoped_weather_override weather_clear( WEATHER_CLEAR );

        REQUIRE( !player_character.is_blind() );
        REQUIRE( !player_character.in_sleep_state() );
        REQUIRE( !player_character.has_effect( effect_narcosis ) );

        player_character.recalc_sight_limits();

        calendar::turn = time;

        map_test_case t;
        t.setup = setup;
        t.expected_results = expected_results;
        if( anchor_char == 0 ) {
            t.set_anchor_char_from( {'u', 'U', 'V'} );
        } else {
            t.set_anchor_char_from( {anchor_char} );
        }
        REQUIRE( t.anchor_char.has_value() );
        t.anchor_map_pos = player_character.pos_bub();

        if( flags.crouching ) {
            player_character.set_movement_mode( move_mode_crouch );
        } else {
            player_character.set_movement_mode( move_mode_walk );
        }
        if( flags.headlamp ) {
            player_add_headlamp();
        }
        if( flags.blindfold ) {
            player_wear_blindfold();
        }
        if( flags.moncam ) {
            player_character.add_moncam( { mon_test_camera, 60 } );
        }
        if( flags.myopic ) {
            player_character.set_mutation( trait_MYOPIC );
        }

        std::stringstream section_name;
        section_name << section_prefix;
        section_name << t.generate_transform_combinations();

        // Sanity check on player placement in relation to `t`
        // must be invoked after transformations are applied to `t`
        t.validate_anchor_point( player_character.pos_bub() );

        SECTION( section_name.str() ) {
            t.for_each_tile( set_up_tiles );
            int zlev = t.get_origin().z();
            map &here = get_map();
            // We have to run the whole thing twice, because the first time through the
            // player's vision_threshold is based on the previous lighting level (so
            // they might, for example, have poor nightvision due to having just been
            // in daylight)
            here.invalidate_visibility_cache();
            here.update_visibility_cache( zlev );
            // make sure floor caches are valid on all zlevels above
            for( int z = -2; z <= OVERMAP_HEIGHT; z++ ) {
                here.invalidate_map_cache( z );
            }
            here.build_map_cache( zlev );
            here.invalidate_visibility_cache();
            here.update_visibility_cache( zlev );
            here.invalidate_map_cache( zlev );
            here.build_map_cache( zlev );
            if( intermission ) {
                intermission();
            }

            INFO( vision_test_info( t ) );
            t.for_each_tile( assert_tile_light_level );
        }
    }
};
} // namespace

static std::optional<units::angle> testcase_veh_dir( point const &def, vision_test_case const &t,
        map_test_case::tile &tile )
{
    std::optional<units::angle> dir = std::nullopt;
    point const dim( t.setup[0].size(), t.setup.size() );
    if( tile.p_local == def ) {
        dir = 0_degrees;
    } else if( tile.p_local == def.rotate( 1, dim ) ) {
        dir = 90_degrees;
    } else if( tile.p_local == def.rotate( 2, dim ) ) {
        dir = 180_degrees;
    } else if( tile.p_local == def.rotate( 3, dim ) ) {
        dir = 270_degrees;
    }
    return dir;
}

// The following characters are used in these setups:
// ' ' - empty, outdoors
// '-' - empty, indoors
// 'U' - player, outdoors
// 'u' - player, indoors
// 'L' - light, indoors
// '#' - wall
// '=' - window frame

TEST_CASE( "vision_daylight", "[shadowcasting][vision]" )
{
    vision_test_case t {
        {
            "   ",
            "   ",
            " U ",
        },
        {
            "444",
            "444",
            "444",
        },
        day_time
    };

    t.test_all();
}

TEST_CASE( "vision_day_indoors", "[shadowcasting][vision]" )
{
    vision_test_case t {
        {
            "###",
            "#u#",
            "###",
        },
        {
            "111",
            "111",
            "111",
        },
        day_time
    };

    t.test_all();
}

TEST_CASE( "vision_light_shining_in", "[shadowcasting][vision]" )
{
    vision_test_case t {
        {
            "##########",
            "#--------#",
            "#u-------#",
            "#--------=",
            "##########",
        },
        {
            "1144444666",
            "1144444466",
            "1144444444",
            "1144444444",
            "1144444444",
        },
        day_time
    };

    t.test_all();
}

TEST_CASE( "vision_no_lights", "[shadowcasting][vision]" )
{
    vision_test_case t {
        {
            "   ",
            " U ",
        },
        {
            "111",
            "111",
        },
        midnight
    };

    t.test_all();
}

TEST_CASE( "vision_utility_light", "[shadowcasting][vision]" )
{
    vision_test_case t {
        {
            " L ",
            "   ",
            " U ",
        },
        {
            "444",
            "444",
            "444",
        },
        midnight
    };

    t.test_all();
}

TEST_CASE( "vision_wall_obstructs_light", "[shadowcasting][vision]" )
{
    vision_test_case t {
        {
            " L ",
            "###",
            " U ",
        },
        {
            "666",
            "111",
            "111",
        },
        midnight
    };

    t.test_all();
}

TEST_CASE( "vision_wall_can_be_lit_by_player", "[shadowcasting][vision]" )
{
    vision_test_case t {
        {
            " U",
            "  ",
            "  ",
            "##",
            "--",
        },
        {
            "44",
            "44",
            "44",
            "44",
            "66",
        },
        midnight
    };
    t.flags.headlamp = true;

    t.test_all();
}

TEST_CASE( "vision_crouching_blocks_vision_but_not_light", "[shadowcasting][vision]" )
{
    vision_test_case t {
        {
            "###",
            "#u#",
            "#=#",
            "   ",
        },
        {
            "444",
            "444",
            "444",
            "666",
        },
        day_time
    };
    t.flags.crouching = true;

    t.test_all();
}

TEST_CASE( "vision_translucent_blocks_vision_but_not_light", "[shadowcasting][vision]" )
{
    vision_test_case t{
        {
            "###",
            "#u#",
            "#G#",
            "   ",
        },
        {
            "444",
            "444",
            "444",
            "666",
        },
        day_time
    };

    t.test_all();
}

TEST_CASE( "vision_see_wall_in_moonlight", "[shadowcasting][vision]" )
{
    const time_point full_moon = calendar::turn_zero + calendar::season_length() / 6;
    // Verify that I've picked the full_moon time correctly.
    CHECK( get_moon_phase( full_moon ) == MOON_FULL );

    vision_test_case t {
        {
            "---",
            "###",
            "   ",
            "   ",
            " U ",
        },
        {
            "666",
            "111",
            "111",
            "111",
            "111",
        },
        // Want a night time
        full_moon - time_past_midnight( full_moon )
    };

    t.test_all();
}

TEST_CASE( "vision_player_opaque_neighbors_still_visible_night", "[shadowcasting][vision]" )
{
    /**
     *  Even when stating inside the opaque wall and surrounded by opaque walls,
     *  you should see yourself and immediate surrounding.
     *  (walls here simulate the behavior of the fully opaque fields, e.g. thick smoke)
     */
    vision_test_case t {
        {
            "#####",
            "#####",
            "##u##",
            "#####",
            "#####",
        },
        {
            "66666",
            "61116",
            "61116",
            "61116",
            "66666",
        },
        midnight
    };

    if( GENERATE( false, true ) ) {
        // first scenario: player is surrounded by walls
        // overriding 'u' to set up brick wall and roof at player's position
        t.set_up_tiles =
            ifchar( 'u', ter_set( ter_t_brick_wall ) + ter_set_flat_roof_above ) ||
            t.set_up_tiles;

        t.section_prefix = "walls_";
    } else {
        // second scenario: player is surrounded by thick smoke
        // overriding 'u' to set thick smoke everywhere
        t.set_up_tiles = [&]( map_test_case::tile t ) {
            get_map().add_field( t.p, field_fd_smoke );
            return true;
        };
        t.section_prefix = "smoke_";
    }

    t.test_all();
}

TEST_CASE( "vision_single_tile_skylight", "[shadowcasting][vision]" )
{
    /**
     * Light shines through the single-tile hole in the roof. Apparent light should be symmetrical.
     */
    vision_test_case t {
        {
            "---------",
            "-#######-",
            "-#-----#-",
            "-#-----#-",
            "-#--U--#-",
            "-#-----#-",
            "-#-----#-",
            "-#######-",
            "---------",
        },
        {
            "666666666",
            "661111166",
            "611111116",
            "611141116",
            "611444116",
            "611141116",
            "611111116",
            "661111166",
            "666666666",
        },
        day_time
    };

    t.test_all();
}

TEST_CASE( "vision_junction_reciprocity", "[vision][reciprocity]" )
{
    const map &here = get_map();

    bool player_in_junction = GENERATE( true, false );
    CAPTURE( player_in_junction );

    vision_test_case t {
        player_in_junction ?
        std::vector<std::string>{
            "###   ",
            "#u####",
            "#---z#",
            "######",
}:
        std::vector<std::string>{
            "###   ",
            "#z####",
            "#---u#",
            "######",
        },
        player_in_junction ?
        std::vector<std::string>{
            "444666",
            "444666",
            "444466",
            "444466",
}:
        std::vector<std::string>{
            "666666",
            "444444",
            "444444",
            "444444",
        },
        day_time
    };

    monster *zombie = nullptr;
    tile_predicate spawn_zombie = [&]( map_test_case::tile tile ) {
        zombie = g->place_critter_at( mon_zombie, tile.p );
        get_map().ter_set( tile.p + tripoint::above, ter_t_flat_roof );
        return true;
    };

    t.set_up_tiles =
        ifchar( 'z', spawn_zombie ) ||
        t.set_up_tiles;
    t.flags.headlamp = true;
    t.test_all();

    if( player_in_junction ) {
        REQUIRE( !get_avatar().sees( here, *zombie ) );
        REQUIRE( !zombie->sees( here, get_avatar() ) );
    } else {
        REQUIRE( get_avatar().sees( here, *zombie ) );
        REQUIRE( zombie->sees( here, get_avatar() ) );
    }
}

TEST_CASE( "vision_blindfold_reciprocity", "[vision][reciprocity]" )
{
    const map &here = get_map();

    vision_test_case t {
        {
            "U  Z",
        },
        {
            "4666",
        },
        day_time
    };

    monster *zombie = nullptr;
    tile_predicate spawn_zombie = [&]( map_test_case::tile tile ) {
        zombie = g->place_critter_at( mon_zombie, tile.p );
        return true;
    };

    t.flags.blindfold = true;
    t.set_up_tiles =
        ifchar( 'C', spawn_moncam ) ||
        ifchar( 'Z', spawn_zombie ) ||
        t.set_up_tiles;
    t.test_all();

    REQUIRE( !get_avatar().sees( here,  *zombie ) );
    // don't "optimize" lightcasting with player sight range
    REQUIRE( zombie->sees( here, get_avatar() ) );
}

TEST_CASE( "vision_moncam_basic", "[shadowcasting][vision][moncam]" )
{
    const map &here = get_map();

    bool add_moncam = GENERATE( true, false );
    bool obstructed = GENERATE( true, false );

    vision_test_case t {
        obstructed ?
        std::vector<std::string>{
            "             ",
            "             ",
            "             ",
            "      Z      ",
            "             ",
            "             ",
            "      C      ",
            "             ",
            "             ",
            "             ",
            "             ",
            "           ##",
            "           #u",
} :
        std::vector<std::string>{
            "             ",
            "             ",
            "             ",
            "      Z      ",
            "             ",
            "             ",
            "      C      ",
            "             ",
            "             ",
            "             ",
            "             ",
            "             ",
            "            u",
        },
        add_moncam ?
        std::vector<std::string>{
            "6661111111666",
            "6611111111166",
            "6111111111116",
            "1111111111111",
            "1111111111111",
            "1111111111111",
            "1111114111111",
            "1111111111111",
            "1111111111111",
            "1111111111111",
            "6111111111116",
            "6611111111166",
            "6661111111664",
} :
        std::vector<std::string>{
            "6666666666666",
            "6666666666666",
            "6666666666666",
            "6666666666666",
            "6666666666666",
            "6666666666666",
            "6666666666666",
            "6666666666666",
            "6666666666666",
            "6666666666666",
            "6666666666666",
            "6666666666666",
            "6666666666664",
        }
        ,
        sunset( calendar::turn )
    };

    monster *zombie = nullptr;
    tile_predicate spawn_zombie = [&]( map_test_case::tile tile ) {
        zombie = g->place_critter_at( mon_zombie, tile.p );
        return true;
    };
    t.flags.blindfold = true;
    t.flags.moncam = add_moncam;
    t.set_up_tiles =
        ifchar( 'C', spawn_moncam ) ||
        ifchar( 'Z', spawn_zombie ) ||
        t.set_up_tiles;

    t.test_all();

    avatar &u = get_avatar();
    REQUIRE( zombie->sees( here, u ) == !obstructed );
    if( add_moncam ) {
        REQUIRE( u.sees( here,  zombie->pos_bub( here ), true ) );
    } else {
        REQUIRE( !u.sees( here, zombie->pos_bub( here ), true ) );
    }
}

TEST_CASE( "vision_moncam_otherz", "[shadowcasting][vision][moncam]" )
{
    tripoint const disp = GENERATE( tripoint::below, tripoint::zero, tripoint::above );
    vision_test_case t {
        {
            "-c-",
            "###",
            "#u#",
            "###",
        },
        disp.z != 0 ?
        std::vector<std::string> {
            "666",
            "666",
            "616",
            "666",
}:
        std::vector<std::string> {
            "444",
            "414",
            "616",
            "666",
        },
        day_time
    };

    tile_predicate spawn_moncam_disp = [&]( map_test_case::tile tile ) {
        tile_predicate const p = ter_set( ter_t_floor ) + ter_set( ter_t_floor, tripoint::below ) +
                                 ter_set_flat_roof_above;
        p( tile );
        monster *const slime = g->place_critter_at( mon_test_camera, tile.p + disp );
        REQUIRE( slime->posz() == get_avatar().posz() + disp.z );
        REQUIRE( slime->type->vision_day == 6 );
        slime->friendly = -1;
        return true;
    };
    t.section_prefix = string_format( "%i_", disp.z );
    t.flags.moncam = true;
    t.flags.blindfold = true; // FIXME: remove once 3dfov takes LOS into account
    t.set_up_tiles =
        ifchar( 'c', spawn_moncam_disp ) ||
        t.set_up_tiles;

    t.test_all();
}

TEST_CASE( "vision_vehicle_mirrors", "[shadowcasting][vision][vehicle]" )
{
    map &here = get_map();
    clear_vehicles();
    bool const blindfold = GENERATE( true, false );
    vision_test_case t {
        {
            "        ",
            "        ",
            "       M",
            "       U",
            "       M",
            "        ",
            "        ",
        },
        blindfold ?
        std::vector<std::string> {
            "66666666",
            "66666666",
            "66666666",
            "66666664",
            "66666666",
            "66666666",
            "66666666",
} :
        std::vector<std::string> {
            "44444444",
            "44444444",
            "66666644",
            "66666644",
            "66666644",
            "44444444",
            "44444444",
        },
        day_time
    };
    tile_predicate spawn_veh = [&]( map_test_case::tile tile ) {
        std::optional<units::angle> dir = testcase_veh_dir( {7, 2}, t, tile );
        if( dir ) {
            vehicle *v = here.add_vehicle( vehicle_prototype_meth_lab, tile.p, *dir, 0,
                                           veh_spawn_status::UNDAMAGED );
            for( const vpart_reference &vp : v->get_avail_parts( "OPENABLE" ) ) {
                v->close( here, vp.part_index() );
            }
        }
        return true;
    };
    t.flags.blindfold = blindfold;
    t.set_up_tiles =
        ifchar( 'M', spawn_veh ) ||
        t.set_up_tiles;
    t.test_all();
    clear_vehicles();
}

TEST_CASE( "vision_vehicle_camera", "[shadowcasting][vision][vehicle]" )
{
    clear_vehicles();
    bool const blindfold = GENERATE( true, false );
    vision_test_case t {
        {
            " M ",
            "   ",
            "   ",
            "   ",
        },
        blindfold ?
        std::vector<std::string>{
            "616",
            "666",
            "666",
            "666",
} :
        std::vector<std::string>{
            "111",
            "444",
            "444",
            "444",
        },
        day_time
    };

    tile_predicate spawn_veh_cam = [&]( map_test_case::tile tile ) {
        // NOLINTNEXTLINE(cata-use-named-point-constants)
        std::optional<units::angle> const dir = testcase_veh_dir( { 1, 0 }, t, tile );
        if( dir ) {
            vehicle *v =
                get_map().add_vehicle( vehicle_prototype_vehicle_camera_test, tile.p, *dir, 0,
                                       veh_spawn_status::UNDAMAGED );
            v->camera_on = true;
        }
        return true;
    };

    t.anchor_char = 'M';
    t.flags.blindfold = blindfold;
    t.set_up_tiles =
        ifchar( 'M', spawn_veh_cam ) ||
        t.set_up_tiles;

    t.test_all();
    clear_vehicles();
}

TEST_CASE( "vision_vehicle_camera_skew", "[shadowcasting][vision][vehicle][vehicle_fake]" )
{
    map &here = get_map();

    clear_vehicles();
    bool const camera_on = GENERATE( true, false );
    int const fiddle = GENERATE( 0, 1, 2 );
    vision_test_case t {
        {
            "    M",
            "     ",
            "     ",
            "     ",
            "     ",
        },
        camera_on ?
        std::vector<std::string>{
            "44611",
            "44444",
            "44446",
            "44446",
            "44444",
        }
:
        std::vector<std::string>{
            "66611",
            "66611",
            "66666",
            "66666",
            "66666",
        },     day_time
    };

    vehicle *v = nullptr;
    tile_predicate spawn_veh_cam = [&]( map_test_case::tile tile ) {
        std::optional<units::angle> const dir = testcase_veh_dir( { 4, 0 }, t, tile );
        if( dir ) {
            units::angle const skew = *dir + 45_degrees;
            v = here.add_vehicle( vehicle_prototype_vehicle_camera_test, tile.p, skew, 0,
                                  veh_spawn_status::UNDAMAGED );
            v->camera_on = camera_on;
        }
        return true;
    };

    auto const fiddle_parts = [&]() {
        if( fiddle > 0 ) {
            std::vector<vehicle_part *> const horns = v->get_parts_at( v->pos_abs(), "HORN", {} );
            v->remove_part( *horns.front() );
        }
        if( fiddle > 1 ) {
            REQUIRE( v->install_part( here, point_rel_ms::zero, vpart_inboard_mirror ) != -1 );
        }
        if( fiddle > 0 ) {
            here.add_vehicle_to_cache( v );
            here.invalidate_map_cache( get_avatar().posz() );
            here.build_map_cache( get_avatar().posz() );
        }
    };

    t.anchor_char = 'M';
    t.intermission = fiddle_parts;
    t.set_up_tiles =
        ifchar( 'M', spawn_veh_cam ) ||
        t.set_up_tiles;

    CAPTURE( camera_on, fiddle );
    t.test_all();
    clear_vehicles();
}

TEST_CASE( "vision_moncam_invalidation", "[shadowcasting][vision][moncam]" )
{
    clear_vehicles();
    vision_test_case t {
        {
            "   ",
            " M ",
            "   ",
            "   ",
            "###",
            " C ",
        },
        {
            "111",
            "111",
            "444",
            "444",
            "444",
            "444",
        },
        day_time
    };

    tile_predicate spawn_veh_cam = [&]( map_test_case::tile tile ) {
        // NOLINTNEXTLINE(cata-use-named-point-constants)
        std::optional<units::angle> const dir = testcase_veh_dir( { 1, 1 }, t, tile );
        if( dir ) {
            vehicle *v =
                get_map().add_vehicle( vehicle_prototype_vehicle_camera_test, tile.p, *dir, 0,
                                       veh_spawn_status::UNDAMAGED );
            v->camera_on = true;
        }
        return true;
    };

    monster *slime = nullptr;
    tile_predicate spawn_moncam_wiggle = [&]( map_test_case::tile tile ) {
        slime = g->place_critter_at( mon_test_camera, tile.p );
        slime->friendly = -1;
        return true;
    };

    auto wiggle_slime = [&]() {
        // vehicle camera should still work even if only the moncam moved
        slime->Creature::move_to( slime->pos_abs() + tripoint::east );
        get_map().build_map_cache( slime->posz() );
        slime->Creature::move_to( slime->pos_abs() - tripoint::east );
        get_map().build_map_cache( slime->posz() );
    };

    t.anchor_char = 'M';
    t.flags.moncam = true;
    t.intermission = wiggle_slime;
    t.set_up_tiles =
        ifchar( 'C', spawn_moncam_wiggle ) ||
        ifchar( 'M', spawn_veh_cam ) ||
        t.set_up_tiles;

    t.test_all();
    clear_vehicles();
}

TEST_CASE( "vision_bright_source", "[vision]" )
{
    vision_test_case t {
        {
            "U             Z",
        },
        {
            "444444444444462",
        },
        day_time
    };

    monster *zombie = nullptr;
    tile_predicate spawn_shocker = [&]( map_test_case::tile tile ) {
        zombie = g->place_critter_at( mon_zombie_electric, tile.p );
        return true;
    };

    t.flags.myopic = true;
    t.set_up_tiles =
        ifchar( 'Z', spawn_shocker ) ||
        t.set_up_tiles;
    t.test_all();
}

TEST_CASE( "vision_inside_meth_lab", "[shadowcasting][vision][moncam]" )
{
    map &here = get_map();

    clear_vehicles();

    bool door_open = GENERATE( false, true );
    bool moncam = GENERATE( false, true );

    vision_test_case t {
        {
            "  MCM  ", // left M is origin location of meth lab (driver's seat); camera can see side mirrors
            "       ",
            "       ",
            "   U   ",
            "       ",
            "       ",
            "   D   ", // D mark door to be opened
            "       "
        },
        door_open ?
        !moncam ?
        std::vector<std::string> {
            // when door is open, light shines inside, forming a cone
            "6666666",
            "6444446",
            "6444446",
            "6444446",
            "6444446",
            "6144416",
            "6444446",
            "6644466"
} :
        std::vector<std::string> {
            "4444444",
            "4444444",
            "4444444",
            "6444446",
            "6444446",
            "6144416",
            "6444446",
            "6644466"
} :

        moncam ?
        std::vector<std::string> {
            // active moncam can see through mirrors
            "4444444",
            "4444444",
            "4411144",
            "6411146",
            "6111116",
            "6111116",
            "6111116",
            "6666666"
} :
        std::vector<std::string> {
            // when door is closed, everything is dark
            "6666666",
            "6111116",
            "6111116",
            "6111116",
            "6111116",
            "6111116",
            "6111116",
            "6666666"
        },
        day_time
    };

    vehicle *v = nullptr;
    std::optional<tripoint_bub_ms> door = std::nullopt;

    // opens or closes a specific door (marked as 'D')
    // this is called twice: after either vehicle or door is set
    // and it executed a single time when both vehicle and door position are available
    auto open_door = [&]() {
        if( !door_open || !v || !door ) {
            return;
        }
        // open door at `door` location
        for( const vehicle_part *vp : v->get_parts_at( &here, *door, "OPENABLE", part_status_flag::any ) ) {
            v -> open( here, v->index_of_part( vp ) );
        }
    };

    tile_predicate set_door_location = [&]( map_test_case::tile tile ) {
        door = tile.p;
        open_door();
        return true;
    };

    tile_predicate spawn_meth_lab = [&]( map_test_case::tile tile ) {
        std::optional<units::angle> dir;
        if( tile.p_local == point( 2, 0 ) ) {
            dir = 270_degrees;
        } else if( tile.p_local == point( 4, 7 ) ) {
            dir = 90_degrees;
        } else if( tile.p_local == point( 0, 4 ) ) {
            dir = 180_degrees;
        } else if( tile.p_local == point( 7, 2 ) ) {
            dir = 0_degrees;
        }
        if( dir ) {
            v = here.add_vehicle( vehicle_prototype_meth_lab, tile.p, *dir, 0, veh_spawn_status::UNDAMAGED );
            for( const vpart_reference &vp : v->get_avail_parts( "OPENABLE" ) ) {
                v -> close( here, vp.part_index() );
            }
            open_door();
        }
        return true;
    };

    t.flags.moncam = moncam;
    t.set_up_tiles =
        ifchar( 'C', spawn_moncam ) ||
        ifchar( 'M', spawn_meth_lab ) ||
        ifchar( 'D', set_door_location ) ||
        t.set_up_tiles;

    t.test_all();
    clear_vehicles();
}

static void set_up_transition_scene( const tripoint_bub_ms &origin )
{
    clear_avatar();
    clear_map_without_vision( -2, OVERMAP_HEIGHT );
    g->place_player( origin );
    // placing the avatar can load submaps with their own monsters, and safe
    // mode would stop these tests' moves
    clear_creatures();
    g->set_safe_mode( SAFE_MODE_OFF );
    calendar::turn = day_time;
    g->reset_light_level();
}

// brick wall ring under a roof around a floor tile at origin
static void build_transition_room( const tripoint_bub_ms &origin )
{
    map &here = get_map();
    for( const tripoint_bub_ms &p : here.points_in_radius( origin, 1 ) ) {
        here.ter_set( p, p == origin ? ter_t_floor : ter_t_brick_wall );
        here.ter_set( p + tripoint::above, ter_t_flat_roof );
    }
}

static void rebuild_vision_caches()
{
    map &here = get_map();
    for( int z = -OVERMAP_DEPTH; z <= OVERMAP_HEIGHT; ++z ) {
        here.invalidate_map_cache( z );
    }
    here.build_map_cache( 0 );
    here.invalidate_visibility_cache();
    here.update_visibility_cache( 0 );
}

// twice, as vision_test_case does: second pass sees lighting the first built
static void settle_vision_caches()
{
    rebuild_vision_caches();
    rebuild_vision_caches();
}

static void build_vision_caches_incrementally()
{
    map &here = get_map();
    here.build_map_cache( 0 );
    here.update_visibility_cache( 0 );
}

TEST_CASE( "vision_own_tile_override_follows_avatar", "[vision]" )
{
    tripoint_bub_ms start;
    int steps = 0;
    SECTION( "within_a_submap" ) {
        start = { 64, 64, 0 };
        steps = 1;
    }
    SECTION( "across_a_submap_edge_then_one_more_step" ) {
        // 71 is east column of center submap: first step shifts the map, second
        // lands next to shift's landing tile
        start = { 71, 64, 0 };
        steps = 2;
    }
    CAPTURE( start, steps );
    set_up_transition_scene( start );
    scoped_weather_override fog( weather_fog );
    map &here = get_map();
    avatar &you = get_avatar();
    settle_vision_caches();
    for( int step = 0; step < steps; ++step ) {
        REQUIRE( avatar_action::move( you, here, tripoint_rel_ms::east ) );
    }
    build_vision_caches_incrementally();
    const tripoint_bub_ms left_tile = you.pos_bub() + tripoint::west;
    const tripoint_bub_ms two_behind = left_tile + tripoint::west;
    const level_cache &cache = here.access_cache( 0 );
    REQUIRE( cache.outside_cache[two_behind.xy()] );
    const float left_tile_vision = cache.vision_transparency_cache[left_tile.xy()];
    const float two_behind_seen = cache.seen_cache[two_behind.xy()];
    rebuild_vision_caches();
    REQUIRE( cache.vision_transparency_cache[left_tile.xy()] > LIGHT_TRANSPARENCY_OPEN_AIR );
    CHECK( left_tile_vision == cache.vision_transparency_cache[left_tile.xy()] );
    CHECK( two_behind_seen == Approx( cache.seen_cache[two_behind.xy()] ) );
}

TEST_CASE( "vision_crouch_cover_follows_avatar", "[vision]" )
{
    const tripoint_bub_ms origin{ 60, 60, 0 };
    set_up_transition_scene( origin );
    map &here = get_map();
    avatar &you = get_avatar();
    const level_cache &cache = here.access_cache( 0 );
    const tripoint_bub_ms destination = origin + tripoint_rel_ms{ 4, 0, 0 };
    const tripoint_bub_ms old_frame = origin + tripoint::south;
    const tripoint_bub_ms new_frame = destination + tripoint::south;
    here.ter_set( old_frame, ter_t_window_frame );
    here.ter_set( new_frame, ter_t_window_frame );
    you.set_movement_mode( move_mode_crouch );
    settle_vision_caches();
    REQUIRE( cache.vision_transparency_cache[old_frame.xy()] == LIGHT_TRANSPARENCY_SOLID );
    REQUIRE( cache.vision_transparency_cache[new_frame.xy()] > LIGHT_TRANSPARENCY_SOLID );
    SECTION( "teleport" ) {
        REQUIRE( teleport::teleport_to_point( you, destination, true, false, false ) );
    }
    SECTION( "walk" ) {
        for( int step = 0; step < 4; ++step ) {
            REQUIRE( avatar_action::move( you, here, tripoint_rel_ms::east ) );
        }
    }
    REQUIRE( you.pos_bub() == destination );
    build_vision_caches_incrementally();
    const float incremental_old = cache.vision_transparency_cache[old_frame.xy()];
    const float incremental_new = cache.vision_transparency_cache[new_frame.xy()];
    rebuild_vision_caches();
    CHECK( incremental_old == cache.vision_transparency_cache[old_frame.xy()] );
    CHECK( incremental_new == cache.vision_transparency_cache[new_frame.xy()] );
}

TEST_CASE( "vision_posture_change_recasts_fov", "[vision]" )
{
    const tripoint_bub_ms origin{ 60, 60, 0 };
    set_up_transition_scene( origin );
    build_transition_room( origin );
    map &here = get_map();
    avatar &you = get_avatar();
    const level_cache &cache = here.access_cache( 0 );
    const tripoint_bub_ms window = origin + tripoint::south;
    const tripoint_bub_ms target = window + tripoint::south;
    here.ter_set( window, ter_t_window_frame );
    bool crouching = false;
    SECTION( "crouching" ) {
        crouching = true;
    }
    SECTION( "standing" ) {
        you.set_movement_mode( move_mode_crouch );
    }
    CAPTURE( crouching );
    settle_vision_caches();
    const float before = cache.seen_cache[target.xy()];
    you.set_movement_mode( crouching ? move_mode_crouch : move_mode_walk );
    build_vision_caches_incrementally();
    const float incremental_seen = cache.seen_cache[target.xy()];
    rebuild_vision_caches();
    REQUIRE( cache.seen_cache[target.xy()] != Approx( before ) );
    CHECK( incremental_seen == Approx( cache.seen_cache[target.xy()] ) );
}

TEST_CASE( "vision_translucent_tile_blocks_again_after_avatar_leaves", "[vision]" )
{
    // inside center submap, so leaving the tile doesn't shift the map
    const tripoint_bub_ms window{ 64, 64, 0 };
    set_up_transition_scene( window );
    map &here = get_map();
    const level_cache &cache = here.access_cache( 0 );
    here.ter_set( window, ter_t_window_stained_green );
    settle_vision_caches();
    REQUIRE( cache.vision_transparency_cache[window.xy()] == LIGHT_TRANSPARENCY_OPEN_AIR );
    g->place_player( window + tripoint::north );
    REQUIRE( get_avatar().pos_bub() == window + tripoint::north );
    build_vision_caches_incrementally();
    CHECK( cache.vision_transparency_cache[window.xy()] == LIGHT_TRANSPARENCY_SOLID );
}

TEST_CASE( "vision_cache_transitions_match_rebuild", "[vision]" )
{
    // inside center submap, so one step doesn't shift the map
    const tripoint_bub_ms origin{ 64, 64, 0 };
    set_up_transition_scene( origin );
    map &here = get_map();
    avatar &you = get_avatar();
    here.ter_set( origin + tripoint_rel_ms{ 0, 2, 0 }, ter_t_window_frame );
    here.ter_set( origin + tripoint_rel_ms{ 3, 2, 0 }, ter_t_window_frame );
    const vision_cache_oracle oracle( los_pairs_around( origin, 6 ) );

    SECTION( "step" ) {
        here.rebuild_vision_caches_from_scratch( 0 );
        oracle.prime();
        REQUIRE( avatar_action::move( you, here, tripoint_rel_ms::east ) );
    }
    SECTION( "cross_a_submap_edge_then_one_more_step" ) {
        // 71 is east column of center submap
        g->place_player( { 71, 64, 0 } );
        here.rebuild_vision_caches_from_scratch( 0 );
        oracle.prime();
        REQUIRE( avatar_action::move( you, here, tripoint_rel_ms::east ) );
        REQUIRE( avatar_action::move( you, here, tripoint_rel_ms::east ) );
    }
    SECTION( "crouched_teleport_within_a_submap" ) {
        you.set_movement_mode( move_mode_crouch );
        here.rebuild_vision_caches_from_scratch( 0 );
        oracle.prime();
        REQUIRE( teleport::teleport_to_point( you, origin + tripoint_rel_ms{ 3, 1, 0 }, true, false,
                                              false ) );
    }
    SECTION( "crouched_walk" ) {
        you.set_movement_mode( move_mode_crouch );
        here.rebuild_vision_caches_from_scratch( 0 );
        oracle.prime();
        for( int step = 0; step < 3; ++step ) {
            REQUIRE( avatar_action::move( you, here, tripoint_rel_ms::east ) );
        }
    }
    build_vision_caches_incrementally();
    oracle.check_matches_rebuild();
}

TEST_CASE( "vision_cache_scene_transitions_match_rebuild", "[vision]" )
{
    const tripoint_bub_ms origin{ 60, 60, 0 };
    set_up_transition_scene( origin );
    map &here = get_map();
    avatar &you = get_avatar();
    const vision_cache_oracle oracle( los_pairs_around( origin, 6 ) );

    SECTION( "unrelated_opening" ) {
        // blocker and opening are in different submaps, so opening the door
        // rebuilds only the door's submap
        build_transition_room( origin );
        const tripoint_bub_ms blocker = origin + tripoint::south;
        const tripoint_bub_ms opening = origin + tripoint::north;
        const ter_str_id blocker_ter = GENERATE( ter_t_window_stained_green,
                                       ter_t_door_glass_frosted_c );
        const ter_str_id opening_ter = GENERATE( ter_t_door_c, ter_t_curtains, ter_t_window_domestic );
        CAPTURE( blocker_ter, opening_ter );
        here.ter_set( blocker, blocker_ter );
        here.ter_set( opening, opening_ter );
        here.rebuild_vision_caches_from_scratch( 0 );
        oracle.prime();
        REQUIRE( here.open_door( you, opening, true ) );
        build_vision_caches_incrementally();
        oracle.check_matches_rebuild( vision_layers::scene_and_fov );
    }
    SECTION( "weather_turns_to_fog" ) {
        build_transition_room( origin );
        here.ter_set( origin + tripoint::east, ter_t_window_frame );
        scoped_weather_override clear( weather_clear );
        here.rebuild_vision_caches_from_scratch( 0 );
        const tripoint_bub_ms outside = origin + tripoint_rel_ms{ 3, 0, 0 };
        const float clear_transparency = here.access_cache( 0 ).transparency_cache[outside.xy()];
        oracle.prime();
        scoped_weather_override fog( weather_fog );
        build_vision_caches_incrementally();
        REQUIRE( here.access_cache( 0 ).transparency_cache[outside.xy()] != clear_transparency );
        oracle.check_matches_rebuild( vision_layers::scene_and_fov );
    }
    SECTION( "shelter_turns_to_open_ground_in_fog" ) {
        scoped_weather_override fog( weather_fog );
        here.ter_set( origin, ter_t_floor );
        here.rebuild_vision_caches_from_scratch( 0 );
        REQUIRE_FALSE( here.access_cache( 0 ).outside_cache[( origin + tripoint::east ).xy()] );
        oracle.prime();
        here.ter_set( origin, ter_t_grass );
        build_vision_caches_incrementally();
        oracle.check_matches_rebuild( vision_layers::scene_and_fov );
    }
}

TEST_CASE( "vision_translucent_furniture_blocks_sight_but_not_light", "[vision]" )
{
    const tripoint_bub_ms origin{ 60, 60, 0 };
    set_up_transition_scene( origin );
    map &here = get_map();
    const tripoint_bub_ms partition = origin + tripoint::south;
    const tripoint_bub_ms beyond = partition + tripoint::south;
    here.furn_set( partition, furn_test_f_translucent );
    here.rebuild_vision_caches_from_scratch( 0 );
    const level_cache &cache = here.access_cache( 0 );
    CHECK( cache.transparency_cache[partition.xy()] > LIGHT_TRANSPARENCY_SOLID );
    CHECK( cache.sight_cache[partition.xy()] == LIGHT_TRANSPARENCY_SOLID );
    CHECK( cache.seen_cache[beyond.xy()] == 0.0f );
}

TEST_CASE( "vision_physical_wo_fields_keeps_vehicle_walls", "[vision]" )
{
    const tripoint_bub_ms origin{ 60, 60, 0 };
    set_up_transition_scene( origin );
    map &here = get_map();
    vehicle *v = here.add_vehicle( vehicle_prototype_meth_lab, origin + tripoint_rel_ms{ 10, 0, 0 },
                                   0_degrees, 0, veh_spawn_status::UNDAMAGED );
    REQUIRE( v != nullptr );
    for( const vpart_reference &vp : v->get_avail_parts( "OPENABLE" ) ) {
        v->close( here, vp.part_index() );
    }
    here.rebuild_vision_caches_from_scratch( 0 );
    int walls = 0;
    for( const vpart_reference &vp : v->get_avail_parts( "OPAQUE" ) ) {
        const tripoint_bub_ms p = vp.pos_bub( here );
        if( !here.is_transparent( p ) ) {
            ++walls;
            CAPTURE( p );
            CHECK_FALSE( here.is_transparent_wo_fields( p ) );
        }
    }
    REQUIRE( walls > 0 );
    clear_vehicles();
}

TEST_CASE( "vision_vehicle_door_opening_refreshes_reachable_zones", "[vision]" )
{
    const tripoint_bub_ms origin{ 60, 60, 0 };
    set_up_transition_scene( origin );
    map &here = get_map();
    // corridor west to east through origin, walled and roofed
    for( const tripoint_bub_ms &p : here.points_in_radius( origin, 2 ) ) {
        const bool corridor = p.y() == origin.y() && std::abs( p.x() - origin.x() ) <= 1;
        here.ter_set( p, corridor ? ter_t_floor : ter_t_brick_wall );
        here.ter_set( p + tripoint::above, ter_t_flat_roof );
    }
    vehicle *v = here.add_vehicle( vehicle_prototype_none, origin, 0_degrees, 0,
                                   veh_spawn_status::UNDAMAGED );
    REQUIRE( v != nullptr );
    REQUIRE( v->install_part( here, point_rel_ms::zero, vpart_frame ) >= 0 );
    const int door = v->install_part( here, point_rel_ms::zero, vpart_door_opaque );
    REQUIRE( door >= 0 );
    v->close( here, door );
    here.rebuild_vehicle_level_caches();
    monster *const west = g->place_critter_at( mon_zombie, origin + tripoint::west );
    monster *const east = g->place_critter_at( mon_zombie, origin + tripoint::east );
    REQUIRE( west != nullptr );
    REQUIRE( east != nullptr );
    creature_tracker &creatures = get_creature_tracker();
    const auto find_east = [&]() {
        return creatures.find_reachable( *west, [east]( Creature * c ) {
            return c == east;
        } );
    };
    here.build_map_cache( 0 );
    REQUIRE_FALSE( here.passable( origin ) );
    REQUIRE_FALSE( here.is_transparent_wo_fields( origin ) );
    REQUIRE( find_east() == nullptr );
    v->open( here, door );
    here.build_map_cache( 0 );
    REQUIRE( here.is_transparent_wo_fields( origin ) );
    CHECK( find_east() == east );
    clear_vehicles();
}

TEST_CASE( "vision_optical_los_respects_translucent_terrain", "[vision]" )
{
    const tripoint_bub_ms origin{ 60, 60, 0 };
    set_up_transition_scene( origin );
    build_transition_room( origin );
    map &here = get_map();
    const ter_str_id blocker = GENERATE( ter_t_window_stained_green, ter_t_door_glass_frosted_c );
    CAPTURE( blocker );
    const tripoint_bub_ms window = origin + tripoint::south;
    const tripoint_bub_ms target = window + tripoint::south;
    here.ter_set( window, blocker );
    here.rebuild_vision_caches_from_scratch( 0 );
    REQUIRE( here.access_cache( 0 ).seen_cache[target.xy()] == 0.0f );
    const monster observer( mon_zombie, origin );
    CHECK_FALSE( observer.sees( here, target ) );
    CHECK_FALSE( here.sees( origin, target, 10 ) );
    CHECK( here.sees( origin, target, 10, true, los_trace::physical ) );
    // projectiles keep the physical trace, so a shot still lines up through it
    const std::vector<tripoint_bub_ms> path = here.find_clear_path( origin, target, true );
    REQUIRE_FALSE( path.empty() );
    CHECK( path.back() == target );
}

TEST_CASE( "vision_heat_radiation_passes_translucent_partitions", "[vision]" )
{
    const tripoint_bub_ms origin{ 60, 60, 0 };
    set_up_transition_scene( origin );
    map &here = get_map();
    // away from the avatar, whose own heat check walks the line
    const tripoint_bub_ms location = origin + tripoint_rel_ms{ 4, 0, 0 };
    const tripoint_bub_ms partition = location + tripoint::east;
    const tripoint_bub_ms fire = partition + tripoint::east;
    REQUIRE( here.add_field( fire, field_fd_fire, 3 ) );
    const bool furniture = GENERATE( false, true );
    CAPTURE( furniture );
    if( furniture ) {
        here.furn_set( partition, furn_test_f_translucent );
    } else {
        here.ter_set( partition, ter_t_window_stained_green );
    }
    here.build_map_cache( 0 );
    REQUIRE_FALSE( here.sees( location, fire, 6 ) );
    const units::temperature_delta screened = get_heat_radiation( location );
    here.furn_set( partition, furn_str_id::NULL_ID() );
    here.ter_set( partition, ter_t_grass );
    here.build_map_cache( 0 );
    const units::temperature_delta open = get_heat_radiation( location );
    REQUIRE( units::to_fahrenheit_delta( open ) > 0.0f );
    CHECK( units::to_fahrenheit_delta( screened ) == Approx( units::to_fahrenheit_delta( open ) ) );
}

TEST_CASE( "vision_pairwise_los_follows_offscreen_doors", "[vision]" )
{
    const tripoint_bub_ms origin{ 60, 60, 0 };
    set_up_transition_scene( origin );
    build_transition_room( origin );
    map &here = get_map();
    const tripoint_bub_ms door{ 80, 80, 0 };
    const tripoint_bub_ms from = door + tripoint::west;
    const tripoint_bub_ms to = door + tripoint::east;
    const bool initially_open = GENERATE( false, true );
    CAPTURE( initially_open );
    here.ter_set( door, initially_open ? ter_t_door_o : ter_t_door_c );
    here.rebuild_vision_caches_from_scratch( 0 );
    REQUIRE( here.access_cache( 0 ).seen_cache[door.xy()] == 0.0f );
    REQUIRE( here.sees( from, to, 10 ) == initially_open );
    if( initially_open ) {
        REQUIRE( here.close_door( door, true, false ) );
    } else {
        REQUIRE( here.open_door( get_avatar(), door, true ) );
    }
    SECTION( "after_an_incremental_build" ) {
        build_vision_caches_incrementally();
        CHECK( here.sees( from, to, 10 ) != initially_open );
    }
    SECTION( "before_any_build" ) {
        // NPC opening door is followed by NPCs looking through it in the same
        // turn, with no cache build between them
        CHECK( here.sees( from, to, 10 ) != initially_open );
    }
}

TEST_CASE( "vision_potential_los_allows_a_pending_field_change", "[vision]" )
{
    const tripoint_bub_ms origin{ 64, 64, 0 };
    set_up_transition_scene( origin );
    map &here = get_map();
    const tripoint_bub_ms smoke = origin + tripoint::east;
    const tripoint_bub_ms target = smoke + tripoint::east;
    REQUIRE( here.add_field( smoke, field_fd_smoke, 3 ) );
    here.build_map_cache( 0 );
    REQUIRE_FALSE( here.sees( origin, target, 5 ) );
    here.remove_field( smoke, field_fd_smoke );
    // no cache build between the change and the query
    CHECK( here.has_potential_los( origin, target ) );
    CHECK( here.sees( origin, target, 5 ) );
}

TEST_CASE( "vision_clear_path_search_leaves_default_los_alone", "[vision]" )
{
    const tripoint_bub_ms source{ 60, 60, 0 };
    const tripoint_bub_ms destination{ 64, 61, 0 };
    set_up_transition_scene( source );
    map &here = get_map();
    // wall on the default line that another line can pass
    bool found = false;
    for( const tripoint_bub_ms &p : line_to( source, destination, 0, 0 ) ) {
        if( p == destination ) {
            break;
        }
        here.ter_set( p, ter_t_brick_wall );
        here.rebuild_vision_caches_from_scratch( 0 );
        if( !here.sees( source, destination, 10 ) &&
            !here.find_clear_path( source, destination, true ).empty() ) {
            found = true;
            break;
        }
        here.ter_set( p, ter_t_grass );
    }
    REQUIRE( found );
    here.rebuild_vision_caches_from_scratch( 0 );
    REQUIRE_FALSE( here.find_clear_path( source, destination, true ).empty() );
    CHECK_FALSE( here.sees( source, destination, 10 ) );
}

TEST_CASE( "vision_scene_caches_place_vehicles_on_the_map_being_built", "[vision]" )
{
    set_up_transition_scene( { 60, 60, 0 } );
    // map far outside the reality bubble, made current the way mapgen does
    smallmap far;
    far.load( project_to<coords::omt>( get_avatar().pos_abs() ) + point_rel_omt( 20, 20 ), false );
    swap_map swap( *far.cast_to_map() );
    map &here = get_map();
    REQUIRE( &here != &reality_bubble() );
    for( const tripoint_bub_ms &p : here.points_on_zlevel( 0 ) ) {
        here.ter_set( p, ter_t_grass );
        here.furn_set( p, furn_str_id::NULL_ID() );
    }
    const tripoint_bub_ms spot{ 12, 12, 0 };
    vehicle *v = here.add_vehicle( vehicle_prototype_meth_lab, spot, 0_degrees, 0,
                                   veh_spawn_status::UNDAMAGED );
    REQUIRE( v != nullptr );
    for( const vpart_reference &vp : v->get_avail_parts( "OPENABLE" ) ) {
        v->close( here, vp.part_index() );
    }
    // sight query builds this map's scene caches first
    here.sees( spot, spot + tripoint::east, 5 );
    int walls = 0;
    for( const vpart_reference &vp : v->get_avail_parts( "OPAQUE" ) ) {
        walls += !here.is_transparent( vp.pos_bub( here ) );
    }
    CHECK( walls > 0 );
    here.destroy_vehicle( v );
}

TEST_CASE( "vision_caches_of_a_freshly_loaded_map_match_the_saved_scene", "[vision]" )
{
    const tripoint_bub_ms origin{ 60, 60, 0 };
    set_up_transition_scene( origin );
    build_transition_room( origin );
    map &here = get_map();
    GIVEN( "a settled view out of a window" ) {
        const tripoint_bub_ms window = origin + tripoint::east;
        const tripoint_bub_ms target = window + tripoint::east;
        here.ter_set( window, ter_t_window_frame );
        settle_vision_caches();
        const float expected_seen = here.access_cache( 0 ).seen_cache[target.xy()];
        const lit_level expected_visibility = here.access_cache( 0 ).visibility_cache[target.xy()];
        REQUIRE( expected_seen > 0.0f );
        WHEN( "map is saved and loaded into a fresh map" ) {
            const tripoint_abs_sm saved_origin = here.get_abs_sub();
            here.save();
            here = map();
            here.load( saved_origin, false, false );
            build_vision_caches_incrementally();
            THEN( "the view is the one it saved" ) {
                CHECK( here.access_cache( 0 ).seen_cache[target.xy()] == Approx( expected_seen ) );
                CHECK( here.access_cache( 0 ).visibility_cache[target.xy()] == expected_visibility );
            }
        }
    }
}

TEST_CASE( "vision_cache_stationary_build_is_noop", "[vision]" )
{
    const tripoint_bub_ms origin{ 64, 64, 0 };
    set_up_transition_scene( origin );
    build_transition_room( origin );
    get_map().rebuild_vision_caches_from_scratch( 0 );
    check_stationary_build_is_noop();
}

TEST_CASE( "pl_sees-oob-nocrash", "[vision]" )
{
    const map &here = get_map();

    // oob crash from game::place_player_overmap() or game::start_game(), simplified
    clear_avatar();
    get_map().load( project_to<coords::sm>( get_avatar().pos_abs() ) + point::south_east, false,
                    false );
    get_avatar().sees( here, tripoint_bub_ms::zero ); // CRASH?

    clear_avatar();
}
