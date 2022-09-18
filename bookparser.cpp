#include "bookparser.hpp"

#include <cassert>

ReMatchOffsets ReMatchResult::offsets_for(int group) {
    gint start_pos, end_pos;
    g_match_info_fetch_pos(minfo.get(), gint(group), &start_pos, &end_pos);
    assert(group != 0 || start_pos == 0);
    return ReMatchOffsets{offset_to_match_start + start_pos, offset_to_match_start + end_pos};
}

line_token LineParser::next() {
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
