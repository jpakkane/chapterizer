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

#include <paginator2.hpp>
#include <paragraphformatter.hpp>
#include <cassert>

namespace {

const std::vector<TextCommands> &get_lines(const TextElement &e) {
    if(auto *sec = std::get_if<SectionElement>(&e)) {
        return sec->lines;
    } else if(auto *par = std::get_if<ParagraphElement>(&e)) {
        return par->lines;
    } else {
        std::abort();
    }
};

void plaintextprinter(FILE *f, const TextCommands &c) {
    if(const auto *just = std::get_if<JustifiedMarkupDrawCommand>(&c)) {
        for(const auto &w : just->markup_words) {
            if(w.back() != ' ') {
                fprintf(f, "%s ", w.c_str());
            } else {
                fprintf(f, "%s", w.c_str());
            }
        }
    } else if(const auto *rag = std::get_if<MarkupDrawCommand>(&c)) {
        if(rag->markup.back() != ' ') {
            fprintf(f, "%s ", rag->markup.c_str());
        } else {
            fprintf(f, "%s", rag->markup.c_str());
        }
    }
};

size_t lines_on_page(const Page &p) {
    size_t num_lines = 0;
    if(auto *reg = std::get_if<RegularPage>(&p)) {
        for(auto it = reg->main_text.start; it != reg->main_text.end; ++it) {
            ++num_lines;
        }
    } else {
        fprintf(stderr, "Unsupported page.\n");
        std::abort();
    }
    return num_lines;
}

} // namespace

void TextElementIterator::operator++() {
    if(element_id >= elems->size()) {
        return;
    }

    const auto &lines = get_lines((*elems)[element_id]);
    ++line_id;
    if(line_id >= lines.size()) {
        ++element_id;
        line_id = 0;
    }
}

void TextElementIterator::operator--() {
    if(element_id == 0 || line_id == 0) {
        return;
    }

    const auto &lines = get_lines((*elems)[element_id]);
    if(line_id == 0) {
        --element_id;
        line_id = lines.size() - 1;
    } else {
        --line_id;
    }
}

const TextElement &TextElementIterator::element() { return elems->at(element_id); }

const TextCommands &TextElementIterator::line() { return get_lines(element()).at(line_id); }

Paginator2::Paginator2(const Document &d)
    : doc(d), page(doc.data.pdf.page), styles(d.data.pdf.styles), spaces(d.data.pdf.spaces),
      m(doc.data.pdf.margins) {
    stats = nullptr;
}

Paginator2::~Paginator2() { fclose(stats); }

void Paginator2::generate_pdf(const char *outfile) {
    printf("Add functionality here.\n");
    std::filesystem::path statfile(outfile);
    statfile.replace_extension(".stats.txt");
    stats = fopen(statfile.string().c_str(), "w");
    fprintf(stats, "Statistics\n\n");
    build_main_text();
    if(true) {
        std::filesystem::path dumpfile(outfile);
        dumpfile.replace_extension(".dump.txt");
        dump_text(dumpfile.string().c_str());
    }
    print_stats();
}

void Paginator2::build_main_text() {
    ExtraPenaltyAmounts extras;
    bool first_paragraph = true;

    assert(std::holds_alternative<Section>(doc.elements.front()));
    size_t element_id{(size_t)-1};
    for(const auto &e : doc.elements) {
        ++element_id;

        if(auto *sec = std::get_if<Section>(&e)) {
            create_section(*sec, extras);
            // first_section, first_paragraph;
            first_paragraph = true;
        } else if(auto *par = std::get_if<Paragraph>(&e)) {
            create_paragraph(*par,
                             extras,
                             first_paragraph ? styles.normal_noindent : styles.normal,
                             Length::zero());
            first_paragraph = false;
        } else {
            fprintf(stderr, "Not supported yet.\n");
            std::abort();
        }
    }
    optimize_page_splits();
    // create_pdf();
}

