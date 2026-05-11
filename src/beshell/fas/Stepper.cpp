#include "Stepper.hpp"
#include "FastAccelStepper.h"
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

using namespace std ;

namespace be::fas {
    struct ISRData {
        Stepper* stepper;
        uint8_t pin;
    };

    struct LimitEventParam {
        uint8_t pin;
    };

    static constexpr TickType_t LIMIT_DEBOUNCE_TICKS = pdMS_TO_TICKS(50);

    DEFINE_NCLASS_META(Stepper, EventEmitter)
    std::vector<JSCFunctionListEntry> Stepper::methods = {
        // 引脚设置相关
        JS_CFUNC_DEF("setDirectionPin", 0, Stepper::setDirectionPin),
        JS_CFUNC_DEF("getDirectionPin", 0, Stepper::getDirectionPin),
        JS_CFUNC_DEF("directionPinHighCountsUp", 0, Stepper::directionPinHighCountsUp),
        
        // 使能引脚相关
        JS_CFUNC_DEF("setEnablePin", 0, Stepper::setEnablePin),
        JS_CFUNC_DEF("getEnablePinHighActive", 0, Stepper::getEnablePinHighActive),
        JS_CFUNC_DEF("getEnablePinLowActive", 0, Stepper::getEnablePinLowActive),
        
        // 自动使能相关
        JS_CFUNC_DEF("setAutoEnable", 0, Stepper::setAutoEnable),
        JS_CFUNC_DEF("setDelayToEnable", 0, Stepper::setDelayToEnable),
        JS_CFUNC_DEF("setDelayToDisable", 0, Stepper::setDelayToDisable),
        
        // 速度设置相关
        JS_CFUNC_DEF("setSpeedInHz", 0, Stepper::setSpeedInHz),
        JS_CFUNC_DEF("setSpeedInUs", 0, Stepper::setSpeedInUs),
        JS_CFUNC_DEF("setSpeedInTicks", 0, Stepper::setSpeedInTicks),
        JS_CFUNC_DEF("getMaxSpeedInHz", 0, Stepper::getMaxSpeedInHz),
        JS_CFUNC_DEF("getMaxSpeedInUs", 0, Stepper::getMaxSpeedInUs),
        JS_CFUNC_DEF("getMaxSpeedInTicks", 0, Stepper::getMaxSpeedInTicks),
        JS_CFUNC_DEF("getSpeedInUs", 0, Stepper::getSpeedInUs),
        JS_CFUNC_DEF("getSpeedInTicks", 0, Stepper::getSpeedInTicks),
        JS_CFUNC_DEF("getSpeedInMilliHz", 0, Stepper::getSpeedInMilliHz),
        
        // 加速度设置相关
        JS_CFUNC_DEF("setAcceleration", 0, Stepper::setAcceleration),
        JS_CFUNC_DEF("setLinearAcceleration", 0, Stepper::setLinearAcceleration),
        JS_CFUNC_DEF("getAcceleration", 0, Stepper::getAcceleration),
        JS_CFUNC_DEF("getCurrentAcceleration", 0, Stepper::getCurrentAcceleration),
        
        // 跳跃启动相关
        JS_CFUNC_DEF("setJumpStart", 0, Stepper::setJumpStart),
        
        // 运动控制相关
        JS_CFUNC_DEF("move", 0, Stepper::move),
        JS_CFUNC_DEF("moveTo", 0, Stepper::moveTo),
        JS_CFUNC_DEF("runForward", 0, Stepper::runForward),
        JS_CFUNC_DEF("runBackward", 0, Stepper::runBackward),
        JS_CFUNC_DEF("keepRunning", 0, Stepper::keepRunning),
        JS_CFUNC_DEF("moveByAcceleration", 0, Stepper::moveByAcceleration),
        JS_CFUNC_DEF("moveTimed", 0, Stepper::moveTimed),
        JS_CFUNC_DEF("stopMove", 0, Stepper::stopMove),
        JS_CFUNC_DEF("forceStop", 0, Stepper::forceStop),
        JS_CFUNC_DEF("forwardStep", 0, Stepper::forwardStep),
        JS_CFUNC_DEF("backwardStep", 0, Stepper::backwardStep),
        
        // 状态查询相关
        JS_CFUNC_DEF("getCurrentPosition", 0, Stepper::getCurrentPosition),
        JS_CFUNC_DEF("setCurrentPosition", 0, Stepper::setCurrentPosition),
        JS_CFUNC_DEF("isRunning", 0, Stepper::isRunning),
        JS_CFUNC_DEF("getCurrentSpeedInUs", 0, Stepper::getCurrentSpeedInUs),
        JS_CFUNC_DEF("targetPos", 0, Stepper::targetPos),
        JS_CFUNC_DEF("rampState", 0, Stepper::rampState),
        
        // 队列相关
        JS_CFUNC_DEF("isQueueEmpty", 0, Stepper::isQueueEmpty),
        JS_CFUNC_DEF("isQueueFull", 0, Stepper::isQueueFull),
        JS_CFUNC_DEF("isQueueRunning", 0, Stepper::isQueueRunning),
        JS_CFUNC_DEF("ticksInQueue", 0, Stepper::ticksInQueue),
        JS_CFUNC_DEF("queueEntries", 0, Stepper::queueEntries),
        
        // 停止相关
        JS_CFUNC_DEF("stepsToStop", 0, Stepper::stepsToStop),
        JS_CFUNC_DEF("isStopping", 0, Stepper::isStopping),
        
        // 位置设置相关
        JS_CFUNC_DEF("getPositionAfterCommandsCompleted", 0, Stepper::getPositionAfterCommandsCompleted),
        JS_CFUNC_DEF("setPositionAfterCommandsCompleted", 0, Stepper::setPositionAfterCommandsCompleted),
        
        // 周期相关
        JS_CFUNC_DEF("getPeriodInUsAfterCommandsCompleted", 0, Stepper::getPeriodInUsAfterCommandsCompleted),
        JS_CFUNC_DEF("getPeriodInTicksAfterCommandsCompleted", 0, Stepper::getPeriodInTicksAfterCommandsCompleted),
        
        // 连续运行相关
        JS_CFUNC_DEF("isRunningContinuously", 0, Stepper::isRunningContinuously),
        
        // 其他引脚相关
        JS_CFUNC_DEF("getStepPin", 0, Stepper::getStepPin),

        JS_CFUNC_DEF("waitStop", 0, Stepper::waitStop),
        JS_CFUNC_DEF("setLimitPin", 0, Stepper::setLimitPin),
    } ;

