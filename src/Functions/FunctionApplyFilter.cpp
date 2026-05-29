#include <memory>
#include <Columns/ColumnRuntimeFilter.h>
#include <Columns/ColumnsNumber.h>
#include <DataTypes/DataTypesNumber.h>
#include <Functions/FunctionHelpers.h>
#include <Functions/FunctionFactory.h>
#include <Functions/IFunction.h>
#include <Interpreters/BloomFilter.h>
#include <Processors/QueryPlan/RuntimeFilterLookup.h>
#include <Common/FunctionDocumentation.h>

namespace DB
{

namespace ErrorCodes
{
    extern const int ILLEGAL_TYPE_OF_ARGUMENT;
    extern const int TOO_FEW_ARGUMENTS_FOR_FUNCTION;
}

/// Special function for JOIN runtime filtering
/// Syntax: __applyFilter(handle, key)
/// - handle: a `ColumnRuntimeFilter` carrying the plan-built `FutureRuntimeFilter` (built by
///   `BuildRuntimeFilterStep`); the rendezvous is the handle pointer, mirroring how `IN` reaches
///   its `FutureSet` via a `ColumnSet` argument.
/// - key: Value of any type that is checked to be present in the filter.
/// Returns false if the key should be filtered
class FunctionApplyFilter final : public IFunction
{
public:
    static constexpr auto name = "__applyFilter";
    static FunctionPtr create(ContextPtr) { return std::make_shared<FunctionApplyFilter>(); }

    String getName() const override { return name; }

    bool isVariadic() const override { return false; }
    bool isInjective(const ColumnsWithTypeAndName &) const override { return false; }

    /// A runtime filter's result is not a pure function of its arguments — it depends on the
    /// dynamically built filter, which differs between executions of the same plan (e.g. recursive
    /// CTE iterations or materialized-view blocks). `isDeterministic() == false` keeps it out of
    /// the query condition cache (which keys on the now-deterministic filter expression and would
    /// otherwise serve a stale per-granule result to a later execution with different keys). We
    /// keep `isDeterministicInScopeOfQuery() == true` so the filter can still be pushed into
    /// PREWHERE: within a single read the built filter is fixed, so the predicate is stable there.
    bool isDeterministic() const override { return false; }

    bool isSuitableForConstantFolding() const override { return false; }
    bool isSuitableForShortCircuitArgumentsExecution(const DataTypesWithConstInfo & /*arguments*/) const override { return false; }
    size_t getNumberOfArguments() const override { return 2; }

    DataTypePtr getReturnTypeImpl(const DataTypes & arguments) const override
    {
        if (arguments.size() != 2)
            throw Exception(ErrorCodes::TOO_FEW_ARGUMENTS_FOR_FUNCTION,
                            "Number of arguments for function {} can't be {}, should be 2",
                            getName(), arguments.size());

        if (arguments[0]->getTypeId() != TypeIndex::RuntimeFilter)
            throw Exception(
                    ErrorCodes::ILLEGAL_TYPE_OF_ARGUMENT,
                    "First argument of function '{}' must be a runtime filter handle",
                    getName());

        return std::make_shared<DataTypeUInt8>();
    }

    DataTypePtr getReturnTypeForDefaultImplementationForDynamic() const override
    {
        return std::make_shared<DataTypeUInt8>();
    }

    bool useDefaultImplementationForConstants() const override { return true; }
    bool useDefaultImplementationForNulls() const override { return false; }

    ColumnPtr executeImpl(const ColumnsWithTypeAndName & arguments, const DataTypePtr &, size_t input_rows_count) const override
    {
        const auto * column_runtime_filter = checkAndGetColumnConstData<const ColumnRuntimeFilter>(arguments[0].column.get());
        if (!column_runtime_filter)
            column_runtime_filter = checkAndGetColumn<const ColumnRuntimeFilter>(arguments[0].column.get());
        if (!column_runtime_filter)
            throw Exception(
                    ErrorCodes::ILLEGAL_TYPE_OF_ARGUMENT,
                    "First argument of function '{}' must be a runtime filter handle",
                    getName());

        const auto handle = column_runtime_filter->getData();

        /// If the filter has not been built yet (no build stream ran), all rows pass.
        auto filter = handle ? handle->get() : nullptr;
        if (!filter)
            return DataTypeUInt8().createColumnConst(input_rows_count, true);

        const auto & data_column = arguments[1];

        return filter->find(data_column);
    }
};

REGISTER_FUNCTION(FilterContains)
{
    factory.registerFunction<FunctionApplyFilter>(FunctionDocumentation::INTERNAL_FUNCTION_DOCS, FunctionFactory::Case::Sensitive);
}

}
