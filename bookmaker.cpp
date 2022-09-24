#include <pdfrenderer.hpp>
#include <utils.hpp>
#include <chaptercommon.hpp>
#include <paragraphformatter.hpp>
#include <wordhyphenator.hpp>
#include <formatting.hpp>
#include <bookparser.hpp>
#include <tinyxml2.h>
#include <filesystem>
#include <stack>

#include <glib.h>
#include <cassert>
#include <fstream>
#include <cstring>

struct Chapter {
    std::string title;
    std::vector<std::string> paragraphs;
};

namespace fs = std::filesystem;

namespace {

// The contents of these files is always the same.

const char mimetext[] = "application/epub+zip";

const char containertext[] = R"(<?xml version='1.0' encoding='utf-8'?>
<container xmlns="urn:oasis:names:tc:opendocument:xmlns:container" version="1.0">
  <rootfiles>
    <rootfile full-path="OEBPS/content.opf" media-type="application/oebps-package+xml"/>
  </rootfiles>
</container>
)";

const char csstext[] = R"(h1, h2, h3, h4, h5 {
  font-family: Sans-Serif;
}

p {
  font-family: Serif;
  text-align: justify;
}

p#preformatted {
  font-family: Mono;
}

)";

} // namespace

struct margins {
    double inner = 15;
    double outer = 10;
    double upper = 10;
    double lower = 20;
};

struct PageSize {
    int w_mm;
    int h_mm;
};

void render_page_num(PdfRenderer &book,
                     const FontParameters &par,
                     int page_num,
                     const PageSize &p,
                     const margins &m) {
    char buf[128];
    snprintf(buf, 128, "%d", page_num);
    const double yloc = p.h_mm - 2 * m.lower / 3;
    const double leftmargin = page_num % 2 ? m.inner : m.outer;
    const double xloc = leftmargin + (p.w_mm - m.inner - m.outer) / 2;
    book.render_line_centered(buf, par, mm2pt(xloc), mm2pt(yloc));
}

template<typename T> void style_change(T &stack, typename T::value_type val) {
    if(stack.contains(val)) {
        stack.pop(val);
        // If the
    } else {
        stack.push(val);
    }
}

// NOTE: mutates the input words.
std::vector<FormattingChange> extract_styling(StyleStack &current_style, std::string &word) {
    std::vector<FormattingChange> changes;
    std::string buf;
    const char *word_start = word.c_str();
    const char *in = word_start;
    int num_changes = 0;

    while(*in) {
        auto c = g_utf8_get_char(in);

        switch(c) {
        case italic_codepoint:
            style_change(current_style, ITALIC_S);
            changes.push_back(FormattingChange{size_t(in - word_start - num_changes), ITALIC_S});
            ++num_changes;
            break;
        case bold_codepoint:
            style_change(current_style, BOLD_S);
            changes.push_back(FormattingChange{size_t(in - word_start - num_changes), BOLD_S});
            ++num_changes;
            break;
        case tt_codepoint:
            style_change(current_style, TT_S);
            changes.push_back(FormattingChange{size_t(in - word_start - num_changes), TT_S});
            ++num_changes;
            break;
        case smallcaps_codepoint:
            style_change(current_style, SMALLCAPS_S);
            changes.push_back(FormattingChange{size_t(in - word_start - num_changes), SMALLCAPS_S});
            ++num_changes;
            break;
        case superscript_codepoint:
            style_change(current_style, SUPERSCRIPT_S);
            changes.push_back(
                FormattingChange{size_t(in - word_start - num_changes), SUPERSCRIPT_S});
            ++num_changes;
            break;
        default:
            char tmp[10];
            const int bytes_written = g_unichar_to_utf8(c, tmp);
            tmp[bytes_written] = '\0';
            buf += tmp;
        }
        in = g_utf8_next_char(in);
    }
    word = buf;
    return changes;
}

