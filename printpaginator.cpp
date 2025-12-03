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

#include <printpaginator.hpp>
#include <paragraphformatter.hpp>
#include <chapterformatter.hpp>
#include <cassert>
#include <random>

namespace {

void plaintextprinter(FILE *f, const TextCommands &c) {
    (void)f;
    (void)c;
    /*
    if(const auto *just = std::get_if<JustifiedTextDrawCommand>(&c)) {
        for(const auto &w : just->markup_words) {
            if(w.back() != ' ') {
                fprintf(f, "%s ", w.c_str());
            } else {
                fprintf(f, "%s", w.c_str());
            }
        }
    } else if(const auto *rag = std::get_if<TextDrawCommand>(&c)) {
        if(rag->markup.empty()) {
        } else if(rag->markup.back() != ' ') {
            fprintf(f, "%s ", rag->markup.c_str());
        } else {
            fprintf(f, "%s", rag->markup.c_str());
        }
    }
*/
};

const std::vector<TextCommands> empty_line{
    TextDrawCommand{{}, Length::zero(), Length::zero(), TextAlignment::Left}};

// Does not do any justification, just a straight conversion.
std::vector<HBRun> line2runs(const HBLine &line) {
    std::vector<HBRun> all_line_runs;
    for(const auto &w : line.words) {
        for(const auto &r : w.runs) {
            all_line_runs.push_back(r);
        }
    }
    return all_line_runs;
}

} // namespace

const std::vector<TextCommands> &get_lines(const TextElement &e) {
    if(auto *sec = std::get_if<SectionElement>(&e)) {
        return sec->lines;
    } else if(auto *par = std::get_if<ParagraphElement>(&e)) {
        return par->lines;
    } else if(auto *spc = std::get_if<SpecialTextElement>(&e)) {
        return spc->lines;
    } else if(auto *empty = std::get_if<EmptyLineElement>(&e)) {
        (void)empty;
        return empty_line;
    } else {
        std::abort();
    }
};

size_t get_num_logical_lines(const TextElement &e) {
    if(auto *empty = std::get_if<EmptyLineElement>(&e)) {
        return empty->num_lines;
    } else if(auto *image = std::get_if<ImageElement>(&e)) {
        (void)image;
        return 1;
    } else {
        return get_lines(e).size();
    }
}

size_t lines_on_page(const Page &p) {
    size_t num_lines = 0;
    if(auto *reg = std::get_if<RegularPage>(&p)) {
        for(auto it = reg->main_text.start; it != reg->main_text.end; ++it) {
            if(num_lines > 0 || !std::holds_alternative<EmptyLineElement>(it.element())) {
                ++num_lines;
            }
        }
        if(reg->image) {
            num_lines += reg->image->height_in_lines;
        }
    } else if(auto *sec = std::get_if<SectionPage>(&p)) {
        const size_t chapter_heading_top_whitespace = 8;
        num_lines += chapter_heading_top_whitespace; // FIXME
        num_lines += 1;
        auto it = sec->main_text.start;
        it.next_element();
        for(; it != sec->main_text.end; ++it) {
            ++num_lines;
        }
    } else {
        fprintf(stderr, "Unsupported page.\n");
        std::abort();
    }
    return num_lines;
}

