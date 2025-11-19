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

#pragma once

#include <metadata.hpp>

#include <bookparser.hpp>
#include <capypdfrenderer.hpp>
#include <formatting.hpp>
#include <hbfontcache.hpp>
#include <draftparagraphformatter.hpp>
#include <capypdf.hpp>

#include <memory>

struct HBRunDrawCommand {
    std::vector<HBRun> runs;
    Length x;
    Length y;
    TextAlignment alignment;
};

struct SimpleTextDrawCommand {
    std::string text;
    HBTextParameters par;
    Length x;
    Length y;
    TextAlignment alignment;
};

struct CapyImageCommand {
    CapyImageInfo i;
    Length x; // Relative to left edge of text block.
    Length y;
    Length display_height;
    Length display_width;
};

typedef std::variant<SimpleTextDrawCommand, HBRunDrawCommand> HBTextCommands;

struct PageLayout {
    std::vector<CapyImageCommand> images;
    std::vector<HBTextCommands> text;
    std::vector<HBTextCommands> footnote;

    bool empty() const { return text.empty() && footnote.empty() && images.empty(); }

    void clear() {
        images.clear();
        text.clear();
        footnote.clear();
    }
};

struct Heights {
    Length figure_height;
    Length text_height;
    Length footnote_height;
    Length whitespace_height;

    Length total_height() const {
        return figure_height + text_height + footnote_height + whitespace_height;
    }

    void clear() {
        figure_height = text_height = footnote_height = whitespace_height = Length::zero();
    }
};

std::vector<FormattingChange> extract_styling(StyleStack &current_style, std::string &word);

class DraftPaginator {
public:
    DraftPaginator(const Document &d);

    void generate_pdf(const char *outfile);

    void draw_debug_bars(int num_bars, const Length bar_start_y);

private:
    void render_page_num(const HBChapterParameters &par);
    std::vector<HBTextCommands>
    build_justified_paragraph(const std::vector<std::vector<std::string>> &lines,
                              const HBChapterParameters &text_par,
                              const Length target_width,
                              const Length x_off = Length::zero(),
                              const Length y_off = Length::zero());
    std::vector<HBTextCommands>
    build_ragged_paragraph(const std::vector<std::vector<std::string>> &lines,
                           const HBChapterParameters &text_par,
                           const TextAlignment alignment,
                           Length rel_y);
    std::vector<HBTextCommands> build_ragged_paragraph(const std::vector<std::vector<HBRun>> &lines,
                                                       const HBChapterParameters &text_par,
                                                       const TextAlignment alignment,
                                                       Length extra_x,
                                                       Length rel_y);
    std::vector<EnrichedWord> text_to_formatted_words(const std::string &text,
                                                      bool permit_hyphenation = true);

    Length current_left_margin() const { return current_page % 2 ? m.inner : m.outer; }

    void new_page(bool draw_page_num);

    void flush_draw_commands();

    Length textblock_width() const { return page.w - m.inner - m.outer; }
    Length textblock_height() const { return page.h - m.upper - m.lower; }

    void add_pending_figure(const CapyImageInfo &f);
    void add_top_image(const CapyImageInfo &image);

    void create_draft_title_page();
    void create_maintext();

    void
    create_section(const Section &s, Length &rel_y, bool &first_section, bool &first_paragraph);
    void create_paragraph(const Paragraph &p,
                          Length &rel_y,
                          const Length &bottom_watermark,
                          const HBChapterParameters &chpar,
                          Length extra_indent);
    void create_footnote(const Footnote &f, const Length &bottom_watermark);
    void create_numberlist(const NumberList &nl, Length &rel_y);

    int count_words();

    const Document &doc;
    // These are just helpers to cut down on typing.
    const PageSize &page;
    HBChapterStyles styles;
    const Spaces &spaces;
    const Margins &m;
    std::unique_ptr<CapyPdfRenderer> rend;
    WordHyphenator hyphen;
    int current_page = 1;
    int chapter_start_page = -1;

    // These keep track of the current page stats.
    PageLayout layout;
    Heights heights;
    std::vector<CapyImageInfo> pending_figures;
    std::vector<HBTextCommands> pending_footnotes;
    HBFontCache fc;
};
