#pragma once

#include "common.hpp"

namespace lsplant::art::jit {
class JitCodeCache {
    CREATE_MEM_FUNC_SYMBOL_ENTRY(void, MoveObsoleteMethod, JitCodeCache *thiz,
                                 ArtMethod *old_method, ArtMethod *new_method) {
        if (MoveObsoleteMethodSym) [[likely]] {
            MoveObsoleteMethodSym(thiz, old_method, new_method);
        } else {
            // fallback to set data
            new_method->SetData(old_method->GetData());
            old_method->SetData(nullptr);
        }
    }

    CREATE_MEM_HOOK_STUB_ENTRY("_ZN3art3jit12JitCodeCache19GarbageCollectCacheEPNS_6ThreadE", void,
                               GarbageCollectCache, (JitCodeCache * thiz, Thread *self), {
                                   auto movements = GetJitMovements();
                                   LOGD("Before jit cache gc, moving %zu hooked methods",
                                        movements.size());
                                   for (auto [target, backup] : movements) {
                                       MoveObsoleteMethod(thiz, target, backup);
                                   }
                                   backup(thiz, self);
                               });

    CREATE_MEM_FUNC_SYMBOL_ENTRY(void*, GetProfilingInfo, JitCodeCache *thiz,
                                 ArtMethod *old_method, Thread *self) {
        if (GetProfilingInfoSym) [[likely]] {
            return GetProfilingInfoSym(thiz, old_method, self);
        } else {
            return nullptr;
        }
    }

    CREATE_MEM_HOOK_STUB_ENTRY("_ZN3art3jit12JitCodeCache19ResetHotnessCounterEPNS_9ArtMethodEPNS_6ThreadE", void,
                               ResetHotnessCounter, (JitCodeCache * thiz, ArtMethod* method, Thread *self), {
                                   if (GetProfilingInfo(thiz, method, self) == nullptr) {
                                       ArtMethod *original = nullptr;
                                       for (auto [orig, bak]: hooked_methods_) {
                                           if (bak.second == method) {
                                               original = orig;
                                               break;
                                           }
                                       }
                                       if (original) {
                                           LOGD("fix ResetHotnessCounter: backup %p -> original %p", method, original);
                                           method = original;
                                       } else {
                                           LOGE("fix ResetHotnessCounter: no backup found for %p", method);
                                       }
                                   }
                                   backup(thiz, method, self);
                               });

public:
    static bool Init(const HookHandler &handler) {
        auto sdk_int = GetAndroidApiLevel();
        if (sdk_int >= __ANDROID_API_O__) [[likely]] {
            if (!RETRIEVE_MEM_FUNC_SYMBOL(
                    MoveObsoleteMethod,
                    "_ZN3art3jit12JitCodeCache18MoveObsoleteMethodEPNS_9ArtMethodES3_"))
                [[unlikely]] {
                return false;
            }
        }
        if (sdk_int >= __ANDROID_API_N__) [[likely]] {
            if (!HookSyms(handler, GarbageCollectCache)) [[unlikely]] {
                return false;
            }
        }
        if (!RETRIEVE_MEM_FUNC_SYMBOL(GetProfilingInfo, "_ZN3art3jit12JitCodeCache16GetProfilingInfoEPNS_9ArtMethodEPNS_6ThreadE")) {
            LOGE("GetProfilingInfo not found!");
        } else {
            if (!HookSyms(handler, ResetHotnessCounter)) {
                LOGE("failed to hook ResetHotnessCounter!");
            }
        }
        return true;
    }
};
}  // namespace lsplant::art::jit
