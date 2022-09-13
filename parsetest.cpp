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
#include <glib.h>
#include <fcntl.h>
#include <sys/mman.h>

#include <vector>
#include <string>

#include <clocale>
#include <cassert>

#include <filesystem>
#include <optional>
#include <variant>

double mm2pt(const double x) { return x * 2.8346456693; }
double pt2mm(const double x) { return x / 2.8346456693; }

namespace fs = std::filesystem;

struct ReMatchOffsets {
    int64_t start_pos;
    int64_t end_pos;

    std::string get_string(const char *original_data) const {
        gchar *norm =
            g_utf8_normalize(original_data + start_pos, end_pos - start_pos, G_NORMALIZE_NFC);
        std::string result{norm};
        g_free(norm);
        return result;
    }
};

struct Section {
    int level;
    ReMatchOffsets off;
};

struct PlainLine {
    ReMatchOffsets off;
};

struct NewLine {};

struct NewBlock {};

struct EndOfFile {};

typedef std::variant<Section, PlainLine, NewLine, NewBlock, EndOfFile> line_token;

class BasicParser {
public:
    BasicParser(const char *data_, const int64_t data_size_) : data(data_), data_size(data_size_) {
        whitespace = g_regex_new("^\\s+", GRegexCompileFlags(0), GRegexMatchFlags(0), nullptr);
        section = g_regex_new("^#+\\s+.*", GRegexCompileFlags(0), GRegexMatchFlags(0), nullptr);
        line = g_regex_new("^.+", GRegexCompileFlags(0), GRegexMatchFlags(0), nullptr);
        newline = g_regex_new("^\\n+", G_REGEX_MULTILINE, GRegexMatchFlags(0), nullptr);
    }

    ~BasicParser() {
        g_regex_unref(newline);
        g_regex_unref(section);
        g_regex_unref(line);
        g_regex_unref(whitespace);
    }

    line_token next() {
        if(offset >= data_size) {
            return EndOfFile{};
        }

        auto match_result = try_match(newline, G_REGEX_MATCH_ANCHORED);
        if(match_result) {
            if(match_result->end_pos - match_result->start_pos > 1) {
                return NewBlock{};
            }
            return NewLine{};
        }
        match_result = try_match(section, GRegexMatchFlags(0));
        if(match_result) {
            match_result->start_pos += 2; // FIXME
            return Section{1, *match_result};
        }
        match_result = try_match(line, GRegexMatchFlags(0));
        if(match_result) {
            return PlainLine{*match_result};
        }

        printf("Parsing failed.");
        std::abort();
    }

private:
    std::optional<ReMatchOffsets> try_match(GRegex *regex, GRegexMatchFlags flags) {
        GMatchInfo *minfo = nullptr;
        if(g_regex_match(regex, data + offset, flags, &minfo)) {
            gint start_pos, end_pos;
            g_match_info_fetch_pos(minfo, 0, &start_pos, &end_pos);
            assert(start_pos == 0);
            std::string word{data + start_pos, data + end_pos};
            ReMatchOffsets m;
            m.start_pos = offset + start_pos;
            m.end_pos = offset + end_pos;
            offset += end_pos - start_pos;
            g_match_info_unref(minfo);
            return m;
        }
        return {};
    }

    const char *data;
    int64_t data_size;
    int64_t offset = 0;
    GRegex *whitespace;
    GRegex *section;
    GRegex *line;
    GRegex *newline;
};

struct Document {
    std::vector<std::string> paragraphs;
    std::vector<std::string> sections;
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

    BasicParser p(data, data_size);
    line_token token = p.next();
    while(!std::holds_alternative<EndOfFile>(token)) {
        if(std::holds_alternative<Section>(token)) {
            auto &s = std::get<Section>(token);
            assert(section_text.empty());
            section_text = s.off.get_string(data);
        } else if(std::holds_alternative<PlainLine>(token)) {
            auto &l = std::get<PlainLine>(token);
            if(!section_text.empty()) {
                assert(paragraph_text.empty());
                section_text += ' ';
                section_text += l.off.get_string(data);
            } else {
                if(!paragraph_text.empty()) {
                    paragraph_text += ' ';
                }
                paragraph_text += l.off.get_string(data);
            }
        } else if(std::holds_alternative<NewLine>(token)) {
            auto &nl = std::get<NewLine>(token);
            (void)nl;
        } else if(std::holds_alternative<NewBlock>(token)) {
            auto &change = std::get<NewBlock>(token);
            if(!section_text.empty()) {
                doc.sections.emplace_back(std::move(section_text));
                section_text.clear();
            }
            if(!paragraph_text.empty()) {
                doc.paragraphs.emplace_back(std::move(paragraph_text));
                paragraph_text.clear();
            }
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
    printf("%d lines\n", (int)document.paragraphs.size());

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