    Stepper::Stepper(JSContext * ctx, FastAccelStepper * stepper)
        : EventEmitter(ctx,build(ctx,JS_NULL))
        , stepper(stepper)
    {
        enableNativeEvent(ctx, sizeof(LimitEventParam), 10) ;
    }

    Stepper::~Stepper() {
        for(auto data : (std::vector<ISRData*>)limitPins) {
            gpio_isr_handler_remove((gpio_num_t)data->pin);
            delete data;
        }
        limitPins.clear();
    }

    JSValue Stepper::constructor(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        JSTHROW("call fas.stepperConnectToPin() to create stepper") ;
    }

    /*
     * 设置步进电机的方向控制引脚、极性和切换方向时的延迟时间（以微秒为单位）。
     *
     * @module FastAccelStepper
     * @component beshell-FastAccelStepper
     * @function setDirectionPin
     * @param pin:number GPIO 引脚号，用于控制步进电机的方向。
     * @param polarity:boolean=true 方向极性，true 表示正向，false 表示反向。
     * @param delay_us:number=0 切换方向时的延迟时间，单位为微秒。这个参数用于确保在切换方向时有足够的时间让电机停下来，以避免损坏。
     * @return undefined
     * @throws 如果参数无效或设置失败，将抛出异常。
     * @example
     * ```javascript
     * stepper.setDirectionPin(5, true, 100);
     * ```
     */
    JSValue Stepper::setDirectionPin(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        THIS_NCLASS(Stepper, stepper)
        if(argc < 1) {
            JSTHROW("setDirectionPin requires at least 1 argument: pin")
        }
        uint32_t pin32;
        bool polarity = true;
        uint32_t delay32 = 0;
        JS_ToUint32(ctx, &pin32, argv[0]);
        if(argc > 1) {
            polarity = JS_ToBool(ctx, argv[1]);
        }
        if(argc > 2) {
            JS_ToUint32(ctx, &delay32, argv[2]);
        }
        uint8_t pin = (uint8_t)pin32;
        uint16_t delay_us = (uint16_t)delay32;
        stepper->stepper->setDirectionPin(pin, polarity, delay_us);
        return JS_UNDEFINED;
    }

    /*
     * 设置步进电机的使能控制引脚和低电平激活模式。
     *
     * @module FastAccelStepper
     * @component beshell-FastAccelStepper
     * @function setEnablePin
     * @param pin:number GPIO 引脚号，用于控制步进电机的使能。
     * @param low_active:boolean=true true 表示低电平激活使能，false 表示高电平激活。
     * @return undefined
     * @throws 如果参数无效或设置失败，将抛出异常。
     * @example
     * ```javascript
     * stepper.setEnablePin(4, true);
     * ```
     */
    JSValue Stepper::setEnablePin(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        THIS_NCLASS(Stepper, stepper)
        if(argc < 1) {
            JSTHROW("setEnablePin requires at least 1 argument: pin")
        }
        uint32_t pin32;
        bool low_active = true;
        JS_ToUint32(ctx, &pin32, argv[0]);
        if(argc > 1) {
            low_active = JS_ToBool(ctx, argv[1]);
        }
        uint8_t pin = (uint8_t)pin32;
        stepper->stepper->setEnablePin(pin, low_active);
        return JS_UNDEFINED;
    }

    /*
     * 获取步进电机的步进引脚号。
     *
     * @module FastAccelStepper
     * @component beshell-FastAccelStepper
     * @function getStepPin
     * @return number 步进引脚的 GPIO 编号。
     * @example
     * ```javascript
     * const pin = stepper.getStepPin();
     * console.log("Step pin:", pin);
     * ```
     */
    JSValue Stepper::getStepPin(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        THIS_NCLASS(Stepper, stepper)
        uint8_t pin = stepper->stepper->getStepPin();
        return JS_NewUint32(ctx, pin);
    }

    /*
     * 获取步进电机的方向引脚号。
     *
     * @module FastAccelStepper
     * @component beshell-FastAccelStepper
     * @function getDirectionPin
     * @return number 方向引脚的 GPIO 编号。
     * @example
     * ```javascript
     * const pin = stepper.getDirectionPin();
     * console.log("Direction pin:", pin);
     * ```
     */
    JSValue Stepper::getDirectionPin(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        THIS_NCLASS(Stepper, stepper)
        uint8_t pin = stepper->stepper->getDirectionPin();
        return JS_NewUint32(ctx, pin);
    }

    /*
     * 检查方向引脚高电平是否表示正向计数。
     *
     * @module FastAccelStepper
     * @component beshell-FastAccelStepper
     * @function directionPinHighCountsUp
     * @return boolean 如果高电平表示正向计数返回 true，否则返回 false。
     * @example
     * ```javascript
     * const highCountsUp = stepper.directionPinHighCountsUp();
     * console.log("Direction pin high counts up:", highCountsUp);
     * ```
     */
    JSValue Stepper::directionPinHighCountsUp(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        THIS_NCLASS(Stepper, stepper)
        bool highCountsUp = stepper->stepper->directionPinHighCountsUp();
        return JS_NewBool(ctx, highCountsUp);
    }

    /*
     * 获取使能引脚高电平激活的引脚号。
     *
     * @module FastAccelStepper
     * @component beshell-FastAccelStepper
     * @function getEnablePinHighActive
     * @return number 使能引脚高电平激活的 GPIO 编号。
     * @example
     * ```javascript
     * const pin = stepper.getEnablePinHighActive();
     * console.log("Enable pin (high active):", pin);
     * ```
     */
    JSValue Stepper::getEnablePinHighActive(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        THIS_NCLASS(Stepper, stepper)
        uint8_t pin = stepper->stepper->getEnablePinHighActive();
        return JS_NewUint32(ctx, pin);
    }

