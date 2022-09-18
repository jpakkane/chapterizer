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

    LineParser linep(data, data_size);
    StructureParser strucp;

    line_token token = linep.next();
    while(!std::holds_alternative<EndOfFile>(token)) {
        strucp.push(token);
        token = linep.next();
    }

    // FIXME, add stored data if any.
    return strucp.get_document();
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
