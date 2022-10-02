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

#include <paginator.hpp>
#include <utils.hpp>
#include <chaptercommon.hpp>
#include <paragraphformatter.hpp>
#include <wordhyphenator.hpp>
#include <bookparser.hpp>

#include <cassert>

namespace {

const Millimeter image_separator = Millimeter::from_value(4);

template<typename T> void style_change(T &stack, typename T::value_type val) {
    if(stack.contains(val)) {
        stack.pop(val);
    } else {
        stack.push(val);
    }
}

void adjust_y(TextCommands &c, Millimeter diff) {
    if(std::holds_alternative<MarkupDrawCommand>(c)) {
        auto &mc = std::get<MarkupDrawCommand>(c);
        mc.y += diff;
    } else if(std::holds_alternative<JustifiedMarkupDrawCommand>(c)) {
        auto &mc = std::get<JustifiedMarkupDrawCommand>(c);
        mc.y += diff;
    } else {
        std::abort();
    }
}

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

} // namespace

Paginator::Paginator(const Document &d)
    : doc(d), page(doc.data.pdf.page), styles(d.data.pdf.styles), m(doc.data.pdf.margins) {}

void Paginator::generate_pdf(const char *outfile) {
    rend.reset(new PdfRenderer(
        outfile, page.w.topt(), page.h.topt(), doc.data.title.c_str(), doc.data.author.c_str()));

    const auto paragraph_width = page.w - m.inner - m.outer;
    const auto section_width = 0.8 * paragraph_width;

    ChapterParameters noindent_text_par = styles.normal;
    noindent_text_par.indent = Millimeter::zero();

    ExtraPenaltyAmounts extras;
    const Millimeter bottom_watermark = page.h - m.lower - m.upper;
    const Millimeter title_above_space = Millimeter::from_value(20);
    const Millimeter title_below_space = Millimeter::from_value(10);
    const Millimeter different_paragraph_space = Millimeter::from_value(3);
    const Millimeter codeblock_indent = Millimeter::from_value(10);
    Millimeter rel_y;
    const Millimeter footnote_separation = Millimeter::from_value(2);
    bool first_paragraph = true;
    bool first_section = true;

    for(const auto &e : doc.elements) {
        if(std::holds_alternative<Section>(e)) {
            const Section &s = std::get<Section>(e);
            if(!first_section) {
                new_page(true);
                if(current_page % 2 == 0) {
                    new_page(false);
                }
            }
            chapter_start_page = rend->page_num();
            first_section = false;
            rel_y = Millimeter::zero();
            rel_y += title_above_space;
            heights.whitespace_height += title_above_space;
            assert(s.level == 1);
            // Fancy stuff above the text.
            std::string title_number = std::to_string(s.number);
            title_number += ".";
            layout.text.emplace_back(MarkupDrawCommand{title_number,
                                                       &styles.section.font,
                                                       textblock_width() / 2,
                                                       rel_y,
                                                       TextAlignment::Centered});
            rel_y += 2 * styles.section.line_height.tomm();
            heights.text_height += 2 * styles.section.line_height.tomm();

            // The title. Hyphenation is prohibited.
            std::vector<EnrichedWord> processed_words = text_to_formatted_words(s.text, false);
            ParagraphFormatter b(processed_words, section_width, styles.section, extras);
            auto lines = b.split_formatted_lines();
            auto built_lines =
                build_ragged_paragraph(lines, styles.section, TextAlignment::Centered, rel_y);
            for(auto &line : built_lines) {
                layout.text.emplace_back(std::move(line));
                rel_y += styles.section.line_height.tomm();
                heights.text_height += styles.section.line_height.tomm();
            }
            rel_y += title_below_space;
            heights.text_height += title_below_space;
            first_paragraph = true;
        } else if(std::holds_alternative<Paragraph>(e)) {
            const Paragraph &p = std::get<Paragraph>(e);
            const ChapterParameters &cur_par = first_paragraph ? noindent_text_par : styles.normal;
            std::vector<EnrichedWord> processed_words = text_to_formatted_words(p.text);
            ParagraphFormatter b(processed_words, paragraph_width, cur_par, extras);
            auto lines = b.split_formatted_lines();
            auto built_lines = build_justified_paragraph(lines, cur_par);

            Millimeter current_y_origin = rel_y;
            int lines_in_paragraph = 0;
            for(auto &line : built_lines) {
                if(heights.total_height() + cur_par.line_height.tomm() > bottom_watermark) {
                    new_page(true);
                    current_y_origin = -lines_in_paragraph * cur_par.line_height.tomm();
                    rel_y = Millimeter::zero();
                }
                ++lines_in_paragraph;
                layout.text.emplace_back(std::move(line));
                adjust_y(layout.text.back(), current_y_origin);
                rel_y += cur_par.line_height.tomm();
                heights.text_height += cur_par.line_height.tomm();
            }
            first_paragraph = false;
        } else if(std::holds_alternative<Footnote>(e)) {
            if(!layout.footnote.empty()) {
                printf("More than one footnote per page is not yet supported.");
                std::abort();
            }
            const Footnote &f = std::get<Footnote>(e);
            heights.whitespace_height += footnote_separation;
            std::vector<EnrichedWord> processed_words = text_to_formatted_words(f.text);
            ParagraphFormatter b(processed_words, paragraph_width, styles.footnote, extras);
            auto lines = b.split_formatted_lines();
            std::string fnum = std::to_string(f.number);
            fnum += '.';
            auto tmpy = heights.footnote_height;
            layout.footnote.emplace_back(MarkupDrawCommand{std::move(fnum),
                                                           &styles.footnote.font,
                                                           Millimeter::zero(),
                                                           tmpy,
                                                           TextAlignment::Left});
            auto built_lines = build_justified_paragraph(lines, styles.footnote);
            // FIXME, assumes there is always enough space for a footnote.
            heights.footnote_height += built_lines.size() * styles.footnote.line_height.tomm();
            layout.footnote.insert(layout.footnote.end(), built_lines.begin(), built_lines.end());
        } else if(std::holds_alternative<SceneChange>(e)) {
            rel_y += styles.normal.line_height.tomm();
            heights.whitespace_height += styles.normal.line_height.tomm();
            if(rel_y >= bottom_watermark) {
                new_page(true);
                rel_y = Millimeter::zero();
            }
            first_paragraph = true;
        } else if(std::holds_alternative<CodeBlock>(e)) {
            const CodeBlock &cb = std::get<CodeBlock>(e);
            rel_y += different_paragraph_space;
            heights.whitespace_height += different_paragraph_space;
            for(const auto &line : cb.raw_lines) {
                if(heights.total_height() >= bottom_watermark) {
                    new_page(true);
                    rel_y = Millimeter::zero();
                }
                layout.text.emplace_back(MarkupDrawCommand{
                    line.c_str(), &styles.code.font, codeblock_indent, rel_y, TextAlignment::Left});
                rel_y += styles.code.line_height.tomm();
                heights.text_height += styles.code.line_height.tomm();
            }
            first_paragraph = true;
            rel_y += different_paragraph_space;
            heights.whitespace_height += different_paragraph_space;
        } else if(std::holds_alternative<Figure>(e)) {
            const Figure &cb = std::get<Figure>(e);
            const auto fullpath = doc.data.top_dir / cb.file;
            auto image = rend->get_image(fullpath.c_str());
            const Millimeter display_width = textblock_width();
            const Millimeter display_height = display_width * image.h / image.w;
            if(chapter_start_page == rend->page_num()) {
                add_pending_figure(image);
            } else if(heights.figure_height > Millimeter::zero()) {
                add_pending_figure(image);
            } else if(heights.total_height() + display_height + image_separator >
                      bottom_watermark) {
                add_pending_figure(image);
            } else {
                add_top_image(image);
                // rend->draw_image(image, current_left_margin(), m.upper, display_width,
                // display_height);
                //  printf("Image is %d PPI\n", int(image.w / display_width.v * 25.4));
                //   heights.figure_height += display_height;
            }
        } else {
            std::abort();
        }
    }
    if(!layout.empty()) {
        flush_draw_commands();
    }
    rend.reset(nullptr);
}

