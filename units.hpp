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

#include <compare>

inline double mm2pt(const double x) { return x * 2.8346456693; }
inline double pt2mm(const double x) { return x / 2.8346456693; }

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

    Point topt() const;
};

inline Millimeter operator*(const double mul, const Millimeter &p) { return p * mul; }

class Length {
private:
    explicit Length(double d) : v_m(d){};

public:
    double v_m = 0.0;

    static Length from_mm(double val) { return Length(val * 1000); }
    static Length from_pt(double val) { return Length::from_mm(pt2mm(val)); }

    Length() : v_m(0.0) {}
    Length(const Length &d) : v_m(d.v_m){};

    static Length from_value(double mm) { return Length{mm}; }
    static Length zero() { return Length{0.0}; }

    Length operator-() const { return Length{-v_m}; }

    Length &operator=(const Length &p) {
        v_m = p.v_m;
        return *this;
    }

    Length &operator+=(const Length &p) {
        v_m += p.v_m;
        return *this;
    }

    Length operator+(const Length &o) const { return Length{v_m + o.v_m}; }

    Length operator-(const Length &o) const { return Length{v_m - o.v_m}; }

    Length operator*(const double o) const { return Length{v_m * o}; }

    Length operator/(const double o) const { return Length{v_m / o}; }

    std::partial_ordering operator<=>(const Length &o) const { return v_m <=> o.v_m; }

    double pt() const { return mm2pt(mm()); }
    double mm() const { return 1000 * v_m; }
};
