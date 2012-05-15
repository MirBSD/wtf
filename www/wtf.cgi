#!/usr/bin/perl -T
my $rcsid = '$MirOS: wtf/www/wtf.cgi,v 1.4 2012/05/15 20:34:09 tg Exp $';
#-
# Copyright © 2012
#	Thorsten Glaser <tg@mirbsd.org>
#
# Provided that these terms and disclaimer and all copyright notices
# are retained or reproduced in an accompanying document, permission
# is granted to deal in this work without restriction, including un‐
# limited rights to use, publicly perform, distribute, sell, modify,
# merge, give away, or sublicence.
#
# This work is provided “AS IS” and WITHOUT WARRANTY of any kind, to
# the utmost extent permitted by applicable law, neither express nor
# implied; without malicious intent or gross negligence. In no event
# may a licensor, author or contributor be held liable for indirect,
# direct, other damage, loss, or other issues arising in any way out
# of dealing in the work, even if advised of the possibility of such
# damage or existence of a defect, except proven that it results out
# of said person’s immediate fault when using the work as intended.
#-
# Implementation of MirBSD wtf(1) as CGI

use strict;
use warnings;

use File::Basename qw(dirname);
my ($mydir) = (dirname($0) =~ /(.*)/);	# untaint

use File::Spec::Functions qw(catfile);
my $db = catfile($mydir, 'acronyms');
my $template = catfile($mydir, 'wtf.htm');
if ((-r $db) && (-r $template) &&
    (open(TEMPLATE, $template)) &&
    (open(ACRONYMS, $db))) {
	undef $mydir;
} else {
	print "Status: 500 Internal Script Error\r\n";
	print "Content-Type: text/plain\r\n\r\n";
	print "Cannot read acronyms database '" . $db .
	    "' or template '" . $template . "'!\r\n";
	exit(1);
}

my $output = "";
my $query = "";
my @results = ();

if (defined($ENV{QUERY_STRING})) {
	for my $p (split(/[;&]+/, $ENV{QUERY_STRING})) {
		next unless $p;
		$p =~ y/+/ /;
		my ($key, $val) = split(/=/, $p, 2);
		next unless defined($key);

		next unless ($key eq "q");
		next unless defined($val);
		$val =~ s/%([0-9A-Fa-f][0-9A-Fa-f])/chr(hex($1))/eg;
		next if $val =~ /[\t\r\n]/;
		$query = $val;
	}
	if ($query eq "") {
		my $p = $ENV{QUERY_STRING};
		$p =~ y/+/ /;
		$p =~ s/%([0-9A-Fa-f][0-9A-Fa-f])/chr(hex($1))/eg;
		next if $p =~ /[\t\r\n]/;
		$query = $p;
	}
}

sub tohtml {
	local ($_) = @_;

	s/&/&#38;/g;
	s/</&#60;/g;
	s/>/&#62;/g;
	return $_;
}

if ($query ne "") {
	my $enc = tohtml($query);

	$enc =~ s/\"/&#34;/g;
	$query = uc($query);
	$query =~ y/äü/ÄÜ/;

	foreach my $line (<ACRONYMS>) {
		chomp($line);
		if ($line =~ /^\Q$query	\E(.*)$/) {
			push(@results, $1);
		}
	}

	if (@results > 0) {
		$output = "<h2>Results for " . tohtml($query) . "</h2>\n<ul>\n";
		foreach my $r (@results) {
			$output .= " <li>" . tohtml($r) . "</li>\n";
		}
		$output .= "</ul>\n";
	} else {
		$output = "<h2>No results</h2>\n<p>Gee… I don’t know what “" .
		    tohtml($query) . "” means…</p>\n";
	}

	$output .= "<form action=\"https://duckduckgo.com/?kp=-1&#38;kl=wt-wt&#38;kb=t&#38;kh=1&#38;kj=g2&#38;km=l&#38;ka=monospace&#38;ku=1&#38;ko=s&#38;k1=-1&#38;kv=1&#38;t=debian\" method=\"post\">" .
	    "<p><input type=\"hidden\" name=\"q\" value=\"" . $enc .
	    "\" /><input type=\"submit\" value=\"Search the web for: " .
	    $enc . "\" /></p></form>\n";
}
close(ACRONYMS);

print "Content-Type: text/html; charset=utf-8\r\n\r\n";
my $state = 0;
foreach my $line (<TEMPLATE>) {
	chomp($line);
	if ($line eq '<!-- wtf-begin -->') {
		$state = 1;
	}
	if ($line eq '<!-- wtf-result -->') {
		$line = $output;
		$state = 0;
	}
	if ($line =~ /rcsdiv.*rcsid/) {
		$line =~ s!\Q</p>\E! by <span class=\"rcsid\">$rcsid</span>$&!;
	}
	print $line . "\n" unless $state;
}
close(TEMPLATE);
exit(0);
