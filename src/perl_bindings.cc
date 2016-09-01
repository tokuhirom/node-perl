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
    v8::Local<v8::Value> perl2js(SV * sv) {
        Nan::EscapableHandleScope scope;

        // see xs-src/pack.c in msgpack-perl
        SvGETMAGIC(sv);

        if (SvPOKp(sv)) {
            STRLEN len;
            const char *s = SvPV(sv, len);
            return scope.Escape(Nan::New(s, len).ToLocalChecked());
        } else if (SvNOK(sv)) {
            return scope.Escape(Nan::New(SvNVX(sv)));
        } else if (SvIOK(sv)) {
            return scope.Escape(Nan::New((double)SvIVX(sv)));
        } else if (SvROK(sv)) {
            return scope.Escape(this->perl2js_rv(sv));
        } else if (!SvOK(sv)) {
            return scope.Escape(Nan::Undefined());
        } else if (isGV(sv)) {
            std::cerr << "Cannot pass GV to v8 world" << std::endl;
            return scope.Escape(Nan::Undefined());
        } else {
            sv_dump(sv);
            Nan::ThrowError("node-perl-simple doesn't support this type");
            return scope.Escape(Nan::Undefined());
        }
        // TODO: return callback function for perl code.
        // Perl callbacks should be managed by objects.
        // TODO: Handle async.
    }

    SV* js2perl(v8::Local<v8::Value> val) const;

    v8::Local<v8::Value> CallMethod2(const Nan::FunctionCallbackInfo<v8::Value>& args, bool in_list_context) {
        ARG_STR(0, method);
        return this->CallMethod2(NULL, *method, 1, args, in_list_context);
    }
    v8::Local<v8::Value> CallMethod2(SV * self, const char *method, int offset, const Nan::FunctionCallbackInfo<v8::Value>& args, bool in_list_context) {
        Nan::EscapableHandleScope scope;

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
                Nan::ThrowError("There is no way to pass this value to perl world.");
                return scope.Escape(Nan::Undefined());
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
                Nan::ThrowError(this->perl2js(ERRSV));
                return scope.Escape(Nan::Undefined());
            } else {
                Handle<Array> retval = Array::New();
                for (int i=0; i<n; i++) {
                    SV* retsv = POPs;
                    retval->Set(n-i-1, this->perl2js(retsv));
                }
                PUTBACK;
                FREETMPS;
                LEAVE;
                return scope.Escape(retval);
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
                Nan::ThrowError(this->perl2js(ERRSV));
                return scope.Escape(Nan::Undefined());
            } else {
                SV* retsv = TOPs;
                v8::Local<v8::Value> retval = this->perl2js(retsv);
                PUTBACK;
                FREETMPS;
                LEAVE;
                return scope.Escape(retval);
            }
        }
    }

    v8::Local<v8::Value> perl2js_rv(SV * rv);
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

    static Nan::Persistent<v8::FunctionTemplate> constructor;

    static void Init(v8::Local<v8::Object> target) {
        v8::Local<v8::FunctionTemplate> t = Nan::New<v8::FunctionTemplate>(New);
        t->SetClassName(Nan::New("NodePerlMethod").ToLocalChecked());
        t->InstanceTemplate()->SetInternalFieldCount(1);

        Nan::SetPrototypeMethod(t, "callList", NodePerlMethod::callList);

        target->Set(Nan::New("NodePerlMethod").ToLocalChecked(), t->GetFunction());
    }
    static v8::Local<v8::Value> New(const Nan::FunctionCallbackInfo<v8::Value>& args) {
        Nan::EscapableHandleScope scope;

        if (!args.IsConstructCall())
            return args.Callee()->NewInstance();

        ARG_EXT(0, jssv);
        ARG_EXT(1, jsmyp);
        ARG_STR(2, jsname);
        SV* sv = static_cast<SV*>(jssv->Value());
        PerlInterpreter* myp = static_cast<PerlInterpreter*>(jsmyp->Value());
        (new NodePerlMethod(sv, *jsname, myp))->Wrap(args.Holder());
        return scope.Escape(args.Holder());
    }
    static v8::Local<v8::Value> call(const Nan::FunctionCallbackInfo<v8::Value>& args) {
        Nan::EscapableHandleScope scope;
        return scope.Escape(Unwrap<NodePerlMethod>(args.This())->Call(args, false));
    }
    static v8::Local<v8::Value> callList(const Nan::FunctionCallbackInfo<v8::Value>& args) {
        Nan::EscapableHandleScope scope;
        return scope.Escape(Unwrap<NodePerlMethod>(args.This())->Call(args, true));
    }

    v8::Local<v8::Value> Call(const Nan::FunctionCallbackInfo<v8::Value>& args, bool in_list_context) {
        return this->CallMethod2(this->sv_, name_.c_str(), 0, args, in_list_context);
    }
};

