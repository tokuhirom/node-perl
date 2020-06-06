#include "EXTERN.h"
#include "XSUB.h"
#include "perl.h"

EXTERN_C void xs_init(pTHX);

EXTERN_C void boot_DynaLoader(pTHX_ CV *cv);

#ifdef WIN32
EXTERN_C void boot_Win32CORE(pTHX_ CV *cv);
#endif

EXTERN_C void xs_init(pTHX)
{
    static const char file[] = __FILE__;
    dXSUB_SYS;
    PERL_UNUSED_CONTEXT;

    /* DynaLoader is a special case */
    newXS("DynaLoader::boot_DynaLoader", boot_DynaLoader, file);
#ifdef WIN32
    newXS("Win32CORE::bootstrap", boot_Win32CORE, file);
#endif
}
