/*
 * The Oreo QCRIL blob constructs android::Parcel objects in its own stack
 * frames. Android 12 enlarged that C++ class, so calling its constructor
 * overwrites QCRIL's stack canary. Keep the legacy object opaque and retain
 * just the AOSP Parcel wire-format operations used by this blob.
 */
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>

namespace {

struct LegacyParcel {
    const void *owner;
    uint8_t *data;
    size_t size;
    size_t capacity;
    size_t position;
    LegacyParcel *next;
};

pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
LegacyParcel *g_parcels;

bool is_qcril_parcel_call(void *caller)
{
    Dl_info info = {};
    if (dladdr(caller, &info) == 0 || info.dli_fname == NULL) {
        return false;
    }
    return strstr(info.dli_fname, "/libril-qc-qmi-1.so") != NULL;
}

template <typename Function>
Function platform_parcel_function(const char *name)
{
    return reinterpret_cast<Function>(dlsym(RTLD_NEXT, name));
}

LegacyParcel *find_parcel_locked(const void *owner)
{
    for (LegacyParcel *parcel = g_parcels; parcel != NULL; parcel = parcel->next) {
        if (parcel->owner == owner) {
            return parcel;
        }
    }
    return NULL;
}

LegacyParcel *find_or_create_parcel(const void *owner)
{
    pthread_mutex_lock(&g_lock);
    LegacyParcel *parcel = find_parcel_locked(owner);
    if (parcel == NULL) {
        parcel = static_cast<LegacyParcel *>(calloc(1, sizeof(*parcel)));
        if (parcel != NULL) {
            parcel->owner = owner;
            parcel->next = g_parcels;
            g_parcels = parcel;
        }
    }
    pthread_mutex_unlock(&g_lock);
    return parcel;
}

int reserve_and_write(LegacyParcel *parcel, const void *source, size_t length)
{
    const size_t aligned_length = (length + 3U) & ~static_cast<size_t>(3U);
    if (length > SIZE_MAX - parcel->position ||
            aligned_length > SIZE_MAX - parcel->position) {
        return -12;
    }

    const size_t end = parcel->position + aligned_length;
    if (end > parcel->capacity) {
        size_t capacity = parcel->capacity == 0 ? 64 : parcel->capacity;
        while (capacity < end) {
            if (capacity > SIZE_MAX / 2) {
                capacity = end;
                break;
            }
            capacity *= 2;
        }
        uint8_t *data = static_cast<uint8_t *>(realloc(parcel->data, capacity));
        if (data == NULL) {
            return -12;
        }
        parcel->data = data;
        parcel->capacity = capacity;
    }

    if (length != 0) {
        memcpy(parcel->data + parcel->position, source, length);
    }
    if (aligned_length != length) {
        memset(parcel->data + parcel->position + length, 0, aligned_length - length);
    }
    parcel->position += aligned_length;
    if (parcel->position > parcel->size) {
        parcel->size = parcel->position;
    }
    return 0;
}

}  // namespace

extern "C" void parcel_constructor(void *owner)
        __asm__("_ZN7android6ParcelC1Ev");
extern "C" void parcel_constructor(void *owner)
{
    if (!is_qcril_parcel_call(__builtin_return_address(0))) {
        typedef void (*Function)(void *);
        Function function = platform_parcel_function<Function>("_ZN7android6ParcelC1Ev");
        if (function != NULL) {
            function(owner);
        }
        return;
    }
    (void)find_or_create_parcel(owner);
}

extern "C" void parcel_destructor(void *owner)
        __asm__("_ZN7android6ParcelD1Ev");
extern "C" void parcel_destructor(void *owner)
{
    if (!is_qcril_parcel_call(__builtin_return_address(0))) {
        typedef void (*Function)(void *);
        Function function = platform_parcel_function<Function>("_ZN7android6ParcelD1Ev");
        if (function != NULL) {
            function(owner);
        }
        return;
    }
    pthread_mutex_lock(&g_lock);
    LegacyParcel **current = &g_parcels;
    while (*current != NULL && (*current)->owner != owner) {
        current = &(*current)->next;
    }
    if (*current != NULL) {
        LegacyParcel *parcel = *current;
        *current = parcel->next;
        free(parcel->data);
        free(parcel);
    }
    pthread_mutex_unlock(&g_lock);
}

extern "C" const void *parcel_data(const void *owner)
        __asm__("_ZNK7android6Parcel4dataEv");
