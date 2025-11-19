/*
 * Copyright 2022-2025 Jussi Pakkanen
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

#include <draftpaginator.hpp>
#include <utils.hpp>
#include <chaptercommon.hpp>
#include <draftparagraphformatter.hpp>
#include <wordhyphenator.hpp>
#include <bookparser.hpp>

#include <cassert>

namespace {

const char *wordcount_str[3] = {"<undef>", "%d words", "%d sanaa"};

const int image_dpi = 600;

const Length image_separator = Length::from_mm(4);

template<typename T> void style_change(T &stack, typename T::value_type val) {
    if(stack.contains(val)) {
        stack.pop(val);
    } else {
        stack.push(val);
    }
}

void adjust_y(HBTextCommands &c, Length diff) {
    if(std::holds_alternative<HBRunDrawCommand>(c)) {
        auto &mc = std::get<HBRunDrawCommand>(c);
        mc.y += diff;
    } else {
        std::abort();
    }
}

// FIXME, move to setup_draft_settings.
HBChapterStyles build_default_styles() {
    HBChapterStyles def;
    HBTextParameters basic_font{
        Length::from_pt(12),
        HBFontProperties{TextCategory::Serif, TextStyle::Regular, TextExtra::None}};
    HBTextParameters code_font{
        Length::from_pt(10),
        HBFontProperties{TextCategory::Monospace, TextStyle::Regular, TextExtra::None}};
    HBTextParameters section_font{
        Length::from_pt(14),
        HBFontProperties{TextCategory::SansSerif, TextStyle::Bold, TextExtra::None}};

    def.normal = HBChapterParameters{Length::from_pt(20), Length::from_mm(10), basic_font};
    def.normal_noindent = def.normal;
    def.normal_noindent.indent = Length::zero();

    def.code = def.normal;
    def.code.font = code_font;

    def.colophon = def.normal;
    def.dedication = def.normal;
    def.footnote = def.normal;
    def.lists = def.normal;
    def.letter = def.normal;
    def.letter.font.par.style = TextStyle::Italic;

    def.section.font = section_font;
    def.section.line_height = Length::from_pt(25);

    def.title = def.section;
    def.author = def.section;
    def.author.font.par.style = TextStyle::Regular;

    return def;
}

} // namespace

// NOTE: mutates the input words.
std::vector<FormattingChange> extract_styling(StyleStack &current_style, std::string &word) {
    std::vector<FormattingChange> changes;
    std::string buf;
    const char *word_start = word.c_str();
    const char *in = word_start;
    int num_changes = 0;

    while(*in) {
        auto c = g_utf8_get_char(in);

        switch(c) {
        case italic_codepoint:
            style_change(current_style, ITALIC_S);
            changes.push_back(FormattingChange{size_t(in - word_start - num_changes), ITALIC_S});
            ++num_changes;
            break;
        case bold_codepoint:
            style_change(current_style, BOLD_S);
            changes.push_back(FormattingChange{size_t(in - word_start - num_changes), BOLD_S});
            ++num_changes;
            break;
        case tt_codepoint:
            style_change(current_style, TT_S);
            changes.push_back(FormattingChange{size_t(in - word_start - num_changes), TT_S});
            ++num_changes;
            break;
        case smallcaps_codepoint:
            style_change(current_style, SMALLCAPS_S);
            changes.push_back(FormattingChange{size_t(in - word_start - num_changes), SMALLCAPS_S});
            ++num_changes;
            break;
        case superscript_codepoint:
            style_change(current_style, SUPERSCRIPT_S);
            changes.push_back(
                FormattingChange{size_t(in - word_start - num_changes), SUPERSCRIPT_S});
            ++num_changes;
            break;
        case subscript_codepoint:
            style_change(current_style, SUBSCRIPT_S);
            changes.push_back(FormattingChange{size_t(in - word_start - num_changes), SUBSCRIPT_S});
            ++num_changes;
            break;
        default:
            char tmp[10];
            const int bytes_written = g_unichar_to_utf8(c, tmp);
            tmp[bytes_written] = '\0';
            buf += tmp;
        }
        in = g_utf8_next_char(in);
    }
    word = buf;
    return changes;
}

DraftPaginator::DraftPaginator(const Document &d)
    : doc(d), page(doc.data.pdf.page), styles(build_default_styles()), spaces(d.data.pdf.spaces),
      m(doc.data.pdf.margins) {
    if(!doc.data.is_draft) {
        fprintf(stderr, "Tried to create draft output in non-draft mode.\n");
        std::abort();
    }
}

void DraftPaginator::generate_pdf(const char *outfile) {
    capypdf::DocumentProperties dprop;
    capypdf::PageProperties pprop;

    pprop.set_pagebox(CAPY_BOX_MEDIA, 0, 0, page.w.pt(), page.h.pt());
    dprop.set_default_page_properties(pprop);
    dprop.set_title(doc.data.title);
    dprop.set_author(doc.data.author);
    dprop.set_creator("SuperPDF from outer space!");
    assert(doc.data.is_draft);

    rend.reset(new CapyPdfRenderer(outfile, page.w, page.h, Length::zero(), dprop, fc));

    const bool only_maintext = false;

    if(!only_maintext) {
        create_draft_title_page();
        new_page(false);
    }
    create_maintext();

    while(!layout.empty()) {
        render_page_num(styles.normal);
        flush_draw_commands();
    }
    rend->new_page();
    rend.reset(nullptr);
}

void DraftPaginator::create_maintext() {
    const Length bottom_watermark = page.h - m.lower - m.upper;
    Length rel_y; // Relative to top margin.
    bool first_paragraph = true;
    bool first_section = true;

    for(const auto &e : doc.elements) {
        if(std::holds_alternative<Section>(e)) {
            create_section(std::get<Section>(e), rel_y, first_section, first_paragraph);
        } else if(std::holds_alternative<Paragraph>(e)) {
            create_paragraph(std::get<Paragraph>(e),
                             rel_y,
                             bottom_watermark,
                             first_paragraph ? styles.normal_noindent : styles.normal,
                             Length::zero());
            first_paragraph = false;
        } else if(std::holds_alternative<Footnote>(e)) {
            if(!layout.footnote.empty() || !pending_footnotes.empty()) {
                printf("More than one footnote per page is not yet supported.");
                std::abort();
            }
            create_footnote(std::get<Footnote>(e), bottom_watermark);
        } else if(std::holds_alternative<SceneChange>(e)) {
            layout.text.emplace_back(SimpleTextDrawCommand{
                "#", styles.normal.font, textblock_width() / 2, rel_y, TextAlignment::Centered});
            rel_y -= styles.normal.line_height;
            heights.whitespace_height += styles.normal.line_height;
            if(rel_y >= bottom_watermark) {
                new_page(true);
                rel_y = Length::zero();
            }
            first_paragraph = true;
        } else if(std::holds_alternative<CodeBlock>(e)) {
            const CodeBlock &cb = std::get<CodeBlock>(e);
            rel_y -= spaces.different_paragraphs;
            heights.whitespace_height += spaces.different_paragraphs;
            for(const auto &line : cb.raw_lines) {
                if(heights.total_height() >= bottom_watermark) {
                    new_page(true);
                    rel_y = Length::zero();
                }
                layout.text.emplace_back(SimpleTextDrawCommand{line.c_str(),
                                                               styles.code.font,
                                                               spaces.codeblock_indent,
                                                               rel_y,
                                                               TextAlignment::Left});
                rel_y -= styles.code.line_height;
                heights.text_height += styles.code.line_height;
            }
            first_paragraph = true;
            rel_y -= spaces.different_paragraphs;
            heights.whitespace_height += spaces.different_paragraphs;
        } else if(std::holds_alternative<Letter>(e)) {
            const Letter &l = std::get<Letter>(e);
            rel_y -= spaces.different_paragraphs;
            heights.whitespace_height += spaces.different_paragraphs;
            first_paragraph = true;
            for(const auto &partext : l.paragraphs) {
                Paragraph p{partext};
                create_paragraph(
                    p, rel_y, bottom_watermark, styles.letter, doc.data.pdf.spaces.letter_indent);
                first_paragraph = false;
            }
            rel_y -= spaces.different_paragraphs;
            heights.whitespace_height += spaces.different_paragraphs;
            first_paragraph = true;
        } else if(std::holds_alternative<Figure>(e)) {
            const Figure &cb = std::get<Figure>(e);
            const auto fullpath = doc.data.top_dir / cb.file;
            auto image = rend->get_image(fullpath);
            Length display_width = Length::from_mm(double(image.w) / image_dpi * 25.4);
            Length display_height = Length::from_mm(double(image.h) / image_dpi * 25.4);
            display_height = display_height / 2;
            display_width = display_width / 2;
            if(chapter_start_page == rend->page_num()) {
                add_pending_figure(image);
            } else if(heights.figure_height > Length::zero()) {
                add_pending_figure(image);
            } else if(heights.total_height() + display_height + image_separator >
                      bottom_watermark) {
                add_pending_figure(image);
            } else {
                add_top_image(image);
            }
        } else if(std::holds_alternative<NumberList>(e)) {
            // FIXME: expand commands like \footnote{1} to values.
            const NumberList &nl = std::get<NumberList>(e);
            create_numberlist(nl, rel_y);
        } else if(std::holds_alternative<SignBlock>(e)) {
            const SignBlock &sign = std::get<SignBlock>(e);
            rel_y += spaces.different_paragraphs;
            heights.whitespace_height += spaces.different_paragraphs;
            for(const auto &line : sign.raw_lines) {
                if(heights.total_height() >= bottom_watermark) {
                    new_page(true);
                    rel_y = Length::zero();
                }
                // FIXME, should use small caps.
                layout.text.emplace_back(SimpleTextDrawCommand{line,
                                                               styles.normal.font,
                                                               textblock_width() / 2,
                                                               rel_y,
                                                               TextAlignment::Centered});
                rel_y += styles.code.line_height;
                heights.text_height += styles.code.line_height;
            }
            first_paragraph = true;
            rel_y += spaces.different_paragraphs;
            heights.whitespace_height += spaces.different_paragraphs;
        } else {
            printf("Unknown element in document array.\n");
            std::abort();
        }
    }
}

void DraftPaginator::create_section(const Section &s,
                                    Length &rel_y,
                                    bool &first_section,
                                    bool &first_paragraph) {
    const auto paragraph_width = page.w - m.inner - m.outer;
    const auto section_width = 0.8 * paragraph_width;
    printf("Processing section: %s\n", s.text.c_str());
    if(!first_section) {
        new_page(true);
    }
    chapter_start_page = rend->page_num();
    rend->add_section_outline(s.number, s.text);
    first_section = false;
    rel_y = Length::zero();
    rel_y -= spaces.above_section;
    heights.whitespace_height += spaces.above_section;
    assert(s.level == 1);
    // Fancy stuff above the text.
    std::string title_string;
    TextAlignment section_alignment = TextAlignment::Centered;
    title_string = std::to_string(s.number);
    title_string += ". ";
    title_string += s.text;
    section_alignment = TextAlignment::Left;
    // The title. Hyphenation is prohibited.
    std::vector<EnrichedWord> processed_words = text_to_formatted_words(title_string, false);
    DraftParagraphFormatter b(processed_words, section_width, styles.section, fc);
    auto lines = b.split_formatted_lines_to_runs();
    auto built_lines =
        build_ragged_paragraph(lines, styles.section, section_alignment, Length::zero(), rel_y);
    for(auto &line : built_lines) {
        layout.text.emplace_back(std::move(line));
        rel_y -= styles.section.line_height;
        heights.text_height += styles.section.line_height;
    }
    rel_y -= spaces.below_section;
    heights.text_height += spaces.below_section;
    first_paragraph = true;
}

void DraftPaginator::create_paragraph(const Paragraph &p,
                                      Length &rel_y,
                                      const Length &bottom_watermark,
                                      const HBChapterParameters &chpar,
                                      Length extra_indent) {
    const auto paragraph_width = textblock_width() - 2 * extra_indent;
    std::vector<EnrichedWord> processed_words = text_to_formatted_words(p.text);
    DraftParagraphFormatter b(processed_words, paragraph_width, chpar, fc);
    auto lines = b.split_formatted_lines_to_runs();
    std::vector<HBTextCommands> built_lines;
    built_lines =
        build_ragged_paragraph(lines, chpar, TextAlignment::Left, Length::zero(), Length::zero());
    if(!built_lines.empty()) {
        auto &first_line = built_lines[0];
        if(std::holds_alternative<SimpleTextDrawCommand>(first_line)) {
            std::get<SimpleTextDrawCommand>(first_line).x += chpar.indent;
        } else if(std::holds_alternative<HBRunDrawCommand>(first_line)) {
            std::get<HBRunDrawCommand>(first_line).x += chpar.indent;
        } else {
            std::abort();
        }
    }
    // Shift sideways
    for(auto &line : built_lines) {
        if(std::holds_alternative<SimpleTextDrawCommand>(line)) {
            std::get<SimpleTextDrawCommand>(line).x += extra_indent;
        } else if(std::holds_alternative<HBRunDrawCommand>(line)) {
            std::get<HBRunDrawCommand>(line).x += extra_indent;
        } else {
            std::abort();
        }
    }
    Length current_y_origin = rel_y;
    int lines_in_paragraph = 0;
    for(auto &line : built_lines) {
        if(heights.total_height() + chpar.line_height > bottom_watermark) {
            new_page(true);
            current_y_origin = lines_in_paragraph * chpar.line_height;
            rel_y = Length::zero();
        }
        ++lines_in_paragraph;
        layout.text.emplace_back(std::move(line));
        adjust_y(layout.text.back(), current_y_origin);
        rel_y -= chpar.line_height;
        heights.text_height += chpar.line_height;
    }
}

void DraftPaginator::create_footnote(const Footnote &f, const Length &bottom_watermark) {
    const auto paragraph_width = page.w - m.inner - m.outer;
    heights.whitespace_height += spaces.footnote_separation;
    std::vector<EnrichedWord> processed_words = text_to_formatted_words(f.text);
    const Length number_indent = Length::from_pt(16);
    DraftParagraphFormatter b(
        processed_words, paragraph_width - number_indent, styles.footnote, fc);
    auto lines = b.split_formatted_lines_to_runs();
    std::string fnum = std::to_string(f.number);
    fnum += '.';
    auto tmpy = heights.footnote_height;
    auto built_lines = build_ragged_paragraph(
        lines, styles.footnote, TextAlignment::Left, number_indent, Length::zero());
    const auto footnote_total_height = built_lines.size() * styles.footnote.line_height;
    // FIXME, split the footnote over two pages.
    if(heights.total_height() + footnote_total_height >= bottom_watermark) {
        pending_footnotes.emplace_back(SimpleTextDrawCommand{
            std::move(fnum), styles.footnote.font, Length::zero(), tmpy, TextAlignment::Left});
        pending_footnotes.insert(pending_footnotes.end(), built_lines.begin(), built_lines.end());
    } else {
        // FIXME: draw in flush_commands instead?
        layout.footnote.emplace_back(SimpleTextDrawCommand{
            std::move(fnum), styles.footnote.font, Length::zero(), tmpy, TextAlignment::Left});
        heights.footnote_height += footnote_total_height;
        layout.footnote.insert(layout.footnote.end(), built_lines.begin(), built_lines.end());
    }
}

void DraftPaginator::create_numberlist(const NumberList &nl, Length &rel_y) {
    const auto paragraph_width = page.w - m.inner - m.outer;

    const Length number_area = Length::from_mm(5);
    const Length indent = spaces.codeblock_indent; // FIXME
    const Length text_width = paragraph_width - number_area - 2 * indent;
    const Length item_separator = spaces.different_paragraphs / 2;
    rel_y -= spaces.different_paragraphs;
    heights.whitespace_height += spaces.different_paragraphs;
    for(size_t i = 0; i < nl.items.size(); ++i) {
        if(i != 0) {
            rel_y -= item_separator;
            heights.whitespace_height += item_separator;
        }
        std::vector<EnrichedWord> processed_words = text_to_formatted_words(nl.items[i]);
        DraftParagraphFormatter b(processed_words, text_width, styles.lists, fc);
        auto lines = b.split_formatted_lines_to_runs();
        std::string fnum = std::to_string(i + 1);
        fnum += '.';
        layout.text.emplace_back(SimpleTextDrawCommand{
            std::move(fnum), styles.lists.font, indent, rel_y, TextAlignment::Left});
        for(auto &line : build_ragged_paragraph(lines,
                                                styles.lists,
                                                TextAlignment::Left, // FIXME
                                                indent + number_area,
                                                rel_y)) {
            // FIXME, handle page changes.
            layout.text.emplace_back(std::move(line));
            heights.text_height += styles.lists.line_height;
            rel_y -= styles.lists.line_height;
        }
    }
    rel_y -= spaces.different_paragraphs;
    heights.whitespace_height += spaces.different_paragraphs;
}

void DraftPaginator::add_top_image(const CapyImageInfo &image) {
    CapyImageCommand cmd;
    cmd.i = image;
    cmd.display_width = Length::from_mm(double(image.w) / image_dpi * 25.4);
    cmd.display_height = Length::from_mm(double(image.h) / image_dpi * 25.4);
    cmd.display_height = cmd.display_height / 2;
    cmd.display_width = cmd.display_width / 2;
    cmd.x = textblock_width() / 2 - cmd.display_width / 2;
    cmd.y = page.h - m.upper - cmd.display_height;
    layout.images.emplace_back(std::move(cmd));
    assert(heights.figure_height < Length::from_mm(0.0001));
    heights.figure_height += cmd.display_height;
    heights.figure_height += image_separator;
    layout.images.emplace_back(std::move(cmd));
    //  printf("Image is %d PPI\n", int(image.w / display_width.v * 25.4));
    //   heights.figure_height += display_height;
}

void DraftPaginator::render_page_num(const HBChapterParameters &par) {
    // In the official draft style the first page does not have a number
    // so logical page numbers are offset from physical pages by one.
    std::string text = doc.data.draftdata.page_number_template + std::to_string(current_page - 1);
    rend->render_text(text.c_str(),
                      par.font,
                      current_left_margin() + textblock_width(),
                      page.h - (m.upper - 2 * par.line_height),
                      TextAlignment::Right);
}

std::vector<HBTextCommands>
DraftPaginator::build_justified_paragraph(const std::vector<std::vector<std::string>> &lines,
                                          const HBChapterParameters &text_par,
                                          const Length target_width,
                                          const Length x_off,
                                          const Length y_off) {
    // This should really be removed, because draft versions never have justified
    // text. For now leave it in due to laziness and bw compatibility.
    Length rel_y;
    const Length x;
    std::vector<HBTextCommands> line_commands;
    line_commands.reserve(lines.size());
    size_t line_num = 0;
    for(const auto &markup_words : lines) {
        Length current_indent = line_num == 0 ? text_par.indent : Length{};
        std::string full_line;
        for(const auto &w : markup_words) {
            full_line += w;
        }
        // FIXME, not correct.
        line_commands.emplace_back(SimpleTextDrawCommand{std::move(full_line),
                                                         text_par.font,
                                                         x + current_indent + x_off,
                                                         rel_y + y_off,
                                                         TextAlignment::Left});
        line_num++;
        rel_y -= text_par.line_height;
    }
    return line_commands;
}

std::vector<HBTextCommands>
DraftPaginator::build_ragged_paragraph(const std::vector<std::vector<std::string>> &lines,
                                       const HBChapterParameters &text_par,
                                       const TextAlignment alignment,
                                       Length rel_y) {
    std::vector<HBTextCommands> line_commands;
    const auto rel_x =
        alignment == TextAlignment::Centered ? textblock_width() / 2 : Length::zero();

    line_commands.reserve(lines.size());
    for(const auto &markup_words : lines) {
        std::string full_line;
        for(const auto &w : markup_words) {
            full_line += w;
        }
        assert(alignment != TextAlignment::Right);
        line_commands.emplace_back(
            SimpleTextDrawCommand{std::move(full_line), text_par.font, rel_x, rel_y, alignment});
        rel_y -= text_par.line_height;
    }
    return line_commands;
}

std::vector<HBTextCommands>
DraftPaginator::build_ragged_paragraph(const std::vector<std::vector<HBRun>> &lines,
                                       const HBChapterParameters &text_par,
                                       const TextAlignment alignment,
                                       Length extra_x,
                                       Length rel_y) {
    std::vector<HBTextCommands> line_commands;
    const auto rel_x =
        extra_x + (alignment == TextAlignment::Centered ? textblock_width() / 2 : Length::zero());
    line_commands.reserve(lines.size());
    for(const auto &runs : lines) {
        assert(alignment != TextAlignment::Right);
        line_commands.emplace_back(HBRunDrawCommand{std::move(runs), rel_x, rel_y, alignment});
        rel_y -= text_par.line_height;
    }
    return line_commands;
}

std::vector<EnrichedWord> DraftPaginator::text_to_formatted_words(const std::string &text,
                                                                  bool permit_hyphenation) {
    StyleStack current_style("dummy", Length::from_pt(10));
    auto plain_words = split_to_words(std::string_view(text));
    std::vector<EnrichedWord> processed_words;
    const Language lang = permit_hyphenation ? doc.data.language : Language::Unset;
    for(const auto &word : plain_words) {
        auto working_word = word;
        auto start_style = current_style;
        auto formatting_data = extract_styling(current_style, working_word);
        restore_special_chars(working_word);
        auto hyphenation_data = hyphen.hyphenate(working_word, lang);
        processed_words.emplace_back(EnrichedWord{std::move(working_word),
                                                  std::move(hyphenation_data),
                                                  std::move(formatting_data),
                                                  start_style});
    }
    return processed_words;
}

void DraftPaginator::new_page(bool draw_page_num) {
    flush_draw_commands();
    if(draw_page_num) {
        render_page_num(styles.normal);
    }
    rend->new_page();
    if(!pending_figures.empty()) {
        const auto image_only_page_height = 0.6 * textblock_height();
        // FIXME. Only one figure per page. Could have several.
        assert(layout.images.empty());
        add_top_image(pending_figures.front());
        pending_figures.erase(pending_figures.begin());
        assert(!layout.images.empty());
        if(layout.images.front().display_height > image_only_page_height) {
            ++current_page;
            new_page(draw_page_num);
            return;
        }
    }
    ++current_page;
    if(!pending_footnotes.empty()) {
        assert(layout.footnote.empty());
        layout.footnote = std::move(pending_footnotes);
        pending_footnotes.clear();
        heights.footnote_height += layout.footnote.size() * styles.footnote.line_height +
                                   doc.data.pdf.spaces.footnote_separation;
    }
}

void DraftPaginator::draw_debug_bars(int num_bars, const Length bar_start_y) {
    const Length boxheight = styles.section.line_height;
    const Length chaffwidth = Length::from_mm(6);
    for(int i = 0; i < num_bars; ++i) {
        rend->fill_box(current_left_margin(),
                       bar_start_y + 2 * i * boxheight,
                       textblock_width(),
                       boxheight,
                       0.85);
    }
    rend->draw_box(current_left_margin() - chaffwidth,
                   bar_start_y,
                   textblock_width() + 2 * chaffwidth,
                   2 * num_bars * boxheight,
                   0.5,
                   Length::from_pt(0.1));
    // Text area box
    const double line_width = 0.4;
    std::vector<Coord> dashcoords;
    dashcoords.emplace_back(Coord{current_left_margin(), bar_start_y});
    dashcoords.emplace_back(Coord{current_left_margin(), bar_start_y + 2 * num_bars * boxheight});
    rend->draw_dash_line(dashcoords, line_width);
    dashcoords.clear();
    dashcoords.emplace_back(Coord{current_left_margin() + textblock_width(), bar_start_y});
    dashcoords.emplace_back(
        Coord{current_left_margin() + textblock_width(), bar_start_y + 2 * num_bars * boxheight});
    rend->draw_dash_line(dashcoords, line_width);
    dashcoords.clear();

    const auto hole_center_x = current_left_margin() - chaffwidth / 2;
    const auto hole_center_y = bar_start_y + boxheight / 2.0;
    const auto hole_center_deltax = textblock_width() + chaffwidth;
    const auto hole_center_deltay = 2 * boxheight;
    const Length minor_radius = Length::from_mm(1.5);
    const Length major_radius = Length::from_mm(1.0);
    const int num_corners = 8;
    for(int xind = 0; xind < 2; ++xind) {
        for(int yind = 0; yind < num_bars; ++yind) {
            dashcoords.clear();
            for(int i = 0; i < (2 * num_corners + 1); ++i) {
                const auto &current_radius = i % 2 ? major_radius : minor_radius;
                dashcoords.emplace_back(
                    Coord{hole_center_x + xind * hole_center_deltax +
                              current_radius * cos(2 * M_PI * i / (num_corners * 2.0)),
                          hole_center_y + yind * hole_center_deltay +
                              current_radius * sin(2 * M_PI * i / (num_corners * 2.0))});
            }
            rend->draw_poly_line(dashcoords, Length::from_pt(0.2));
        }
    }
}

void DraftPaginator::flush_draw_commands() {
    const bool draw_cut_guide = false;
    const bool draw_textarea_box = false;
    Length footnote_block_start = m.lower + heights.footnote_height;
    if(doc.data.debug_draw && !doc.data.is_draft && current_page == chapter_start_page) {
        const Length bar_start_y = m.upper + spaces.above_section - 2 * styles.section.line_height;
        draw_debug_bars(4, bar_start_y);
    }
    if(draw_textarea_box) {
        rend->draw_box(current_left_margin(),
                       m.upper,
                       textblock_width(),
                       textblock_height(),
                       0.5,
                       Length::from_pt(0.1));
    }
    if(draw_cut_guide) {
        const Length cut_x =
            current_page % 2 ? Length::from_mm(0.2) : page.w - Length::from_mm(0.2);
        rend->draw_line(cut_x, Length::zero(), cut_x, page.h, Length::from_mm(0.1));
    }
    for(const auto &c : layout.images) {
        rend->draw_image(c.i, c.x + current_left_margin(), c.y, c.display_width, c.display_height);
    }
    for(const auto &c : layout.text) {
        if(std::holds_alternative<SimpleTextDrawCommand>(c)) {
            const auto &md = std::get<SimpleTextDrawCommand>(c);
            rend->render_text(md.text.c_str(),
                              md.par,
                              md.x + current_left_margin(),
                              page.h - m.upper - heights.figure_height + md.y,
                              md.alignment);
        } else if(std::holds_alternative<HBRunDrawCommand>(c)) {
            const auto &md = std::get<HBRunDrawCommand>(c);
            rend->render_runs(md.runs,
                              md.x + current_left_margin(),
                              page.h - m.upper - heights.figure_height + md.y,
                              md.alignment);
        } else {
            printf("Unknown draw command.\n");
        }
    }

    for(const auto &c : layout.footnote) {
        if(std::holds_alternative<SimpleTextDrawCommand>(c)) {
            const auto &md = std::get<SimpleTextDrawCommand>(c);
            rend->render_text(md.text.c_str(),
                              md.par,
                              current_left_margin() + md.x,
                              md.y + footnote_block_start,
                              md.alignment);
        } else if(std::holds_alternative<HBRunDrawCommand>(c)) {
            const auto &md = std::get<HBRunDrawCommand>(c);
            rend->render_runs(
                md.runs, current_left_margin() + md.x, md.y + footnote_block_start, md.alignment);
        } else {
            printf("Unknown draw command.\n");
        }
    }
    if(!layout.footnote.empty()) {
        const Length line_thickness = Length::from_pt(0.8);
        const Length line_distance = doc.data.pdf.spaces.footnote_separation;
        const Length line_indent = Length::from_mm(-5);
        const Length line_width = Length::from_mm(20);
        const Length &baseline_correction = styles.footnote.line_height;
        const Length x0 = current_left_margin() + line_indent;
        const Length y0 = footnote_block_start + 0.3 * line_distance + 0.7 * baseline_correction;
        rend->draw_line(x0, y0, x0 + line_width, y0, line_thickness);
    }
    layout.clear();
    heights.clear();
}

void DraftPaginator::add_pending_figure(const CapyImageInfo &f) { pending_figures.push_back(f); }

int DraftPaginator::count_words() {
    int num_words = 0;
    for(const auto &f : doc.data.sources) {
        const auto full_path = doc.data.top_dir / f;
        num_words += words_in_file(full_path.c_str());
    }
    if(num_words < 1000)
        return num_words;
    return ((num_words + 500) / 1000) * 1000;
}

void DraftPaginator::create_draft_title_page() {
    const int num_words = count_words();
    const auto middle = current_left_margin() + textblock_width() / 2;
    auto textblock_center = page.h - (m.upper + textblock_height() / 2);
    auto y = page.h - m.upper;
    const auto single_line_height = styles.normal.font.size * 1.5;
    const auto left_edge = current_left_margin();
    const auto right_edge = left_edge + textblock_width();
    const int bufsize = 128;
    char buf[bufsize];
    snprintf(buf, bufsize, wordcount_str[(int)doc.data.language], num_words);
    rend->render_text_as_is(doc.data.author.c_str(), styles.normal.font, left_edge, y);
    rend->render_text(buf, styles.normal.font, right_edge, y, TextAlignment::Right);
    y -= single_line_height;

    rend->render_text_as_is(doc.data.draftdata.phone.c_str(), styles.normal.font, left_edge, y);
    y -= single_line_height;
    rend->render_text_as_is(doc.data.draftdata.email.c_str(), styles.code.font, left_edge, y);

    y = textblock_center + 3 * styles.title.line_height;
    rend->render_text(
        doc.data.title.c_str(), styles.title.font, middle, y, TextAlignment::Centered);
    y -= 2 * styles.title.line_height;
    rend->render_text(
        doc.data.author.c_str(), styles.author.font, middle, y, TextAlignment::Centered);

    y -= styles.title.line_height;
    const std::string date = current_date();
    rend->render_text(date.c_str(), styles.author.font, middle, y, TextAlignment::Centered);
}