class NodePerlObject: protected ObjectWrap, protected PerlFoo {
protected:
    SV * sv_;

public:
    static Nan::Persistent<v8::FunctionTemplate> constructor;

    static void Init(v8::Local<v8::Object> target) {
        v8::Local<v8::FunctionTemplate> t = Nan::New<v8::FunctionTemplate>(New);
        t->SetClassName(Nan::New("NodePerlObject").ToLocalChecked());
        t->InstanceTemplate()->SetInternalFieldCount(1);

        Nan::SetPrototypeMethod(t, "getClassName", NodePerlObject::getClassName);

        target->Set(Nan::New("NodePerlObject").ToLocalChecked(), t->GetFunction());
    }

    static v8::Local<v8::Value> GetNamedProperty(Local<String> name,
                          const AccessorInfo &info) {
        Nan::EscapableHandleScope scope;

        if (info.This()->InternalFieldCount() < 1 || info.Data().IsEmpty()) {
            Nan::ThrowError("SetNamedProperty intercepted by non-Proxy object");
            return scope.Escape(Nan::Undefined());
        }

        return scope.Escape(Unwrap<NodePerlObject>(info.This())->getNamedProperty(name));
    }
    v8::Local<v8::Value> getNamedProperty(Local<String> name) {
        Nan::EscapableHandleScope scope;
        v8::String::Utf8Value stmt(name);
        Local<Value> arg0 = External::Cast(sv_)->Value();
        Local<Value> arg1 = External::Cast(my_perl)->Value();
        Local<Value> arg2 = name;
        Local<Value> args[] = {arg0, arg1, arg2};
        v8::Handle<v8::Object> retval(
            Nan::New(NodePerlMethod::constructor)->GetFunction()->NewInstance(3, args)
        );
        return scope.Escape(retval);
    }

    NodePerlObject(SV *sv, PerlInterpreter *myp): sv_(sv), PerlFoo(myp) {
        SvREFCNT_inc(sv);
    }
    ~NodePerlObject() {
        SvREFCNT_dec(sv_);
    }
    static v8::Local<v8::Value> getClassName(const Nan::FunctionCallbackInfo<v8::Value>& args) {
        Nan::EscapableHandleScope scope;
        return scope.Escape(Unwrap<NodePerlObject>(args.This())->getClassName());
    }
    v8::Local<v8::Value> getClassName() {
        Nan::EscapableHandleScope scope;
        if (SvPOK(sv_)) {
            STRLEN len;
            const char * str = SvPV(sv_, len);
            return scope.Escape(Nan::New(str, len).ToLocalChecked());
        } else {
            return scope.Escape(Nan::New(sv_reftype(SvRV(sv_),TRUE)).ToLocalChecked());
        }
    }
    static SV* getSV(v8::Local<v8::Object> val) {
        return Unwrap<NodePerlObject>(val)->sv_;
    }
    static v8::Local<v8::Value> blessed(v8::Local<v8::Object> val) {
        return Unwrap<NodePerlObject>(val)->blessed();
    }
    v8::Local<v8::Value> blessed() {
        Nan::EscapableHandleScope scope;
        if(!(SvROK(sv_) && SvOBJECT(SvRV(sv_)))) {
            return scope.Escape(Nan::Undefined());
        }
        return scope.Escape(Nan::New(sv_reftype(SvRV(sv_),TRUE).ToLocalChecked()));
    }

