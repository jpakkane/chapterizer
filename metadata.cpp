#include <metadata.hpp>
#include <fstream>
#include <nlohmann/json.hpp>

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

FontStyles_temp parse_fontstyle(const json &data) {
    FontStyles_temp style;
    style.name = get_string(data, "font");
    style.style = get_string(data, "style");
    style.size = Point::from_value(get_double(data, "pointsize"));
    style.line_height = Point::from_value(get_double(data, "line_height"));
    return style;
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
    m.language = get_string(data, "language");

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

    auto pdf = data["pdf"];
    m.pdf.ofname = get_string(pdf, "filename");
    auto page = pdf["page"];
    auto margins = pdf["margins"];
    m.pdf.page.w = Millimeter::from_value(get_int(page, "width"));
    m.pdf.page.h = Millimeter::from_value(get_int(page, "height"));
    m.pdf.margins.inner = Millimeter::from_value(get_int(margins, "inner"));
    m.pdf.margins.outer = Millimeter::from_value(get_int(margins, "outer"));
    m.pdf.margins.upper = Millimeter::from_value(get_int(margins, "upper"));
    m.pdf.margins.lower = Millimeter::from_value(get_int(margins, "lower"));
    auto styles = pdf["text_styles"];
    m.pdf.normal_style = parse_fontstyle(styles["plain"]);
    m.pdf.section_style = parse_fontstyle(styles["section"]);
    m.pdf.code_style = parse_fontstyle(styles["code"]);
    m.pdf.footnote_style = parse_fontstyle(styles["footnote"]);

    auto epub = data["epub"];
    m.epub.ofname = get_string(epub, "filename");
    m.epub.cover = get_string(epub, "cover");
    m.epub.ISBN = get_string(epub, "ISBN");
    m.epub.file_as = get_string(epub, "file_as");
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
