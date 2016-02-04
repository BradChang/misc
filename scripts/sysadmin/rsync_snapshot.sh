#!/bin/sh
#
# rsync_snapshot.sh
#
#
############################################################################

DATE="/bin/date"
RSYNC="/usr/bin/rsync"
RM="/bin/rm"
LN="/bin/ln"
MKDIR="/bin/mkdir"
TOUCH="/bin/touch"
CUT="/bin/cut"
TIME=`$DATE "+%Y%m%d_%H%M%S"`
LOCKFILE="/admin/rsync.lck"
RSYNC_CONFIG_FILES="/admin/etc/rsync_config_files"
HOST_DIRS="${RSYNC_CONFIG_FILES}/rsync_host_list"
DEST="/data01/rsync-backups"

# Check to see if previous job is still running
if [ -f $LOCKFILE ]
then
        echo "Previous rsync job still running as of `date`, exiting."
        exit 1
fi

# Create new lockfile for this job
$TOUCH $LOCKFILE

# Run the rsync
for SRC in `cat $HOST_DIRS`
        do
        ERROR=0
        HOST=`echo $SRC | $CUT -d : -f 1`
        HOST_PORT=`echo $SRC | $CUT -d : -f 2`
        SRC_DIR=`echo $SRC | $CUT -d : -f 3`
        HOST_EXCLUDES_FILE="${RSYNC_CONFIG_FILES}/${HOST}_excludes"
        if [ -f $HOST_EXCLUDES_FILE ]
        then
                EXCLUDES="--exclude-from=${HOST_EXCLUDES_FILE}"
        fi
        BACKUPS_DEST="${DEST}/${HOST}"
        if  [ ! -d $BACKUPS_DEST ]
        then
                $MKDIR $BACKUPS_DEST
        fi

        echo
        echo
        # Run the rsync snapshot
        echo "Initiating rsync of $HOST at `$DATE`"
        $RSYNC -a --numeric-ids -e "ssh -p ${HOST_PORT} -q" --link-dest=${BACKUPS_DEST}/mrb ${HOST}:${SRC_DIR} ${BACKUPS_DEST}/${HOST}_backup_${TIME} $EXCLUDES

        if [ $? -eq 0 ]
        then
                ( cd ${BACKUPS_DEST}; $RM ./mrb; $LN -s ./${HOST}_backup_${TIME} ./mrb )
        else
                echo "rsync reported errors, symlink not updated!!"
                ERROR=1
        fi

        if [ $ERROR -eq 0 ]
        then

                echo \*\*
                echo "rsync of $HOST completed without errors at `$DATE`"
        else
                echo \!\!
                echo "rsync of $HOST completed with errors at `$DATE`"
        fi

done

# Remove lockfile
$RM $LOCKFILE
if [ $? -ne 0 ]
then
        echo "Could not remove lockfile as of `date`, exiting."
        exit 1
fi

# End of script