void render_formatted_lines(const std::vector<std::vector<std::string>> &lines,
                            double &x,
                            double &y,
                            const double &bottom_watermark,
                            int &current_page,
                            const margins &m,
                            const PageSize &page,
                            const ChapterParameters &text_par,
                            PdfRenderer &book) {
    const bool debug_draw = true;
    size_t line_num = 0;
    for(const auto &markup_words : lines) {
        double current_indent = line_num == 0 ? text_par.indent : 0;
        if(y >= bottom_watermark) {
            render_page_num(book, text_par.font, current_page, page, m);
            book.new_page();
            ++current_page;
            y = m.upper;
            x = current_page % 2 ? m.inner : m.outer;
            if(debug_draw) {
                if(current_page % 2) {
                    book.draw_box(mm2pt(m.inner),
                                  mm2pt(m.upper),
                                  mm2pt(page.w_mm - m.inner - m.outer),
                                  mm2pt(page.h_mm - m.upper - m.lower));
                } else {
                    book.draw_box(mm2pt(m.outer),
                                  mm2pt(m.upper),
                                  mm2pt(page.w_mm - m.inner - m.outer),
                                  mm2pt(page.h_mm - m.upper - m.lower));
                }
            }
        }
        if(line_num < lines.size() - 1) {
            book.render_line_justified(markup_words,
                                       text_par.font,
                                       text_par.paragraph_width_mm - current_indent,
                                       mm2pt(x + current_indent),
                                       mm2pt(y));
        } else {
            book.render_markup_as_is(
                markup_words, text_par.font, mm2pt(x + current_indent), mm2pt(y));
        }
        line_num++;
        y += pt2mm(text_par.line_height_pt);
    }
}

std::vector<EnrichedWord> text_to_formatted_words(const std::string &text,
                                                  const WordHyphenator &hyphen) {
    StyleStack current_style;
    auto plain_words = split_to_words(std::string_view(text));
    std::vector<EnrichedWord> processed_words;
    for(const auto &word : plain_words) {
        auto working_word = word;
        auto start_style = current_style;
        auto formatting_data = extract_styling(current_style, working_word);
        auto hyphenation_data = hyphen.hyphenate(working_word);
        processed_words.emplace_back(EnrichedWord{std::move(working_word),
                                                  std::move(hyphenation_data),
                                                  std::move(formatting_data),
                                                  start_style});
    }
    return processed_words;
}