    /*
     * 获取使能引脚低电平激活的引脚号。
     *
     * @module FastAccelStepper
     * @component beshell-FastAccelStepper
     * @function getEnablePinLowActive
     * @return number 使能引脚低电平激活的 GPIO 编号。
     * @example
     * ```javascript
     * const pin = stepper.getEnablePinLowActive();
     * console.log("Enable pin (low active):", pin);
     * ```
     */
    JSValue Stepper::getEnablePinLowActive(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        THIS_NCLASS(Stepper, stepper)
        uint8_t pin = stepper->stepper->getEnablePinLowActive();
        return JS_NewUint32(ctx, pin);
    }

    /*
     * 设置步进电机的自动使能模式。
     *
     * @module FastAccelStepper
     * @component beshell-FastAccelStepper
     * @function setAutoEnable
     * @param enable:boolean true 启用自动使能，电机移动前自动使能，停止后自动禁用。
     * @return undefined
     * @throws 如果参数无效，将抛出异常。
     * @example
     * ```javascript
     * stepper.setAutoEnable(true);
     * ```
     */
    JSValue Stepper::setAutoEnable(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        THIS_NCLASS(Stepper, stepper)
        if(argc < 1) {
            JSTHROW("setAutoEnable requires 1 argument: enable")
        }
        bool enable = JS_ToBool(ctx, argv[0]);
        stepper->stepper->setAutoEnable(enable);
        return JS_UNDEFINED;
    }

    /*
     * 设置电机使能后的延迟时间。
     *
     * @module FastAccelStepper
     * @component beshell-FastAccelStepper
     * @function setDelayToEnable
     * @param delay_us:number 使能到第一个步进脉冲之间的延迟时间，单位为微秒。
     * @return undefined
     * @throws 如果参数无效，将抛出异常。
     * @example
     * ```javascript
     * stepper.setDelayToEnable(500);
     * ```
     */
    JSValue Stepper::setDelayToEnable(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        THIS_NCLASS(Stepper, stepper)
        if(argc < 1) {
            JSTHROW("setDelayToEnable requires 1 argument: delay_us")
        }
        uint32_t delay32;
        JS_ToUint32(ctx, &delay32, argv[0]);
        uint16_t delay_us = (uint16_t)delay32;
        stepper->stepper->setDelayToEnable(delay_us);
        return JS_UNDEFINED;
    }

    /*
     * 设置电机停止后的禁用延迟时间。
     *
     * @module FastAccelStepper
     * @component beshell-FastAccelStepper
     * @function setDelayToDisable
     * @param delay_ms:number 电机停止后到禁用输出之间的延迟时间，单位为毫秒。
     * @return undefined
     * @throws 如果参数无效，将抛出异常。
     * @example
     * ```javascript
     * stepper.setDelayToDisable(100);
     * ```
     */
    JSValue Stepper::setDelayToDisable(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        THIS_NCLASS(Stepper, stepper)
        if(argc < 1) {
            JSTHROW("setDelayToDisable requires 1 argument: delay_ms")
        }
        uint32_t delay32;
        JS_ToUint32(ctx, &delay32, argv[0]);
        uint16_t delay_ms = (uint16_t)delay32;
        stepper->stepper->setDelayToDisable(delay_ms);
        return JS_UNDEFINED;
    }

    /*
     * 设置步进电机的最大速度。
     *
     * @module FastAccelStepper
     * @component beshell-FastAccelStepper
     * @function setSpeedInHz
     * @param hz:number 最大速度，单位为步/秒。
     * @return undefined
     * @throws 如果参数无效，将抛出异常。
     * @example
     * ```javascript
     * stepper.setSpeedInHz(1000);
     * ```
     */
    JSValue Stepper::setSpeedInHz(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        THIS_NCLASS(Stepper, stepper)
        if(argc < 1) {
            JSTHROW("setSpeedInHz requires 1 argument: hz")
        }
        uint32_t hz;
        JS_ToUint32(ctx, &hz, argv[0]);
        stepper->stepper->setSpeedInHz(hz);
        return JS_UNDEFINED;
    }

    /*
     * 设置步进电机的最大速度。
     *
     * @module FastAccelStepper
     * @component beshell-FastAccelStepper
     * @function setSpeedInUs
     * @param us:number 最大速度，单位为微秒/步。
     * @return undefined
     * @throws 如果参数无效，将抛出异常。
     * @example
     * ```javascript
     * stepper.setSpeedInUs(1000);
     * ```
     */
    JSValue Stepper::setSpeedInUs(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        THIS_NCLASS(Stepper, stepper)
        if(argc < 1) {
            JSTHROW("setSpeedInUs requires 1 argument: us")
        }
        uint32_t us;
        JS_ToUint32(ctx, &us, argv[0]);
        stepper->stepper->setSpeedInUs(us);
        return JS_UNDEFINED;
    }

    /*
     * 设置步进电机的最大速度。
     *
     * @module FastAccelStepper
     * @component beshell-FastAccelStepper
     * @function setSpeedInTicks
     * @param ticks:number 最大速度，单位为定时器时钟周期。
     * @return undefined
     * @throws 如果参数无效，将抛出异常。
     * @example
     * ```javascript
     * stepper.setSpeedInTicks(16000);
     * ```
     */
    JSValue Stepper::setSpeedInTicks(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        THIS_NCLASS(Stepper, stepper)
        if(argc < 1) {
            JSTHROW("setSpeedInTicks requires 1 argument: ticks")
        }
        uint32_t ticks;
        JS_ToUint32(ctx, &ticks, argv[0]);
        stepper->stepper->setSpeedInTicks(ticks);
        return JS_UNDEFINED;
    }

