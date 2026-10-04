#pragma once

#include <QDBusConnection>
#include <QDBusContext>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QTimer>
#include <functional>
#include <qqmlintegration.h>

// Owned by BluetoothClient. Only Agent1's scriptable slots are exported on
// D-Bus; touchscreen decisions are deliberately not remotely callable.
class PairingAgent : public QObject, protected QDBusContext {
  Q_OBJECT
  Q_CLASSINFO("D-Bus Interface", "org.bluez.Agent1")
  QML_NAMED_ELEMENT(PairingAgent)
  QML_UNCREATABLE("PairingAgent is owned by BluetoothClient")
  Q_PROPERTY(bool pending READ pending NOTIFY promptChanged)
  Q_PROPERTY(QString deviceName READ deviceName NOTIFY promptChanged)
  Q_PROPERTY(QString passkey READ passkey NOTIFY promptChanged)
  Q_PROPERTY(int secondsRemaining READ secondsRemaining NOTIFY promptChanged)
  Q_PROPERTY(quint64 requestId READ requestId NOTIFY promptChanged)

public:
  /// Export Agent1 and install deadlines/owner observation; registration waits for enable.
  /// @param bus BlueZ bus. @param parent Optional owner. No return value.
  explicit PairingAgent(const QDBusConnection &bus, QObject *parent = nullptr);
  /// Reject pending confirmation and remove the export/agent. No parameters/return.
  ~PairingAgent() override;
  /// @param enabled Desired registration permission; disabling rejects pending requests.
  /// No return; registration/default-agent/unregistration outcomes arrive asynchronously.
  void setEnabled(bool enabled);
  /// Install owner-supplied device lookups. No return value.
  /// @param name Callback path -> display name. @param paired Callback path -> bond flag.
  void setDeviceLookup(std::function<QString(const QString &)> name,
                       std::function<bool(const QString &)> paired);
  /// @return Whether an unanswered delayed confirmation is stored.
  bool pending() const { return !m_requests.isEmpty(); }
  /// @return Current requesting-device display name, or empty after completion.
  QString deviceName() const { return m_name; }
  /// @return Zero-padded six-digit code, or empty without an active prompt.
  QString passkey() const { return m_code; }
  /// @return Backend deadline's remaining whole seconds for display.
  int secondsRemaining() const { return m_seconds; }
  /// @return Monotonic accepted-prompt ID for rejecting stale touchscreen decisions.
  quint64 requestId() const { return m_requestId; }
  /// Resolve only the current, unexpired prompt. No return value.
  /// @param requestId Displayed prompt ID. @param accept Explicit matching-code consent.
  Q_INVOKABLE void resolve(quint64 requestId, bool accept);

public slots:
  /// Handle trusted daemon release, reject pending prompt and report loss. No params/return.
  Q_SCRIPTABLE void Release();
  /// Handle trusted daemon cancellation and reject pending prompt. No params/return.
  Q_SCRIPTABLE void Cancel();
  /// Begin an explicit consent prompt; D-Bus reply is delayed until its resolution.
  /// @param device Requesting object path. @param passkey Numeric code, 0..999999.
  /// No C++ return; acceptance sends an empty reply, rejection sends a D-Bus error.
  Q_SCRIPTABLE void RequestConfirmation(const QDBusObjectPath &device, uint passkey);
  /// Cancel a notification-only association flow that cannot gate consent. No return.
  /// @param device Requester. @param passkey Unused code. @param entered Unused digit count.
  Q_SCRIPTABLE void DisplayPasskey(const QDBusObjectPath &device, uint passkey,
                                  ushort entered);
  /// Reject legacy PIN input. @param device Unused requester path.
  /// @return Empty placeholder; the caller receives a D-Bus rejection, not a PIN.
  Q_SCRIPTABLE QString RequestPinCode(const QDBusObjectPath &device);
  /// Reject passkey-input flow. @param device Unused requester path.
  /// @return Zero placeholder; the caller receives a D-Bus rejection, not a passkey.
  Q_SCRIPTABLE uint RequestPasskey(const QDBusObjectPath &device);
  /// Reject legacy display flow. @param device Unused path. @param pin Unused PIN.
  /// No return value; sends a D-Bus rejection.
  Q_SCRIPTABLE void DisplayPinCode(const QDBusObjectPath &device, const QString &pin);
  /// Reject unconfirmed Just Works. @param device Unused requester. No return value.
  Q_SCRIPTABLE void RequestAuthorization(const QDBusObjectPath &device);
  /// Authorize a trusted enabled caller only when the device's cached Paired flag is true.
  /// @param device Requester path. @param uuid Currently unused (no UUID allowlist).
  /// No return value; empty D-Bus success or rejection is sent.
  Q_SCRIPTABLE void AuthorizeService(const QDBusObjectPath &device, const QString &uuid);

signals:
  /// Notify prompt fields/countdown changes. No parameters/return.
  void promptChanged();
  /// @param error Pairing failure text; empty clears owner's error. No return value.
  void errorChanged(const QString &error);

private:
  /// @return Whether enabled D-Bus sender matches the stored BlueZ unique owner.
  /// Rejects untrusted calls; local non-D-Bus calls simply return false.
  bool trustedCall();
  /// Store delayed request, identity/code and deadline, or reject invalid/duplicate input.
  /// @param device Requester path. @param passkey Numeric code. No return value.
  void request(const QDBusObjectPath &device, uint passkey);
  /// Reply to stored confirmations and clear prompt/timers. No return value.
  /// @param accept Success decision. @param reason Error detail used on rejection only.
  void finish(bool accept, const QString &reason);
  /// @param device Path to cancel through the recorded BlueZ owner. No return; errors async.
  void cancelPairing(const QString &device);
  /// Resolve owner, register DisplayYesNo, then request default-agent status.
  /// No parameters/return; generation-checked async failures notify the owner.
  void registerAgent();
  /// @param reportErrors Watch normal replies, or send without reply during teardown.
  /// No return; expected already-removed/service-loss errors are ignored.
  void unregisterAgent(bool reportErrors = true);
  /// Reject unsupported trusted calls and publish matching-code requirement. No params/return.
  void unsupported();
  QDBusConnection m_bus;
  QString m_owner;
  bool m_enabled = false;
  bool m_exported = false;
  quint64 m_generation = 0;
  quint64 m_requestId = 0;
  QList<QDBusMessage> m_requests;
  QString m_device;
  QString m_name;
  QString m_code;
  int m_seconds = 0;
  QTimer m_deadline;
  QTimer m_countdown;
  std::function<QString(const QString &)> m_nameLookup;
  std::function<bool(const QString &)> m_pairedLookup;
};
