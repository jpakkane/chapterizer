/*
 * Copyright 2025 Jussi Pakkanen
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

#include <paginator.hpp>

struct SectionElement {
    std::vector<TextCommands> lines;
    size_t chapter_number;
};

struct EmptyLineElement {
    size_t num_lines = 1;
};

struct ParagraphElement {
    std::vector<TextCommands> lines;
    ChapterParameters params;
    Length paragraph_width;
};

struct SpecialTextElement {
    std::vector<TextCommands> lines;
    Length extra_indent;
    const FontParameters *font = nullptr;
    TextAlignment alignment;
};

struct FootnoteElement {};

typedef std::
    variant<SectionElement, ParagraphElement, SpecialTextElement, EmptyLineElement, FootnoteElement>
        TextElement;

struct TextElementIterator {
    TextElementIterator() {
        element_id = 0;
        line_id = 0;
        elems = nullptr;
    }

    explicit TextElementIterator(std::vector<TextElement> &original) {
        elems = &original;
        element_id = 0;
        line_id = 0;
    }

    const TextElement &element();
    const TextCommands &line();

    void operator++();
    void operator--();

    void next_element() {
        if(element_id >= elems->size()) {
            return;
        }
        ++element_id;
        line_id = 0;
    }

    bool operator==(const TextElementIterator &o) const {
        return elems == o.elems && element_id == o.element_id && line_id == o.line_id;
    }

    bool operator!=(const TextElementIterator &o) const { return !(*this == o); }

    size_t element_id;
    size_t line_id;
    std::vector<TextElement> *elems;
};

struct TextLimits {
    TextElementIterator start;
    TextElementIterator end;
};

struct SectionPage {
    size_t section;
    TextLimits main_text;
};

struct RegularPage {
    TextLimits main_text;
    std::optional<size_t> image_id;
    std::optional<TextLimits> footnotes;
};

struct ImagePage {
    size_t image_id;
};

struct EmptyPage {};

typedef std::variant<SectionPage, RegularPage, ImagePage, EmptyPage> Page;

struct HeightMismatch {
    size_t page_number;
    int64_t delta;
};

struct PageStatistics {
    std::vector<size_t> widows;
    std::vector<size_t> orphans;
    std::vector<HeightMismatch> mismatches;
    bool single_line_last_page = false;
    size_t total_penalty = 0;
};

struct PageLayoutResult {
    std::vector<Page> pages;
    PageStatistics stats;
};

const std::vector<TextCommands> &get_lines(const TextElement &e);
size_t lines_on_page(const Page &p);

class Paginator2 {
public:
    Paginator2(const Document &d);
    ~Paginator2();

    void generate_pdf(const char *outfile);

private:
    void build_main_text();

    std::vector<TextCommands>
    build_justified_paragraph(const std::vector<std::vector<std::string>> &lines,
                              const ChapterParameters &text_par,
                              const Length target_width);
    std::vector<TextCommands>
    build_ragged_paragraph(const std::vector<std::vector<std::string>> &lines,
                           const ChapterParameters &text_par,
                           const TextAlignment alignment);

    void create_section(const Section &s, const ExtraPenaltyAmounts &extras);
    void create_codeblock(const CodeBlock &cb);
    void create_sign(const SignBlock &cb);
    void create_letter(const Letter &letter);

    void create_paragraph(const Paragraph &p,
                          const ExtraPenaltyAmounts &extras,
                          const ChapterParameters &chpar,
                          Length extra_indent);

    void optimize_page_splits();

    void render_output();

    std::vector<EnrichedWord> text_to_formatted_words(const std::string &text,
                                                      bool permit_hyphenation = true);

    Length textblock_width() const { return page.w - m.inner - m.outer; }
    Length textblock_height() const { return page.h - m.upper - m.lower; }

    void dump_text(const char *path);
    void print_stats(const PageLayoutResult &res);

    void new_page();

    void draw_edge_markers(size_t chapter_number, size_t page_number);
    void draw_page_number(size_t page_number);

    void render_maintext_lines(const TextElementIterator &start_loc,
                               const TextElementIterator &end_loc,
                               size_t book_page_number,
                               Length y, int current_line=-1);

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

    // Add frontmatter
    std::vector<Page> maintext_pages;
    // Add backmatter
    std::vector<TextElement> elements;
    FILE *stats;
};
