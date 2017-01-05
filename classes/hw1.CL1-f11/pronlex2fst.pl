#!/usr/bin/perl -w

use strict;

my %phoneList = ();
my %dict = ();

print <<EOH;
0	1	<s>	<s>
1	0	<eps>	<eps>
0	2	</s>	</s>
2	0	<eps>	<eps>
0	3	<unk>	<unk>
3	0	<eps>	<eps>
EOH

my $stateCount = 3;

while(<>)
{
 chomp;
 my @line = split /\t/;
 my $word = shift @line;
 
 my @phones = split ' ',$line[0];

 
 for(my $phone = 0; $phone <= $#phones; $phone++)
 {
  print $phone ? $stateCount : 0;
  print "\t";
  $stateCount++;
  print "$stateCount\t";
  
  print $phone ? "<eps>" : $word;
  print "\t";
  print "$phones[$phone]\n";
 }
 print "$stateCount	0	<eps>	<eps>\n";
 $stateCount++;
}

print "0\n";
