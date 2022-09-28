#pragma once

#include <utils.hpp>

#include <string>
#include <vector>
#include <variant>
#include <filesystem>

struct Millimeter;

struct Point {
private:
    explicit Point(double d) : v(d){};

public:
    double v = 0.0;

    Point() : v(0.0){};
    Point(const Point &d) : v(d.v){};

    static Point from_value(double d) { return Point{d}; }
    static Point zero() { return Point{0}; }

    Point &operator=(const Point &p) {
        v = p.v;
        return *this;
    }

    Point &operator+=(const Point &p) {
        v += p.v;
        return *this;
    }

    Point &operator-=(const Point &p) {
        v -= p.v;
        return *this;
    }

    Point operator-(const Point &p) const { return Point{v - p.v}; }

    Point operator+(const Point &p) const { return Point{v + p.v}; }

    Point operator/(const double div) const { return Point{v / div}; }

    Point operator*(const double &mul) const { return Point{v * mul}; }

    Millimeter tomm() const;
};

inline Point operator*(const double mul, const Point &p) { return p * mul; }

struct Millimeter {
private:
    explicit Millimeter(double d) : v(d){};

public:
    double v = 0.0;

    Millimeter() : v(0.0) {}
    Millimeter(const Millimeter &d) : v(d.v){};

    static Millimeter from_value(double mm) { return Millimeter{mm}; }
    static Millimeter zero() { return Millimeter{0.0}; }

    Millimeter operator-() const { return Millimeter{-v}; }

    Millimeter &operator=(const Millimeter &p) {
        v = p.v;
        return *this;
    }

    Millimeter &operator+=(const Millimeter &p) {
        v += p.v;
        return *this;
    }

    Millimeter operator+(const Millimeter &o) const { return Millimeter{v + o.v}; }

    Millimeter operator-(const Millimeter &o) const { return Millimeter{v - o.v}; }

    Millimeter operator*(const double o) const { return Millimeter{v * o}; }

    Millimeter operator/(const double o) const { return Millimeter{v / o}; }

    bool operator<(const Millimeter &o) const { return v < o.v; }

    bool operator<=(const Millimeter &o) const { return v <= o.v; }

    bool operator>=(const Millimeter &o) const { return v >= o.v; }

    bool operator>(const Millimeter &o) const { return v > o.v; }

    Point topt() const { return Point::from_value(mm2pt(v)); }
};

inline Millimeter operator*(const double mul, const Millimeter &p) { return p * mul; }

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
    // All paths in metadata are relative to this (i.e. where the JSON file was)
    std::filesystem::path top_dir;
    std::string title;
    std::string author;
    std::string language;
    std::vector<std::string> sources;
    PdfMetadata pdf;
    EpubMetadata epub;
};

struct Paragraph {
    std::string text;
};

struct Section {
    int level;
    int number;
    std::string text;
};

struct CodeBlock {
    std::vector<std::string> raw_lines;
};

struct Footnote {
    int number;
    std::string text;
};

struct Figure {
    std::string file;
};

struct SceneChange {};

// Also needs images, footnotes, unformatted text etc.
typedef std::variant<Paragraph, Section, SceneChange, CodeBlock, Footnote, Figure> DocElement;

struct Document {
    Metadata data;
    std::vector<DocElement> elements;

    int num_chapters() const;
    int num_footnotes() const;
};

Metadata load_book_json(const char *path);