void create_pdf(const char *ofilename, const Document &doc) {
    PageSize page;
    page.w_mm = 110;
    page.h_mm = 175;
    PdfRenderer book(ofilename, mm2pt(page.w_mm), mm2pt(page.h_mm));
    margins m;
    FontParameters title_font;
    ChapterParameters text_par;
    text_par.font.name = "Gentium";
    text_par.font.point_size = 10;
    text_par.font.type = FontStyle::Regular;
    text_par.indent = 5;
    text_par.line_height_pt = 12;
    text_par.paragraph_width_mm = page.w_mm - m.inner - m.outer;
    ChapterParameters code_par = text_par;
    code_par.font.name = "Liberation Mono";
    code_par.font.point_size = 8;
    ChapterParameters footnote_par = text_par;
    footnote_par.font.point_size = 9;
    footnote_par.line_height_pt = 11;
    footnote_par.indent = 4;
    ExtraPenaltyAmounts extras;
    const double bottom_watermark = page.h_mm - m.lower - pt2mm(text_par.line_height_pt);
    const double title_above_space = 30;
    const double title_below_space = 10;
    const double different_paragraph_space = 2;
    title_font.name = "Noto sans";
    title_font.point_size = 14;
    title_font.type = FontStyle::Bold;
    double x = m.inner;
    double y = m.upper;
    const double indent = 5;
    WordHyphenator hyphen;
    int current_page = 1;
    bool first_paragraph = true;
    bool first_section = true;
    for(const auto &e : doc.elements) {
        if(std::holds_alternative<Section>(e)) {
            const Section &s = std::get<Section>(e);
            if(!first_section) {
                render_page_num(book, text_par.font, current_page, page, m);
                book.new_page();
                ++current_page;
                if(current_page % 2 == 0) {
                    book.new_page();
                    ++current_page;
                }
            }
            first_section = false;
            y = m.upper;
            x = current_page % 2 ? m.inner : m.outer;
            y += title_above_space;
            assert(s.level == 1);
            std::string full_title = std::to_string(s.number);
            full_title += ". ";
            full_title += s.text;
            book.render_markup_as_is(full_title.c_str(), title_font, mm2pt(x), mm2pt(y));
            y += pt2mm(title_font.point_size);
            y += title_below_space;
            first_paragraph = true;
        } else if(std::holds_alternative<Paragraph>(e)) {
            const Paragraph &p = std::get<Paragraph>(e);
            StyleStack current_style;
            text_par.indent = first_paragraph ? 0 : indent;
            std::vector<EnrichedWord> processed_words = text_to_formatted_words(p.text, hyphen);
            ParagraphFormatter b(processed_words, text_par, extras);
            auto lines = b.split_formatted_lines();
            render_formatted_lines(
                lines, x, y, bottom_watermark, current_page, m, page, text_par, book);
            first_paragraph = false;
        } else if(std::holds_alternative<Footnote>(e)) {
            const Footnote &f = std::get<Footnote>(e);
            StyleStack current_style;
            std::vector<EnrichedWord> processed_words = text_to_formatted_words(f.text, hyphen);
            ParagraphFormatter b(processed_words, footnote_par, extras);
            auto lines = b.split_formatted_lines();
            std::string fnum = std::to_string(f.number);
            fnum += '.';
            book.render_text_as_is(fnum.c_str(), footnote_par.font, mm2pt(x), mm2pt(y));
            render_formatted_lines(
                lines, x, y, bottom_watermark, current_page, m, page, footnote_par, book);
        } else if(std::holds_alternative<SceneChange>(e)) {
            y += pt2mm(text_par.line_height_pt);
            if(y >= bottom_watermark) {
                render_page_num(book, text_par.font, current_page, page, m);
                book.new_page();
                ++current_page;
                y = m.upper;
                x = current_page % 2 ? m.inner : m.outer;
            }
            first_paragraph = true;
        } else if(std::holds_alternative<CodeBlock>(e)) {
            const CodeBlock &cb = std::get<CodeBlock>(e);
            y += different_paragraph_space;
            for(const auto &line : cb.raw_lines) {
                if(y >= bottom_watermark) {
                    render_page_num(book, text_par.font, current_page, page, m);
                    book.new_page();
                    ++current_page;
                    y = m.upper;
                    x = current_page % 2 ? m.inner : m.outer;
                }
                book.render_text_as_is(line.c_str(), code_par.font, mm2pt(x), mm2pt(y));
                y += pt2mm(code_par.line_height_pt);
            }
            first_paragraph = true;
            y += different_paragraph_space;
        } else {
            std::abort();
        }
    }
}

void package(const char *ofilename, const char *builddir) {
    std::string command{"cd "};
    command += builddir;
    command += "; zip -r -n .png:.jpg:.tif:.gif ../";
    command += ofilename;
    command += " mimetype META-INF OEBPS";
    command += " > /dev/null";

    // I feel shame.
    if(system(command.c_str()) != 0) {
        std::abort();
    }
}

