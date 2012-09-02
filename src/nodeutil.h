#pragma once

#define BUILDING_NODE_EXTENSION
#include <node.h>
#include <node_buffer.h>
#include <v8.h>

/* ******************************************************
 * exception utilities
 */
#define THROW_TYPE_ERROR(str) \
    ThrowException(Exception::TypeError(String::New(str)))

/* ******************************************************
 * Argument utilities.
 */
#define ARG_EXT(I, VAR) \
    if (args.Length() <= (I) || !args[I]->IsExternal()) \
        return ThrowException(Exception::TypeError( \
            String::New("Argument " #I " must be an external"))); \
    Local<External> VAR = Local<External>::Cast(args[I]);

/**
 * ARG_STR(0, src);
 *
 * see http://blog.64p.org/entry/2012/09/02/101609
 */
#define ARG_STR(I, VAR) \
    if (args.Length() <= (I)) \
        return ThrowException(Exception::TypeError( \
            String::New("Argument " #I " must be a string"))); \
    String::Utf8Value VAR(args[I]->ToString());

#define ARG_OBJ(I, VAR) \
    if (args.Length() <= (I) || !args[I]->IsObject()) \
        return ThrowException(Exception::TypeError( \
            String::New("Argument " #I " must be a object"))); \
    Local<Object> VAR = Local<Object>::Cast(args[I]);

#define ARG_INT(I, VAR) \
    if (args.Length() <= (I) || !args[I]->IsInt32()) \
        return ThrowException(Exception::TypeError( \
            String::New("Argument " #I " must be an integer"))); \
    int32_t VAR = args[I]->Int32Value();

#define ARG_BUF(I, VAR) \
    if (args.Length() <= (I) || !Buffer::HasInstance(args[I])) \
        return ThrowException(Exception::TypeError( \
            String::New("Argument " #I " must be an Buffer"))); \
    void * VAR = Buffer::Data(args[I]->ToObject());

/* ******************************************************
 * Class construction utilities
 */
#define SET_ENUM_VALUE(target, _value) \
        target->Set(String::NewSymbol(#_value), \
                Integer::New(_value), \
                static_cast<PropertyAttribute>(ReadOnly|DontDelete))

