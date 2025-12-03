/*
 * Copyright 2025 Jussi Pakkanen
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

#include <printpaginator.hpp>

struct OptimalResultFound {};

class ChapterFormatter {
public:
    ChapterFormatter(const TextElementIterator &start,
                     const TextElementIterator &end,
                     const std::vector<TextElement> &elms,
                     size_t target_height);

    PageLayoutResult optimize_pages();

private:
    static constexpr size_t WidowPenalty = 10;
    static constexpr size_t OrphanPenalty = 10;
    static constexpr size_t MismatchPenalty = 7;
    static constexpr size_t SingleLinePage = 1000;

    PageStatistics compute_penalties(const std::vector<Page> &pages) const;

    void optimize_recursive(TextElementIterator run_start,
                            PageLayoutResult &r,
                            const std::optional<ImageElement> incoming_pending_image = {});

    bool stop_recursing(TextElementIterator loc, const PageLayoutResult &r);

    const TextElementIterator start;
    const TextElementIterator end;
    const std::vector<TextElement> elements;

    size_t best_penalty = size_t(-1);
    PageLayoutResult best_layout;
    const size_t target_height;

    std::unordered_map<TextElementIterator, std::vector<std::size_t>> best_reaches;
};
