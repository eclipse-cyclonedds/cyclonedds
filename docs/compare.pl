open A, "< $ARGV[0]" or die "can't open $ARGV[0]";
open B, "< $ARGV[1]" or die "can't open $ARGV[1]";
while (defined ($a = <A>) && defined ($b = <B>)) {
  $a =~ s/[\r\n]+$//s;
  $b =~ s/[\r\n]+$//s;
  print "$ARGV[0] difference detected\n" and exit 1 unless $a eq $b;
}
exit 0;
