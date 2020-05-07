#include "eval/eval/field_access.h"

#include <type_traits>

#include "google/protobuf/map_field.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/substitute.h"
#include "internal/proto_util.h"
#include "base/canonical_errors.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

namespace {

using ::google::protobuf::Arena;
using ::google::protobuf::FieldDescriptor;
using ::google::protobuf::MapValueRef;
using ::google::protobuf::Message;
using ::google::protobuf::Reflection;

// Well-known type protobuf type names which require special get / set behavior.
constexpr const char kProtobufDuration[] = "google.protobuf.Duration";
constexpr const char kProtobufTimestamp[] = "google.protobuf.Timestamp";

// Singular message fields and repeated message fields have similar access model
// To provide common approach, we implement accessor classes, based on CRTP.
// FieldAccessor is CRTP base class, specifying Get.. method family.
template <class Derived>
class FieldAccessor {
 public:
  bool GetBool() const { return static_cast<const Derived*>(this)->GetBool(); }

  int64_t GetInt32() const {
    return static_cast<const Derived*>(this)->GetInt32();
  }

  uint64_t GetUInt32() const {
    return static_cast<const Derived*>(this)->GetUInt32();
  }

  int64_t GetInt64() const {
    return static_cast<const Derived*>(this)->GetInt64();
  }

  uint64_t GetUInt64() const {
    return static_cast<const Derived*>(this)->GetUInt64();
  }

  double GetFloat() const {
    return static_cast<const Derived*>(this)->GetFloat();
  }

  double GetDouble() const {
    return static_cast<const Derived*>(this)->GetDouble();
  }

  const std::string* GetString(std::string* buffer) const {
    return static_cast<const Derived*>(this)->GetString(buffer);
  }

  const Message* GetMessage() const {
    return static_cast<const Derived*>(this)->GetMessage();
  }

  int64_t GetEnumValue() const {
    return static_cast<const Derived*>(this)->GetEnumValue();
  }

  // This method provides message field content, wrapped in CelValue.
  // If value provided successfully, returns Ok.
  // arena Arena to use for allocations if needed.
  // result pointer to object to store value in.
  cel_base::Status CreateValueFromFieldAccessor(Arena* arena, CelValue* result) {
    switch (field_desc_->cpp_type()) {
      case FieldDescriptor::CPPTYPE_BOOL: {
        bool value = GetBool();
        *result = CelValue::CreateBool(value);
        break;
      }
      case FieldDescriptor::CPPTYPE_INT32: {
        int64_t value = GetInt32();
        *result = CelValue::CreateInt64(value);
        break;
      }
      case FieldDescriptor::CPPTYPE_INT64: {
        int64_t value = GetInt64();
        *result = CelValue::CreateInt64(value);
        break;
      }
      case FieldDescriptor::CPPTYPE_UINT32: {
        uint64_t value = GetUInt32();
        *result = CelValue::CreateUint64(value);
        break;
      }
      case FieldDescriptor::CPPTYPE_UINT64: {
        uint64_t value = GetUInt64();
        *result = CelValue::CreateUint64(value);
        break;
      }
      case FieldDescriptor::CPPTYPE_FLOAT: {
        double value = GetFloat();
        *result = CelValue::CreateDouble(value);
        break;
      }
      case FieldDescriptor::CPPTYPE_DOUBLE: {
        double value = GetDouble();
        *result = CelValue::CreateDouble(value);
        break;
      }
      case FieldDescriptor::CPPTYPE_STRING: {
        std::string buffer;
        const std::string* value = GetString(&buffer);
        if (value == &buffer) {
          value = google::protobuf::Arena::Create<std::string>(arena, std::move(buffer));
        }
        switch (field_desc_->type()) {
          case FieldDescriptor::TYPE_STRING:
            *result = CelValue::CreateString(value);
            break;
          case FieldDescriptor::TYPE_BYTES:
            *result = CelValue::CreateBytes(value);
            break;
          default:
            return cel_base::Status(cel_base::StatusCode::kInvalidArgument,
                                "Error handling C++ string conversion");
        }
        break;
      }
      case FieldDescriptor::CPPTYPE_MESSAGE: {
        const google::protobuf::Message* msg_value = GetMessage();
        *result = CelValue::CreateMessage(msg_value, arena);
        break;
      }
      case FieldDescriptor::CPPTYPE_ENUM: {
        int enum_value = GetEnumValue();
        *result = CelValue::CreateInt64(enum_value);
        break;
      }
      default:
        return cel_base::Status(cel_base::StatusCode::kInvalidArgument,
                            "Unhandled C++ type conversion");
    }

    return cel_base::OkStatus();
  }

