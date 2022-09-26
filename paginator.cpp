#include <paginator.hpp>
#include <utils.hpp>
#include <chaptercommon.hpp>
#include <paragraphformatter.hpp>
#include <wordhyphenator.hpp>
#include <bookparser.hpp>

#include <cassert>

namespace {

template<typename T> void style_change(T &stack, typename T::value_type val) {
    if(stack.contains(val)) {
        stack.pop(val);
    } else {
        stack.push(val);
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

Paginator::Paginator(const Document &d) : doc(d) {
    page.w = Millimeter::from_value(110);
    page.h = Millimeter::from_value(175);
    font_styles.basic.name = "Gentium";
    font_styles.basic.size = Point::from_value(10);
    font_styles.code.name = "Liberation Mono";
    font_styles.code.size = Point::from_value(8);
    font_styles.footnote = font_styles.basic;
    font_styles.footnote.size = Point::from_value(9);
    font_styles.heading.name = "Noto sans";
    font_styles.heading.size = Point::from_value(14);
    font_styles.heading.type = FontStyle::Bold;
}

void Paginator::generate_pdf(const char *outfile) {
    rend.reset(new PdfRenderer(outfile, page.w.topt(), page.h.topt()));

    ChapterParameters text_par;
    text_par.indent = Millimeter::from_value(5);
    text_par.line_height = Point::from_value(12);
    text_par.paragraph_width = page.w - m.inner - m.outer;
    text_par.font = font_styles.basic;

    ChapterParameters code_par = text_par;
    code_par.font = font_styles.code;

    ChapterParameters footnote_par = text_par;
    footnote_par.font = font_styles.footnote;
    footnote_par.line_height = Point::from_value(11);
    footnote_par.indent = Millimeter::from_value(4);

    ExtraPenaltyAmounts extras;
    const Millimeter bottom_watermark = page.h - m.lower - m.upper - text_par.line_height.tomm();
    const Millimeter title_above_space = Millimeter::from_value(30);
    const Millimeter title_below_space = Millimeter::from_value(10);
    const Millimeter different_paragraph_space = Millimeter::from_value(2);
    Millimeter x = m.inner;
    Millimeter rel_y;
    const Millimeter footnote_separation = Millimeter::from_value(4);
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
            first_section = false;
            rel_y = Millimeter::zero();
            x = current_left_margin();
            rel_y += title_above_space;
            heights.whitespace_height += title_above_space;
            assert(s.level == 1);
            std::string full_title = std::to_string(s.number);
            full_title += ". ";
            full_title += s.text;
            rend->render_markup_as_is(
                full_title.c_str(), font_styles.heading, x.topt(), (rel_y + m.upper).topt());
            rel_y += font_styles.heading.size.tomm();
            heights.text_height += font_styles.heading.size.tomm();
            rel_y += title_below_space;
            heights.text_height += title_below_space;
            first_paragraph = true;
        } else if(std::holds_alternative<Paragraph>(e)) {
            const Paragraph &p = std::get<Paragraph>(e);
            StyleStack current_style;
            auto cur_par = text_par;
            cur_par.indent = first_paragraph ? Millimeter::zero() : text_par.indent;
            std::vector<EnrichedWord> processed_words = text_to_formatted_words(p.text);
            ParagraphFormatter b(processed_words, cur_par, extras);
            auto lines = b.split_formatted_lines();
            render_formatted_lines(lines, x, rel_y, bottom_watermark, cur_par, heights.text_height);
            first_paragraph = false;
        } else if(std::holds_alternative<Footnote>(e)) {
            if(!layout.footnote.empty()) {
                printf("More than one footnote per page is not yet supported.");
                std::abort();
            }
            const Footnote &f = std::get<Footnote>(e);
            heights.whitespace_height += footnote_separation;
            StyleStack current_style;
            std::vector<EnrichedWord> processed_words = text_to_formatted_words(f.text);
            ParagraphFormatter b(processed_words, footnote_par, extras);
            auto lines = b.split_formatted_lines();
            std::string fnum = std::to_string(f.number);
            fnum += '.';
            auto tmpy = Millimeter::zero();
            layout.footnote.emplace_back(
                MarkupDrawCommand{std::move(fnum), &footnote_par.font, x, tmpy});
            build_formatted_lines(lines, x, tmpy, footnote_par, heights.footnote_height);
        } else if(std::holds_alternative<SceneChange>(e)) {
            rel_y += text_par.line_height.tomm();
            heights.whitespace_height += text_par.line_height.tomm();
            if(rel_y >= bottom_watermark) {
                new_page(true);
                rel_y = Millimeter::zero();
                x = current_left_margin();
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
                    x = current_left_margin();
                }
                rend->render_text_as_is(
                    line.c_str(), code_par.font, x.topt(), (rel_y + m.upper).topt());
                rel_y += code_par.line_height.tomm();
                heights.text_height += code_par.line_height.tomm();
            }
            first_paragraph = true;
            rel_y += different_paragraph_space;
            heights.whitespace_height += different_paragraph_space;
        } else {
            std::abort();
        }
    }
}

