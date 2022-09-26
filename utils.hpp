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

#include <string>
#include <vector>
#include <cstdint>

double mm2pt(const double x);
double pt2mm(const double x);

std::vector<std::string> split_to_words(std::string_view in_text);
std::vector<std::string> split_to_lines(const std::string &in_text);

class MMapper {
public:
    explicit MMapper(const char *path);
    ~MMapper();

    const char *data() const { return buf; }
    int64_t size() const { return bufsize; }

private:
    const char *buf;
    int64_t bufsize;
};

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
