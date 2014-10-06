=pod
my $num_steps = 450;
my $outer_tube_steps = 8;
my $inner_tube_steps = 8;
my $inner_tube_segs = 12;
=cut
my $num_steps = (380/2);
my $outer_tube_steps = 8;
my $inner_tube_steps = 5;
my $inner_tube_segs = 7;

my $outer_tube_quads = $num_steps*$outer_tube_steps;
my $inner_tube_quads = $inner_tube_steps*($inner_tube_segs - 1);

my $total_quads = ($num_steps/2)*$inner_tube_quads + 2*$outer_tube_quads;
# my $total_quads = ($num_steps/4)*$inner_tube_quads;

print "total quads: $total_quads\n";
