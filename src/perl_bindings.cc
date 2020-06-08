#include <nan.h>
#include <v8.h>

#include <EXTERN.h> /* from the Perl distribution     */
#include <perl.h> /* from the Perl distribution     */
#include <embed.h>
#include <config.h>
#include <ppport.h>
#include <iostream>

#include <string>

#include "nodeutil.h"

#ifdef WIN32
#define PSAPI_VERSION 1
#include <windows.h>
#include <psapi.h>
#include <stdio.h>
#else
#include <dlfcn.h>
#endif

#ifdef New
#undef New
#endif

// src/perlxsi.cc
EXTERN_C void xs_init(pTHX);

class NodePerl;

#define INTERPRETER_NAME "node-perl-simple"

// TODO: pass the NodePerlObject to perl5 world.

class PerlFoo
{
  protected:
    PerlInterpreter *my_perl;

    PerlFoo(): my_perl(NULL) {}

    PerlFoo(PerlInterpreter *myp): my_perl(myp) {}

  public:
    v8::Local<v8::Value> perl2js(SV *sv)
    {
        Nan::EscapableHandleScope scope;

        // see xs-src/pack.c in msgpack-perl
        SvGETMAGIC(sv);

        if (SvPOKp(sv))
        {
            STRLEN len;

            const char *s = SvPV(sv, len);

            return scope.Escape(Nan::New(s, len).ToLocalChecked());
        }
        else if (SvNOK(sv))
        {
            return scope.Escape(Nan::New<v8::Number>(SvNVX(sv)));
        }
        else if (SvIOK(sv))
        {
            return scope.Escape(Nan::New<v8::Number>(SvIVX(sv)));
        }
        else if (SvROK(sv))
        {
            return scope.Escape(this->perl2js_rv(sv));
        }
        else if (!SvOK(sv))
        {
            return scope.Escape(Nan::Undefined());
        }
        else if (isGV(sv))
        {
            std::cerr << "Cannot pass GV to v8 world" << std::endl;

            return scope.Escape(Nan::Undefined());
        }
        else
        {
            sv_dump(sv);

            Nan::ThrowError(
                v8::Exception::TypeError(Nan::New("node-perl-simple doesn't support this type").ToLocalChecked()));

            return scope.Escape(Nan::Undefined());
        }
        // TODO: return callback function for perl code.
        // Perl callbacks should be managed by objects.
        // TODO: Handle async.
    }

    SV *js2perl(v8::Local<v8::Value> val) const;

    v8::Local<v8::Value> CallMethod2(const Nan::FunctionCallbackInfo<v8::Value> &info, bool in_list_context)
    {
        const auto &args = info;

        ARG_STR(0, method);

        return this->CallMethod2(NULL, *method, 1, info, in_list_context);
    }