 protected:
  FieldAccessor(const Message* msg, const FieldDescriptor* field_desc)
      : msg_(msg), field_desc_(field_desc) {}

  const Message* msg_;
  const FieldDescriptor* field_desc_;
};

// Accessor class, to work with singular fields
class ScalarFieldAccessor : public FieldAccessor<ScalarFieldAccessor> {
 public:
  ScalarFieldAccessor(const Message* msg, const FieldDescriptor* field_desc)
      : FieldAccessor(msg, field_desc) {}

  bool GetBool() const { return GetReflection()->GetBool(*msg_, field_desc_); }

  int64_t GetInt32() const {
    return GetReflection()->GetInt32(*msg_, field_desc_);
  }

  uint64_t GetUInt32() const {
    return GetReflection()->GetUInt32(*msg_, field_desc_);
  }

  int64_t GetInt64() const {
    return GetReflection()->GetInt64(*msg_, field_desc_);
  }

  uint64_t GetUInt64() const {
    return GetReflection()->GetUInt64(*msg_, field_desc_);
  }

  double GetFloat() const {
    return GetReflection()->GetFloat(*msg_, field_desc_);
  }

  double GetDouble() const {
    return GetReflection()->GetDouble(*msg_, field_desc_);
  }

  const std::string* GetString(std::string* buffer) const {
    return &GetReflection()->GetStringReference(*msg_, field_desc_, buffer);
  }

  const Message* GetMessage() const {
    return &GetReflection()->GetMessage(*msg_, field_desc_);
  }

  int64_t GetEnumValue() const {
    return GetReflection()->GetEnumValue(*msg_, field_desc_);
  }

  const Reflection* GetReflection() const { return msg_->GetReflection(); }
};

// Accessor class, to work with repeated fields.
class RepeatedFieldAccessor : public FieldAccessor<RepeatedFieldAccessor> {
 public:
  RepeatedFieldAccessor(const Message* msg, const FieldDescriptor* field_desc,
                        int index)
      : FieldAccessor(msg, field_desc), index_(index) {}

  bool GetBool() const {
    return GetReflection()->GetRepeatedBool(*msg_, field_desc_, index_);
  }

  int64_t GetInt32() const {
    return GetReflection()->GetRepeatedInt32(*msg_, field_desc_, index_);
  }

  uint64_t GetUInt32() const {
    return GetReflection()->GetRepeatedUInt32(*msg_, field_desc_, index_);
  }

  int64_t GetInt64() const {
    return GetReflection()->GetRepeatedInt64(*msg_, field_desc_, index_);
  }

  uint64_t GetUInt64() const {
    return GetReflection()->GetRepeatedUInt64(*msg_, field_desc_, index_);
  }

  double GetFloat() const {
    return GetReflection()->GetRepeatedFloat(*msg_, field_desc_, index_);
  }

  double GetDouble() const {
    return GetReflection()->GetRepeatedDouble(*msg_, field_desc_, index_);
  }

  const std::string* GetString(std::string* buffer) const {
    return &GetReflection()->GetRepeatedStringReference(*msg_, field_desc_,
                                                        index_, buffer);
  }

  const Message* GetMessage() const {
    return &GetReflection()->GetRepeatedMessage(*msg_, field_desc_, index_);
  }

  int64_t GetEnumValue() const {
    return GetReflection()->GetRepeatedEnumValue(*msg_, field_desc_, index_);
  }

