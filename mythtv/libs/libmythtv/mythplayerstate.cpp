// MythTV
#include "audioplayer.h"
#include "osd.h"
#include "mythplayerstate.h"

/*! \class MythAudioState
 * \brief A simple wrapper around audio state used to signal changes
 * in the current state.
*/
MythAudioState::MythAudioState(AudioPlayer* Player, int64_t Offset)
  : m_hasAudioOut(Player->HasAudioOut()),
    m_volumeControl(Player->ControlsVolume()),
    m_volume(Player->GetVolume()),
    m_muteState(Player->GetMuteState()),
    m_canUpmix(Player->CanUpmix()),
    m_isUpmixing(Player->IsUpmixing()),
    m_paused(Player->IsPaused()),
    m_audioOffset(Offset)
{
}

/*! \class MythOverlayState
 * \brief A simpler than simple wrapper around OSD state.
*/
MythOverlayState::MythOverlayState(bool Browsing, bool Editing)
  : m_browsing(Browsing),
    m_editing(Editing)
{
}

MythVisualiserState::MythVisualiserState(bool Embedding, bool Visualising,
                                         QString Name, const QStringList& Visualisers)
  : m_embedding(Embedding),
    m_visualising(Visualising),
    m_visualiserName(Name),
    m_visualiserList(Visualisers)
{
}