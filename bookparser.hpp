#pragma once

#include <glib.h>

#include <string>
#include <string_view>
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

std::string get_normalized_string(std::string_view v);

enum class SpecialBlockType : int { Code, Footnote, Unset };

struct ReMatchResult {
    re_match minfo;
    int64_t offset_to_match_start;
    std::string_view whole_match;

    ReMatchOffsets offsets_for(int group);
    std::string_view view_for(int group, const char *original_data);
};

struct SectionDecl {
    int level;
    std::string_view text;
};

struct PlainLine {
    std::string_view text;
};

struct NewLine {};

struct SceneDecl {};

struct NewBlock {};

struct StartOfSpecialBlock {
    SpecialBlockType type;
};

struct EndOfSpecialBlock {};

struct EndOfFile {};

typedef std::variant<SectionDecl,
                     PlainLine,
                     NewLine,
                     SceneDecl,
                     NewBlock,
                     StartOfSpecialBlock,
                     EndOfSpecialBlock,
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

struct Footnote {
    int number;
    std::string text;
};

struct SceneChange {};

// Also needs images, footnotes, unformatted text etc.
typedef std::variant<Paragraph, Section, SceneChange, CodeBlock, Footnote> DocElement;

class LineParser {
public:
    LineParser(const char *data_, const int64_t data_size_) : data(data_), data_size(data_size_) {
        whitespace = g_regex_new("  \\s+", GRegexCompileFlags(0), G_REGEX_MATCH_ANCHORED, nullptr);
        section =
            g_regex_new("(#+)\\s+(.*)", GRegexCompileFlags(0), G_REGEX_MATCH_ANCHORED, nullptr);
        line = g_regex_new(".+", GRegexCompileFlags(0), G_REGEX_MATCH_ANCHORED, nullptr);
        newline = g_regex_new("\\n+", G_REGEX_MULTILINE, G_REGEX_MATCH_ANCHORED, nullptr);
        scene = g_regex_new("#s", GRegexCompileFlags(0), G_REGEX_MATCH_ANCHORED, nullptr);
        specialblock_start =
            g_regex_new("```(\\w+)", GRegexCompileFlags(0), G_REGEX_MATCH_ANCHORED, nullptr);
        specialblock_end =
            g_regex_new("``` *\n", G_REGEX_MULTILINE, G_REGEX_MATCH_ANCHORED, nullptr);
    }

    ~LineParser() {
        g_regex_unref(newline);
        g_regex_unref(section);
        g_regex_unref(line);
        g_regex_unref(whitespace);
        g_regex_unref(specialblock_start);
        g_regex_unref(specialblock_end);
    }

    line_token next();

private:
    std::optional<ReMatchResult> try_match(GRegex *regex, GRegexMatchFlags flags) {
        GMatchInfo *minfo = nullptr;
        if(g_regex_match(regex, data + offset, flags, &minfo)) {
            ReMatchResult match_info{re_match{minfo}, offset, std::string_view{}};
            match_info.whole_match = match_info.view_for(0, data);
            offset += match_info.whole_match.length();
            return match_info;
        }
        g_match_info_free(minfo);
        return {};
    }

    const char *data;
    bool parsing_specialblock = false;
    int64_t data_size;
    int64_t offset = 0;
    GRegex *whitespace;
    GRegex *section;
    GRegex *line;
    GRegex *newline;
    GRegex *scene;
    GRegex *specialblock_start;
    GRegex *specialblock_end;
};

struct Document {
    // Add metadata entries for things like name, ISBN, authors etc.
    std::vector<DocElement> elements;

    int num_chapters() const;
    int num_footnotes() const;
};

class StructureParser {
public:
    ~StructureParser();
    void push(const line_token &l);
    Document get_document();

private:
    enum class ParsingState : int {
        unset,
        paragraph,
        section,
        specialblock,
    };

    void set_state(ParsingState new_state);

    void build_element();

    std::string pop_lines_to_string();

    Document doc;
    bool has_finished = false;
    int section_level = 1; // FIXME
    int section_number = 0;
    int footnote_number = 0;
    ParsingState current_state = ParsingState::unset;
    SpecialBlockType current_special = SpecialBlockType::Unset;
    std::vector<std::string> stored_lines;
};
