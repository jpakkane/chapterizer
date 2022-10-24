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

#include <utils.hpp>
#include <glib.h>
#include <sstream>
#include <fstream>

#include <cassert>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

#include <filesystem>
#include <unordered_map>
namespace {

// Store formatting characters in unused ASCII codepoints during processing.
//
// All characters under 33 are taken except 0, 10 (linefeed).

std::unordered_map<char, char> to_internal{
    {'/', 1}, {'*', 2}, {'|', 3}, {'`', 4}, {'#', 5}, {'\\', 6}, {'^', 7}};
std::unordered_map<char, char> from_internal{
    {1, '/'}, {2, '*'}, {3, '|'}, {4, '`'}, {5, '#'}, {6, '\\'}, {7, '^'}};

} // namespace

std::vector<std::string> split_to_lines(const std::string &in_text) {
    std::vector<std::string> lines;
    std::string val;
    const char separator = '\n';
    std::vector<std::string> words;
    std::stringstream sstream(in_text);
    while(std::getline(sstream, val, separator)) {
        while(!val.empty() && val.back() == ' ') {
            val.pop_back();
        }
        while(!val.empty() && val.front() == ' ') {
            val.erase(val.begin());
        }
        if(!val.empty()) {
            words.push_back(val);
        }
    }
    return words;
}

std::vector<std::string> split_to_words(std::string_view in_text) {
    std::string text;
    text.reserve(in_text.size());
    for(size_t i = 0; i < in_text.size(); ++i) {
        if(in_text[i] == '\n') {
            text.push_back(' ');
        } else {
            text.push_back(in_text[i]);
        }
    }
    while(!text.empty() && text.back() == ' ') {
        text.pop_back();
    }
    while(!text.empty() && text.front() == ' ') {
        text.erase(text.begin());
    }
    std::string val;
    const char separator = ' ';
    std::vector<std::string> words;
    std::stringstream sstream(text);
    while(std::getline(sstream, val, separator)) {
        if(!val.empty()) {
            words.push_back(val);
        }
    }
    return words;
}

MMapper::MMapper(const char *path) {
    bufsize = std::filesystem::file_size(path);
    int fd = open(path, O_RDONLY);
    assert(fd > 0);
    buf = static_cast<const char *>(mmap(nullptr, bufsize, PROT_READ, MAP_PRIVATE, fd, 0));
    assert(buf);
    close(fd);
}

MMapper::~MMapper() { munmap((void *)(buf), bufsize); }

std::vector<std::string> read_lines(const char *p) {
    std::vector<std::string> lines;
    std::ifstream input(p);
    assert(!input.fail());
    for(std::string line; std::getline(input, line);) {
        if(!g_utf8_validate(line.c_str(), line.length(), nullptr)) {
            printf("Invalid UTF-8 in %s.\n", p);
            std::abort();
        }
        lines.push_back(line);
    }
    return lines;
}

std::vector<std::string> read_paragraphs(const char *p) {
    std::vector<std::string> paragraphs;
    std::string buf;
    for(const auto &line : read_lines(p)) {
        if(line.empty()) {
            if(!buf.empty()) {
                paragraphs.emplace_back(std::move(buf));
                buf.clear();
            }
        } else {
            if(buf.empty()) {
                buf = line;
            } else {
                buf += ' ';
                buf += line;
            }
        }
    }
    if(!buf.empty()) {
        paragraphs.emplace_back(std::move(buf));
    }
    return paragraphs;
}

std::string current_date() {
    char buf[200];
    time_t t;
    struct tm *tmp;
    t = time(NULL);
    tmp = localtime(&t);
    if(tmp == NULL) {
        std::abort();
    }

    if(strftime(buf, 200, "%Y-%m-%d", tmp) == 0) {
        std::abort();
    }
    return std::string{buf};
}

char special2internal(char c) {
    auto it = to_internal.find(c);
    if(it == to_internal.end()) {
        return c;
    }
    return it->second;
}

char internal2special(char c) {
    auto it = from_internal.find(c);
    if(it == from_internal.end()) {
        return c;
    }
    return it->second;
}

void restore_special_chars(std::string &s) {
    for(size_t i = 0; i < s.size(); ++i) {
        s[i] = internal2special(s[i]);
    }
}

int words_in_file(const char *fname) {
    int num_words = 0;
    std::ifstream ifile(fname);
    assert(!ifile.fail());
    std::string line;
    std::string word;
    while(std::getline(ifile, line)) {
        std::stringstream wordstream(line);
        while(std::getline(wordstream, word, ' ')) {
            ++num_words;
        }
    }
    return num_words;
}
