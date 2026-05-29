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
  *
  * Its name and value-hash are derived from the handle's *structural* fingerprint, not from the
  * handle pointer, so two independently built plans of the same query (as in
  * `considerEnablingParallelReplicas`) produce identical column hashes — which is what lets the
  * plan cache keys match without any runtime-filter-specific special-casing in plan hashing.
  */
class ColumnRuntimeFilter final : public COWHelper<IColumnDummy, ColumnRuntimeFilter>
{
private:
    friend class COWHelper<IColumnDummy, ColumnRuntimeFilter>;

    ColumnRuntimeFilter(size_t s_, FutureRuntimeFilterPtr data_) : data(std::move(data_)) { s = s_; }
    ColumnRuntimeFilter(const ColumnRuntimeFilter &) = default;

public:
    const char * getFamilyName() const override { return "RuntimeFilter"; }
    TypeIndex getDataType() const override { return TypeIndex::RuntimeFilter; }
    MutableColumnPtr cloneDummy(size_t s_) const override { return ColumnRuntimeFilter::create(s_, data); }

    FutureRuntimeFilterPtr getData() const { return data; }
    void setData(FutureRuntimeFilterPtr data_) { data = std::move(data_); }

    /// Deterministic identity: the handle's structural fingerprint, never the pointer. Uses the
    /// `_runtime_filter_<id>` shape so the EXPLAIN-normalizing tests can canonicalize it.
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
