#!/usr/bin/perl
#
# Perl script to attrition a directory to keep only $THRESH 
# subdirectories. The subdirectores MUST BE DATE NAMED to 
# sort properly to delete oldest-before-newest. 
#
# YYYYMMDD naming convention is best: that sorts oldest first
#

use File::Path qw(remove_tree);

chomp ($host = `uname -n`);
my $recip="user\@email.domain";
my $mail="/bin/mail";

my $dir = "/tmp/blah";
my $disk_usage = `du -sh $dir`;
my @files = glob "$dir/*";

my $THRESH=10;  # how many subdirs to keep in $dir
my @to_unlink;  # directories we plan to remove_tree
my %unlinked;   # this is a hash whose keys are all recursively deleted files
  
push @to_unlink, shift @files while (scalar(@files) > $THRESH);

my $tmp; # files confirmd to be unlinked in each pass
for my $rmdir (@to_unlink) {
  next unless -d $rmdir;
  remove_tree( $rmdir, {result => \$tmp} );
  $unlinked{$_}++ for @$tmp;
}

my $subject = "\"Attrition on $host\"";
open(MAIL,"|$mail -s $subject $recip");
print MAIL "Utilization:\n$disk_usage\n";
print MAIL "Deleted " . scalar(keys %unlinked) . " files.\n\n";
print MAIL " $_\n" for sort keys %unlinked;
close(MAIL);

