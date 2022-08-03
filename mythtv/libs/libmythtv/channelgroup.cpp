// c++
#include <algorithm>

// mythtv
#include "libmythbase/mythdb.h"
#include "libmythbase/mythlogging.h"

#include "channelgroup.h"
#include "channelutil.h"
#include "sourceutil.h"

#define LOC QString("Channel Group: ")

inline bool lt_group(const ChannelGroupItem &a, const ChannelGroupItem &b)
{
    return QString::localeAwareCompare(a.m_name, b.m_name) < 0;
}

bool ChannelGroup::ToggleChannel(uint chanid, int changrpid, bool delete_chan)
{
    // Check if it already exists for that chanid...
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT channelgroup.id "
        "FROM channelgroup "
        "WHERE channelgroup.chanid = :CHANID AND "
        "channelgroup.grpid = :GRPID "
        "LIMIT 1");
    query.bindValue(":CHANID", chanid);
    query.bindValue(":GRPID", changrpid);

    if (!query.exec())
    {
        MythDB::DBError("ChannelGroup::ToggleChannel", query);
        return false;
    }
    if (query.next() && delete_chan)
    {
        // We have a record...Remove it to toggle...
        QString id = query.value(0).toString();
        query.prepare("DELETE FROM channelgroup WHERE id = :CHANID");
        query.bindValue(":CHANID", id);
        if (!query.exec())
            MythDB::DBError("ChannelGroup::ToggleChannel -- delete", query);
        LOG(VB_GENERAL, LOG_INFO, LOC +
            QString("Removing channel %1 from group %2.").arg(id).arg(changrpid));
    }
    else if (query.size() == 0)
    {
        // We have no record...Add one to toggle...
        query.prepare("INSERT INTO channelgroup (chanid,grpid) "
                      "VALUES (:CHANID, :GRPID)");
        query.bindValue(":CHANID", chanid);
        query.bindValue(":GRPID", changrpid);
        if (!query.exec())
            MythDB::DBError("ChannelGroup::ToggleChannel -- insert", query);
        LOG(VB_GENERAL, LOG_INFO, LOC +
            QString("Adding channel %1 to group %2.")
                 .arg(chanid).arg(changrpid));
    }
    else
    {
        LOG(VB_GENERAL, LOG_INFO, LOC +
            QString("Channel %1 already present in group %2.")
                 .arg(chanid).arg(changrpid));
    }

    return true;
}

bool ChannelGroup::AddChannel(uint chanid, int changrpid)
{
    // Make sure the channel group exists
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT grpid, name FROM channelgroupnames "
        "WHERE grpid = :GRPID");
    query.bindValue(":GRPID", changrpid);

    if (!query.exec())
    {
        MythDB::DBError("ChannelGroup::AddChannel", query);
        return false;
    }
    if (query.size() == 0)
    {
        LOG(VB_GENERAL, LOG_INFO, LOC +
            QString("AddChannel failed to find channel group %1.").arg(changrpid));
        return false;
    }

    query.first();
    QString groupName = query.value(1).toString();

    // Make sure the channel exists and is visible
    query.prepare(
        "SELECT chanid, name FROM channel "
        "WHERE chanid = :CHANID "
        "AND visible > 0 "
        "AND deleted IS NULL");
    query.bindValue(":CHANID", chanid);

    if (!query.exec())
    {
        MythDB::DBError("ChannelGroup::AddChannel", query);
        return false;
    }
    if (query.size() == 0)
    {
        LOG(VB_GENERAL, LOG_INFO, LOC +
            QString("AddChannel failed to find channel %1.").arg(chanid));
        return false;
    }

    query.first();
    QString chanName = query.value(1).toString();

    // Check if it already exists for that chanid...
    query.prepare(
        "SELECT channelgroup.id "
        "FROM channelgroup "
        "WHERE channelgroup.chanid = :CHANID AND "
        "channelgroup.grpid = :GRPID "
        "LIMIT 1");
    query.bindValue(":CHANID", chanid);
    query.bindValue(":GRPID", changrpid);

    if (!query.exec())
    {
        MythDB::DBError("ChannelGroup::AddChannel", query);
        return false;
    }
    if (query.size() == 0)
    {
        // We have no record...Add one to toggle...
        query.prepare("INSERT INTO channelgroup (chanid,grpid) "
                      "VALUES (:CHANID, :GRPID)");
        query.bindValue(":CHANID", chanid);
        query.bindValue(":GRPID", changrpid);
        if (!query.exec())
            MythDB::DBError("ChannelGroup::AddChannel -- insert", query);
        LOG(VB_GENERAL, LOG_INFO, LOC +
            QString("Adding channel %1 to group %2.")
                 .arg(chanName, groupName));
    }

    return true;
}