  const Reflection* GetReflection() const { return msg_->GetReflection(); }

 private:
  int index_;
};

// Accessor class, to work with map values
class MapValueAccessor : public FieldAccessor<MapValueAccessor> {
 public:
  MapValueAccessor(const Message* msg, const FieldDescriptor* field_desc,
                   const MapValueRef* value_ref)
      : FieldAccessor(msg, field_desc), value_ref_(value_ref) {}

  bool GetBool() const { return value_ref_->GetBoolValue(); }

  int64_t GetInt32() const { return value_ref_->GetInt32Value(); }

  uint64_t GetUInt32() const { return value_ref_->GetUInt32Value(); }

  int64_t GetInt64() const { return value_ref_->GetInt64Value(); }

  uint64_t GetUInt64() const { return value_ref_->GetUInt64Value(); }

  double GetFloat() const { return value_ref_->GetFloatValue(); }

  double GetDouble() const { return value_ref_->GetDoubleValue(); }

  const std::string* GetString(std::string* /*buffer*/) const {
    return &value_ref_->GetStringValue();
  }

  const Message* GetMessage() const { return &value_ref_->GetMessageValue(); }

  int64_t GetEnumValue() const { return value_ref_->GetEnumValue(); }

  const Reflection* GetReflection() const { return msg_->GetReflection(); }

 private:
  const MapValueRef* value_ref_;
};

// Helper classes that should retrieve values from CelValue,
// when CelValue content inherits from Message.
template <class T, bool ZZ>
class MessageRetriever {
 public:
  absl::optional<const Message*> operator()(const T&) const { return {}; }
};

// Partial specialization, valid when T is assignable to message
//
template <class T>
class MessageRetriever<T, true> {
 public:
  absl::optional<const Message*> operator()(const T& arg) const {
    const Message* msg = arg;
    return msg;
  }
};

class MessageRetrieverOp {
 public:
  template <typename T>
  absl::optional<const Message*> operator()(const T& arg) {
    // Metaprogramming hacks...
    return MessageRetriever<T, std::is_assignable<const Message*&, T>::value>()(
        arg);
  }
};

}  // namespace

cel_base::Status CreateValueFromSingleField(const google::protobuf::Message* msg,
                                        const FieldDescriptor* desc,
                                        google::protobuf::Arena* arena,
                                        CelValue* result) {
  ScalarFieldAccessor accessor(msg, desc);
  return accessor.CreateValueFromFieldAccessor(arena, result);
}

cel_base::Status CreateValueFromRepeatedField(const google::protobuf::Message* msg,
                                          const FieldDescriptor* desc,
                                          google::protobuf::Arena* arena, int index,
                                          CelValue* result) {
  RepeatedFieldAccessor accessor(msg, desc, index);
  return accessor.CreateValueFromFieldAccessor(arena, result);
}

cel_base::Status CreateValueFromMapValue(const google::protobuf::Message* msg,
                                     const FieldDescriptor* desc,
                                     const MapValueRef* value_ref,
                                     google::protobuf::Arena* arena, CelValue* result) {
  MapValueAccessor accessor(msg, desc, value_ref);
  return accessor.CreateValueFromFieldAccessor(arena, result);
}

// Singular message fields and repeated message fields have similar access model
// To provide common approach, we implement field setter classes, based on CRTP.
// FieldAccessor is CRTP base class, specifying Get.. method family.
template <class Derived>
class FieldSetter {
 public:
  bool AssignBool(const CelValue& cel_value) const {
    bool value;

    if (!cel_value.GetValue(&value)) {
      return false;
    }
    static_cast<const Derived*>(this)->SetBool(value);
    return true;
  }

  bool AssignInt32(const CelValue& cel_value) const {
    int64_t value;
    if (!cel_value.GetValue(&value)) {
      return false;
    }
    static_cast<const Derived*>(this)->SetInt32(value);
    return true;
  }

