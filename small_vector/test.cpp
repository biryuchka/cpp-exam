#define CATCH_CONFIG_MAIN
#include "Catch2/single_include/catch2/catch.hpp"

#include <vector>
#include <stdexcept>
#include <utility>
#include <string>

#include "small_vector.hpp"

template <typename T, size_t N = 8>
using Container = SmallVector<T, N>;

// template <typename T, size_t N = 8>
// using Container = std::vector<T>;

// ==================== Вспомогательные типы ====================

struct ThrowOnCopy {
    int value;

    explicit ThrowOnCopy(int v = 0) : value(v) {}
    ThrowOnCopy(const ThrowOnCopy&) { throw std::runtime_error("copy"); }
    ThrowOnCopy(ThrowOnCopy&& other) noexcept : value(other.value) {}
    ThrowOnCopy& operator=(const ThrowOnCopy&) { throw std::runtime_error("copy assign"); }
    ThrowOnCopy& operator=(ThrowOnCopy&& other) noexcept {
        value = other.value;
        return *this;
    }
};

struct ThrowOnMoveAndCopy {
    int value;
    static int throw_after;
    static int counter;

    explicit ThrowOnMoveAndCopy(int v = 0) : value(v) {}

    ThrowOnMoveAndCopy(const ThrowOnMoveAndCopy& other) : value(other.value) {
        if (++counter >= throw_after) {
            throw std::runtime_error("copy");
        }
    }

    ThrowOnMoveAndCopy(ThrowOnMoveAndCopy&& other) : value(other.value) {
        if (++counter >= throw_after) {
            throw std::runtime_error("move");
        }
    }

    ThrowOnMoveAndCopy& operator=(const ThrowOnMoveAndCopy& other) {
        if (++counter >= throw_after) {
            throw std::runtime_error("copy assign");
        }
        value = other.value;
        return *this;
    }

    ThrowOnMoveAndCopy& operator=(ThrowOnMoveAndCopy&& other) {
        if (++counter >= throw_after) {
            throw std::runtime_error("move assign");
        }
        value = other.value;
        return *this;
    }

    static void reset(int throw_at = 1000000) {
        counter = 0;
        throw_after = throw_at;
    }
};

int ThrowOnMoveAndCopy::throw_after = 1000000;
int ThrowOnMoveAndCopy::counter = 0;

struct NonTrivial {
    std::string data;
    NonTrivial() : data("default") {}
    explicit NonTrivial(const std::string& s) : data(s) {}
    bool operator==(const NonTrivial& other) const { return data == other.data; }
};

struct MoveOnly {
    int value;
    explicit MoveOnly(int v = 0) : value(v) {}
    MoveOnly(const MoveOnly&) = delete;
    MoveOnly& operator=(const MoveOnly&) = delete;
    MoveOnly(MoveOnly&& other) noexcept : value(other.value) { other.value = -1; }
    MoveOnly& operator=(MoveOnly&& other) noexcept {
        value = other.value;
        other.value = -1;
        return *this;
    }
};

// ==================== Тесты конструкторов ====================

TEST_CASE("Default constructor", "[constructor]") {
    Container<int> v;
    REQUIRE(v.size() == 0);
    REQUIRE(v.begin() == v.end());
}

TEST_CASE("Size constructor", "[constructor]") {
    SECTION("zero size") {
        Container<int> v(0);
        REQUIRE(v.size() == 0);
    }

    SECTION("small size (fits in stack buffer)") {
        Container<int, 8> v(5);
        REQUIRE(v.size() == 5);
        for (size_t i = 0; i < v.size(); ++i) {
            REQUIRE(v[i] == 0);
        }
    }

    SECTION("large size (overflows to heap)") {
        Container<int, 4> v(100);
        REQUIRE(v.size() == 100);
        for (size_t i = 0; i < v.size(); ++i) {
            REQUIRE(v[i] == 0);
        }
    }

    SECTION("with non-trivial type") {
        Container<NonTrivial, 4> v(3);
        REQUIRE(v.size() == 3);
        for (size_t i = 0; i < v.size(); ++i) {
            REQUIRE(v[i].data == "default");
        }
    }
}

