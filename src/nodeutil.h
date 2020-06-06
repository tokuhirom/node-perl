#pragma once

// #define BUILDING_NODE_EXTENSION
// #include <node.h>
// #include <node_buffer.h>
// #include <v8.h>

/* ******************************************************
 * exception utilities
 */
#define THROW_TYPE_ERROR(str) Nan::ThrowError(Exception::TypeError(Nan::New(str).ToLocalChecked()))

/* ******************************************************
 * Argument utilities.
 */
#define ARG_EXT(I, VAR)                                                                                              \
    if (args.Length() <= (I) || !args[I]->IsExternal())                                                              \
    {                                                                                                                \
        Nan::ThrowError(v8::Exception::TypeError(Nan::New("Argument " #I " must be an external").ToLocalChecked())); \
    }                                                                                                                \
    v8::Local<v8::External> VAR = v8::Local<v8::External>::Cast(args[I]);

/**
 * ARG_STR(0, src);
 *
 * see http://blog.64p.org/entry/2012/09/02/101609
 */
#define ARG_STR(I, VAR)                                                                                           \
    if (args.Length() <= (I))                                                                                     \
    {                                                                                                             \
        Nan::ThrowError(v8::Exception::TypeError(Nan::New("Argument " #I " must be a string").ToLocalChecked())); \
    }                                                                                                             \
    Nan::Utf8String VAR(args[I]->ToString(Nan::GetCurrentContext()).FromMaybe(v8::Local<v8::String>()));

#define ARG_OBJ(I, VAR)                                                                                           \
    if (args.Length() <= (I) || !args[I]->IsObject())                                                             \
    {                                                                                                             \
        Nan::ThrowError(v8::Exception::TypeError(Nan::New("Argument " #I " must be a object").ToLocalChecked())); \
    }                                                                                                             \
    v8::Local<v8::Object> VAR = v8::Local<v8::Object>::Cast(args[I]);

#define ARG_INT(I, VAR)                                                                                             \
    if (args.Length() <= (I) || !args[I]->IsInt32())                                                                \
    {                                                                                                               \
        Nan::ThrowError(v8::Exception::TypeError(Nan::New("Argument " #I " must be an integer").ToLocalChecked())); \
    }                                                                                                               \
    int32_t VAR = args[I]->Int32Value();

#define ARG_BUF(I, VAR)                                                                                            \
    if (args.Length() <= (I) || !Buffer::HasInstance(args[I]))                                                     \
    {                                                                                                              \
        Nan::ThrowError(v8::Exception::TypeError(Nan::New("Argument " #I " must be an Buffer").ToLocalChecked())); \
    }                                                                                                              \
    void *VAR = Buffer::Data(args[I]->ToObject());

/* ******************************************************
 * Class construction utilities
 */
#define SET_ENUM_VALUE(target, _value)              \
    target->Set(                                    \
        Nan::New(#_value).ToLocalChecked(),         \
        Nan::New<Integer>(_value).ToLocalChecked(), \
        static_cast<PropertyAttribute>(ReadOnly | DontDelete))