bool ChannelGroup::DeleteChannel(uint chanid, int changrpid)
{
    // Check if it already exists for that chanid...
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT channelgroup.id "
        "FROM channelgroup "
        "WHERE channelgroup.chanid = :CHANID AND "
        "channelgroup.grpid = :GRPID "
        "LIMIT 1");
    query.bindValue(":CHANID", chanid);
    query.bindValue(":GRPID", changrpid);

    if (!query.exec())
    {
        MythDB::DBError("ChannelGroup::DeleteChannel", query);
        return false;
    }
    if (query.next())
    {
        // We have a record...Remove it to toggle...
        QString id = query.value(0).toString();
        query.prepare("DELETE FROM channelgroup WHERE id = :CHANID");
        query.bindValue(":CHANID", id);
        if (!query.exec())
            MythDB::DBError("ChannelGroup::DeleteChannel -- delete", query);
        LOG(VB_GENERAL, LOG_INFO, LOC +
            QString("Removing channel with id=%1.").arg(id));
    }

    return true;
}

// Create a list of all channel groups that are manually configured
//
// The channel groups are returned in the following order:
// - Favorites
// - Manually created channel groups, sorted by name.
//
ChannelGroupList ChannelGroup::GetManualChannelGroups(bool includeEmpty)
{
    ChannelGroupList list;
    QString qstr;
    MSqlQuery query(MSqlQuery::InitCon());

    // Always the Favorites with group id 1
    if (includeEmpty)
        qstr = "SELECT grpid, name FROM channelgroupnames "
               "  WHERE grpid = 1 ";
    else
        qstr = "SELECT DISTINCT t1.grpid, name FROM channelgroupnames t1, channelgroup t2 "
               "  WHERE t1.grpid = t2.grpid "
               "  AND t1.grpid = 1";
    query.prepare(qstr);
    if (!query.exec())
        MythDB::DBError("ChannelGroup::GetChannelGroups Favorites", query);
    else
    {
        if (query.next())
        {
           ChannelGroupItem group(query.value(0).toUInt(),
                                  query.value(1).toString());
           list.push_back(group);
        }
    }

    // Then the channel groups that are manually created
    if (includeEmpty)
        qstr = "SELECT grpid, name FROM channelgroupnames "
               "  WHERE name NOT IN (SELECT name FROM videosource) "
               "  AND name <> 'Priority' "
               "  AND grpid <> 1";
    else
        qstr = "SELECT DISTINCT t1.grpid, name FROM channelgroupnames t1, channelgroup t2 "
               "  WHERE t1.grpid = t2.grpid "
               "  AND name NOT IN (SELECT name FROM videosource)"
               "  AND name <> 'Priority' "
               "  AND t1.grpid <> 1";
    qstr += " ORDER BY NAME";
    query.prepare(qstr);
    if (!query.exec())
        MythDB::DBError("ChannelGroup::GetChannelGroups manual", query);
    else
    {
        while (query.next())
        {
           ChannelGroupItem group(query.value(0).toUInt(),
                                  query.value(1).toString());
           list.push_back(group);
        }
    }

    return list;
}

