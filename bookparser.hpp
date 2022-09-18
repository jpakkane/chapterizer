#pragma once

#include <glib.h>

#include <string>
#include <vector>
#include <optional>
#include <variant>
#include <memory>

struct MatchDeleter final {
    void operator()(GMatchInfo *mi) { g_match_info_unref(mi); }
};

typedef std::unique_ptr<GMatchInfo, MatchDeleter> re_match;

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

    ReMatchOffsets offsets_for(int group);
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

class LineParser {
public:
    LineParser(const char *data_, const int64_t data_size_) : data(data_), data_size(data_size_) {
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

    ~LineParser() {
        g_regex_unref(newline);
        g_regex_unref(section);
        g_regex_unref(line);
        g_regex_unref(whitespace);
        g_regex_unref(codeblock_start);
        g_regex_unref(codeblock_end);
    }

    line_token next();

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