TEST_CASE("Size and value constructor", "[constructor]") {
    SECTION("with int") {
        Container<int, 4> v(6, 42);
        REQUIRE(v.size() == 6);
        for (size_t i = 0; i < v.size(); ++i) {
            REQUIRE(v[i] == 42);
        }
    }

    SECTION("fits in stack") {
        Container<int, 8> v(3, 7);
        REQUIRE(v.size() == 3);
        for (size_t i = 0; i < v.size(); ++i) {
            REQUIRE(v[i] == 7);
        }
    }

    SECTION("with non-trivial type") {
        NonTrivial val("hello");
        Container<NonTrivial, 4> v(5, val);
        REQUIRE(v.size() == 5);
        for (size_t i = 0; i < v.size(); ++i) {
            REQUIRE(v[i].data == "hello");
        }
    }
}

TEST_CASE("Copy constructor", "[constructor]") {
    SECTION("empty vector") {
        Container<int> src;
        Container<int> dst(src);
        REQUIRE(dst.size() == 0);
    }

    SECTION("stack-allocated data") {
        Container<int, 8> src(5, 42);
        Container<int, 8> dst(src);
        REQUIRE(dst.size() == 5);
        for (size_t i = 0; i < dst.size(); ++i) {
            REQUIRE(dst[i] == 42);
        }
    }

    SECTION("heap-allocated data") {
        Container<int, 4> src(20, 7);
        Container<int, 4> dst(src);
        REQUIRE(dst.size() == 20);
        for (size_t i = 0; i < dst.size(); ++i) {
            REQUIRE(dst[i] == 7);
        }
    }

    SECTION("independence of copy") {
        Container<int, 4> src;
        src.push_back(1);
        src.push_back(2);
        Container<int, 4> dst(src);
        src.push_back(3);
        REQUIRE(dst.size() == 2);
        REQUIRE(src.size() == 3);
    }
}

TEST_CASE("Move constructor", "[constructor][move]") {
    SECTION("stack-allocated data") {
        Container<int, 8> src;
        for (int i = 0; i < 5; ++i) src.push_back(i);
        size_t old_size = src.size();
        Container<int, 8> dst(std::move(src));
        REQUIRE(dst.size() == old_size);
        for (int i = 0; i < 5; ++i) {
            REQUIRE(dst[i] == i);
        }
    }

    SECTION("heap-allocated data") {
        Container<int, 4> src;
        for (int i = 0; i < 20; ++i) src.push_back(i);
        Container<int, 4> dst(std::move(src));
        REQUIRE(dst.size() == 20);
        for (int i = 0; i < 20; ++i) {
            REQUIRE(dst[i] == i);
        }
    }

    SECTION("with move-only type") {
        Container<MoveOnly, 4> src;
        src.push_back(MoveOnly(1));
        src.push_back(MoveOnly(2));
        Container<MoveOnly, 4> dst(std::move(src));
        REQUIRE(dst.size() == 2);
        REQUIRE(dst[0].value == 1);
        REQUIRE(dst[1].value == 2);
    }
}

// ==================== Тесты операторов ====================

TEST_CASE("Copy assignment operator", "[operator][copy]") {
    SECTION("basic copy") {
        Container<int, 4> src(5, 10);
        Container<int, 4> dst;
        dst = src;
        REQUIRE(dst.size() == 5);
        for (size_t i = 0; i < dst.size(); ++i) {
            REQUIRE(dst[i] == 10);
        }
    }

    SECTION("overwrite existing data") {
        Container<int, 4> src(3, 42);
        Container<int, 4> dst(10, 0);
        dst = src;
        REQUIRE(dst.size() == 3);
        for (size_t i = 0; i < dst.size(); ++i) {
            REQUIRE(dst[i] == 42);
        }
    }

    SECTION("self-assignment") {
        Container<int, 4> v(5, 7);
        v = v;
        REQUIRE(v.size() == 5);
        for (size_t i = 0; i < v.size(); ++i) {
            REQUIRE(v[i] == 7);
        }
    }
}