// Create a list of all channel groups that are automatically created
//
// The channel groups are returned in the following order:
// - Priority channels
// - Channel groups created automatically from videosources, sorted by name.
//
ChannelGroupList ChannelGroup::GetAutomaticChannelGroups(bool includeEmpty)
{
    ChannelGroupList list;
    QString qstr;
    MSqlQuery query(MSqlQuery::InitCon());

    // The Priority channel group if it exists
    if (includeEmpty)
        qstr = "SELECT grpid, name FROM channelgroupnames "
                "  WHERE name = 'Priority' ";
    else
        qstr = "SELECT DISTINCT t1.grpid, name FROM channelgroupnames t1, channelgroup t2 "
               "  WHERE t1.grpid = t2.grpid "
               "  AND name = 'Priority' ";
    query.prepare(qstr);
    if (!query.exec())
        MythDB::DBError("ChannelGroup::GetChannelGroups Priority", query);
    else
    {
        if (query.next())
        {
           ChannelGroupItem group(query.value(0).toUInt(),
                                  query.value(1).toString());
           list.push_back(group);
        }
    }

    // The channel groups that are automatically created from videosources
    if (includeEmpty)
        qstr = "SELECT grpid, name FROM channelgroupnames "
               "  WHERE name IN (SELECT name FROM videosource) ";
    else
        qstr = "SELECT DISTINCT t1.grpid, name FROM channelgroupnames t1, channelgroup t2 "
               "  WHERE t1.grpid = t2.grpid "
               "  AND name IN (SELECT name FROM videosource)";
    qstr += " ORDER BY NAME";
    query.prepare(qstr);
    if (!query.exec())
        MythDB::DBError("ChannelGroup::GetChannelGroups videosources", query);
    else
    {
        while (query.next())
        {
           ChannelGroupItem group(query.value(0).toUInt(),
                                  query.value(1).toString());
           list.push_back(group);
        }
    }
    return list;
}

// Create a list of all channel groups
//
// The channel groups are returned in the following order:
// - Favorites
// - Manually created channel groups, sorted by name.
// - Priority channels
// - Channel groups created automatically from videosources, sorted by name.
//
ChannelGroupList ChannelGroup::GetChannelGroups(bool includeEmpty)
{
    ChannelGroupList list = GetManualChannelGroups(includeEmpty);
    ChannelGroupList more = GetAutomaticChannelGroups(includeEmpty);
    list.insert(list.end(), more.begin(), more.end());
    return list;
}

// Cycle through the available channel groups.
// At the end return -1 to select "All Channels".
int ChannelGroup::GetNextChannelGroup(const ChannelGroupList &sorted, int grpid)
{
    // If no groups return -1 for "All Channels"
    if (sorted.empty())
      return -1;

    // If grpid is "All Channels" (-1), then return the first grpid
    if (grpid == -1)
      return sorted[0].m_grpId;

    auto it = std::find(sorted.cbegin(), sorted.cend(), grpid);

    // If grpid is not in the list, return -1 for "All Channels"
    if (it == sorted.end())
        return -1;

    ++it;

    // If we reached the end, the next option is "All Channels" (-1)
    if (it == sorted.end())
       return -1;

    return it->m_grpId;
}

bool ChannelGroup::InChannelGroupList(const ChannelGroupList &groupList, int grpid)
{
    auto it = std::find(groupList.cbegin(), groupList.cend(), grpid);
    return it != groupList.end();
}

bool ChannelGroup::NotInChannelGroupList(const ChannelGroupList &groupList, int grpid)
{
    return !InChannelGroupList(groupList, grpid);
}

QString ChannelGroup::GetChannelGroupName(int grpid)
{
    // All Channels
    if (grpid == -1)
        return tr("All Channels");

    // No group
    if (grpid == 0)
        return "";

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT name FROM channelgroupnames WHERE grpid = :GROUPID");
    query.bindValue(":GROUPID", grpid);

    if (!query.exec())
        MythDB::DBError("ChannelGroup::GetChannelGroups", query);
    else if (query.next())
        return query.value(0).toString();

    return "";
}

int ChannelGroup::GetChannelGroupId(const QString& changroupname)
{
    // All Channels
    if (changroupname == "All Channels")
      return -1;

    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT grpid FROM channelgroupnames "
                  "WHERE name = :GROUPNAME");
    query.bindValue(":GROUPNAME", changroupname);

    if (!query.exec())
        MythDB::DBError("ChannelGroup::GetChannelGroups", query);
    else if (query.next())
        return query.value(0).toUInt();

    return 0;
}

