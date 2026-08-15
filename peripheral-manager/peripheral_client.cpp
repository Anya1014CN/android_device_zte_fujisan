/*
 * Copyright (C) 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 *
 * QTI Peripheral Manager client implementation.
 *
 * The exported C API and Binder transactions follow the public QTI
 * pm-service.h interface.  The stock Oreo implementation constructs the
 * obsolete Parcel C++ ABI on modern Android and corrupts the caller stack.
 */

#include <binder/IBinder.h>
#include <binder/IInterface.h>
#include <binder/IServiceManager.h>
#include <binder/Parcel.h>
#include <binder/ProcessState.h>
#include <log/log.h>
#include <utils/Errors.h>
#include <utils/String16.h>
#include <utils/String8.h>

#include <atomic>
#include <cstdint>
#include <mutex>
#include <new>

namespace {

constexpr char kVendorBinderDriver[] = "/dev/vndbinder";
constexpr char kServiceName[] = "vendor.qcom.PeripheralManager";
constexpr char kManagerDescriptor[] = "vendor.qcom.IPeripheralManager";
constexpr char kCallbackDescriptor[] = "com.qualcomm.IPeriperalManagerCb";

constexpr uint32_t kTransactionRegister = android::IBinder::FIRST_CALL_TRANSACTION;
constexpr uint32_t kTransactionDisconnect = kTransactionRegister + 1;
constexpr uint32_t kTransactionConnect = kTransactionRegister + 2;
constexpr uint32_t kTransactionUnregister = kTransactionRegister + 3;
constexpr uint32_t kTransactionAcknowledge = kTransactionRegister + 4;

constexpr int kPmSuccess = 0;
constexpr int kPmFailed = -1;
constexpr int kPmUnsupported = 1;

enum PmEvent : int32_t {
    kEventPeripheralGoingOffline,
    kEventPeripheralOffline,
    kEventPeripheralGoingOnline,
    kEventPeripheralOnline,
};

using PmClientNotifier = void (*)(void *, PmEvent);

}  // namespace

namespace android {

class IPeriperalManagerCb : public android::IInterface {
public:
    DECLARE_META_INTERFACE(PeriperalManagerCb);
    virtual void notifyCallback(int32_t event) = 0;
};

class BpPeriperalManagerCb final
        : public android::BpInterface<IPeriperalManagerCb> {
public:
    explicit BpPeriperalManagerCb(const android::sp<android::IBinder> &remote)
        : BpInterface<IPeriperalManagerCb>(remote) {}

    void notifyCallback(int32_t event) override {
        android::Parcel data;
        data.writeInterfaceToken(IPeriperalManagerCb::getInterfaceDescriptor());
        data.writeInt32(event);
        remote()->transact(android::IBinder::FIRST_CALL_TRANSACTION, data, nullptr,
                android::IBinder::FLAG_ONEWAY);
    }
};

class BnPeriperalManagerCb : public android::BnInterface<IPeriperalManagerCb> {
public:
    android::status_t onTransact(uint32_t code, const android::Parcel &data,
            android::Parcel *reply, uint32_t flags = 0) override {
        (void)reply;
        (void)flags;
        if (code != android::IBinder::FIRST_CALL_TRANSACTION ||
                !data.checkInterface(this)) {
            return android::BBinder::onTransact(code, data, reply, flags);
        }
        notifyCallback(data.readInt32());
        return android::NO_ERROR;
    }
};

class IPeripheralManager : public android::IInterface {
public:
    DECLARE_META_INTERFACE(PeripheralManager);
    virtual int registar(const android::String8 &peripheral, const android::String8 &client,
            const android::sp<IPeriperalManagerCb> &callback, int64_t *clientId,
            int64_t *state) = 0;
    virtual int disconnect(int64_t clientId) = 0;
    virtual int connect(int64_t clientId) = 0;
    virtual int unregister(int64_t clientId) = 0;
    virtual int acknowledge(int64_t clientId, int32_t event) = 0;
    virtual android::String8 getPeripheralInfo() = 0;
};

class BpPeripheralManager final : public android::BpInterface<IPeripheralManager> {
public:
    explicit BpPeripheralManager(const android::sp<android::IBinder> &remote)
        : BpInterface<IPeripheralManager>(remote) {}

    int registar(const android::String8 &peripheral, const android::String8 &client,
            const android::sp<IPeriperalManagerCb> &callback, int64_t *clientId,
            int64_t *state) override {
        android::Parcel data;
        android::Parcel reply;
        data.writeInterfaceToken(IPeripheralManager::getInterfaceDescriptor());
        data.writeString8(peripheral);
        data.writeString8(client);
        data.writeStrongBinder(android::IInterface::asBinder(callback));
        if (remote()->transact(kTransactionRegister, data, &reply) != android::NO_ERROR) {
            return kPmFailed;
        }
        if (clientId != nullptr) *clientId = reply.readInt64();
        else (void)reply.readInt64();
        if (state != nullptr) *state = reply.readInt64();
        else (void)reply.readInt64();
        return reply.readInt32();
    }