    v8::Local<v8::Value> CallMethod2(
        SV *self,
        const char *method,
        int offset,
        const Nan::FunctionCallbackInfo<v8::Value> &args,
        bool in_list_context)
    {
        Nan::EscapableHandleScope scope;

        dSP;

        ENTER;

        SAVETMPS;

        PUSHMARK(SP);

        if (self)
        {
            XPUSHs(self);
        }

        for (int i = offset; i < args.Length(); i++)
        {
            SV *arg = this->js2perl(args[i]);

            if (!arg)
            {
                PUTBACK;

                SPAGAIN;

                PUTBACK;

                FREETMPS;

                LEAVE;

                Nan::ThrowError(v8::Exception::Error(
                    Nan::New("There is no way to pass this value to perl world.").ToLocalChecked()));

                return scope.Escape(Nan::Undefined());
            }


            XPUSHs(arg);
        }

        PUTBACK;

        if (in_list_context)
        {
            int n = self ? call_method(method, G_ARRAY | G_EVAL) : call_pv(method, G_ARRAY | G_EVAL);

            SPAGAIN;

            if (SvTRUE(ERRSV))
            {
                POPs;

                PUTBACK;

                FREETMPS;

                LEAVE;

                Nan::ThrowError(this->perl2js(ERRSV));

                return scope.Escape(Nan::Undefined());
            }
            else
            {
                v8::Local<v8::Array> retval = Nan::New<v8::Array>();

                for (int i = 0; i < n; i++)
                {
                    SV *retsv = POPs;

                    Nan::Set(retval, n - i - 1, this->perl2js(retsv));
                }

                PUTBACK;

                FREETMPS;

                LEAVE;

                return scope.Escape(retval);
            }
        }
        else
        {
            if (self)
            {
                call_method(method, G_SCALAR | G_EVAL);
            }
            else
            {
                call_pv(method, G_SCALAR | G_EVAL);
            }

            SPAGAIN;

            if (SvTRUE(ERRSV))
            {
                POPs;

                PUTBACK;

                FREETMPS;

                LEAVE;

                Nan::ThrowError(this->perl2js(ERRSV));

                return scope.Escape(Nan::Undefined());
            }
            else
            {
                SV *retsv = TOPs;

                v8::Local<v8::Value> retval = this->perl2js(retsv);

                PUTBACK;

                FREETMPS;

                LEAVE;

                return scope.Escape(retval);
            }
        }
    }

    v8::Local<v8::Value> perl2js_rv(SV *rv);
};

class NodePerlMethod : public Nan::ObjectWrap, public PerlFoo
{
  public:
    SV *sv_;

    std::string name_;

    NodePerlMethod(SV *sv, const char *name, PerlInterpreter *myp): PerlFoo(myp), sv_(sv), name_(name)
    {
        SvREFCNT_inc(sv);
    }

    ~NodePerlMethod()
    {
        SvREFCNT_dec(sv_);
    }

    static Nan::Persistent<v8::FunctionTemplate> constructor_template;

    static inline Nan::Persistent<v8::Function> &constructor()
    {
        static Nan::Persistent<v8::Function> my_constructor;

        return my_constructor;
    }

    static NAN_MODULE_INIT(Init)
    {
        // Prepare constructor template
        v8::Local<v8::FunctionTemplate> tpl = Nan::New<v8::FunctionTemplate>(NodePerlMethod::New);

        tpl->SetClassName(Nan::New("NodePerlMethod").ToLocalChecked());

        tpl->InstanceTemplate()->SetInternalFieldCount(2);

        Nan::SetCallAsFunctionHandler(tpl->InstanceTemplate(), NodePerlMethod::call);

        Nan::SetPrototypeMethod(tpl, "call", NodePerlMethod::call);

        Nan::SetPrototypeMethod(tpl, "callList", NodePerlMethod::callList);

        // Nan::SetPrototypeMethod(tpl, "eval", NodePerl::evaluate);

        constructor_template.Reset(tpl);

        constructor().Reset(Nan::GetFunction(tpl).ToLocalChecked());

        Nan::Set(target, Nan::New("NodePerlMethod").ToLocalChecked(), Nan::GetFunction(tpl).ToLocalChecked());
    }

    static NAN_METHOD(New)
    {
        if (info.IsConstructCall())
        {
            const Nan::FunctionCallbackInfo<v8::Value> &args = info;

            ARG_EXT(0, jssv);

            ARG_EXT(1, jsmyp);

            ARG_STR(2, jsname);

            SV *sv = static_cast<SV *>(jssv->Value());

            PerlInterpreter *myp = static_cast<PerlInterpreter *>(jsmyp->Value());

            NodePerlMethod *obj = new NodePerlMethod(sv, *jsname, myp);

            obj->Wrap(info.This());

            info.GetReturnValue().Set(info.This());
        }
        else
        {
            const int argc = 3;

            v8::Local<v8::Value> argv[argc] = {info[0], info[1], info[2]};

            info.GetReturnValue().Set(Nan::NewInstance(Nan::New(constructor()), argc, argv).ToLocalChecked());
        }
    }