void generate_epub_manifest(tinyxml2::XMLNode *manifest,
                            const Document &doc,
                            const char *coverfile) {
    auto opf = manifest->GetDocument();

    const int bufsize = 128;
    char buf[bufsize];
    int chapter = 1;
    for(const auto &e : doc.elements) {
        if(!std::holds_alternative<Section>(e)) {
            continue;
        }
        snprintf(buf, bufsize, "chapter%d", chapter);
        auto node = opf->NewElement("item");
        manifest->InsertEndChild(node);
        node->SetAttribute("id", buf);
        strcat(buf, ".xhtml");
        node->SetAttribute("href", buf);
        node->SetAttribute("media-type", "application/xhtml+xml");
        ++chapter;
    }
    if(doc.num_footnotes() > 0) {
        auto node = opf->NewElement("item");
        manifest->InsertEndChild(node);
        node->SetAttribute("id", "footnotes");
        node->SetAttribute("href", "footnotes.xhtml");
        node->SetAttribute("media-type", "application/xhtml+xml");
    }

    auto css = opf->NewElement("item");
    manifest->InsertEndChild(css);
    css->SetAttribute("id", "stylesheet");
    css->SetAttribute("href", "book.css");
    css->SetAttribute("media-type", "text/css");

    if(coverfile) {
        auto item = opf->NewElement("item");
        manifest->InsertEndChild(item);
        item->SetAttribute("href", coverfile);
        item->SetAttribute("id", "coverpic");
        item->SetAttribute("media-type", "image/png");
    }
    auto ncx = opf->NewElement("item");
    manifest->InsertEndChild(ncx);
    ncx->SetAttribute("id", "ncx");
    ncx->SetAttribute("href", "toc.ncx");
    ncx->SetAttribute("media-type", "application/x-dtbncx+xml");
    // FIXME add images, fonts and CSS.
}

void generate_epub_spine(tinyxml2::XMLNode *spine, const Document &doc) {
    auto opf = spine->GetDocument();

    const int bufsize = 128;
    char buf[bufsize];
    int chapter = 1;
    for(const auto &e : doc.elements) {
        if(!std::holds_alternative<Section>(e)) {
            continue;
        }
        snprintf(buf, bufsize, "chapter%d", chapter);
        auto node = opf->NewElement("itemref");
        spine->InsertEndChild(node);
        node->SetAttribute("idref", buf);
        ++chapter;
    }
    if(doc.num_footnotes() > 0) {
        auto node = opf->NewElement("itemref");
        spine->InsertEndChild(node);
        node->SetAttribute("idref", "footnotes");
    }
}

void write_opf(const fs::path &ofile, const Document &doc, const char *coverfile) {
    tinyxml2::XMLDocument opf;

    auto decl = opf.NewDeclaration(nullptr);
    opf.InsertFirstChild(decl);
    auto package = opf.NewElement("package");
    package->SetAttribute("version", "2.0");
    package->SetAttribute("xmlns", "http://www.idpf.org/2007/opf");
    package->SetAttribute("unique-identifier", "id");

    opf.InsertEndChild(package);

    auto metadata = opf.NewElement("metadata");
    package->InsertFirstChild(metadata);
    metadata->SetAttribute("xmlns:dc", "http://purl.org/dc/elements/1.1/");
    metadata->SetAttribute("xmlns:opf", "http://www.idpf.org/2007/opf");

    auto name = opf.NewElement("dc:title");
    metadata->InsertEndChild(name);
    name->SetText("Book title");
    auto language = opf.NewElement("dc:language");
    language->SetText("en");
    metadata->InsertEndChild(language);
    auto identifier = opf.NewElement("dc:identifier");
    metadata->InsertEndChild(identifier);
    identifier->SetAttribute("id", "BookId");
    identifier->SetAttribute("opf:scheme", "ISBN");
    identifier->SetText("123456789X");
    auto creator = opf.NewElement("dc:creator");
    metadata->InsertEndChild(creator);
    creator->SetAttribute("opf:file-as", "Name, Author");
    creator->SetAttribute("opf:role", "aut");
    creator->SetText("Author Name");
    if(coverfile) {
        auto meta = opf.NewElement("meta");
        metadata->InsertEndChild(meta);
        meta->SetAttribute("name", "cover");
        meta->SetAttribute("content", "coverpic");
    }

    auto manifest = opf.NewElement("manifest");
    package->InsertEndChild(manifest);
    generate_epub_manifest(manifest, doc, coverfile);

    auto spine = opf.NewElement("spine");
    package->InsertEndChild(spine);
    spine->SetAttribute("toc", "ncx");
    generate_epub_spine(spine, doc);

    if(opf.SaveFile(ofile.c_str()) != tinyxml2::XML_SUCCESS) {
        printf("Writing opf failed.\n");
        std::abort();
    }
}

