/* Copyright 2025 The JAX Authors.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#include <cstdint>
#include <optional>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "absl/status/status.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/Attributes.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinAttributeInterfaces.h"
#include "mlir/IR/Diagnostics.h"
#include "mlir/IR/DialectRegistry.h"
#include "mlir/IR/ImplicitLocOpBuilder.h"
#include "mlir/IR/Location.h"
#include "mlir/IR/MLIRContext.h"
#include "mlir/IR/ValueRange.h"
#include "mlir/Support/LLVM.h"
#include "jaxlib/mosaic/dialect/tpu/tpu_dialect.h"
#include "xla/mlir/utils/error_util.h"

namespace mlir::tpu {
namespace {

using ::testing::_;
using ::testing::HasSubstr;
using ::testing::status::StatusIs;

class TpuOpsVerificationTest : public ::testing::Test {
 protected:
  TpuOpsVerificationTest()
      : context_([]() {
          DialectRegistry registry;
          registry
              .insert<arith::ArithDialect, memref::MemRefDialect, TPUDialect>();
          return registry;
        }()),
        builder_(UnknownLoc::get(&context_), &context_) {
    context_.loadAllAvailableDialects();
    context_.printOpOnDiagnostic(true);
  }
  ~TpuOpsVerificationTest() {
    for (int i = ops_.size() - 1; i >= 0; --i) {
      ops_[i]->erase();
    }
  }

  template <typename OpTy, typename... Args>
  OpTy Create(Args&&... args) {
    OpTy op = builder_.create<OpTy>(std::forward<Args>(args)...);
    ops_.push_back(op.getOperation());
    return op;
  }

  template <typename OpTy>
  absl::Status VerifyOp(OpTy op) {
    BaseScopedDiagnosticHandler diag(&context_);
    if (op.verify().succeeded()) {
      return absl::OkStatus();
    }
    return diag.ConsumeStatus();
  }

  Type i32() { return builder_.getI32Type(); }

  MemRefType GetMemRefType(
      ArrayRef<int64_t> shape, Type element_type,
      std::optional<MemorySpace> memory_space = std::nullopt) {
    return MemRefType::get(
        shape, element_type, nullptr,
        memory_space.has_value()
            ? MemorySpaceAttr::get(builder_.getContext(), *memory_space)
            : Attribute());
  }

  Value AllocaI32(ArrayRef<int64_t> shape,
                  std::optional<MemorySpace> memory_space = std::nullopt) {
    return Create<memref::AllocaOp>(GetMemRefType(shape, i32(), memory_space))
        .getMemref();
  }

  Value ConstantI32Vector(ArrayRef<int64_t> shape, ArrayRef<int32_t> values) {
    return Create<arith::ConstantOp>(
               /*result=*/VectorType::get(shape, i32()),
               /*value=*/dyn_cast<TypedAttr>(
                   builder().getDenseI32ArrayAttr(values)))
        .getResult();
  }

  ImplicitLocOpBuilder& builder() { return builder_; }

 private:
  MLIRContext context_;
  ImplicitLocOpBuilder builder_;
  std::vector<Operation*> ops_;
};

TEST_F(TpuOpsVerificationTest, VectorLoadVerificationWorks) {
  auto c0 = Create<arith::ConstantIndexOp>(0);
  Value memref = AllocaI32({8});
  auto vl = Create<VectorLoadOp>(
      /*result=*/VectorType::get({8}, i32()),
      /*base=*/memref,
      /*indices=*/ValueRange{c0},
      /*strides=*/builder().getDenseI32ArrayAttr({}),
      /*mask=*/nullptr);

  ASSERT_OK(VerifyOp(vl));
}

TEST_F(TpuOpsVerificationTest,
       VectorLoadRankOfStridesDoesNotMatchBaseMemrefRank) {
  auto c0 = Create<arith::ConstantIndexOp>(0);
  Value memref = AllocaI32({8});
  auto vl = Create<VectorLoadOp>(
      /*result=*/VectorType::get({8}, i32()),
      /*base=*/memref,
      /*indices=*/ValueRange{c0},
      /*strides=*/builder().getDenseI32ArrayAttr({1, 1, 1, 1}),
      /*mask=*/nullptr);
  ASSERT_THAT(VerifyOp(vl), StatusIs(_, HasSubstr("Expected 1 strides.")));
}

TEST_F(TpuOpsVerificationTest, VectorLoadStridesFeatureNotImplemented) {
  auto c0 = Create<arith::ConstantIndexOp>(0);
  Value memref = AllocaI32({8});
  auto vl = Create<VectorLoadOp>(
      /*result=*/VectorType::get({8}, i32()),
      /*base=*/memref,
      /*indices=*/ValueRange{c0},
      /*strides=*/builder().getDenseI32ArrayAttr({1}),
      /*mask=*/nullptr);
  ASSERT_THAT(
      VerifyOp(vl),
      StatusIs(
          _, HasSubstr("Not implemented: general vector load with strides.")));
}

