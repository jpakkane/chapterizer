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

#include <units.hpp>
#include <chaptercommon.hpp>

#include <string>
#include <vector>
#include <variant>
#include <filesystem>

enum class Language : int {
    Unset,
    English,
    Finnish,
};

struct Margins {
    Length inner = Length::from_mm(20);
    Length outer = Length::from_mm(15);
    Length upper = Length::from_mm(15);
    Length lower = Length::from_mm(15);
};

struct PageSize {
    Length w;
    Length h;
};

struct Credits {
    std::string key;
    std::string value;
};

struct ChapterStyles {
    ChapterParameters normal;
    ChapterParameters normal_noindent;
    ChapterParameters code;
    ChapterParameters section;
    ChapterParameters letter;
    ChapterParameters footnote;
    ChapterParameters lists;
    ChapterParameters title;
    ChapterParameters author;
    ChapterParameters colophon;
    ChapterParameters dedication;
};

struct Spaces {
    Length above_section;
    Length below_section;
    Length different_paragraphs;
    Length letter_indent;
    Length codeblock_indent;
    Length footnote_separation;
};

struct PdfMetadata {
    std::string ofname;
    std::vector<std::string> colophon;
    PageSize page;
    Length bleed;
    Margins margins;
    ChapterStyles styles;
    Spaces spaces;
};

struct EpubMetadata {
    std::string ofname;
    std::string ISBN;
    std::string cover;
    std::string stylesheet;
    std::string file_as;
};

struct DraftData {
    std::string surname;
    std::string email;
    std::string phone;
    std::string page_number_template;
};

struct Colophon {
    std::vector<std::string> lines;
};

struct Dedication {
    std::vector<std::string> lines;
};

struct Empty {

};

struct FirstPage {

};

struct Signing {
    std::vector<std::string> lines;
};

typedef std::variant<Colophon, Dedication, Empty, FirstPage, Signing> FrontMatterPage;

struct Metadata {
    // All paths in metadata are relative to this (i.e. where the JSON file was)
    std::filesystem::path top_dir;
    std::string title;
    std::string author;
    bool is_draft = false;
    DraftData draftdata;
    Language language;
    std::vector<FrontMatterPage> frontmatter;
    std::vector<std::string> sources;
    std::vector<std::string> backmatter;
    bool generate_pdf;
    bool generate_epub;
    PdfMetadata pdf;
    EpubMetadata epub;
    // FIXME, remove these three.
    std::vector<std::string> dedication;
    std::vector<Credits> credits;
    std::vector<std::string> postcredits;
    bool debug_draw = false;
};

struct Paragraph {
    std::string text;
};

struct Section {
    int level;
    int number;
    std::string text;
};

struct Letter {
    std::vector<std::string> paragraphs;
};

struct CodeBlock {
    std::vector<std::string> raw_lines;
};

struct SignBlock {
    std::vector<std::string> raw_lines;
};

struct Footnote {
    int number;
    std::string text;
};

struct NumberList {
    std::vector<std::string> items;
};

struct Figure {
    std::string file;
};

struct SceneChange {};

// Also needs images, footnotes, unformatted text etc.
typedef std::variant<Paragraph,
                     Section,
                     SceneChange,
                     CodeBlock,
                     Footnote,
                     NumberList,
                     Figure,
                     Letter,
                     SignBlock>
    DocElement;

struct Document {
    Metadata data;
    std::vector<DocElement> elements;

    int num_chapters() const;
    int num_footnotes() const;
};

Metadata load_book_json(const char *path);
