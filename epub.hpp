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

#include <bookparser.hpp>
#include <filesystem>
#include <tinyxml2.h>
#include <unordered_map>

class Epub {
public:
    explicit Epub(const Document &d);

    void generate(const char *ofilename);

private:
    void write_opf(const std::filesystem::path &ofile);
    void write_ncx(const char *ofile);
    void write_chapters(const std::filesystem::path &outdir);
    void write_footnotes(const std::filesystem::path &outdir);
    void write_navmap(tinyxml2::XMLElement *root);
    void generate_epub_manifest(tinyxml2::XMLNode *manifest);
    void generate_spine(tinyxml2::XMLNode *spine);

    std::string get_epub_image_path(const std::string &fs_name);
    const Document &doc;

    std::filesystem::path oebpsdir;
    std::unordered_map<std::string, std::string> imagenames;
    std::vector<std::string> embedded_images;
};
