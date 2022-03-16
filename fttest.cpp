#include <ft2build.h>
#include FT_FREETYPE_H

#include <cstdlib>
#include <cassert>
#define CHECKFT(ec)                                                                                \
    if(ec) {                                                                                       \
        fterror(ec);                                                                               \
    }

void fterror(FT_Error ec) {
    printf("%s\n", FT_Error_String(ec));
    exit(-1);
}

int main() {
    FT_Library library;
    FT_Error ec;
    FT_Face face;
    ec = FT_Init_FreeType(&library);
    CHECKFT(ec);
    ec = FT_New_Face(
        library, "/usr/share/fonts/truetype/liberation/LiberationSerif-Regular.ttf", 0, &face);
    CHECKFT(ec);
    ec = FT_Set_Char_Size(face, 0, 12 * 64, 300, 300);
    CHECKFT(ec);
    auto glyph_index = FT_Get_Char_Index(face, int('O'));
    assert(glyph_index != 0);
    ec = FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT);
    CHECKFT(ec);
    printf("Advance: %f\n", face->glyph->advance.x / 300 * 2.54 / 32);
    FT_Done_Face(face);
    CHECKFT(ec);
    FT_Done_FreeType(library);
    CHECKFT(ec);
}
