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

const std::unordered_map<std::string, TextCategory> categorymap{{"serif", TextCategory::Serif},
                                                                {"sans", TextCategory::SansSerif},
                                                                {"mono", TextCategory::Monospace}};

const std::unordered_map<std::string, TextStyle> stylemap{{"regular", TextStyle::Regular},
                                                          {"italic", TextStyle::Italic},
                                                          {"bold", TextStyle::Bold},
                                                          {"bolditalic", TextStyle::BoldItalic}};

std::vector<std::string> extract_stringarray(const nlohmann::json &data, const char *entryname) {
    std::vector<std::string> result;
    auto arr = data[entryname];
    if(!arr.is_array()) {
        printf("%s must be an array of strings.\n", entryname);
        std::abort();
    }
    for(const auto &e : arr) {
        if(!e.is_string()) {
            printf("Source array %s entry is not a string.\n", entryname);
            std::abort();
        }
        result.push_back(e.get<std::string>());
    }
    return result;
}

} // namespace

using json = nlohmann::json;

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

HBChapterParameters parse_chapterstyle(const json &data) {
    HBChapterParameters chapter_style;
    chapter_style.line_height = Length::from_pt(get_double(data, "line_height"));
    chapter_style.indent = Length::from_mm(get_double(data, "indent"));
    auto font = data["font"];
    auto cat = get_string(font, "category");
    auto cit = categorymap.find(cat);
    if(cit == categorymap.end()) {
        fprintf(stderr, "Unknown category: %s\n", cat.c_str());
    }
    chapter_style.font.par.cat = cit->second;

    const auto stylestr = get_string(font, "type");
    auto it = stylemap.find(stylestr);
    if(it == stylemap.end()) {
        printf("Unknown type \"%s\".", stylestr.c_str());
        std::abort();
    }
    chapter_style.font.par.style = it->second;
    chapter_style.font.size = Length::from_pt(get_double(font, "pointsize"));
    auto fit = data.find("justify_last");
    if(fit != data.end()) {
        chapter_style.indent_last_line = fit->get<bool>();
    }
    return chapter_style;
}

void setup_draft_settings(Metadata &m) {
    // Fonts
    m.pdf.styles.normal.font.par.cat = TextCategory::Serif;
    m.pdf.styles.normal.font.size = Length::from_pt(12);
    m.pdf.styles.normal.font.par.style = TextStyle::Regular;
    m.pdf.styles.normal.indent = Length::from_mm(10);
    m.pdf.styles.normal.line_height = Length::from_pt(20);
    const auto &normal = m.pdf.styles.normal;
    m.pdf.styles.normal_noindent = normal;
    m.pdf.styles.normal_noindent.indent = Length::zero();

    m.pdf.styles.code = normal;
    m.pdf.styles.code.font.par.cat = TextCategory::Monospace;
    m.pdf.styles.code.font.size = Length::from_pt(10);
    m.pdf.styles.colophon = normal;
    m.pdf.styles.dedication = normal;
    m.pdf.styles.footnote = normal;
    m.pdf.styles.lists = normal;
    m.pdf.styles.letter = normal;
    m.pdf.styles.letter.font.par.style = TextStyle::Italic;

    m.pdf.styles.section.font.par.cat = TextCategory::SansSerif;
    m.pdf.styles.section.font.size = Length::from_pt(14);
    m.pdf.styles.section.font.par.style = TextStyle::Bold;
    m.pdf.styles.section.line_height = Length::from_pt(25);

    m.pdf.styles.title = m.pdf.styles.section;
    m.pdf.styles.author = m.pdf.styles.section;
    m.pdf.styles.author.font.par.style = TextStyle::Regular;

    // Page
    m.pdf.page.w = Length::from_mm(210);
    m.pdf.page.h = Length::from_mm(297);
    m.pdf.margins.inner = Length::from_mm(25.4);
    m.pdf.margins.outer = Length::from_mm(25.4);
    // Should be exactly an inch, but the font does not divide it
    // cleanly so there is more empty space at the bottom than
    // there should be.
    m.pdf.margins.upper = Length::from_mm(20.4) + m.pdf.styles.normal.line_height;
    m.pdf.margins.lower = Length::from_mm(20.4);

    // Spaces
    m.pdf.spaces.below_section = Length::from_mm(0);
    m.pdf.spaces.above_section = Length::from_mm(60);
    m.pdf.spaces.codeblock_indent = Length::from_mm(20);
    m.pdf.spaces.letter_indent = Length::from_mm(20);
    m.pdf.spaces.different_paragraphs = Length::from_mm(5);
    m.pdf.spaces.footnote_separation = Length::from_mm(5);
}