  bool AssignUInt32(const CelValue& cel_value) const {
    uint64_t value;
    if (!cel_value.GetValue(&value)) {
      return false;
    }
    static_cast<const Derived*>(this)->SetUInt32(value);
    return true;
  }

  bool AssignInt64(const CelValue& cel_value) const {
    int64_t value;
    if (!cel_value.GetValue(&value)) {
      return false;
    }
    static_cast<const Derived*>(this)->SetInt64(value);
    return true;
  }

  bool AssignUInt64(const CelValue& cel_value) const {
    uint64_t value;
    if (!cel_value.GetValue(&value)) {
      return false;
    }
    static_cast<const Derived*>(this)->SetUInt64(value);
    return true;
  }

  bool AssignFloat(const CelValue& cel_value) const {
    double value;
    if (!cel_value.GetValue(&value)) {
      return false;
    }
    static_cast<const Derived*>(this)->SetFloat(value);
    return true;
  }

  bool AssignDouble(const CelValue& cel_value) const {
    double value;
    if (!cel_value.GetValue(&value)) {
      return false;
    }
    static_cast<const Derived*>(this)->SetDouble(value);
    return true;
  }

  bool AssignString(const CelValue& cel_value) const {
    CelValue::StringHolder value;
    if (!cel_value.GetValue(&value)) {
      return false;
    }
    static_cast<const Derived*>(this)->SetString(value);
    return true;
  }

  bool AssignBytes(const CelValue& cel_value) const {
    CelValue::BytesHolder value;
    if (!cel_value.GetValue(&value)) {
      return false;
    }
    static_cast<const Derived*>(this)->SetBytes(value);
    return true;
  }

  bool AssignEnum(const CelValue& cel_value) const {
    int64_t value;
    if (!cel_value.GetValue(&value)) {
      return false;
    }
    static_cast<const Derived*>(this)->SetEnum(value);
    return true;
  }

  bool AssignMessage(const CelValue& cel_value) const {
    // We attempt to retrieve value if it derives from google::protobuf::Message.
    // That includes both generic Protobuf message types and specific
    // message types stored in CelValue as separate entities.
    auto value = cel_value.template Visit<absl::optional<const Message*>>(
        MessageRetrieverOp());

    if (!value.has_value()) {
      GOOGLE_LOG(ERROR) << "Has No Value";
      return false;
    }

    static_cast<const Derived*>(this)->SetMessage(value.value());
    return true;
  }

  bool AssignDuration(const CelValue& cel_value) const {
    absl::Duration d;
    if (!cel_value.GetValue(&d)) {
      GOOGLE_LOG(ERROR) << "Unable to retrieve duration";
      return false;
    }
    google::protobuf::Duration duration;
    google::api::expr::internal::EncodeDuration(d, &duration);
    static_cast<const Derived*>(this)->SetMessage(&duration);
    return true;
  }

  bool AssignTimestamp(const CelValue& cel_value) const {
    absl::Time t;
    if (!cel_value.GetValue(&t)) {
      GOOGLE_LOG(ERROR) << "Unable to retrieve timestamp";
      return false;
    }
    google::protobuf::Timestamp timestamp;
    google::api::expr::internal::EncodeTime(t, &timestamp);
    static_cast<const Derived*>(this)->SetMessage(&timestamp);
    return true;
  }

