#!/usr/bin/perl -T
my $rcsid = '$MirOS: wtf/www/wtf.cgi,v 1.14 2015/03/14 01:09:22 tg Exp $';
#-
# Copyright © 2012, 2014, 2015
#	Thorsten “mirabilos” Glaser <tg@mirbsd.org>
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

# ltrim and rtrim
$query =~ s/^\s+//;
$query =~ s/\s+$//;

sub tohtml {
	local ($_) = @_;

	s/&/&#38;/g;
	s/</&#60;/g;
	s/>/&#62;/g;
	s/\"/&#34;/g;

	return $_;
}

my $acrcsid = "";

if ($query ne "") {
	my $enc = tohtml($query);

	$query =~ y/a-z/A-Z/;
	$query =~ s/ä/Ä/g;
	$query =~ s/ö/Ö/g;
	$query =~ s/ü/Ü/g;
	$query =~ s/ñ/Ñ/g;
	$query =~ s/á/Á/g;
	$query =~ s/é/É/g;
	$query =~ s/í/Í/g;
	$query =~ s/ó/Ó/g;
	$query =~ s/ç/Ç/g;
	$query =~ s/è/È/g;

	foreach my $line (<ACRONYMS>) {
		chomp($line);
		if ($line =~ /^\$MirOS: /) {
			$acrcsid = $line;
		}
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

	$output .= "<p>\n <a href=\"man.cgi?" . $enc .
	    "\">Manual page lookup for: " . $enc . "</a>\n</p>\n";

	$output .= "<p>\n <input type=\"hidden\" name=\"q\" value=\"" . $enc .
	    " acronym\" /><input type=\"submit\" value=\"Web lookup for: " .
	    $enc . "\" />";
}
close(ACRONYMS);

print "Content-Type: text/html; charset=utf-8\r\n\r\n";
my $state = 0;
foreach my $line (<TEMPLATE>) {
	chomp($line);
	if ($line eq '<!-- wtf-result begin -->') {
		$state = 1 unless $query eq "";
		next;
	}
	if ($line eq '<!-- wtf-result end -->') {
		$line = $output;
		$state = 0;
	}
	if ($line =~ /rcsdiv.*rcsid/) {
		$line =~ s!\Q</p>\E! by <span class=\"rcsid\">$rcsid</span>$&!;
		if ($acrcsid ne "") {
			$line =~ s!\Q</p>\E! from <span class=\"rcsid\">$acrcsid</span>$&!;
		}
	}
	print $line . "\n" unless $state;
}
close(TEMPLATE);
exit(0);
