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

#include "utils.hpp"
#include <sstream>

#include <cassert>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>

#include <filesystem>

double mm2pt(const double x) { return x * 2.8346456693; }
double pt2mm(const double x) { return x / 2.8346456693; }

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