  // This method provides message field content, wrapped in CelValue.
  // If value provided successfully, returns Ok.
  // arena Arena to use for allocations if needed.
  // result pointer to object to store value in.
  bool SetFieldFromCelValue(const CelValue& value) {
    switch (field_desc_->cpp_type()) {
      case FieldDescriptor::CPPTYPE_BOOL: {
        return AssignBool(value);
      }
      case FieldDescriptor::CPPTYPE_INT32: {
        return AssignInt32(value);
      }
      case FieldDescriptor::CPPTYPE_INT64: {
        return AssignInt64(value);
      }
      case FieldDescriptor::CPPTYPE_UINT32: {
        return AssignUInt32(value);
      }
      case FieldDescriptor::CPPTYPE_UINT64: {
        return AssignUInt64(value);
      }
      case FieldDescriptor::CPPTYPE_FLOAT: {
        return AssignFloat(value);
      }
      case FieldDescriptor::CPPTYPE_DOUBLE: {
        return AssignDouble(value);
      }
      case FieldDescriptor::CPPTYPE_STRING: {
        switch (field_desc_->type()) {
          case FieldDescriptor::TYPE_STRING:

            return AssignString(value);
          case FieldDescriptor::TYPE_BYTES:
            return AssignBytes(value);
          default:
            return false;
        }
        break;
      }
      case FieldDescriptor::CPPTYPE_MESSAGE: {
        const std::string& type_name = field_desc_->message_type()->full_name();
        // When the field is a message, it might be a well-known type with a
        // non-proto representation that requires special handling before it
        // can be set on the field.
        if (type_name == kProtobufTimestamp) {
          return AssignTimestamp(value);
        } else if (type_name == kProtobufDuration) {
          return AssignDuration(value);
        }
        return AssignMessage(value);
      }
      case FieldDescriptor::CPPTYPE_ENUM: {
        return AssignEnum(value);
      }
      default:
        return false;
    }

    return true;
  }

 protected:
  FieldSetter(Message* msg, const FieldDescriptor* field_desc)
      : msg_(msg), field_desc_(field_desc) {}

  Message* msg_;
  const FieldDescriptor* field_desc_;
};

// Accessor class, to work with singular fields
class ScalarFieldSetter : public FieldSetter<ScalarFieldSetter> {
 public:
  ScalarFieldSetter(Message* msg, const FieldDescriptor* field_desc)
      : FieldSetter(msg, field_desc) {}

  bool SetBool(bool value) const {
    GetReflection()->SetBool(msg_, field_desc_, value);
    return true;
  }

  bool SetInt32(int32_t value) const {
    GetReflection()->SetInt32(msg_, field_desc_, value);
    return true;
  }

  bool SetUInt32(uint32_t value) const {
    GetReflection()->SetUInt32(msg_, field_desc_, value);
    return true;
  }

  bool SetInt64(int64_t value) const {
    GetReflection()->SetInt64(msg_, field_desc_, value);
    return true;
  }

  bool SetUInt64(uint64_t value) const {
    GetReflection()->SetUInt64(msg_, field_desc_, value);
    return true;
  }

  bool SetFloat(float value) const {
    GetReflection()->SetFloat(msg_, field_desc_, value);
    return true;
  }

  bool SetDouble(double value) const {
    GetReflection()->SetDouble(msg_, field_desc_, value);
    return true;
  }

  bool SetString(CelValue::StringHolder value) const {
    GetReflection()->SetString(msg_, field_desc_, std::string(value.value()));
    return true;
  }

  bool SetBytes(CelValue::BytesHolder value) const {
    GetReflection()->SetString(msg_, field_desc_, std::string(value.value()));
    return true;
  }

  bool SetMessage(const Message* value) const {
    if (!value) {
      GOOGLE_LOG(ERROR) << "Message is NULL";
      return true;
    }
    if (value->GetDescriptor()->full_name() !=
        field_desc_->message_type()->full_name()) {
      return false;
    }

    GetReflection()->MutableMessage(msg_, field_desc_)->MergeFrom(*value);
    return true;
  }

  bool SetEnum(const int64_t value) const {
    GetReflection()->SetEnumValue(msg_, field_desc_, value);
    return true;
  }

  const Reflection* GetReflection() const { return msg_->GetReflection(); }
};

// Appender class, to work with repeated fields
class RepeatedFieldSetter : public FieldSetter<RepeatedFieldSetter> {
 public:
  RepeatedFieldSetter(Message* msg, const FieldDescriptor* field_desc)
      : FieldSetter(msg, field_desc) {}

  bool SetBool(bool value) const {
    GetReflection()->AddBool(msg_, field_desc_, value);
    return true;
  }

  bool SetInt32(int32_t value) const {
    GetReflection()->AddInt32(msg_, field_desc_, value);
    return true;
  }

