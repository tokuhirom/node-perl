#include "nodeutil.h"

#include <string>
#include <vector>
#include <iostream>
#include <dlfcn.h>

#include <EXTERN.h>               /* from the Perl distribution     */
#include <perl.h>                 /* from the Perl distribution     */
#include <embed.h>
#include "ppport.h"
#include "config.h"

#ifdef New
#undef New
#endif

// src/perlxsi.cc
EXTERN_C void xs_init (pTHX);

using namespace v8;
using namespace node;

#define INTERPRETER_NAME "node-perl-simple"

// TODO: pass the NodePerlObject to perl5 world.

class PerlFoo {
protected:
    PerlInterpreter *my_perl;

    PerlFoo(): my_perl(NULL) { }
    PerlFoo(PerlInterpreter *myp): my_perl(myp) { }
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

    SV* js2perl(Handle<Value> val) const;

    Handle<Value> CallMethod2(const Arguments& args, bool in_list_context) {
        ARG_STR(0, method);
        return this->CallMethod2(NULL, *method, 1, args, in_list_context);
    }
    Handle<Value> CallMethod2(SV * self, const char *method, int offset, const Arguments& args, bool in_list_context) {
        HandleScope scope;

        dSP;
        ENTER;
        SAVETMPS;
        PUSHMARK(SP);
        if (self) {
            XPUSHs(self);
        }
        for (int i=offset; i<args.Length(); i++) {
            SV * arg = this->js2perl(args[i]);
            if (!arg) {
                PUTBACK;
                SPAGAIN;
                PUTBACK;
                FREETMPS;
                LEAVE;
                return ThrowException(Exception::Error(String::New("There is no way to pass this value to perl world.")));
            }
            XPUSHs(arg);
        }
        PUTBACK;
        if (in_list_context) {
            int n = self ? call_method(method, G_ARRAY|G_EVAL) : call_pv(method, G_ARRAY|G_EVAL);
            SPAGAIN;
            if (SvTRUE(ERRSV)) {
                POPs;
                PUTBACK;
                FREETMPS;
                LEAVE;
                return ThrowException(this->perl2js(ERRSV));
            } else {
                Handle<Array> retval = Array::New();
                for (int i=0; i<n; i++) {
                    SV* retsv = POPs;
                    retval->Set(n-i-1, this->perl2js(retsv));
                }
                PUTBACK;
                FREETMPS;
                LEAVE;
                return scope.Close(retval);
            }
        } else {
            if (self) {
                call_method(method, G_SCALAR|G_EVAL);
            } else {
                call_pv(method, G_SCALAR|G_EVAL);
            }
            SPAGAIN;
            if (SvTRUE(ERRSV)) {
                POPs;
                PUTBACK;
                FREETMPS;
                LEAVE;
                return ThrowException(this->perl2js(ERRSV));
            } else {
                SV* retsv = TOPs;
                Handle<Value> retval = this->perl2js(retsv);
                PUTBACK;
                FREETMPS;
                LEAVE;
                return scope.Close(retval);
            }
        }
    }

    Handle<Value> perl2js_rv(SV * rv);
};

class NodePerlMethod: ObjectWrap, PerlFoo {
public:
    SV * sv_;
    std::string name_;

    NodePerlMethod(SV *sv, const char * name, PerlInterpreter *myp): sv_(sv), name_(name), PerlFoo(myp) {
        SvREFCNT_inc(sv);
    }
    ~NodePerlMethod() {
        SvREFCNT_dec(sv_);
    }

    static Persistent<FunctionTemplate> constructor_template;

