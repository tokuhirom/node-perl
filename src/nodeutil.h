#pragma once

#include "nan.h"

/* ******************************************************
 * exception utilities
 */
#define THROW_TYPE_ERROR(str) \
    Nan::ThrowError(str)

/* ******************************************************
 * Argument utilities.
 */
#define ARG_EXT(I, VAR) \
    if (args.Length() <= (I) || !args[I]->IsExternal()) { \
        Nan::ThrowError("Argument " #I " must be an external"); \
        return Nan::Undefined(); \
    } \
    Local<External> VAR = Local<External>::Cast(args[I]);

/**
 * ARG_STR(0, src);
 *
 * see http://blog.64p.org/entry/2012/09/02/101609
 */
#define ARG_STR(I, VAR) \
    if (args.Length() <= (I)) { \
        Nan::ThrowError("Argument " #I " must be a string"); \
        return Nan::Undefined(); \
    } \
    Nan::Utf8String VAR(args[I]->ToString());

#define ARG_OBJ(I, VAR) \
    if (args.Length() <= (I) || !args[I]->IsObject()) { \
        Nan::ThrowError("Argument " #I " must be a object"); \
        return Nan::Undefined(); \
    } \
    Local<Object> VAR = Local<Object>::Cast(args[I]);

#define ARG_INT(I, VAR) \
    if (args.Length() <= (I) || !args[I]->IsInt32()) { \
        Nan::ThrowError("Argument " #I " must be an integer"); \
        return Nan::Undefined(); \
    } \
    int32_t VAR = args[I]->Int32Value();

#define ARG_BUF(I, VAR) \
    if (args.Length() <= (I) || !Buffer::HasInstance(args[I])) { \
        Nan::ThrowError("Argument " #I " must be a buffer"); \
        return Nan::Undefined(); \
    } \
    void * VAR = Buffer::Data(args[I]->ToObject());

/* ******************************************************
 * Class construction utilities
 */
#define SET_ENUM_VALUE(target, _value) \
        target->Set(String::NewSymbol(#_value), \
                Integer::New(_value), \
                static_cast<PropertyAttribute>(ReadOnly|DontDelete))