void write_navmap(tinyxml2::XMLElement *root, const Document &doc) {
    auto ncx = root->GetDocument();

    const int bufsize = 128;
    char buf[bufsize];

    auto navmap = ncx->NewElement("navMap");
    root->InsertEndChild(navmap);

    int chapter = 1;
    for(const auto &e : doc.elements) {
        if(!std::holds_alternative<Section>(e)) {
            continue;
        }
        snprintf(buf, bufsize, "chapter%d", chapter);
        auto navpoint = ncx->NewElement("navPoint");
        navmap->InsertEndChild(navpoint);
        navpoint->SetAttribute("class", "chapter");
        navpoint->SetAttribute("id", buf);
        snprintf(buf, bufsize, "%d", chapter);
        navpoint->SetAttribute("playOrder", buf);
        auto navlabel = ncx->NewElement("navLabel");
        navpoint->InsertEndChild(navlabel);
        auto text = ncx->NewElement("text");
        navlabel->InsertEndChild(text);
        snprintf(buf, bufsize, "Chapter %d", chapter);
        text->SetText(buf);
        auto content = ncx->NewElement("content");
        navpoint->InsertEndChild(content);
        snprintf(buf, bufsize, "chapter%d.xhtml", chapter);
        content->SetAttribute("src", buf);
        ++chapter;
    }
    if(doc.num_footnotes() > 0) {
        auto navpoint = ncx->NewElement("navPoint");
        navpoint->SetAttribute("id", "footnotes");
        navpoint->SetAttribute("class", "chapter");
        snprintf(buf, bufsize, "%d", chapter);
        ++chapter;
        navpoint->SetAttribute("playOrder", buf);
        auto navlabel = ncx->NewElement("navLabel");
        navpoint->InsertEndChild(navlabel);
        auto text = ncx->NewElement("text");
        text->SetText("Footnotes");
        navlabel->InsertEndChild(text);
        auto content = ncx->NewElement("content");
        content->SetAttribute("src", "footnotes.xhtml");
        navpoint->InsertEndChild(content);
        navmap->InsertEndChild(navpoint);
    }
}

