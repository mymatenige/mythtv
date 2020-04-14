#!/usr/bin/perl -w

use strict;

use DBI;
use DBD::mysql;
use MythTV;

#my $aspect = '16:9';
my $chanid = undef;
my $export_dir = '/tmp';
my $max_bitrate = '160';
my $starttime = undef;

foreach (@ARGV)
{
#	if ($_ =~ m/aspect/)
#	{
#		$aspect = (split(/\=/, $_))[1];
#	}
	if ($_ =~ m/chanid/)
	{
		$chanid = (split(/\=/, $_))[1];
	}
	if ($_ =~ m/export_dir/)
	{
		$export_dir = (split(/\=/, $_))[1];
	}
	if ($_ =~ m/starttime/)
	{
		$starttime = (split(/\=/, $_))[1];
	}
}

if (!defined($chanid) || !defined($starttime))
{
	die "Invalid arguments.\n";
}

my $mythtv = new MythTV();
my $database_handle = $mythtv->{'dbh'};

my $query = undef;
my $query_handle = undef;

$query = "SELECT basename, subtitle, title FROM recorded WHERE chanid = '$chanid' AND starttime = '$starttime'";
$query_handle = $database_handle->prepare($query);
$query_handle->execute() || die "Query for recorded failed.\n";

my $basename = undef;
my $subtitle = undef;
my $title = undef;

$query_handle->bind_columns(undef, \$basename, \$subtitle, \$title);
$query_handle->fetch();

if (!defined($basename) || !defined($subtitle) || !defined($title))
{
	die "Recording not found.\n";
}

my $import_dir = undef;

if ($mythtv->backend_setting('DBSchemaVer') < 1171)
{
	$import_dir = $mythtv->backend_setting('RecordFilePrefix');
}
else
{
	my $storage_group = new MythTV::StorageGroup();
	$import_dir = $storage_group->FindRecordingDir($basename);
}

if (!defined($import_dir))
{
	die "Directory not found.\n";
}

$query = "SELECT name FROM channel WHERE chanid = '$chanid'";
$query_handle = $database_handle->prepare($query);
$query_handle->execute() || die "Query for channel failed.\n";

my $name = undef;

$query_handle->bind_columns(undef, \$name);
$query_handle->fetch();

if (!defined($name))
{
	die "Channel not found.\n";
}

my $new_name = $name;
my $new_starttime = $starttime;
my $new_subtitle = $subtitle;
my $new_title = $title;

$new_name =~ s/\s+/-/g;
$new_starttime =~ s/[|^\W|\s|-|]//g;
$new_subtitle =~ s/\W+/_/g;
$new_title =~ s/\W+/_/g;

my $import_file = $import_dir."/".$basename;
my $export_file = $export_dir."/".$new_name."_".$new_title."_".$new_subtitle."_".$new_starttime.".m4v";

my $file_output = `/usr/bin/ffmpeg -i '$import_file' 2>&1`;
$file_output =~ s/^.*?Audio.*?, (\d*) kb\/s.*$/$1/s;

if ($file_output =~ /^\d+$/)
{
	if ($file_output < $max_bitrate)
	{
		$max_bitrate = $file_output;
	}
}

$max_bitrate .= "k";

my $command = undef;

# H264, 768Kbps 25fps, 640x480
# AAC, 160Kbps, stereo, 48kHz
$command = "/bin/nice -n19 /usr/bin/ffmpeg -i '$import_file' -strict experimental -f mp4 -vcodec libx264 -profile baseline -b 768k -g 250 -r 25 -s '640x480' -acodec aac -ab '$max_bitrate' -ac 2 -ar 48k -y '$export_file' > /dev/null 2>&1";
print "$command\n";
system "$command";