    int disconnect(int64_t clientId) override { return transactClient(kTransactionDisconnect, clientId); }
    int connect(int64_t clientId) override { return transactClient(kTransactionConnect, clientId); }
    int unregister(int64_t clientId) override { return transactClient(kTransactionUnregister, clientId); }

    int acknowledge(int64_t clientId, int32_t event) override {
        android::Parcel data;
        android::Parcel reply;
        data.writeInterfaceToken(IPeripheralManager::getInterfaceDescriptor());
        data.writeInt64(clientId);
        data.writeInt32(event);
        return remote()->transact(kTransactionAcknowledge, data, &reply) == android::NO_ERROR ?
                reply.readInt32() : kPmFailed;
    }

    android::String8 getPeripheralInfo() override {
        android::Parcel data;
        android::Parcel reply;
        data.writeInterfaceToken(IPeripheralManager::getInterfaceDescriptor());
        return remote()->transact(kTransactionAcknowledge + 1, data, &reply) == android::NO_ERROR ?
                reply.readString8() : android::String8();
    }

private:
    int transactClient(uint32_t transaction, int64_t clientId) {
        android::Parcel data;
        android::Parcel reply;
        data.writeInterfaceToken(IPeripheralManager::getInterfaceDescriptor());
        data.writeInt64(clientId);
        return remote()->transact(transaction, data, &reply) == android::NO_ERROR ?
                reply.readInt32() : kPmFailed;
    }
};

class BnPeripheralManager : public android::BnInterface<IPeripheralManager> {
public:
    android::status_t onTransact(uint32_t code, const android::Parcel &data,
            android::Parcel *reply, uint32_t flags = 0) override;
};

android::status_t BnPeripheralManager::onTransact(uint32_t code,
        const android::Parcel &data, android::Parcel *reply, uint32_t flags) {
    (void)flags;
    if (!data.checkInterface(this) || reply == nullptr) {
        return android::BBinder::onTransact(code, data, reply, flags);
    }
    switch (code) {
        case kTransactionRegister: {
            const android::String8 peripheral = data.readString8();
            const android::String8 client = data.readString8();
            const android::sp<IPeriperalManagerCb> callback =
                    IPeriperalManagerCb::asInterface(data.readStrongBinder());
            int64_t clientId = 0;
            int64_t state = 0;
            const int result = registar(peripheral, client, callback, &clientId, &state);
            reply->writeInt64(clientId);
            reply->writeInt64(state);
            reply->writeInt32(result);
            return android::NO_ERROR;
        }
        case kTransactionDisconnect: reply->writeInt32(disconnect(data.readInt64())); return android::NO_ERROR;
        case kTransactionConnect: reply->writeInt32(connect(data.readInt64())); return android::NO_ERROR;
        case kTransactionUnregister: reply->writeInt32(unregister(data.readInt64())); return android::NO_ERROR;
        case kTransactionAcknowledge: reply->writeInt32(acknowledge(data.readInt64(), data.readInt32())); return android::NO_ERROR;
        case kTransactionAcknowledge + 1: reply->writeString8(getPeripheralInfo()); return android::NO_ERROR;
        default: return android::BBinder::onTransact(code, data, reply, flags);
    }
}

IMPLEMENT_META_INTERFACE(PeriperalManagerCb, "com.qualcomm.IPeriperalManagerCb");
IMPLEMENT_META_INTERFACE(PeripheralManager, "vendor.qcom.IPeripheralManager");

}  // namespace android

namespace {

std::once_flag gBinderInitOnce;

void ensureVendorBinder() {
    std::call_once(gBinderInitOnce, [] {
        android::ProcessState::initWithDriver(kVendorBinderDriver);
        android::ProcessState::self()->startThreadPool();
    });
}

class PeripheralClient;

class PeripheralCallback final : public android::BBinder {
public:
    explicit PeripheralCallback(PeripheralClient *client) : mClient(client) {}

    const android::String16 &getInterfaceDescriptor() const override {
        static const android::String16 descriptor(kCallbackDescriptor);
        return descriptor;
    }

    android::status_t onTransact(uint32_t code, const android::Parcel &data,
            android::Parcel *reply, uint32_t flags = 0) override;

private:
    PeripheralClient *mClient;
};

class PeripheralClient final {
public:
    PeripheralClient(PmClientNotifier notifier, void *userData,
            const char *peripheralName, const char *clientName)
        : mNotifier(notifier),
          mUserData(userData),
          mPeripheralName(peripheralName),
          mClientName(clientName),
          mCallback(new PeripheralCallback(this)),
          mClientId(0),
          mDead(false) {}

    ~PeripheralClient() = default;

