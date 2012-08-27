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

// src/perlxsi.cc
EXTERN_C void xs_init (pTHX);

using namespace v8;
using namespace node;


#define INTERPRETER_NAME "node-perl-simple"

#define REQ_EXT_ARG(I, VAR) \
if (args.Length() <= (I) || !args[I]->IsExternal()) \
return ThrowException(Exception::TypeError( \
String::New("Argument " #I " must be an external"))); \
Local<External> VAR = Local<External>::Cast(args[I]);

class PerlFoo {
protected:
    PerlInterpreter *my_perl;
public:
    Handle<Value> perl2js(SV * sv) {
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
            return scope.Close(this->perl2js_rv(sv));
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

    Handle<Value> perl2js_rv(SV * rv);
};

class PerlObject: ObjectWrap, PerlFoo {
private:
    SV * sv_;
    PerlInterpreter *my_perl;

public:
    static Persistent<FunctionTemplate> constructor_template;

    static void Init(Handle<Object> target) {
        Local<FunctionTemplate> t = FunctionTemplate::New(PerlObject::New);
        constructor_template = Persistent<FunctionTemplate>::New(t);
        constructor_template->SetClassName(String::NewSymbol("PerlObject"));

        NODE_SET_PROTOTYPE_METHOD(t, "getClassName", PerlObject::getClassName);
        NODE_SET_PROTOTYPE_METHOD(t, "call", PerlObject::call);

        Local<ObjectTemplate> instance_template = constructor_template->InstanceTemplate();
        instance_template->SetInternalFieldCount(1);

        // NODE_SET_PROTOTYPE_METHOD(t, "eval", NodePerl::eval);
        target->Set(String::NewSymbol("PerlObject"), constructor_template->GetFunction());
    }

    PerlObject(SV *sv, PerlInterpreter *myp): sv_(sv), my_perl(myp) {
        SvREFCNT_inc(sv);
    }
    ~PerlObject() {
        SvREFCNT_dec(sv_);
    }
    static Handle<Value> getClassName(const Arguments& args) {
        HandleScope scope;
        return scope.Close(Unwrap<PerlObject>(args.This())->getClassName());
    }
    Handle<Value> getClassName() {
        HandleScope scope;
        return scope.Close(String::New(sv_reftype(SvRV(sv_),TRUE)));
    }
    static Handle<Value> call(const Arguments& args) {
        HandleScope scope;
        return scope.Close(Unwrap<PerlObject>(args.This())->Call(args));
    }
    Handle<Value> Call(const Arguments& args) {
        HandleScope scope;
        if (!args[0]->IsString()) {
            // TODO
            abort();
        }
        v8::String::Utf8Value method(args[0]);

        // TODO: pass arguments
        // TODO: Object#call_list
        // TODO: Perl#call
        // TODO: Perl#call_list

        dSP;
        ENTER;
        SAVETMPS;
        PUSHMARK(SP);
        XPUSHs(sv_);
        PUTBACK;
        call_method(*method, G_SCALAR);
        SPAGAIN;
        SV* retsv = TOPs;
        Handle<Value> retval = this->perl2js(retsv);
        PUTBACK;
        FREETMPS;

        return scope.Close(retval);
    }
    static Handle<Value> New(const Arguments& args) {
        HandleScope scope;

        if (!args.IsConstructCall())
            return args.Callee()->NewInstance();

        REQ_EXT_ARG(0, jssv);
        REQ_EXT_ARG(1, jsmyp);
        SV* sv = static_cast<SV*>(jssv->Value());
        PerlInterpreter* myp = static_cast<PerlInterpreter*>(jsmyp->Value());
        try {
            (new PerlObject(sv, myp))->Wrap(args.Holder());
        } catch (const char *msg) {
            return ThrowException(Exception::Error(String::New(msg)));
        }
        return scope.Close(args.Holder());
    }
};

class NodePerl: ObjectWrap, PerlFoo {

public:
    static void Init(Handle<Object> target) {
        Local<FunctionTemplate> t = FunctionTemplate::New(NodePerl::New);
        NODE_SET_PROTOTYPE_METHOD(t, "_eval", NodePerl::eval);
        t->InstanceTemplate()->SetInternalFieldCount(1);
        target->Set(String::New("Perl"), t->GetFunction());
    }

    NodePerl() {
        char **av = {NULL};
        const char *embedding[] = { "", "-e", "0" };

        // XXX PL_origalen makes segv.
        // PL_origalen = 1; // for save $0
        PERL_SYS_INIT3(0,&av,NULL);
        my_perl = perl_alloc();
        perl_construct(my_perl);

        perl_parse(my_perl, xs_init, 3, (char**)embedding, NULL);
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
        return perl2js(eval_pv(stmt, TRUE));
    }

public:
};

Handle<Value> PerlFoo::perl2js_rv(SV * rv) {
    HandleScope scope;

    SV *sv = SvRV(rv);
    SvGETMAGIC(sv);
    svtype svt = SvTYPE(sv);

    if (SvOBJECT(sv)) { // blessed object.
        Local<Value> arg0 = External::New(rv);
        Local<Value> arg1 = External::New(my_perl);
        Local<Value> args[] = {arg0, arg1};
        v8::Handle<v8::Object> retval(
            PerlObject::constructor_template->GetFunction()->NewInstance(2, args)
        );
        return scope.Close(retval);
    } else if (svt == SVt_PVHV) {
        HV* hval = (HV*)sv;
        HE* he;
        v8::Local<v8::Object> retval = v8::Object::New();
        while ((he = hv_iternext(hval))) {
            retval->Set(
                this->perl2js(hv_iterkeysv(he)),
                this->perl2js(hv_iterval(hval, he))
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
                retval->Set(v8::Number::New(i), this->perl2js(*svp));
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

Persistent<FunctionTemplate> PerlObject::constructor_template;


extern "C" void init(Handle<Object> target) {
    HandleScope scope;

    NodePerl::Init(target);
    PerlObject::Init(target);
}