TEST_F(TpuOpsVerificationTest, VectorLoadBaseAndResultTypesDoNotMatch) {
  auto c0 = Create<arith::ConstantIndexOp>(0);
  Value memref = AllocaI32({8});
  auto vl = Create<VectorLoadOp>(
      /*result=*/VectorType::get({8}, builder().getF32Type()),
      /*base=*/memref,
      /*indices=*/ValueRange{c0},
      /*strides=*/builder().getDenseI32ArrayAttr({}),
      /*mask=*/nullptr);

  ASSERT_THAT(
      VerifyOp(vl),
      StatusIs(_,
               HasSubstr("Expected base and result element type to match.")));
}

TEST_F(TpuOpsVerificationTest,
       VectorLoadRankOfIndicesDoesNotMatchBaseMemrefRank) {
  auto c0 = Create<arith::ConstantIndexOp>(0);
  Value memref = AllocaI32({8});
  auto vl = Create<VectorLoadOp>(
      /*result=*/VectorType::get({8}, i32()),
      /*base=*/memref,
      /*indices=*/ValueRange{c0, c0, c0},
      /*strides=*/builder().getDenseI32ArrayAttr({}),
      /*mask=*/nullptr);

  ASSERT_THAT(VerifyOp(vl), StatusIs(_, HasSubstr("Expected 1 indices.")));
}

TEST_F(TpuOpsVerificationTest, VectorLoadValidMaskSucceeds) {
  auto c0 = Create<arith::ConstantIndexOp>(0);
  Value memref = AllocaI32({8, 128});
  Value mask = ConstantI32Vector(/*shape=*/{8, 1},
                                 /*values=*/{1, 1, 1, 1, 1, 1, 1, 1});
  auto vl = Create<VectorLoadOp>(
      /*result=*/VectorType::get({8, 128}, i32()),
      /*base=*/memref,
      /*indices=*/ValueRange{c0, c0},
      /*strides=*/builder().getDenseI32ArrayAttr({}),
      /*mask=*/mask);

  ASSERT_OK(VerifyOp(vl));
}

TEST_F(TpuOpsVerificationTest, VectorLoadMaskInvalidResultBitWidth) {
  auto c0 = Create<arith::ConstantIndexOp>(0);
  auto memref = Create<memref::AllocaOp>(
      MemRefType::get({8, 128}, builder().getI64Type()));
  Value mask = ConstantI32Vector(/*shape=*/{8, 1},
                                 /*values=*/{1, 1, 1, 1, 1, 1, 1, 1});
  auto vl = Create<VectorLoadOp>(
      /*result=*/VectorType::get({8, 128}, builder().getI64Type()),
      /*base=*/memref.getMemref(),
      /*indices=*/ValueRange{c0, c0},
      /*strides=*/builder().getDenseI32ArrayAttr({}),
      /*mask=*/mask);

  ASSERT_THAT(
      VerifyOp(vl),
      StatusIs(
          _, HasSubstr(
                 "Not implemented: masked load with non-32-bit element type")));
}

TEST_F(TpuOpsVerificationTest,
       VectorLoadMaskNotBroadcastableToResultShapeInvalidMinor) {
  auto c0 = Create<arith::ConstantIndexOp>(0);
  Value memref = AllocaI32({8, 128});
  Value mask = ConstantI32Vector(/*shape=*/{8, 2},
                                 /*values=*/{1});
  auto vl = Create<VectorLoadOp>(
      /*result=*/VectorType::get({8, 128}, i32()),
      /*base=*/memref,
      /*indices=*/ValueRange{c0, c0},
      /*strides=*/builder().getDenseI32ArrayAttr({}),
      /*mask=*/mask);

  ASSERT_THAT(
      VerifyOp(vl),
      StatusIs(
          _, HasSubstr(
                 "Expected mask shape to be broadcastable to result shape.")));
}

TEST_F(TpuOpsVerificationTest,
       VectorLoadMaskNotBroadcastableToResultShapeInvalidMajor) {
  auto c0 = Create<arith::ConstantIndexOp>(0);
  Value memref = AllocaI32({8, 128});
  Value mask = ConstantI32Vector(/*shape=*/{5, 1},
                                 /*values=*/{1});
  auto vl = Create<VectorLoadOp>(
      /*result=*/VectorType::get({8, 128}, i32()),
      /*base=*/memref,
      /*indices=*/ValueRange{c0, c0},
      /*strides=*/builder().getDenseI32ArrayAttr({}),
      /*mask=*/mask);

  ASSERT_THAT(
      VerifyOp(vl),
      StatusIs(
          _, HasSubstr(
                 "Expected mask shape to be broadcastable to result shape.")));
}

}  // namespace
}  // namespace mlir::tpu