    static v8::Local<v8::Value> New(const Nan::FunctionCallbackInfo<v8::Value>& args) {
        Nan::EscapableHandleScope scope;

        if (!args.IsConstructCall())
            return args.Callee()->NewInstance();

        ARG_EXT(0, jssv);
        ARG_EXT(1, jsmyp);
        SV* sv = static_cast<SV*>(jssv->Value());
        PerlInterpreter* myp = static_cast<PerlInterpreter*>(jsmyp->Value());
        (new NodePerlObject(sv, myp))->Wrap(args.Holder());
        return scope.Escape(args.Holder());
    }
};

class NodePerlClass: NodePerlObject {
public:
    static Nan::Persistent<v8::FunctionTemplate> constructor;

    static void Init(v8::Local<v8::Object> target) {
        v8::Local<v8::FunctionTemplate> t = Nan::New<v8::FunctionTemplate>(New);
        t->SetClassName(Nan::New("NodePerlClass").ToLocalChecked());
        t->InstanceTemplate()->SetInternalFieldCount(1);

        target->Set(Nan::New("NodePerlClass").ToLocalChecked(), t->GetFunction());
    }
};

class NodePerl: ObjectWrap, PerlFoo {

public:
    static void Init(v8::Local<v8::Object> target) {
        v8::Local<v8::FunctionTemplate> t = Nan::New<v8::FunctionTemplate>(New);
        t->SetClassName(Nan::New("Perl").ToLocalChecked());

        Nan::SetPrototypeMethod(t, "evaluate", NodePerl::evaluate);
        Nan::SetPrototypeMethod(t, "getClass", NodePerl::getClass);
        Nan::SetPrototypeMethod(t, "call", NodePerl::call);
        Nan::SetPrototypeMethod(t, "callList", NodePerl::callList);

        t->InstanceTemplate()->SetInternalFieldCount(1);

        Nan::SetPrototypeMethod(t, "blessed", NodePerl::blessed);

        target->Set(Nan::New("Perl").ToLocalChecked(), t->GetFunction());
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

    static v8::Local<v8::Value> New(const Nan::FunctionCallbackInfo<v8::Value>& args) {
        Nan::EscapableHandleScope scope;

        if (!args.IsConstructCall())
            return args.Callee()->NewInstance();
        (new NodePerl())->Wrap(args.Holder());
        return scope.Escape(args.Holder());
    }

    static v8::Local<v8::Value> blessed(const Nan::FunctionCallbackInfo<v8::Value>& args) {
        Nan::EscapableHandleScope scope;
        ARG_OBJ(0, jsobj);

        if (Nan::New(NodePerlClass::constructor)->HasInstance(jsobj)) {
            return scope.Escape(NodePerlObject::blessed(jsobj));
        } else {
            return scope.Escape(Nan::Undefined());
        }
    }

    static v8::Local<v8::Value> evaluate(const Nan::FunctionCallbackInfo<v8::Value>& args) {
        Nan::EscapableHandleScope scope;
        if (!args[0]->IsString()) {
            Nan::ThrowError("Arguments must be string");
            return scope.Escape(Nan::Undefined());
	}
        v8::String::Utf8Value stmt(args[0]);

        v8::Local<v8::Value> retval = Unwrap<NodePerl>(args.This())->evaluate(*stmt);
        return scope.Escape(retval);
    }

    static v8::Local<v8::Value> getClass(const Nan::FunctionCallbackInfo<v8::Value>& args) {
        Nan::EscapableHandleScope scope;
        if (!args[0]->IsString()) {
            Nan::ThrowError("Arguments must be string");
            return scope.Escape(Nan::Undefined());
        }
        v8::String::Utf8Value stmt(args[0]);

        v8::Local<v8::Value> retval = Unwrap<NodePerl>(args.This())->getClass(*stmt);
        return scope.Escape(retval);
    }

    static v8::Local<v8::Value> call(const Nan::FunctionCallbackInfo<v8::Value>& args) {
        Nan::EscapableHandleScope scope;
        return scope.Escape(Unwrap<NodePerl>(args.This())->CallMethod2(args, false));
    }
    static v8::Local<v8::Value> callList(const Nan::FunctionCallbackInfo<v8::Value>& args) {
        Nan::EscapableHandleScope scope;
        return scope.Escape(Unwrap<NodePerl>(args.This())->CallMethod2(args, true));
    }

private:
    v8::Local<v8::Value> getClass(const char *name) {
        Nan::EscapableHandleScope scope;
        Local<Value> arg0 = External::Cast(sv_2mortal(newSVpv(name, 0)))->Value();
        Local<Value> arg1 = External::Cast(my_perl)->Value();
        Local<Value> args[] = {arg0, arg1};
        v8::Handle<v8::Object> retval(
            Nan::New(NodePerlClass::constructor)->GetFunction()->NewInstance(2, args)
        );
        return scope.Escape(retval);
    }
    v8::Local<v8::Value> evaluate(const char *stmt) {
        return perl2js(eval_pv(stmt, TRUE));
    }

public:
};

SV* PerlFoo::js2perl(v8::Local<v8::Value> val) const {
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
        v8::Local<v8::Object> jsobj = v8::Local<v8::Object>::Cast(val);
        if (Nan::New(NodePerlObject::constructor)->HasInstance(jsobj)) {
            SV * ret = NodePerlObject::getSV(jsobj);
            return ret;
        } else if (Nan::New(NodePerlClass::constructor)->HasInstance(jsobj)) {
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

v8::Local<v8::Value> PerlFoo::perl2js_rv(SV * rv) {
    Nan::EscapableHandleScope scope;

    SV *sv = SvRV(rv);
    SvGETMAGIC(sv);
    svtype svt = (svtype)SvTYPE(sv);

    if (SvOBJECT(sv)) { // blessed object.
        Local<Value> arg0 = External::Cast(rv)->Value();
        Local<Value> arg1 = External::Cast(my_perl)->Value();
        Local<Value> args[] = {arg0, arg1};
        v8::Handle<v8::Object> retval(
            Nan::New(NodePerlObject::constructor)->GetFunction()->NewInstance(2, args)
        );
        return scope.Escape(retval);
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
        return scope.Escape(retval);
    } else if (svt == SVt_PVAV) {
        AV* ary = (AV*)sv;
        v8::Local<v8::Array> retval = v8::Array::New();
        int len = av_len(ary) + 1;
        for (int i=0; i<len; ++i) {
            SV** svp = av_fetch(ary, i, 0);
            if (svp) {
                retval->Set(Nan::New(i), this->perl2js(*svp));
            } else {
                retval->Set(Nan::New(i), Nan::Undefined());
            }
        }
        return scope.Escape(retval);
    } else if (svt < SVt_PVAV) {
        sv_dump(sv);
        Nan::ThrowError("node-perl-simple doesn't support scalarref");
        return scope.Escape(Nan::Undefined());
    } else {
        return scope.Escape(Nan::Undefined());
    }
}

Nan::Persistent<v8::FunctionTemplate> NodePerlObject::constructor;
Nan::Persistent<v8::FunctionTemplate> NodePerlMethod::constructor;
Nan::Persistent<v8::FunctionTemplate> NodePerlClass::constructor;

/**
  * Load lazily libperl for dynamic loaded xs.
  * I don't know the better way to resolve symbols in xs.
  * patches welcome.
  *
  * And this code is not portable.
  * patches welcome.
  */
static v8::Local<v8::Value> InitPerl(const Nan::FunctionCallbackInfo<v8::Value>& args) {
    Nan::EscapableHandleScope scope;

    void *lib = dlopen(LIBPERL, RTLD_LAZY|RTLD_GLOBAL);
    if (lib) {
        dlclose(lib);
        return scope.Escape(Nan::Undefined());
    } else {
        std::cerr << dlerror() << std::endl;
        return scope.Escape(Nan::Undefined());
    }
}

extern "C" void init(v8::Local<v8::Object> target) {
    {
        Handle<FunctionTemplate> t = FunctionTemplate::New(InitPerl);
        target->Set(Nan::New("InitPerl").ToLocalChecked(), t->GetFunction());
    }

    NodePerl::Init(target);
    NodePerlObject::Init(target);
    NodePerlClass::Init(target);
    NodePerlMethod::Init(target);
}

NODE_MODULE(perl, init)