void parse_font_files(FontFiles &f, const json &fdict) {
    if(fdict.contains("regular")) {
        f.regular = fdict["regular"];
    }
    if(fdict.contains("italic")) {
        f.italic = fdict["italic"];
    }
    if(fdict.contains("bold")) {
        f.bold = fdict["bold"];
    }
    if(fdict.contains("bolditalic")) {
        f.bold = fdict["bolditalic"];
    }
}

void parse_font_paths(FontFilePaths &paths, const json &fonts) {
    parse_font_files(paths.serif, fonts["serif"]);
    parse_font_files(paths.sansserif, fonts["sans"]);
    parse_font_files(paths.mono, fonts["mono"]);
}

void load_pdf_element(Metadata &m, const json &pdf) {
    m.pdf.ofname = get_string(pdf, "filename");
    if(m.is_draft) {
        std::filesystem::path ofname = m.pdf.ofname;
        auto draftname = ofname.stem();
        draftname += "-draft";
        draftname.replace_extension(ofname.extension());
        ofname.replace_filename(draftname);
        m.pdf.ofname = ofname.c_str();
    }
    auto page = pdf["page"];
    auto margins = pdf["margins"];
    if(pdf.contains("bleed")) {
        m.pdf.bleed = Length::from_mm(get_double(pdf, "bleed"));
    } else {
        m.pdf.bleed = Length::zero();
    }

    if(pdf.contains("colophon")) {
        auto colophon_file = m.top_dir / get_string(pdf, "colophon");
        m.pdf.colophon = read_lines(colophon_file.c_str());
    }
    if(m.is_draft) {
        setup_draft_settings(m);
    } else {

        parse_font_paths(m.pdf.font_files, pdf["fontfiles"]);

        m.pdf.page.w = Length::from_mm(get_int(page, "width"));
        m.pdf.page.h = Length::from_mm(get_int(page, "height"));
        m.pdf.margins.inner = Length::from_mm(get_int(margins, "inner"));
        m.pdf.margins.outer = Length::from_mm(get_int(margins, "outer"));
        m.pdf.margins.upper = Length::from_mm(get_int(margins, "upper"));
        m.pdf.margins.lower = Length::from_mm(get_int(margins, "lower"));

        auto styles = pdf["styles"];
        m.pdf.styles.normal = parse_chapterstyle(styles["normal"]);
        m.pdf.styles.normal_noindent = m.pdf.styles.normal;
        m.pdf.styles.normal_noindent.indent = Length::zero();
        m.pdf.styles.section = parse_chapterstyle(styles["section"]);
        m.pdf.styles.code = parse_chapterstyle(styles["code"]);
        m.pdf.styles.letter = parse_chapterstyle(styles["letter"]);
        m.pdf.styles.footnote = parse_chapterstyle(styles["footnote"]);
        m.pdf.styles.lists = parse_chapterstyle(styles["lists"]);
        m.pdf.styles.title = parse_chapterstyle(styles["title"]);
        m.pdf.styles.author = parse_chapterstyle(styles["author"]);
        m.pdf.styles.colophon = parse_chapterstyle(styles["colophon"]);
        m.pdf.styles.dedication = parse_chapterstyle(styles["dedication"]);

        auto spaces = pdf["spaces"];
        m.pdf.spaces.above_section = Length::from_mm(get_double(spaces, "above_section"));
        m.pdf.spaces.below_section = Length::from_mm(get_double(spaces, "below_section"));
        m.pdf.spaces.different_paragraphs =
            Length::from_mm(get_double(spaces, "different_paragraphs"));
        m.pdf.spaces.codeblock_indent = Length::from_mm(get_double(spaces, "codeblock_indent"));
        m.pdf.spaces.letter_indent = Length::from_mm(get_double(spaces, "letter_indent"));
        m.pdf.spaces.footnote_separation =
            Length::from_mm(get_double(spaces, "footnote_separation"));
    }
}

