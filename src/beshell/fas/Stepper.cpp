#include "Stepper.hpp"
#include <beshell/module/gpio/GPIO.hpp>
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

    static constexpr TickType_t LIMIT_DEBOUNCE_TICKS = pdMS_TO_TICKS(10);


    Stepper::Stepper(JSContext * ctx, FastAccelStepper * stepper)
        : EventEmitter(ctx,build(ctx,JS_NULL))
        , stepper(stepper)
    {
        enableNativeEvent(ctx, sizeof(LimitEventParam), 10) ;
        xTaskCreate(limitTaskFunc, "fas_limit", 2048, this, configMAX_PRIORITIES - 1, &_limitTaskHandle);
    }

    Stepper::~Stepper() {
        if (_limitTaskHandle) {
            vTaskDelete(_limitTaskHandle);
        }
        for(auto data : (std::vector<ISRData*>)limitPins) {
            gpio_isr_handler_remove((gpio_num_t)data->pin);
            delete data;
        }
        limitPins.clear();
    }

    JSValue Stepper::constructor(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        JSTHROW("call fas.stepperConnectToPin() to create stepper") ;
    }

    /**
     * 等待电机停止运行。返回一个 Promise，当电机完全停止后 resolve。
     * 内部通过定时轮询 isRunning() 状态实现，不会阻塞 JS 主线程。
     *
     * 例子:
     * ```javascript
     * stepper.move(1000);
     * await stepper.waitStop();
     * console.log('电机已停止，当前位置:', stepper.getCurrentPosition());
     *
     * // 配合限位使用
     * stepper.setLimitPin(17);
     * await stepper.waitStop();
     * ```
     *
     * @module fas
     * @class Stepper
     * @component beshell-FastAccelStepper
     * @method waitStop
     * @return Promise\<void\> 电机停止后 resolve 的 Promise
     */
    JSValue Stepper::waitStop(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        THIS_NCLASS(Stepper, stepper)

        // printf("isRunning(): %d\n", stepper->stepper->isRunning());

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

    /**
     * 为步进电机设置限位引脚。可以多次调用此方法为一个电机添加多个限位引脚。
     * 当限位引脚触发时，电机将立即停止并触发 "limit" 事件。
     * 如需在限位触发后反向移出限位区域，可调用 {@link ignoreLimit} 临时屏蔽限位信号，
     * 移动完成后再恢复监听。
     *
     * 注意：此方法不会检查引脚是否已被注册。如果对同一个引脚重复调用，
     * 会导致 ISR 数据泄漏和不可预知的错误行为，请确保每个引脚只设置一次。
     *
     * 例子:
     * ```javascript
     * // 设置多个限位引脚
     * stepper.setLimitPin(17);        // GPIO17，默认低电平有效
     * stepper.setLimitPin(18, false); // GPIO18，高电平有效
     *
     * stepper.on('limit', (pin) => {
     *     console.log('限位触发:', pin);
     * });
     *
     * // 临时忽略限位
     * stepper.ignoreLimit(true);
     * ```
     *
     * @module fas
     * @class Stepper
     * @component beshell-FastAccelStepper
     * @method setLimitPin
     * @param pin:number 限位引脚 GPIO 编号
     * @param activeLow:boolean=true 触发电平，true 为下降沿触发（低电平有效），false 为上升沿触发
     * @return undefined
     */
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
            be::GPIO::installISR(0) ;
            isr_installed = true;
        }

        // 创建 ISR 数据并存储
        ISRData* data = new ISRData{stepper, (uint8_t)pin};
        stepper->limitPins.push_back(data);

        // 添加 ISR 处理器，允许多个限位引脚并保持之前引脚的中断有效
        gpio_isr_handler_add(pin, Stepper::limitISR, data);

        return JS_UNDEFINED;
    }

    void Stepper::limitTaskFunc(void* arg) {
        Stepper* stepper = (Stepper*)arg;
        while (true) {
            uint32_t pin;
            xTaskNotifyWait(0, 0xFFFFFFFF, &pin, portMAX_DELAY);
            stepper->stepper->forceStop();
            LimitEventParam evt = { (uint8_t)pin };
            stepper->emitNativeEvent(&evt);
        }
    }

    void IRAM_ATTR Stepper::limitISR(void* arg) {
        ISRData* data = (ISRData*)arg;
        Stepper* stepper = data->stepper;
        uint8_t pin = data->pin;

        if(stepper->ignoreLimitFlag) {
            return ;
        }

        TickType_t tick = xTaskGetTickCountFromISR();
        TickType_t last = stepper->lastLimitEventTick;
        if(last != 0 && (tick - last) < LIMIT_DEBOUNCE_TICKS) {
            return;
        }
        stepper->lastLimitEventTick = tick;

        BaseType_t higherPriorityTaskWoken = pdFALSE;
        xTaskNotifyFromISR(stepper->_limitTaskHandle, pin, eSetValueWithOverwrite, &higherPriorityTaskWoken);
        portYIELD_FROM_ISR(higherPriorityTaskWoken);
    }

    /**
     * 临时忽略限位引脚的触发信号。通常用于撞到限位后需要反向移动电机离开限位位置的场景。
     *
     * 例子:
     * ```javascript
     * stepper.setLimitPin(17);
     * stepper.move(1000);
     * await stepper.waitStop();
     *
     * // 撞到限位后，忽略限位并反向移出
     * stepper.ignoreLimit(true);
     * await stepper.move(-200);
     * stepper.ignoreLimit(false);
     * ```
     *
     * @module fas
     * @class Stepper
     * @component beshell-FastAccelStepper
     * @method ignoreLimit
     * @param ignore:boolean=true 是否忽略限位，true 忽略，false 恢复监听
     * @return undefined
     */
    JSValue Stepper::ignoreLimit(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        THIS_NCLASS(Stepper, stepper)
        bool ignore = true;
        if(argc > 0) {
            ignore = JS_ToBool(ctx, argv[0]);
        }
        stepper->ignoreLimitFlag = ignore;
        return JS_UNDEFINED;
    }

    void Stepper::onNativeEvent(JSContext *ctx, void * param) {
        LimitEventParam * evt = (LimitEventParam *)param;
        emitSyncFree("limit", {JS_NewUint32(ctx, evt->pin)});
    }
}
