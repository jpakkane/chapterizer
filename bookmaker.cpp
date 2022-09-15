#include <pdfrenderer.hpp>
#include <utils.hpp>
#include <chaptercommon.hpp>
#include <paragraphformatter.hpp>
#include <wordhyphenator.hpp>
#include <formatting.hpp>

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

)";

bool looks_like_title(const std::string &line) {
    if(line.size() < 2) {
        return false;
    }
    if(line[0] == '#' && line[1] == ' ')
        return true;
    return false;
}

} // namespace

std::vector<Chapter> load_text(const char *fname) {
    std::vector<Chapter> chapters;
    std::ifstream ifile(fname);

    bool reading_title = false;
    int num_lines = 0;
    std::string title_text;
    std::string paragraph_text;
    std::vector<std::string> paragraphs;
    for(std::string line; std::getline(ifile, line);) {
        ++num_lines;
        // FIXME, strip.
        if(line.empty()) {
            if(reading_title) {
                reading_title = false;
            } else {
                if(!paragraph_text.empty()) {
                    paragraphs.emplace_back(std::move(paragraph_text));
                    paragraph_text.clear();
                }
            }
            continue;
        }
        if(!g_utf8_validate(line.c_str(), line.length(), nullptr)) {
            printf("Line %d not valid UTF-8.\n", num_lines);
            exit(1);
        }
        gchar *normalized_text = g_utf8_normalize(line.c_str(), line.length(), G_NORMALIZE_NFC);
        line = normalized_text;
        g_free(normalized_text);
        if(!reading_title) {
            if(looks_like_title(line)) {
                if(!paragraph_text.empty()) {
                    paragraphs.emplace_back(std::move(paragraph_text));
                    paragraph_text.clear();
                }
                if(!paragraphs.empty()) {
                    chapters.emplace_back(Chapter{std::move(title_text), std::move(paragraphs)});
                    title_text.clear();
                    paragraphs.clear();
                }
                reading_title = true;
                title_text = line.substr(2);
                continue;
            } else {
                paragraph_text += line;
                paragraph_text += ' ';
            }
        } else {
            title_text += line;
            title_text += ' ';
        }
    }
    if(!paragraph_text.empty()) {
        paragraphs.emplace_back(std::move(paragraph_text));
    }
    printf("File had %d lines.\n", num_lines);
    chapters.emplace_back(Chapter{std::move(title_text), std::move(paragraphs)});
    return chapters;
}

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

