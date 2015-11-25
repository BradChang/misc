#!/usr/bin/perl
#
# Perl script to find disk usage above watermarks
#


# Declare variables
chomp ($host = `uname -n`);
my $sysadmin="admins\@email.domain";
my $mail="/bin/mail";

### Change this to bump up the warning threshold!
$high_mark = 95;

# Ok let's find those disks getting over the limit.
# Add any fs to exlcude with  | grep -v <fs>
foreach $i (`df -l | grep -v /dev/shm`) {
    my @disk = split(/\s+/, $i);
    my $used = $disk[4];
    chop ($used);
    if ($used > $high_mark) {
        my $subject = "\" System Alert on $host\"";
        my $message = "$disk[5] is currently $used percent utilized\n";
        open(MAIL,"|$mail -s $subject $sysadmin");
        print MAIL $message;
            close(MAIL);
   }
}

# End of program
