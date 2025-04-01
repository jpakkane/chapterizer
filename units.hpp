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

class Length {
private:
    explicit Length(double d) : v_m(d) {};

public:
    double v_m = 0.0;

    static Length from_mm(double val) { return Length(val / 1000); }
    static Length from_pt(double val) { return Length::from_mm(pt2mm(val)); }

    Length() : v_m(0.0) {}
    Length(const Length &d) : v_m(d.v_m) {};

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

    Length &operator-=(const Length &o) {
        v_m -= o.v_m;
        return *this;
    }

    std::partial_ordering operator<=>(const Length &o) const { return v_m <=> o.v_m; }

    double pt() const { return mm2pt(mm()); }
    double mm() const { return 1000 * v_m; }
};

inline Length operator*(const double d, const Length l) { return l * d; }