    static NAN_METHOD(call)
    {
        return info.GetReturnValue().Set(Nan::ObjectWrap::Unwrap<NodePerlMethod>(info.This())->Call(info, false));
    }

    static NAN_METHOD(callList)
    {
        return info.GetReturnValue().Set(Nan::ObjectWrap::Unwrap<NodePerlMethod>(info.This())->Call(info, true));
    }

    v8::Local<v8::Value> Call(const Nan::FunctionCallbackInfo<v8::Value> &info, bool in_list_context)
    {
        return this->CallMethod2(this->sv_, name_.c_str(), 0, info, in_list_context);
    }
};

class NodePerlObject : public Nan::ObjectWrap, public PerlFoo
{
  protected:
    SV *sv_;

  public:
    static Nan::Persistent<v8::FunctionTemplate> constructor_template;

    static inline Nan::Persistent<v8::Function> &constructor()
    {
        static Nan::Persistent<v8::Function> my_constructor;

        return my_constructor;
    }

    static NAN_MODULE_INIT(Init)
    {
        // Prepare constructor template
        v8::Local<v8::FunctionTemplate> tpl = Nan::New<v8::FunctionTemplate>(New);

        tpl->SetClassName(Nan::New("NodePerlObject").ToLocalChecked());

        tpl->InstanceTemplate()->SetInternalFieldCount(1);

        Nan::SetNamedPropertyHandler(tpl->InstanceTemplate(), NodePerlObject::GetNamedProperty);

        Nan::SetPrototypeMethod(tpl, "getClassName", NodePerlObject::getClassName);

        constructor_template.Reset(tpl);

        constructor().Reset(Nan::GetFunction(tpl).ToLocalChecked());

        Nan::Set(target, Nan::New("NodePerlObject").ToLocalChecked(), Nan::GetFunction(tpl).ToLocalChecked());
    }

    static void GetNamedProperty(v8::Local<v8::String> propertyName, const Nan::PropertyCallbackInfo<v8::Value> &info)
    {
        if (info.This()->InternalFieldCount() < 1 || info.Data().IsEmpty())
        {
            Nan::ThrowError(
                v8::Exception::Error(Nan::New("SetNamedProperty intercepted by non-Proxy object").ToLocalChecked()));

            info.GetReturnValue().Set(Nan::Undefined());

            return;
        }

        info.GetReturnValue().Set(Nan::ObjectWrap::Unwrap<NodePerlObject>(info.This())->getNamedProperty(propertyName));

        return;
    }

    v8::Local<v8::Value> getNamedProperty(const v8::Local<v8::String> propertyName) const
    {
        Nan::EscapableHandleScope scope;

        Nan::Utf8String stmt(propertyName);

        v8::Local<v8::Value> arg0 = Nan::New<v8::External>(sv_);

        v8::Local<v8::Value> arg1 = Nan::New<v8::External>(my_perl);

        v8::Local<v8::Value> arg2 = propertyName;

        v8::Local<v8::Value> args[] = {arg0, arg1, arg2};

        v8::Local<v8::Object> retval(
            Nan::NewInstance(
                Nan::GetFunction(Nan::New<v8::FunctionTemplate>(NodePerlMethod::constructor_template)).ToLocalChecked(),
                3,
                args)
                .ToLocalChecked());

        return scope.Escape(retval);
    }

    NodePerlObject(SV *sv, PerlInterpreter *myp): PerlFoo(myp), sv_(sv)
    {
        SvREFCNT_inc(sv);
    }

    ~NodePerlObject()
    {
        SvREFCNT_dec(sv_);
    }

    static NAN_METHOD(getClassName)
    {
        return info.GetReturnValue().Set(Nan::ObjectWrap::Unwrap<NodePerlObject>(info.This())->getClassName());
    }