void render_page_num(
    PdfRenderer &book, FontParameters &par, int page_num, const PageSize &p, const margins &m) {
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

void create_pdf(const char *ofilename, std::vector<Chapter> &chapters) {
    PageSize page;

    page.w_mm = 110;
    page.h_mm = 175;
    PdfRenderer book(ofilename, mm2pt(page.w_mm), mm2pt(page.h_mm));
    margins m;
    FontParameters title_font;
    ChapterParameters chapter_par;
    chapter_par.font.name = "Gentium";
    chapter_par.font.point_size = 10;
    chapter_par.font.type = FontStyle::Regular;
    chapter_par.indent = 5; // First chapter does not get indented.
    chapter_par.line_height_pt = 12;
    chapter_par.paragraph_width_mm = page.w_mm - m.inner - m.outer;
    ExtraPenaltyAmounts extras;
    const double bottom_watermark = page.h_mm - m.lower - pt2mm(chapter_par.line_height_pt);
    const double title_above_space = 30;
    const double title_below_space = 10;
    title_font.name = "Noto sans";
    title_font.point_size = 14;
    title_font.type = FontStyle::Bold;
    double x = m.inner;
    double y = m.upper;
    const double indent = 5;
    WordHyphenator hyphen;
    int current_page = 1;
    bool first_chapter = true;
    int chapter_num = 0;
    for(const auto &c : chapters) {
        printf("Processing chapter %d/%d.\n", ++chapter_num, (int)chapters.size());
        if(!first_chapter) {
            render_page_num(book, chapter_par.font, current_page, page, m);
            book.new_page();
            ++current_page;
            if(current_page % 2 == 0) {
                book.new_page();
                ++current_page;
            }
            y = m.upper;
            x = current_page % 2 ? m.inner : m.outer;
        }
        y += title_above_space;
        book.render_line_as_is(c.title.c_str(), title_font, mm2pt(x), mm2pt(y));
        y += pt2mm(title_font.point_size + 1);
        y += title_below_space;
        bool first_paragraph = true;
        for(const auto &p : c.paragraphs) {
            StyleStack current_style;
            chapter_par.indent = first_paragraph ? 0 : indent;
            auto plain_words = split_to_words(std::string_view(p));
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
            ParagraphFormatter b(processed_words, chapter_par, extras);
            auto lines = b.split_formatted_lines();
            size_t line_num = 0;
            for(const auto &markup_words : lines) {
                double current_indent = line_num == 0 ? chapter_par.indent : 0;
                if(y >= bottom_watermark) {
                    render_page_num(book, chapter_par.font, current_page, page, m);
                    book.new_page();
                    ++current_page;
                    y = m.upper;
                    x = current_page % 2 ? m.inner : m.outer;
                }
                if(line_num < lines.size() - 1) {
                    book.render_line_justified(markup_words,
                                               chapter_par.font,
                                               chapter_par.paragraph_width_mm - current_indent,
                                               mm2pt(x + current_indent),
                                               mm2pt(y));
                } else {
                    book.render_line_as_is(
                        markup_words, chapter_par.font, mm2pt(x + current_indent), mm2pt(y));
                }
                line_num++;
                y += pt2mm(chapter_par.line_height_pt);
            }
            first_paragraph = false;
        }
        first_chapter = false;
    }
    render_page_num(book, chapter_par.font, current_page, page, m);
}

void package(const char *ofilename, const char *builddir) {
    std::string command{"cd "};
    command += builddir;
    command += "; zip -r -n .png:.jpg:.tif:.gif ../";
    command += ofilename;
    command += " mimetype META-INF OEBPS";

    // I feel shame.
    if(system(command.c_str()) != 0) {
        std::abort();
    }
}

void generate_epub_manifest(tinyxml2::XMLNode *manifest,
                            const std::vector<Chapter> &chapters,
                            const char *coverfile) {
    auto opf = manifest->GetDocument();
    const int num_chapters = int(chapters.size());

    const int bufsize = 128;
    char buf[bufsize];
    for(int i = 0; i < num_chapters; i++) {
        snprintf(buf, bufsize, "chapter%d", i + 1);
        auto node = opf->NewElement("item");
        manifest->InsertEndChild(node);
        node->SetAttribute("id", buf);
        strcat(buf, ".xhtml");
        node->SetAttribute("href", buf);
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

void generate_epub_spine(tinyxml2::XMLNode *spine, const std::vector<Chapter> &chapters) {
    auto opf = spine->GetDocument();
    const int num_chapters = int(chapters.size());

    const int bufsize = 128;
    char buf[bufsize];
    for(int i = 0; i < num_chapters; i++) {
        snprintf(buf, bufsize, "chapter%d", i + 1);
        auto node = opf->NewElement("itemref");
        spine->InsertEndChild(node);
        node->SetAttribute("idref", buf);
    }
}

void write_opf(const fs::path &ofile, const std::vector<Chapter> &chapters, const char *coverfile) {
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
    name->SetText("War of the Worlds");
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
    creator->SetAttribute("opf:file-as", "Wells, HG");
    creator->SetAttribute("opf:role", "aut");
    creator->SetText("HG Wells");
    if(coverfile) {
        auto meta = opf.NewElement("meta");
        metadata->InsertEndChild(meta);
        meta->SetAttribute("name", "cover");
        meta->SetAttribute("content", "coverpic");
    }

    auto manifest = opf.NewElement("manifest");
    package->InsertEndChild(manifest);
    generate_epub_manifest(manifest, chapters, coverfile);

    auto spine = opf.NewElement("spine");
    package->InsertEndChild(spine);
    spine->SetAttribute("toc", "ncx");
    generate_epub_spine(spine, chapters);

    if(opf.SaveFile(ofile.c_str()) != tinyxml2::XML_SUCCESS) {
        printf("Writing opf failed.\n");
        std::abort();
    }
}

void write_navmap(tinyxml2::XMLElement *root, const std::vector<Chapter> &chapters) {
    auto ncx = root->GetDocument();
    const int num_chapters = int(chapters.size());

    const int bufsize = 128;
    char buf[bufsize];

    auto navmap = ncx->NewElement("navMap");
    root->InsertEndChild(navmap);

    for(int i = 0; i < num_chapters; i++) {
        snprintf(buf, bufsize, "chapter%d", i + 1);
        auto navpoint = ncx->NewElement("navPoint");
        navmap->InsertEndChild(navpoint);
        navpoint->SetAttribute("class", "chapter");
        navpoint->SetAttribute("id", buf);
        snprintf(buf, bufsize, "%d", i + 1);
        navpoint->SetAttribute("playOrder", buf);
        auto navlabel = ncx->NewElement("navLabel");
        navpoint->InsertEndChild(navlabel);
        auto text = ncx->NewElement("text");
        navlabel->InsertEndChild(text);
        snprintf(buf, bufsize, "Chapter %d", i + 1);
        text->SetText(buf);
        auto content = ncx->NewElement("content");
        navpoint->InsertEndChild(content);
        snprintf(buf, bufsize, "chapter%d.xhtml", i + 1);
        content->SetAttribute("src", buf);
    }
}

void write_ncx(const char *ofile, const std::vector<Chapter> &chapters) {
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
    text->SetText("War of the Worlds");

    auto docauthor = ncx.NewElement("docAuthor");
    root->InsertEndChild(docauthor);
    text = ncx.NewElement("text");
    docauthor->InsertEndChild(text);
    text->SetText("Wells, HG");

    write_navmap(root, chapters);
    ncx.SaveFile(ofile);
}

bool is_stylechar(char c) {
    return c == italic_character || c == bold_character || c == tt_character ||
           c == smallcaps_character;
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

void write_chapters(const fs::path &outdir, const std::vector<Chapter> &chapters) {
    const int num_chapters = int(chapters.size());

    const int bufsize = 128;
    char tmpbuf[bufsize];

    for(int i = 0; i < num_chapters; i++) {
        snprintf(tmpbuf, bufsize, "chapter%d.xhtml", i + 1);
        auto ofile = outdir / tmpbuf;
        tinyxml2::XMLDocument doc;
        auto decl = doc.NewDeclaration(nullptr);
        doc.InsertFirstChild(decl);
        auto doctype = doc.NewUnknown(
            R"(DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.1//EN" "http://www.w3.org/TR/xhtml11/DTD/xhtml11.dtd")");
        doc.InsertEndChild(doctype);
        auto html = doc.NewElement("html");
        doc.InsertEndChild(html);
        html->SetAttribute("xmlns", "http://www.w3.org/1999/xhtml");
        html->SetAttribute("xml:lang", "en");

        auto head = doc.NewElement("head");
        html->InsertEndChild(head);
        auto meta = doc.NewElement("meta");
        head->InsertEndChild(meta);
        meta->SetAttribute("http-equiv", "Content-Type");
        meta->SetAttribute("content", "application/xhtml+xml; charset=utf-8");
        auto title = doc.NewElement("title");
        head->InsertEndChild(title);
        title->SetText("War of the Worlds");
        auto style = doc.NewElement("link");
        head->InsertEndChild(style);
        style->SetAttribute("rel", "stylesheet");
        style->SetAttribute("href", "book.css");
        style->SetAttribute("type", "text/css");

        auto body = doc.NewElement("body");
        html->InsertEndChild(body);
        auto heading = doc.NewElement("h2");
        body->InsertEndChild(heading);
        heading->SetText(chapters[i].title.c_str());
        for(const auto &paragraph : chapters[i].paragraphs) {
            StyleStack current_style;
            std::stack<tinyxml2::XMLNode *> tagstack;
            auto p = doc.NewElement("p");
            tagstack.push(p);
            std::string buf;
            for(char c : paragraph) {
                switch(c) {
                case italic_character:
                    handle_tag_switch(doc, current_style, tagstack, buf, ITALIC_S, "i");
                    break;
                case bold_character:
                    handle_tag_switch(doc, current_style, tagstack, buf, BOLD_S, "b");
                    break;
                case tt_character:
                    handle_tag_switch(doc, current_style, tagstack, buf, TT_S, "tt");
                    break;
                case smallcaps_character:
                    handle_tag_switch(doc,
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
                auto ending = doc.NewText(buf.c_str());
                tagstack.top()->InsertEndChild(ending);
            }
            tagstack.pop();
            assert(tagstack.empty());
            body->InsertEndChild(p);
        }

        doc.SaveFile(ofile.c_str());
    }
}

void create_epub(const char *ofilename, const std::vector<Chapter> &chapters) {
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

    write_opf(contentfile, chapters, has_cover ? cover_in.c_str() : nullptr);
    write_ncx(ncxfile.c_str(), chapters);
    write_chapters(oebpsdir, chapters);

    unlink(ofilename);
    package(ofilename, outdir.c_str());
}

int main(int argc, char **argv) {
    if(argc != 2) {
        printf("%s <input text file>\n", argv[0]);
        return 1;
    }
    auto chapters = load_text(argv[1]);
    /*
    printf("The file had %d chapters.\n", (int)chapters.size());
    printf("%s: %d paragraphs.\n",
           chapters.front().title.c_str(),
           (int)chapters.front().paragraphs.size());
    // printf("%s\n", chapters.front().paragraphs.back().c_str());
    */
    create_pdf("bookout.pdf", chapters);
    create_epub("war_test.epub", chapters);
    return 0;
}