    /*
     * 获取步进电机的最大速度（以赫兹为单位）。
     *
     * @module FastAccelStepper
     * @component beshell-FastAccelStepper
     * @function getMaxSpeedInHz
     * @return number 最大速度，单位为赫兹。
     * @example
     * ```javascript
     * const maxSpeed = stepper.getMaxSpeedInHz();
     * console.log("Max speed in Hz:", maxSpeed);
     * ```
     */
    JSValue Stepper::getMaxSpeedInHz(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        THIS_NCLASS(Stepper, stepper)
        uint32_t speed = stepper->stepper->getMaxSpeedInHz();
        return JS_NewUint32(ctx, speed);
    }

    /*
     * 获取步进电机的最大速度（以微秒为单位）。
     *
     * @module FastAccelStepper
     * @component beshell-FastAccelStepper
     * @function getMaxSpeedInUs
     * @return number 最大速度，单位为微秒。
     * @example
     * ```javascript
     * const maxSpeed = stepper.getMaxSpeedInUs();
     * console.log("Max speed in us:", maxSpeed);
     * ```
     */
    JSValue Stepper::getMaxSpeedInUs(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        THIS_NCLASS(Stepper, stepper)
        uint16_t speed = stepper->stepper->getMaxSpeedInUs();
        return JS_NewUint32(ctx, speed);
    }

    /*
     * 获取步进电机的最大速度（以时钟周期为单位）。
     *
     * @module FastAccelStepper
     * @component beshell-FastAccelStepper
     * @function getMaxSpeedInTicks
     * @return number 最大速度，单位为时钟周期。
     * @example
     * ```javascript
     * const maxSpeed = stepper.getMaxSpeedInTicks();
     * console.log("Max speed in ticks:", maxSpeed);
     * ```
     */
    JSValue Stepper::getMaxSpeedInTicks(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        THIS_NCLASS(Stepper, stepper)
        uint16_t speed = stepper->stepper->getMaxSpeedInTicks();
        return JS_NewUint32(ctx, speed);
    }

    /*
     * 获取步进电机的当前速度（以微秒为单位）。
     *
     * @module FastAccelStepper
     * @component beshell-FastAccelStepper
     * @function getSpeedInUs
     * @return number 当前速度，单位为微秒。
     * @example
     * ```javascript
     * const speed = stepper.getSpeedInUs();
     * console.log("Current speed in us:", speed);
     * ```
     */
    JSValue Stepper::getSpeedInUs(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        THIS_NCLASS(Stepper, stepper)
        uint32_t speed = stepper->stepper->getSpeedInUs();
        return JS_NewUint32(ctx, speed);
    }

    /*
     * 获取步进电机的当前速度（以时钟周期为单位）。
     *
     * @module FastAccelStepper
     * @component beshell-FastAccelStepper
     * @function getSpeedInTicks
     * @return number 当前速度，单位为时钟周期。
     * @example
     * ```javascript
     * const speed = stepper.getSpeedInTicks();
     * console.log("Current speed in ticks:", speed);
     * ```
     */
    JSValue Stepper::getSpeedInTicks(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        THIS_NCLASS(Stepper, stepper)
        uint32_t speed = stepper->stepper->getSpeedInTicks();
        return JS_NewUint32(ctx, speed);
    }

    /*
     * 获取步进电机的当前速度（以毫赫兹为单位）。
     *
     * @module FastAccelStepper
     * @component beshell-FastAccelStepper
     * @function getSpeedInMilliHz
     * @return number 当前速度，单位为毫赫兹。
     * @example
     * ```javascript
     * const speed = stepper.getSpeedInMilliHz();
     * console.log("Current speed in milliHz:", speed);
     * ```
     */
    JSValue Stepper::getSpeedInMilliHz(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        THIS_NCLASS(Stepper, stepper)
        uint32_t speed = stepper->stepper->getSpeedInMilliHz();
        return JS_NewUint32(ctx, speed);
    }

    /*
     * 设置步进电机的加速度。
     *
     * @module FastAccelStepper
     * @component beshell-FastAccelStepper
     * @function setAcceleration
     * @param accel:number 加速度，单位为步/秒²。
     * @return undefined
     * @throws 如果参数无效，将抛出异常。
     * @example
     * ```javascript
     * stepper.setAcceleration(1000);
     * ```
     */
    JSValue Stepper::setAcceleration(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        THIS_NCLASS(Stepper, stepper)
        if(argc < 1) {
            JSTHROW("setAcceleration requires 1 argument: accel")
        }
        uint32_t accel;
        JS_ToUint32(ctx, &accel, argv[0]);
        stepper->stepper->setAcceleration(accel);
        return JS_UNDEFINED;
    }

    /*
     * 获取步进电机的加速度。
     *
     * @module FastAccelStepper
     * @component beshell-FastAccelStepper
     * @function getAcceleration
     * @return number 加速度值。
     * @example
     * ```javascript
     * const accel = stepper.getAcceleration();
     * console.log("Acceleration:", accel);
     * ```
     */
    JSValue Stepper::getAcceleration(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        THIS_NCLASS(Stepper, stepper)
        uint32_t accel = stepper->stepper->getAcceleration();
        return JS_NewUint32(ctx, accel);
    }

    /*
     * 获取步进电机的当前加速度。
     *
     * @module FastAccelStepper
     * @component beshell-FastAccelStepper
     * @function getCurrentAcceleration
     * @return number 当前加速度值。
     * @example
     * ```javascript
     * const accel = stepper.getCurrentAcceleration();
     * console.log("Current acceleration:", accel);
     * ```
     */
    JSValue Stepper::getCurrentAcceleration(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        THIS_NCLASS(Stepper, stepper)
        int32_t accel = stepper->stepper->getCurrentAcceleration();
        return JS_NewInt32(ctx, accel);
    }

