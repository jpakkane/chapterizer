/*
 * Copyright 2022 Jussi Pakkanen
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <ft2build.h>
#include FT_FREETYPE_H

#include <cairo-pdf.h>

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

void draw_letter(cairo_t *cr, char l, double xoffset) {
    char tmp[2] = {l, 0};
    cairo_save(cr);
    cairo_move_to(cr, 50 + 1.15 * xoffset / 220 * 595, 100);
    cairo_show_text(cr, tmp);
    cairo_restore(cr);
}

int main() {
    const int FONTSIZE = 12;
    FT_Library library;
    FT_Error ec;
    FT_Face face;
    cairo_surface_t *surf = cairo_pdf_surface_create("cairotest.pdf", 595, 842);
    cairo_t *cr = cairo_create(surf);
    cairo_select_font_face(cr, "Gentium", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, FONTSIZE);
    ec = FT_Init_FreeType(&library);
    CHECKFT(ec);
    ec = FT_New_Face(library, "/usr/share/fonts/truetype/gentium/Gentium-R.ttf", 0, &face);
    CHECKFT(ec);
    printf("Font has kerning %d.\n", FT_HAS_KERNING(face));
    ec = FT_Set_Char_Size(face, 0, FONTSIZE * 64, 300, 300);
    CHECKFT(ec);
    const std::string testword{
        "kenoiva ja jonka vetolujuus oli terasvaijerin tasoa. Kestavyys tuli "
        "tarpeeseen, silla kyseinen"};
    std::string_view latter{testword};
    latter.remove_prefix(1);
    auto glyph_index = FT_Get_Char_Index(face, testword[0]);
    assert(glyph_index != 0);
    ec = FT_Load_Glyph(face, glyph_index, FT_LOAD_TARGET_MONO);
    CHECKFT(ec);
    draw_letter(cr, testword[0], 0);
    double total_w = glyph2mm(face);
    auto previous_index = glyph_index;
    FT_Vector v;
    for(const auto c : latter) {
        glyph_index = FT_Get_Char_Index(face, c);
        ec = FT_Get_Kerning(face, previous_index, glyph_index, FT_KERNING_DEFAULT, &v);
        CHECKFT(ec);
        if(v.x != 0) {
            printf("Non-zero kerning pair.\n");
            total_w += ftmetric2mm(v.x);
        }
        draw_letter(cr, c, total_w);
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

    cairo_move_to(cr, 100, 100);
    cairo_surface_destroy(surf);
    cairo_destroy(cr);
}