    v8::Local<v8::Value> getClassName() const
    {
        Nan::EscapableHandleScope scope;

        if (SvPOK(sv_))
        {
            STRLEN len;

            const char *str = SvPV(sv_, len);

            return scope.Escape(Nan::New(str, len).ToLocalChecked());
        }
        else
        {
            return scope.Escape(Nan::New(sv_reftype(SvRV(sv_), TRUE)).ToLocalChecked());
        }
    }

    static SV *getSV(v8::Local<v8::Object> val)
    {
        return Nan::ObjectWrap::Unwrap<NodePerlObject>(val)->sv_;
    }

    static v8::Local<v8::Value> blessed(v8::Local<v8::Object> val)
    {
        return Nan::ObjectWrap::Unwrap<NodePerlObject>(val)->blessed();
    }

    v8::Local<v8::Value> blessed() const
    {
        Nan::EscapableHandleScope scope;

        if (!(SvROK(sv_) && SvOBJECT(SvRV(sv_))))
        {
            return scope.Escape(Nan::Undefined());
        }

        return scope.Escape(Nan::New(sv_reftype(SvRV(sv_), TRUE)).ToLocalChecked());
    }

    static NAN_METHOD(New)
    {
        const Nan::FunctionCallbackInfo<v8::Value> &args = info;

        ARG_EXT(0, jssv);

        ARG_EXT(1, jsmyp);

        SV *sv = static_cast<SV *>(jssv->Value());

        PerlInterpreter *myp = static_cast<PerlInterpreter *>(jsmyp->Value());

        (new NodePerlObject(sv, myp))->Wrap(args.Holder());

        return info.GetReturnValue().Set(args.Holder());
    }
};

class NodePerlClass : public NodePerlObject
{
  public:
    static Nan::Persistent<v8::FunctionTemplate> constructor_template;

    static inline Nan::Persistent<v8::Function> &constructor()
    {
        static Nan::Persistent<v8::Function> my_constructor;

        return my_constructor;
    }

    static NAN_MODULE_INIT(Init)
    {
        // Prepare constructor template
        v8::Local<v8::FunctionTemplate> tpl = Nan::New<v8::FunctionTemplate>(New);

        tpl->SetClassName(Nan::New("NodePerlClass").ToLocalChecked());

        tpl->InstanceTemplate()->SetInternalFieldCount(1);

        Nan::SetNamedPropertyHandler(tpl->InstanceTemplate(), NodePerlObject::GetNamedProperty);

        constructor_template.Reset(tpl);

        constructor().Reset(Nan::GetFunction(tpl).ToLocalChecked());

        Nan::Set(target, Nan::New("NodePerlClass").ToLocalChecked(), Nan::GetFunction(tpl).ToLocalChecked());
    }
};

class NodePerl : public Nan::ObjectWrap, public PerlFoo
{
  public:
    static inline Nan::Persistent<v8::Function> &constructor()
    {
        static Nan::Persistent<v8::Function> my_constructor;

        return my_constructor;
    }

    static NAN_MODULE_INIT(Init)
    {
        v8::Local<v8::FunctionTemplate> tpl = Nan::New<v8::FunctionTemplate>(New);

        Nan::SetPrototypeMethod(tpl, "evaluate", NodePerl::evaluate);

        Nan::SetPrototypeMethod(tpl, "getClass", NodePerl::getClass);

        Nan::SetPrototypeMethod(tpl, "call", NodePerl::call);

        Nan::SetPrototypeMethod(tpl, "callList", NodePerl::callList);

        Nan::SetPrototypeMethod(tpl, "destroy", NodePerl::destroy);

        tpl->InstanceTemplate()->SetInternalFieldCount(1);

        Nan::SetMethod(tpl, "blessed", NodePerl::blessed);

        constructor().Reset(Nan::GetFunction(tpl).ToLocalChecked());

        Nan::Set(target, Nan::New("Perl").ToLocalChecked(), Nan::GetFunction(tpl).ToLocalChecked());
    }