void Paginator::add_top_image(const ImageInfo &image) {
    ImageCommand cmd;
    cmd.i = image;
    cmd.display_width = textblock_width();
    cmd.display_height = cmd.display_width * image.h / image.w;
    layout.images.emplace_back(std::move(cmd));
    assert(heights.figure_height < Millimeter::from_value(0.0001));
    heights.figure_height += cmd.display_height;
    heights.figure_height += image_separator;
    layout.images.emplace_back(std::move(cmd));
}

void Paginator::render_page_num(const FontParameters &par) {
    char buf[128];
    snprintf(buf, 128, "%d", current_page);
    const Millimeter yloc = page.h - 2.0 * m.lower / 3.0;
    const Millimeter xloc = current_left_margin() + (page.w - m.inner - m.outer) / 2;
    rend->render_line_centered(buf, par, xloc.topt(), yloc.topt());
}

std::vector<TextCommands>
Paginator::build_justified_paragraph(const std::vector<std::vector<std::string>> &lines,
                                     const ChapterParameters &text_par) {
    Millimeter rel_y = Millimeter::zero();
    const Millimeter x = Millimeter::zero();
    std::vector<TextCommands> line_commands;
    line_commands.reserve(lines.size());
    size_t line_num = 0;
    for(const auto &markup_words : lines) {
        Millimeter current_indent = line_num == 0 ? text_par.indent : Millimeter{};
        if(line_num < lines.size() - 1) {
            line_commands.emplace_back(
                JustifiedMarkupDrawCommand{markup_words,
                                           &text_par.font,
                                           (x + current_indent),
                                           rel_y,
                                           textblock_width() - current_indent});
        } else {
            std::string full_line;
            for(const auto &w : markup_words) {
                full_line += w;
            }
            line_commands.emplace_back(MarkupDrawCommand{std::move(full_line),
                                                         &text_par.font,
                                                         x + current_indent,
                                                         rel_y,
                                                         TextAlignment::Left});
        }
        line_num++;
        rel_y += text_par.line_height.tomm();
    }
    return line_commands;
}

