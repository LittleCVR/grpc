// Copyright 2021 gRPC authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "test/core/transport/binder/mock_objects.h"

#include <memory>

#include "absl/memory/memory.h"
#include "src/core/lib/iomgr/error.h"
#include "src/core/lib/iomgr/closure.h"

namespace grpc_binder {

using ::testing::Return;

MockReadableParcel::MockReadableParcel() {
  ON_CALL(*this, ReadBinder).WillByDefault([](std::unique_ptr<Binder>* binder) {
    *binder = std::make_unique<MockBinder>();
    return absl::OkStatus();
  });
  ON_CALL(*this, ReadInt32).WillByDefault(Return(absl::OkStatus()));
  ON_CALL(*this, ReadByteArray).WillByDefault(Return(absl::OkStatus()));
  ON_CALL(*this, ReadString).WillByDefault(Return(absl::OkStatus()));
}

MockWritableParcel::MockWritableParcel() {
  ON_CALL(*this, WriteInt32).WillByDefault(Return(absl::OkStatus()));
  ON_CALL(*this, WriteBinder).WillByDefault(Return(absl::OkStatus()));
  ON_CALL(*this, WriteString).WillByDefault(Return(absl::OkStatus()));
  ON_CALL(*this, WriteByteArray).WillByDefault(Return(absl::OkStatus()));
}

MockBinder::MockBinder() {
  ON_CALL(*this, PrepareTransaction).WillByDefault(Return(absl::OkStatus()));
  ON_CALL(*this, Transact).WillByDefault(Return(absl::OkStatus()));
  ON_CALL(*this, GetWritableParcel).WillByDefault(Return(&mock_input_));
  ON_CALL(*this, ConstructTxReceiver)
      .WillByDefault(
          [this](grpc_core::RefCountedPtr<WireReader> /*wire_reader_ref*/,
                 TransactionReceiver::OnTransactCb cb) {
            return std::make_unique<MockTransactionReceiver>(
                cb, BinderTransportTxCode::SETUP_TRANSPORT, &mock_output_);
          });
}

void CallTransactCallback(void* args, grpc_error_handle) {
  TransactCbArgs* s = static_cast<TransactCbArgs*>(args);
  s->transact_cb(static_cast<transaction_code_t>(s->code), s->output, /*uid=*/0).IgnoreError();
}

MockTransactionReceiver::MockTransactionReceiver(OnTransactCb transact_cb,
                                  BinderTransportTxCode code,
                                  MockReadableParcel* output) :
      combiner_(grpc_combiner_create()) {
  if (code == BinderTransportTxCode::SETUP_TRANSPORT) {
    EXPECT_CALL(*output, ReadInt32).WillOnce([](int32_t* version) {
      *version = 1;
      return absl::OkStatus();
    });
  }

  args_ = new TransactCbArgs();
  args_->transact_cb = transact_cb;
  args_->code = code;
  args_->output = output;
  combiner_->Run(
    GRPC_CLOSURE_CREATE(CallTransactCallback, args_, nullptr),
    absl::OkStatus());
}

}  // namespace grpc_binder
