#!/usr/bin/perl
use strict;
use warnings;
use IO::Socket;
use Getopt::Long;

# this program can turn on/off a Ethernet ProXR Lite Relay (1-channel) 
#
# usage:
#
# ./relay.pl -on -r 10.0.0.55 2101
# ./relay.pl -off -r 10.0.0.55 2101
#
# replace the IP address above with the real address of the relay.

# note:
# 
# when the relay is first plugged in to the network it pulls a DHCP address.
# you can figure out this address either from your router, or by running 
#
#    tcpdump -i eth0 'udp and port 13000' 
#
# You should see periodic UDP broadcasts from each relay.

# see http://bit.ly/2cwKEQV for product info 
# see http://bit.ly/2d7rWBJ for an example of control sequence
# Some ProXR models have multiple relays - this script works for up to four
# relays in bank 1 using the -n option. The default is relay 1 bank 1.



sub usage {
  print "usage: $0 -on|-off [-n N] [-v] [-r <ip>] [-p <port>]\n";
  exit(-1);
}

our $relay = "10.0.0.55";
our $port = 2101;
our $verbose;
our $help;
our $on;
our $off;
our %msg;
our $n = 1; # relay number
$msg{1}{on}  = "\xAA\x03\xFE\x6C\x01\x18";
$msg{1}{off} = "\xAA\x03\xFE\x64\x01\x10";
$msg{2}{on}  = "\xAA\x03\xFE\x6d\x01\x19";
$msg{2}{off} = "\xAA\x03\xFE\x65\x01\x11";
$msg{3}{on}  = "\xAA\x03\xFE\x6e\x01\x1a";
$msg{3}{off} = "\xAA\x03\xFE\x66\x01\x12";
$msg{4}{on}  = "\xAA\x03\xFE\x6f\x01\x1b";
$msg{4}{off} = "\xAA\x03\xFE\x67\x01\x13";


usage unless GetOptions("port=i"   => \$port,
                        "relay=s" =>  \$relay,
                        "verbose+" => \$verbose,
                        "on"       => \$on,
                        "off"      => \$off,
                        "n=i"      => \$n,
                        "help"     => \$help);
usage if $help;
usage unless ($on or $off);
my $sock = IO::Socket::INET->new(PeerPort => $port, PeerAddr => $relay, 
                                 Proto => "tcp", ) or die "socket: $!\n";
my $cmd = $on ? $msg{$n}{on} : $msg{$n}{off};
$sock->send($cmd);

