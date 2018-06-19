#	Poor Man's CGI Module for Perl -- Error Handling
#
#	(c) 2002--2012 Martin Mares <mj@ucw.cz>
#
#	This software may be freely distributed and used according to the terms
#	of the GNU Lesser General Public License.

package UCW::CGI::ErrorHandler;

# E-mail address of the script admin (optional, preferably set in a BEGIN block)
our $error_mail;

# A function called for reporting of errors
our $error_hook;

# Set to true if you want to show detailed error messages to the user
our $print_errors = 0;

my $error_reported;
our $exit_code;

sub report_bug($)
{
	if (!defined $error_reported) {
		$error_reported = 1;
		print STDERR $_[0];
		if (defined($error_hook)) {
			&$error_hook($_[0]);
		} else {
			print "Status: 500\n";
			print "Content-Type: text/plain\n\n";
			if ($print_errors) {
				print "Internal bug: ", $_[0], "\n";
			} else {
				print "Internal bug.\n";
			}
			print "Please notify $error_mail\n" if defined $error_mail;
		}
	}
	die;
}

BEGIN {
	$SIG{__DIE__} = sub { report_bug($_[0]); };
	$SIG{__WARN__} = sub { report_bug("WARNING: " . $_[0]); };
	$exit_code = 0;
}

END {
	$? = $exit_code;
}

42;
