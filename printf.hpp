#pragma once
#include <array>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <ostream>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include <type_traits>

namespace sjtu {

using sv_t = std::string_view;

struct format_error : std::exception {
public:
    format_error(const char *msg = "invalid format") : msg(msg) {}
    auto what() const noexcept -> const char * override {
        return msg;
    }

private:
    const char *msg;
};

template <typename Tp>
struct formatter;

struct format_info {
    inline static constexpr auto npos = static_cast<std::size_t>(-1);
    std::size_t position; // where is the specifier
    std::size_t consumed; // how many characters consumed
};

template <typename... Args>
struct format_string {
public:
    // must be constructed at compile time, to ensure the format string is valid
    consteval format_string(const char *fmt);
    constexpr auto get_format() const -> std::string_view {
        return fmt_str;
    }
    constexpr auto get_index() const -> std::span<const format_info> {
        return fmt_idx;
    }

private:
    inline static constexpr auto Nm = sizeof...(Args);
    std::string_view fmt_str;            // the format string
    std::array<format_info, Nm> fmt_idx; // where are the specifiers
};

// Helper function to find next specifier position
constexpr auto find_next_spec_pos(sv_t fmt, std::size_t start) -> std::size_t {
    for (std::size_t i = start; i < fmt.size(); ++i) {
        if (fmt[i] == '%') {
            if (i + 1 >= fmt.size()) {
                throw format_error{"missing specifier after '%'"};
            }
            if (fmt[i + 1] != '%') {
                return i;
            }
            // Skip escaped %%
            ++i;
        }
    }
    return sv_t::npos;
}

template <typename... Args>
consteval auto compile_time_format_check(sv_t fmt_str, std::span<format_info> idx) -> void {
    std::size_t arg_idx = 0;
    std::size_t search_pos = 0;
    
    auto check_arg = [&]<typename T>() {
        std::size_t spec_pos = find_next_spec_pos(fmt_str, search_pos);
        
        if (spec_pos == sv_t::npos) {
            idx[arg_idx] = {
                .position = format_info::npos,
                .consumed = 0,
            };
            ++arg_idx;
            return;
        }
        
        // Parse the specifier
        sv_t spec_view = fmt_str.substr(spec_pos + 1);
        formatter<T> f{};
        std::size_t consumed = f.parse(spec_view);
        
        idx[arg_idx] = {
            .position = spec_pos,
            .consumed = consumed,
        };
        ++arg_idx;
        
        // Move search position past this specifier
        if (consumed > 0) {
            search_pos = spec_pos + 1 + consumed;
        } else if (spec_view.starts_with("_")) {
            search_pos = spec_pos + 2;
        } else {
            throw format_error{"invalid specifier"};
        }
    };
    
    (check_arg.template operator()<Args>(), ...);
    
    // Check for extra specifiers
    if (find_next_spec_pos(fmt_str, search_pos) != sv_t::npos) {
        throw format_error{"too many specifiers"};
    }
}

template <typename... Args>
consteval format_string<Args...>::format_string(const char *fmt) :
    fmt_str(fmt), fmt_idx() {
    compile_time_format_check<Args...>(fmt_str, fmt_idx);
}

// Formatter for string-like types
template <typename StrLike>
    requires(
        std::same_as<StrLike, std::string> ||      //
        std::same_as<StrLike, std::string_view> || //
        std::same_as<StrLike, char *> ||           //
        std::same_as<StrLike, const char *>        //
    )
struct formatter<StrLike> {
    static constexpr auto parse(sv_t fmt) -> std::size_t {
        return fmt.starts_with("s") ? 1 : 0;
    }
    static auto format_to(std::ostream &os, auto val, sv_t fmt) -> void {
        if (fmt.starts_with("s") || fmt.starts_with("_")) {
            os << static_cast<sv_t>(val);
        } else {
            throw format_error{"invalid format for string"};
        }
    }
};

// Formatter for signed integer types
template <typename IntType>
    requires(std::is_integral_v<IntType> && std::is_signed_v<IntType>)
struct formatter<IntType> {
    static constexpr auto parse(sv_t fmt) -> std::size_t {
        if (fmt.starts_with("d")) return 1;
        if (fmt.starts_with("u")) return 1;
        return 0; // %_ case
    }
    static auto format_to(std::ostream &os, const IntType &val, sv_t fmt) -> void {
        if (fmt.starts_with("d") || fmt.starts_with("_")) {
            os << static_cast<int64_t>(val);
        } else if (fmt.starts_with("u")) {
            os << static_cast<uint64_t>(val);
        } else {
            throw format_error{"invalid format for signed integer"};
        }
    }
};

// Formatter for unsigned integer types
template <typename IntType>
    requires(std::is_integral_v<IntType> && std::is_unsigned_v<IntType>)
struct formatter<IntType> {
    static constexpr auto parse(sv_t fmt) -> std::size_t {
        if (fmt.starts_with("d")) return 1;
        if (fmt.starts_with("u")) return 1;
        return 0; // %_ case
    }
    static auto format_to(std::ostream &os, const IntType &val, sv_t fmt) -> void {
        if (fmt.starts_with("u") || fmt.starts_with("_")) {
            os << static_cast<uint64_t>(val);
        } else if (fmt.starts_with("d")) {
            os << static_cast<int64_t>(val);
        } else {
            throw format_error{"invalid format for unsigned integer"};
        }
    }
};

// Formatter for vector types
template <typename T>
struct formatter<std::vector<T>> {
    static constexpr auto parse(sv_t fmt) -> std::size_t {
        return 0; // only %_ is supported
    }
    static auto format_to(std::ostream &os, const std::vector<T> &val, sv_t fmt) -> void {
        if (!fmt.starts_with("_") && !fmt.empty()) {
            throw format_error{"vector only supports %_"};
        }
        os << "[";
        for (size_t i = 0; i < val.size(); ++i) {
            if (i > 0) os << ",";
            formatter<T>::format_to(os, val[i], "_");
        }
        os << "]";
    }
};

template <typename... Args>
using format_string_t = format_string<std::decay_t<Args>...>;

template <typename... Args>
inline auto printf(format_string_t<Args...> fmt, const Args &...args) -> void {
    auto fmt_str = fmt.get_format();
    auto fmt_idx = fmt.get_index();
    
    std::size_t current_pos = 0;
    std::size_t arg_idx = 0;
    
    auto print_arg = [&]<typename T>(const T &arg) {
        const auto &info = fmt_idx[arg_idx++];
        
        if (info.position == format_info::npos) {
            // No specifier for this argument - shouldn't happen if format check is correct
            return;
        }
        
        // Print everything before the specifier
        while (current_pos < info.position) {
            if (fmt_str[current_pos] == '%' && current_pos + 1 < fmt_str.size() && fmt_str[current_pos + 1] == '%') {
                std::cout << '%';
                current_pos += 2;
            } else {
                std::cout << fmt_str[current_pos];
                current_pos++;
            }
        }
        
        // Get the format specifier
        std::size_t spec_len = info.consumed > 0 ? info.consumed : 1;
        sv_t spec_fmt = fmt_str.substr(info.position + 1, spec_len);
        
        // Format the argument
        formatter<std::decay_t<T>>::format_to(std::cout, arg, spec_fmt);
        
        // Move past the specifier
        current_pos = info.position + 1 + spec_len;
    };
    
    (print_arg(args), ...);
    
    // Print remaining format string
    while (current_pos < fmt_str.size()) {
        if (fmt_str[current_pos] == '%' && current_pos + 1 < fmt_str.size() && fmt_str[current_pos + 1] == '%') {
            std::cout << '%';
            current_pos += 2;
        } else {
            std::cout << fmt_str[current_pos];
            current_pos++;
        }
    }
}

} // namespace sjtu
