#pragma once

#include <QObject>
#include <QString>
#include <qqmlintegration.h>

#include "BluetoothClient.h"
#include "SpotifyClient.h"
#include "SpotifyServiceClient.h"

class ReleaseDateClient;

class PlaybackController : public QObject {
  Q_OBJECT
  QML_SINGLETON
  QML_NAMED_ELEMENT(PlaybackController)

  Q_PROPERTY(PlaybackState playbackState READ playbackState NOTIFY
                 playbackStateChanged)
  Q_PROPERTY(bool isBluetoothActive READ isBluetoothActive NOTIFY
                 isBluetoothActiveChanged)
  Q_PROPERTY(SpotifyClient *spotify READ spotify CONSTANT)
  Q_PROPERTY(BluetoothClient *bluetooth READ bluetooth CONSTANT)
  Q_PROPERTY(QString releaseDate READ releaseDate NOTIFY releaseDateChanged)
  Q_PROPERTY(bool switching READ switching NOTIFY sourceStatusChanged)
  Q_PROPERTY(bool sourceReady READ sourceReady NOTIFY sourceStatusChanged)
  Q_PROPERTY(QString sourceError READ sourceError NOTIFY sourceStatusChanged)

public:
  enum PlaybackState {
    SpotifyUnavailable,
    SpotifyWaiting,
    SpotifyActive,
    BluetoothWaiting,
    BluetoothActive,
  };
  Q_ENUM(PlaybackState)

  /// Construct the facade on the real buses and schedule Spotify boot.
  /// @param parent Optional QObject owner. Constructors do not return a value.
  explicit PlaybackController(QObject *parent = nullptr);
  /// Construct owned clients and observe exclusive source readiness.
  /// @param sessionBus Kiosk user's bus for systemd and MPRIS.
  /// @param systemBus BlueZ bus; injected private buses support tests.
  /// @param parent Optional QObject owner.
  /// @param transitionTimeoutMs Whole-transition deadline in milliseconds.
  /// @note No return value; boot begins asynchronously on the event loop.
  PlaybackController(const QDBusConnection &sessionBus, const QDBusConnection &systemBus,
                     QObject *parent = nullptr, int transitionTimeoutMs = 10000);

  /// @return Cached requested-source view state; not proof of readiness.
  PlaybackState playbackState() const;
  /// @return Whether the view state is BluetoothActive, independently of readiness.
  bool isBluetoothActive() const;
  /// @return Borrowed pointer to the facade-owned Spotify client.
  SpotifyClient *spotify() const;
  /// @return Borrowed pointer to the facade-owned Bluetooth client.
  BluetoothClient *bluetooth() const;
  /// @return Current date/status display string, or empty while pending/cleared.
  QString releaseDate() const;
  /// @return Whether the bounded transition is still progressing.
  bool switching() const { return m_switching; }
  /// @return Whether observed backend states establish readiness and exclusivity.
  bool sourceReady() const { return m_sourceReady; }
  /// @return Lifecycle error for the retained selection, or empty when clear.
  QString sourceError() const { return m_sourceError; }

  /// Request Play on the selected ready source. No parameters/return value.
  /// Async command errors are exposed by the source client; no automatic Play.
  Q_INVOKABLE void play();
  /// Request Pause when ready and not switching. No parameters/return value.
  Q_INVOKABLE void pause();
  /// Request Next when ready and not switching. No parameters/return value.
  Q_INVOKABLE void next();
  /// Request Previous when ready and not switching. No parameters/return value.
  Q_INVOKABLE void previous();
  /// Request Spotify-only absolute seeking; no return value or success guarantee.
  /// @param positionMs Track position in milliseconds; ignored in Bluetooth mode.
  Q_INVOKABLE void seek(qint64 positionMs);
  /// Select Bluetooth and begin ordered shutdown/startup. No parameters/return.
  Q_INVOKABLE void switchToBluetooth();
  /// Select Spotify and begin ordered shutdown/startup. No parameters/return.
  Q_INVOKABLE void switchToSpotify();
  /// Explicitly retry the retained source; never a passive retry. No params/return.
  Q_INVOKABLE void retrySource();

signals:
  /// Notify view-state changes. No parameters/return; consumers reread state.
  void playbackStateChanged();
  /// Notify changes in the BluetoothActive predicate. No parameters/return.
  void isBluetoothActiveChanged();
  /// Notify date/status display changes. No parameters/return.
  void releaseDateChanged();
  /// Notify switching/readiness/error changes as one group. No parameters/return.
  void sourceStatusChanged();

private:
  PlaybackState m_playbackState = SpotifyWaiting;
  SpotifyClient *m_spotify = nullptr;
  BluetoothClient *m_bluetooth = nullptr;
  ReleaseDateClient *m_releaseDates = nullptr;
  SpotifyServiceClient *m_service = nullptr;
  bool m_bluetoothSelected = false;
  bool m_switching = false;
  bool m_sourceReady = false;
  QString m_sourceError;
  quint64 m_attempt = 0;
  bool m_outgoingIssued = false;
  bool m_incomingIssued = false;
  bool m_commandPending = false;
  QTimer m_transitionTimer;
  /// Reset the attempt, mute supplementally, select the view and start its deadline.
  /// @param bluetooth True requests Bluetooth; false requests Spotify.
  /// @note No return; requests during an existing transition are ignored.
  void beginTransition(bool bluetooth);
  /// Advance each lifecycle command at most once, only after observed shutdown.
  /// No parameters/return; acknowledgements do not establish actual readiness.
  void advanceTransition();
  /// Advance an active attempt or recover readiness passively without new commands.
  /// No parameters/return; errors clear only when observed exclusivity is ready.
  void observeSource();
  /// Freeze commands, invalidate callbacks and retain requested selection.
  /// @param error User-facing failure text. No return; observation continues.
  void failTransition(const QString &error);
  /// @return Whether both states are known and the selected backend is exclusive/ready.
  bool requestedReady() const;
  /// Derive source-specific metadata/waiting presentation. No parameters/return.
  void refreshPlaybackState();

  /// Refresh Spotify presentation or pause an unexpected inactive stream.
  /// No parameters/return; supplemental pause is not proof of shutdown.
  void onSpotifyChanged();
  /// Reapply supplemental BT safeguards and refresh connection presentation.
  /// No parameters/return; connection changes cannot select another source.
  void onBluetoothChanged();
  /// Submit active Spotify title/first artist, or clear lookup. No params/return.
  void refreshReleaseDate();
  /// Change-notify the view enum and its derived Bluetooth-active predicate.
  /// @param next Requested presentation state. No return value.
  void setPlaybackState(PlaybackState next);
};