  bool SetUInt32(uint32_t value) const {
    GetReflection()->AddUInt32(msg_, field_desc_, value);
    return true;
  }

  bool SetInt64(int64_t value) const {
    GetReflection()->AddInt64(msg_, field_desc_, value);
    return true;
  }

  bool SetUInt64(uint64_t value) const {
    GetReflection()->AddUInt64(msg_, field_desc_, value);
    return true;
  }

  bool SetFloat(float value) const {
    GetReflection()->AddFloat(msg_, field_desc_, value);
    return true;
  }

  bool SetDouble(double value) const {
    GetReflection()->AddDouble(msg_, field_desc_, value);
    return true;
  }

  bool SetString(CelValue::StringHolder value) const {
    GetReflection()->AddString(msg_, field_desc_, std::string(value.value()));
    return true;
  }

  bool SetBytes(CelValue::BytesHolder value) const {
    GetReflection()->AddString(msg_, field_desc_, std::string(value.value()));
    return true;
  }

  bool SetMessage(const Message* value) const {
    if (!value) return true;
    if (value->GetDescriptor()->full_name() !=
        field_desc_->message_type()->full_name()) {
      return false;
    }

    GetReflection()->AddMessage(msg_, field_desc_)->MergeFrom(*value);
    return true;
  }

  bool SetEnum(const int64_t value) const {
    GetReflection()->AddEnumValue(msg_, field_desc_, value);
    return true;
  }

 private:
  const Reflection* GetReflection() const { return msg_->GetReflection(); }
};

// This method sets message field
// If value provided successfully, returns Ok.
// arena Arena to use for allocations if needed.
// result pointer to object to store value in.
::cel_base::Status SetValueToSingleField(const CelValue& value,
                                     const FieldDescriptor* desc,
                                     Message* msg) {
  ScalarFieldSetter setter(msg, desc);
  return (setter.SetFieldFromCelValue(value))
             ? ::cel_base::OkStatus()
             : ::cel_base::InvalidArgumentError(absl::Substitute(
                   "Could not assign supplied argument to message \"$0\" field "
                   "\"$1\" of type $2: type was $3",
                   msg->GetDescriptor()->name(), desc->name(),
                   desc->type_name(), absl::StrCat(value.type())));
}

::cel_base::Status AddValueToRepeatedField(const CelValue& value,
                                       const FieldDescriptor* desc,
                                       Message* msg) {
  RepeatedFieldSetter setter(msg, desc);
  return (setter.SetFieldFromCelValue(value))
             ? ::cel_base::OkStatus()
             : ::cel_base::InvalidArgumentError(absl::Substitute(
                   "Could not add supplied argument to message \"$0\" field "
                   "\"$1\".",
                   msg->GetDescriptor()->name(), desc->name()));
}

::cel_base::Status AddValueToMapField(const CelValue& key, const CelValue& value,
                                  const FieldDescriptor* desc, Message* msg) {
  auto entry_msg = msg->GetReflection()->AddMessage(msg, desc);
  auto key_field_desc = entry_msg->GetDescriptor()->FindFieldByNumber(1);
  auto value_field_desc = entry_msg->GetDescriptor()->FindFieldByNumber(2);

  ScalarFieldSetter key_setter(entry_msg, key_field_desc);
  ScalarFieldSetter value_setter(entry_msg, value_field_desc);

  if (!key_setter.SetFieldFromCelValue(key)) {
    return ::cel_base::InvalidArgumentError(
        absl::Substitute("Could not assign supplied argument to message \"$0\" "
                         "field \"$1\" map key.",
                         msg->GetDescriptor()->name(), desc->name()));
  }

  if (!value_setter.SetFieldFromCelValue(value)) {
    return ::cel_base::InvalidArgumentError(
        absl::Substitute("Could not assign supplied argument to message \"$0\" "
                         "field \"$1\" map value.",
                         msg->GetDescriptor()->name(), desc->name()));
  }

  return ::cel_base::OkStatus();
}

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