void Paginator2::optimize_page_splits() {
    assert(current_page == 1);
    size_t lines_on_page = 0;
    const size_t max_lines = 25;
    TextElementIterator start(elements);
    TextElementIterator end = start;
    end.element_id = elements.size();

    for(TextElementIterator current = start; current != end; ++current) {
        const auto &e = current.element();
        if(lines_on_page >= max_lines) {
            TextLimits limits;
            limits.start = start;
            limits.end = current;
            pages.emplace_back(RegularPage{limits, {}, {}});
            start = current;
            lines_on_page = 1;
        } else {
            ++lines_on_page;
        }
    }
    if(lines_on_page > 0) {
        TextLimits limits;
        limits.start = start;
        limits.end = end;
        pages.emplace_back(RegularPage{limits, {}, {}});
    }
}

void Paginator2::create_section(const Section &s, const ExtraPenaltyAmounts &extras) {
    SectionElement selem;
    const auto paragraph_width = page.w - m.inner - m.outer;
    const auto section_width = 0.8 * paragraph_width;
    printf("Processing section: %s\n", s.text.c_str());
    // chapter_start_page = rend->page_num();
    // rend->add_section_outline(s.number, s.text);
    assert(s.level == 1);
    // Fancy stuff above the text.
    std::string title_string;
    TextAlignment section_alignment = TextAlignment::Centered;
    Length rel_y = Length::zero();
    if(doc.data.is_draft) {
        title_string = std::to_string(s.number);
        title_string += ". ";
        title_string += s.text;
        section_alignment = TextAlignment::Left;
    } else {
        title_string = std::to_string(s.number);
        selem.lines.emplace_back(MarkupDrawCommand{title_string,
                                                   &styles.section.font,
                                                   textblock_width() / 2,
                                                   rel_y,
                                                   TextAlignment::Centered});
        title_string = s.text;
    }
    // The title. Hyphenation is prohibited.
    std::vector<EnrichedWord> processed_words = text_to_formatted_words(title_string, false);
    ParagraphFormatter b(processed_words, section_width, styles.section, extras);
    auto lines = b.split_formatted_lines();
    auto built_lines = build_ragged_paragraph(lines, styles.section, section_alignment);
    for(auto &line : built_lines) {
        selem.lines.emplace_back(std::move(line));
    }
    elements.emplace_back(std::move(selem));
}

void Paginator2::create_paragraph(const Paragraph &p,
                                  const ExtraPenaltyAmounts &extras,
                                  const ChapterParameters &chpar,
                                  Length extra_indent) {
    ParagraphElement pelem;
    pelem.paragraph_width = textblock_width() - 2 * extra_indent;
    std::vector<EnrichedWord> processed_words = text_to_formatted_words(p.text);
    ParagraphFormatter b(processed_words, pelem.paragraph_width, chpar, extras);
    auto lines = b.split_formatted_lines();
    pelem.params = chpar;
    if(doc.data.is_draft) {
        pelem.lines = build_ragged_paragraph(lines, chpar, TextAlignment::Left);
    } else {
        pelem.lines = build_justified_paragraph(lines, chpar, pelem.paragraph_width);
    }
    // Shift sideways
    elements.emplace_back(std::move(pelem));
}