void TextElementIterator::operator++() {
    if(element_id >= elems->size()) {
        return;
    }

    auto num_lines = get_num_logical_lines((*elems)[element_id]);
    ++line_id;
    if(line_id >= num_lines) {
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

PrintPaginator::PrintPaginator(const Document &d)
    : doc(d), page(doc.data.pdf.page), styles(d.data.pdf.styles), spaces(d.data.pdf.spaces),
      m(doc.data.pdf.margins) {
    stats = nullptr;
    if(doc.data.is_draft) {
        fprintf(stderr, "Tried to generate final print when in draft mode.\n");
        std::abort();
    }
}

PrintPaginator::~PrintPaginator() { fclose(stats); }

void PrintPaginator::generate_pdf(const char *outfile) {
    capypdf::DocumentProperties dprop;
    capypdf::PageProperties pprop;

    if(doc.data.pdf.bleed.mm() > 0) {
        pprop.set_pagebox(CAPY_BOX_MEDIA,
                          0,
                          0,
                          (page.w + 2 * doc.data.pdf.bleed).pt(),
                          (page.h + 2 * doc.data.pdf.bleed).pt());
        pprop.set_pagebox(CAPY_BOX_TRIM,
                          doc.data.pdf.bleed.pt(),
                          doc.data.pdf.bleed.pt(),
                          (page.w + doc.data.pdf.bleed).pt(),
                          (page.h + doc.data.pdf.bleed).pt());
    } else {
        pprop.set_pagebox(CAPY_BOX_MEDIA, 0, 0, page.w.pt(), page.h.pt());
    }
    dprop.set_default_page_properties(pprop);
    dprop.set_title(doc.data.title);
    dprop.set_author(doc.data.author);
    dprop.set_creator("SuperPDF from outer space!");
    assert(!doc.data.is_draft);

    rend.reset(new CapyPdfRenderer(outfile, page.w, page.h, doc.data.pdf.bleed, dprop, fc));
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
    if(debug_page) {
        rend->draw_box(Length::zero(), Length::zero(), page.w, page.h, 0.8, Length::from_pt(0.5));
        rend->draw_box(current_left_margin(),
                       m.lower,
                       textblock_width(),
                       textblock_height(),
                       0.8,
                       Length::from_pt(0.5));
    }
    // rend->init_page();
    render_output();
}

void PrintPaginator::render_output() {
    render_frontmatter();
    render_mainmatter();
    render_backmatter();
}

void PrintPaginator::render_frontmatter() {
    for(const auto &f : doc.data.frontmatter) {
        if(std::holds_alternative<Empty>(f)) {

        } else if(auto *col = std::get_if<Colophon>(&f)) {
            const Length &line_height = styles.colophon.line_height;
            Length y = m.lower + col->lines.size() * line_height;
            const Length &left = current_left_margin();
            for(const auto &line : col->lines) {
                rend->render_text_as_is(line.c_str(), styles.colophon.font, left, y);
                y -= line_height;
            }
        } else if(auto *ded = std::get_if<Dedication>(&f)) {
            Length y = page.h - (m.upper + textblock_height() / 4);
            const Length &line_height = styles.dedication.line_height;
            const Length &middle = current_left_margin() + textblock_width() / 2;
            for(const auto &line : ded->lines) {
                rend->render_text_as_is(
                    line.c_str(), styles.dedication.font, middle, y, TextAlignment::Centered);
                y -= line_height;
            }
        } else if(std::holds_alternative<FirstPage>(f)) {
            const char stylespan_start[] = R"(<span variant="small-caps" letter_spacing="1500">)";
            const char stylespan_end[] = "</span>";
            std::string buf;
            Length y = page.h - (m.upper + textblock_height() / 8);
            const Length &line_height = styles.normal.line_height;
            const Length &middle = current_left_margin() + textblock_width() / 2;
            auto tempfont = styles.normal.font;
            // tempfont.name = "TeX Gyre Schola";
            tempfont.size = Length::from_pt(20);
            buf = stylespan_start;
            buf += doc.data.author;
            buf += stylespan_end;
            rend->render_text_as_is(buf.c_str(), tempfont, middle, y, TextAlignment::Centered);
            tempfont.size = Length::from_pt(24);
            y -= 8 * line_height;
            auto colon = doc.data.title.find(':');
            if(colon == std::string::npos) {
                buf = stylespan_start;
                buf += doc.data.title;
                buf += stylespan_end;
                rend->render_text_as_is(buf.c_str(), tempfont, middle, y, TextAlignment::Centered);
            } else {
                auto part1 = doc.data.title.substr(0, colon + 1);
                auto part2 = doc.data.title.substr(colon + 2);
                auto space = part2.find(' ');
                auto part2_1 = part2.substr(0, space);
                auto part2_2 = part2.substr(space + 1);
                buf = stylespan_start;
                buf += part1;
                buf += stylespan_end;
                rend->render_text_as_is(buf.c_str(), tempfont, middle, y, TextAlignment::Centered);
                y -= 4 * line_height;
                buf = stylespan_start;
                buf += part2_1;
                buf += stylespan_end;
                tempfont.size = Length::from_pt(28);
                rend->render_text_as_is(buf.c_str(), tempfont, middle, y, TextAlignment::Centered);
                y -= 2 * line_height;
                buf = stylespan_start;
                buf += part2_2;
                buf += stylespan_end;
                rend->render_text_as_is(buf.c_str(), tempfont, middle, y, TextAlignment::Centered);
            }
        } else if(auto *signing = std::get_if<Signing>(&f)) {
            render_signing_page(*signing);
        } else {
            fprintf(stderr, "Failure of fail.\n");
            std::abort();
        }
        rend->new_page();
    }
}

void PrintPaginator::render_signing_page(const Signing &s) {
    const size_t NUM_ENTRIES = 128;
    std::array<Length, NUM_ENTRIES> ydelta;
    std::array<Length, NUM_ENTRIES> xdelta;
    std::array<double, NUM_ENTRIES> tilt;
    std::default_random_engine e(66);
    std::normal_distribution<double> shiftdist(0.0, 0.05);
    std::string feeder{"X"};
    for(size_t i = 0; i < NUM_ENTRIES; ++i) {
        ydelta[i] = 2 * Length::from_pt(shiftdist(e));
        xdelta[i] = 3 * Length::from_pt(shiftdist(e));
        tilt[i] = shiftdist(e) / 2;
    }
    Length y = page.h - (m.upper + textblock_height() / 5);
    const Length letter_width = Length::from_pt(6);
    const Length &line_height = styles.code.line_height;
    const Length &middle = current_left_margin() + textblock_width() / 2;

    for(const auto &line : s.lines) {
        const auto num_letters = line.size();
        Length x = middle - double(num_letters) / 2 * letter_width;
        for(size_t i = 0; i < num_letters; ++i) {
            const auto current_char = line[i];
            if((unsigned char)current_char >= NUM_ENTRIES) {
                fprintf(stderr, "Ääkköset ovat vielä ongelma.\n");
                std::abort();
            }
            feeder[0] = current_char;
            rend->render_wonky_text(feeder.c_str(),
                                    styles.code.font,
                                    ydelta[current_char],
                                    xdelta[current_char],
                                    tilt[current_char],
                                    0.2,
                                    x,
                                    y);
            x += letter_width;
        }
        y -= line_height;
    }
}

void PrintPaginator::render_floating_image(const ImageElement &imel) {
    Length imw = Length::from_mm(double(imel.info.w) / imel.ppi * 25.4);
    Length imh = Length::from_mm(double(imel.info.h) / imel.ppi * 25.4);
    Length y = page.h - (m.upper + imh);
    Length x = current_left_margin() + textblock_width() / 2 - imw / 2;
    rend->draw_image(imel.info, x, y, imw, imh);
}

void PrintPaginator::render_mainmatter() {
    size_t current_section_number = 0;
    for(const auto &current_section : maintext_sections) {
        ++current_section_number;
        for(const auto &p : current_section) {
            size_t book_page_number = rend->page_num();
            if(auto *reg_page = std::get_if<RegularPage>(&p)) {
                const Length line_height = styles.normal.line_height;
                Length y = page.h - (m.upper + line_height);
                if(reg_page->image) {
                    render_floating_image(reg_page->image.value());
                    y -= line_height * reg_page->image->height_in_lines;
                }
                render_maintext_lines(
                    reg_page->main_text.start, reg_page->main_text.end, book_page_number, y);
                draw_edge_markers(current_section_number, book_page_number);
                draw_page_number(book_page_number);
            } else if(auto *sec_page = std::get_if<SectionPage>(&p)) {
                if(book_page_number % 2 == 0) {
                    new_page();
                    ++book_page_number;
                }
                rend->add_section_outline(sec_page->section, "luku");
                const auto &textblock_left = (book_page_number % 2) == 0
                                                 ? doc.data.pdf.margins.outer
                                                 : doc.data.pdf.margins.inner;
                const size_t chapter_heading_top_whitespace = 8;
                const Length line_height = styles.normal.line_height;
                Length y = page.h - (m.upper + chapter_heading_top_whitespace * line_height);
                auto it = sec_page->main_text.start;
                const auto &section_element = std::get<SectionElement>(it.element());
                it.next_element();
                assert(section_element.lines.size() == 1);
                const auto &chapter_number =
                    std::get<TextDrawCommand>(section_element.lines.front());
                const Length hack_delta = Length::from_pt(-20);
                rend->render_runs(chapter_number.runs,
                                  textblock_left + textblock_width() / 2,
                                  y - hack_delta,
                                  chapter_number.alignment);
                y -= line_height;
                render_maintext_lines(it, sec_page->main_text.end, book_page_number, y, 0);
            } else if(std::holds_alternative<EmptyPage>(p)) {
            } else {
                fprintf(stderr, "Not implemented yet.\n");
                std::abort();
            }
            new_page();
        }
    }
}

void PrintPaginator::render_backmatter() {
    if(doc.data.credits.empty()) {
        return;
    }
    // const auto paragraph_width = page.w - m.inner - m.outer;
    auto y = m.upper;
    // const Length halfgap = Length::from_mm(2);
    // const auto xmiddle = current_left_margin() + paragraph_width / 2;
    // const auto x1 = xmiddle - halfgap;
    // const auto x2 = x1 + 2 * halfgap;
    const char stylespan[] = R"(<span variant="small-caps" letter_spacing="1500">)";

    std::string buf;
    for(const auto &centry : doc.data.credits) {
        if(const auto *regular = std::get_if<CreditsEntry>(&centry)) {
            const auto &key = regular->key;
            const auto &value = regular->value;
            buf = stylespan;
            buf += key.c_str();
            buf += "</span>";
            if(!key.empty()) {
                std::abort();
                // rend->render_markup_as_is(
                //     buf.c_str(), styles.normal.font, x1, y, TextAlignment::Right);
            }
            buf = stylespan;
            buf += value.c_str();
            buf += "</span>";
            if(!value.empty()) {
                std::abort();
                // rend->render_markup_as_is(
                //     buf.c_str(), styles.normal.font, x2, y, TextAlignment::Left);
            }
        } else {
            const auto &line = std::get<CreditsTitle>(centry).line;
            if(!line.empty()) {
                buf = stylespan;
                buf += line;
                buf += "</span>";
                std::abort();
                // rend->render_markup_as_is(
                //     buf.c_str(), styles.normal.font, xmiddle, y, TextAlignment::Centered);
            }
        }
        y += styles.normal.line_height;
    }
    rend->finalize_page();
}

void PrintPaginator::render_maintext_lines(const TextElementIterator &start_loc,
                                           const TextElementIterator &end_loc,
                                           size_t book_page_number,
                                           Length y,
                                           int current_line) {
    const Length line_height = styles.normal.line_height;
    const auto &textblock_left =
        (book_page_number % 2) == 0 ? doc.data.pdf.margins.outer : doc.data.pdf.margins.inner;
    for(auto it = start_loc; it != end_loc; ++it) {
        ++current_line;
        if(std::holds_alternative<ParagraphElement>(it.element())) {
            const auto &line = it.line();
            if(const auto *j = std::get_if<JustifiedTextDrawCommand>(&line)) {
                rend->render_line_justified(j->words, j->width, textblock_left + j->x, y);
            } else if(const auto *r = std::get_if<TextDrawCommand>(&line)) {
                rend->render_runs(r->runs, textblock_left + r->x, y, TextAlignment::Left);
            } else {
                std::abort();
            }
            y -= line_height;
        } else if(auto *special = std::get_if<SpecialTextElement>(&it.element())) {
            const auto &line = it.line();
            const auto mu = std::get<TextDrawCommand>(line);
            rend->render_runs(
                mu.runs, textblock_left + special->extra_indent, y, special->alignment);
            y -= line_height;
        } else if(auto *empty = std::get_if<EmptyLineElement>(&it.element())) {
            // Empty lines at the top of the page are ignored.
            if(current_line != 0) {
                y -= empty->num_lines * line_height;
            }
        } else if(std::holds_alternative<ImageElement>(it.element())) {
            // Images are rendered at the start of the page.
        } else {
            fprintf(stderr, "ERROR is.\n");
            std::abort();
        }
    }
}

void PrintPaginator::new_page() {
    const int pages_per_foil = 32;
    const auto foil_num = rend->page_num() / pages_per_foil;
    const auto foil_page_num = rend->page_num() % pages_per_foil;
    if(foil_page_num == 1 && foil_num > 0) {
        const int bufsize = 128;
        char buf[bufsize];
        auto loc = doc.data.title.find(":");
        if(loc != std::string::npos) {
            auto titletxt = doc.data.title.substr(0, loc - 2);
            snprintf(buf, bufsize, "%s — %d", titletxt.c_str(), foil_num + 1);
            auto style = styles.normal.font;
            style.size = Length::from_pt(7);
            auto printloc = page.h - m.lower + 3 * styles.normal.line_height;
            rend->render_text_as_is(buf, style, current_left_margin(), printloc);
        }
    }
    rend->new_page();
    if(debug_page) {
        rend->draw_box(Length::zero(), Length::zero(), page.w, page.h, 0.8, Length::from_pt(0.5));
        rend->draw_box(current_left_margin(),
                       m.lower,
                       textblock_width(),
                       textblock_height(),
                       0.8,
                       Length::from_pt(0.5));
    }
}

void PrintPaginator::draw_edge_markers(size_t chapter_number, size_t page_number) {
    assert(chapter_number > 0);
    const Length stroke_width = Length::from_mm(5);
    const Length tab_height = 1.5 * stroke_width;
    Length x = (page_number % 2) ? page.w : Length::zero();
    // move downwards per chapter
    Length y =
        page.h / 2 + (5 - (((int64_t)chapter_number - 1) % 10)) * tab_height + stroke_width / 2;

    rend->fill_rounded_corner_box(x - stroke_width / 2, y, stroke_width, tab_height, 0.8);
}

void PrintPaginator::draw_page_number(size_t page_number) {
    const Length x = (page_number % 2) ? page.w - m.outer : m.outer;
    const Length y = styles.normal.line_height * 2;
    const TextAlignment align = (page_number % 2) ? TextAlignment::Right : TextAlignment::Left;
    // https://gitlab.gnome.org/GNOME/pango/-/issues/855
    char buf[80];
    snprintf(buf, 80, "%d", (int)page_number);
    rend->render_text_as_is(buf, styles.normal.font, x, y, align);
}

void PrintPaginator::build_main_text() {
    ExtraPenaltyAmounts extras;
    bool first_paragraph = true;

    assert(std::holds_alternative<Section>(doc.elements.front()));
    for(const auto &e : doc.elements) {

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
        } else if(auto *fig = std::get_if<Figure>(&e)) {
            const auto fullpath = doc.data.top_dir / fig->file;
            ImageElement imel;
            imel.info = rend->get_image(fullpath.c_str());
            imel.ppi = 1200;
            auto display_height = Length::from_mm(double(imel.info.h) / imel.ppi * 25.4);
            imel.height_in_lines = display_height.pt() / styles.normal.line_height.pt() + 1;
            elements.emplace_back(std::move(imel));
        } else if(auto *cb = std::get_if<CodeBlock>(&e)) {
            elements.emplace_back(EmptyLineElement{1});
            create_codeblock(*cb);
            elements.emplace_back(EmptyLineElement{1});
            first_paragraph = true;
        } else if(auto *sc = std::get_if<SceneChange>(&e)) {
            (void)sc;
            elements.emplace_back(EmptyLineElement{1});
            first_paragraph = true;
        } else if(auto *foot = std::get_if<Footnote>(&e)) {
            (void)foot;
            // FIXME.
        } else if(auto *letter = std::get_if<Letter>(&e)) {
            elements.emplace_back(EmptyLineElement{1});
            create_letter(*letter);
            elements.emplace_back(EmptyLineElement{1});
            first_paragraph = true;
        } else if(auto *sign = std::get_if<SignBlock>(&e)) {
            elements.emplace_back(EmptyLineElement{1});
            create_sign(*sign);
            elements.emplace_back(EmptyLineElement{1});
            first_paragraph = true;
        } else {
            fprintf(stderr, "Maintext entry not supported yet.\n");
            std::abort();
        }
    }
    printf("Optimizing page splits.\n");
    optimize_page_splits();
    // create_pdf();
}

void PrintPaginator::create_codeblock(const CodeBlock &cb) {
    SpecialTextElement el;
    el.extra_indent = spaces.codeblock_indent;
    el.font = &styles.code.font;
    el.alignment = TextAlignment::Left;
    for(const auto &line : cb.raw_lines) {
        std::vector<HBRun> bob;
        bob.emplace_back(styles.code.font, line);
        el.lines.emplace_back(
            TextDrawCommand{std::move(bob), Length::zero(), Length::zero(), el.alignment});
    }
    elements.emplace_back(std::move(el));
}

void PrintPaginator::create_sign(const SignBlock &sign) {
    SpecialTextElement el;
    ExtraPenaltyAmounts extra;
    el.extra_indent = Length::zero();
    el.font = &styles.normal.font;
    const auto textwidth = textblock_width();
    for(const auto &line : sign.raw_lines) {
        // FIXME. This should not be done here. Smallcapsness should be a
        // font property instead.
        if(line.empty() || line.front() != '|') {
            std::string tmpline("|");
            tmpline += line;
            tmpline += '|';
            std::vector<EnrichedWord> processed_words = text_to_formatted_words(tmpline);
            ParagraphFormatter b(processed_words, textwidth, styles.normal, extra, fc);
            auto lines = b.split_formatted_lines();
            el.extra_indent = textblock_width() / 2;
            el.alignment = TextAlignment::Centered;
            auto rag_lines = build_ragged_paragraph(lines, el.alignment);
            assert(rag_lines.size() == 1);
            el.lines.emplace_back(std::move(rag_lines.front()));
        } else {
            std::abort();
            /*
            el.lines.emplace_back(MarkupDrawCommand{line.c_str(),
                                                    &styles.normal.font,
                                                    Length::zero(),
                                                    Length::zero(),
                                                    TextAlignment::Centered});
*/
        }
    }
    elements.emplace_back(std::move(el));
}

void PrintPaginator::create_letter(const Letter &letter) {
    ExtraPenaltyAmounts extra;
    size_t par_number = 0;
    for(const auto &partext : letter.paragraphs) {
        if(par_number > 0) {
            elements.emplace_back(EmptyLineElement{1});
        }
        SpecialTextElement el;
        el.extra_indent = spaces.codeblock_indent;
        el.font = &styles.letter.font;
        el.alignment = TextAlignment::Left;
        auto paragraph_width = textblock_width() - 2 * spaces.letter_indent;
        std::vector<EnrichedWord> processed_words = text_to_formatted_words(partext);
        ParagraphFormatter b(processed_words, paragraph_width, styles.letter, extra, fc);
        auto lines = b.split_formatted_lines();
        el.extra_indent = spaces.letter_indent;
        el.lines = build_ragged_paragraph(lines, el.alignment);
        elements.emplace_back(std::move(el));
        ++par_number;
    }
}

void PrintPaginator::optimize_page_splits() {
    TextElementIterator start(elements);
    TextElementIterator end(start);
    size_t target_height = textblock_height().mm() / styles.normal.line_height.mm();
    end.element_id = elements.size();
    end.line_id = 0;
    maintext_sections.reserve(100);
    assert(maintext_sections.empty());
    size_t section_number = 1;
    while(start != end) {
        printf("Optimizing section %d.\n", (int)section_number);
        auto next = start;
        do {
            next.next_element();
        } while(!(next == end || std::holds_alternative<SectionElement>(next.element())));
        ChapterFormatter chf(start, next, elements, target_height);
        auto optimized_chapter = chf.optimize_pages();
        print_stats(optimized_chapter, section_number);
        maintext_sections.emplace_back(std::move(optimized_chapter.pages));
        ++section_number;
        start = next;
    }
}

void PrintPaginator::create_section(const Section &s, const ExtraPenaltyAmounts &extras) {
    SectionElement selem;
    const auto paragraph_width = page.w - m.inner - m.outer;
    const auto section_width = 0.8 * paragraph_width;
    printf("Processing section: %s\n", s.text.c_str());
    // chapter_start_page = rend->page_num();
    // rend->add_section_outline(s.number, s.text);
    assert(s.level == 1);
    selem.chapter_number = s.number;
    // Fancy stuff above the text.
    std::string title_string;
    TextAlignment section_alignment = TextAlignment::Centered;
    Length rel_y = Length::zero();
    title_string = "·";
    title_string += std::to_string(s.number);
    title_string += "·";
    std::vector<HBRun> bob;
    bob.emplace_back(styles.section.font, title_string);
    selem.lines.emplace_back(
        TextDrawCommand{std::move(bob), textblock_width() / 2, rel_y, TextAlignment::Centered});
    title_string = s.text;
    // The title. Hyphenation is prohibited.
    const bool only_number_in_chapter_heading = true;
    if(!only_number_in_chapter_heading) {
        std::vector<EnrichedWord> processed_words = text_to_formatted_words(title_string, false);
        ParagraphFormatter b(processed_words, section_width, styles.section, extras, fc);
        auto lines = b.split_formatted_lines();
        auto built_lines = build_ragged_paragraph(lines, section_alignment);
        for(auto &line : built_lines) {
            selem.lines.emplace_back(std::move(line));
        }
    }
    elements.emplace_back(std::move(selem));
    elements.emplace_back(EmptyLineElement{1});
}

void PrintPaginator::create_paragraph(const Paragraph &p,
                                      const ExtraPenaltyAmounts &extras,
                                      const HBChapterParameters &chpar,
                                      Length extra_indent) {
    ParagraphElement pelem;
    pelem.paragraph_width = textblock_width() - 2 * extra_indent;
    std::vector<EnrichedWord> processed_words = text_to_formatted_words(p.text);
    ParagraphFormatter b(processed_words, pelem.paragraph_width, chpar, extras, fc);
    auto lines = b.split_formatted_lines();
    pelem.params = chpar;
    pelem.lines = build_justified_paragraph(lines, chpar, pelem.paragraph_width);
    // Shift sideways
    elements.emplace_back(std::move(pelem));
}

std::vector<TextCommands>
PrintPaginator::build_justified_paragraph(const std::vector<HBLine> &lines,
                                          const HBChapterParameters &text_par,
                                          const Length target_width) {
    Length rel_y = Length::zero();
    std::vector<TextCommands> line_commands;
    line_commands.reserve(lines.size());
    size_t line_num = 0;
    for(const auto &line : lines) {
        Length current_indent = line_num == 0 ? text_par.indent : Length{};
        if(line_num < lines.size() - 1) {
            line_commands.emplace_back(
                JustifiedTextDrawCommand{line /* FIXME, should do a std::move */,
                                         current_indent,
                                         rel_y,
                                         target_width - current_indent});
        } else {
            line_commands.emplace_back(
                TextDrawCommand{line2runs(line), current_indent, rel_y, TextAlignment::Left});
        }
        line_num++;
        rel_y += text_par.line_height;
    }
    return line_commands;
}

std::vector<TextCommands> PrintPaginator::build_ragged_paragraph(const std::vector<HBLine> &lines,
                                                                 const TextAlignment alignment) {
    std::vector<TextCommands> line_commands;
    const auto rel_x =
        alignment == TextAlignment::Centered ? textblock_width() / 2 : Length::zero();
    const auto rel_y = Length::zero(); // FIXME, eventually remove.
    line_commands.reserve(lines.size());
    std::vector<HBRun> all_line_runs;
    for(const auto &line : lines) {
        line_commands.emplace_back(TextDrawCommand{line2runs(line), rel_x, rel_y, alignment});
    }
    return line_commands;
}

std::vector<EnrichedWord> PrintPaginator::text_to_formatted_words(const std::string &text,
                                                                  bool permit_hyphenation) {
    StyleStack current_style("dummy", styles.code.font.size);
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

void PrintPaginator::dump_text(const char *path) {
    FILE *f = fopen(path, "w");
    std::unique_ptr<FILE, int (*)(FILE *)> fcloser(f, fclose);

    size_t page_num = 0;
    size_t section_num = 0;
    for(const auto &s : maintext_sections) {
        ++section_num;
        fprintf(f, "\n -- SECTION %d --\n\n", (int)section_num);
        for(const auto &p : s) {
            ++page_num;
            fprintf(f, "%s -- PAGE %d --\n\n", (page_num != 1) ? "\n" : "", (int)page_num);
            if(auto *reg = std::get_if<RegularPage>(&p)) {
                TextElementIterator previous = reg->main_text.start;
                for(TextElementIterator it = reg->main_text.start; it != reg->main_text.end; ++it) {
                    if(previous.element_id != it.element_id) {
                        fprintf(f, "\n");
                    }
                    if(std::holds_alternative<ImageElement>(it.element())) {
                        fprintf(f, "-- IMAGE --");
                    } else {
                        plaintextprinter(f, it.line());
                    }
                    fprintf(f, "\n");
                    previous = it;
                }
            } else if(auto *sec = std::get_if<SectionPage>(&p)) {
                fprintf(f, "Chapter %d\n", (int)sec->section);
                TextElementIterator previous = sec->main_text.start;
                for(TextElementIterator it = sec->main_text.start; it != sec->main_text.end; ++it) {
                    if(previous.element_id != it.element_id) {
                        fprintf(f, "\n");
                    }
                    plaintextprinter(f, it.line());
                    fprintf(f, "\n");
                    previous = it;
                }
            } else {
                std::abort();
            }
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

void PrintPaginator::print_stats(const PageLayoutResult &res, size_t section_number) {
    fprintf(stats, "-- Section %d --\n\n", (int)section_number);
    const size_t page_number_offset = 1;
    for(size_t page_num = 0; page_num < res.pages.size(); ++page_num) {
        fprintf(stats, "-- Page %d --\n\n", (int)(page_num + page_number_offset));

        for(const auto &p : res.stats.orphans) {
            if(p == page_number_offset + page_num) {
                fprintf(stats, "Orphan line.\n");
                break;
            }
        }
        for(const auto &p : res.stats.widows) {
            if(p == page_number_offset + page_num) {
                fprintf(stats, "Widow line.\n");
                break;
            }
        }

        // Mismatch
        for(const auto &mismatch : res.stats.mismatches) {
            if(mismatch.page_number == page_number_offset + page_num) {
                fprintf(stats, "Height mismatch.\n");
            }
        }

        if(res.stats.single_line_last_page && page_num == res.pages.size() - 1) {
            fprintf(stats, "FATAL: single line page.\n");
        }
        fprintf(stats, "\n");
    }
}
