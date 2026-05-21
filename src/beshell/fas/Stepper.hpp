#pragma once

#include <beshell/EventEmitter.hpp>
#include <FastAccelStepper.h>

namespace be::fas {
    struct ISRData;

    class Stepper: public be::EventEmitter {
        DECLARE_NCLASS_META
    private:
        static std::vector<JSCFunctionListEntry> methods ;
        // static std::vector<JSCFunctionListEntry> staticMethods ;

        ::FastAccelStepper * stepper = nullptr ;

        std::vector<ISRData*> limitPins;
        volatile bool ignoreLimitFlag = 0;
        volatile TickType_t lastLimitEventTick = 0;
        TaskHandle_t _limitTaskHandle = nullptr;

        static void IRAM_ATTR limitISR(void* arg) ;
        static void limitTaskFunc(void* arg) ;

    public:
        Stepper(JSContext * ctx, ::FastAccelStepper * stepper) ;
        ~Stepper() ;
        static JSValue constructor(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) ;

        // Configuration and initialization methods
        static JSValue setDirectionPin(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) ;
        static JSValue setEnablePin(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) ;
        static JSValue setAutoEnable(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) ;
        static JSValue setDelayToEnable(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) ;
        static JSValue setDelayToDisable(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) ;
        static JSValue setSpeedInHz(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) ;
        static JSValue setSpeedInUs(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) ;
        static JSValue setSpeedInTicks(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) ;
        static JSValue setAcceleration(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) ;
        static JSValue setLinearAcceleration(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) ;
        static JSValue setJumpStart(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) ;

        // Motion control methods
        static JSValue move(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) ;
        static JSValue moveTo(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) ;
        static JSValue runForward(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) ;
        static JSValue runBackward(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) ;
        static JSValue keepRunning(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) ;
        static JSValue moveByAcceleration(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) ;
        static JSValue moveTimed(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) ;
        static JSValue stopMove(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) ;
        static JSValue forceStop(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) ;
        static JSValue forwardStep(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) ;
        static JSValue backwardStep(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) ;

        // Status query methods
        static JSValue getCurrentPosition(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) ;
        static JSValue isRunning(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) ;
        static JSValue getCurrentSpeedInUs(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) ;
        static JSValue targetPos(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) ;
        static JSValue rampState(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) ;

        // Additional getter methods
        static JSValue getStepPin(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) ;
        static JSValue getDirectionPin(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) ;
        static JSValue directionPinHighCountsUp(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) ;
        static JSValue getEnablePinHighActive(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) ;
        static JSValue getEnablePinLowActive(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) ;
        static JSValue getMaxSpeedInHz(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) ;
        static JSValue getMaxSpeedInUs(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) ;
        static JSValue getMaxSpeedInTicks(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) ;
        static JSValue getSpeedInUs(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) ;
        static JSValue getSpeedInTicks(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) ;
        static JSValue getSpeedInMilliHz(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) ;
        static JSValue getAcceleration(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) ;
        static JSValue getCurrentAcceleration(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) ;
        static JSValue stepsToStop(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) ;
        static JSValue isQueueEmpty(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) ;
        static JSValue isQueueFull(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) ;
        static JSValue isQueueRunning(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) ;
        static JSValue ticksInQueue(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) ;
        static JSValue queueEntries(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) ;
        static JSValue getPositionAfterCommandsCompleted(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) ;
        static JSValue getPeriodInUsAfterCommandsCompleted(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) ;
        static JSValue getPeriodInTicksAfterCommandsCompleted(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) ;
        static JSValue isRunningContinuously(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) ;
        static JSValue isStopping(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) ;

        // Additional setter
        static JSValue setCurrentPosition(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) ;
        static JSValue setPositionAfterCommandsCompleted(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) ;

        static JSValue waitStop(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) ;
        static JSValue setLimitPin(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) ;
        static JSValue ignoreLimit(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) ;

    protected:
        virtual void onNativeEvent(JSContext *ctx, void * param) ;

    } ;
}