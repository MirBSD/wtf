#!/usr/bin/perl -T
my $rcsid = '$MirOS: wtf/www/wtf.cgi,v 1.33 2022/01/03 03:17:30 tg Exp $';
#-
# Copyright © 2012, 2014, 2015, 2017, 2020, 2021, 2022
#	mirabilos <m@mirbsd.org>
# Copyright © 2017
#	<RT|Chatzilla> via IRC
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
# Implementation of core parts of MirBSD wtf(1) as CGI.

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

my $acrcsid = "";
my $query = "";
my $queryorig = "";
my @wtfresults = ();
my $qhtml = 'invalid <tt>QUERY_STRING</tt> or no <tt>q</tt> parameter';
my $output = "<p id=\"serp\">($qhtml)</p>";

sub tohtml {
	local $_ = @_ ? shift : $_;

	s/&/&#38;/g;
	s/</&#60;/g;
	s/>/&#62;/g;
	s/\"/&#34;/g;

	return $_;
}

if (defined($ENV{QUERY_STRING})) {
	for my $p (split(/[;&]+/, $ENV{QUERY_STRING})) {
		next unless $p;
		my ($key, $val) = split(/=/, $p, 2);
		next unless defined($key);
		next unless defined($val);
		$queryorig = $val if $key eq "q";
	}
	if ($queryorig eq "") {
		my $p = $ENV{QUERY_STRING};
		$queryorig = $p unless $p =~ /^(.*[;&])?[a-z]+=([;&].*)?$/;
	}

	$queryorig =~ y/+/ /;
	$queryorig =~ s/%([0-9A-Fa-f][0-9A-Fa-f])/chr(hex($1))/eg;
	# ltrim and rtrim
	$queryorig =~ s/^\s+//;
	$queryorig =~ s/\s+$//;
	# take your funny business outside, please
	$queryorig = "" if $queryorig =~ /[\t\r\n]/;

	$qhtml = tohtml($queryorig) if $queryorig ne "";
}

# query for wtf
$query = $queryorig;
if ($query ne "") {
	my $enc = tohtml($query);
	# urlencode
	(my $enq = $query) =~ s/[^!()*.0-9A-Z_a-z~-]/sprintf("%%%02X", ord($&));/eg;

	# uppercase search term
	$query =~ y/a-z/A-Z/;
	# specific full stop removal rule
	$query =~ y/.//d if (($query =~ /[A-Z]\./) || ($query =~ /^\..*[^.-]/));

	my $line = <ACRONYMS>;		# grab first line from acronyms file
	$line =~ s/^\s+//;		# verwĳder leading spaces
	my @pairs = split / /, $line;	# split space-separated pairs
	foreach $a (@pairs){
		# split slash-separated word pair
		my @pair = split /\//, $a;
		# manual UCS uppercasing
		$query =~ s/$pair[0]/$pair[1]/g;
	}

	$qhtml = tohtml($query);

	# DB version (rely on proper file format)
	$line = <ACRONYMS>;
	$acrcsid = substr($line, 5, -1);

	# now search for the term
	my $past_term = 0;
	$query .= "\t";
	foreach my $line (<ACRONYMS>) {
		if (rindex($line, $query, 0) == 0) {
			push(@wtfresults, substr($line, length($query), -1));
			$past_term = 1;
		} elsif ($past_term) {
			last;
		}
	}

	$output = "<fieldset id=\"serp\">\n";
	$output .= " <legend xml:lang=\"de-DE-1901\">Suchergebnisse</legend>\n\n";

	if (@wtfresults > 0) {
		$output .= " <h2>Results for $qhtml</h2>\n <ul>\n";
		foreach my $r (@wtfresults) {
			$output .= "  <li>" . tohtml($r) . "</li>\n";
		}
		$output .= " </ul>\n";
	} else {
		$output .= " <h2>No results</h2>\n" .
		    " <p>Gee… I don’t know what “$qhtml” means…</p>\n";
	}

	$output .= " <p><a href=\"man.cgi?q=" . tohtml($enq) .
	    "\">Manual page lookup for: " . $enc . "</a></p>\n";

	$output .= " <form accept-charset=\"utf-8\" " .
	    "action=\"https://duckduckgo.com/?kp=-1&#38;kl=wt-wt&#38;kb=t&#38;kh=1&#38;kj=g2&#38;km=l&#38;ka=monospace&#38;ku=1&#38;ko=s&#38;k1=-1&#38;kv=1&#38;t=debian\" " .
	    "method=\"post\"><p>\n  <input type=\"hidden\" name=\"q\" value=\"" .
	    $enc . " acronym\" />\n  <input type=\"submit\" value=\"Web search: " .
	    $enc . "\" />\n </p></form>\n <p>DuckDuckGo is a search engine " .
	    "with more privacy and lots of\n  features. This search is " .
	    "external content, not part of MirBSD.</p>\n";

	$output .= "</fieldset>";
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
	if ($line =~ /^<.head><body/) {
		$line = '</head><body>';
	}
	print $line . "\n";
}
close(TEMPLATE);
exit(0);
