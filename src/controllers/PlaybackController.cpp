#include "PlaybackController.h"

#include "BluetoothClient.h"
#include "ReleaseDateClient.h"
#include "SpotifyClient.h"
#include <QPointer>

PlaybackController::PlaybackController(QObject *parent)
    : PlaybackController(QDBusConnection::sessionBus(), QDBusConnection::systemBus(), parent) {}

PlaybackController::PlaybackController(const QDBusConnection &sessionBus,
    const QDBusConnection &systemBus, QObject *parent, int transitionTimeoutMs)
    : QObject(parent) {
  m_spotify = new SpotifyClient(sessionBus, this);
  m_bluetooth = new BluetoothClient(systemBus, this);
  m_service = new SpotifyServiceClient(sessionBus, this);
  m_releaseDates = new ReleaseDateClient(this);

  connect(m_releaseDates, &ReleaseDateClient::releaseDateChanged, this,
          &PlaybackController::releaseDateChanged);
  connect(m_spotify, &SpotifyClient::titleChanged, this,
          &PlaybackController::refreshReleaseDate);
  connect(m_spotify, &SpotifyClient::artistChanged, this,
          &PlaybackController::refreshReleaseDate);
  connect(m_spotify, &SpotifyClient::hasTrackChanged, this,
          &PlaybackController::refreshReleaseDate);

  connect(m_spotify, &SpotifyClient::availableChanged, this,
          &PlaybackController::onSpotifyChanged);
  connect(m_spotify, &SpotifyClient::hasTrackChanged, this,
          &PlaybackController::onSpotifyChanged);
  connect(m_spotify, &SpotifyClient::isSpotifyPlayingChanged, this,
          &PlaybackController::onSpotifyChanged);

  connect(m_bluetooth, &BluetoothClient::connectedDeviceNameChanged, this,
          &PlaybackController::onBluetoothChanged);

  connect(m_service, &SpotifyServiceClient::stateChanged, this, &PlaybackController::observeSource);
  connect(m_bluetooth, &BluetoothClient::adapterStateChanged, this, &PlaybackController::observeSource);
  connect(m_bluetooth, &BluetoothClient::errorMessageChanged, this, &PlaybackController::observeSource);
  m_transitionTimer.setSingleShot(true);
  m_transitionTimer.setParent(this);
  m_transitionTimer.setObjectName(QStringLiteral("sourceTransitionDeadline"));
  m_transitionTimer.setTimerType(Qt::PreciseTimer);
  m_transitionTimer.setInterval(transitionTimeoutMs);
  connect(&m_transitionTimer, &QTimer::timeout, this, [this]() {
    if (m_switching) {
      failTransition((m_bluetoothSelected ? QStringLiteral("Bluetooth") : QStringLiteral("Spotify"))
          + QStringLiteral(" source transition timed out — check system config and retry"));
    }
  });
  QTimer::singleShot(0, this, [this]() {
    if (m_attempt == 0) {
      beginTransition(false);
    }
  });
}

PlaybackController::PlaybackState PlaybackController::playbackState() const {
  return m_playbackState;
}

bool PlaybackController::isBluetoothActive() const {
  return m_playbackState == BluetoothActive;
}

SpotifyClient *PlaybackController::spotify() const { return m_spotify; }

BluetoothClient *PlaybackController::bluetooth() const { return m_bluetooth; }

QString PlaybackController::releaseDate() const { return m_releaseDates->releaseDate(); }

void PlaybackController::play() {
  if (!m_sourceReady || m_switching) {
    return;
  }
  if (m_playbackState == BluetoothActive) {
    m_bluetooth->play();
  } else {
    m_spotify->play();
  }
}

void PlaybackController::pause() {
  if (!m_sourceReady || m_switching) {
    return;
  }
  if (m_playbackState == BluetoothActive) {
    m_bluetooth->pause();
  } else {
    m_spotify->pause();
  }
}

