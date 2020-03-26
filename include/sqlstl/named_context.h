#pragma once

#include <string>

namespace sqlstl
{
    class named_context
    {
    public:
        named_context() {}

        named_context(const std::string& name)
            : name_(name)
        {}

        const std::string& get_name() const { return name_; }

        static std::string create_name() { return std::to_string(rand()); }

    private:
        std::string name_;
    };
}
