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

#include <metadata.hpp>
#include <utils.hpp>
#include <fstream>
#include <nlohmann/json.hpp>
#include <unordered_map>

namespace {

const std::unordered_map<std::string, Language> langmap{
    {"unk", Language::Unset}, {"en", Language::English}, {"fi", Language::Finnish}};

const std::unordered_map<std::string, FontStyle> stylemap{{"regular", FontStyle::Regular},
                                                          {"italic", FontStyle::Italic},
                                                          {"bold", FontStyle::Bold},
                                                          {"bolditalic", FontStyle::BoldItalic}};

} // namespace

using json = nlohmann::json;

Millimeter Point::tomm() const { return Millimeter::from_value(pt2mm(v)); }

std::string get_string(const json &data, const char *key) {
    if(!data.contains(key)) {
        printf("Missing required key %s.\n", key);
        std::abort();
    }
    auto value = data[key];
    if(!value.is_string()) {
        printf("Element %s is not a string.\n", key);
        std::abort();
    }
    return value.get<std::string>();
}

double get_double(const json &data, const char *key) {
    if(!data.contains(key)) {
        printf("Missing required key %s.\n", key);
        std::abort();
    }
    auto value = data[key];
    if(!value.is_number()) {
        printf("Element %s is not a string.\n", key);
        std::abort();
    }
    return value.get<double>();
}

int get_int(const json &data, const char *key) {
    if(!data.contains(key)) {
        printf("Missing required key %s.\n", key);
        std::abort();
    }
    auto value = data[key];
    if(!value.is_number_integer()) {
        printf("Element %s is not a string.\n", key);
        std::abort();
    }
    return value.get<int>();
}

ChapterParameters parse_chapterstyle(const json &data) {
    ChapterParameters chapter_style;
    chapter_style.line_height = Point::from_value(get_double(data, "line_height"));
    chapter_style.indent = Millimeter::from_value(get_double(data, "indent"));
    auto font = data["font"];
    chapter_style.font.name = get_string(font, "name");
    const auto stylestr = get_string(font, "type");
    auto it = stylemap.find(stylestr);
    if(it == stylemap.end()) {
        printf("Unknown type \"%s\".", stylestr.c_str());
        std::abort();
    }
    chapter_style.font.type = it->second;
    chapter_style.font.size = Point::from_value(get_double(font, "pointsize"));
    return chapter_style;
}

void load_pdf_element(Metadata &m, const json &pdf) {
    m.pdf.ofname = get_string(pdf, "filename");
    auto page = pdf["page"];
    auto margins = pdf["margins"];
    m.pdf.page.w = Millimeter::from_value(get_int(page, "width"));
    m.pdf.page.h = Millimeter::from_value(get_int(page, "height"));
    m.pdf.margins.inner = Millimeter::from_value(get_int(margins, "inner"));
    m.pdf.margins.outer = Millimeter::from_value(get_int(margins, "outer"));
    m.pdf.margins.upper = Millimeter::from_value(get_int(margins, "upper"));
    m.pdf.margins.lower = Millimeter::from_value(get_int(margins, "lower"));
    auto styles = pdf["styles"];
    m.pdf.normal_style = parse_chapterstyle(styles["plain"]);
    m.pdf.section_style = parse_chapterstyle(styles["section"]);
    m.pdf.code_style = parse_chapterstyle(styles["code"]);
    m.pdf.footnote_style = parse_chapterstyle(styles["footnote"]);
}

void load_epub_element(Metadata &m, const json &epub) {
    m.epub.ofname = get_string(epub, "filename");
    m.epub.cover = get_string(epub, "cover");
    m.epub.ISBN = get_string(epub, "ISBN");
    m.epub.file_as = get_string(epub, "file_as");
}

Metadata load_book_json(const char *path) {
    Metadata m;
    std::filesystem::path json_file(path);
    m.top_dir = json_file.parent_path();
    std::ifstream ifile(path);
    if(ifile.fail()) {
        printf("Could not open file %s.\n", path);
        std::abort();
    }

    json data = json::parse(ifile);
    assert(data.is_object());
    m.author = get_string(data, "author");
    m.title = get_string(data, "title");
    const auto langstr = get_string(data, "language");
    auto it = langmap.find(langstr);
    if(it == langmap.end()) {
        printf("Unsupported language %s\n", langstr.c_str());
        std::abort();
    }
    m.language = it->second;

    auto sources = data["sources"];
    if(!sources.is_array()) {
        printf("Sources must be an array of strings.\n");
        std::abort();
    }
    for(const auto &e : sources) {
        if(!e.is_string()) {
            printf("Source array entry is not a string.\n");
            std::abort();
        }
        m.sources.push_back(e.get<std::string>());
    }

    if(data.contains("pdf")) {
        m.generate_pdf = true;
        load_pdf_element(m, data["pdf"]);
    } else {
        m.generate_pdf = false;
    }

    if(data.contains("epub")) {
        m.generate_epub = true;
        load_epub_element(m, data["epub"]);
    } else {
        m.generate_epub = false;
    }
    return m;
}

int Document::num_chapters() const {
    return std::count_if(elements.begin(), elements.end(), [](const DocElement &e) {
        return std::holds_alternative<Section>(e);
    });
}

int Document::num_footnotes() const {
    return std::count_if(elements.begin(), elements.end(), [](const DocElement &e) {
        return std::holds_alternative<Footnote>(e);
    });
}

Point Millimeter::topt() const { return Point::from_value(mm2pt(v)); }