    bool registerWithManager(int *state) {
        ensureVendorBinder();
        android::sp<android::IBinder> service =
                android::defaultServiceManager()->checkService(android::String16(kServiceName));
        if (service == nullptr) {
            ALOGE("Peripheral Manager service is unavailable");
            return false;
        }

        int64_t clientId = 0;
        int64_t currentState = 0;
        android::Parcel data;
        android::Parcel reply;
        data.writeInterfaceToken(android::String16(kManagerDescriptor));
        data.writeString8(mPeripheralName);
        data.writeString8(mClientName);
        data.writeStrongBinder(mCallback);

        android::status_t status = service->transact(kTransactionRegister, data, &reply);
        if (status != android::NO_ERROR) {
            ALOGE("Peripheral Manager registration transaction failed: %d", status);
            return false;
        }

        clientId = reply.readInt64();
        currentState = reply.readInt64();
        if (reply.readInt32() != kPmSuccess) {
            ALOGE("Peripheral Manager rejected registration for %s", mPeripheralName.c_str());
            return false;
        }

        {
            std::lock_guard<std::mutex> lock(mLock);
            mService = service;
            mClientId = clientId;
            mDead = false;
        }
        if (state != nullptr) {
            *state = static_cast<int>(currentState);
        }
        return true;
    }

    int connect() {
        return transactClientId(kTransactionConnect);
    }

    int disconnect() {
        return transactClientId(kTransactionDisconnect);
    }

    int unregister() {
        const int result = transactClientId(kTransactionUnregister);
        std::lock_guard<std::mutex> lock(mLock);
        mService.clear();
        return result;
    }

    int acknowledge(PmEvent event) {
        std::lock_guard<std::mutex> lock(mLock);
        if (mService == nullptr || mDead) {
            return kPmFailed;
        }
        android::Parcel data;
        android::Parcel reply;
        data.writeInterfaceToken(android::String16(kManagerDescriptor));
        data.writeInt64(mClientId);
        data.writeInt32(static_cast<int32_t>(event));
        const android::status_t status = mService->transact(kTransactionAcknowledge, data, &reply);
        return status == android::NO_ERROR ? reply.readInt32() : kPmFailed;
    }

    void notify(PmEvent event) {
        PmClientNotifier notifier = nullptr;
        void *userData = nullptr;
        {
            std::lock_guard<std::mutex> lock(mLock);
            notifier = mNotifier;
            userData = mUserData;
        }
        if (notifier != nullptr) {
            notifier(userData, event);
        }
    }

private:
    int transactClientId(uint32_t transaction) {
        std::lock_guard<std::mutex> lock(mLock);
        if (mService == nullptr || mDead) {
            return kPmFailed;
        }
        android::Parcel data;
        android::Parcel reply;
        data.writeInterfaceToken(android::String16(kManagerDescriptor));
        data.writeInt64(mClientId);
        const android::status_t status = mService->transact(transaction, data, &reply);
        return status == android::NO_ERROR ? reply.readInt32() : kPmFailed;
    }

    std::mutex mLock;
    PmClientNotifier mNotifier;
    void *mUserData;
    android::String8 mPeripheralName;
    android::String8 mClientName;
    android::sp<PeripheralCallback> mCallback;
    android::sp<android::IBinder> mService;
    int64_t mClientId;
    bool mDead;
};

android::status_t PeripheralCallback::onTransact(uint32_t code,
        const android::Parcel &data, android::Parcel *reply, uint32_t flags) {
    (void)reply;
    (void)flags;
    if (code != android::IBinder::FIRST_CALL_TRANSACTION || !data.checkInterface(this)) {
        return android::BBinder::onTransact(code, data, reply, flags);
    }
    mClient->notify(static_cast<PmEvent>(data.readInt32()));
    return android::NO_ERROR;
}

}  // namespace

extern "C" int pm_client_register(PmClientNotifier notifier, void *clientData,
        const char *devName, const char *clientName, int *state, void **handle) {
    if (devName == nullptr || clientName == nullptr || handle == nullptr) {
        return kPmFailed;
    }
    *handle = nullptr;
    PeripheralClient *client = new (std::nothrow) PeripheralClient(
            notifier, clientData, devName, clientName);
    if (client == nullptr || !client->registerWithManager(state)) {
        delete client;
        return kPmFailed;
    }
    *handle = client;
    return kPmSuccess;
}

extern "C" int pm_client_event_acknowledge(void *clientId, PmEvent event) {
    return clientId == nullptr ? kPmFailed :
            static_cast<PeripheralClient *>(clientId)->acknowledge(event);
}

extern "C" int pm_client_unregister(void *clientId) {
    if (clientId == nullptr) {
        return kPmFailed;
    }
    PeripheralClient *client = static_cast<PeripheralClient *>(clientId);
    const int result = client->unregister();
    delete client;
    return result;
}

extern "C" int pm_client_connect(void *clientId) {
    return clientId == nullptr ? kPmFailed :
            static_cast<PeripheralClient *>(clientId)->connect();
}

extern "C" int pm_client_disconnect(void *clientId) {
    return clientId == nullptr ? kPmFailed :
            static_cast<PeripheralClient *>(clientId)->disconnect();
}

extern "C" int pm_show_peripherals(const char *names[8]) {
    (void)names;
    return kPmUnsupported;
}
