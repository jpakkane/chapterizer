#include <ft2build.h>
#include FT_FREETYPE_H

#include <string>

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

template<typename T> double ftmetric2mm(T v) { return v / 300 * 25.4 / 64; }
double glyph2mm(FT_Face face) { return ftmetric2mm(face->glyph->advance.x); }

int main() {
    FT_Library library;
    FT_Error ec;
    FT_Face face;
    ec = FT_Init_FreeType(&library);
    CHECKFT(ec);
    ec = FT_New_Face(
        library, "/usr/share/fonts/truetype/liberation/LiberationSerif-Regular.ttf", 0, &face);
    CHECKFT(ec);
    printf("Font has kerning %d.\n", FT_HAS_KERNING(face));
    ec = FT_Set_Char_Size(face, 0, 12 * 64, 300, 300);
    CHECKFT(ec);
    const std::string testword{"Maapallon keskushallinto Omniviraatin avaruusvoimien"};
    std::string_view latter{testword};
    latter.remove_prefix(1);
    auto glyph_index = FT_Get_Char_Index(face, testword[0]);
    assert(glyph_index != 0);
    ec = FT_Load_Glyph(face, glyph_index, FT_LOAD_TARGET_MONO);
    CHECKFT(ec);
    double total_w = glyph2mm(face);
    auto previous_index = glyph_index;
    FT_Vector v;
    for(const auto c : latter) {
        glyph_index = FT_Get_Char_Index(face, c);
        ec = FT_Get_Kerning(face, previous_index, glyph_index, FT_KERNING_DEFAULT, &v);
        CHECKFT(ec);
        total_w += ftmetric2mm(v.x);
        ec = FT_Load_Glyph(face, glyph_index, FT_LOAD_TARGET_MONO);
        CHECKFT(ec);
        total_w += glyph2mm(face);
        previous_index = glyph_index;
    }
    printf("Total width of text: %.2f.\n", total_w);
    FT_Done_Face(face);
    CHECKFT(ec);
    FT_Done_FreeType(library);
    CHECKFT(ec);
}