std::vector<TextCommands>
Paginator2::build_justified_paragraph(const std::vector<std::vector<std::string>> &lines,
                                      const ChapterParameters &text_par,
                                      const Length target_width) {
    Length rel_y = Length::zero();
    Length x_off = Length::zero();
    Length y_off = Length::zero();
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
Paginator2::build_ragged_paragraph(const std::vector<std::vector<std::string>> &lines,
                                   const ChapterParameters &text_par,
                                   const TextAlignment alignment) {
    std::vector<TextCommands> line_commands;
    const auto rel_x =
        alignment == TextAlignment::Centered ? textblock_width() / 2 : Length::zero();
    const auto rel_y = Length::zero(); // FIXME, eventually remove.
    line_commands.reserve(lines.size());
    for(const auto &markup_words : lines) {
        std::string full_line;
        for(const auto &w : markup_words) {
            full_line += w;
        }
        assert(alignment != TextAlignment::Right);
        line_commands.emplace_back(
            MarkupDrawCommand{std::move(full_line), &text_par.font, rel_x, rel_y, alignment});
    }
    return line_commands;
}

std::vector<EnrichedWord> Paginator2::text_to_formatted_words(const std::string &text,
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

void Paginator2::dump_text(const char *path) {
    FILE *f = fopen(path, "w");
    std::unique_ptr<FILE, int (*)(FILE *)> fcloser(f, fclose);

    size_t page_num = 0;
    for(const auto &p : pages) {
        ++page_num;
        fprintf(f, "%s -- PAGE %d --\n\n", (page_num != 1) ? "\n" : "", (int)page_num);
        if(auto *reg = std::get_if<RegularPage>(&p)) {
            TextElementIterator previous = reg->main_text.start;
            for(TextElementIterator it = reg->main_text.start; it != reg->main_text.end; ++it) {
                if(previous.element_id != it.element_id) {
                    fprintf(f, "\n");
                }
                plaintextprinter(f, it.line());
                fprintf(f, "\n");
                previous = it;
            }
        } else if(auto *sec = std::get_if<SectionPage>(&p)) {
            std::abort();
        } else {
            std::abort();
        }
    }
    /*
    while(false) {
        if(auto *sec = std::get_if<SectionElement>(&e)) {
            fprintf(f, "SECTION\n\n");
            for(const auto &l : sec->lines) {
                plaintextprinter(l);
                fprintf(f, "\n");
            }
            fprintf(f, "\n");
        } else if(auto *par = std::get_if<ParagraphElement>(&e)) {
            fprintf(f, "\n");
            for(const auto &l : par->lines) {
                plaintextprinter(l);
                fprintf(f, "\n");
            }
            fprintf(f, "\n");
        } else {
            // ignore
        }
    }
*/
}

void Paginator2::print_stats() {
    size_t even_page_height = 0;
    size_t odd_page_height = 0;
    for(size_t page_num = 0; page_num < pages.size(); ++page_num) {
        const auto &p = pages[page_num];
        const size_t num_lines_on_page = lines_on_page(p);
        if((page_num + 1) % 2) {
            odd_page_height = num_lines_on_page;
        } else {
            even_page_height = num_lines_on_page;
        }
        const bool on_last_page = page_num == pages.size() - 1;
        const bool on_first_page = page_num == 0;
        fprintf(stats, "Page %d\n\n", (int)page_num + 1);
        const auto &page_info = std::get<RegularPage>(p);
        size_t first_element_id = page_info.main_text.start.element_id;
        size_t first_line_id = page_info.main_text.start.line_id;
        size_t last_element_id = page_info.main_text.end.element_id;
        size_t last_line_id = page_info.main_text.end.line_id;

        const auto &start_lines = get_lines(elements[first_element_id]);
        if(last_element_id >= elements.size()) {
            continue;
            // FIXME: add end-of-chapter widow super penalty here.
        }
        const auto &end_lines = get_lines(elements[last_element_id]);

        // Orphan
        if(end_lines.size() > 1 && last_line_id == 1) {
            fprintf(stats, "Orphan line: ");
            plaintextprinter(stats, end_lines[0]);
            fprintf(stats, "\n");
        }
        // Widow
        if(start_lines.size() > 1 && first_line_id == start_lines.size() - 1) {
            fprintf(stats, "Widow line: ");
            plaintextprinter(stats, start_lines[start_lines.size() - 1]);
            fprintf(stats, "\n");
        }

        // Mismatch
        if(!on_first_page && !on_last_page && (((page_num + 1) % 2) == 1)) {
            if(even_page_height != odd_page_height) {
                fprintf(stderr, "Page spread height mismatch.\n");
            }
        }
        fprintf(stats, "\n");
    }
}
