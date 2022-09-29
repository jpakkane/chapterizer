#include <epub.hpp>
#include <formatting.hpp>

#include <cassert>

#include <stack>

namespace fs = std::filesystem;

namespace {

std::array<const char *, 3> langnames{"unknown", "en", "fi"};

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

void handle_tag_switch(tinyxml2::XMLDocument &epubdoc,
                       StyleStack &current_style,
                       std::stack<tinyxml2::XMLNode *> &tagstack,
                       std::string &buf,
                       char style,
                       const char *tag_name,
                       const char *attribute = nullptr,
                       const char *value = nullptr) {
    if(current_style.contains(style)) {
        auto ptext = epubdoc.NewText(buf.c_str());
        buf.clear();
        tagstack.top()->InsertEndChild(ptext);
        current_style.pop(style);
        tagstack.pop();
    } else {
        auto ptext = epubdoc.NewText(buf.c_str());
        buf.clear();
        auto new_element = epubdoc.NewElement(tag_name);
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

} // namespace

Epub::Epub(const Document &d) : doc(d) {}

void Epub::generate(const char *ofilename) {
    fs::path outdir{"epubtmp"};
    fs::remove_all(outdir);
    auto metadir = outdir / "META-INF";
    oebpsdir = outdir / "OEBPS";
    auto mimefile = outdir / "mimetype";
    auto containerfile = metadir / "container.xml";
    auto contentfile = oebpsdir / "content.opf";
    auto ncxfile = oebpsdir / "toc.ncx";
    auto cssfile = oebpsdir / "book.css";

    fs::create_directories(metadir);
    fs::create_directory(oebpsdir);

    if(!doc.data.epub.cover.empty()) {
        fs::path cover_in = doc.data.top_dir / doc.data.epub.cover;
        fs::path cover_out = oebpsdir / "cover.png";
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

    write_chapters(oebpsdir);
    write_footnotes(oebpsdir);
    write_opf(contentfile);
    write_ncx(ncxfile.c_str());

    unlink(ofilename);
    package(ofilename, outdir.c_str());
}

void Epub::write_opf(const fs::path &ofile) {
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
    language->SetText(langnames[size_t(doc.data.language)]);
    metadata->InsertEndChild(language);
    auto identifier = opf.NewElement("dc:identifier");
    metadata->InsertEndChild(identifier);
    identifier->SetAttribute("id", "BookId");
    identifier->SetAttribute("opf:scheme", "ISBN");
    identifier->SetText(doc.data.epub.ISBN.c_str());
    auto creator = opf.NewElement("dc:creator");
    metadata->InsertEndChild(creator);
    creator->SetAttribute("opf:file-as", doc.data.epub.file_as.c_str());
    creator->SetAttribute("opf:role", "aut");
    creator->SetText(doc.data.author.c_str());
    if(!doc.data.epub.cover.empty()) {
        auto meta = opf.NewElement("meta");
        metadata->InsertEndChild(meta);
        meta->SetAttribute("name", "cover");
        meta->SetAttribute("content", "coverpic");
    }

    auto manifest = opf.NewElement("manifest");
    package->InsertEndChild(manifest);
    generate_epub_manifest(manifest);

    auto spine = opf.NewElement("spine");
    package->InsertEndChild(spine);
    spine->SetAttribute("toc", "ncx");
    generate_spine(spine);

    if(opf.SaveFile(ofile.c_str()) != tinyxml2::XML_SUCCESS) {
        printf("Writing opf failed.\n");
        std::abort();
    }
}

void Epub::write_ncx(const char *ofile) {
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
    text->SetText(doc.data.title.c_str());

    auto docauthor = ncx.NewElement("docAuthor");
    root->InsertEndChild(docauthor);
    text = ncx.NewElement("text");
    docauthor->InsertEndChild(text);
    text->SetText(doc.data.author.c_str());

    write_navmap(root);
    ncx.SaveFile(ofile);
}

void Epub::write_chapters(const fs::path &outdir) {
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
        } else if(std::holds_alternative<Figure>(e)) {
            const auto &figure = std::get<Figure>(e);
            const auto imagepath = get_epub_image_path(figure.file);
            auto p = epubdoc.NewElement("p");
            body->InsertEndChild(p);
            auto img = epubdoc.NewElement("img");
            img->SetAttribute("src", imagepath.c_str());
            p->InsertEndChild(img);
        } else {
            std::abort();
        }
    }
    // FIXME, assumes that the last entry is not a section declaration. Which is possible, but
    // very silly.
    epubdoc.SaveFile(ofile.c_str());
}

void Epub::write_footnotes(const fs::path &outdir) {
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

void Epub::write_navmap(tinyxml2::XMLElement *root) {
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

void Epub::generate_epub_manifest(tinyxml2::XMLNode *manifest) {
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

    int imagenum = 0;
    for(const auto &image : embedded_images) {
        auto item = opf->NewElement("item");
        snprintf(buf, bufsize, "image%d", imagenum);
        item->SetAttribute("id", buf);
        item->SetAttribute("href", image.c_str());
        item->SetAttribute("media-type", "image/png");
        manifest->InsertEndChild(item);
        ++imagenum;
    }

    if(!doc.data.epub.cover.empty()) {
        auto item = opf->NewElement("item");
        manifest->InsertEndChild(item);
        item->SetAttribute("href", "cover.png");
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

void Epub::generate_spine(tinyxml2::XMLNode *spine) {
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

std::string Epub::get_epub_image_path(const std::string &fs_name) {
    auto it = imagenames.find(fs_name);
    if(it != imagenames.end()) {
        return it->second;
    }
    char buf[1024];
    snprintf(buf, 1024, "image-%d.png", (int)imagenames.size());
    std::string epub_name{buf};
    auto epub_path = oebpsdir / epub_name;
    auto fullpath = doc.data.top_dir / fs_name;
    std::filesystem::copy_file(fullpath, epub_path);

    embedded_images.push_back(epub_name);
    imagenames[fs_name] = epub_name;

    return epub_name;
}