std::vector<TextCommands>
Paginator::build_ragged_paragraph(const std::vector<std::vector<std::string>> &lines,
                                  const ChapterParameters &text_par,
                                  const TextAlignment alignment,
                                  Millimeter rel_y) {
    std::vector<TextCommands> line_commands;
    line_commands.reserve(lines.size());
    for(const auto &markup_words : lines) {
        std::string full_line;
        for(const auto &w : markup_words) {
            full_line += w;
        }
        assert(alignment != TextAlignment::Right);
        line_commands.emplace_back(MarkupDrawCommand{
            std::move(full_line),
            &text_par.font,
            alignment == TextAlignment::Centered ? textblock_width() / 2 : Millimeter::zero(),
            rel_y,
            alignment});
        rel_y += text_par.line_height.tomm();
    }
    return line_commands;
}

std::vector<EnrichedWord> Paginator::text_to_formatted_words(const std::string &text,
                                                             bool permit_hyphenation) {
    StyleStack current_style;
    auto plain_words = split_to_words(std::string_view(text));
    std::vector<EnrichedWord> processed_words;
    const Language lang = permit_hyphenation ? doc.data.language : Language::Unset;
    for(const auto &word : plain_words) {
        auto working_word = word;
        auto start_style = current_style;
        auto formatting_data = extract_styling(current_style, working_word);
        auto hyphenation_data = hyphen.hyphenate(working_word, lang);
        processed_words.emplace_back(EnrichedWord{std::move(working_word),
                                                  std::move(hyphenation_data),
                                                  std::move(formatting_data),
                                                  start_style});
    }
    return processed_words;
}

void Paginator::new_page(bool draw_page_num) {
    flush_draw_commands();
    if(draw_page_num) {
        render_page_num(styles.normal.font);
    }
    rend->new_page();
    if(pending_figure) {
        add_top_image(*pending_figure);
        pending_figure.reset();
    }
    ++current_page;
    if(debug_draw) {
        if(current_page % 2) {
            rend->draw_box(m.inner.topt(),
                           m.upper.topt(),
                           (page.w - m.inner - m.outer).topt(),
                           (page.h - m.upper - m.lower).topt());
        } else {
            rend->draw_box(m.outer.topt(),
                           m.upper.topt(),
                           (page.w - m.inner - m.outer).topt(),
                           (page.h - m.upper - m.lower).topt());
        }
    }
}

