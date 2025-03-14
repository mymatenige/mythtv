#ifndef VIDEOUTILS_H_
#define VIDEOUTILS_H_

#include "libmythmetadata/metadatacommon.h"
#include "libmythmetadata/mythmetaexp.h"
#include "libmythmetadata/parentalcontrols.h"

template <typename T>
inline void CheckedSet(T *uiItem, const QString &value)
{
    if (uiItem)
    {
        if (!value.isEmpty())
            uiItem->SetText(value);
        else
            uiItem->Reset();
    }
}

class MythUIStateType;
class MythUIType;
class MythUIImage;

template <>
META_PUBLIC void CheckedSet( MythUIStateType *uiItem, const QString &value);

META_PUBLIC void CheckedSet( MythUIType *container, const QString &itemName,
        const QString &value);

META_PUBLIC void CheckedSet( MythUIImage *uiItem, const QString &filename);

META_PUBLIC QStringList GetVideoDirsByHost(const QString& host);
META_PUBLIC QStringList GetVideoDirs();

META_PUBLIC bool IsDefaultCoverFile(const QString &coverfile);
META_PUBLIC bool IsDefaultScreenshot(const QString &screenshot);
META_PUBLIC bool IsDefaultBanner(const QString &banner);
META_PUBLIC bool IsDefaultFanart(const QString &fanart);

class VideoMetadata;

META_PUBLIC QString GetDisplayUserRating(float userrating);
META_PUBLIC QString GetDisplayLength(std::chrono::minutes length);
META_PUBLIC QString GetDisplayBrowse(bool browse);
META_PUBLIC QString GetDisplayWatched(bool watched);
META_PUBLIC QString GetDisplayProcessed(bool processed);
META_PUBLIC QString GetDisplayYear(int year);
META_PUBLIC QString GetDisplayRating(const QString &rating);

META_PUBLIC QString GetDisplayGenres(const VideoMetadata &item);
META_PUBLIC QString GetDisplayCountries(const VideoMetadata &item);
META_PUBLIC QStringList GetDisplayCast(const VideoMetadata &item);

META_PUBLIC QString TrailerToState(const QString &trailerFile);
META_PUBLIC QString ParentalLevelToState(const ParentalLevel &level);
META_PUBLIC QString WatchedToState(bool watched);

META_PUBLIC VideoContentType ContentTypeFromString(const QString &type);
META_PUBLIC QString ContentTypeToString(VideoContentType type);

#endif // VIDEOUTILS_H_