void load_epub_element(Metadata &m, const json &epub) {
    m.epub.ofname = get_string(epub, "filename");
    m.epub.cover = get_string(epub, "cover");
    m.epub.ISBN = get_string(epub, "ISBN");
    m.epub.stylesheet = get_string(epub, "stylesheet");
    m.epub.file_as = get_string(epub, "file_as");
}

void strip(std::string &s) {
    while(!s.empty() && s.back() == ' ') {
        s.pop_back();
    }
    while(!s.empty() && s.front() == ' ') {
        s.erase(0, 1);
    }
}

std::vector<Credits> load_credits(const char *credits_path) {
    std::vector<Credits> c;
    std::string key, val;
    for(const auto &l : read_lines(credits_path)) {
        const auto p = l.find('+');
        if(p == std::string::npos) {
            val = l;
            strip(val);
            c.emplace_back(CreditsTitle{std::move(val)});
        } else {
            key = l.substr(0, p);
            val = l.substr(p + 1, std::string::npos);
            strip(key);
            strip(val);
            c.emplace_back(CreditsEntry{std::move(key), std::move(val)});
        }
    }
    return c;
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
    auto draft = data["draft"];
    if(!draft.is_null()) {
        m.is_draft = true;
        m.draftdata.email = get_string(draft, "email");
        m.draftdata.phone = get_string(draft, "phone");
        m.draftdata.surname = get_string(draft, "surname");
        m.draftdata.page_number_template = m.draftdata.surname + " / " + m.title + " / ";
    }
    if(data.contains("debug_draw")) {
        m.debug_draw = data["debug_draw"].get<bool>();
    }
    const auto langstr = get_string(data, "language");
    auto it = langmap.find(langstr);
    if(it == langmap.end()) {
        printf("Unsupported language %s\n", langstr.c_str());
        std::abort();
    }
    m.language = it->second;

    auto frontmatter = extract_stringarray(data, "frontmatter");
    for(const auto &text : frontmatter) {
        if(text == "empty") {
            m.frontmatter.emplace_back(Empty{});
        } else if(text == "colophon.txt") {
            auto cfile = m.top_dir / text;
            m.frontmatter.emplace_back(Colophon{read_lines(cfile.c_str())});
        } else if(text == "dedication.txt") {
            auto dfile = m.top_dir / text;
            m.frontmatter.emplace_back(Dedication{read_lines(dfile.c_str())});
        } else if(text == "firstpage") {
            m.frontmatter.emplace_back(FirstPage{});
        } else if(text == "signing.txt") {
            auto sfile = m.top_dir / text;
            m.frontmatter.emplace_back(Signing{read_lines(sfile.c_str())});
        } else {
            fprintf(stderr, "Frontmatter not supported yet.\n");
            std::abort();
        }
    }
    m.sources = extract_stringarray(data, "sources");
    auto backmatter = extract_stringarray(data, "backmatter");
    for(const auto &text : backmatter) {
        if(text == "credits.txt") {
            auto credits_path = m.top_dir / text;
            m.credits = load_credits(credits_path.c_str());
        } else {
            fprintf(stderr, "Backmatter not yet supported.\n");
            std::abort();
        }
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