    static void Init(Handle<Object> target) {
        Local<FunctionTemplate> t = FunctionTemplate::New(NodePerlMethod::New);
        constructor_template = Persistent<FunctionTemplate>::New(t);
        constructor_template->SetClassName(String::NewSymbol("NodePerlMethod"));

        /*
        NODE_SET_PROTOTYPE_METHOD(t, "call", NodePerlMethod::call);
        */
        NODE_SET_PROTOTYPE_METHOD(t, "callList", NodePerlMethod::callList);

        Local<ObjectTemplate> instance_template = constructor_template->InstanceTemplate();
        instance_template->SetInternalFieldCount(1);
        instance_template->SetCallAsFunctionHandler(NodePerlMethod::call, Undefined());

        // NODE_SET_PROTOTYPE_METHOD(t, "eval", NodePerl::eval);
        target->Set(String::NewSymbol("NodePerlMethod"), constructor_template->GetFunction());
    }
    static Handle<Value> New(const Arguments& args) {
        HandleScope scope;

        if (!args.IsConstructCall())
            return args.Callee()->NewInstance();

        ARG_EXT(0, jssv);
        ARG_EXT(1, jsmyp);
        ARG_STR(2, jsname);
        SV* sv = static_cast<SV*>(jssv->Value());
        PerlInterpreter* myp = static_cast<PerlInterpreter*>(jsmyp->Value());
        (new NodePerlMethod(sv, *jsname, myp))->Wrap(args.Holder());
        return scope.Close(args.Holder());
    }
    static Handle<Value> call(const Arguments& args) {
        HandleScope scope;
        return scope.Close(Unwrap<NodePerlMethod>(args.This())->Call(args, false));
    }
    static Handle<Value> callList(const Arguments& args) {
        HandleScope scope;
        return scope.Close(Unwrap<NodePerlMethod>(args.This())->Call(args, true));
    }
    Handle<Value> Call(const Arguments& args, bool in_list_context) {
        return this->CallMethod2(this->sv_, name_.c_str(), 0, args, in_list_context);
    }
};

class NodePerlObject: protected ObjectWrap, protected PerlFoo {
protected:
    SV * sv_;

public:
    static Persistent<FunctionTemplate> constructor_template;

    static void Init(Handle<Object> target) {
        Local<FunctionTemplate> t = FunctionTemplate::New(NodePerlObject::New);
        constructor_template = Persistent<FunctionTemplate>::New(t);
        constructor_template->SetClassName(String::NewSymbol("NodePerlObject"));

        NODE_SET_PROTOTYPE_METHOD(t, "getClassName", NodePerlObject::getClassName);

        Local<ObjectTemplate> instance_template = constructor_template->InstanceTemplate();
        instance_template->SetInternalFieldCount(1);
        instance_template->SetNamedPropertyHandler(NodePerlObject::GetNamedProperty);

        // NODE_SET_PROTOTYPE_METHOD(t, "eval", NodePerl::eval);
        target->Set(String::NewSymbol("NodePerlObject"), constructor_template->GetFunction());
    }

    static Handle<Value> GetNamedProperty(Local<String> name,
                          const AccessorInfo &info) {
        HandleScope scope;

        if (info.This()->InternalFieldCount() < 1 || info.Data().IsEmpty()) {
            return THROW_TYPE_ERROR("SetNamedProperty intercepted "
                            "by non-Proxy object");
        }

        return scope.Close(Unwrap<NodePerlObject>(info.This())->getNamedProperty(name));
    }
    Handle<Value> getNamedProperty(Local<String> name) {
        HandleScope scope;
        v8::String::Utf8Value stmt(name);
        Local<Value> arg0 = External::New(sv_);
        Local<Value> arg1 = External::New(my_perl);
        Local<Value> arg2 = name;
        Local<Value> args[] = {arg0, arg1, arg2};
        v8::Handle<v8::Object> retval(
            NodePerlMethod::constructor_template->GetFunction()->NewInstance(3, args)
        );
        return scope.Close(retval);
    }

    NodePerlObject(SV *sv, PerlInterpreter *myp): sv_(sv), PerlFoo(myp) {
        SvREFCNT_inc(sv);
    }
    ~NodePerlObject() {
        SvREFCNT_dec(sv_);
    }
    static Handle<Value> getClassName(const Arguments& args) {
        HandleScope scope;
        return scope.Close(Unwrap<NodePerlObject>(args.This())->getClassName());
    }
    Handle<Value> getClassName() {
        HandleScope scope;
        if (SvPOK(sv_)) {
            STRLEN len;
            const char * str = SvPV(sv_, len);
            return scope.Close(String::New(str, len));
        } else {
            return scope.Close(String::New(sv_reftype(SvRV(sv_),TRUE)));
        }
    }
    static SV* getSV(Handle<Object> val) {
        return Unwrap<NodePerlObject>(val)->sv_;
    }
    static Handle<Value> blessed(Handle<Object> val) {
        return Unwrap<NodePerlObject>(val)->blessed();
    }
    Handle<Value> blessed() {
        HandleScope scope;
        if(!(SvROK(sv_) && SvOBJECT(SvRV(sv_)))) {
            return scope.Close(Undefined());
        }
        return scope.Close(String::New(sv_reftype(SvRV(sv_),TRUE)));
    }