TEST_CASE("Move assignment operator", "[operator][move]") {
    SECTION("basic move") {
        Container<int, 4> src;
        for (int i = 0; i < 5; ++i) src.push_back(i * 10);
        Container<int, 4> dst;
        dst = std::move(src);
        REQUIRE(dst.size() == 5);
        for (int i = 0; i < 5; ++i) {
            REQUIRE(dst[i] == i * 10);
        }
    }

    SECTION("move from heap to replace stack") {
        Container<int, 4> src;
        for (int i = 0; i < 20; ++i) src.push_back(i);
        Container<int, 4> dst(2, 99);
        dst = std::move(src);
        REQUIRE(dst.size() == 20);
        for (int i = 0; i < 20; ++i) {
            REQUIRE(dst[i] == i);
        }
    }

    SECTION("with move-only type") {
        Container<MoveOnly, 4> src;
        src.push_back(MoveOnly(42));
        Container<MoveOnly, 4> dst;
        dst = std::move(src);
        REQUIRE(dst.size() == 1);
        REQUIRE(dst[0].value == 42);
    }
}

TEST_CASE("Subscript operator", "[operator]") {
    Container<int, 4> v;
    v.push_back(10);
    v.push_back(20);
    v.push_back(30);

    SECTION("read access") {
        REQUIRE(v[0] == 10);
        REQUIRE(v[1] == 20);
        REQUIRE(v[2] == 30);
    }

    SECTION("write access") {
        v[1] = 99;
        REQUIRE(v[1] == 99);
    }

    SECTION("const access") {
        const auto& cv = v;
        REQUIRE(cv[0] == 10);
    }
}

// ==================== Тесты методов ====================

TEST_CASE("size", "[method]") {
    Container<int, 4> v;
    REQUIRE(v.size() == 0);
    v.push_back(1);
    REQUIRE(v.size() == 1);
    v.push_back(2);
    REQUIRE(v.size() == 2);
    v.pop_back();
    REQUIRE(v.size() == 1);
}

TEST_CASE("reserve", "[method]") {
    SECTION("reserve less than current — no change") {
        Container<int, 8> v(5, 1);
        v.reserve(2);
        REQUIRE(v.size() == 5);
        for (size_t i = 0; i < v.size(); ++i) {
            REQUIRE(v[i] == 1);
        }
    }

    SECTION("reserve triggers move to heap") {
        Container<int, 4> v;
        v.push_back(1);
        v.push_back(2);
        v.reserve(100);
        REQUIRE(v.size() == 2);
        REQUIRE(v[0] == 1);
        REQUIRE(v[1] == 2);
    }

    SECTION("data preserved after reserve") {
        Container<NonTrivial, 4> v;
        v.push_back(NonTrivial("a"));
        v.push_back(NonTrivial("b"));
        v.reserve(50);
        REQUIRE(v.size() == 2);
        REQUIRE(v[0].data == "a");
        REQUIRE(v[1].data == "b");
    }
}

TEST_CASE("resize", "[method]") {
    SECTION("resize to larger fills with default") {
        Container<int, 4> v(2, 5);
        v.resize(6);
        REQUIRE(v.size() == 6);
        REQUIRE(v[0] == 5);
        REQUIRE(v[1] == 5);
        for (size_t i = 2; i < 6; ++i) {
            REQUIRE(v[i] == 0);
        }
    }

    SECTION("resize to smaller truncates") {
        Container<int, 4> v(10, 3);
        v.resize(2);
        REQUIRE(v.size() == 2);
        REQUIRE(v[0] == 3);
        REQUIRE(v[1] == 3);
    }

    SECTION("resize to zero") {
        Container<int, 4> v(5, 1);
        v.resize(0);
        REQUIRE(v.size() == 0);
    }

    SECTION("resize with non-trivial type") {
        Container<NonTrivial, 4> v;
        v.push_back(NonTrivial("hello"));
        v.resize(3);
        REQUIRE(v.size() == 3);
        REQUIRE(v[0].data == "hello");
        REQUIRE(v[1].data == "default");
        REQUIRE(v[2].data == "default");
    }
}

TEST_CASE("push_back const ref", "[method]") {
    SECTION("within stack buffer") {
        Container<int, 8> v;
        for (int i = 0; i < 8; ++i) {
            v.push_back(i);
        }
        REQUIRE(v.size() == 8);
        for (int i = 0; i < 8; ++i) {
            REQUIRE(v[i] == i);
        }
    }

    SECTION("overflow to heap") {
        Container<int, 4> v;
        for (int i = 0; i < 20; ++i) {
            v.push_back(i);
        }
        REQUIRE(v.size() == 20);
        for (int i = 0; i < 20; ++i) {
            REQUIRE(v[i] == i);
        }
    }

    SECTION("with non-trivial type") {
        Container<NonTrivial, 2> v;
        NonTrivial val("test");
        v.push_back(val);
        v.push_back(val);
        v.push_back(val);
        REQUIRE(v.size() == 3);
        for (size_t i = 0; i < v.size(); ++i) {
            REQUIRE(v[i].data == "test");
        }
    }
}

