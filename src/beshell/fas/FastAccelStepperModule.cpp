#include "./FastAccelStepperModule.hpp"
#include "./Stepper.hpp"
#include <FastAccelStepper.h>

/**
 * FastAccelStepper 步进电机控制模块
 *
 * 基于 FastAccelStepper 库，提供高性能的步进电机控制能力。支持速度控制、位置控制、加减速等高级功能。
 *
 * 示例：
 * ```javascript
 * import * as fas from "fas"
 *
 * // 初始化引擎
 * fas.setup()
 *
 * // 连接步进电机到引脚
 * const stepper = fas.stepperConnectToPin(2)
 *
 * // 设置速度和加速度
 * stepper.setSpeedInHz(1000)
 * stepper.setAcceleration(5000)
 *
 * // 移动 1000 步
 * stepper.move(1000)
 * ```
 *
 * @module fas
 * @component beshell-FastAccelStepper
 */


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

    /**
     * 初始化 FastAccelStepper 引擎
     *
     * 在使用任何步进电机之前必须先调用此方法初始化引擎。如果未显式调用，`stepperConnectToPin()` 会自动初始化。
     *
     * 示例：
     * ```javascript
     * import * as fas from "fas"
     *
     * // 初始化引擎（默认支持 1 个电机）
     * fas.setup()
     *
     * // 指定最大电机数量
     * fas.setup(3)
     * ```
     *
     * @module fas
     * @function setup
     * @param maxSteppers:number=1 最大步进电机数量
     * @return undefined
     * @throws FastAccelStepper engine already inited
     */
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

    /**
     * 将步进电机连接到指定引脚
     *
     * 创建一个 [Stepper](Stepper.html) 实例并连接到指定的 GPIO 引脚。如果引擎尚未初始化，会自动调用 `setup()` 进行初始化。
     *
     * 示例：
     * ```javascript
     * import * as fas from "fas"
     *
     * // 连接步进电机到 GPIO 2
     * const stepper = fas.stepperConnectToPin(2)
     *
     * // 设置参数
     * stepper.setSpeedInHz(800)
     * stepper.setAcceleration(10000)
     *
     * // 移动
     * stepper.move(2000)
     * ```
     *
     * @module fas
     * @function stepperConnectToPin
     * @param pin:number GPIO 引脚号
     * @return [Stepper](Stepper.html) 步进电机实例
     * @throws failed to connect stepper
     */
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