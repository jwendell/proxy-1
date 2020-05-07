#include "eval/eval/const_value_step.h"
#include "eval/eval/expression_step_base.h"
#include "google/protobuf/duration.pb.h"
#include "google/protobuf/timestamp.pb.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

using google::api::expr::v1alpha1::Constant;
using google::api::expr::v1alpha1::Expr;

namespace {

class ConstValueStep : public ExpressionStepBase {
 public:
  ConstValueStep(const CelValue& value, int64_t expr_id, bool comes_from_ast)
      : ExpressionStepBase(expr_id, comes_from_ast), value_(value) {}

  cel_base::Status Evaluate(ExecutionFrame* context) const override;

 private:
  CelValue value_;
};

cel_base::Status ConstValueStep::Evaluate(ExecutionFrame* frame) const {
  frame->value_stack().Push(value_);

  return cel_base::OkStatus();
}

}  // namespace

absl::optional<CelValue> ConvertConstant(const Constant* const_expr) {
  CelValue value = CelValue::CreateNull();
  switch (const_expr->constant_kind_case()) {
    case Constant::kNullValue:
      value = CelValue::CreateNull();
      break;
    case Constant::kBoolValue:
      value = CelValue::CreateBool(const_expr->bool_value());
      break;
    case Constant::kInt64Value:
      value = CelValue::CreateInt64(const_expr->int64_value());
      break;
    case Constant::kUint64Value:
      value = CelValue::CreateUint64(const_expr->uint64_value());
      break;
    case Constant::kDoubleValue:
      value = CelValue::CreateDouble(const_expr->double_value());
      break;
    case Constant::kStringValue:
      value = CelValue::CreateString(&const_expr->string_value());
      break;
    case Constant::kBytesValue:
      value = CelValue::CreateBytes(&const_expr->bytes_value());
      break;
    case Constant::kDurationValue:
      value = CelValue::CreateDuration(&const_expr->duration_value());
      break;
    case Constant::kTimestampValue:
      value = CelValue::CreateTimestamp(&const_expr->timestamp_value());
      break;
    default:
      // constant with no kind specified
      return {};
      break;
  }
  return value;
}

cel_base::StatusOr<std::unique_ptr<ExpressionStep>> CreateConstValueStep(
    CelValue value, int64_t expr_id, bool comes_from_ast) {
  std::unique_ptr<ExpressionStep> step =
      absl::make_unique<ConstValueStep>(value, expr_id, comes_from_ast);
  return std::move(step);
}

// Factory method for Constant(Enum value) - based Execution step
cel_base::StatusOr<std::unique_ptr<ExpressionStep>> CreateConstValueStep(
    const google::protobuf::EnumValueDescriptor* value_descriptor, int64_t expr_id) {
  CelValue value = CelValue::CreateInt64(value_descriptor->number());

  std::unique_ptr<ExpressionStep> step =
      absl::make_unique<ConstValueStep>(value, expr_id, false);
  return std::move(step);
}

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