    static Handle<Value> New(const Arguments& args) {
        HandleScope scope;

        if (!args.IsConstructCall())
            return args.Callee()->NewInstance();

        ARG_EXT(0, jssv);
        ARG_EXT(1, jsmyp);
        SV* sv = static_cast<SV*>(jssv->Value());
        PerlInterpreter* myp = static_cast<PerlInterpreter*>(jsmyp->Value());
        (new NodePerlObject(sv, myp))->Wrap(args.Holder());
        return scope.Close(args.Holder());
    }
};

class NodePerlClass: NodePerlObject {
public:
    static Persistent<FunctionTemplate> constructor_template;

    static void Init(Handle<Object> target) {
        Local<FunctionTemplate> t = FunctionTemplate::New(NodePerlClass::New);
        constructor_template = Persistent<FunctionTemplate>::New(t);
        constructor_template->SetClassName(String::NewSymbol("NodePerlClass"));

        Local<ObjectTemplate> instance_template = constructor_template->InstanceTemplate();
        instance_template->SetInternalFieldCount(1);
        instance_template->SetNamedPropertyHandler(NodePerlObject::GetNamedProperty);

        target->Set(String::NewSymbol("NodePerlClass"), constructor_template->GetFunction());
    }
};

class NodePerl: ObjectWrap, PerlFoo {

public:
    static void Init(Handle<Object> target) {
        Local<FunctionTemplate> t = FunctionTemplate::New(NodePerl::New);
        NODE_SET_PROTOTYPE_METHOD(t, "evaluate", NodePerl::evaluate);
        NODE_SET_PROTOTYPE_METHOD(t, "getClass", NodePerl::getClass);
        NODE_SET_PROTOTYPE_METHOD(t, "call", NodePerl::call);
        NODE_SET_PROTOTYPE_METHOD(t, "callList", NodePerl::callList);
        t->InstanceTemplate()->SetInternalFieldCount(1);
        NODE_SET_METHOD(t, "blessed", NodePerl::blessed);
        target->Set(String::New("Perl"), t->GetFunction());
    }

    NodePerl() : PerlFoo() {
        // std::cerr << "[Construct Perl]" << std::endl;

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
        // std::cerr << "[Destruct Perl]" << std::endl;
        PL_perl_destruct_level = 2;
        perl_destruct(my_perl);
        perl_free(my_perl);
    }

    static Handle<Value> New(const Arguments& args) {
        HandleScope scope;

        if (!args.IsConstructCall())
            return args.Callee()->NewInstance();
        (new NodePerl())->Wrap(args.Holder());
        return scope.Close(args.Holder());
    }

    static Handle<Value> blessed(const Arguments& args) {
        HandleScope scope;
        ARG_OBJ(0, jsobj);

        if (NodePerlObject::constructor_template->HasInstance(jsobj)) {
            return scope.Close(NodePerlObject::blessed(jsobj));
        } else {
            return scope.Close(Undefined());
        }
    }

    static Handle<Value> evaluate(const Arguments& args) {
        HandleScope scope;
        if (!args[0]->IsString()) {
            return ThrowException(Exception::Error(String::New("Arguments must be string")));
        }
        v8::String::Utf8Value stmt(args[0]);

        Handle<Value> retval = Unwrap<NodePerl>(args.This())->evaluate(*stmt);
        return scope.Close(retval);
    }

    static Handle<Value> getClass(const Arguments& args) {
        HandleScope scope;
        if (!args[0]->IsString()) {
            return ThrowException(Exception::Error(String::New("Arguments must be string")));
        }
        v8::String::Utf8Value stmt(args[0]);

        Handle<Value> retval = Unwrap<NodePerl>(args.This())->getClass(*stmt);
        return scope.Close(retval);
    }

    static Handle<Value> call(const Arguments& args) {
        HandleScope scope;
        return scope.Close(Unwrap<NodePerl>(args.This())->CallMethod2(args, false));
    }
    static Handle<Value> callList(const Arguments& args) {
        HandleScope scope;
        return scope.Close(Unwrap<NodePerl>(args.This())->CallMethod2(args, true));
    }

private:
    Handle<Value> getClass(const char *name) {
        HandleScope scope;
        Local<Value> arg0 = External::New(sv_2mortal(newSVpv(name, 0)));
        Local<Value> arg1 = External::New(my_perl);
        Local<Value> args[] = {arg0, arg1};
        v8::Handle<v8::Object> retval(
            NodePerlClass::constructor_template->GetFunction()->NewInstance(2, args)
        );
        return scope.Close(retval);
    }
    Handle<Value> evaluate(const char *stmt) {
        return perl2js(eval_pv(stmt, TRUE));
    }

public:
};

