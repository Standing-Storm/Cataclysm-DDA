#if defined(TILES)
#include <functional>
#include <string>
#include <vector>

#include "cata_catch.h"
#include "cata_imgui.h"
#include "cata_scope_helpers.h"
#include "font_loader.h"
#include "imgui/imgui.h"
#include "sdltiles.h"

TEST_CASE( "font_config_bitmap_sheets", "[imgui_fonts]" )
{
    CHECK( is_bitmap_typeface( "data/font/map_font_LARWICK.png" ) );
    CHECK( is_bitmap_typeface( "fixedsys.bmp" ) );
    CHECK_FALSE( is_bitmap_typeface( "data/font/Terminus.ttf" ) );
    CHECK_FALSE( is_bitmap_typeface( "unifont" ) );
    CHECK_FALSE( is_bitmap_typeface( "" ) );
}

TEST_CASE( "imgui_fonts_keep_roles", "[imgui_fonts]" )
{
    restore_on_out_of_scope restore_height( fontheight );
    fontheight = 16;
    ImGuiContext *ctx = ImGui::CreateContext();
    on_out_of_scope destroy_ctx( [ctx]() {
        ImGui::DestroyContext( ctx );
    } );
    ImGuiIO &io = ImGui::GetIO();
    const std::vector<font_config> gui = { font_config( "data/font/Roboto-Medium.ttf" ) };
    GIVEN( "mono list starting with a bitmap sheet" ) {
        const std::vector<font_config> mono = {
            font_config( "data/font/map_font_LARWICK.png" ), font_config( "data/font/Terminus.ttf" )
        };
        WHEN( "fonts load without CJK" ) {
            cataimgui::add_cata_fonts( io, gui, mono, false );
            THEN( "gui, mono and 1.5x gui fonts are separate fonts" ) {
                REQUIRE( io.Fonts->Fonts.Size == 3 );
                CHECK( io.Fonts->Fonts[0]->LegacySize == 16.0f );
                CHECK( io.Fonts->Fonts[1]->LegacySize == 16.0f );
                CHECK( io.Fonts->Fonts[2]->LegacySize == 24.0f );
            }
        }
        WHEN( "fonts load with CJK" ) {
            cataimgui::add_cata_fonts( io, gui, mono, true );
            THEN( "gui and mono are separate fonts" ) {
                CHECK( io.Fonts->Fonts.Size == 2 );
            }
        }
    }
    GIVEN( "a list whose first face is missing" ) {
        const std::vector<font_config> mono = {
            font_config( "data/font/no_such_face.ttf" ), font_config( "data/font/Terminus.ttf" )
        };
        THEN( "next face starts the font instead of merging into the previous one" ) {
            cataimgui::add_cata_fonts( io, gui, mono, false );
            CHECK( io.Fonts->Fonts.Size == 3 );
        }
    }
}
#endif
