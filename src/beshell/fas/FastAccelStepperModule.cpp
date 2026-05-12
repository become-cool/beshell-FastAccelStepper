#include "./FastAccelStepperModule.hpp"
#include "./Stepper.hpp"
#include <FastAccelStepper.h>


static FastAccelStepperEngine engine;
static bool engine_inited = false ;

namespace be::fas {

    char const * const FastAccelStepperModule::name = "fas" ;

    FastAccelStepperModule::FastAccelStepperModule(JSContext * ctx, const char * name)
        : NativeModule(ctx, name, 0)
    {
        exportClass<Stepper>() ;
        exportFunction("setup",setup,0) ;
        exportFunction("stepperConnectToPin",stepperConnectToPin,0) ;

        exportValue("version", JS_NewString(ctx, "1.0.1")) ;
    }

    // void FastAccelStepperModule::use(be::BeShell * beshell) {
    //     beshell->use<be::XXX>() ;
    // }

    // void FastAccelStepperModule::exports(JSContext *ctx) {
    // }

    JSValue FastAccelStepperModule::setup(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        if (engine_inited) {
            JSTHROW("FastAccelStepper engine already inited") ;
        }

        if(argc>0) {
            ARGV_TO_UINT8(0,stepper_num) ;
            engine.init(stepper_num) ;
        }
        else {
            engine.init() ;
        }
        engine_inited = true ;

        return JS_UNDEFINED ;
    }

    JSValue FastAccelStepperModule::stepperConnectToPin(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        if(!engine_inited) {
            engine.init() ;
            engine_inited = true ;
        }

        ARGV_TO_GPIO(0, pin) ;

        ::FastAccelStepper * stepper = engine.stepperConnectToPin(pin) ;
        if(!stepper) {
            JSTHROW("failed to connect stepper to pin %d", pin) ;
        }

        Stepper * stepper_obj = new Stepper(ctx, stepper) ;
        // stepper_obj->shared() ;

        return stepper_obj->jsobj ;
    }
    
}