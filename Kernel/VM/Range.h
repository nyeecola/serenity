/*
 * Copyright (c) 2018-2021, Andreas Kling <kling@serenityos.org>
 * Copyright (c) 2021, Leon Albrecht <leon2002.la@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <Kernel/VirtualAddress.h>

namespace Kernel {

class Range {
    friend class RangeAllocator;

public:
    Range() = delete;
    Range(VirtualAddress base, size_t size)
        : m_base(base)
        , m_size(size)
    {
    }

    VirtualAddress base() const { return m_base; }
    size_t size() const { return m_size; }
    bool is_valid() const { return !m_base.is_null(); }

    bool contains(VirtualAddress vaddr) const { return vaddr >= base() && vaddr < end(); }

    VirtualAddress end() const { return m_base.offset(m_size); }

    bool operator==(const Range& other) const
    {
        return m_base == other.m_base && m_size == other.m_size;
    }

    bool contains(VirtualAddress base, size_t size) const
    {
        if (base.offset(size) < base)
            return false;
        return base >= m_base && base.offset(size) <= end();
    }

    bool contains(const Range& other) const
    {
        return contains(other.base(), other.size());
    }

    Vector<Range, 2> carve(const Range&) const;
    Range intersect(const Range&) const;

    static KResultOr<Range> expand_to_page_boundaries(FlatPtr address, size_t size);

private:
    VirtualAddress m_base;
    size_t m_size { 0 };
};

}

template<>
struct AK::Formatter<Kernel::Range> : Formatter<FormatString> {
    void format(FormatBuilder& builder, Kernel::Range value)
    {
        return Formatter<FormatString>::format(builder, "{} - {} (size {:p})", value.base().as_ptr(), value.base().offset(value.size() - 1).as_ptr(), value.size());
    }
};