void write_ncx(const char *ofile, const Document &doc) {
    tinyxml2::XMLDocument ncx;

    auto decl = ncx.NewDeclaration(nullptr);
    ncx.InsertFirstChild(decl);
    auto doctype = ncx.NewUnknown(
        R"(DOCTYPE ncx PUBLIC "-//NISO//DTD ncx 2005-1//EN" "http://www.daisy.org/z3986/2005/ncx-2005-1.dtd")");
    ncx.InsertEndChild(doctype);

    auto root = ncx.NewElement("ncx");
    ncx.InsertEndChild(root);
    root->SetAttribute("version", "2005-1");
    root->SetAttribute("xml:lang", "en");
    root->SetAttribute("xmlns", "http://www.daisy.org/z3986/2005/ncx/");

    auto head = ncx.NewElement("head");
    root->InsertEndChild(head);

    auto meta = ncx.NewElement("meta");
    head->InsertEndChild(meta);
    meta->SetAttribute("name", "dtb:uid");
    meta->SetAttribute("content", "123456789X");

    meta = ncx.NewElement("meta");
    head->InsertEndChild(meta);
    meta->SetAttribute("name", "dtb:depth");
    meta->SetAttribute("content", "1");

    meta = ncx.NewElement("meta");
    head->InsertEndChild(meta);
    meta->SetAttribute("name", "dtb:totalPageCount");
    meta->SetAttribute("content", "0");

    meta = ncx.NewElement("meta");
    head->InsertEndChild(meta);
    meta->SetAttribute("name", "dtb:maxPageNumber");
    meta->SetAttribute("content", "0");

    auto doctitle = ncx.NewElement("docTitle");
    root->InsertEndChild(doctitle);
    auto text = ncx.NewElement("text");
    doctitle->InsertEndChild(text);
    text->SetText("Name of the Book");

    auto docauthor = ncx.NewElement("docAuthor");
    root->InsertEndChild(docauthor);
    text = ncx.NewElement("text");
    docauthor->InsertEndChild(text);
    text->SetText("Author, Name");

    write_navmap(root, doc);
    ncx.SaveFile(ofile);
}

bool is_stylechar(char c) {
    return c == italic_character || c == bold_character || c == tt_character ||
           c == smallcaps_character || c == superscript_character;
}

void handle_tag_switch(tinyxml2::XMLDocument &doc,
                       StyleStack &current_style,
                       std::stack<tinyxml2::XMLNode *> &tagstack,
                       std::string &buf,
                       char style,
                       const char *tag_name,
                       const char *attribute = nullptr,
                       const char *value = nullptr) {
    if(current_style.contains(style)) {
        auto ptext = doc.NewText(buf.c_str());
        buf.clear();
        tagstack.top()->InsertEndChild(ptext);
        current_style.pop(style);
        tagstack.pop();
    } else {
        auto ptext = doc.NewText(buf.c_str());
        buf.clear();
        auto new_element = doc.NewElement(tag_name);
        if(attribute) {
            new_element->SetAttribute(attribute, value);
        }
        tagstack.top()->InsertEndChild(ptext);
        tagstack.top()->InsertEndChild(new_element);
        tagstack.push(static_cast<tinyxml2::XMLNode *>(new_element));
        current_style.push(style);
    }
}

tinyxml2::XMLElement *write_header(tinyxml2::XMLDocument &epubdoc) {
    auto decl = epubdoc.NewDeclaration(nullptr);
    epubdoc.InsertFirstChild(decl);
    auto doctype = epubdoc.NewUnknown(
        R"(DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.1//EN" "http://www.w3.org/TR/xhtml11/DTD/xhtml11.dtd")");
    epubdoc.InsertEndChild(doctype);
    auto html = epubdoc.NewElement("html");
    epubdoc.InsertEndChild(html);
    html->SetAttribute("xmlns", "http://www.w3.org/1999/xhtml");
    html->SetAttribute("xml:lang", "en");

    auto head = epubdoc.NewElement("head");
    html->InsertEndChild(head);
    auto meta = epubdoc.NewElement("meta");
    head->InsertEndChild(meta);
    meta->SetAttribute("http-equiv", "Content-Type");
    meta->SetAttribute("content", "application/xhtml+xml; charset=utf-8");
    auto title = epubdoc.NewElement("title");
    head->InsertEndChild(title);
    title->SetText("Name of Book");
    auto style = epubdoc.NewElement("link");
    head->InsertEndChild(style);
    style->SetAttribute("rel", "stylesheet");
    style->SetAttribute("href", "book.css");
    style->SetAttribute("type", "text/css");

    auto body = epubdoc.NewElement("body");
    html->InsertEndChild(body);
    return body;
}

tinyxml2::XMLElement *write_block_of_text(tinyxml2::XMLDocument &epubdoc, const std::string &text) {
    StyleStack current_style;
    std::stack<tinyxml2::XMLNode *> tagstack;
    auto p = epubdoc.NewElement("p");
    tagstack.push(p);
    std::string buf;
    for(char c : text) {
        switch(c) {
        case italic_character:
            handle_tag_switch(epubdoc, current_style, tagstack, buf, ITALIC_S, "i");
            break;
        case bold_character:
            handle_tag_switch(epubdoc, current_style, tagstack, buf, BOLD_S, "b");
            break;
        case tt_character:
            handle_tag_switch(epubdoc, current_style, tagstack, buf, TT_S, "tt");
            break;
        case superscript_character:
            handle_tag_switch(epubdoc, current_style, tagstack, buf, SUPERSCRIPT_S, "sup");
            break;
        case smallcaps_character:
            handle_tag_switch(epubdoc,
                              current_style,
                              tagstack,
                              buf,
                              SMALLCAPS_S,
                              "span",
                              "variant",
                              "small-caps");
            break;
        default:
            buf += c;
        }
    }
    assert(!tagstack.empty());
    if(!buf.empty()) {
        auto ending = epubdoc.NewText(buf.c_str());
        tagstack.top()->InsertEndChild(ending);
    }
    tagstack.pop();
    assert(tagstack.empty());
    return p;
}

void write_paragraph(tinyxml2::XMLDocument &epubdoc,
                     tinyxml2::XMLElement *body,
                     const Paragraph &par) {
    body->InsertEndChild(write_block_of_text(epubdoc, par.text));
}

void write_codeblock(tinyxml2::XMLDocument &epubdoc,
                     tinyxml2::XMLElement *body,
                     const CodeBlock &code) {
    auto p = epubdoc.NewElement("p");
    p->SetAttribute("id", "preformatted");
    for(size_t i = 0; i < code.raw_lines.size(); ++i) {
        auto *textline = epubdoc.NewText(code.raw_lines[i].c_str());
        p->InsertEndChild(textline);
        if(i != code.raw_lines.size() - 1) {
            p->InsertEndChild(epubdoc.NewElement("br"));
        }
    }
    body->InsertEndChild(p);
}

void write_footnotes(const fs::path &outdir, Document &doc) {
    const auto num_footnotes = doc.num_footnotes();
    if(num_footnotes == 0) {
        return;
    }
    tinyxml2::XMLDocument epubdoc;
    tinyxml2::XMLElement *body = write_header(epubdoc);
    auto heading = epubdoc.NewElement("h2");
    heading->SetText("Footnotes");
    body->InsertEndChild(heading);
    const auto ofile = outdir / "footnotes.xhtml";
    std::string temphack;

    for(const auto &e : doc.elements) {
        if(!std::holds_alternative<Footnote>(e)) {
            continue;
        }
        const Footnote &fn = std::get<Footnote>(e);
        temphack = std::to_string(fn.number);
        temphack += ". ";
        temphack += fn.text;
        // FIXME, add link back.
        auto p = write_block_of_text(epubdoc, temphack.c_str());
        p->SetAttribute("class", "footnote");
        body->InsertEndChild(p);
    }
    epubdoc.SaveFile(ofile.c_str());
}

void write_chapters(const fs::path &outdir, Document &doc) {
    const int bufsize = 128;
    char tmpbuf[bufsize];
    tinyxml2::XMLDocument epubdoc;

    auto ofile = outdir / "__BUG__";
    int chapter = 1;
    assert(!doc.elements.empty());
    if(!std::holds_alternative<Section>(doc.elements.front())) {
        printf("Document must begin with a section marker.\n");
        std::abort();
    }
    bool first_chapter = true;
    tinyxml2::XMLElement *body = nullptr;

    for(const auto &e : doc.elements) {
        if(std::holds_alternative<Paragraph>(e)) {
            write_paragraph(epubdoc, body, std::get<Paragraph>(e));
        } else if(std::holds_alternative<Section>(e)) {
            const auto &sec = std::get<Section>(e);
            if(first_chapter) {
                first_chapter = false;
            } else {
                epubdoc.SaveFile(ofile.c_str());
                epubdoc.Clear();
            }
            assert(std::holds_alternative<Section>(e));
            body = write_header(epubdoc);
            snprintf(tmpbuf, bufsize, "chapter%d.xhtml", chapter);
            ofile = outdir / tmpbuf;
            snprintf(tmpbuf, bufsize, "%d. ", chapter);
            ++chapter;
            auto heading = epubdoc.NewElement("h2");
            body->InsertEndChild(heading);
            heading->SetText((tmpbuf + sec.text).c_str());
        } else if(std::holds_alternative<CodeBlock>(e)) {
            write_codeblock(epubdoc, body, std::get<CodeBlock>(e));
        } else if(std::holds_alternative<SceneChange>(e)) {
            auto p = epubdoc.NewElement("p");
            p->SetText(" "); // There may be a smarter way of doing this.
            body->InsertEndChild(p);
        } else if(std::holds_alternative<Footnote>(e)) {
            // These contain the footnote text and are not written here.
            // We just ignore them.

            // FIXME: add links to the footnote file.
        } else {
            std::abort();
        }
    }
    // FIXME, assumes that the last entry is not a section declaration. Which is possible, but
    // very silly.
    epubdoc.SaveFile(ofile.c_str());
}

void create_epub(const char *ofilename, Document &doc) {
    fs::path outdir{"epubtmp"};
    fs::remove_all(outdir);
    auto metadir = outdir / "META-INF";
    auto oebpsdir = outdir / "OEBPS";
    auto mimefile = outdir / "mimetype";
    auto containerfile = metadir / "container.xml";
    auto contentfile = oebpsdir / "content.opf";
    auto ncxfile = oebpsdir / "toc.ncx";
    auto cssfile = oebpsdir / "book.css";

    fs::path cover_in{"cover.png"};
    fs::path cover_out = oebpsdir / cover_in;
    bool has_cover = false;

    fs::create_directories(metadir);
    fs::create_directory(oebpsdir);

    if(fs::exists(cover_in)) {
        has_cover = true;
        fs::copy(cover_in, cover_out);
    }

    FILE *f = fopen(mimefile.c_str(), "w");
    fwrite(mimetext, 1, strlen(mimetext), f);
    fclose(f);

    f = fopen(containerfile.c_str(), "w");
    fwrite(containertext, 1, strlen(containertext), f);
    fclose(f);

    f = fopen(cssfile.c_str(), "w");
    fwrite(csstext, 1, strlen(csstext), f);
    fclose(f);

    write_opf(contentfile, doc, has_cover ? cover_in.c_str() : nullptr);
    write_ncx(ncxfile.c_str(), doc);
    write_chapters(oebpsdir, doc);
    write_footnotes(oebpsdir, doc);

    unlink(ofilename);
    package(ofilename, outdir.c_str());
}

Document load_document(const char *fname) {
    Document doc;
    MMapper map(fname);
    if(!g_utf8_validate(map.data(), map.size(), nullptr)) {
        printf("Invalid utf-8.\n");
        std::abort();
    }
    GError *err = nullptr;
    if(err) {
        std::abort();
    }

    LineParser linep(map.data(), map.size());
    StructureParser strucp;

    line_token token = linep.next();
    while(!std::holds_alternative<EndOfFile>(token)) {
        strucp.push(token);
        token = linep.next();
    }
    strucp.push(token);

    // FIXME, add stored data if any.
    return strucp.get_document();
}

int main(int argc, char **argv) {
    if(argc != 2) {
        printf("%s <input text file>\n", argv[0]);
        return 1;
    }
    auto doc = load_document(argv[1]);
    /*
    printf("The file had %d chapters.\n", (int)chapters.size());
    printf("%s: %d paragraphs.\n",
           chapters.front().title.c_str(),
           (int)chapters.front().paragraphs.size());
    // printf("%s\n", chapters.front().paragraphs.back().c_str());
    */
    create_pdf("bookout.pdf", doc);
    create_epub("war_test.epub", doc);
    return 0;
}
