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
};

struct Section {
    int level;
    ReMatchOffsets off;
};

struct PlainLine {
    ReMatchOffsets off;
};

struct NewLine {};

struct ChapterChange {};

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

    std::optional<std::string> next() {
        if(offset >= data_size) {
            return {};
        }

        auto match_result = try_match(newline, G_REGEX_MATCH_ANCHORED);
        if(match_result) {
            return "NL";
        }
        match_result = try_match(section, GRegexMatchFlags(0));
        if(match_result) {
            return "SEC";
        }
        match_result = try_match(line, GRegexMatchFlags(0));
        if(match_result) {
            return "LINE";
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

int parse_file(const char *data, const uintmax_t data_size) {
    int result = 0;
    uintmax_t offset = 0;
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
    std::optional<std::string> token = p.next();
    while(token) {
        printf("%s\n", token.value().c_str());
        token = p.next();
    }

    return result;
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
    printf("%d lines\n", document);

    //    cairo_status_t status;
    cairo_surface_t *surface = cairo_pdf_surface_create("parsingtest.pdf", 595, 842);
    cairo_t *cr = cairo_create(surface);
    cairo_save(cr);
    // cairo_set_source_rgb(cr, 1.0, 0.2, 0.1);
    PangoLayout *layout = pango_cairo_create_layout(cr);
    int line_num = -1;

    int y = 72;

    cairo_surface_destroy(surface);
    cairo_destroy(cr);
    return 0;
}
