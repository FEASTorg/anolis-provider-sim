#pragma once

#include <algorithm>
#include <map>
#include <string>

#include <google/protobuf/util/time_util.h>
#include "protocol.pb.h"

namespace sim_devices
{

    using anolis::deviceprovider::v0::SignalValue;
    using anolis::deviceprovider::v0::Status;
    using anolis::deviceprovider::v0::Value;
    using anolis::deviceprovider::v0::ValueType;

    // -----------------------------
    // Common helpers
    // -----------------------------

    static inline google::protobuf::Timestamp now_ts()
    {
        return google::protobuf::util::TimeUtil::GetCurrentTime();
    }

    static inline double clamp(double v, double lo, double hi)
    {
        return std::max(lo, std::min(hi, v));
    }

    static inline bool value_is_type(const Value &v, ValueType t)
    {
        return v.type() == t;
    }

    static inline bool get_arg_bool(const std::map<std::string, Value> &args, const char *key, bool &out)
    {
        auto it = args.find(key);
        if (it == args.end())
            return false;
        if (!value_is_type(it->second, ValueType::VALUE_TYPE_BOOL))
            return false;
        out = it->second.bool_value();
        return true;
    }

    static inline bool get_arg_int64(const std::map<std::string, Value> &args, const char *key, int64_t &out)
    {
        auto it = args.find(key);
        if (it == args.end())
            return false;
        if (!value_is_type(it->second, ValueType::VALUE_TYPE_INT64))
            return false;
        out = it->second.int64_value();
        return true;
    }

    static inline bool get_arg_double(const std::map<std::string, Value> &args, const char *key, double &out)
    {
        auto it = args.find(key);
        if (it == args.end())
            return false;
        if (!value_is_type(it->second, ValueType::VALUE_TYPE_DOUBLE))
            return false;
        out = it->second.double_value();
        return true;
    }

    static inline bool get_arg_string(const std::map<std::string, Value> &args, const char *key, std::string &out)
    {
        auto it = args.find(key);
        if (it == args.end())
            return false;
        if (!value_is_type(it->second, ValueType::VALUE_TYPE_STRING))
            return false;
        out = it->second.string_value();
        return true;
    }

    static inline Value make_bool(bool b)
    {
        Value v;
        v.set_type(ValueType::VALUE_TYPE_BOOL);
        v.set_bool_value(b);
        return v;
    }

    static inline Value make_double(double d)
    {
        Value v;
        v.set_type(ValueType::VALUE_TYPE_DOUBLE);
        v.set_double_value(d);
        return v;
    }

    static inline Value make_string(const std::string &s)
    {
        Value v;
        v.set_type(ValueType::VALUE_TYPE_STRING);
        v.set_string_value(s);
        return v;
    }

    static inline SignalValue make_signal_value(const std::string &id, const Value &value)
    {
        SignalValue sv;
        sv.set_signal_id(id);
        *sv.mutable_value() = value;
        *sv.mutable_timestamp() = now_ts();
        sv.set_quality(SignalValue::QUALITY_OK);
        return sv;
    }

    // -----------------------------
    // CallResult type
    // -----------------------------

    struct CallResult
    {
        Status::Code code = Status::CODE_OK;
        std::string message = "ok";
    };

    static inline CallResult ok() { return {Status::CODE_OK, "ok"}; }
    static inline CallResult bad(const std::string &m) { return {Status::CODE_INVALID_ARGUMENT, m}; }
    static inline CallResult nf(const std::string &m) { return {Status::CODE_NOT_FOUND, m}; }
    static inline CallResult precond(const std::string &m) { return {Status::CODE_FAILED_PRECONDITION, m}; }

} // namespace sim_devices
