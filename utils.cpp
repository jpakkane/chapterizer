#include "utils.hpp"
#include <sstream>

double mm2pt(const double x) { return x * 2.8346456693; }
double pt2mm(const double x) { return x / 2.8346456693; }

std::vector<std::string> split_to_words(std::string_view in_text) {
    std::string text;
    text.reserve(in_text.size());
    for(size_t i = 0; i < in_text.size(); ++i) {
        if(in_text[i] == '\n') {
            text.push_back(' ');
        } else {
            text.push_back(in_text[i]);
        }
    }
    while(!text.empty() && text.back() == ' ') {
        text.pop_back();
    }
    while(!text.empty() && text.front() == ' ') {
        text.erase(text.begin());
    }
    std::string val;
    const char separator = ' ';
    std::vector<std::string> words;
    std::stringstream sstream(text);
    while(std::getline(sstream, val, separator)) {
        if(!val.empty()) {
            words.push_back(val);
        }
    }
    return words;
}

std::vector<std::string> split_to_lines(const std::string &in_text) {
    std::vector<std::string> lines;
    std::string val;
    const char separator = '\n';
    std::vector<std::string> words;
    std::stringstream sstream(in_text);
    while(std::getline(sstream, val, separator)) {
        while(!val.empty() && val.back() == ' ') {
            val.pop_back();
        }
        while(!val.empty() && val.front() == ' ') {
            val.erase(val.begin());
        }
        if(!val.empty()) {
            words.push_back(val);
        }
    }
    return words;
}
