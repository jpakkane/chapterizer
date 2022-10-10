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
#include <epub.hpp>
#include <utils.hpp>

#include <glib.h>
#include <cassert>
#include <cstring>

Document load_document(const char *fname) {
    Document doc;
    doc.data = load_book_json(fname);

    StructureParser strucp(doc);
    for(const auto &s : doc.data.sources) {
        const auto fpath = doc.data.top_dir / s;
        MMapper map(fpath.c_str());
        if(!g_utf8_validate(map.data(), map.size(), nullptr)) {
            printf("Invalid utf-8.\n");
            std::abort();
        }
        for(const auto c : map.view()) {
            if(c == '\t') {
                printf("Input file %s contains a TAB character. These are prohibited in input "
                       "files.\n",
                       fpath.c_str());
                std::abort();
            }
            if(c == 0 || c == '\n' || c >= 32 || c < 0) {
                // OK.
            } else {
                printf(
                    "Input file %s contains a prohibited invisible ASCII control character %d.\n",
                    fpath.c_str(),
                    int(c));
                std::abort();
            }
        }

        GError *err = nullptr;
        if(err) {
            std::abort();
        }

        LineParser linep(map.data(), map.size());
        line_token token = linep.next();
        while(!std::holds_alternative<EndOfFile>(token)) {
            strucp.push(token);
            token = linep.next();
        }
        strucp.push(token);
    }
    return doc;
}

int main(int argc, char **argv) {
    if(argc != 2) {
        printf("%s <bookdef.json>\n", argv[0]);
        return 1;
    }
    auto doc = load_document(argv[1]);
    if(doc.data.generate_pdf) {
        Paginator p(doc);
        auto ofile = doc.data.top_dir / doc.data.pdf.ofname;
        p.generate_pdf(ofile.c_str());
    }
    if(doc.data.generate_epub) {
        Epub epub(doc);
        epub.generate("epub_test.epub");
    }
    return 0;
}
