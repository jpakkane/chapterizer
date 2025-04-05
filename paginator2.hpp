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
};

struct EmptyLineElement {};

struct ParagraphElement {
    std::vector<TextCommands> lines;
    ChapterParameters params;
    Length paragraph_width;
};

struct SpecialTextElement {};

struct FootnoteElement {};

typedef std::variant<SectionElement, EmptyLineElement, ParagraphElement, FootnoteElement>
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

    bool operator==(const TextElementIterator &o) const {
        return elems == o.elems && element_id == o.element_id && line_id == o.line_id;
    }

    bool operator!=(const TextElementIterator &o) const {
        return !(*this == o);
    }

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

struct EmptyPage {

};

typedef std::variant<SectionPage, RegularPage, ImagePage, EmptyPage> Page;

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

    void create_paragraph(const Paragraph &p,
                          const ExtraPenaltyAmounts &extras,
                          const ChapterParameters &chpar,
                          Length extra_indent);

    void optimize_page_splits();

    std::vector<EnrichedWord> text_to_formatted_words(const std::string &text,
                                                      bool permit_hyphenation = true);

    Length textblock_width() const { return page.w - m.inner - m.outer; }
    Length textblock_height() const { return page.h - m.upper - m.lower; }

    void dump_text(const char *path);
    void print_stats();

    const Document &doc;
    // These are just helpers to cut down on typing.
    const PageSize &page;
    const ChapterStyles &styles;
    const Spaces &spaces;
    const Margins &m;
    // std::unique_ptr<PdfRenderer> rend;
    WordHyphenator hyphen;
    int current_page = 1;
    int chapter_start_page = -1;

    std::vector<Page> pages;
    std::vector<TextElement> elements;
    FILE *stats;
};
