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

#include <hyphen.h>

#include <string>
#include <vector>

enum class SplitType : int {
    Regular,
    NoHyphen,
};

struct HyphenPoint {
    size_t loc;
    SplitType type;

    bool operator==(const HyphenPoint &o) const { return loc == o.loc && type == o.type; }
};

struct HyphenatedWord {
    std::vector<HyphenPoint> hyphen_points;
    std::string word;

    std::string get_visual_string() const;

    void sanity_check() const;
};

class WordHyphenator {
public:
    WordHyphenator();
    ~WordHyphenator();

    HyphenatedWord hyphenate(const std::string &word) const;
    std::vector<HyphenatedWord> hyphenate(const std::vector<std::string> &words) const;

private:
    HyphenDict *dict;
};
