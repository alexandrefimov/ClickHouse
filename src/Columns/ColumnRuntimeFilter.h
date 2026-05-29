#pragma once

#include <Columns/IColumnDummy.h>
#include <Core/Field.h>
#include <Processors/QueryPlan/RuntimeFilterLookup.h>
#include <Common/SipHash.h>

#include <string>


namespace DB
{

/** A column carrying the handle of a join runtime filter (see `FutureRuntimeFilter`).
  * Mirrors `ColumnSet`: it behaves like a constant dummy column and holds the plan-carried handle
  * that the `__applyFilter` function reads at execution time.
  */
class ColumnRuntimeFilter final : public COWHelper<IColumnDummy, ColumnRuntimeFilter>
{
private:
    friend class COWHelper<IColumnDummy, ColumnRuntimeFilter>;

    ColumnRuntimeFilter(size_t size_, FutureRuntimeFilterPtr data_)
        : data(std::move(data_))
    {
        s = size_;
    }

    ColumnRuntimeFilter(const ColumnRuntimeFilter &) = default;

public:
    const char * getFamilyName() const override { return "RuntimeFilter"; }

    TypeIndex getDataType() const override { return TypeIndex::RuntimeFilter; }

    MutableColumnPtr cloneDummy(size_t size_) const override { return ColumnRuntimeFilter::create(size_, data); }

    FutureRuntimeFilterPtr getData() const { return data; }

    void setData(FutureRuntimeFilterPtr data_) { data = std::move(data_); }

    String getName() const override
    {
        return "_runtime_filter_" + (data ? std::to_string(data->getStructuralHash()) : String("null"));
    }

    void updateHashWithValue(size_t /*n*/, SipHash & hash) const override
    {
        hash.update(data ? data->getStructuralHash() : 0);
    }

    // Used only for debugging, making it DUMPABLE
    Field operator[](size_t) const override { return {}; }

private:
    FutureRuntimeFilterPtr data;
};

}
