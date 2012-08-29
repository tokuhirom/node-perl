use strict;
use warnings;
use utf8;
use Config;

open my $fh, '>', 'src/config.h' or die "Cannot open config.h: $!";
print $fh q{#define LIBPERL "} . $Config{libperl}. qq{"\n};
close $fh;