void Paginator::flush_draw_commands() {
    Millimeter footnote_block_start = page.h - m.lower - heights.footnote_height;
    if(debug_draw && current_page == chapter_start_page) {
        const Point boxheight = Point::from_value(12);
        const Millimeter chaffwidth = Millimeter::from_value(6);
        for(int i = 0; i < 6; ++i) {
            rend->fill_box(current_left_margin().topt(),
                           m.upper.topt() + 2 * i * boxheight,
                           textblock_width().topt(),
                           boxheight,
                           0.9);
        }
        std::vector<Coord> dashcoords;
        dashcoords.emplace_back(Coord{current_left_margin().topt(), m.upper.topt()});
        dashcoords.emplace_back(
            Coord{(current_left_margin() + textblock_width()).topt(), m.upper.topt()});
        dashcoords.emplace_back(Coord{(current_left_margin() + textblock_width()).topt(),
                                      m.upper.topt() + 12 * boxheight});
        dashcoords.emplace_back(
            Coord{current_left_margin().topt(), m.upper.topt() + 12 * boxheight});
        dashcoords.emplace_back(Coord{current_left_margin().topt(), m.upper.topt()});
        rend->draw_dash_line(dashcoords);
        dashcoords.clear();

        dashcoords.emplace_back(Coord{current_left_margin().topt(), m.upper.topt()});
        dashcoords.emplace_back(Coord{(current_left_margin() - chaffwidth).topt(), m.upper.topt()});
        dashcoords.emplace_back(
            Coord{(current_left_margin() - chaffwidth).topt(), m.upper.topt() + 12 * boxheight});
        dashcoords.emplace_back(
            Coord{current_left_margin().topt(), m.upper.topt() + 12 * boxheight});
        rend->draw_dash_line(dashcoords);
        dashcoords.clear();
        const auto hole_center_x = current_left_margin() - chaffwidth / 2;
        const auto hole_center_y = m.upper + chaffwidth / 2.0;
        const Millimeter minor_radius = Millimeter::from_value(1.5);
        const Millimeter major_radius = Millimeter::from_value(1.0);
        const int num_corners = 8;
        for(int i = 0; i < (2 * num_corners + 1); ++i) {
            const auto &current_radius = i % 2 ? major_radius : minor_radius;
            dashcoords.emplace_back(Coord{
                (hole_center_x + current_radius * cos(2 * M_PI * i / (num_corners * 2.0))).topt(),
                (hole_center_y + current_radius * sin(2 * M_PI * i / (num_corners * 2.0))).topt()});
        }
        rend->draw_poly_line(dashcoords, Point::from_value(0.2));
    }
    for(const auto &c : layout.images) {
        rend->draw_image(c.i, current_left_margin(), m.upper, c.display_width, c.display_height);
    }
    for(const auto &c : layout.text) {
        if(std::holds_alternative<MarkupDrawCommand>(c)) {
            const auto &md = std::get<MarkupDrawCommand>(c);
            rend->render_markup_as_is(md.markup.c_str(),
                                      *md.font,
                                      (md.x + current_left_margin()).topt(),
                                      (md.y + m.upper + heights.figure_height).topt(),
                                      md.alignment);
        } else if(std::holds_alternative<JustifiedMarkupDrawCommand>(c)) {
            const auto &md = std::get<JustifiedMarkupDrawCommand>(c);
            rend->render_line_justified(md.markup_words,
                                        *md.font,
                                        md.width,
                                        (current_left_margin() + md.x).topt(),
                                        (md.y + m.upper + heights.figure_height).topt());
        } else {
            printf("Unknown draw command.\n");
        }
    }

    for(const auto &c : layout.footnote) {
        if(std::holds_alternative<MarkupDrawCommand>(c)) {
            const auto &md = std::get<MarkupDrawCommand>(c);
            rend->render_markup_as_is(md.markup.c_str(),
                                      *md.font,
                                      (current_left_margin() + md.x).topt(),
                                      (md.y + footnote_block_start).topt(),
                                      md.alignment);
        } else if(std::holds_alternative<JustifiedMarkupDrawCommand>(c)) {
            const auto &md = std::get<JustifiedMarkupDrawCommand>(c);
            rend->render_line_justified(md.markup_words,
                                        *md.font,
                                        md.width,
                                        (current_left_margin() + md.x).topt(),
                                        (md.y + footnote_block_start).topt());
        } else {
            printf("Unknown draw command.\n");
        }
    }
    if(!layout.footnote.empty()) {
        const Point line_thickness = Point::from_value(1);
        const Millimeter line_distance = Millimeter::from_value(2);
        const Millimeter line_indent = Millimeter::from_value(-5);
        const Millimeter line_width = Millimeter::from_value(20);
        const Millimeter x0 = m.inner + line_indent;
        const Millimeter y0 = footnote_block_start - line_distance;
        rend->draw_line(x0.topt(), y0.topt(), (x0 + line_width).topt(), y0.topt(), line_thickness);
    }
    layout.clear();
    heights.clear();
}

void Paginator::add_pending_figure(const ImageInfo &f) {
    if(pending_figure) {
        printf("Multiple pending figures not yet supported.\n");
        std::abort();
    }
    pending_figure = f;
}
