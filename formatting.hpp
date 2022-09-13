#pragma once

#include <wordhyphenator.hpp>

#include <cstdio>
#include <cstdint>
#include <cstdlib>

const char ITALIC_S = 1;
const char BOLD_S = (1 << 1);
const char TT_S = (1 << 2);
const char SMALLCAPS_S = (1 << 3);

const uint32_t italic_character = '/';
const uint32_t bold_character = '*';
const uint32_t tt_character = '`';
const uint32_t smallcaps_character = '|';

const uint32_t italic_codepoint = '/';
const uint32_t bold_codepoint = '*';
const uint32_t tt_codepoint = '`';
const uint32_t smallcaps_codepoint = '|';
#if 0
const uint32_t superscript_char = '^';
const uint32_t subscript_char = '_';
#endif

template<typename T, int max_elements> class SmallStack final {

public:
    typedef T value_type;

    SmallStack() = default;

    bool empty() const { return size == 0; }

    bool contains(T val) const {
        for(int i = 0; i < size; ++i) {
            if(arr[i] == val) {
                return true;
            }
        }
        return false;
    }

    void push(T new_val) {
        if(contains(new_val)) {
            printf("Tried to push an element that is already in the stack.\n");
            std::abort();
        }
        if(size >= max_elements) {
            printf("Stack overflow.\n");
            std::abort();
        }
        arr[size] = new_val;
        ++size;
    }

    void pop(T new_val) {
        if(empty()) {
            printf("Tried to pop an empty stack.\n");
            std::abort();
        }
        if(arr[size - 1] != new_val) {
            printf("Tried to pop a different value than is at the end of the stack.\n");
            std::abort();
        }
        --size;
    }

    bool operator==(const SmallStack<T, max_elements> &other) const {
        if(size != other.size) {
            return false;
        }
        for(int i = 0; i < size; ++i) {
            if(arr[i] != other.arr[i]) {
                return false;
            }
        }
        return true;
    }

    void write_buildup_markup(std::string &buf) const {
        for(int i = 0; i < size; ++i) {
            switch(arr[i]) {
            case ITALIC_S:
                buf += "<i>";
                break;
            case BOLD_S:
                buf += "<b>";
                break;
            case TT_S:
                buf += "<tt>";
                break;
            case SMALLCAPS_S:
                buf += "<span variant=\"small-caps\" letter_spacing=\"100\">";
                break;
            default:
                std::abort();
            }
        }
    }

    void write_teardown_markup(std::string &buf) const {

        for(int i = size - 1; i >= 0; --i) {
            switch(arr[i]) {
            case ITALIC_S:
                buf += "</i>";
                break;
            case BOLD_S:
                buf += "</b>";
                break;
            case TT_S:
                buf += "</tt>";
                break;
            case SMALLCAPS_S:
                buf += "</span>";
                break;
            default:
                std::abort();
            }
        }
    }

    const T *cbegin() const { return arr; }
    const T *cend() const { return arr + size; }

    const T *crbegin() const { return arr + size - 1; } // FIXME, need to use -- to progress.
    const T *crend() const { return arr - 1; }

private:
    T arr[max_elements];
    int size = 0;
};

typedef SmallStack<char, 4> StyleStack;

struct FormattingChange {
    size_t offset;
    char format;
};

struct EnrichedWord {
    std::string text;
    std::vector<HyphenPoint> hyphen_points;
    std::vector<FormattingChange> f;
    StyleStack start_style;
};