void PlaybackController::next() {
  if (!m_sourceReady || m_switching) {
    return;
  }
  if (m_playbackState == BluetoothActive) {
    m_bluetooth->next();
  } else {
    m_spotify->next();
  }
}

void PlaybackController::previous() {
  if (!m_sourceReady || m_switching) {
    return;
  }
  if (m_playbackState == BluetoothActive) {
    m_bluetooth->previous();
  } else {
    m_spotify->previous();
  }
}

void PlaybackController::seek(qint64 positionMs) {
  // AVRCP has no seek-absolute; seek is Spotify only (ADR 0006).
  if (m_sourceReady && !m_switching && !m_bluetoothSelected) {
    m_spotify->seek(positionMs);
  }
}

void PlaybackController::switchToSpotify() {
  beginTransition(false);
}

void PlaybackController::switchToBluetooth() {
  beginTransition(true);
}

void PlaybackController::onSpotifyChanged() {
  // Supplemental silence if an unexpected MPRIS stream appears during
  // Bluetooth selection. The service lifecycle is the exclusivity boundary.
  if (m_bluetoothSelected) {
    if (m_spotify->isSpotifyPlaying()) {
      m_spotify->pause();
    }
    return;
  }

  refreshPlaybackState();
}

void PlaybackController::onBluetoothChanged() {
  if (m_bluetooth->hasConnectedDevice()) {
    m_bluetooth->setMuted(!m_bluetoothSelected || !m_sourceReady);
    if (!m_bluetoothSelected || !m_sourceReady) {
      m_bluetooth->pauseAll();
    }
  }
  refreshPlaybackState();
  if (m_bluetoothSelected && m_sourceReady && !m_bluetooth->hasConnectedDevice()) {
    m_bluetooth->ensureDiscoverable();
  }
}

void PlaybackController::retrySource() {
  beginTransition(m_bluetoothSelected);
}

void PlaybackController::beginTransition(bool bluetooth) {
  if (m_switching) {
    return;
  }
  ++m_attempt;
  m_bluetoothSelected = bluetooth;
  m_sourceReady = false;
  m_sourceError.clear();
  m_switching = true;
  m_outgoingIssued = false;
  m_incomingIssued = false;
  m_commandPending = false;
  m_bluetooth->setPairingEnabled(false);
  // Supplemental silence during shutdown; readiness never relies on this.
  m_bluetooth->setMuted(true);
  m_transitionTimer.start();
  refreshPlaybackState();
  emit sourceStatusChanged();
  advanceTransition();
}

bool PlaybackController::requestedReady() const {
  if (!m_service->known() || !m_bluetooth->adapterStateKnown()) {
    return false;
  }
  return m_bluetoothSelected
      ? m_service->stopped() && m_bluetooth->sinkReady()
      : m_service->running() && !m_bluetooth->adapterPowered();
}

