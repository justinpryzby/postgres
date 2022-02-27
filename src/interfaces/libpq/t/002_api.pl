# Copyright (c) 2022, PostgreSQL Global Development Group
use strict;
use warnings;

use Config;
use PostgreSQL::Test::Utils;
use Test::More;

my $test_dir = $ENV{TESTDIR};
if ($PostgreSQL::Test::Utils::windows_os &&
	$Config{osname} eq 'MSWin32')
{
	$ENV{PATH} =~ s!;!;$test_dir\\test;!;
}
else
{
	$ENV{PATH} =~ s!:!:$test_dir/test:!;
}

# Test PQsslAttribute(NULL, "library")
my ($out, $err) = run_command([ 'libpq_testclient', '--ssl' ]);

if ($ENV{with_ssl} eq 'openssl')
{
	is($out, 'OpenSSL', 'PQsslAttribute(NULL, "library") returns "OpenSSL"');
}
else
{
	is( $err,
		'SSL is not enabled',
		'PQsslAttribute(NULL, "library") returns NULL');
}

done_testing();
