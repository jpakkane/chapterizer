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
#include <algorithm>
#include <cassert>

ChapterFormatter::ChapterFormatter(const TextElementIterator &start_,
                                   const TextElementIterator &end_,
                                   const std::vector<TextElement> &elms,
                                   size_t target_height_)
    : start{start_}, end{end_}, elements{elms}, target_height{target_height_} {}

PageLayoutResult ChapterFormatter::optimize_pages() {
    PageLayoutResult r;

    auto run_start = start;
    try {
        optimize_recursive(run_start, r);
    } catch(const OptimalResultFound &) {
    }
    return std::move(best_layout);
}

bool ChapterFormatter::stop_recursing(TextElementIterator loc, const PageLayoutResult &r) {
    size_t max_reaches = 5;
    auto current_penalty = compute_penalties(r.pages).total_penalty;
    auto &reaches = best_reaches[loc];
    if(reaches.size() >= max_reaches) {
        if(current_penalty >= reaches.back()) {
            return true;
        }
        reaches.pop_back();
    }
    auto insertion_point = std::lower_bound(reaches.begin(), reaches.end(), current_penalty);
    reaches.insert(insertion_point, current_penalty);

    return false;
}

void ChapterFormatter::optimize_recursive(
    TextElementIterator run_start,
    PageLayoutResult &r,
    const std::optional<ImageElement> incoming_pending_image) {
    size_t lines_on_page = 0;
    std::optional<size_t> page_section_number;
    std::optional<ImageElement> current_page_image;
    std::optional<ImageElement> outgoing_pending_image;
    if(compute_penalties(r.pages).total_penalty > best_penalty) {
        return;
    }

    if(incoming_pending_image) {
        // FIXME, check for full page images.
        current_page_image = incoming_pending_image;
        lines_on_page = current_page_image->height_in_lines;
    }
    for(TextElementIterator current = run_start; current != end; ++current) {
        auto push_and_resume = [&](const TextElementIterator &startpoint,
                                   const TextElementIterator &endpoint) {
            TextLimits limits{startpoint, endpoint};
            if(page_section_number) {
                assert(!current_page_image);
                r.pages.emplace_back(SectionPage{*page_section_number, limits});
            } else {
                r.pages.emplace_back(RegularPage{limits, {}, current_page_image});
            }
            const size_t height_validation = r.pages.size();
            optimize_recursive(endpoint, r);
            assert(height_validation == r.pages.size());
            r.pages.pop_back();
        };
        // Have we filled the current page?
        if(lines_on_page >= target_height) {
            // Exact.
            auto endpoint = current;
            push_and_resume(run_start, endpoint);
            // One line short
            --endpoint;
            push_and_resume(run_start, endpoint);
            // One line long
            if(current != end) {
                endpoint = current;
                ++endpoint;
                push_and_resume(run_start, endpoint);
            }
            // It's a bit weird to return here, but that's recursion for you.
            return;
        }
        if(auto *sec = std::get_if<SectionElement>(&current.element())) {
            // There can be only one of these in a chapter and it must come first.
            assert(current == run_start);
            assert(lines_on_page == 0);
            const size_t chapter_heading_top_whitespace = 8;
            lines_on_page +=
                chapter_heading_top_whitespace; // Hack, replace with a proper whitespace element.
            page_section_number = sec->chapter_number;
            ++lines_on_page;
        } else if(auto *par = std::get_if<ParagraphElement>(&current.element())) {
            (void)par;
            ++lines_on_page;
        } else if(const auto *empty = std::get_if<EmptyLineElement>(&current.element())) {
            if(lines_on_page != 0) {
                // Ignore empty space at the beginning of the line.
                lines_on_page += empty->num_lines;
            }
        } else if(const auto *cb = std::get_if<SpecialTextElement>(&current.element())) {
            (void)cb;
            ++lines_on_page;
        } else if(const auto *imel = std::get_if<ImageElement>(&current.element())) {
            if(lines_on_page + imel->height_in_lines > target_height) {
                assert(!outgoing_pending_image);
                outgoing_pending_image = *imel;
            } else {
                assert(!current_page_image);
                lines_on_page += imel->height_in_lines;
                current_page_image = *imel;
            }
        } else {
            // FIXME, add images etc.
            std::abort();
        }
    }
    if(lines_on_page > 0) {
        TextLimits limits;
        limits.start = run_start;
        limits.end = end;
        if(page_section_number) {
            r.pages.emplace_back(SectionPage{page_section_number.value(), limits});
        } else {
            r.pages.emplace_back(RegularPage{limits, {}, {}});
        }
    }
    r.stats = compute_penalties(r.pages);
    if(r.stats.total_penalty < best_penalty) {
        best_layout = r;
        best_penalty = r.stats.total_penalty;
        if(best_penalty == 0) {
            throw OptimalResultFound{};
        }
    }
    if(lines_on_page > 0) {
        r.pages.pop_back();
    }
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
        const TextLimits *limits = nullptr;
        if(auto *rp = std::get_if<RegularPage>(&p)) {
            limits = &rp->main_text;
        } else if(const auto *sp = std::get_if<SectionPage>(&p)) {
            limits = &sp->main_text;
        } else {
            fprintf(stderr, "Unsupported.\n");
            std::abort();
        }
        const size_t first_element_id = limits->start.element_id;
        const size_t first_line_id = limits->start.line_id;
        const size_t last_element_id = limits->end.element_id;
        const size_t last_line_id = limits->end.line_id;

        const auto &start_lines = get_lines(elements[first_element_id]);
        if(last_element_id >= elements.size()) {
            if(first_element_id == elements.size() - 1 && first_line_id == start_lines.size() - 1) {
                stats.single_line_last_page = true;
                stats.total_penalty += SingleLinePage;
                continue;
            }
        }
        if(last_element_id < elements.size()) {
            const auto &end_lines = get_lines(elements[last_element_id]);

            // Orphan (single line at the end of a page)
            if(end_lines.size() > 1 && last_line_id == 1) {
                stats.orphans.push_back(page_number_offset + page_num);
                stats.total_penalty += OrphanPenalty;
            }
        }
        // These are only counted for "regular" pages.
        if(std::holds_alternative<RegularPage>(p)) {
            // Widow
            if(start_lines.size() > 1 && first_line_id == start_lines.size() - 1) {
                stats.widows.push_back(page_number_offset + page_num);
                stats.total_penalty += WidowPenalty;
            }
            // Mismatch
            if(!on_first_page && !on_last_page && (((page_num + 1) % 2) == 1)) {
                if(even_page_height != odd_page_height) {
                    const auto mismatch_amount =
                        (int64_t)even_page_height - (int64_t)odd_page_height;
                    stats.mismatches.emplace_back(
                        HeightMismatch{page_number_offset + page_num, mismatch_amount});
                    stats.total_penalty += abs(mismatch_amount) * MismatchPenalty;
                }
            }
        }
    }
    return stats;
}
