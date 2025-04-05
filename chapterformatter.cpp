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

#include <chapterformatter.hpp>

ChapterFormatter::ChapterFormatter(const TextElementIterator &start_,
                                   const TextElementIterator &end_,
                                   const std::vector<TextElement> &elms)
    : start{start_}, end{end_}, elements{elms} {}

PageLayoutResult ChapterFormatter::optimize_pages() {
    PageLayoutResult r;
    size_t lines_on_page = 0;
    const size_t max_lines = 25;

    auto run_start = start;

    for(TextElementIterator current = run_start; current != end; ++current) {
        if(lines_on_page >= max_lines) {
            TextLimits limits;
            limits.start = run_start;
            limits.end = current;
            r.pages.emplace_back(RegularPage{limits, {}, {}});
            run_start = current;
            lines_on_page = 1;
        } else {
            ++lines_on_page;
        }
    }
    if(lines_on_page > 0) {
        TextLimits limits;
        limits.start = run_start;
        limits.end = end;
        r.pages.emplace_back(RegularPage{limits, {}, {}});
    }
    r.stats = compute_penalties(r.pages);
    best_layout = std::move(r);
    return std::move(best_layout);
}

PageStatistics ChapterFormatter::compute_penalties(const std::vector<Page> &pages) const {
    PageStatistics stats;
    const size_t page_number_offset = 1;
    size_t even_page_height = 0;
    size_t odd_page_height = 0;
    for(size_t page_num = 0; page_num < pages.size(); ++page_num) {
        const auto &p = pages[page_num];
        const size_t num_lines_on_page = lines_on_page(p);
        if((page_num + 1) % 2) {
            odd_page_height = num_lines_on_page;
        } else {
            even_page_height = num_lines_on_page;
        }
        const bool on_last_page = page_num == pages.size() - 1;
        const bool on_first_page = page_num == 0;
        const auto &page_info = std::get<RegularPage>(p);
        size_t first_element_id = page_info.main_text.start.element_id;
        size_t first_line_id = page_info.main_text.start.line_id;
        size_t last_element_id = page_info.main_text.end.element_id;
        size_t last_line_id = page_info.main_text.end.line_id;

        const auto &start_lines = get_lines(elements[first_element_id]);
        if(last_element_id >= elements.size()) {
            continue;
            // FIXME: add end-of-chapter widow super penalty here.
        }
        const auto &end_lines = get_lines(elements[last_element_id]);

        // Orphan
        if(end_lines.size() > 1 && last_line_id == 1) {
            stats.orphans.push_back(page_number_offset + page_num);
        }
        // Widow
        if(start_lines.size() > 1 && first_line_id == start_lines.size() - 1) {
            stats.widows.push_back(page_number_offset + page_num);
        }

        // Mismatch
        if(!on_first_page && !on_last_page && (((page_num + 1) % 2) == 1)) {
            if(even_page_height != odd_page_height) {
                stats.mismatches.emplace_back(
                    HeightMismatch{page_number_offset + page_num,
                                   (int64_t)even_page_height - (int64_t)odd_page_height});
            }
        }
    }
    return stats;
}
