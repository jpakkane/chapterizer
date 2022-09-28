#pragma once

#include <utils.hpp>

#include <string>
#include <vector>

struct Margins {
    Millimeter inner = Millimeter::from_value(20);
    Millimeter outer = Millimeter::from_value(15);
    Millimeter upper = Millimeter::from_value(15);
    Millimeter lower = Millimeter::from_value(15);
};

struct PageSize {
    Millimeter w;
    Millimeter h;
};

struct FontStyles_temp {
    std::string name;
    std::string style; // FIXME to use enum.
    Point size;
    Point line_height;
};

struct PdfMetadata {
    std::string ofname;
    PageSize page;
    Margins margins;
    FontStyles_temp normal_style;
    FontStyles_temp code_style;
    FontStyles_temp section_style;
    FontStyles_temp footnote_style;
};

struct EpubMetadata {
    std::string ofname;
    std::string ISBN;
    std::string cover;
};

struct Metadata {
    std::string title;
    std::string author;
    std::string language;
    std::vector<std::string> sources;
    PdfMetadata pdf;
    EpubMetadata epub;
};

Metadata load_book(const char *path);
