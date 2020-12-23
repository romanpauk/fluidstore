#pragma once

namespace crdt
{
    class noncopyable
    {
    public:
        noncopyable() {}

        noncopyable(const noncopyable&) = delete;
        noncopyable& operator = (const noncopyable&) = delete;

        noncopyable(noncopyable&&) = delete;
        noncopyable& operator = (noncopyable&&) = delete;
    };
}