TEST_CASE("push_back rvalue ref", "[method][move]") {
    SECTION("move-only type") {
        Container<MoveOnly, 4> v;
        v.push_back(MoveOnly(1));
        v.push_back(MoveOnly(2));
        v.push_back(MoveOnly(3));
        REQUIRE(v.size() == 3);
        REQUIRE(v[0].value == 1);
        REQUIRE(v[1].value == 2);
        REQUIRE(v[2].value == 3);
    }

    SECTION("move semantics used") {
        Container<std::string, 4> v;
        std::string s = "hello world this is a long string to avoid SSO";
        v.push_back(std::move(s));
        REQUIRE(v[0] == "hello world this is a long string to avoid SSO");
        // s was moved from — should be empty or in valid moved-from state
    }
}

TEST_CASE("emplace_back", "[method][move]") {
    SECTION("construct in-place") {
        Container<NonTrivial, 4> v;
        v.emplace_back("constructed");
        REQUIRE(v.size() == 1);
        REQUIRE(v[0].data == "constructed");
    }

    SECTION("with move-only type") {
        Container<MoveOnly, 4> v;
        v.emplace_back(42);
        REQUIRE(v.size() == 1);
        REQUIRE(v[0].value == 42);
    }

    SECTION("multiple emplace_back") {
        Container<std::pair<int, std::string>, 4> v;
        v.emplace_back(1, "one");
        v.emplace_back(2, "two");
        REQUIRE(v.size() == 2);
        REQUIRE(v[0].first == 1);
        REQUIRE(v[0].second == "one");
        REQUIRE(v[1].first == 2);
        REQUIRE(v[1].second == "two");
    }

    SECTION("overflow to heap with emplace_back") {
        Container<int, 2> v;
        for (int i = 0; i < 10; ++i) {
            v.emplace_back(i * 100);
        }
        REQUIRE(v.size() == 10);
        for (int i = 0; i < 10; ++i) {
            REQUIRE(v[i] == i * 100);
        }
    }
}

TEST_CASE("pop_back", "[method]") {
    Container<int, 4> v;
    v.push_back(1);
    v.push_back(2);
    v.push_back(3);

    v.pop_back();
    REQUIRE(v.size() == 2);
    REQUIRE(v[0] == 1);
    REQUIRE(v[1] == 2);

    v.pop_back();
    v.pop_back();
    REQUIRE(v.size() == 0);
}

// ==================== Тесты итераторов ====================

TEST_CASE("Iterators", "[iterator]") {
    Container<int, 4> v;
    for (int i = 0; i < 5; ++i) v.push_back(i * 10);

    SECTION("begin/end") {
        int expected = 0;
        for (auto it = v.begin(); it != v.end(); ++it) {
            REQUIRE(*it == expected);
            expected += 10;
        }
        REQUIRE(expected == 50);
    }

    SECTION("cbegin/cend") {
        int expected = 0;
        for (auto it = v.cbegin(); it != v.cend(); ++it) {
            REQUIRE(*it == expected);
            expected += 10;
        }
        REQUIRE(expected == 50);
    }

    SECTION("range-based for loop") {
        int sum = 0;
        for (const auto& x : v) {
            sum += x;
        }
        REQUIRE(sum == 100);
    }

    SECTION("modify through iterator") {
        for (auto it = v.begin(); it != v.end(); ++it) {
            *it += 1;
        }
        REQUIRE(v[0] == 1);
        REQUIRE(v[4] == 41);
    }

    SECTION("empty container") {
        Container<int, 4> empty;
        REQUIRE(empty.begin() == empty.end());
        REQUIRE(empty.cbegin() == empty.cend());
    }
}

// ==================== Тесты перехода stack -> heap ====================