SV* PerlFoo::js2perl(Handle<Value> val) const {
    if (val->IsTrue()) {
        return &PL_sv_yes;
    } else if (val->IsFalse()) {
        return &PL_sv_no;
    } else if (val->IsString()) {
        v8::String::Utf8Value method(val);
        return sv_2mortal(newSVpv(*method, method.length()));
    } else if (val->IsArray()) {
        Handle<Array> jsav = Handle<Array>::Cast(val);
        AV * av = newAV();
        av_extend(av, jsav->Length());
        for (int i=0; i<jsav->Length(); ++i) {
            SV * elem = this->js2perl(jsav->Get(i));
            av_push(av, SvREFCNT_inc(elem));
        }
        return sv_2mortal(newRV_noinc((SV*)av));
    } else if (val->IsObject()) {
        Handle<Object> jsobj = Handle<Object>::Cast(val);
        if (NodePerlObject::constructor_template->HasInstance(jsobj)) {
            SV * ret = NodePerlObject::getSV(jsobj);
            return ret;
        } else if (NodePerlClass::constructor_template->HasInstance(jsobj)) {
            SV * ret = NodePerlObject::getSV(jsobj);
            return ret;
        } else {
            Handle<Array> keys = jsobj->GetOwnPropertyNames();
            HV * hv = newHV();
            hv_ksplit(hv, keys->Length());
            for (int i=0; i<keys->Length(); ++i) {
                SV * k = this->js2perl(keys->Get(i));
                SV * v = this->js2perl(jsobj->Get(keys->Get(i)));
                hv_store_ent(hv, k, v, 0);
                // SvREFCNT_dec(k);
            }
            return sv_2mortal(newRV_inc((SV*)hv));
        }
    } else if (val->IsInt32()) {
        return sv_2mortal(newSViv(val->Int32Value()));
    } else if (val->IsUint32()) {
        return sv_2mortal(newSVuv(val->Uint32Value()));
    } else if (val->IsNumber()) {
        return sv_2mortal(newSVnv(val->NumberValue()));
    } else {
        // RegExp, Date, External
        return NULL;
    }
}

Handle<Value> PerlFoo::perl2js_rv(SV * rv) {
    HandleScope scope;

    SV *sv = SvRV(rv);
    SvGETMAGIC(sv);
    svtype svt = (svtype)SvTYPE(sv);

    if (SvOBJECT(sv)) { // blessed object.
        Local<Value> arg0 = External::New(rv);
        Local<Value> arg1 = External::New(my_perl);
        Local<Value> args[] = {arg0, arg1};
        v8::Handle<v8::Object> retval(
            NodePerlObject::constructor_template->GetFunction()->NewInstance(2, args)
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

Persistent<FunctionTemplate> NodePerlObject::constructor_template;
Persistent<FunctionTemplate> NodePerlMethod::constructor_template;
Persistent<FunctionTemplate> NodePerlClass::constructor_template;

/**
  * Load lazily libperl for dynamic loaded xs.
  * I don't know the better way to resolve symbols in xs.
  * patches welcome.
  *
  * And this code is not portable.
  * patches welcome.
  */
static Handle<Value> InitPerl(const Arguments& args) {
    HandleScope scope;

    void *lib = dlopen(LIBPERL, RTLD_LAZY|RTLD_GLOBAL);
    if (lib) {
        dlclose(lib);
        return scope.Close(Undefined());
    } else {
        std::cerr << dlerror() << std::endl;
        return scope.Close(Undefined());
        // return ThrowException(Exception::Error(String::New(dlerror())));
    }
}

extern "C" void init(Handle<Object> target) {
    {
        Handle<FunctionTemplate> t = FunctionTemplate::New(InitPerl);
        target->Set(String::New("InitPerl"), t->GetFunction());
    }

    NodePerl::Init(target);
    NodePerlObject::Init(target);
    NodePerlClass::Init(target);
    NodePerlMethod::Init(target);
}

NODE_MODULE(perl, init)