void Paginator::render_page_num(const FontParameters &par) {
    char buf[128];
    snprintf(buf, 128, "%d", current_page);
    const Millimeter yloc = page.h - 2.0 * m.lower / 3.0;
    const Millimeter xloc = current_left_margin() + (page.w - m.inner - m.outer) / 2;
    rend->render_line_centered(buf, par, xloc.topt(), yloc.topt());
}

void Paginator::render_formatted_lines(const std::vector<std::vector<std::string>> &lines,
                                       Millimeter &x,
                                       Millimeter &rel_y,
                                       const Millimeter &bottom_watermark,
                                       const ChapterParameters &text_par,
                                       Millimeter &height_counter) {
    size_t line_num = 0;
    for(const auto &markup_words : lines) {
        Millimeter current_indent = line_num == 0 ? text_par.indent : Millimeter{};
        if(rel_y >= bottom_watermark) {
            new_page(true);
            rel_y = Millimeter::zero();
            x = current_left_margin();
        }
        if(line_num < lines.size() - 1) {
            rend->render_line_justified(markup_words,
                                        text_par.font,
                                        text_par.paragraph_width - current_indent,
                                        (x + current_indent).topt(),
                                        (rel_y + m.upper).topt());
        } else {
            rend->render_markup_as_is(
                markup_words, text_par.font, (x + current_indent).topt(), (rel_y + m.upper).topt());
        }
        line_num++;
        rel_y += text_par.line_height.tomm();
        height_counter += text_par.line_height.tomm();
    }
}

void Paginator::build_formatted_lines(const std::vector<std::vector<std::string>> &lines,
                                      Millimeter &x,
                                      Millimeter &rel_y,
                                      const ChapterParameters &text_par,
                                      Millimeter &height_counter) {
    size_t line_num = 0;
    for(const auto &markup_words : lines) {
        Millimeter current_indent = line_num == 0 ? text_par.indent : Millimeter{};
        if(line_num < lines.size() - 1) {
            layout.footnote.emplace_back(
                JustifiedMarkupDrawCommand{markup_words,
                                           &text_par.font,
                                           (x + current_indent),
                                           rel_y,
                                           text_par.paragraph_width - current_indent});
        } else {
            std::string full_line;
            for(const auto &w : markup_words) {
                full_line += w;
            }
            layout.footnote.emplace_back(
                MarkupDrawCommand{std::move(full_line), &text_par.font, x, rel_y});
        }
        line_num++;
        rel_y += text_par.line_height.tomm();
        height_counter += text_par.line_height.tomm();
    }
}

std::vector<EnrichedWord> Paginator::text_to_formatted_words(const std::string &text) {
    StyleStack current_style;
    auto plain_words = split_to_words(std::string_view(text));
    std::vector<EnrichedWord> processed_words;
    for(const auto &word : plain_words) {
        auto working_word = word;
        auto start_style = current_style;
        auto formatting_data = extract_styling(current_style, working_word);
        auto hyphenation_data = hyphen.hyphenate(working_word);
        processed_words.emplace_back(EnrichedWord{std::move(working_word),
                                                  std::move(hyphenation_data),
                                                  std::move(formatting_data),
                                                  start_style});
    }
    return processed_words;
}

void Paginator::new_page(bool draw_page_num) {
    const bool debug_draw = true;
    flush_draw_commands();
    if(draw_page_num) {
        render_page_num(font_styles.basic);
    }
    rend->new_page();
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
    for(const auto &c : layout.footnote) {
        if(std::holds_alternative<MarkupDrawCommand>(c)) {
            const auto &md = std::get<MarkupDrawCommand>(c);
            rend->render_markup_as_is(
                md.markup.c_str(), *md.font, md.x.topt(), (md.y + footnote_block_start).topt());
        } else if(std::holds_alternative<JustifiedMarkupDrawCommand>(c)) {
            const auto &md = std::get<JustifiedMarkupDrawCommand>(c);
            rend->render_line_justified(md.markup_words,
                                        *md.font,
                                        md.width,
                                        md.x.topt(),
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