TEST_CASE("Stack to heap transition", "[transition]") {
    Container<int, 4> v;

    SECTION("data preserved across transition") {
        v.push_back(1);
        v.push_back(2);
        v.push_back(3);
        v.push_back(4);
        // This push should trigger transition to heap
        v.push_back(5);
        REQUIRE(v.size() == 5);
        REQUIRE(v[0] == 1);
        REQUIRE(v[1] == 2);
        REQUIRE(v[2] == 3);
        REQUIRE(v[3] == 4);
        REQUIRE(v[4] == 5);
    }

    SECTION("many elements after transition") {
        for (int i = 0; i < 1000; ++i) {
            v.push_back(i);
        }
        REQUIRE(v.size() == 1000);
        for (int i = 0; i < 1000; ++i) {
            REQUIRE(v[i] == i);
        }
    }

    SECTION("non-trivial type across transition") {
        Container<NonTrivial, 2> sv;
        sv.push_back(NonTrivial("a"));
        sv.push_back(NonTrivial("b"));
        sv.push_back(NonTrivial("c"));
        REQUIRE(sv.size() == 3);
        REQUIRE(sv[0].data == "a");
        REQUIRE(sv[1].data == "b");
        REQUIRE(sv[2].data == "c");
    }
}

// ==================== Тесты exception safety ====================

TEST_CASE("Exception safety: push_back strong guarantee", "[exception]") {
    SECTION("exception during reallocation copy preserves state") {
        ThrowOnMoveAndCopy::reset(1000000);
        Container<ThrowOnMoveAndCopy, 2> v;
        v.emplace_back(1);
        v.emplace_back(2);

        size_t old_size = v.size();
        int old_val0 = v[0].value;
        int old_val1 = v[1].value;

        // Next push_back will require reallocation, set throw on 3rd copy/move
        ThrowOnMoveAndCopy::reset(3);
        REQUIRE_THROWS(v.push_back(ThrowOnMoveAndCopy(3)));

        // Strong guarantee: state must be unchanged
        REQUIRE(v.size() == old_size);
        REQUIRE(v[0].value == old_val0);
        REQUIRE(v[1].value == old_val1);
    }
}

TEST_CASE("Exception safety: reserve strong guarantee", "[exception]") {
    ThrowOnMoveAndCopy::reset(1000000);
    Container<ThrowOnMoveAndCopy, 4> v;
    v.emplace_back(10);
    v.emplace_back(20);

    size_t old_size = v.size();

    // Throw during reallocation in reserve
    ThrowOnMoveAndCopy::reset(2);
    REQUIRE_THROWS(v.reserve(100));

    REQUIRE(v.size() == old_size);
    REQUIRE(v[0].value == 10);
    REQUIRE(v[1].value == 20);
}

TEST_CASE("Exception safety: resize strong guarantee", "[exception]") {
    ThrowOnMoveAndCopy::reset(1000000);
    Container<ThrowOnMoveAndCopy, 4> v;
    v.emplace_back(1);
    v.emplace_back(2);

    size_t old_size = v.size();

    // Throw during resize that requires reallocation
    ThrowOnMoveAndCopy::reset(2);
    REQUIRE_THROWS(v.resize(100));

    REQUIRE(v.size() == old_size);
    REQUIRE(v[0].value == 1);
    REQUIRE(v[1].value == 2);
}

TEST_CASE("Exception safety: no leaks on constructor failure", "[exception]") {
    ThrowOnMoveAndCopy::reset(3);
    ThrowOnMoveAndCopy val(0);
    REQUIRE_THROWS(Container<ThrowOnMoveAndCopy, 4>(5, val));
    // If no leak, this test simply shouldn't crash or leak (run under sanitizer)
}

// ==================== Тест move_if_noexcept ====================

TEST_CASE("move_if_noexcept: noexcept move uses move", "[move][noexcept]") {
    Container<ThrowOnCopy, 4> v;
    // ThrowOnCopy throws on copy, but has noexcept move
    // If implementation uses move_if_noexcept, it should use move (noexcept) and succeed
    v.push_back(ThrowOnCopy(1));
    v.push_back(ThrowOnCopy(2));
    v.push_back(ThrowOnCopy(3));
    v.push_back(ThrowOnCopy(4));

    // This triggers reallocation — must not throw because move is noexcept
    REQUIRE_NOTHROW(v.push_back(ThrowOnCopy(5)));
    REQUIRE(v.size() == 5);
    REQUIRE(v[4].value == 5);
}