    NodePerl(): PerlFoo()
    {
        // std::cerr << "[Construct Perl]" << std::endl;

        char **av = {NULL};

        const char *embedding[] = {"", "-e", "0"};

        // XXX PL_origalen makes segv.
        // PL_origalen = 1; // for save

        PERL_SYS_INIT3(0, &av, NULL);

        my_perl = perl_alloc();

        perl_construct(my_perl);

        perl_parse(my_perl, xs_init, 3, (char **)embedding, NULL);

        PL_exit_flags |= PERL_EXIT_DESTRUCT_END;

        perl_run(my_perl);
    }

    ~NodePerl()
    {
        // std::cerr << "[Destruct Perl]" << std::endl;

        PL_perl_destruct_level = 2;

        perl_destruct(my_perl);

        perl_free(my_perl);
    }

    static NAN_METHOD(New)
    {
        if (!info.IsConstructCall())
        {
            return info.GetReturnValue().Set(Nan::NewInstance(Nan::New(constructor()), 0, {}).ToLocalChecked());
        }

        (new NodePerl())->Wrap(info.Holder());

        return info.GetReturnValue().Set(info.Holder());
    }

    static NAN_METHOD(blessed)
    {
        const auto &args = info;

        ARG_OBJ(0, jsobj);

        if (Nan::New<v8::FunctionTemplate>(NodePerlObject::constructor_template)->HasInstance(jsobj))
        {
            return info.GetReturnValue().Set(NodePerlObject::blessed(jsobj));
        }
        else
        {
            return info.GetReturnValue().Set(Nan::Undefined());
        }
    }

    static NAN_METHOD(evaluate)
    {
        if (!info[0]->IsString())
        {
            Nan::ThrowError(v8::Exception::Error(Nan::New("Arguments must be string").ToLocalChecked()));

            return info.GetReturnValue().Set(Nan::Undefined());
        }

        Nan::Utf8String stmt(info[0]);

        v8::Local<v8::Value> retval = Nan::ObjectWrap::Unwrap<NodePerl>(info.This())->evaluate(*stmt);

        return info.GetReturnValue().Set(retval);
    }

    static NAN_METHOD(getClass)
    {
        if (!info[0]->IsString())
        {
            Nan::ThrowError(v8::Exception::Error(Nan::New("Arguments must be string").ToLocalChecked()));

            return info.GetReturnValue().Set(Nan::Undefined());
        }

        Nan::Utf8String stmt(info[0]);

        v8::Local<v8::Value> retval = Nan::ObjectWrap::Unwrap<NodePerl>(info.This())->getClass(*stmt);

        return info.GetReturnValue().Set(retval);
    }

    static NAN_METHOD(call)
    {
        NodePerl *nodePerl = Nan::ObjectWrap::Unwrap<NodePerl>(info.This());

        info.GetReturnValue().Set(nodePerl->CallMethod2(info, false));
    }

    static NAN_METHOD(callList)
    {
        NodePerl *nodePerl = Nan::ObjectWrap::Unwrap<NodePerl>(info.This());

        info.GetReturnValue().Set(nodePerl->CallMethod2(info, true));
    }

    static NAN_METHOD(destroy)
    {
        return Nan::ObjectWrap::Unwrap<NodePerl>(info.This())->destroy();
    }

  private:
    void destroy()
    {
        this->persistent().Reset();
        this->~NodePerl();
    }

    v8::Local<v8::Value> getClass(const char *name) const
    {
        Nan::EscapableHandleScope scope;

        v8::Local<v8::Value> arg0 = Nan::New<v8::External>(sv_2mortal(newSVpv(name, 0)));

        v8::Local<v8::Value> arg1 = Nan::New<v8::External>(my_perl);

        v8::Local<v8::Value> info[] = {arg0, arg1};

        v8::Local<v8::Object> retval(
            Nan::NewInstance(
                Nan::GetFunction(Nan::New<v8::FunctionTemplate>(NodePerlClass::constructor_template)).ToLocalChecked(),
                2,
                info)
                .ToLocalChecked());

        return scope.Escape(retval);
    }

