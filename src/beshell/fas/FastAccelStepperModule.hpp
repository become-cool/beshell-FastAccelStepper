#pragma once

#include <beshell/BeShell.hpp>
#include <beshell/NativeModule.hpp>

namespace be::fas {
    class FastAccelStepperModule: public be::NativeModule {
    public:

        static char const * const name ;

        FastAccelStepperModule(JSContext * ctx, const char * name=nullptr) ;

        // static void use(be::BeShell * beshell) ;

        // void exports(JSContext *ctx) ;

        static JSValue setup(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) ;
        static JSValue stepperConnectToPin(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) ;
    } ;
}
