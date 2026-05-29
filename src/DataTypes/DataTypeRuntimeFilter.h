#pragma once

#include <Columns/ColumnRuntimeFilter.h>
#include <DataTypes/IDataTypeDummy.h>


namespace DB
{

/** The data type of the plan-carried join runtime-filter handle (see `ColumnRuntimeFilter`).
  * Used only as an intermediate inside the `__applyFilter` expression, analogous to `DataTypeSet`
  * for `IN`.
  */
class DataTypeRuntimeFilter final : public IDataTypeDummy
{
public:
    const char * getFamilyName() const override { return "RuntimeFilter"; }

    TypeIndex getTypeId() const override { return TypeIndex::RuntimeFilter; }

    bool equals(const IDataType & rhs) const override { return typeid(rhs) == typeid(*this); }

    bool isParametric() const override { return false; }

    // Used for expressions analysis.
    MutableColumnPtr createColumn() const override { return ColumnRuntimeFilter::create(0, nullptr); }

    // Used only for debugging, making it DUMPABLE
    Field getDefault() const override { return Tuple(); }
};

}
