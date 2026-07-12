#include <stdint.h>

/*
 * Stock audio.primary.msm8996.so expects these globals to be visible before
 * the HAL is loaded through the vendor audio service.
 */
__attribute__((visibility("default"))) int32_t dolby_status = 0;
__attribute__((visibility("default"))) int32_t gAllowUseHiFiSession = 0;
__attribute__((visibility("default"))) int32_t gUseAkmSpeaker = 0;
__attribute__((visibility("default"))) uint32_t hal_log_mask = 0;
