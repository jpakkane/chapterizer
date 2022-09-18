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

#include <pango/pangocairo.h>
#include <cairo-pdf.h>
#include <fcntl.h>
#include <sys/mman.h>

#include <vector>
#include <string>

#include <clocale>
#include <cassert>

#include <filesystem>

#include <bookparser.hpp>

namespace fs = std::filesystem;

class StructureParser {};

struct Document {
    // Add metadata entries for things like name, ISBN, authors etc.
    std::vector<DocElement> elements;
};

Document parse_file(const char *data, const uintmax_t data_size) {
    Document doc;
    if(!g_utf8_validate(data, data_size, nullptr)) {
        printf("Invalid utf-8.\n");
        std::abort();
    }
    GError *err = nullptr;
    if(err) {
        std::abort();
    }

    std::string section_text;
    std::string paragraph_text;
    std::vector<std::string> codeblock_text;
    bool in_codeblock = false;

    LineParser p(data, data_size);
    line_token token = p.next();
    int section_number = 0;
    while(!std::holds_alternative<EndOfFile>(token)) {
        if(std::holds_alternative<SectionDecl>(token)) {
            auto &s = std::get<SectionDecl>(token);
            assert(section_text.empty());
            section_text = get_normalized_string(s.text);
            ++section_number;
        } else if(std::holds_alternative<PlainLine>(token)) {
            auto &l = std::get<PlainLine>(token);
            if(in_codeblock) {
                codeblock_text.emplace_back(get_normalized_string(l.text));
            } else {
                if(!section_text.empty()) {
                    assert(paragraph_text.empty());
                    section_text += ' ';
                    section_text += get_normalized_string(l.text);
                } else {
                    if(!paragraph_text.empty()) {
                        paragraph_text += ' ';
                    }
                    paragraph_text += get_normalized_string(l.text);
                }
            }
        } else if(std::holds_alternative<NewLine>(token)) {
            auto &nl = std::get<NewLine>(token);
            (void)nl;
        } else if(std::holds_alternative<NewBlock>(token)) {
            if(!section_text.empty()) {
                doc.elements.emplace_back(
                    Section{1, section_number, std::move(section_text)}); // FIXME
                section_text.clear();
            }
            if(!paragraph_text.empty()) {
                doc.elements.emplace_back(Paragraph{std::move(paragraph_text)});
                paragraph_text.clear();
            }
        } else if(std::holds_alternative<SceneDecl>(token)) {
            doc.elements.emplace_back(SceneChange{});
        } else if(std::holds_alternative<StartOfCodeBlock>(token)) {
            assert(codeblock_text.empty());
            // FIXME, duplication.
            if(!section_text.empty()) {
                doc.elements.emplace_back(
                    Section{1, section_number, std::move(section_text)}); // FIXME
                section_text.clear();
            }
            if(!paragraph_text.empty()) {
                doc.elements.emplace_back(Paragraph{std::move(paragraph_text)});
                paragraph_text.clear();
            }
            in_codeblock = true;
        } else if(std::holds_alternative<EndOfCodeBlock>(token)) {
            doc.elements.emplace_back(CodeBlock{std::move(codeblock_text)});
            codeblock_text.clear();
            in_codeblock = false;
        } else {
            std::abort();
        }
        token = p.next();
    }

    // FIXME, add stored data if any.
    return doc;
}

int main(int argc, char **argv) {
    setlocale(LC_ALL, "");
    if(argc != 2) {
        printf("%s <input file>\n", argv[0]);
        return 1;
    }
    fs::path infile{argv[1]};
    auto fsize = fs::file_size(infile);
    int fd = open(argv[1], O_RDONLY);
    const char *data =
        static_cast<const char *>(mmap(nullptr, fsize, PROT_READ, MAP_PRIVATE, fd, 0));
    assert(data);
    auto document = parse_file(data, fsize);
    close(fd);
    printf("%d elements\n", (int)document.elements.size());

    for(const auto &s : document.elements) {
        if(std::holds_alternative<Section>(s)) {
            printf("Section\n");
        } else if(std::holds_alternative<Paragraph>(s)) {
            printf("Paragraph\n");
        } else if(std::holds_alternative<SceneChange>(s)) {
            printf("Scene change\n");
        } else if(std::holds_alternative<CodeBlock>(s)) {
            printf("Code block\n");
        } else {
            printf("Unknown variant type.\n");
            std::abort();
        }
    }

    //    cairo_status_t status;
    cairo_surface_t *surface = cairo_pdf_surface_create("parsingtest.pdf", 595, 842);
    cairo_t *cr = cairo_create(surface);
    cairo_save(cr);
    // cairo_set_source_rgb(cr, 1.0, 0.2, 0.1);
    PangoLayout *layout = pango_cairo_create_layout(cr);

    cairo_move_to(cr, 72, 72);
    pango_layout_set_text(layout, "Not done yet.", -1);
    pango_cairo_update_layout(cr, layout);
    pango_cairo_show_layout(cr, layout);

    cairo_surface_destroy(surface);
    cairo_destroy(cr);
    return 0;
}