    v8::Local<v8::Value> evaluate(const char *stmt)
    {
        return perl2js(eval_pv(stmt, TRUE));
    }
};

v8::Local<v8::Value> PerlFoo::perl2js_rv(SV *rv)
{
    Nan::EscapableHandleScope scope;

    SV *sv = SvRV(rv);

    SvGETMAGIC(sv);

    svtype svt = (svtype)SvTYPE(sv);

    if (SvOBJECT(sv))
    { // blessed object.
        v8::Local<v8::Value> arg0 = Nan::New<v8::External>(rv);

        v8::Local<v8::Value> arg1 = Nan::New<v8::External>(my_perl);

        v8::Local<v8::Value> args[] = {arg0, arg1};

        v8::Local<v8::Object> retval(
            Nan::NewInstance(
                Nan::GetFunction(Nan::New<v8::FunctionTemplate>(NodePerlObject::constructor_template)).ToLocalChecked(),
                2,
                args)
                .ToLocalChecked());

        return scope.Escape(retval);
    }
    else if (svt == SVt_PVHV)
    {
        HV *hval = (HV *)sv;

        HE *he;

        v8::Local<v8::Object> retval = Nan::New<v8::Object>();

        while ((he = hv_iternext(hval)))
        {
            Nan::Set(retval, this->perl2js(hv_iterkeysv(he)), this->perl2js(hv_iterval(hval, he)));
        }

        return scope.Escape(retval);
    }
    else if (svt == SVt_PVAV)
    {
        AV *ary = (AV *)sv;

        v8::Local<v8::Array> retval = Nan::New<v8::Array>();

        int len = av_len(ary) + 1;

        for (int i = 0; i < len; ++i)
        {
            SV **svp = av_fetch(ary, i, 0);

            if (svp)
            {
                Nan::Set(retval, i, this->perl2js(*svp));
            }
            else
            {
                Nan::Set(retval, i, Nan::Undefined());
            }
        }

        return scope.Escape(retval);
    }
    else if (svt < SVt_PVAV)
    {
        sv_dump(sv);

        Nan::ThrowError(v8::Exception::Error(Nan::New("node-perl-simple doesn't support scalarref").ToLocalChecked()));

        return scope.Escape(Nan::Undefined());
    }
    else
    {
        return scope.Escape(Nan::Undefined());
    }
};