extern "C" const void *parcel_data(const void *owner)
{
    if (!is_qcril_parcel_call(__builtin_return_address(0))) {
        typedef const void *(*Function)(const void *);
        Function function = platform_parcel_function<Function>("_ZNK7android6Parcel4dataEv");
        return function == NULL ? NULL : function(owner);
    }
    LegacyParcel *parcel = find_or_create_parcel(owner);
    return parcel == NULL ? NULL : parcel->data;
}

extern "C" size_t parcel_data_size(const void *owner)
        __asm__("_ZNK7android6Parcel8dataSizeEv");
extern "C" size_t parcel_data_size(const void *owner)
{
    if (!is_qcril_parcel_call(__builtin_return_address(0))) {
        typedef size_t (*Function)(const void *);
        Function function = platform_parcel_function<Function>("_ZNK7android6Parcel8dataSizeEv");
        return function == NULL ? 0 : function(owner);
    }
    LegacyParcel *parcel = find_or_create_parcel(owner);
    return parcel == NULL ? 0 : parcel->size;
}

extern "C" int parcel_set_data_position(const void *owner, size_t position)
        __asm__("_ZNK7android6Parcel15setDataPositionEm");
extern "C" int parcel_set_data_position(const void *owner, size_t position)
{
    if (!is_qcril_parcel_call(__builtin_return_address(0))) {
        typedef int (*Function)(const void *, size_t);
        Function function = platform_parcel_function<Function>("_ZNK7android6Parcel15setDataPositionEm");
        return function == NULL ? -22 : function(owner, position);
    }
    LegacyParcel *parcel = find_or_create_parcel(owner);
    if (parcel == NULL || position > parcel->size) {
        return -22;
    }
    parcel->position = position;
    return 0;
}

extern "C" int parcel_write(void *owner, const void *source, size_t length)
        __asm__("_ZN7android6Parcel5writeEPKvm");
extern "C" int parcel_write(void *owner, const void *source, size_t length)
{
    if (!is_qcril_parcel_call(__builtin_return_address(0))) {
        typedef int (*Function)(void *, const void *, size_t);
        Function function = platform_parcel_function<Function>("_ZN7android6Parcel5writeEPKvm");
        return function == NULL ? -22 : function(owner, source, length);
    }
    LegacyParcel *parcel = find_or_create_parcel(owner);
    return parcel == NULL ? -12 : reserve_and_write(parcel, source, length);
}

extern "C" int parcel_write_int32(void *owner, int32_t value)
        __asm__("_ZN7android6Parcel10writeInt32Ei");
extern "C" int parcel_write_int32(void *owner, int32_t value)
{
    if (!is_qcril_parcel_call(__builtin_return_address(0))) {
        typedef int (*Function)(void *, int32_t);
        Function function = platform_parcel_function<Function>("_ZN7android6Parcel10writeInt32Ei");
        return function == NULL ? -22 : function(owner, value);
    }
    LegacyParcel *parcel = find_or_create_parcel(owner);
    return parcel == NULL ? -12 : reserve_and_write(parcel, &value, sizeof(value));
}

extern "C" int parcel_write_string16(void *owner, const uint16_t *value, size_t length)
        __asm__("_ZN7android6Parcel13writeString16EPKDsm");
extern "C" int parcel_write_string16(void *owner, const uint16_t *value, size_t length)
{
    if (!is_qcril_parcel_call(__builtin_return_address(0))) {
        typedef int (*Function)(void *, const uint16_t *, size_t);
        Function function = platform_parcel_function<Function>("_ZN7android6Parcel13writeString16EPKDsm");
        return function == NULL ? -22 : function(owner, value, length);
    }
    if (length > INT32_MAX || length == SIZE_MAX) {
        return -22;
    }
    if (value == NULL) {
        return parcel_write_int32(owner, -1);
    }
    int result = parcel_write_int32(owner, static_cast<int32_t>(length));
    if (result != 0) {
        return result;
    }
    const size_t byte_count = (length + 1) * sizeof(*value);
    uint16_t *terminated = static_cast<uint16_t *>(malloc(byte_count));
    if (terminated == NULL) {
        return -12;
    }
    memcpy(terminated, value, length * sizeof(*value));
    terminated[length] = 0;
    LegacyParcel *parcel = find_or_create_parcel(owner);
    result = parcel == NULL ? -12 : reserve_and_write(parcel, terminated, byte_count);
    free(terminated);
    return result;
}