// UpdateChannelGroups
//
// Create and maintain a channel group for each connected video source
// with the name of the video source.
// Create and maintain a channel group Priority for
// all channels that have a recording priority bigger than 0.
//
void ChannelGroup::UpdateChannelGroups(void)
{
    QMap<int, QString> allSources;
    QMap<int, QString> connectedSources;
    QMap<int, QString> disconnectedSources;

    // Get list of all video sources
    {
        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("SELECT sourceid,name FROM videosource;");
        if (query.exec())
        {
            while (query.next())
                allSources[query.value(0).toInt()] = query.value(1).toString();
        }
        else
            MythDB::DBError("UpdateChannelGroups videosource 1", query);
    }

    // Split all video sources into a list of video sources that are connected to one
    // or more capture cards and a list of video sources that are not connected.
    for (auto sourceId : allSources.keys())
    {
        int count = SourceUtil::GetConnectionCount(sourceId);
        if (count > 0)
            connectedSources[sourceId] = allSources[sourceId];
        else
            disconnectedSources[sourceId] = allSources[sourceId];
    }

    // Remove channelgroup channels and the channelgroupname for disconnected video sources.
    for (auto sourceName : disconnectedSources)
    {
        int groupId = ChannelGroup::GetChannelGroupId(sourceName);
        if (groupId > 0)
        {
            // Yes, channelgroup does exist. Remove all existing channels.
            LOG(VB_GENERAL, LOG_INFO, QString("Remove channels of channelgroup %1").arg(sourceName));
            MSqlQuery query(MSqlQuery::InitCon());
            QString qstr =
                "DELETE FROM channelgroup WHERE grpid = :GRPID;";
            query.prepare(qstr);
            query.bindValue(":GRPID", groupId);

            if (!query.exec())
                MythDB::DBError("UpdateChannelGroups channelgroup 1", query);

            // And also the channelgroupname
            LOG(VB_GENERAL, LOG_INFO, QString("Remove channelgroup %1").arg(sourceName));
            qstr =
                "DELETE FROM channelgroupnames WHERE grpid = :GRPID;";
            query.prepare(qstr);
            query.bindValue(":GRPID", groupId);

            if (!query.exec())
                MythDB::DBError("UpdateChannelGroups channelgroupnames 1", query);
        }
    }

    // Add channelgroupname entry if it does not exist yet
    for (auto sourceName : connectedSources)
    {
        int groupId = ChannelGroup::GetChannelGroupId(sourceName);
        if (groupId == 0)
        {
            LOG(VB_GENERAL, LOG_INFO, QString("Add channelgroup %1").arg(sourceName));

            MSqlQuery query(MSqlQuery::InitCon());
            QString qstr =
                "INSERT INTO channelgroupnames (name) VALUE (:NEWNAME);";
            query.prepare(qstr);
            query.bindValue(":NEWNAME", sourceName);

            if (!query.exec())
                MythDB::DBError("UpdateChannelGroups channelgroupnames 2", query);
        }
    }

    // Remove all channels from channelgroup that do not exist anymore or that are not visible.
    // This includes the channels in the non-automatic groups such as Favorites.
    {
        ChannelGroupList list = GetAutomaticChannelGroups(false);
        MSqlQuery query(MSqlQuery::InitCon());
        for (auto chgrp : list)
        {
            query.prepare(
                "DELETE from channelgroup WHERE grpid = :GRPID"
                "  AND chanid NOT IN "
                "  (SELECT chanid FROM channel WHERE deleted IS NULL AND visible > 0)");
            query.bindValue(":GRPID", chgrp.m_grpId);
            if (!query.exec())
            {
                MythDB::DBError("ChannelGroup::UpdateChannelGroups", query);
                return;
            }
            if (query.numRowsAffected() > 0)
            {
                LOG(VB_GENERAL, LOG_INFO, QString("Removed %1 channels from channelgroup %2")
                    .arg(query.numRowsAffected()).arg(chgrp.m_grpId));
            }
        }

        // query.prepare(
        //     "DELETE from channelgroup WHERE chanid NOT IN "
        //     "(SELECT chanid FROM channel WHERE deleted IS NULL AND visible > 0)");
        // if (!query.exec())
        // {
        //     MythDB::DBError("ChannelGroup::UpdateChannelGroups", query);
        //     return;
        // }
        // if (query.numRowsAffected() > 0)
        // {
        //     LOG(VB_GENERAL, LOG_INFO, QString("Removed %1 channels from channelgroup")
        //         .arg(query.numRowsAffected()));
        // }
    }

    // Add all visible channels to the channelgroups
    for (auto sourceId : connectedSources.keys())
    {
        QString sourceName = connectedSources[sourceId];
        int groupId = ChannelGroup::GetChannelGroupId(sourceName);

        if (groupId > 0)
        {
            LOG(VB_GENERAL, LOG_INFO, QString("Update channelgroup %1").arg(sourceName));
            MSqlQuery query(MSqlQuery::InitCon());
            query.prepare(
                "SELECT chanid FROM channel "
                "WHERE deleted IS NULL "
                "AND visible > 0 "
                "AND sourceid = :SOURCEID");
            query.bindValue(":SOURCEID", sourceId);
            if (!query.exec())
            {
                MythDB::DBError("ChannelGroup::UpdateChannelGroups", query);
                return;
            }

            while (query.next())
            {
                uint chanId = query.value(0).toUInt();
                ChannelGroup::AddChannel(chanId, groupId);
            }
        }
    }

    // Channelgroup Priority
    {
        // Add channel group for Priority channels
        QString sourceName = "Priority";
        int groupId = ChannelGroup::GetChannelGroupId(sourceName);
        if (groupId == 0)
        {
            LOG(VB_GENERAL, LOG_INFO, QString("Add channelgroup %1").arg(sourceName));
            MSqlQuery query(MSqlQuery::InitCon());
            QString qstr =
                "INSERT INTO channelgroupnames (name) VALUE (:NEWNAME);";
            query.prepare(qstr);
            query.bindValue(":NEWNAME", sourceName);

            if (!query.exec())
                MythDB::DBError("UpdateChannelGroups Priority channelgroup", query);
        }
        LOG(VB_GENERAL, LOG_INFO, QString("Update channelgroup %1").arg(sourceName));

        // Update all channels in channel group Priority
        groupId = ChannelGroup::GetChannelGroupId(sourceName);
        if (groupId > 0)
        {
            // Remove all channels from channelgroup Priority that do not have priority anymore.
            MSqlQuery query(MSqlQuery::InitCon());
            query.prepare(
                "DELETE from channelgroup WHERE grpid = :GRPID "
                "  AND chanid NOT IN "
                "  (SELECT chanid FROM channel WHERE deleted IS NULL "
                "    AND visible > 0 AND recpriority > 0)");
            query.bindValue(":GRPID", groupId);
            if (!query.exec())
            {
                MythDB::DBError("ChannelGroup::UpdateChannelGroups", query);
                return;
            }
            if (query.numRowsAffected() > 0)
            {
                LOG(VB_GENERAL, LOG_INFO, QString("Removed %1 channels from channelgroup Priority")
                    .arg(query.numRowsAffected()));
            }

            // Add all channels from all connected video groups if they have priority
            for (auto sourceId : connectedSources.keys())
            {
                query.prepare(
                    "SELECT chanid FROM channel "
                    "WHERE deleted IS NULL "
                    "AND visible > 0 "
                    "AND sourceid = :SOURCEID "
                    "AND recpriority > 0");
                query.bindValue(":SOURCEID", sourceId);
                if (!query.exec())
                {
                    MythDB::DBError("UpdateChannelGroups Priority select channels", query);
                    return;
                }
                while (query.next())
                {
                    uint chanId = query.value(0).toUInt();
                    ChannelGroup::AddChannel(chanId, groupId);
                }
            }
        }
        else
        {
            LOG(VB_GENERAL, LOG_INFO, QString("Channelgroup Priority not present"));
        }
    }
}