SV *PerlFoo::js2perl(v8::Local<v8::Value> val) const
{
    if (val->IsTrue())
    {
        return &PL_sv_yes;
    }
    else if (val->IsFalse())
    {
        return &PL_sv_no;
    }
    else if (val->IsString())
    {
        Nan::Utf8String method(val);

        return sv_2mortal(newSVpv(*method, method.length()));
    }
    else if (val->IsArray())
    {
        v8::Local<v8::Array> jsav = v8::Local<v8::Array>::Cast(val);

        AV *av = newAV();

        av_extend(av, jsav->Length());

        for (unsigned int i = 0; i < jsav->Length(); ++i)
        {
            SV *elem = this->js2perl(Nan::Get(jsav, i).ToLocalChecked());

            av_push(av, SvREFCNT_inc(elem));
        }

        return sv_2mortal(newRV_noinc((SV *)av));
    }
    else if (val->IsObject())
    {
        v8::Local<v8::Object> jsobj = v8::Local<v8::Object>::Cast(val);

        v8::Local<v8::FunctionTemplate> NodePerlObject =
            Nan::New<v8::FunctionTemplate>(NodePerlObject::constructor_template);

        if (NodePerlObject->HasInstance(jsobj))
        {
            SV *ret = NodePerlObject::getSV(jsobj);

            return ret;
        }
        else if (NodePerlObject->HasInstance(jsobj))
        {
            SV *ret = NodePerlObject::getSV(jsobj);

            return ret;
        }
        else
        {
            v8::Local<v8::Array> keys = Nan::GetOwnPropertyNames(jsobj).ToLocalChecked();

            HV *hv = newHV();

            hv_ksplit(hv, keys->Length());

            for (unsigned int i = 0; i < keys->Length(); ++i)
            {
                auto keysIdx = Nan::Get(keys, i).ToLocalChecked();

                SV *k = this->js2perl(keysIdx);

                SV *v = this->js2perl(Nan::Get(jsobj, keysIdx).ToLocalChecked());

                hv_store_ent(hv, k, v, 0);

                // SvREFCNT_dec(k);
            }

            return sv_2mortal(newRV_inc((SV *)hv));
        }
    }
    else if (val->IsInt32())
    {
        int32_t value = Nan::To<int32_t>(val).FromJust();

        return sv_2mortal(newSViv(value));
    }
    else if (val->IsUint32())
    {
        uint32_t value = Nan::To<uint32_t>(val).FromJust();

        return sv_2mortal(newSVuv(value));
    }
    else if (val->IsNumber())
    {
        int value = Nan::To<int>(val).FromJust();

        return sv_2mortal(newSVnv(value));
    }
    else
    {
        return NULL;
    }
};

Nan::Persistent<v8::FunctionTemplate> NodePerlObject::constructor_template;

Nan::Persistent<v8::FunctionTemplate> NodePerlMethod::constructor_template;

Nan::Persistent<v8::FunctionTemplate> NodePerlClass::constructor_template;

/**
 * Load lazily libperl for dynamic loaded xs.
 * I don't know the better way to resolve symbols in xs.
 * patches welcome.
 *
 * And this code is not portable.
 * patches welcome.
 */
#ifdef WIN32
static NAN_METHOD(InitPerl)
{
    auto uMode = SetErrorMode(SEM_FAILCRITICALERRORS);

    auto hModule = LoadLibraryEx(LIBPERL_DIR LIBPERL, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);

    auto error = GetLastError();

    if (hModule)
    {
        FreeLibrary(hModule);
    }
    else
    {
        CHAR errorBuffer[65535];

        auto pos = FormatMessage(
            FORMAT_MESSAGE_FROM_SYSTEM,
            NULL,
            error,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            errorBuffer,
            sizeof(errorBuffer),
            NULL);

        if (pos > 0)
        {
            if (errorBuffer[pos - 2] == '\r' && errorBuffer[pos - 1] == '\n')
                errorBuffer[pos - 2] = '\0';

            std::cerr << "Error loading " << LIBPERL_DIR LIBPERL << ": " << errorBuffer << std::endl;
        }
    }

    SetErrorMode(uMode);

    return info.GetReturnValue().Set(Nan::Undefined());
}
#else
static NAN_METHOD(InitPerl)
{
    void *lib = dlopen(LIBPERL, RTLD_LAZY | RTLD_GLOBAL);

    if (lib)
    {
        dlclose(lib);

        return info.GetReturnValue().Set(Nan::Undefined());
    }
    else
    {
        std::cerr << dlerror() << std::endl;

        return info.GetReturnValue().Set(Nan::Undefined());

        // return Nan::ThrowError(v8::Exception::Error(Nan::New(dlerror())));
    }
}
#endif

extern "C" NAN_MODULE_INIT(init)
{
    {
        Nan::Set(
            target,
            Nan::New("InitPerl").ToLocalChecked(),
            Nan::GetFunction(Nan::New<v8::FunctionTemplate>(InitPerl)).ToLocalChecked());
    }

    NodePerl::Init(target);

    NodePerlObject::Init(target);

    NodePerlClass::Init(target);

    NodePerlMethod::Init(target);
}

NODE_MODULE(perl, init)
