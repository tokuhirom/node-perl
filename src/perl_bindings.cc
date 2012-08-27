#define BUILDING_NODE_EXTENSION

#include <node.h>
#include <string>
#include <vector>
#include <iostream>

#include <EXTERN.h>               /* from the Perl distribution     */
#include <perl.h>                 /* from the Perl distribution     */
#include "ppport.h"

#ifdef New
#undef New
#endif

#define INTERPRETER_NAME "node-perl-simple"

using namespace v8;
using namespace node;

class NodePerl: ObjectWrap {
private:
    PerlInterpreter *my_perl;

public:
    NodePerl() {
        char **av = {NULL};
        const char *embedding[] = { "", "-e", "0" };

        PERL_SYS_INIT3(0,&av,NULL);
        my_perl = perl_alloc();
        perl_construct(my_perl);

        perl_parse(my_perl, NULL, 3, (char**)embedding, NULL);
        PL_exit_flags |= PERL_EXIT_DESTRUCT_END;
        perl_run(my_perl);
    }

    ~NodePerl() {
        PL_perl_destruct_level = 2;
        perl_destruct(my_perl);
        perl_free(my_perl);
    }

    static Handle<Value> New(const Arguments& args) {
        HandleScope scope;

        if (!args.IsConstructCall())
            return args.Callee()->NewInstance();
        try {
            (new NodePerl())->Wrap(args.Holder());
        } catch (const char *msg) {
            return ThrowException(Exception::Error(String::New(msg)));
        }
        return scope.Close(args.Holder());
    }

    static Handle<Value> eval(const Arguments& args) {
        HandleScope scope;
        if (!args[0]->IsString()) {
            return ThrowException(Exception::Error(String::New("Arguments must be string")));
        }
        v8::String::Utf8Value stmt(args[0]);

        Handle<Value> retval = Unwrap<NodePerl>(args.This())->eval(*stmt);
        return scope.Close(retval);
    }

private:
    Handle<Value> eval(const char *stmt) {
        return convert(eval_pv(stmt, TRUE));
    }

    Handle<Value> convert(SV * sv) {
        HandleScope scope;

        // see xs-src/pack.c in msgpack-perl
        SvGETMAGIC(sv);

        if (SvPOKp(sv)) {
            STRLEN len;
            const char *s = SvPV(sv, len);
            return scope.Close(v8::String::New(s, len));
        } else if (SvNOK(sv)) {
            return scope.Close(v8::Number::New(SvNVX(sv)));
        } else if (SvIOK(sv)) {
            return scope.Close(Integer::New(SvIVX(sv)));
        } else if (SvROK(sv)) {
            return scope.Close(this->convert_rv(SvRV(sv)));
        } else if (!SvOK(sv)) {
            return scope.Close(Undefined());
        } else if (isGV(sv)) {
            std::cerr << "Cannot pass GV to v8 world" << std::endl;
            return scope.Close(Undefined());
        } else {
            sv_dump(sv);
            return ThrowException(Exception::Error(String::New("node-perl-simple doesn't support this type")));
        }
        // TODO: return callback function for perl code.
    }

    inline Handle<Value> convert_rv(SV * sv) {
        HandleScope scope;

        SvGETMAGIC(sv);
        svtype svt = SvTYPE(sv);

        if (SvOBJECT(sv)) {
            // Type flag for blessed scalars.
            // TODO: support blessed object
            return scope.Close(Undefined());
        } else if (svt == SVt_PVHV) {
            HV* hval = (HV*)sv;
            HE* he;
            v8::Local<v8::Object> retval = v8::Object::New();
            while ((he = hv_iternext(hval))) {
                retval->Set(
                    this->convert(hv_iterkeysv(he)),
                    this->convert(hv_iterval(hval, he))
                );
            }
            return scope.Close(retval);
        } else if (svt == SVt_PVAV) {
            AV* ary = (AV*)sv;
            v8::Local<v8::Array> retval = v8::Array::New();
            int len = av_len(ary) + 1;
            for (int i=0; i<len; ++i) {
                SV** svp = av_fetch(ary, i, 0);
                if (svp) {
                    retval->Set(v8::Number::New(i), this->convert(*svp));
                } else {
                    retval->Set(v8::Number::New(i), Undefined());
                }
            }
            return scope.Close(retval);
        } else if (svt < SVt_PVAV) {
            sv_dump(sv);
            return ThrowException(Exception::Error(String::New("node-perl-simple doesn't support scalarref")));
        } else {
            return scope.Close(Undefined());
        }
    }
};

extern "C" void init(Handle<Object> target) {
    HandleScope scope;
    Local<FunctionTemplate> t = FunctionTemplate::New(NodePerl::New);
    NODE_SET_PROTOTYPE_METHOD(t, "eval", NodePerl::eval);
    t->InstanceTemplate()->SetInternalFieldCount(1);
    target->Set(String::New("Perl"), t->GetFunction());
}