    /*
     * 设置线性加速度的步数。
     *
     * @module FastAccelStepper
     * @component beshell-FastAccelStepper
     * @function setLinearAcceleration
     * @param steps:number 线性加速度的步数，用于平滑启动。
     * @return undefined
     * @throws 如果参数无效，将抛出异常。
     * @example
     * ```javascript
     * stepper.setLinearAcceleration(100);
     * ```
     */
    JSValue Stepper::setLinearAcceleration(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        THIS_NCLASS(Stepper, stepper)
        if(argc < 1) {
            JSTHROW("setLinearAcceleration requires 1 argument: steps")
        }
        uint32_t steps32;
        JS_ToUint32(ctx, &steps32, argv[0]);
        uint16_t steps = (uint16_t)steps32;
        stepper->stepper->setLinearAcceleration(steps);
        return JS_UNDEFINED;
    }

    /*
     * 设置跳跃启动的步数。
     *
     * @module FastAccelStepper
     * @component beshell-FastAccelStepper
     * @function setJumpStart
     * @param steps:number 跳跃启动的步数，用于快速启动。
     * @return undefined
     * @throws 如果参数无效，将抛出异常。
     * @example
     * ```javascript
     * stepper.setJumpStart(50);
     * ```
     */
    JSValue Stepper::setJumpStart(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        THIS_NCLASS(Stepper, stepper)
        if(argc < 1) {
            JSTHROW("setJumpStart requires 1 argument: steps")
        }
        uint32_t steps32;
        JS_ToUint32(ctx, &steps32, argv[0]);
        uint16_t steps = (uint16_t)steps32;
        stepper->stepper->setJumpStart(steps);
        return JS_UNDEFINED;
    }

    /*
     * 相对移动步进电机指定步数。
     *
     * @module FastAccelStepper
     * @component beshell-FastAccelStepper
     * @function move
     * @param steps:number 移动的步数，正数为正向，负数为反向。
     * @param blocking:boolean=false 是否阻塞等待移动完成。
     * @return undefined
     * @throws 如果参数无效，将抛出异常。
     * @example
     * ```javascript
     * stepper.move(1000, true);
     * ```
     */
    JSValue Stepper::move(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        THIS_NCLASS(Stepper, stepper)
        if(argc < 1) {
            JSTHROW("move requires at least 1 argument: steps")
        }
        int32_t steps;
        bool blocking = false;
        JS_ToInt32(ctx, &steps, argv[0]);
        if(argc > 1) {
            blocking = JS_ToBool(ctx, argv[1]);
        }
        stepper->stepper->move(steps, blocking);
        return JS_UNDEFINED;
    }

    /*
     * 绝对移动步进电机到指定位置。
     *
     * @module FastAccelStepper
     * @component beshell-FastAccelStepper
     * @function moveTo
     * @param position:number 目标位置。
     * @param blocking:boolean=false 是否阻塞等待移动完成。
     * @return undefined
     * @throws 如果参数无效，将抛出异常。
     * @example
     * ```javascript
     * stepper.moveTo(5000, false);
     * ```
     */
    JSValue Stepper::moveTo(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        THIS_NCLASS(Stepper, stepper)
        if(argc < 1) {
            JSTHROW("moveTo requires at least 1 argument: position")
        }
        int32_t position;
        bool blocking = false;
        JS_ToInt32(ctx, &position, argv[0]);
        if(argc > 1) {
            blocking = JS_ToBool(ctx, argv[1]);
        }
        stepper->stepper->moveTo(position, blocking);
        return JS_UNDEFINED;
    }

    /*
     * 让步进电机持续向前运行。
     *
     * @module FastAccelStepper
     * @component beshell-FastAccelStepper
     * @function runForward
     * @return undefined
     * @example
     * ```javascript
     * stepper.runForward();
     * ```
     */
    JSValue Stepper::runForward(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        THIS_NCLASS(Stepper, stepper)
        stepper->stepper->runForward();
        return JS_UNDEFINED;
    }

    /*
     * 让步进电机持续向后运行。
     *
     * @module FastAccelStepper
     * @component beshell-FastAccelStepper
     * @function runBackward
     * @return undefined
     * @example
     * ```javascript
     * stepper.runBackward();
     * ```
     */
    JSValue Stepper::runBackward(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        THIS_NCLASS(Stepper, stepper)
        stepper->stepper->runBackward();
        return JS_UNDEFINED;
    }

    /*
     * 保持步进电机当前方向持续运行。
     *
     * @module FastAccelStepper
     * @component beshell-FastAccelStepper
     * @function keepRunning
     * @return undefined
     * @example
     * ```javascript
     * stepper.keepRunning();
     * ```
     */
    JSValue Stepper::keepRunning(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        THIS_NCLASS(Stepper, stepper)
        stepper->stepper->keepRunning();
        return JS_UNDEFINED;
    }

    /*
     * 通过加速度控制步进电机的移动。
     *
     * @module FastAccelStepper
     * @component beshell-FastAccelStepper
     * @function moveByAcceleration
     * @param accel:number 加速度值，正数加速，负数减速。
     * @param reverse:boolean=true 是否允许反向。
     * @return undefined
     * @throws 如果参数无效，将抛出异常。
     * @example
     * ```javascript
     * stepper.moveByAcceleration(500, true);
     * ```
     */
    JSValue Stepper::moveByAcceleration(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        THIS_NCLASS(Stepper, stepper)
        if(argc < 1) {
            JSTHROW("moveByAcceleration requires at least 1 argument: accel")
        }
        int32_t accel;
        bool reverse = true;
        JS_ToInt32(ctx, &accel, argv[0]);
        if(argc > 1) {
            reverse = JS_ToBool(ctx, argv[1]);
        }
        stepper->stepper->moveByAcceleration(accel, reverse);
        return JS_UNDEFINED;
    }

