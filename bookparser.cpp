#include "bookparser.hpp"

#include <cassert>

std::string get_normalized_string(std::string_view v) {
    gchar *norm = g_utf8_normalize(v.data(), v.length(), G_NORMALIZE_NFC);
    std::string result{norm};
    g_free(norm);
    return result;
}

ReMatchOffsets ReMatchResult::offsets_for(int group) {
    gint start_pos, end_pos;
    g_match_info_fetch_pos(minfo.get(), gint(group), &start_pos, &end_pos);
    assert(group != 0 || start_pos == 0);
    return ReMatchOffsets{offset_to_match_start + start_pos, offset_to_match_start + end_pos};
}

std::string_view ReMatchResult::view_for(int group, const char *original_data) {
    ReMatchOffsets off = offsets_for(group);
    return std::string_view(original_data + off.start_pos, off.end_pos - off.start_pos);
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
            return PlainLine{std::string_view{}}; // Empty line.
        }
    }
    auto match_result = try_match(newline, GRegexMatchFlags(0));
    if(match_result) {
        if(match_result->whole_match.length() > 1) {
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
        const int depth = hash_offsets.end_pos - hash_offsets.start_pos;
        assert(depth == 1); // Fix eventually.

        return SectionDecl{depth, match_result->view_for(2, data)};
    }
    match_result = try_match(line, GRegexMatchFlags(0));
    if(match_result) {
        return PlainLine{match_result->whole_match};
    }

    printf("Parsing failed.");
    std::abort();
}

std::string StructureParser::pop_lines_to_string() {
    std::string line;
    for(const auto &l : stored_lines) {
        line += l;
        line += ' ';
    }
    line.pop_back();
    stored_lines.clear();
    return line;
}

void StructureParser::build_element() {
    switch(current_state) {
    case ParsingState::unset:
        std::abort();
    case ParsingState::codeblock:
        doc.elements.emplace_back(CodeBlock{std::move(stored_lines)});
        break;
    case ParsingState::section:
        doc.elements.emplace_back(Section{1, section_number, pop_lines_to_string()});
        break;
    case ParsingState::paragraph:
        doc.elements.emplace_back(Paragraph{pop_lines_to_string()});
        break;
    default:
        std::abort();
    }
}

void StructureParser::set_state(ParsingState new_state) {
    assert(current_state == ParsingState::unset || (new_state != current_state));
    if(current_state != ParsingState::unset) {
        build_element();
    }
    assert(stored_lines.empty());
    current_state = new_state;
}

void StructureParser::push(const line_token &l) {
    if(has_finished) {
        std::abort();
    }
    if(std::holds_alternative<PlainLine>(l)) {
        if(current_state == ParsingState::unset) {
            set_state(ParsingState::paragraph);
        }
        stored_lines.emplace_back(std::get<PlainLine>(l).text);
        return;
    }

    if(std::holds_alternative<NewLine>(l)) {
        return;
    }

    if(std::holds_alternative<SectionDecl>(l)) {
        set_state(ParsingState::section);
        ++section_number;
        stored_lines.emplace_back(std::get<SectionDecl>(l).text);
    } else if(std::holds_alternative<StartOfCodeBlock>(l)) {
        set_state(ParsingState::codeblock);
    } else if(std::holds_alternative<EndOfCodeBlock>(l)) {
        set_state(ParsingState::unset);
    } else if(std::holds_alternative<NewBlock>(l)) {
        set_state(ParsingState::unset);
    } else if(std::holds_alternative<SceneDecl>(l)) {
        set_state(ParsingState::unset);
        doc.elements.push_back(SceneChange{});
    } else {
        std::abort();
    }
}

Document StructureParser::get_document() {
    if(has_finished) {
        std::abort();
    }
    // Get data from pending declarations (i.e. the last paragraph)
    has_finished = true;
    return std::move(doc);
}
