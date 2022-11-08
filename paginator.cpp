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

void adjust_y(TextCommands &c, Length diff) {
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
    : doc(d), page(doc.data.pdf.page), styles(d.data.pdf.styles), spaces(d.data.pdf.spaces),
      m(doc.data.pdf.margins) {}

void Paginator::generate_pdf(const char *outfile) {
    rend.reset(
        new PdfRenderer(outfile, page.w, page.h, doc.data.title.c_str(), doc.data.author.c_str()));

    const bool only_maintext = false;

    if(!only_maintext) {
        if(doc.data.is_draft) {
            create_draft_title_page();
        } else {
            create_title_page();
            create_colophon();
            if(!doc.data.dedication.empty()) {
                create_dedication();
                new_page(false);
            }
        }
    }

    create_maintext();

    while(!layout.empty()) {
        if(doc.data.is_draft) {
            render_page_num(styles.normal.font);
            flush_draw_commands();
            // FIXME add code that prints the "The End" page.
        } else {
            new_page(true);
        }
    }
    if(!only_maintext) {
        if(!doc.data.is_draft) {
            create_credits();
            new_page(false);
            create_postcredits();
            flush_draw_commands();
        }
    }
    rend.reset(nullptr);
}

void Paginator::create_maintext() {
    const auto paragraph_width = page.w - m.inner - m.outer;
    const auto section_width = 0.8 * paragraph_width;

    ExtraPenaltyAmounts extras;
    const Length bottom_watermark = page.h - m.lower - m.upper;
    Length rel_y;
    bool first_paragraph = true;
    bool first_section = true;

    for(const auto &e : doc.elements) {
        if(std::holds_alternative<Section>(e)) {
            const Section &s = std::get<Section>(e);
            printf("Processing section: %s\n", s.text.c_str());
            if(!first_section) {
                new_page(true);
                if(!doc.data.is_draft && current_page % 2 == 0) {
                    new_page(false);
                }
            }
            chapter_start_page = rend->page_num();
            rend->add_section_outline(s.number, s.text);
            first_section = false;
            rel_y = Length::zero();
            rel_y += spaces.above_section;
            heights.whitespace_height += spaces.above_section;
            assert(s.level == 1);
            // Fancy stuff above the text.
            std::string title_string;
            TextAlignment section_alignment = TextAlignment::Centered;
            if(doc.data.is_draft) {
                title_string = std::to_string(s.number);
                title_string += ". ";
                title_string += s.text;
                section_alignment = TextAlignment::Left;
            } else {
                title_string = "§ ";
                title_string += std::to_string(s.number);
                layout.text.emplace_back(MarkupDrawCommand{title_string,
                                                           &styles.section.font,
                                                           textblock_width() / 2,
                                                           rel_y,
                                                           TextAlignment::Centered});
                rel_y += 2 * styles.section.line_height;
                heights.text_height += 2 * styles.section.line_height;
                title_string = s.text;
            }
            // The title. Hyphenation is prohibited.
            std::vector<EnrichedWord> processed_words =
                text_to_formatted_words(title_string, false);
            ParagraphFormatter b(processed_words, section_width, styles.section, extras);
            auto lines = b.split_formatted_lines();
            auto built_lines =
                build_ragged_paragraph(lines, styles.section, section_alignment, rel_y);
            for(auto &line : built_lines) {
                layout.text.emplace_back(std::move(line));
                rel_y += styles.section.line_height;
                heights.text_height += styles.section.line_height;
            }
            rel_y += spaces.below_section;
            heights.text_height += spaces.below_section;
            first_paragraph = true;
        } else if(std::holds_alternative<Paragraph>(e)) {
            const Paragraph &p = std::get<Paragraph>(e);
            const ChapterParameters &cur_par =
                first_paragraph ? styles.normal_noindent : styles.normal;
            std::vector<EnrichedWord> processed_words = text_to_formatted_words(p.text);
            ParagraphFormatter b(processed_words, paragraph_width, cur_par, extras);
            auto lines = b.split_formatted_lines();
            std::vector<TextCommands> built_lines;
            if(doc.data.is_draft) {
                built_lines =
                    build_ragged_paragraph(lines, cur_par, TextAlignment::Left, Length::zero());
                if(!built_lines.empty()) {
                    auto &first_line = built_lines[0];
                    if(std::holds_alternative<MarkupDrawCommand>(first_line)) {
                        std::get<MarkupDrawCommand>(first_line).x += cur_par.indent;
                    } else if(std::holds_alternative<JustifiedMarkupDrawCommand>(first_line)) {
                        std::get<JustifiedMarkupDrawCommand>(first_line).x += cur_par.indent;
                    } else {
                        std::abort();
                    }
                }
            } else {
                built_lines = build_justified_paragraph(lines, cur_par, textblock_width());
            }
            Length current_y_origin = rel_y;
            int lines_in_paragraph = 0;
            for(auto &line : built_lines) {
                if(heights.total_height() + cur_par.line_height > bottom_watermark) {
                    new_page(true);
                    current_y_origin = -lines_in_paragraph * cur_par.line_height;
                    rel_y = Length::zero();
                }
                ++lines_in_paragraph;
                layout.text.emplace_back(std::move(line));
                adjust_y(layout.text.back(), current_y_origin);
                rel_y += cur_par.line_height;
                heights.text_height += cur_par.line_height;
            }
            first_paragraph = false;
        } else if(std::holds_alternative<Footnote>(e)) {
            if(!layout.footnote.empty() || !pending_footnotes.empty()) {
                printf("More than one footnote per page is not yet supported.");
                std::abort();
            }
            const Footnote &f = std::get<Footnote>(e);
            heights.whitespace_height += spaces.footnote_separation;
            std::vector<EnrichedWord> processed_words = text_to_formatted_words(f.text);
            ParagraphFormatter b(processed_words, paragraph_width, styles.footnote, extras);
            auto lines = b.split_formatted_lines();
            std::string fnum = std::to_string(f.number);
            fnum += '.';
            auto tmpy = heights.footnote_height;
            auto built_lines = build_justified_paragraph(lines, styles.footnote, textblock_width());
            const auto footnote_total_height = built_lines.size() * styles.footnote.line_height;
            // FIXME, split the footnote over two pages.
            if(heights.total_height() + footnote_total_height >= bottom_watermark) {
                pending_footnotes.emplace_back(MarkupDrawCommand{std::move(fnum),
                                                                 &styles.footnote.font,
                                                                 Length::zero(),
                                                                 tmpy,
                                                                 TextAlignment::Left});
                pending_footnotes.insert(
                    pending_footnotes.end(), built_lines.begin(), built_lines.end());
            } else {
                // FIXME: draw in flush_commands instead?
                layout.footnote.emplace_back(MarkupDrawCommand{std::move(fnum),
                                                               &styles.footnote.font,
                                                               Length::zero(),
                                                               tmpy,
                                                               TextAlignment::Left});
                heights.footnote_height += footnote_total_height;
                layout.footnote.insert(
                    layout.footnote.end(), built_lines.begin(), built_lines.end());
            }
        } else if(std::holds_alternative<SceneChange>(e)) {
            if(doc.data.is_draft) {
                layout.text.emplace_back(MarkupDrawCommand{"#",
                                                           &styles.normal.font,
                                                           textblock_width() / 2,
                                                           rel_y,
                                                           TextAlignment::Centered});
            }
            rel_y += styles.normal.line_height;
            heights.whitespace_height += styles.normal.line_height;
            if(rel_y >= bottom_watermark) {
                new_page(true);
                rel_y = Length::zero();
            }
            first_paragraph = true;
        } else if(std::holds_alternative<CodeBlock>(e)) {
            const CodeBlock &cb = std::get<CodeBlock>(e);
            rel_y += spaces.different_paragraphs;
            heights.whitespace_height += spaces.different_paragraphs;
            for(const auto &line : cb.raw_lines) {
                if(heights.total_height() >= bottom_watermark) {
                    new_page(true);
                    rel_y = Length::zero();
                }
                layout.text.emplace_back(MarkupDrawCommand{line.c_str(),
                                                           &styles.code.font,
                                                           spaces.codeblock_indent,
                                                           rel_y,
                                                           TextAlignment::Left});
                rel_y += styles.code.line_height;
                heights.text_height += styles.code.line_height;
            }
            first_paragraph = true;
            rel_y += spaces.different_paragraphs;
            heights.whitespace_height += spaces.different_paragraphs;
        } else if(std::holds_alternative<Figure>(e)) {
            const Figure &cb = std::get<Figure>(e);
            const auto fullpath = doc.data.top_dir / cb.file;
            auto image = rend->get_image(fullpath.c_str());
            Length display_width = Length::from_mm(image.w / image_dpi * 25.4);
            Length display_height = Length::from_mm(image.h / image_dpi * 25.4);
            if(doc.data.is_draft) {
                display_height = display_height / 2;
                display_width = display_width / 2;
            }
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
            const NumberList &nl = std::get<NumberList>(e);
            const Length number_area = Length::from_mm(5);
            const Length indent = spaces.codeblock_indent; // FIXME
            const Length text_width = paragraph_width - number_area - indent;
            const Length item_separator = spaces.different_paragraphs / 2;
            rel_y += spaces.different_paragraphs;
            heights.whitespace_height += spaces.different_paragraphs;
            for(size_t i = 0; i < nl.items.size(); ++i) {
                if(i != 0) {
                    rel_y += item_separator;
                    heights.whitespace_height += item_separator;
                }
                std::vector<EnrichedWord> processed_words = text_to_formatted_words(nl.items[i]);
                ParagraphFormatter b(processed_words, text_width, styles.lists, extras);
                auto lines = b.split_formatted_lines();
                std::string fnum = std::to_string(i + 1);
                fnum += '.';
                layout.text.emplace_back(MarkupDrawCommand{
                    std::move(fnum), &styles.lists.font, indent, rel_y, TextAlignment::Left});
                for(auto &line : build_justified_paragraph(
                        lines, styles.lists, text_width, indent + number_area, rel_y)) {
                    // FIXME, handle page changes.
                    layout.text.emplace_back(std::move(line));
                    heights.text_height += styles.lists.line_height;
                    rel_y += styles.lists.line_height;
                }
            }
            rel_y += spaces.different_paragraphs;
            heights.whitespace_height += spaces.different_paragraphs;
        } else {
            printf("Unknown element in document array.\n");
            std::abort();
        }
    }
}

void Paginator::add_top_image(const ImageInfo &image) {
    ImageCommand cmd;
    cmd.i = image;
    cmd.display_width = Length::from_mm(double(image.w) / image_dpi * 25.4);
    cmd.display_height = Length::from_mm(double(image.h) / image_dpi * 25.4);
    if(doc.data.is_draft) {
        cmd.display_height = cmd.display_height / 2;
        cmd.display_width = cmd.display_width / 2;
    }
    cmd.x = textblock_width() / 2 - cmd.display_width / 2;
    cmd.y = m.upper;
    layout.images.emplace_back(std::move(cmd));
    assert(heights.figure_height < Length::from_mm(0.0001));
    heights.figure_height += cmd.display_height;
    heights.figure_height += image_separator;
    layout.images.emplace_back(std::move(cmd));
    //  printf("Image is %d PPI\n", int(image.w / display_width.v * 25.4));
    //   heights.figure_height += display_height;
}

void Paginator::render_page_num(const FontParameters &par) {
    if(doc.data.is_draft) {
        // In the official draft style the first page does not have a number
        // so logical page numbers are offset from physical pages by one.
        std::string text =
            doc.data.draftdata.page_number_template + std::to_string(current_page - 1);
        rend->render_markup_as_is(text.c_str(),
                                  styles.normal.font,
                                  current_left_margin() + textblock_width(),
                                  m.upper - 2 * styles.normal.line_height,
                                  TextAlignment::Right);
    } else {
        char buf[128];
        snprintf(buf, 128, "%d", current_page);
        const Length yloc = page.h - m.lower + styles.normal.line_height;
        const Length xloc = current_left_margin() + (page.w - m.inner - m.outer) / 2;
        rend->render_line_centered(buf, par, xloc, yloc);
    }
}

std::vector<TextCommands>
Paginator::build_justified_paragraph(const std::vector<std::vector<std::string>> &lines,
                                     const ChapterParameters &text_par,
                                     const Length target_width,
                                     const Length x_off,
                                     const Length y_off) {
    Length rel_y;
    const Length x;
    std::vector<TextCommands> line_commands;
    line_commands.reserve(lines.size());
    size_t line_num = 0;
    for(const auto &markup_words : lines) {
        Length current_indent = line_num == 0 ? text_par.indent : Length{};
        if(line_num < lines.size() - 1) {
            line_commands.emplace_back(JustifiedMarkupDrawCommand{markup_words,
                                                                  &text_par.font,
                                                                  (x + current_indent) + x_off,
                                                                  rel_y + y_off,
                                                                  target_width - current_indent});
        } else {
            std::string full_line;
            for(const auto &w : markup_words) {
                full_line += w;
            }
            line_commands.emplace_back(MarkupDrawCommand{std::move(full_line),
                                                         &text_par.font,
                                                         x + current_indent + x_off,
                                                         rel_y + y_off,
                                                         TextAlignment::Left});
        }
        line_num++;
        rel_y += text_par.line_height;
    }
    return line_commands;
}

std::vector<TextCommands>
Paginator::build_ragged_paragraph(const std::vector<std::vector<std::string>> &lines,
                                  const ChapterParameters &text_par,
                                  const TextAlignment alignment,
                                  Length rel_y) {
    std::vector<TextCommands> line_commands;
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
            MarkupDrawCommand{std::move(full_line), &text_par.font, rel_x, rel_y, alignment});
        rel_y += text_par.line_height;
    }
    return line_commands;
}

