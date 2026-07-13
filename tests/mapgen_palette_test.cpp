#include "cata_catch.h"
#include "map.h"
#include "mapgendata.h"
#include "mapgen_functions.h"
#include "map_helpers.h"
#include "test_data.h"

static void generate_baseline_data( const std::map<itype_id, int> &data )
{
    printf( "\n        \"expected\": {\n" );
    for( const std::pair<const itype_id, int> &dat : data ) {
        printf( "          \"%s\": [ %i, %i ],\n", dat.first.c_str(), 1, dat.second * 10 );
    }
    printf( "        }" );
    static_cast<void>( fflush( stdout ) );
}

TEST_CASE( "mapgen_palette_item_spawn_test", "[mapgen_palette]" )
{
    map &here = get_map();
    mapgendata mgdata( here, mapgendata::dummy_settings );

    for( const mapgen_palette_test_data &palette_test : test_data::mapgen_palette_data ) {
        clear_map_and_put_player_underground();
        run_mapgen_func( palette_test.mapgen_id, mgdata );

        std::map<itype_id, int> counts;

        for( const tripoint_bub_ms &p : here.points_on_zlevel( 0 ) ) {
            for( const item &it : here.i_at( p ) ) {
                counts[it.typeId()]++;
            }
        }

        if( palette_test.expected.empty() ) {
            generate_baseline_data( counts );
            FAIL( "no expected values given, generated baseline values\nbeware of trailing comma in last entry" );
        }

        for( std::pair<const itype_id, int> &actual : counts ) {
            INFO( string_format( "type %s, amount %i", actual.first.str(), actual.second ) );
            bool found = palette_test.expected.find( actual.first ) != palette_test.expected.end();
            CHECK( found );
            if( found ) {
                std::pair<int, int> expected = palette_test.expected.find( actual.first )->second;
                CHECK( actual.second >= expected.first );
                CHECK( actual.second <= expected.second );
            }
        }
    }
}