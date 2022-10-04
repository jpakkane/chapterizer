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

#include "bookparser.hpp"

#include <cassert>

#include <algorithm>

namespace {

const std::unordered_map<std::string, SpecialBlockType> specialmap{
    {"code", SpecialBlockType::Code},
    {"footnote", SpecialBlockType::Footnote},
    {"numberlist", SpecialBlockType::NumberList}};

}

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

    if(parsing_specialblock) {
        auto block_end = try_match(specialblock_end, GRegexMatchFlags(0));
        if(block_end) {
            parsing_specialblock = false;
            return EndOfSpecialBlock{};
        } else {
            auto full_line = try_match(line, GRegexMatchFlags(0));
            auto nl = try_match(single_newline, GRegexMatchFlags(0));
            if(!nl) {
                std::abort();
            }
            if(full_line) {
                return PlainLine{full_line->whole_match};
            }
            return PlainLine{std::string_view{}}; // Empty line.
        }
    }
    auto match_result = try_match(multi_newline, GRegexMatchFlags(0));
    if(match_result) {
        if(match_result->whole_match.length() > 1) {
            return NewBlock{};
        }
        return NewLine{};
    }
    match_result = try_match(specialblock_start, GRegexMatchFlags(0));
    if(match_result) {
        if(parsing_specialblock) {
            printf("Nested codeblocks not supported.\n");
            std::abort();
        }
        parsing_specialblock = true;
        if(!try_match(multi_newline, GRegexMatchFlags(0))) {
            std::abort();
        }
        const auto block_name = match_result->view_for(1, data);
        auto it = specialmap.find(std::string{block_name});
        if(it != specialmap.end()) {
            return StartOfSpecialBlock{it->second};
        }
        std::string tmp{block_name};
        printf("Unknown special block type: %s\n", tmp.c_str());
        std::abort();
    }
    match_result = try_match(specialblock_end, GRegexMatchFlags(0));
    if(match_result) {
        printf("End of codeblock without start of same.\n");
        std::abort();
    }
    match_result = try_match(directive, GRegexMatchFlags(0));
    if(match_result) {
        auto dir_name = match_result->view_for(1, data);
        if(dir_name == "s") {
            return SceneDecl{};
        } else if(dir_name == "figure") {
            auto fname = match_result->view_for(2, data);
            while(!fname.empty() && fname.front() == ' ') {
                fname.remove_prefix(1);
            }
            return FigureDecl{std::string{fname}};
        } else {
            std::string tmp{dir_name};
            printf("Unknown directive '%s'.\n", tmp.c_str());
            std::abort();
        }
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

StructureParser::~StructureParser() {
    if(!stored_lines.empty()) {
        printf("Stored lines not fully drained.\n");
        std::abort();
    }
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

std::vector<std::string> StructureParser::pop_lines_to_paragraphs() {
    std::vector<std::string> paras;
    std::string buf;
    for(auto &l : stored_lines) {
        if(l.empty()) {
            if(!buf.empty()) {
                paras.emplace_back(std::move(buf));
                buf.clear();
            }
        } else {
            if(buf.empty()) {
                buf = std::move(l);
            } else {
                buf += ' ';
                buf += l;
            }
        }
    }
    if(!buf.empty()) {
        paras.emplace_back(std::move(buf));
    }
    stored_lines.clear();
    return paras;
}

void StructureParser::build_element() {
    switch(current_state) {
    case ParsingState::unset:
        std::abort();
    case ParsingState::specialblock:
        if(current_special == SpecialBlockType::Code) {
            doc.elements.emplace_back(CodeBlock{std::move(stored_lines)});
        } else if(current_special == SpecialBlockType::Footnote) {
            doc.elements.emplace_back(Footnote{footnote_number, pop_lines_to_string()});
        } else if(current_special == SpecialBlockType::NumberList) {
            doc.elements.emplace_back(NumberList{pop_lines_to_paragraphs()});
        } else {
            std::abort();
        }
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
    if(current_state == ParsingState::specialblock) {
        current_special = SpecialBlockType::Unset;
    }
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
    } else if(std::holds_alternative<StartOfSpecialBlock>(l)) {
        set_state(ParsingState::specialblock);
        current_special = std::get<StartOfSpecialBlock>(l).type;
        if(current_special == SpecialBlockType::Footnote) {
            ++footnote_number;
        }
    } else if(std::holds_alternative<EndOfSpecialBlock>(l)) {
        set_state(ParsingState::unset);
    } else if(std::holds_alternative<NewBlock>(l)) {
        set_state(ParsingState::unset);
    } else if(std::holds_alternative<SceneDecl>(l)) {
        set_state(ParsingState::unset);
        doc.elements.push_back(SceneChange{});
    } else if(std::holds_alternative<FigureDecl>(l)) {
        set_state(ParsingState::unset);
        doc.elements.push_back(Figure{std::get<FigureDecl>(l).fname});
    } else if(std::holds_alternative<EndOfFile>(l)) {
        set_state(ParsingState::unset);
    } else {
        std::abort();
    }
}

void StructureParser::finish() {
    if(has_finished) {
        std::abort();
    }
    // Get data from pending declarations (i.e. the last paragraph)
    has_finished = true;
}
