# This script was automatically generated from the dsa-133
# Debian Security Advisory
# It is released under the Nessus Script Licence.
# Advisory is copyright 1997-2004 Software in the Public Interest, Inc.
# See http://www.debian.org/license
# DSA2nasl Convertor is copyright 2004 Michel Arboi

if (! defined_func('bn_random')) exit(0);

desc = '
Mark Litchfield found a denial of service attack in the Apache
web-server. While investigating the problem the Apache Software
Foundation discovered that the code for handling invalid requests which
use chunked encoding also might allow arbitrary code execution.
This has been fixed in version 1.3.9-14.1-1.21.20000309-1 of the Debian
apache-perl package and we recommend that you upgrade your apache-perl
package immediately.
An update for the soon to be released Debian GNU/Linux 3.0/woody
distribution will be available soon.


Solution : http://www.debian.org/security/2002/dsa-133
Risk factor : High';

if (description) {
 script_id(14970);
 script_version("$Revision: 1.7 $");
 script_xref(name: "DSA", value: "133");
 script_cve_id("CVE-2002-0392");
 script_bugtraq_id(5033);
 script_xref(name: "CERT", value: "944335");

 script_description(english: desc);
 script_copyright(english: "This script is (C) 2006 Michel Arboi <mikhail@nessus.org>");
 script_name(english: "[DSA133] DSA-133-1 apache-perl");
 script_category(ACT_GATHER_INFO);
 script_family(english: "Debian Local Security Checks");
 script_dependencies("ssh_get_info.nasl");
 script_require_keys("Host/Debian/dpkg-l");
 script_summary(english: "DSA-133-1 apache-perl");
 exit(0);
}

include("debian_package.inc");

w = 0;
if (deb_check(prefix: 'apache-perl', release: '2.2', reference: '1.3.9-14.1-1.21.20000309-1')) {
 w ++;
 if (report_verbosity > 0) desc = strcat(desc, '\nThe package apache-perl is vulnerable in Debian 2.2.\nUpgrade to apache-perl_1.3.9-14.1-1.21.20000309-1\n');
}
if (w) { security_hole(port: 0, data: desc); }
