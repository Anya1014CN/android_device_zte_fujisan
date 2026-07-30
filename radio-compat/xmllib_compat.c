/*
 * Compatibility surface for the two obsolete Qualcomm xmllib entry points
 * imported by the Oreo libconfigdb blob.  AOSP no longer ships libxml.so.
 *
 * Returning an error makes libconfigdb use its existing missing-config path;
 * it is preferable to a loader failure that prevents the radio daemon from
 * starting at all.  These variadic declarations preserve the legacy call ABI
 * without reproducing the old private parser implementation.
 */
#define LOG_TAG "fujisan-xmllib"

#include <log/log.h>

int xmllib_parser_parse(void *parser, ...)
{
    (void)parser;
    ALOGW("legacy xmllib parser requested; configuration unavailable");
    return -1;
}

void xmllib_parser_free(void *parser, ...)
{
    (void)parser;
}