std::vector<EnrichedWord> Paginator::text_to_formatted_words(const std::string &text,
                                                             bool permit_hyphenation) {
    StyleStack current_style(styles.code.font);
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

void Paginator::new_page(bool draw_page_num) {
    flush_draw_commands();
    if(draw_page_num) {
        render_page_num(styles.normal.font);
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

void Paginator::draw_debug_bars(int num_bars) {
    const Length boxheight = styles.section.line_height;
    const Length chaffwidth = Length::from_mm(6);
    for(int i = 0; i < num_bars; ++i) {
        rend->fill_box(
            current_left_margin(), m.upper + 2 * i * boxheight, textblock_width(), boxheight, 0.9);
    }
    rend->draw_box(current_left_margin() - chaffwidth,
                   m.upper,
                   textblock_width() + 2 * chaffwidth,
                   2 * num_bars * boxheight,
                   Length::from_pt(0.1));
    // Text area box
    std::vector<Coord> dashcoords;
    dashcoords.emplace_back(Coord{current_left_margin(), m.upper});
    dashcoords.emplace_back(Coord{current_left_margin(), m.upper + 2 * num_bars * boxheight});
    rend->draw_dash_line(dashcoords);
    dashcoords.clear();
    dashcoords.emplace_back(Coord{current_left_margin() + textblock_width(), m.upper});
    dashcoords.emplace_back(
        Coord{current_left_margin() + textblock_width(), m.upper + 2 * num_bars * boxheight});
    rend->draw_dash_line(dashcoords);
    dashcoords.clear();

    const auto hole_center_x = current_left_margin() - chaffwidth / 2;
    const auto hole_center_y = m.upper + boxheight / 2.0;
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

void Paginator::flush_draw_commands() {
    const bool draw_cut_guide = false;
    const bool draw_textarea_box = false;
    Length footnote_block_start = page.h - m.lower - heights.footnote_height;
    if(doc.data.debug_draw && !doc.data.is_draft && current_page == chapter_start_page) {
        draw_debug_bars(4);
    }
    if(draw_textarea_box) {
        rend->draw_box(current_left_margin(),
                       m.upper,
                       textblock_width(),
                       textblock_height(),
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
        if(std::holds_alternative<MarkupDrawCommand>(c)) {
            const auto &md = std::get<MarkupDrawCommand>(c);
            rend->render_markup_as_is(md.markup.c_str(),
                                      *md.font,
                                      md.x + current_left_margin(),
                                      md.y + m.upper + heights.figure_height,
                                      md.alignment);
        } else if(std::holds_alternative<JustifiedMarkupDrawCommand>(c)) {
            const auto &md = std::get<JustifiedMarkupDrawCommand>(c);
            rend->render_line_justified(md.markup_words,
                                        *md.font,
                                        md.width,
                                        current_left_margin() + md.x,
                                        md.y + m.upper + heights.figure_height);
        } else {
            printf("Unknown draw command.\n");
        }
    }

    for(const auto &c : layout.footnote) {
        if(std::holds_alternative<MarkupDrawCommand>(c)) {
            const auto &md = std::get<MarkupDrawCommand>(c);
            rend->render_markup_as_is(md.markup.c_str(),
                                      *md.font,
                                      current_left_margin() + md.x,
                                      md.y + footnote_block_start,
                                      md.alignment);
        } else if(std::holds_alternative<JustifiedMarkupDrawCommand>(c)) {
            const auto &md = std::get<JustifiedMarkupDrawCommand>(c);
            rend->render_line_justified(md.markup_words,
                                        *md.font,
                                        md.width,
                                        current_left_margin() + md.x,
                                        md.y + footnote_block_start);
        } else {
            printf("Unknown draw command.\n");
        }
    }
    if(!layout.footnote.empty()) {
        const Length line_thickness = Length::from_pt(1);
        const Length line_distance = doc.data.pdf.spaces.footnote_separation;
        const Length line_indent = Length::from_mm(-5);
        const Length line_width = Length::from_mm(20);
        const Length x0 = current_left_margin() + line_indent;
        const Length y0 = footnote_block_start - line_distance;
        rend->draw_line(x0, y0, x0 + line_width, y0, line_thickness);
    }
    layout.clear();
    heights.clear();
}

void Paginator::add_pending_figure(const ImageInfo &f) { pending_figures.push_back(f); }

int Paginator::count_words() {
    int num_words = 0;
    for(const auto &f : doc.data.sources) {
        const auto full_path = doc.data.top_dir / f;
        num_words += words_in_file(full_path.c_str());
    }
    if(num_words < 1000)
        return num_words;
    return ((num_words + 500) / 1000) * 1000;
}

void Paginator::create_draft_title_page() {
    const int num_words = count_words();
    const auto middle = current_left_margin() + textblock_width() / 2;
    auto textblock_center = m.upper + textblock_height() / 2;
    auto y = m.upper;
    const auto single_line_height = styles.normal.font.size * 1.5;
    const auto left_edge = current_left_margin();
    const auto right_edge = left_edge + textblock_width();
    const int bufsize = 128;
    char buf[bufsize];
    snprintf(buf, bufsize, wordcount_str[(int)doc.data.language], num_words);
    rend->render_markup_as_is(
        doc.data.author.c_str(), styles.normal.font, left_edge, y, TextAlignment::Left);
    rend->render_markup_as_is(buf, styles.normal.font, right_edge, y, TextAlignment::Right);
    y += single_line_height;

    rend->render_markup_as_is(
        doc.data.draftdata.phone.c_str(), styles.normal.font, left_edge, y, TextAlignment::Left);
    y += single_line_height;
    rend->render_markup_as_is(
        doc.data.draftdata.email.c_str(), styles.code.font, left_edge, y, TextAlignment::Left);

    y = textblock_center - 3 * styles.title.line_height;
    rend->render_markup_as_is(
        doc.data.title.c_str(), styles.title.font, middle, y, TextAlignment::Centered);
    y += 2 * styles.title.line_height;
    rend->render_markup_as_is(
        doc.data.author.c_str(), styles.author.font, middle, y, TextAlignment::Centered);

    y += styles.title.line_height;
    const std::string date = current_date();
    rend->render_markup_as_is(date.c_str(), styles.author.font, middle, y, TextAlignment::Centered);

    new_page(false);
}

void Paginator::create_title_page() {
    const auto middle = current_left_margin() + textblock_width() / 2;
    const auto text_top = m.upper + textblock_height() / 2 - 4 * styles.title.line_height;
    auto y = text_top;
    const Length gap = Length::from_mm(10);
    rend->render_markup_as_is(
        doc.data.title.c_str(), styles.title.font, middle, y, TextAlignment::Centered);
    y += styles.title.line_height;
    rend->render_markup_as_is(
        doc.data.author.c_str(), styles.author.font, middle, y, TextAlignment::Centered);
    y += styles.author.line_height;
    const auto text_bottom = y;
    const auto donut_outer = 0.8 * textblock_width() / 2;
    const auto donut_inner = 0.6 * donut_outer;

    // Fancy stuff here.
    y = text_top;
    rend->draw_arc(middle, text_top - gap, donut_outer, 0, M_PI, Length::from_pt(2));
    rend->draw_arc(middle, text_top - gap, donut_inner, 0, M_PI, Length::from_pt(2));
    rend->draw_arc(middle, text_bottom + gap, donut_outer, M_PI, 0, Length::from_pt(2));
    rend->draw_arc(middle, text_bottom + gap, donut_inner, M_PI, 0, Length::from_pt(2));
    new_page(false);
}

void Paginator::create_colophon() {
    const auto x = current_left_margin();
    auto y = page.h - m.lower - (doc.data.pdf.colophon.size() + 1) * styles.colophon.line_height;
    for(size_t i = 0; i < doc.data.pdf.colophon.size(); ++i) {
        if(!doc.data.pdf.colophon[i].empty()) {
            rend->render_text_as_is(doc.data.pdf.colophon[i].c_str(), styles.colophon.font, x, y);
        }
        y += styles.colophon.line_height;
    }

    new_page(false);
}

void Paginator::create_dedication() {
    ExtraPenaltyAmounts extras;
    const auto paragraph_width = page.w - m.inner - m.outer;
    const auto dedication_width = 2 * paragraph_width / 3;
    // FIXME, ragged_paragraph should take this as an argument.
    // const auto x = current_left_margin() + paragraph_width / 2;
    auto y = m.upper + (page.h - m.upper - m.lower) / 8;

    for(const auto &text : doc.data.dedication) {
        const auto processed_words = text_to_formatted_words(text, false);
        ParagraphFormatter b(processed_words, dedication_width, styles.dedication, extras);
        auto lines = b.split_formatted_lines();
        auto built_lines =
            build_ragged_paragraph(lines, styles.dedication, TextAlignment::Centered, y);
        for(auto &line : built_lines) {
            layout.text.emplace_back(std::move(line));
            y += styles.dedication.line_height;
            heights.text_height += styles.dedication.line_height;
        }
        y += styles.dedication.line_height;
        heights.text_height += styles.dedication.line_height;
    }
    new_page(false);
}

void Paginator::create_credits() {
    const auto paragraph_width = page.w - m.inner - m.outer;
    auto y = m.upper;
    const Length halfgap = Length::from_mm(2);
    const auto x1 = current_left_margin() + paragraph_width / 2 - halfgap;
    const auto x2 = x1 + 2 * halfgap;

    std::string buf;
    for(const auto &[key, value] : doc.data.credits) {
        buf = R"(<span variant="small-caps" letter_spacing="100">)";
        buf += key.c_str();
        buf += "</span>";
        if(!key.empty()) {
            rend->render_markup_as_is(buf.c_str(), styles.normal.font, x1, y, TextAlignment::Right);
        }
        buf = R"(<span variant="small-caps" letter_spacing="100">)";
        buf += value.c_str();
        buf += "</span>";
        if(!value.empty()) {
            rend->render_markup_as_is(buf.c_str(), styles.normal.font, x2, y, TextAlignment::Left);
        }
        y += styles.normal.line_height;
    }
}

void Paginator::create_postcredits() {
    draw_debug_bars(17);
    const auto x = current_left_margin() + Length::from_mm(2);
    Length y = m.upper + styles.section.line_height + Length::from_mm(0.5);
    for(const auto &l : doc.data.postcredits) {
        rend->render_text_as_is(l.c_str(), styles.code.font, x, y);
        y += styles.section.line_height;
    }
}