    /*
     * 在指定时间内完成指定步数的定时移动。
     *
     * @module FastAccelStepper
     * @component beshell-FastAccelStepper
     * @function moveTimed
     * @param steps:number 移动的步数。
     * @param duration:number 持续时间，单位为定时器时钟周期。
     * @param start:boolean=true 是否立即开始。
     * @return number MoveTimedResultCode 结果代码。
     * @throws 如果参数无效，将抛出异常。
     * @example
     * ```javascript
     * let result = stepper.moveTimed(100, 16000, true);
     * console.log(result);
     * ```
     */
    JSValue Stepper::moveTimed(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        THIS_NCLASS(Stepper, stepper)
        if(argc < 2) {
            JSTHROW("moveTimed requires at least 2 arguments: steps, duration")
        }
        int32_t steps32;
        uint32_t duration;
        bool start = true;
        JS_ToInt32(ctx, &steps32, argv[0]);
        JS_ToUint32(ctx, &duration, argv[1]);
        if(argc > 2) {
            start = JS_ToBool(ctx, argv[2]);
        }
        int16_t steps = (int16_t)steps32;
        MoveTimedResultCode result = stepper->stepper->moveTimed(steps, duration, nullptr, start);
        return JS_NewUint32(ctx, (uint32_t)result);
    }

    /*
     * 减速停止步进电机。
     *
     * @module FastAccelStepper
     * @component beshell-FastAccelStepper
     * @function stopMove
     * @return undefined
     * @example
     * ```javascript
     * stepper.stopMove();
     * ```
     */
    JSValue Stepper::stopMove(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        THIS_NCLASS(Stepper, stepper)
        stepper->stepper->stopMove();
        return JS_UNDEFINED;
    }

    /*
     * 立即停止步进电机，无减速度。
     *
     * @module FastAccelStepper
     * @component beshell-FastAccelStepper
     * @function forceStop
     * @return undefined
     * @example
     * ```javascript
     * stepper.forceStop();
     * ```
     */
    JSValue Stepper::forceStop(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        THIS_NCLASS(Stepper, stepper)
        stepper->stepper->forceStop();
        return JS_UNDEFINED;
    }

    /*
     * 执行单个向前步进。
     *
     * @module FastAccelStepper
     * @component beshell-FastAccelStepper
     * @function forwardStep
     * @param blocking:boolean=false 是否阻塞等待完成。
     * @return undefined
     * @example
     * ```javascript
     * stepper.forwardStep(true);
     * ```
     */
    JSValue Stepper::forwardStep(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        THIS_NCLASS(Stepper, stepper)
        bool blocking = false;
        if(argc > 0) {
            blocking = JS_ToBool(ctx, argv[0]);
        }
        stepper->stepper->forwardStep(blocking);
        return JS_UNDEFINED;
    }

    /*
     * 执行单个向后步进。
     *
     * @module FastAccelStepper
     * @component beshell-FastAccelStepper
     * @function backwardStep
     * @param blocking:boolean=false 是否阻塞等待完成。
     * @return undefined
     * @example
     * ```javascript
     * stepper.backwardStep(false);
     * ```
     */
    JSValue Stepper::backwardStep(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        THIS_NCLASS(Stepper, stepper)
        bool blocking = false;
        if(argc > 0) {
            blocking = JS_ToBool(ctx, argv[0]);
        }
        stepper->stepper->backwardStep(blocking);
        return JS_UNDEFINED;
    }

    /*
     * 获取步进电机的当前位置。
     *
     * @module FastAccelStepper
     * @component beshell-FastAccelStepper
     * @function getCurrentPosition
     * @return number 当前位置。
     * @example
     * ```javascript
     * let pos = stepper.getCurrentPosition();
     * console.log(pos);
     * ```
     */
    JSValue Stepper::getCurrentPosition(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        THIS_NCLASS(Stepper, stepper)
        int32_t pos = stepper->stepper->getCurrentPosition();
        return JS_NewInt32(ctx, pos);
    }

    /*
     * 设置步进电机的当前位置。
     *
     * @module FastAccelStepper
     * @component beshell-FastAccelStepper
     * @function setCurrentPosition
     * @param new_pos:number 新的当前位置值。
     * @return undefined
     * @throws 如果参数无效，将抛出异常。
     * @example
     * ```javascript
     * stepper.setCurrentPosition(1000);
     * ```
     */
    JSValue Stepper::setCurrentPosition(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        THIS_NCLASS(Stepper, stepper)
        if(argc < 1) {
            JSTHROW("setCurrentPosition requires 1 argument: new_pos")
        }
        int32_t new_pos;
        JS_ToInt32(ctx, &new_pos, argv[0]);
        stepper->stepper->setCurrentPosition(new_pos);
        return JS_UNDEFINED;
    }

    /*
     * 检查步进电机是否正在运行。
     *
     * @module FastAccelStepper
     * @component beshell-FastAccelStepper
     * @function isRunning
     * @return boolean 是否正在运行。
     * @example
     * ```javascript
     * if (stepper.isRunning()) {
     *     console.log("Motor is running");
     * }
     * ```
     */
    JSValue Stepper::isRunning(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        THIS_NCLASS(Stepper, stepper)
        bool running = stepper->stepper->isRunning();
        return JS_NewBool(ctx, running);
    }

    /*
     * 获取步进电机的当前速度。
     *
     * @module FastAccelStepper
     * @component beshell-FastAccelStepper
     * @function getCurrentSpeedInUs
     * @param realtime:boolean=true 是否实时获取。
     * @return number 当前速度，单位为微秒/步。
     * @example
     * ```javascript
     * let speed = stepper.getCurrentSpeedInUs(true);
     * console.log(speed);
     * ```
     */
    JSValue Stepper::getCurrentSpeedInUs(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        THIS_NCLASS(Stepper, stepper)
        bool realtime = true;
        if(argc > 0) {
            realtime = JS_ToBool(ctx, argv[0]);
        }
        uint32_t speed = stepper->stepper->getCurrentSpeedInUs(realtime);
        return JS_NewUint32(ctx, speed);
    }

    /*
     * 获取步进电机的目标位置。
     *
     * @module FastAccelStepper
     * @component beshell-FastAccelStepper
     * @function targetPos
     * @return number 目标位置。
     * @example
     * ```javascript
     * let target = stepper.targetPos();
     * console.log(target);
     * ```
     */
    JSValue Stepper::targetPos(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        THIS_NCLASS(Stepper, stepper)
        int32_t target = stepper->stepper->targetPos();
        return JS_NewInt32(ctx, target);
    }

