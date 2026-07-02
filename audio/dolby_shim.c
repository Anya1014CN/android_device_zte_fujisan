#include <stdint.h>

/*
 * Stock audio.primary.msm8996.so expects this global object to already exist
 * in the process namespace before the HAL is dlopen()'d.
 */
__attribute__((visibility("default"))) int32_t dolby_status = 0;
__attribute__((visibility("default"))) int32_t gAllowUseHiFiSession = 0;
__attribute__((visibility("default"))) int32_t gUseAkmSpeaker = 0;
__attribute__((visibility("default"))) uint32_t hal_log_mask = 0;
