#!/usr/bin/perl -T
my $rcsid = '$MirOS: wtf/www/wtf.cgi,v 1.19 2015/12/27 02:25:09 tg Exp $';
#-
# Copyright © 2012, 2014, 2015
#	mirabilos <m@mirbsd.org>
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
	$query =~ y/.//d if $query =~ /[A-Z]\./;
	$query =~ s/à/À/g;
	$query =~ s/á/Á/g;
	$query =~ s/ä/Ä/g;
	$query =~ s/å/Å/g;
	$query =~ s/æ/Æ/g;
	$query =~ s/ç/Ç/g;
	$query =~ s/è/È/g;
	$query =~ s/é/É/g;
	$query =~ s/í/Í/g;
	$query =~ s/ð/Ð/g;
	$query =~ s/ñ/Ñ/g;
	$query =~ s/ó/Ó/g;
	$query =~ s/ö/Ö/g;
	$query =~ s/ø/Ø/g;
	$query =~ s/ü/Ü/g;
	$query =~ s/þ/Þ/g;
	$query =~ s/ą/Ą/g;
	$query =~ s/ć/Ć/g;
	$query =~ s/ĉ/Ĉ/g;
	$query =~ s/č/Č/g;
	$query =~ s/đ/Đ/g;
	$query =~ s/ę/Ę/g;
	$query =~ s/ĝ/Ĝ/g;
	$query =~ s/ĥ/Ĥ/g;
	$query =~ s/ĵ/Ĵ/g;
	$query =~ s/ł/Ł/g;
	$query =~ s/ń/Ń/g;
	$query =~ s/ś/Ś/g;
	$query =~ s/ŝ/Ŝ/g;
	$query =~ s/š/Š/g;
	$query =~ s/ŭ/Ŭ/g;
	$query =~ s/ź/Ź/g;
	$query =~ s/ż/Ż/g;
	$query =~ s/ž/Ž/g;
	$query =~ s/ε/Ε/g;
	$query =~ s/λ/Λ/g;
	$query =~ s/ο/Ο/g;
	$query =~ s/ς/Σ/g;
	$query =~ s/σ/Σ/g;
	$query =~ s/а/А/g;
	$query =~ s/б/Б/g;
	$query =~ s/г/Г/g;
	$query =~ s/д/Д/g;
	$query =~ s/е/Е/g;
	$query =~ s/з/З/g;
	$query =~ s/й/Й/g;
	$query =~ s/к/К/g;
	$query =~ s/л/Л/g;
	$query =~ s/м/М/g;
	$query =~ s/н/Н/g;
	$query =~ s/о/О/g;
	$query =~ s/р/Р/g;
	$query =~ s/с/С/g;
	$query =~ s/т/Т/g;
	$query =~ s/у/У/g;
	$query =~ s/ф/Ф/g;
	$query =~ s/қ/Қ/g;
	$query =~ s/ա/Ա/g;
	$query =~ s/հ/Հ/g;
	$query =~ s/յ/Յ/g;

	foreach my $line (<ACRONYMS>) {
		chomp($line);
		if ($line =~ /^ \@\(\#\)(.*)$/) {
			$acrcsid = $1;
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

	$output .= "<form accept-charset=\"utf-8\" " .
	    "action=\"https://duckduckgo.com/?kp=-1&#38;kl=wt-wt&#38;kb=t&#38;kh=1&#38;kj=g2&#38;km=l&#38;ka=monospace&#38;ku=1&#38;ko=s&#38;k1=-1&#38;kv=1&#38;t=debian\" " .
	    "method=\"post\"><p>\n <input type=\"hidden\" name=\"q\" value=\"" .
	    $enc . " acronym\" /><input type=\"submit\" value=\"Web search: " .
	    $enc . "\" />\n</p></form>\n<p>DuckDuckGo is a search engine " .
	    "with more privacy and lots of\n features. This search is " .
	    "external content, not part of MirOS.</p>";
}
close(ACRONYMS);

print "Content-Type: text/html; charset=utf-8\r\n\r\n";
foreach my $line (<TEMPLATE>) {
	chomp($line);
	if ($line eq '<!-- wtf-result -->') {
		$line = $output;
	}
	if ($line =~ /rcsdiv.*rcsid/) {
		$line =~ s!\Q</p>\E! by <span class=\"rcsid\">$rcsid</span>$&!;
		if ($acrcsid ne "") {
			$line =~ s!\Q</p>\E! from <span class=\"rcsid\">$acrcsid</span>$&!;
		}
	}
	print $line . "\n";
}
close(TEMPLATE);
exit(0);
