#pragma once

#include <string>
#include <vector>

double mm2pt(const double x);
double pt2mm(const double x);

std::vector<std::string> split_to_words(std::string_view in_text);
std::vector<std::string> split_to_lines(const std::string &in_text);
