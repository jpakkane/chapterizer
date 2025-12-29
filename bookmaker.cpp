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

#include <printpaginator.hpp>
#include <draftpaginator.hpp>
#include <epub.hpp>
#include <utils.hpp>

#include <glib.h>
#include <cassert>
#include <cstring>

void replace_dashes(std::string &str) {
    size_t dash_loc;
    while((dash_loc = str.find("---")) != std::string::npos) {
        str.replace(dash_loc, 3, "—");
    }

    while((dash_loc = str.find("--")) != std::string::npos) {
        str.replace(dash_loc, 2, "–");
    }
}

void replace_quotes(std::string &str) {
    size_t quote_loc;
    while((quote_loc = str.find('"')) != std::string::npos) {
        str.replace(quote_loc, 1, "”");
    }

    while((quote_loc = str.find("'")) != std::string::npos) {
        str.replace(quote_loc, 1, "’");
    }
    // Support only Finnish style for now.
}

void replace_ellipses(std::string &str) {
    size_t quote_loc;
    while((quote_loc = str.find("...")) != std::string::npos) {
        str.replace(quote_loc, 3, "…");
    }
    // Assume other types do not exist.
}

void replace_characters(std::string &str) {
    replace_dashes(str);
    replace_quotes(str);
    replace_ellipses(str);
}

void preprocess_document(Document &d) {
    for(auto &e : d.elements) {
        if(auto *p = std::get_if<Paragraph>(&e)) {
            replace_characters(p->text);
        } else if(auto *s = std::get_if<Section>(&e)) {
            replace_characters(s->text);
        } else if(auto *sc = std::get_if<SceneChange>(&e)) {
            (void)sc;
        } else if(auto *code = std::get_if<CodeBlock>(&e)) {
            // Code is always written as-is.
            (void)code;
        } else if(auto *footnote = std::get_if<Footnote>(&e)) {
            (void)footnote;
            std::abort();
        } else if(auto *figure = std::get_if<Figure>(&e)) {
            (void)figure;
            std::abort();
        } else if(auto *letter = std::get_if<Letter>(&e)) {
            for(auto &lp : letter->paragraphs) {
                replace_characters(lp);
            }
        } else if(auto *sign = std::get_if<SignBlock>(&e)) {
            for(auto &l : sign->raw_lines) {
                replace_characters(l);
            }
        } else if(auto *menu = std::get_if<Menu>(&e)) {
            for(auto &l : menu->raw_lines) {
                replace_characters(l);
            }
        } else {
            printf("Unknown type in preprocess.\n");
            std::abort();
        }
    }
}

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
                printf("Input file %s contains a prohibited invisible ASCII control character "
                       "%d.\n",
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
    // std::string bob("Hello \" there --- my ... man");
    // replace_characters(bob);
    auto doc = load_document(argv[1]);
    preprocess_document(doc);
    if(doc.data.generate_pdf) {
        if(doc.data.is_draft) {
            DraftPaginator p(doc);
            auto ofile = doc.data.top_dir / doc.data.pdf.ofname;
            p.generate_pdf(ofile.c_str());
        } else {
            PrintPaginator p(doc);
            auto ofile = doc.data.top_dir / doc.data.pdf.ofname;
            p.generate_pdf(ofile.c_str());
        }
    }
    if(doc.data.generate_epub) {
        Epub epub(doc);
        epub.generate(doc.data.epub.ofname.c_str());
    }
    return 0;
}
