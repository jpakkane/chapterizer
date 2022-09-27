#pragma once

#include <bookparser.hpp>
#include <filesystem>
#include <tinyxml2.h>
#include <unordered_map>

class Epub {
public:
    explicit Epub(const Document &d);

    void generate(const char *ofilename);

private:
    void write_opf(const std::filesystem::path &ofile, const char *coverfile);
    void write_ncx(const char *ofile);
    void write_chapters(const std::filesystem::path &outdir);
    void write_footnotes(const std::filesystem::path &outdir);
    void write_navmap(tinyxml2::XMLElement *root);
    void generate_epub_manifest(tinyxml2::XMLNode *manifest, const char *coverfile);
    void generate_spine(tinyxml2::XMLNode *spine);

    std::string get_epub_image_path(const std::string &fs_name);
    const Document &doc;

    std::filesystem::path oebpsdir;
    std::unordered_map<std::string, std::string> imagenames;
    std::vector<std::string> embedded_images;
};