    /*
     * 获取步进电机的加减速状态。
     *
     * @module FastAccelStepper
     * @component beshell-FastAccelStepper
     * @function rampState
     * @return number 加减速状态标志。
     */
    JSValue Stepper::rampState(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        THIS_NCLASS(Stepper, stepper)
        uint8_t state = stepper->stepper->rampState();
        return JS_NewUint32(ctx, state);
    }

    /*
     * 获取步进电机的步进引脚号。
     *
     * @module FastAccelStepper
     * @component beshell-FastAccelStepper
     * @function getStepPin
     * @return number 步进引脚的 GPIO 编号。
     * @example
     * ```javascript
     * const pin = stepper.getStepPin();
     * console.log("Step pin:", pin);
     * ```
     */
    /*
     * 获取停止步进电机所需的步数。
     *
     * @module FastAccelStepper
     * @component beshell-FastAccelStepper
     * @function stepsToStop
     * @return number 停止所需的步数。
     * @example
     * ```javascript
     * const steps = stepper.stepsToStop();
     * console.log("Steps to stop:", steps);
     * ```
     */
    JSValue Stepper::stepsToStop(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        THIS_NCLASS(Stepper, stepper)
        uint32_t steps = stepper->stepper->stepsToStop();
        return JS_NewUint32(ctx, steps);
    }

    /*
     * 检查步进电机的命令队列是否为空。
     *
     * @module FastAccelStepper
     * @component beshell-FastAccelStepper
     * @function isQueueEmpty
     * @return boolean 如果队列为空返回 true，否则返回 false。
     * @example
     * ```javascript
     * const empty = stepper.isQueueEmpty();
     * console.log("Queue empty:", empty);
     * ```
     */
    JSValue Stepper::isQueueEmpty(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        THIS_NCLASS(Stepper, stepper)
        bool empty = stepper->stepper->isQueueEmpty();
        return JS_NewBool(ctx, empty);
    }

    /*
     * 检查步进电机的命令队列是否已满。
     *
     * @module FastAccelStepper
     * @component beshell-FastAccelStepper
     * @function isQueueFull
     * @return boolean 如果队列已满返回 true，否则返回 false。
     * @example
     * ```javascript
     * const full = stepper.isQueueFull();
     * console.log("Queue full:", full);
     * ```
     */
    JSValue Stepper::isQueueFull(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        THIS_NCLASS(Stepper, stepper)
        bool full = stepper->stepper->isQueueFull();
        return JS_NewBool(ctx, full);
    }

    /*
     * 检查步进电机的命令队列是否正在运行。
     *
     * @module FastAccelStepper
     * @component beshell-FastAccelStepper
     * @function isQueueRunning
     * @return boolean 如果队列正在运行返回 true，否则返回 false。
     * @example
     * ```javascript
     * const running = stepper.isQueueRunning();
     * console.log("Queue running:", running);
     * ```
     */
    JSValue Stepper::isQueueRunning(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        THIS_NCLASS(Stepper, stepper)
        bool running = stepper->stepper->isQueueRunning();
        return JS_NewBool(ctx, running);
    }

    /*
     * 获取步进电机命令队列中的时钟周期数。
     *
     * @module FastAccelStepper
     * @component beshell-FastAccelStepper
     * @function ticksInQueue
     * @return number 队列中的时钟周期数。
     * @example
     * ```javascript
     * const ticks = stepper.ticksInQueue();
     * console.log("Ticks in queue:", ticks);
     * ```
     */
    JSValue Stepper::ticksInQueue(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        THIS_NCLASS(Stepper, stepper)
        uint32_t ticks = stepper->stepper->ticksInQueue();
        return JS_NewUint32(ctx, ticks);
    }

    /*
     * 获取步进电机命令队列中的条目数。
     *
     * @module FastAccelStepper
     * @component beshell-FastAccelStepper
     * @function queueEntries
     * @return number 队列中的条目数。
     * @example
     * ```javascript
     * const entries = stepper.queueEntries();
     * console.log("Queue entries:", entries);
     * ```
     */
    JSValue Stepper::queueEntries(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        THIS_NCLASS(Stepper, stepper)
        uint8_t entries = stepper->stepper->queueEntries();
        return JS_NewUint32(ctx, entries);
    }

    /*
     * 获取命令执行完成后步进电机将到达的位置。
     *
     * @module FastAccelStepper
     * @component beshell-FastAccelStepper
     * @function getPositionAfterCommandsCompleted
     * @return number 命令执行完成后的位置。
     * @example
     * ```javascript
     * const pos = stepper.getPositionAfterCommandsCompleted();
     * console.log("Position after commands:", pos);
     * ```
     */
    JSValue Stepper::getPositionAfterCommandsCompleted(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        THIS_NCLASS(Stepper, stepper)
        int32_t pos = stepper->stepper->getPositionAfterCommandsCompleted();
        return JS_NewInt32(ctx, pos);
    }

    /*
     * 设置命令执行完成后步进电机将到达的位置。
     *
     * @module FastAccelStepper
     * @component beshell-FastAccelStepper
     * @function setPositionAfterCommandsCompleted
     * @param new_pos:number 新的目标位置值。
     * @return undefined
     * @throws 如果参数无效，将抛出异常。
     * @example
     * ```javascript
     * stepper.setPositionAfterCommandsCompleted(5000);
     * ```
     */
    JSValue Stepper::setPositionAfterCommandsCompleted(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        THIS_NCLASS(Stepper, stepper)
        if(argc < 1) {
            JSTHROW("setPositionAfterCommandsCompleted requires 1 argument: new_pos")
        }
        int32_t new_pos;
        JS_ToInt32(ctx, &new_pos, argv[0]);
        stepper->stepper->setPositionAfterCommandsCompleted(new_pos);
        return JS_UNDEFINED;
    }

