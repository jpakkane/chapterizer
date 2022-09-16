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
#include <memory>

struct MatchDeleter final {
    void operator()(GMatchInfo *mi) { g_match_info_unref(mi); }
};

typedef std::unique_ptr<GMatchInfo, MatchDeleter> re_match;

double mm2pt(const double x) { return x * 2.8346456693; }
double pt2mm(const double x) { return x / 2.8346456693; }

namespace fs = std::filesystem;

struct ReMatchOffsets {
    int64_t start_pos;
    int64_t end_pos;

    std::string get_normalized_string(const char *original_data) const {
        gchar *norm =
            g_utf8_normalize(original_data + start_pos, end_pos - start_pos, G_NORMALIZE_NFC);
        std::string result{norm};
        g_free(norm);
        return result;
    }
};

struct ReMatchResult {
    re_match minfo;
    int64_t offset_to_match_start;
    ReMatchOffsets whole_match;

    ReMatchOffsets offsets_for(int group) {
        gint start_pos, end_pos;
        g_match_info_fetch_pos(minfo.get(), gint(group), &start_pos, &end_pos);
        assert(group != 0 || start_pos == 0);
        return ReMatchOffsets{offset_to_match_start + start_pos, offset_to_match_start + end_pos};
    }
};

struct SectionDecl {
    int level;
    ReMatchOffsets off;
};

struct PlainLine {
    ReMatchOffsets off;
};

struct NewLine {};

struct SceneDecl {};

struct NewBlock {};

struct StartOfCodeBlock {};

struct EndOfCodeBlock {};

struct EndOfFile {};

typedef std::variant<SectionDecl,
                     PlainLine,
                     NewLine,
                     SceneDecl,
                     NewBlock,
                     StartOfCodeBlock,
                     EndOfCodeBlock,
                     EndOfFile>
    line_token;

struct Paragraph {
    std::string text;
};

struct Section {
    int level;
    int number;
    std::string text;
};

struct CodeBlock {
    std::vector<std::string> raw_lines;
};

struct SceneChange {};

// Also needs images, footnotes, unformatted text etc.
typedef std::variant<Paragraph, Section, SceneChange, CodeBlock> DocElement;

class BasicParser {
public:
    BasicParser(const char *data_, const int64_t data_size_) : data(data_), data_size(data_size_) {
        whitespace = g_regex_new("  \\s+", GRegexCompileFlags(0), G_REGEX_MATCH_ANCHORED, nullptr);
        section =
            g_regex_new("(#+)\\s+(.*)", GRegexCompileFlags(0), G_REGEX_MATCH_ANCHORED, nullptr);
        line = g_regex_new(".+", GRegexCompileFlags(0), G_REGEX_MATCH_ANCHORED, nullptr);
        newline = g_regex_new("\\n+", G_REGEX_MULTILINE, G_REGEX_MATCH_ANCHORED, nullptr);
        scene = g_regex_new("#s", GRegexCompileFlags(0), G_REGEX_MATCH_ANCHORED, nullptr);
        codeblock_start =
            g_regex_new("```(\\w+)", GRegexCompileFlags(0), G_REGEX_MATCH_ANCHORED, nullptr);
        codeblock_end = g_regex_new("``` *\n", G_REGEX_MULTILINE, G_REGEX_MATCH_ANCHORED, nullptr);
    }

    ~BasicParser() {
        g_regex_unref(newline);
        g_regex_unref(section);
        g_regex_unref(line);
        g_regex_unref(whitespace);
        g_regex_unref(codeblock_start);
        g_regex_unref(codeblock_end);
    }

    line_token next() {
        if(offset >= data_size) {
            return EndOfFile{};
        }

        if(parsing_codeblock) {
            auto block_end = try_match(codeblock_end, GRegexMatchFlags(0));
            if(block_end) {
                parsing_codeblock = false;
                return EndOfCodeBlock{};
            } else {
                auto full_line = try_match(line, GRegexMatchFlags(0));
                auto nl = try_match(newline, GRegexMatchFlags(0));
                if(!nl) {
                    std::abort();
                }
                if(full_line) {
                    return PlainLine{full_line->whole_match};
                }
                return PlainLine{0, 0}; // Empty line.
            }
        }
        auto match_result = try_match(newline, GRegexMatchFlags(0));
        if(match_result) {
            if(match_result->whole_match.end_pos - match_result->whole_match.start_pos > 1) {
                return NewBlock{};
            }
            return NewLine{};
        }
        match_result = try_match(codeblock_start, GRegexMatchFlags(0));
        if(match_result) {
            if(parsing_codeblock) {
                printf("Nested codeblocks not supported.\n");
                std::abort();
            }
            parsing_codeblock = true;
            if(!try_match(newline, GRegexMatchFlags(0))) {
                std::abort();
            }
            return StartOfCodeBlock{};
        }
        match_result = try_match(codeblock_end, GRegexMatchFlags(0));
        if(match_result) {
            printf("End of codeblock without start of same.\n");
            std::abort();
        }
        match_result = try_match(scene, GRegexMatchFlags(0));
        if(match_result) {
            return SceneDecl{};
        }
        match_result = try_match(section, GRegexMatchFlags(0));
        if(match_result) {
            const auto hash_offsets = match_result->offsets_for(1);
            const auto text_offsets = match_result->offsets_for(2);
            const int depth = hash_offsets.end_pos - hash_offsets.start_pos;
            assert(depth == 1); // Fix eventually.

            return SectionDecl{depth, text_offsets};
        }
        match_result = try_match(line, GRegexMatchFlags(0));
        if(match_result) {
            return PlainLine{match_result->whole_match};
        }

        printf("Parsing failed.");
        std::abort();
    }

private:
    std::optional<ReMatchResult> try_match(GRegex *regex, GRegexMatchFlags flags) {
        GMatchInfo *minfo = nullptr;
        if(g_regex_match(regex, data + offset, flags, &minfo)) {
            ReMatchResult match_info{re_match{minfo}, offset, ReMatchOffsets{0, 0}};
            match_info.whole_match = match_info.offsets_for(0);
            offset += match_info.whole_match.end_pos - match_info.whole_match.start_pos;
            return match_info;
        }
        g_match_info_free(minfo);
        return {};
    }

    const char *data;
    bool parsing_codeblock = false;
    int64_t data_size;
    int64_t offset = 0;
    GRegex *whitespace;
    GRegex *section;
    GRegex *line;
    GRegex *newline;
    GRegex *scene;
    GRegex *codeblock_start;
    GRegex *codeblock_end;
};

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

    BasicParser p(data, data_size);
    line_token token = p.next();
    int section_number = 0;
    while(!std::holds_alternative<EndOfFile>(token)) {
        if(std::holds_alternative<SectionDecl>(token)) {
            auto &s = std::get<SectionDecl>(token);
            assert(section_text.empty());
            section_text = s.off.get_normalized_string(data);
            ++section_number;
        } else if(std::holds_alternative<PlainLine>(token)) {
            auto &l = std::get<PlainLine>(token);
            if(in_codeblock) {
                codeblock_text.emplace_back(l.off.get_normalized_string(data));
            } else {
                if(!section_text.empty()) {
                    assert(paragraph_text.empty());
                    section_text += ' ';
                    section_text += l.off.get_normalized_string(data);
                } else {
                    if(!paragraph_text.empty()) {
                        paragraph_text += ' ';
                    }
                    paragraph_text += l.off.get_normalized_string(data);
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
