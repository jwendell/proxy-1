#ifndef THIRD_PARTY_CEL_CPP_EVAL_EVAL_JUMP_STEP_H_
#define THIRD_PARTY_CEL_CPP_EVAL_EVAL_JUMP_STEP_H_

#include "eval/eval/evaluator_core.h"
#include "eval/eval/expression_step_base.h"
#include "eval/public/activation.h"
#include "eval/public/cel_value.h"
#include "google/api/expr/v1alpha1/syntax.pb.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

class JumpStepBase : public ExpressionStepBase {
 public:
  JumpStepBase(absl::optional<int> jump_offset, int64_t expr_id)
      : ExpressionStepBase(expr_id, false), jump_offset_(jump_offset) {}

  void set_jump_offset(int offset) { jump_offset_ = offset; }

  cel_base::Status Jump(ExecutionFrame* frame) const {
    if (!jump_offset_.has_value()) {
      return cel_base::Status(cel_base::StatusCode::kInternal, "Jump offset not set");
    }
    return frame->JumpTo(jump_offset_.value());
  }

 private:
  absl::optional<int> jump_offset_;
};

// Factory method for Jump step.
cel_base::StatusOr<std::unique_ptr<JumpStepBase>> CreateJumpStep(
    absl::optional<int> jump_offset, int64_t expr_id);

// Factory method for Conditional Jump step.
// Conditional Jump requires a boolean value to sit on the stack.
// It is compared to jump_condition, and if matched, jump is performed.
// leave on stack indicates whether value should be kept on top of the stack or
// removed.
cel_base::StatusOr<std::unique_ptr<JumpStepBase>> CreateCondJumpStep(
    bool jump_condition, bool leave_on_stack, absl::optional<int> jump_offset,
    int64_t expr_id);

// Factory method for ErrorJump step.
// This step performs a Jump when an Error is on the top of the stack.
// Value is left on stack if it is a bool or an error.
cel_base::StatusOr<std::unique_ptr<JumpStepBase>> CreateBoolCheckJumpStep(
    absl::optional<int> jump_offset, int64_t expr_id);

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_EVAL_EVAL_JUMP_STEP_H_