    /*
     * 获取命令执行完成后步进电机的周期（以微秒为单位）。
     *
     * @module FastAccelStepper
     * @component beshell-FastAccelStepper
     * @function getPeriodInUsAfterCommandsCompleted
     * @return number 命令执行完成后的周期，单位为微秒。
     * @example
     * ```javascript
     * const period = stepper.getPeriodInUsAfterCommandsCompleted();
     * console.log("Period after commands (us):", period);
     * ```
     */
    JSValue Stepper::getPeriodInUsAfterCommandsCompleted(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        THIS_NCLASS(Stepper, stepper)
        uint32_t period = stepper->stepper->getPeriodInUsAfterCommandsCompleted();
        return JS_NewUint32(ctx, period);
    }

    /*
     * 获取命令执行完成后步进电机的周期（以时钟周期为单位）。
     *
     * @module FastAccelStepper
     * @component beshell-FastAccelStepper
     * @function getPeriodInTicksAfterCommandsCompleted
     * @return number 命令执行完成后的周期，单位为时钟周期。
     * @example
     * ```javascript
     * const period = stepper.getPeriodInTicksAfterCommandsCompleted();
     * console.log("Period after commands (ticks):", period);
     * ```
     */
    JSValue Stepper::getPeriodInTicksAfterCommandsCompleted(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        THIS_NCLASS(Stepper, stepper)
        uint32_t period = stepper->stepper->getPeriodInTicksAfterCommandsCompleted();
        return JS_NewUint32(ctx, period);
    }

    /*
     * 检查步进电机是否正在连续运行。
     *
     * @module FastAccelStepper
     * @component beshell-FastAccelStepper
     * @function isRunningContinuously
     * @return boolean 如果正在连续运行返回 true，否则返回 false。
     * @example
     * ```javascript
     * const continuous = stepper.isRunningContinuously();
     * console.log("Running continuously:", continuous);
     * ```
     */
    JSValue Stepper::isRunningContinuously(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        THIS_NCLASS(Stepper, stepper)
        bool running = stepper->stepper->isRunningContinuously();
        return JS_NewBool(ctx, running);
    }

    /*
     * 检查步进电机是否正在停止。
     *
     * @module FastAccelStepper
     * @component beshell-FastAccelStepper
     * @function isStopping
     * @return boolean 如果正在停止返回 true，否则返回 false。
     * @example
     * ```javascript
     * const stopping = stepper.isStopping();
     * console.log("Is stopping:", stopping);
     * ```
     */
    JSValue Stepper::isStopping(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        THIS_NCLASS(Stepper, stepper)
        bool stopping = stepper->stepper->isStopping();
        return JS_NewBool(ctx, stopping);
    }

    
    JSValue Stepper::waitStop(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        THIS_NCLASS(Stepper, stepper)

        JSValue resolving_funcs[2];
        JSValue promise = JS_NewPromiseCapability(ctx, resolving_funcs);
        JSValue resolve = resolving_funcs[0];
        JSValue reject = resolving_funcs[1];

        struct TaskData {
            JSContext* ctx;
            JSValue resolve;
            Stepper* stepper;
        };

        TaskData* data = new TaskData{ctx, JS_DupValue(ctx, resolve), stepper};

        xTaskCreate([](void* param) {
            TaskData* d = (TaskData*)param;
            while (d->stepper->stepper->isRunning()) {
                vTaskDelay(pdMS_TO_TICKS(10));
            }
            // Resolve the promise in the main thread
            JS_EnqueueJob(d->ctx, [](JSContext* ctx, int argc, JSValueConst* argv) -> JSValue {
                JSValue resolve = argv[0];
                JSValue ret = JS_Call(ctx, resolve, JS_UNDEFINED, 0, nullptr);
                JS_FreeValue(ctx, ret);
                JS_FreeValue(ctx, resolve);
                return JS_UNDEFINED;
            }, 1, &d->resolve);
            delete d;
            vTaskDelete(NULL);
        }, "waitStop_task", 4096, data, 5, NULL);

        return promise;
    }

    JSValue Stepper::setLimitPin(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        THIS_NCLASS(Stepper, stepper)
        
        ARGV_TO_GPIO(0, pin)
        bool activeLow = true ;
        if(argc > 1) {
            activeLow = JS_ToBool(ctx, argv[1]);
        }

        // 配置 GPIO
        gpio_config_t io_conf = {};
        io_conf.intr_type = activeLow ? GPIO_INTR_NEGEDGE : GPIO_INTR_POSEDGE;
        io_conf.mode = GPIO_MODE_INPUT;
        io_conf.pin_bit_mask = (1ULL << pin);
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
        gpio_config(&io_conf);

        // 安装 ISR 服务（如果尚未安装）
        static bool isr_installed = false;
        if (!isr_installed) {
            gpio_install_isr_service(0);
            isr_installed = true;
        }

        // 创建 ISR 数据并存储
        ISRData* data = new ISRData{stepper, (uint8_t)pin};
        stepper->limitPins.push_back(data);

        // 添加 ISR 处理器，允许多个限位引脚并保持之前引脚的中断有效
        gpio_isr_handler_add(pin, Stepper::limitISR, data);
        
        return JS_UNDEFINED;
    }

    void Stepper::limitISR(void* arg) {
        ISRData* data = (ISRData*)arg;
        Stepper* stepper = data->stepper;
        uint8_t pin = data->pin;

        if(!stepper->stepper->isRunning()) {
            return ;
        }

        TickType_t tick = xTaskGetTickCountFromISR();
        TickType_t last = stepper->lastLimitEventTick;
        if(last != 0 && (tick - last) < LIMIT_DEBOUNCE_TICKS) {
            return;
        }
        stepper->lastLimitEventTick = tick;

        stepper->stepper->forceStop();

        LimitEventParam evt = { pin };
        stepper->emitNativeEvent(&evt, true);
    }

    void Stepper::onNativeEvent(JSContext *ctx, void * param) {
        LimitEventParam * evt = (LimitEventParam *)param;
        emitSyncFree("limit", {JS_NewUint32(ctx, evt->pin)});
    }
}