void PlaybackController::advanceTransition() {
  if (!m_switching || m_commandPending) {
    return;
  }
  if (!m_service->errorMessage().isEmpty()) {
    failTransition(m_service->errorMessage());
    return;
  }
  if (!m_bluetooth->adapterStateKnown()) {
    if (!m_bluetooth->errorMessage().isEmpty()) {
      failTransition(m_bluetooth->errorMessage());
    }
    return;
  }
  if (!m_service->known()) {
    return;
  }
  if (!m_bluetoothSelected && m_incomingIssued && m_service->failed()) {
    failTransition(QStringLiteral("Spotify startup failed — check system config and retry"));
    return;
  }
  const bool outgoingDown = m_bluetoothSelected ? m_service->stopped()
                                               : !m_bluetooth->adapterPowered();
  QPointer<PlaybackController> self(this);
  const quint64 attempt = m_attempt;
  const auto finished = [self, attempt](const QString &error) {
    if (!self || attempt != self->m_attempt || !self->m_switching) {
      return;
    }
    self->m_commandPending = false;
    if (!error.isEmpty()) {
      self->failTransition(error);
    }
    // The source clients now fetch fresh state. Do not advance using cached
    // state just because systemd returned a job path or BlueZ accepted Set.
  };
  if (!outgoingDown) {
    if (!m_outgoingIssued) {
      m_outgoingIssued = true;
      m_commandPending = true;
      if (m_bluetoothSelected) {
        m_service->requestRunning(false, finished);
      } else {
        m_bluetooth->requestAdapterPowered(false, finished);
      }
    }
    return;
  }
  if (requestedReady()) {
    m_switching = false;
    m_transitionTimer.stop();
    observeSource();
    return;
  }
  if (!m_incomingIssued) {
    if (m_bluetoothSelected && !m_bluetooth->adapterAvailable()) {
      failTransition(QStringLiteral("Bluetooth adapter unavailable — check system config"));
      return;
    }
    // A powered adapter still waiting for its A2DP UUID needs observation,
    // not another Powered=true command.
    if (m_bluetoothSelected && m_bluetooth->adapterPowered()) {
      return;
    }
    m_incomingIssued = true;
    m_commandPending = true;
    if (m_bluetoothSelected) {
      m_bluetooth->requestAdapterPowered(true, finished);
    } else {
      m_service->requestRunning(true, finished);
    }
  }
}

void PlaybackController::failTransition(const QString &error) {
  ++m_attempt; // outstanding replies can still be observed, but cannot advance
  m_switching = false;
  m_commandPending = false;
  m_sourceReady = false;
  m_sourceError = error;
  m_transitionTimer.stop();
  emit sourceStatusChanged();
}

void PlaybackController::observeSource() {
  if (m_attempt == 0) {
    return;
  }
  if (m_switching) {
    advanceTransition();
    return;
  }
  const bool ready = requestedReady();
  if (ready == m_sourceReady && (!ready || m_sourceError.isEmpty())) {
    return;
  }
  m_sourceReady = ready;
  if (ready) {
    m_sourceError.clear();
    m_bluetooth->setPairingEnabled(m_bluetoothSelected);
    m_bluetooth->setMuted(!m_bluetoothSelected);
    if (m_bluetoothSelected && !m_bluetooth->hasConnectedDevice()) {
      m_bluetooth->ensureDiscoverable();
    }
  } else {
    m_bluetooth->setPairingEnabled(false);
    m_bluetooth->setMuted(true);
    if (m_sourceError.isEmpty()) {
      m_sourceError = !m_service->errorMessage().isEmpty() ? m_service->errorMessage()
          : (!m_bluetooth->adapterStateKnown() && !m_bluetooth->errorMessage().isEmpty()
                ? m_bluetooth->errorMessage()
                : QStringLiteral("Selected source is no longer ready — check system config and retry"));
    }
  }
  refreshPlaybackState();
  emit sourceStatusChanged();
}

void PlaybackController::refreshPlaybackState() {
  if (m_bluetoothSelected) {
    setPlaybackState(m_bluetooth->hasConnectedDevice() ? BluetoothActive : BluetoothWaiting);
  } else {
    setPlaybackState(m_service->running() && m_spotify->isAvailable() && m_spotify->hasTrack()
        ? SpotifyActive : SpotifyWaiting);
  }
}

void PlaybackController::refreshReleaseDate() {
  if (m_playbackState == SpotifyActive && m_spotify->hasTrack()) {
    m_releaseDates->setTrack(m_spotify->title(), m_spotify->firstArtist());
  } else {
    m_releaseDates->clearTrack();
  }
}

void PlaybackController::setPlaybackState(PlaybackState next) {
  if (m_playbackState == next) {
    return;
  }
  bool wasBt = (m_playbackState == BluetoothActive);
  m_playbackState = next;
  refreshReleaseDate();
  emit playbackStateChanged();
  if (wasBt != (next == BluetoothActive)) {
    emit isBluetoothActiveChanged();
  }
}
