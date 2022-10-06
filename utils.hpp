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

#pragma once

#include <string>
#include <vector>
#include <cstdint>

double mm2pt(const double x);
double pt2mm(const double x);

std::vector<std::string> split_to_words(std::string_view in_text);
std::vector<std::string> split_to_lines(const std::string &in_text);

class MMapper {
public:
    explicit MMapper(const char *path);
    ~MMapper();

    const char *data() const { return buf; }
    int64_t size() const { return bufsize; }

    std::string_view view() const { return std::string_view(buf, bufsize); }

private:
    const char *buf;
    int64_t bufsize;
};

std::vector<std::string> read_lines(const char *p);

std::vector<std::string> read_paragraphs(const char *p);

std::string current_date();
