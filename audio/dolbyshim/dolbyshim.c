/*
 * The Oreo ZTE audio HAL links this symbol to report optional Dolby state.
 * Dolby processing is not part of the Android 12 audio route, so retaining a
 * zero-initialized status preserves the HAL ABI without enabling an effect.
 */
/*
 * The Oreo AK4962 audio HAL exported these policy flags from a companion
 * library.  The Android 12 audio stack does not provide that library, so keep
 * the optional vendor DSP paths disabled while allowing the HAL itself to
 * initialize its ALSA routes.
 */
int dolby_status;
int gAllowUseHiFiSession;
int gUseAkmSpeaker;
int hal_log_mask;
