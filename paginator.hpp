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

#pragma once

#include <metadata.hpp>

#include <bookparser.hpp>
#include <pdfrenderer.hpp>
#include <formatting.hpp>

#include <memory>

struct MarkupDrawCommand {
    std::string markup;
    const FontParameters *font;
    Millimeter x;
    Millimeter y;
    TextAlignment alignment;
};

struct JustifiedMarkupDrawCommand {
    std::vector<std::string> markup_words;
    const FontParameters *font;
    Millimeter x;
    Millimeter y;
    Millimeter width;
};

struct ImageCommand {
    ImageInfo i;
    Millimeter x;
    Millimeter y;
    Millimeter display_height;
    Millimeter display_width;
};

typedef std::variant<MarkupDrawCommand, JustifiedMarkupDrawCommand> TextCommands;

struct PageLayout {
    std::vector<ImageCommand> images;
    std::vector<TextCommands> text;
    std::vector<TextCommands> footnote;

    bool empty() const { return text.empty() && footnote.empty(); }

    void clear() {
        images.clear();
        text.clear();
        footnote.clear();
    }
};

struct Heights {
    Millimeter figure_height;
    Millimeter text_height;
    Millimeter footnote_height;
    Millimeter whitespace_height;

    Millimeter total_height() const {
        return figure_height + text_height + footnote_height + whitespace_height;
    }

    void clear() {
        figure_height = text_height = footnote_height = whitespace_height =
            Millimeter::from_value(0);
    }
};

class Paginator {
public:
    Paginator(const Document &d);

    void generate_pdf(const char *outfile);

private:
    void render_page_num(const FontParameters &par);
    std::vector<TextCommands>
    build_justified_paragraph(const std::vector<std::vector<std::string>> &lines,
                              const ChapterParameters &text_par,
                              const Millimeter target_width,
                              const Millimeter x_off = Millimeter::zero(),
                              const Millimeter y_off = Millimeter::zero());
    std::vector<TextCommands>
    build_ragged_paragraph(const std::vector<std::vector<std::string>> &lines,
                           const ChapterParameters &text_par,
                           const TextAlignment alignment,
                           Millimeter rel_y);
    std::vector<EnrichedWord> text_to_formatted_words(const std::string &text,
                                                      bool permit_hyphenation = true);

    Millimeter current_left_margin() const { return current_page % 2 ? m.inner : m.outer; }

    void new_page(bool draw_page_num);

    void flush_draw_commands();

    Millimeter textblock_width() const { return page.w - m.inner - m.outer; }
    Millimeter textblock_height() const { return page.h - m.upper - m.lower; }

    void add_pending_figure(const ImageInfo &f);
    void add_top_image(const ImageInfo &image);

    void create_draft_title_page();
    void create_title_page();
    void create_colophon();
    void create_dedication();
    void create_credits();
    void create_maintext();

    const Document &doc;
    // These are just helpers to cut down on typing.
    const PageSize &page;
    const ChapterStyles &styles;
    const Spaces &spaces;
    const Margins &m;
    std::unique_ptr<PdfRenderer> rend;
    WordHyphenator hyphen;
    int current_page = 1;
    int chapter_start_page = -1;
    const bool debug_draw = false;

    // These keep track of the current page stats.
    PageLayout layout;
    Heights heights;
    std::optional<ImageInfo> pending_figure;
};
